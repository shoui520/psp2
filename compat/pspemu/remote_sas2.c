/*
 * remote_sas2.c: Media Engine SAS2 synthesizer thread and dispatch bridge
 * for ScePspemu.
 */
#include "host_common.h"

#include <string.h>

#define SAS2_VOICE_COUNT          32
#define SAS2_CONTEXT_COPY_SIZE    0xE20U
#define SAS2_CONTEXT_FULL_SIZE    0xE40U
#define SAS2_VOICE_STATE_OFFSET   0x714U
#define SAS2_VOICE_STATE_SIZE     0x700U

#define SAS2_EVF_SLOT_A           0x00000001U
#define SAS2_EVF_SLOT_B           0x00000002U
#define SAS2_EVF_IDLE             0x00000004U

typedef struct Sas2VoiceState {
	uint32_t leadWords[2];
	uint8_t  leadPad;
	uint8_t  statusByte;
	uint16_t pitchWord;
	uint32_t samplePos;
	uint32_t adpcmLoopAddr;
	uint32_t sampleAddr;
	uint32_t envWords[4];
	uint8_t  midState[11];
	uint8_t  endFlag;
	uint32_t voiceAttr;
} Sas2VoiceState;

typedef struct Sas2Context {
	uint32_t       workBase;
	uint32_t       headerWord;
	uint8_t        grainUnits;
	uint8_t        stereoFlag;
	uint8_t        headerPad[2];
	uint32_t       voiceParams[450];
	Sas2VoiceState voices[SAS2_VOICE_COUNT];
	uint32_t       endFlags;
	uint32_t       trailerWords[10];
} Sas2Context;

typedef struct Sas2DispatchSlot {
	int      cmd;
	uint32_t ctxGuestAddr;
	uint32_t outGuestAddr;
	uint32_t argC;
	uint32_t argD;
	uint32_t argE;
	uint32_t padWord;
} Sas2DispatchSlot;

SceUID           g_sas2EventFlag;
uint32_t         g_sas2WriteSeq;
Sas2DispatchSlot g_sas2Slots[2];
int              g_sas2AsyncEnabled;
uint32_t         g_sas2ReadSeq;
Sas2Context      g_sas2Context;

static int remoteSas2Thread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	while (sceKernelSetEventFlag(g_sas2EventFlag, SAS2_EVF_IDLE) >= 0) {
		uint32_t seq = g_sas2ReadSeq;
		unsigned int slotIdx = seq & 1U;
		if (sceKernelWaitEventFlag(g_sas2EventFlag, 1U << slotIdx, 5U, NULL, NULL) < 0)
			break;

		Sas2DispatchSlot *slot = &g_sas2Slots[slotIdx];
		int cmd = slot->cmd;
		uint32_t argC = slot->argC;
		uint32_t argD = slot->argD;
		uint32_t argE = slot->argE;

		Sas2Context *guestCtx = (Sas2Context *)remotefsTranslateUserGuestAddr(slot->ctxGuestAddr, 1, SAS2_CONTEXT_FULL_SIZE);
		memcpy(&g_sas2Context, guestCtx, SAS2_CONTEXT_COPY_SIZE);

		uint32_t outBytes = ((uint32_t)g_sas2Context.grainUnits +
		                     (uint32_t)g_sas2Context.stereoFlag * (uint32_t)g_sas2Context.grainUnits) << 7;
		void *outBuf = remotefsTranslateUserGuestAddr(slot->outGuestAddr, 1, outBytes);

		for (int i = 0; i < SAS2_VOICE_COUNT; i++) {
			uint32_t guestSample = g_sas2Context.voices[i].sampleAddr;
			void *hostSample = meEdramGetBase(guestSample);
			if (hostSample == NULL)
				hostSample = remotefsTranslateUserGuestAddr(guestSample, 2, 0U);
			g_sas2Context.voices[i].sampleAddr = (uint32_t)(uintptr_t)hostSample;
		}

		uint32_t guestBase = g_sas2Context.workBase;
		void *hostBase = meEdramGetBase(guestBase);
		if (hostBase == NULL)
			hostBase = remotefsTranslateUserGuestAddr(guestBase, 2, 0U);
		g_sas2Context.workBase = (uint32_t)(uintptr_t)hostBase;

		if (cmd == 0)
			pspemuSas2CoreMix(&g_sas2Context, outBuf, argC);
		else if (cmd == 1)
			pspemuSas2CoreMixWithVoice(&g_sas2Context, outBuf, argC, argD, argE);

		guestCtx->endFlags = g_sas2Context.endFlags;
		g_sas2ReadSeq = seq + 1U;

		void *dstVoices = memcpy(guestCtx->voices, g_sas2Context.voices, SAS2_VOICE_STATE_SIZE);
		uintptr_t flushStart = (uintptr_t)dstVoices & ~0x3FU;
		uintptr_t flushEnd = ((uintptr_t)guestCtx + 3667U) & ~0x3FU;
		remotefsFlushGuestAddr((void *)flushStart, (SceSize)(flushEnd - flushStart));
		remotefsFlushGuestAddr(outBuf, outBytes);
	}
	return 0;
}

