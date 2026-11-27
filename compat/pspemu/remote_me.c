/*
 * remote_me.c: Kermit Media Engine IPC server (channels 6 and 8) and
 * sub-dispatchers for ATRAC3+, ATRAC3, MP3, AAC, G.729, AudioMixer, and SAS.
 */
#include "host_common.h"

#include <string.h>

#define ME_KERMIT_CMD_RESET_JPEG_WORK  0xC5E
#define ME_KERMIT_CMD_SET_AVC_PTS      0xC5F
#define ME_KERMIT_CMD_EXEC             0xC60
#define ME_ERROR_UNSUPPORTED_OP        ((int)0x80010058)
#define ME_ERROR_BAD_THREAD_ARG        ((int)0x80020005)
#define ME_MIXER_ERROR_INVALID_ARG     ((int)0x80470003)
#define ME_MIXER_ERROR_ALREADY_INIT    ((int)0x80470002)

typedef struct MeCodecWorkPacket {
	uint32_t headerWords[2];
	uint32_t errorCode;
	uint32_t workCtxAddr;
	uint32_t reservedWords[2];
	uint32_t esBufAddr;
	uint32_t consumedBytes;
	uint32_t pcmBufAddr;
	uint32_t pcmOutBytes;
	uint32_t codecParams[16];
} MeCodecWorkPacket;

typedef struct MeMixerParamPair {
	uint32_t firstAddr;
	uint32_t secondAddr;
} MeMixerParamPair;

uint32_t g_meMixerMaxStreams;
uint32_t g_meMixerGrainSamples;
uint32_t g_meSasBufferBytes;

static int remoteMeThread(SceSize args, void *argp)
{
	PspemuKermitRequest *req;

	if (args != sizeof(int))
		return ME_ERROR_BAD_THREAD_ARG;

	int channel = *(const int *)argp;
	for (;;) {
		int64_t retVal;
		int kermitCmd = remotefsWaitAndGetRequest(channel, &req);

		if (kermitCmd == ME_KERMIT_CMD_SET_AVC_PTS) {
			retVal = remoteMeAvcdecSetTimestamp(
				(uint32_t)req->args[0],
				(uint32_t)req->args[1],
				(uint32_t)req->args[2],
				(uint32_t)req->args[3]);
		} else if (kermitCmd == ME_KERMIT_CMD_RESET_JPEG_WORK) {
			sceJpegWorkReset();
			retVal = 0;
		} else if (kermitCmd == ME_KERMIT_CMD_EXEC) {
			uint32_t meCmd = (uint32_t)req->args[0];
			uint32_t argA = (uint32_t)req->args[1];
			uint32_t argB = (uint32_t)req->args[2];
			uint32_t argC = (uint32_t)req->args[3];
			uint32_t argD = (uint32_t)req->args[4];
			uint32_t argE = (uint32_t)req->args[5];
			uint32_t argF = (uint32_t)req->args[6];
			uint32_t argG = (uint32_t)req->args[7];
			uint32_t argH = (uint32_t)req->args[8];

			switch (meCmd) {
			case 0: case 1: case 2: case 3: case 4: case 5: case 6:
			case 7: case 8: case 9: case 10: case 11: case 16:
				retVal = remoteMeAvcdecDispatch((int)meCmd, argA, argB, argC, argD, argE);
				break;
			case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
				retVal = remoteMeUnusedVideoDispatch((int)meCmd);
				break;
			case 64: case 65: case 66: case 67: case 68: case 69: case 70: case 71:
			case 224: case 225: case 226: case 227: case 228: case 229: case 230:
				pspemuTriggerFatalError(ME_ERROR_UNSUPPORTED_OP);
			case 96: case 99: case 100: case 103: case 104: case 105: case 106:
				retVal = remoteMeAtxDispatch((int)meCmd, argA, argB, argC, argD, argE);
				break;
			case 112: case 113: case 115:
				retVal = remoteMeAt3Dispatch((int)meCmd, argA, argB, argC, argD, argE);
				break;
			case 130: case 137: case 139: case 140:
				retVal = remoteMeMp3Dispatch((int)meCmd, argA, argB, argC, argD);
				break;
			case 144: case 145: case 147: case 149: case 151:
				retVal = remoteMeAacDispatch((int)meCmd, argA, argB, argC, argD, argE);
				break;
			case 256: case 257:
				retVal = remoteSas2MeDispatch((int)(meCmd - 256U), argA, argB, argC, argD, argE);
				break;
			case 264: case 265: case 266:
				retVal = remoteMeMixerDispatch((int)meCmd, argA, argB, argC);
				break;
			case 288: case 289: case 290: case 291:
				retVal = remoteMeSasDispatch((int)meCmd, argA, argB, argC, argD, argE, argF);
				break;
			case 292: case 293: case 294:
				retVal = 0;
				break;
			case 320: case 321: case 322: case 323: case 324: case 325: case 326: case 327:
				retVal = remoteMeJpegDispatch((int)meCmd, argA, argB, argC, argD, argE, argF);
				break;
			case 336: case 337: case 338:
				retVal = remoteMeCscDispatch((int)meCmd, argA, argB, argC, argD, argE, argF, argG, argH);
				break;
			case 352: case 353: case 354: case 355:
				retVal = remoteMeG729Dispatch((int)meCmd, argA, argB, argC, argD, argE);
				break;
			case 384: case 385: case 386: case 387: case 388: case 389:
			case 390: case 391: case 392: case 398: case 399:
				retVal = remoteMeWorkDispatch((int)meCmd, argA, argB);
				break;
			default:
				pspemuRecordTitleListFlags(16U);
				retVal = -1;
				break;
			}
		} else {
			pspemuRecordTitleListFlags(16U);
			retVal = -1;
		}

		remotefsReturnValue(channel, req, retVal);
	}
}

