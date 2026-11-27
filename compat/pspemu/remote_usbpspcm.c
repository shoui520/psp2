/*
 * remote_usbpspcm.c: Kermit remote USB PSPCM (USB host communication) driver
 * proxy for ScePspemu.
 */
#include "host_common.h"
#include <stdio.h>
#include <string.h>

#define PSPEMU_USBPSPCM_CHANNEL            8
#define PSPEMU_USBPSPCM_MAX_UNITS          8

#define PSPEMU_USBPSPCM_IOCTL_RESET        0x03405101U
#define PSPEMU_USBPSPCM_IOCTL_POLL_DATA    0x03406103U
#define PSPEMU_USBPSPCM_IOCTL_CANCEL_WAIT  0x03406105U
#define PSPEMU_USBPSPCM_IOCTL_WAIT_DATA    0x03415104U
#define PSPEMU_USBPSPCM_IOCTL_SET_MODE     0x03416102U

#define PSPEMU_USBPSPCM_DEVCTL_REG_NOTIFY  0x03415001U
#define PSPEMU_USBPSPCM_DEVCTL_UNREG_NOTIFY 0x03415002U
#define PSPEMU_USBPSPCM_DEVCTL_BIND        0x03435005U

#define PSPEMU_USBPSPCM_ERR_NOT_ACTIVATED  ((int)0x80243007)
#define PSPEMU_USBPSPCM_ERR_INVALID_ARG    ((int)0x80243402)
#define PSPEMU_USBPSPCM_VITA_ERR_NOT_REG   ((int)0x80244505)
#define PSPEMU_USBPSPCM_COMPAT_INTR_RETRY  ((int)0x80010086)

typedef struct PspemuUsbPspcmBindGuestReq {
	uint32_t addrLow;
	uint32_t addrHigh;
	uint32_t timeout;
	uint32_t noTimeout;
} PspemuUsbPspcmBindGuestReq;

typedef struct PspemuUsbPspcmBindHostReq {
	uint32_t        addrLow;
	uint32_t        addrHigh;
	const uint32_t *timeoutPtr;
} PspemuUsbPspcmBindHostReq;

typedef union PspemuUsbPspcmEventPair {
	struct {
		int32_t eventCode;
		int32_t eventArg;
	} parts;
	int64_t raw;
} PspemuUsbPspcmEventPair;

static PspemuUsbPspcmEventPair g_usbPspcmEvent;
static SceKernelLwMutexWork g_usbPspcmMutex;
static SceUID g_usbPspcmNotifyThreadId;
static uint32_t g_usbPspcmChannelMode[PSPEMU_USBPSPCM_MAX_UNITS];

static int usbPspcmTranslateError(int err);
static int usbPspcmFsInit(void *arg);
static int usbPspcmFsExit(void *arg);
static int usbPspcmFsOpen(const char *path, int flags, SceMode mode);
static int usbPspcmFsClose(SceUID fd);
static SceSSize usbPspcmFsRead(SceUID fd, void *buf, SceSize size);
static SceSSize usbPspcmFsWrite(SceUID fd, const void *buf, SceSize size);
static int usbPspcmFsIoctl(SceUID fd, unsigned int cmd, void *inData, SceSize inLen,
                           void *outData, SceSize outLen);
static int usbPspcmFsDevctl(const char *dev, unsigned int cmd, void *inData, SceSize inLen,
                            void *outData, SceSize outLen);
static int usbPspcmUnregisterNotify(void);

static const PspemuFsOps g_usbPspcmFsOps = {
	usbPspcmFsInit,
	usbPspcmFsExit,
	usbPspcmFsOpen,
	usbPspcmFsClose,
	usbPspcmFsRead,
	usbPspcmFsWrite,
	NULL,
	usbPspcmFsIoctl,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	usbPspcmFsDevctl,
	NULL,
};

static int usbPspcmTranslateError(int err)
{
	switch (err) {
	case (int)0x80244501: return (int)0x80243401;
	case (int)0x80244502: return (int)0x80243402;
	case (int)0x80244503: return (int)0x80243403;
	case (int)0x80244505: return (int)0x80243405;
	case (int)0x80244506: return (int)0x80243406;
	case (int)0x80244507: return (int)0x80243407;
	case (int)0x80244508: return (int)0x80243408;
	case (int)0x80244509: return (int)0x80243409;
	case (int)0x8024450A: return (int)0x8024340A;
	case (int)0x8024450B: return (int)0x8024340B;
	case (int)0x8024450C: return (int)0x8024340C;
	default:              return err;
	}
}

static int usbPspcmFsInit(void *arg)
{
	(void)arg;
	return 0;
}

static int usbPspcmWorkerThread(SceSize argSize, void *argp)
{
	(void)argSize;
	(void)argp;
	compatFsLoop(PSPEMU_USBPSPCM_CHANNEL, &g_usbPspcmFsOps);
	return 0;
}

