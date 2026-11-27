/*
 * remote_dmacplus.c: Kermit DMAC+ server (channel 7) implementing
 * AVC/JPEG CSC transfers, linked-list RAM/EDRAM DMA, and block copies.
 */
#include "host_common.h"

#include <stdio.h>
#include <string.h>

#define DMACPLUS_CHANNEL_ID        7
#define DMACPLUS_CMD_CH2_COPY      1385
#define DMACPLUS_CMD_SET_CROP      1386
#define DMACPLUS_CMD_CH0_CSC       1387
#define DMACPLUS_CMD_CH1_LLI       1388

typedef struct DmacplusParams {
	uint32_t words[13];
} DmacplusParams;

typedef struct DmacplusLli {
	uint32_t src;
	uint32_t dst;
	uint32_t next;
	uint32_t attr;
} DmacplusLli;

typedef struct DmacplusChannel {
	SceUID         reqSema;
	SceUID         doneSema;
	DmacplusParams params;
	int          (*handler)(const DmacplusParams *params);
} DmacplusChannel;

DmacplusChannel g_dmacplusChZero;
DmacplusChannel g_dmacplusChOne;
DmacplusChannel g_dmacplusChTwo;
uint32_t        g_dmacplusCropX;
uint32_t        g_dmacplusCropY;
uint32_t        g_dmacplusCropW;
uint32_t        g_dmacplusCropH;
int             g_dmacplusDirectEdramMode;

static inline uint32_t dmacplusNormalizeAddr(uint32_t addr)
{
	if (addr != 0U && (addr >> 30) <= 1U && ((addr >> 28) & 3U) <= 1U) {
		if ((addr >> 30) != 0U)
			return addr & ~0x40000000U;
		return addr | 0x80000000U;
	}
	return addr;
}