int remoteSas2MeDispatch(int cmd, uint32_t ctxGuestAddr, uint32_t outGuestAddr,
                         uint32_t argC, uint32_t argD, uint32_t argE)
{
	if (g_sas2AsyncEnabled != 0) {
		if (sceKernelWaitEventFlag(g_sas2EventFlag, SAS2_EVF_IDLE, 5U, NULL, NULL) < 0)
			return -1;
		unsigned int slotIdx = (g_sas2WriteSeq++) & 1U;
		Sas2DispatchSlot *slot = &g_sas2Slots[slotIdx];
		slot->cmd = cmd;
		slot->ctxGuestAddr = ctxGuestAddr;
		slot->outGuestAddr = outGuestAddr;
		slot->argC = argC;
		slot->argD = argD;
		slot->argE = argE;
		sceKernelSetEventFlag(g_sas2EventFlag, 1U << slotIdx);
		return (sceKernelWaitEventFlag(g_sas2EventFlag, SAS2_EVF_IDLE, 1U, NULL, NULL) < 0) ? -1 : 0;
	}

	Sas2Context *guestCtx = (Sas2Context *)remotefsTranslateUserGuestAddr(ctxGuestAddr, 1, SAS2_CONTEXT_FULL_SIZE);
	memcpy(&g_sas2Context, guestCtx, SAS2_CONTEXT_COPY_SIZE);

	void *hostBase = meEdramGetBase(g_sas2Context.workBase);
	if (hostBase == NULL)
		return -1;
	g_sas2Context.workBase = (uint32_t)(uintptr_t)hostBase;

	for (int i = 0; i < SAS2_VOICE_COUNT; i++) {
		void *hostSample = meEdramGetBase(g_sas2Context.voices[i].sampleAddr);
		if (hostSample == NULL)
			return -1;
		g_sas2Context.voices[i].sampleAddr = (uint32_t)(uintptr_t)hostSample;
	}

	uint32_t outBytes = ((uint32_t)g_sas2Context.grainUnits +
	                     (uint32_t)g_sas2Context.stereoFlag * (uint32_t)g_sas2Context.grainUnits) << 7;
	void *outBuf = remotefsTranslateUserGuestAddr(outGuestAddr, 1, outBytes);

	if (cmd == 0)
		pspemuSas2CoreMix(&g_sas2Context, outBuf, argC);
	else if (cmd == 1)
		pspemuSas2CoreMixWithVoice(&g_sas2Context, outBuf, argC, argD, argE);

	guestCtx->endFlags = g_sas2Context.endFlags;
	remotefsFlushGuestAddr(&guestCtx->endFlags, sizeof(guestCtx->endFlags));

	for (int i = 0; i < SAS2_VOICE_COUNT; i++) {
		Sas2VoiceState *dst = &guestCtx->voices[i];
		const Sas2VoiceState *src = &g_sas2Context.voices[i];
		dst->statusByte = src->statusByte;
		dst->pitchWord = src->pitchWord;
		dst->samplePos = src->samplePos;
		dst->adpcmLoopAddr = src->adpcmLoopAddr;
		dst->envWords[0] = src->envWords[0];
		dst->envWords[1] = src->envWords[1];
		dst->envWords[2] = src->envWords[2];
		dst->envWords[3] = src->envWords[3];
		dst->endFlag = src->endFlag;
		dst->voiceAttr = src->voiceAttr;
	}

	uintptr_t flushStart = (uintptr_t)guestCtx->voices & ~0x3FU;
	uintptr_t flushEnd = ((uintptr_t)guestCtx + 3667U) & ~0x3FU;
	remotefsFlushGuestAddr((void *)flushStart, (SceSize)(flushEnd - flushStart));
	return 0;
}

int remoteSas2Init(void)
{
	memset(&g_sas2Context, 0, sizeof(g_sas2Context));
	int ret = pspemuSas2CoreInit(&g_sas2Context);
	if (ret == 0)
		return 0;

	memset(g_sas2Slots, 0, sizeof(g_sas2Slots));
	g_sas2AsyncEnabled = 1;

	g_sas2EventFlag = sceKernelCreateEventFlag("ScePspemuSas2", 0x1000U, 0U, NULL);
	if (g_sas2EventFlag <= 0)
		return g_sas2EventFlag;

	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemoteSas2",
		remoteSas2Thread,
		95,
		0x1000U,
		0,
		0x40000,
		NULL);
	if (thid < 0)
		return thid;
	return sceKernelStartThread(thid, 0, NULL);
}