static int __attribute__((noinline, noclone)) remoteMeStartThread(int channel, const char *name)
{
	int threadChannel = channel;
	SceUID thid = sceKernelCreateThread(
		name,
		remoteMeThread,
		96,
		0x40000U,
		0,
		0x40000,
		NULL);
	if (thid >= 0)
		return sceKernelStartThread(thid, sizeof(threadChannel), &threadChannel);
	return thid;
}

int remoteMeInit(void)
{
	int ret = remoteMeStartThread(6, "ScePspemuRemoteMe");
	if (ret != 0)
		return ret;

	int meFlags = pspemuCompatGetParam(14);
	if (meFlags != -1 && (meFlags & 4) != 0) {
		ret = remoteMeStartThread(8, "ScePspemuRemoteMe2");
		if (ret != 0)
			return ret;
	}

	ret = remoteMeJpegInit();
	if (ret != 0)
		return ret;
	return remoteMeAvcdecInit();
}

int remoteMeAtxDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE)
{
	MeCodecWorkPacket packet;
	void *workCtx;

	switch (cmd) {
	case 96: {
		if (argA == 0U)
			return -1;
		const void *guestPacket = remotefsTranslateUserGuestAddr(argA, 1, sizeof(packet));
		memcpy(&packet, guestPacket, sizeof(packet));

		void *pcmHost = remotefsTranslateUserGuestAddr(packet.pcmBufAddr, 2, 0U);
		packet.pcmBufAddr = (uint32_t)(uintptr_t)pcmHost;
		SceSize esSize = (packet.codecParams[2] != 0U) ? 4106U : (packet.codecParams[6] + 2U);
		packet.esBufAddr = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(packet.esBufAddr, 1, esSize);

		if (sceJpegWorkGuestToHost(packet.workCtxAddr, &workCtx) != 0)
			return -2;
		packet.workCtxAddr = (uint32_t)(uintptr_t)workCtx;

		int ret = meAtxDecodeFrame(&packet);
		if (packet.pcmOutBytes != 0U)
			remotefsFlushGuestAddr(pcmHost, packet.pcmOutBytes);

		*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 36U, 2, 4U) = packet.pcmOutBytes;
		*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 28U, 2, 4U) = packet.consumedBytes;
		uint32_t *guestParams = (uint32_t *)remotefsTranslateUserGuestAddr(argA + 40U, 2, 36U);
		memcpy(guestParams, packet.codecParams, 36U);
		return ret;
	}
	case 99: {
		if (argA == 0U || argB == 0U || argC == 0U || argD == 0U)
			return -1;
		uint32_t *outRate = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 2, 0U);
		uint32_t *outCh = (uint32_t *)remotefsTranslateUserGuestAddr(argB, 2, 0U);
		uint32_t *outFrame = (uint32_t *)remotefsTranslateUserGuestAddr(argC, 2, 0U);
		const uint8_t *hdr = (const uint8_t *)remotefsTranslateUserGuestAddr(argD, 1, 8U);
		int ret = meAtxParseHeader(outRate, outCh, outFrame, hdr);
		remotefsFlushGuestAddr(outRate, 4U);
		remotefsFlushGuestAddr(outCh, 4U);
		remotefsFlushGuestAddr(outFrame, 4U);
		return ret;
	}
	case 100: {
		if (argA == 0U || argB == 0U)
			return -1;
		uint32_t *outSize = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 2, 0U);
		if (sceJpegWorkGuestToHost(argB, &workCtx) != 0)
			return -2;
		int ret = meAtxGetOutputSize(outSize, workCtx);
		remotefsFlushGuestAddr(outSize, 4U);
		return ret;
	}
	case 103:
		if (argE == 0U)
			return -1;
		if (sceJpegWorkGuestToHost(argE, &workCtx) != 0)
			return -2;
		return meAtxInitContext(argA, argB, argC, argD, workCtx);
	case 104:
		return (argB != 0U) ? 0 : -1;
	case 105: {
		if (argA == 0U || argB == 0U)
			return -1;
		const uint32_t *inCh = (const uint32_t *)remotefsTranslateUserGuestAddr(argA, 1, 4U);
		if (sceJpegWorkGuestToHost(argB, &workCtx) != 0)
			return -2;
		return meAtxSetChannels(inCh, workCtx);
	}
	case 106: {
		if (argA == 0U || argB == 0U)
			return -1;
		uint32_t *outErr = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 2, 0U);
		if (sceJpegWorkGuestToHost(argB, &workCtx) != 0)
			return -2;
		int ret = meAtxGetInternalError(outErr, workCtx);
		remotefsFlushGuestAddr(outErr, 4U);
		return ret;
	}
	default:
		return -1;
	}
}