static int usbPspcmFsIoctl(SceUID fd, unsigned int cmd, void *inData, SceSize inLen,
                           void *outData, SceSize outLen)
{
	(void)outData;
	(void)outLen;

	switch (cmd) {
	case PSPEMU_USBPSPCM_IOCTL_RESET:
		return usbPspcmTranslateError(sceUsbPspcmIoctlReset((unsigned int)fd));
	case PSPEMU_USBPSPCM_IOCTL_POLL_DATA:
		return usbPspcmTranslateError(sceUsbPspcmIoctlPollData((unsigned int)fd));
	case PSPEMU_USBPSPCM_IOCTL_CANCEL_WAIT:
		return usbPspcmTranslateError(sceUsbPspcmIoctlCancelWaitData((unsigned int)fd));
	case PSPEMU_USBPSPCM_IOCTL_WAIT_DATA: {
		unsigned int timeoutVal;
		const unsigned int *timeoutPtr = NULL;

		if (inData != NULL) {
			if (inLen != sizeof(unsigned int))
				return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
			timeoutVal = *(const unsigned int *)inData;
			timeoutPtr = &timeoutVal;
		}
		return usbPspcmTranslateError(sceUsbPspcmIoctlWaitData((unsigned int)fd, timeoutPtr));
	}
	case PSPEMU_USBPSPCM_IOCTL_SET_MODE:
		if (inData != NULL && inLen == sizeof(uint32_t)) {
			uint32_t mode = *(const uint32_t *)inData;
			if ((mode & ~3U) == 0U && (unsigned int)fd < PSPEMU_USBPSPCM_MAX_UNITS) {
				g_usbPspcmChannelMode[(unsigned int)fd] = mode;
				return 0;
			}
		}
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	default:
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	}
}

static SceSSize usbPspcmFsWrite(SceUID fd, const void *buf, SceSize size)
{
	if ((unsigned int)fd >= PSPEMU_USBPSPCM_MAX_UNITS)
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	return usbPspcmTranslateError(
		sceUsbPspcmWrite((unsigned int)fd, buf, size, g_usbPspcmChannelMode[(unsigned int)fd]));
}

static SceSSize usbPspcmFsRead(SceUID fd, void *buf, SceSize size)
{
	if ((unsigned int)fd >= PSPEMU_USBPSPCM_MAX_UNITS)
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	return usbPspcmTranslateError(
		sceUsbPspcmRead((unsigned int)fd, buf, size, g_usbPspcmChannelMode[(unsigned int)fd]));
}

static int usbPspcmFsClose(SceUID fd)
{
	return usbPspcmTranslateError(sceUsbPspcmClose((unsigned int)fd));
}

static int usbPspcmFsOpen(const char *path, int flags, SceMode mode)
{
	unsigned int unit;

	if (flags != 3 || mode != 0)
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	unit = (unsigned int)(unsigned char)(path[0] - '0');
	if (unit >= PSPEMU_USBPSPCM_MAX_UNITS)
		return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
	return usbPspcmTranslateError(sceUsbPspcmOpen(unit));
}

static void usbPspcmDisabledThread(SceSize argSize, void *argp)
{
	PspemuKermitRequest *req;

	(void)argSize;
	(void)argp;

	for (;;) {
		int cmd = remotefsWaitAndGetRequest(PSPEMU_USBPSPCM_CHANNEL, &req);
		int64_t retVal = (cmd == 3171) ? 0 : PSPEMU_USBPSPCM_ERR_NOT_ACTIVATED;
		remotefsReturnValue(PSPEMU_USBPSPCM_CHANNEL, req, retVal);
	}
}

static int usbPspcmConnectionDetectThread(SceSize argSize, void *argp)
{
	const int32_t *ev = (const int32_t *)argp;

	(void)argSize;
	g_usbPspcmEvent.parts.eventCode = ev[0];
	g_usbPspcmEvent.parts.eventArg = ev[1];

	while (sceCompatInterrupt(13) == PSPEMU_USBPSPCM_COMPAT_INTR_RETRY)
		sceKernelDelayThread(10000U);
	return 0;
}