static int __attribute__((noinline, noclone)) dmacplusSubmitRequest(
	DmacplusChannel *channel, const DmacplusParams *params)
{
	if (sceKernelWaitSema(channel->doneSema, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	channel->params = *params;

	int ret = sceKernelSignalSema(channel->reqSema, 1);
	if (ret < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int dmacplusOpThread(SceSize args, void *argp) __attribute__((noreturn));
static int dmacplusOpThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;
	DmacplusParams params;

	(void)args;
	(void)argp;

	for (;;) {
		int64_t retVal;
		int cmd = remotefsWaitAndGetRequest(DMACPLUS_CHANNEL_ID, &req);

		switch (cmd) {
		case DMACPLUS_CMD_CH2_COPY:
			for (int i = 0; i < 5; i++)
				params.words[i] = (uint32_t)req->args[i];
			dmacplusSubmitRequest(&g_dmacplusChTwo, &params);
			retVal = 0;
			break;
		case DMACPLUS_CMD_SET_CROP:
			g_dmacplusCropX = (uint32_t)req->args[0];
			g_dmacplusCropY = (uint32_t)req->args[1];
			g_dmacplusCropW = (uint32_t)req->args[2];
			g_dmacplusCropH = (uint32_t)req->args[3];
			retVal = 0;
			break;
		case DMACPLUS_CMD_CH0_CSC:
			for (int i = 0; i < 13; i++)
				params.words[i] = (uint32_t)req->args[i];
			dmacplusSubmitRequest(&g_dmacplusChZero, &params);
			retVal = 0;
			break;
		case DMACPLUS_CMD_CH1_LLI:
			for (int i = 0; i < 5; i++)
				params.words[i] = (uint32_t)req->args[i];
			dmacplusSubmitRequest(&g_dmacplusChOne, &params);
			retVal = 0;
			break;
		default:
			pspemuRecordTitleListFlags(16U);
			retVal = 0xFFFFFFFFLL;
			break;
		}

		remotefsReturnValue(DMACPLUS_CHANNEL_ID, req, retVal);
	}
}

static int dmacplusChannelThread(SceSize args, void *argp)
{
	DmacplusChannel *channel = *(DmacplusChannel *const *)argp;

	(void)args;

	do {
		if (sceKernelWaitSema(channel->reqSema, 1, NULL) < 0)
			break;
		channel->handler(&channel->params);
	} while (sceKernelSignalSema(channel->doneSema, 1) >= 0);

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

static int dmacplusChOneTransfer(const DmacplusParams *params)
{
	DmacplusLli initialLli;
	initialLli.src = params->words[0];
	initialLli.dst = params->words[1];
	initialLli.next = params->words[2];
	initialLli.attr = params->words[3];

	DmacplusLli *lli = &initialLli;
	for (;;) {
		SceSize copySize = lli->attr & 0xFFFU;
		const void *srcHost = remotefsTranslateGuestAddr(dmacplusNormalizeAddr(lli->src), 1, copySize);

		uint32_t dstGuest = dmacplusNormalizeAddr(lli->dst);
		void *dstHost = meEdramGetBase(dstGuest);
		if (dstHost == NULL)
			dstHost = remotefsTranslateUserGuestAddr(dstGuest, 2, copySize);

		memcpy(dstHost, srcHost, copySize);
		remotefsFlushGuestAddr(dstHost, copySize);

		uint32_t nextGuest = dmacplusNormalizeAddr(lli->next);
		lli->next = nextGuest;
		lli = (DmacplusLli *)remotefsTranslateGuestAddr(nextGuest, 1, sizeof(DmacplusLli));
		if (lli == NULL)
			break;
	}

	int ret;
	do {
		ret = sceCompatInterrupt(5);
		if (ret == PSPEMU_ERROR_NOT_SUPPORTED)
			sceKernelDelayThread(10000U);
	} while (ret == PSPEMU_ERROR_NOT_SUPPORTED);

	if (ret != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int __attribute__((noinline, noclone)) dmacplusStartChannel(DmacplusChannel *channel, int index)
{
	DmacplusChannel *threadArg = channel;
	char reqName[32];
	char doneName[32];
	char threadName[32];
	int chNum = index + 1;

	snprintf(reqName, sizeof(reqName), "ScePspemuDmacplusChRequestForCh%d", chNum);
	channel->reqSema = sceKernelCreateSema(reqName, 0, 0, 1, NULL);
	if (channel->reqSema <= 0)
		return channel->reqSema;

	snprintf(doneName, sizeof(doneName), "ScePspemuDmacplusChDoneForCh%d", chNum);
	channel->doneSema = sceKernelCreateSema(doneName, 0, 1, 1, NULL);
	if (channel->doneSema <= 0)
		return channel->doneSema;

	snprintf(threadName, sizeof(threadName), "ScePspemuRemoteDmacplusCh%d", chNum);
	SceUID thid = sceKernelCreateThread(
		threadName,
		dmacplusChannelThread,
		index + 93,
		0x1000U,
		0,
		0x10000,
		NULL);
	if (thid < 0)
		return thid;
	return sceKernelStartThread(thid, sizeof(threadArg), &threadArg);
}

static int dmacplusChZeroAvcCsc(const DmacplusParams *params)
{
	uint32_t srcGuest = dmacplusNormalizeAddr(params->words[0]);
	uint32_t sizeWord = params->words[8];
	uint32_t dstGuestEven = dmacplusNormalizeAddr(params->words[9]);
	uint32_t dstGuestOdd = dmacplusNormalizeAddr(params->words[10]);
	uint32_t fmtWord = params->words[11];

	uint32_t mbWidth = (sizeWord >> 8) & 0x3FU;
	uint32_t mbHeight = (sizeWord >> 16) & 0x3FU;
	uint32_t isJpegMode = sizeWord & 1U;
	uint32_t pixelFormat = (fmtWord >> 1) & 3U;
	uint32_t pitch = (fmtWord >> 8) & 0x7FFU;
	uint32_t bpp = (pixelFormat >= 1U && pixelFormat <= 3U) ? 2U : 4U;
	uint32_t yuvBytes = 384U * mbHeight * mbWidth;

	void *srcYuv;
	if (g_dmacplusDirectEdramMode != 0 && isJpegMode == 0U) {
		srcYuv = meEdramGetBase(0U);
	} else {
		srcYuv = meEdramGetBase(srcGuest);
		if (srcYuv == NULL)
			srcYuv = remotefsTranslateUserGuestAddr(srcGuest, 1, yuvBytes);
	}

	if (yuvBytes != 0U && srcYuv != NULL) {
		uint32_t dstSize = bpp * 16U * pitch * mbHeight;
		if (dstSize != 0U && dstGuestEven != 0U) {
			if (isJpegMode != 0U) {
				if (dstGuestOdd - dstGuestEven != (dstSize >> 1))
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				void *dstRgb = remotefsTranslateGuestAddr(dstGuestEven, 2, dstSize);
				remoteMeJpegCsc(
					dstRgb,
					dstSize,
					srcYuv,
					(int)pixelFormat,
					(int)(16U * mbWidth),
					(int)(16U * mbHeight),
					(int)pitch);
			} else {
				void *dstRgb = remotefsTranslateGuestAddr(dstGuestEven, 2, dstSize);
				if (g_dmacplusCropW != 16U * mbWidth || g_dmacplusCropH != 16U * mbHeight)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				uint32_t cscParams[6];
				cscParams[0] = g_dmacplusCropX;
				cscParams[1] = g_dmacplusCropY;
				cscParams[2] = g_dmacplusCropW;
				cscParams[3] = g_dmacplusCropH;
				cscParams[4] = pitch;
				cscParams[5] = pixelFormat;
				remoteMeAvcdecCsc(dstRgb, dstSize, srcYuv, cscParams);
			}
		}
	}

	int ret;
	do {
		ret = sceCompatInterrupt(4);
		if (ret == PSPEMU_ERROR_NOT_SUPPORTED)
			sceKernelDelayThread(10000U);
	} while (ret == PSPEMU_ERROR_NOT_SUPPORTED);

	if (ret != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int dmacplusChTwoCopy(const DmacplusParams *params)
{
	if (g_dmacplusDirectEdramMode == 0) {
		DmacplusLli initialLli;
		initialLli.src = params->words[0];
		initialLli.dst = params->words[1];
		initialLli.next = params->words[2];
		initialLli.attr = params->words[3];

		uint32_t srcGuest = dmacplusNormalizeAddr(initialLli.src);
		uint32_t dstGuest = dmacplusNormalizeAddr(initialLli.dst);

		uint32_t totalBytes = 0U;
		DmacplusLli *lli = &initialLli;
		for (;;) {
			totalBytes += (lli->attr & 0xFFFU) << ((lli->attr >> 18) & 7U);
			uint32_t nextGuest = dmacplusNormalizeAddr(lli->next);
			lli->next = nextGuest;
			lli = (DmacplusLli *)remotefsTranslateGuestAddr(nextGuest, 1, sizeof(DmacplusLli));
			if (lli == NULL)
				break;
		}

		void *dstHost = remotefsTranslateGuestAddr(dstGuest, 2, totalBytes);
		void *srcHost = meEdramGetBase(srcGuest);
		if (srcHost == NULL)
			srcHost = remotefsTranslateUserGuestAddr(srcGuest, 1, totalBytes);

		if (remoteMeAvcdecCopyFrameToHost(dstHost, srcHost, totalBytes) < 0) {
			if (totalBytes < 0x4000U || pspemuCompatHasFlag(0x1000000U) != 0)
				memcpy(dstHost, srcHost, totalBytes);
			else
				sceDmacMemcpy(dstHost, srcHost, totalBytes);
			remotefsFlushGuestAddr(dstHost, totalBytes);
		}
	}

	int ret;
	do {
		ret = sceCompatInterrupt(6);
		if (ret == PSPEMU_ERROR_NOT_SUPPORTED)
			sceKernelDelayThread(10000U);
	} while (ret == PSPEMU_ERROR_NOT_SUPPORTED);

	if (ret != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

int remoteDmacplusIsEnabled(void)
{
	return (g_dmacplusDirectEdramMode != 0) ? 1 : 0;
}

int remoteDmacplusSetEnabled(int enabled)
{
	g_dmacplusDirectEdramMode = enabled;
	return 0;
}

int remoteDmacplusInit(void)
{
	if (pspemuCompatHasFlag(32U) != 0)
		g_dmacplusDirectEdramMode = 1;

	g_dmacplusChZero.handler = dmacplusChZeroAvcCsc;
	int ret = dmacplusStartChannel(&g_dmacplusChZero, 0);
	if (ret < 0)
		return ret;

	g_dmacplusChOne.handler = dmacplusChOneTransfer;
	ret = dmacplusStartChannel(&g_dmacplusChOne, 1);
	if (ret < 0)
		return ret;

	g_dmacplusChTwo.handler = dmacplusChTwoCopy;
	ret = dmacplusStartChannel(&g_dmacplusChTwo, 2);
	if (ret < 0)
		return ret;

	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemoteDmacplus",
		dmacplusOpThread,
		96,
		0x1000U,
		0,
		0x10000,
		NULL);
	if (thid < 0)
		return thid;
	return sceKernelStartThread(thid, 0, NULL);
}