int remoteMeAt3Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE)
{
	MeCodecWorkPacket packet;
	void *workCtx = NULL;

	if (cmd == 113) {
		if (argA == 0U || argB == 0U)
			return -1;
		uint32_t *outErr = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 2, 0U);
		if (sceJpegWorkGuestToHost(argB, &workCtx) != 0)
			return -2;
		int ret = meAt3GetInternalError(outErr, workCtx);
		remotefsFlushGuestAddr(outErr, 4U);
		return ret;
	}
	if (cmd == 115) {
		if (argD == 0U)
			return -1;
		if (sceJpegWorkGuestToHost(argD, &workCtx) != 0)
			return -2;
		return meAt3InitContext(argA, argB, argC, workCtx, argE);
	}
	if (cmd != 112 || argA == 0U)
		return -1;

	const void *guestPacket = remotefsTranslateUserGuestAddr(argA, 1, sizeof(packet));
	memcpy(&packet, guestPacket, sizeof(packet));
	void *pcmHost = remotefsTranslateUserGuestAddr(packet.pcmBufAddr, 2, 0U);
	packet.pcmBufAddr = (uint32_t)(uintptr_t)pcmHost;
	packet.esBufAddr = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(packet.esBufAddr, 1, packet.codecParams[2] * 2U + 1U);

	if (sceJpegWorkGuestToHost(packet.workCtxAddr, &workCtx) != 0)
		return -2;
	packet.workCtxAddr = (uint32_t)(uintptr_t)workCtx;

	int ret = meAt3DecodeFrame(&packet);
	if (packet.pcmOutBytes != 0U)
		remotefsFlushGuestAddr(pcmHost, packet.pcmOutBytes);

	*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 36U, 2, 4U) = packet.pcmOutBytes;
	*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 28U, 2, 4U) = packet.consumedBytes;
	uint32_t *guestParams = (uint32_t *)remotefsTranslateUserGuestAddr(argA + 40U, 2, 16U);
	memcpy(guestParams, packet.codecParams, 16U);
	return ret;
}