static int usbPspcmUnregisterNotify(void)
{
	int ret;

	if (sceKernelLockLwMutex(&g_usbPspcmMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (g_usbPspcmNotifyThreadId == -1) {
		ret = PSPEMU_USBPSPCM_VITA_ERR_NOT_REG;
	} else {
		ret = sceUsbPspcmDevctl(PSPEMU_USBPSPCM_DEVCTL_UNREG_NOTIFY,
		                        &g_usbPspcmNotifyThreadId, NULL, 0);
		g_usbPspcmNotifyThreadId = -1;
	}

	if (sceKernelUnlockLwMutex(&g_usbPspcmMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return ret;
}

static int usbPspcmFsExit(void *arg)
{
	int ret;

	(void)arg;
	ret = usbPspcmUnregisterNotify();
	if (ret == PSPEMU_USBPSPCM_VITA_ERR_NOT_REG)
		ret = 0;
	return usbPspcmTranslateError(ret);
}

static int usbPspcmFsDevctl(const char *dev, unsigned int cmd, void *inData, SceSize inLen,
                            void *outData, SceSize outLen)
{
	int ret;

	(void)dev;
	(void)inLen;

	if (cmd == PSPEMU_USBPSPCM_DEVCTL_UNREG_NOTIFY) {
		ret = usbPspcmUnregisterNotify();
		return usbPspcmTranslateError(ret);
	}

	if (cmd == PSPEMU_USBPSPCM_DEVCTL_BIND) {
		const PspemuUsbPspcmBindGuestReq *guestReq = (const PspemuUsbPspcmBindGuestReq *)inData;
		PspemuUsbPspcmBindHostReq hostReq;
		uint32_t timeoutVal;
		int unit;

		hostReq.addrLow = guestReq->addrLow;
		hostReq.addrHigh = guestReq->addrHigh;
		if (guestReq->noTimeout == 1U) {
			hostReq.timeoutPtr = NULL;
		} else {
			timeoutVal = guestReq->timeout;
			hostReq.timeoutPtr = &timeoutVal;
		}
		ret = sceUsbPspcmDevctl(PSPEMU_USBPSPCM_DEVCTL_BIND, &hostReq, &unit, sizeof(unit));
		if (ret == 0)
			snprintf((char *)outData, outLen, "usbpspcm%d:", unit);
		return usbPspcmTranslateError(ret);
	}

	if (cmd != PSPEMU_USBPSPCM_DEVCTL_REG_NOTIFY) {
		ret = sceUsbPspcmDevctl(cmd, inData, outData, outLen);
		return usbPspcmTranslateError(ret);
	}

	if (sceKernelLockLwMutex(&g_usbPspcmMutex, 1, NULL) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (g_usbPspcmNotifyThreadId == -1) {
		SceUID thid = sceKernelCreateThread("ScePspemuConnectionDetect",
		                                    (SceKernelThreadEntry)usbPspcmConnectionDetectThread,
		                                    95, 0x1000U, 0, 0x10000, NULL);
		g_usbPspcmNotifyThreadId = thid;
		if (thid < 0) {
			g_usbPspcmNotifyThreadId = -1;
			if (sceKernelUnlockLwMutex(&g_usbPspcmMutex, 1) < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			return PSPEMU_USBPSPCM_ERR_INVALID_ARG;
		}
	}

	ret = sceUsbPspcmDevctl(PSPEMU_USBPSPCM_DEVCTL_REG_NOTIFY, &g_usbPspcmNotifyThreadId, NULL, 0);
	if (sceKernelUnlockLwMutex(&g_usbPspcmMutex, 1) < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	return usbPspcmTranslateError(ret);
}

int64_t remoteUsbPspcmGetEventPair(void)
{
	return g_usbPspcmEvent.raw;
}

int remoteUsbPspcmInit(void)
{
	SceUID thid;
	int ret;

	if (remoteUsbIsPspcmEnabled() != 0) {
		char name[32];
		int i;

		memset(g_usbPspcmChannelMode, 0, sizeof(g_usbPspcmChannelMode));

		for (i = 0; i < PSPEMU_USBPSPCM_MAX_UNITS; i++) {
			snprintf(name, 30U, "ScePspemuRemotePspcm%d", i);
			thid = sceKernelCreateThread(name, (SceKernelThreadEntry)usbPspcmWorkerThread,
			                             96, 0x1000U, 0, 0x10000, NULL);
			if (thid <= 0)
				return thid;
			ret = sceKernelStartThread(thid, 0, NULL);
			if (ret != 0)
				return ret;
		}

		ret = sceKernelCreateLwMutex(&g_usbPspcmMutex, "ScePspemuUsbPspcm", 0, 0, NULL);
		if (ret == 0)
			g_usbPspcmNotifyThreadId = -1;
		return ret;
	}

	thid = sceKernelCreateThread("ScePspemuRemotePspcm0",
	                             (SceKernelThreadEntry)usbPspcmDisabledThread,
	                             96, 0x1000U, 0, 0x10000, NULL);
	if (thid <= 0)
		return thid;
	return sceKernelStartThread(thid, 0, NULL);
}

int remoteUsbPspcmSuspend(void)
{
	if (remoteUsbIsPspcmEnabled() != 0) {
		int retry;
		int ret = 0;

		for (retry = 0; retry < 20; retry++) {
			ret = sceUsbPspcmStopDevice();
			if (ret == 0)
				return 0;
			sceKernelDelayThread(500000U);
		}
		if (ret < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}
	return 0;
}