int remoteMeMp3Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD)
{
	MeCodecWorkPacket packet;
	void *workCtx = NULL;

	switch (cmd) {
	case 130: {
		if (argA == 0U || argB == 0U || argC == 0U || argD == 0U)
			return -1;
		const uint8_t *esBuf = (const uint8_t *)remotefsTranslateUserGuestAddr(argA, 1, 1441U);
		uint32_t *outBitOffset = (uint32_t *)remotefsTranslateUserGuestAddr(argB, 2, 0U);
		void *outInfo = remotefsTranslateUserGuestAddr(argC, 1, 48U);
		if (sceJpegWorkGuestToHost(argD, &workCtx) != 0)
			return -2;
		int ret = meMp3FindSyncAndParse(esBuf, outBitOffset, outInfo, workCtx);
		remotefsFlushGuestAddr(outInfo, 48U);
		remotefsFlushGuestAddr(outBitOffset, 4U);
		return ret;
	}
	case 137:
		if (argB == 0U || argA != 1441U)
			return -1;
		if (sceJpegWorkGuestToHost(argB, &workCtx) != 0)
			return -2;
		return meMp3SetFrameSize(argA, workCtx);
	case 139:
		if (argA == 0U)
			return -1;
		if (sceJpegWorkGuestToHost(argA, &workCtx) != 0)
			return -2;
		return meMp3ResetContext(workCtx);
	case 140: {
		if (argA == 0U)
			return -1;
		const void *guestPacket = remotefsTranslateUserGuestAddr(argA, 1, sizeof(packet));
		memcpy(&packet, guestPacket, sizeof(packet));
		packet.esBufAddr = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(packet.esBufAddr, 1, 1993U);
		void *pcmHost = remotefsTranslateUserGuestAddr(packet.pcmBufAddr, 2, 0U);
		packet.pcmBufAddr = (uint32_t)(uintptr_t)pcmHost;
		if (sceJpegWorkGuestToHost(packet.workCtxAddr, &workCtx) != 0)
			return -2;
		packet.workCtxAddr = (uint32_t)(uintptr_t)workCtx;

		int ret = meMp3DecodeFrame(&packet);
		if (packet.pcmOutBytes != 0U)
			remotefsFlushGuestAddr(pcmHost, packet.pcmOutBytes);

		*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 28U, 2, 4U) = packet.consumedBytes;
		*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 36U, 2, 4U) = packet.pcmOutBytes;
		*(uint32_t *)remotefsTranslateUserGuestAddr(argA + 8U, 2, 4U) = packet.errorCode;
		uint32_t *guestParams = (uint32_t *)remotefsTranslateUserGuestAddr(argA + 40U, 2, 64U);
		memcpy(guestParams, packet.codecParams, 64U);
		return ret;
	}
	default:
		return -1;
	}
}

int remoteMeAacDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE)
{
	switch (cmd) {
	case 144: {
		if (argA == 0U || argB == 0U || argC == 0U || argD == 0U || argE == 0U)
			return -1;
		const void *esBuf = remotefsTranslateUserGuestAddr(argA, 1, 1545U);
		uint32_t *outConsumed = (uint32_t *)remotefsTranslateUserGuestAddr(argB, 2, 0U);
		void *pcmBuf = remotefsTranslateUserGuestAddr(argC, 2, 0U);
		uint32_t *outPcmBytes = (uint32_t *)remotefsTranslateUserGuestAddr(argD, 2, 0U);
		void *workCtx = remotefsTranslateUserGuestAddr(argE, 1, 8U);
		int ret = meAacDecodeFrame(esBuf, outConsumed, pcmBuf, outPcmBytes, workCtx);
		remotefsFlushGuestAddr(outConsumed, 4U);
		if ((int32_t)*outPcmBytes > 0)
			remotefsFlushGuestAddr(pcmBuf, *outPcmBytes);
		remotefsFlushGuestAddr(outPcmBytes, 4U);
		return ret;
	}
	case 145: {
		if (argA == 0U || argB == 0U)
			return -1;
		uint32_t *outErr = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 1, 4U);
		void *workCtx = remotefsTranslateUserGuestAddr(argB, 1, 8U);
		int ret = meAacGetInternalError(outErr, workCtx);
		remotefsFlushGuestAddr(outErr, 4U);
		return ret;
	}
	case 147: {
		if (argB == 0U)
			return -1;
		void *workCtx = remotefsTranslateUserGuestAddr(argB, 1, 8U);
		int ret = meAacInitContext(argA, workCtx);
		remotefsFlushGuestAddr(workCtx, 8U);
		return ret;
	}
	case 149: {
		if (argB == 0U)
			return -1;
		void *workCtx = remotefsTranslateUserGuestAddr(argB, 1, 8U);
		int ret = meAacSetSampleRate((uint8_t)argA, workCtx);
		remotefsFlushGuestAddr(workCtx, 8U);
		return ret;
	}
	case 151: {
		if (argD == 0U)
			return -1;
		void *workCtx = remotefsTranslateUserGuestAddr(argD, 1, 8U);
		int ret = meAacSetOption(argA, argB, argC, workCtx);
		remotefsFlushGuestAddr(workCtx, 8U);
		return ret;
	}
	default:
		return -1;
	}
}

int remoteMeG729Dispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE)
{
	switch (cmd) {
	case 352: {
		if (argA == 0U)
			return -1;
		uint32_t *workCtx = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 1, 8U);
		int ret = meG729EncodeInit(workCtx, argB, argC);
		remotefsFlushGuestAddr(workCtx, 8U);
		return ret;
	}
	case 353: {
		if (argA == 0U || argB == 0U || argC == 0U)
			return -1;
		uint32_t *workCtx = (uint32_t *)remotefsTranslateUserGuestAddr(argA, 1, 8U);
		const void *pcmIn = remotefsTranslateUserGuestAddr(argB, 1, 160U);
		void *bitstreamOut = remotefsTranslateUserGuestAddr(argC, 2, 0U);
		int ret = meG729EncodeCore(workCtx, pcmIn, bitstreamOut);
		if (ret > 0)
			remotefsFlushGuestAddr(bitstreamOut, (SceSize)(ret >> 3));
		return ret;
	}
	case 354: {
		if (argA == 0U)
			return -1;
		void *workCtx = remotefsTranslateUserGuestAddr(argA, 1, 8U);
		int ret = meG729DecodeInit(workCtx);
		remotefsFlushGuestAddr(workCtx, 8U);
		return ret;
	}
	case 355: {
		if (argA == 0U || argD == 0U || argE == 0U)
			return -1;
		void *workCtx = remotefsTranslateUserGuestAddr(argA, 1, 8U);
		const void *bitstreamIn = remotefsTranslateUserGuestAddr(argD, 1, 15U);
		void *pcmOut = remotefsTranslateUserGuestAddr(argE, 2, 0U);
		int ret = meG729DecodeCore(workCtx, (int)argB, (int)argC, bitstreamIn, pcmOut);
		remotefsFlushGuestAddr(pcmOut, 160U);
		return ret;
	}
	default:
		return 0;
	}
}

int remoteMeMixerDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC)
{
	void *hostStreams[4];
	MeMixerParamPair params;

	switch (cmd) {
	case 264:
		if ((argA - 1U) > 3U || (argB - 64U) > 0x7C0U || (argB & 0x1FU) != 0U)
			return ME_MIXER_ERROR_INVALID_ARG;
		if (g_meMixerMaxStreams != 0U || g_meMixerGrainSamples != 0U)
			return ME_MIXER_ERROR_ALREADY_INIT;
		g_meMixerMaxStreams = argA;
		g_meMixerGrainSamples = argB;
		return 0;
	case 265: {
		if (argA == 0U || argB == 0U || argC == 0U)
			return -1;
		memset(hostStreams, 0, sizeof(hostStreams));
		const MeMixerParamPair *guestPair = (const MeMixerParamPair *)remotefsTranslateUserGuestAddr(argA, 1, sizeof(params));
		params = *guestPair;
		const uint32_t *guestStreamAddrs = (const uint32_t *)remotefsTranslateUserGuestAddr(argB, 1, 4U * g_meMixerMaxStreams);
		int16_t *outStereo = (int16_t *)remotefsTranslateUserGuestAddr(argC, 2, 0U);
		memset(outStereo, 0, 4U * g_meMixerGrainSamples);

		for (uint32_t i = 0; i < g_meMixerMaxStreams; i++) {
			if (guestStreamAddrs[i] != 0U)
				hostStreams[i] = remotefsTranslateUserGuestAddr(guestStreamAddrs[i], 1, 2U * g_meMixerGrainSamples);
		}
		if (params.firstAddr == 0U || params.secondAddr == 0U)
			return -1;
		params.firstAddr = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(params.firstAddr, 1, 8U);
		params.secondAddr = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(params.secondAddr, 1, 40U);

		int ret = meAudioMixerMix(&params, hostStreams, outStereo, g_meMixerMaxStreams, (int)g_meMixerGrainSamples);
		remotefsFlushGuestAddr(outStereo, 2U * g_meMixerGrainSamples);
		return ret;
	}
	case 266:
		g_meMixerMaxStreams = 0U;
		g_meMixerGrainSamples = 0U;
		return 0;
	default:
		return -1;
	}
}

int remoteMeSasDispatch(int cmd, uint32_t argA, uint32_t argB, uint32_t argC, uint32_t argD, uint32_t argE, uint32_t argF)
{
	switch (cmd) {
	case 288:
		if (argA == 0U)
			return -1;
		return meSasInit(remotefsTranslateUserGuestAddr(argA, 1, 4U), argB, argC);
	case 289: {
		if (argA == 0U)
			return -1;
		void *ctx = remotefsTranslateUserGuestAddr(argA, 1, 4U);
		g_meSasBufferBytes = 4U * argB;
		return meSasSetOutputSize(ctx, argB, argC, argD, argE, argF);
	}
	case 290: {
		if (argA == 0U || argB == 0U || argC == 0U)
			return -1;
		void *ctx = remotefsTranslateUserGuestAddr(argA, 1, 4U);
		const void *srcBuf = remotefsTranslateUserGuestAddr(argB, 1, g_meSasBufferBytes);
		void *dstBuf = remotefsTranslateUserGuestAddr(argC, 2, 0U);
		int ret = meSasCopyBuffer(ctx, srcBuf, dstBuf);
		remotefsFlushGuestAddr(dstBuf, g_meSasBufferBytes);
		return ret;
	}
	case 291:
		if (argA == 0U)
			return -1;
		return meSasTerm(remotefsTranslateUserGuestAddr(argA, 1, 4U), argB, argC);
	default:
		return -1;
	}
}
