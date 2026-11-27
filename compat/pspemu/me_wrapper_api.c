/*
 * me_wrapper_api.c: Media Engine audio codec wrappers (ATRAC3, ATRAC3+,
 * MP3, AAC, G.729, AudioMixer, SAS) backed by SceAudiocodec, SceAudiodec,
 * and SceG729 for ScePspemu.
 */
#include "host_common.h"

#include <string.h>

#define ME_ADEC_MAX_SLOTS          16
#define ME_ADEC_CODEC_AT3PLUS      0x1000
#define ME_ADEC_CODEC_AT3          0x1001
#define ME_ADEC_CODEC_MP3          0x1002
#define ME_ADEC_CODEC_AAC          0x1003

#define ME_ADEC_BACKEND_NONE       0
#define ME_ADEC_BACKEND_AUDIODEC   1
#define ME_ADEC_BACKEND_AUDIOCODEC 2

#define ME_AT3_ERROR_BAD_FRAME     ((int)0x807F8001)

typedef struct MeAdecSlot {
	union {
		struct {
			SceAudiodecCtrl ctrl;
			SceAudiodecInfo info;
			uint32_t        padWords[4];
		} audiodec;
		struct {
			uint32_t errorCode;
			void    *workBuf;
			uint32_t workSize;
			uint32_t reservedA;
			const void *esBuf;
			uint32_t consumedBytes;
			void    *pcmBuf;
			uint32_t pcmOutBytes;
			uint32_t configWords[16];
		} audiocodec;
		uint32_t rawWords[24];
	} dec;
	uint32_t backendType;
	uint32_t internalError;
	void    *keyPtr;
	size_t   keySize;
	union {
		uint8_t  bytes[64];
		uint32_t words[16];
	} keySnapshot;
} MeAdecSlot;

typedef struct MeAt3ModeEntry {
	int channelMode;
	int frameBytes;
} MeAt3ModeEntry;

typedef struct MeAt3WorkCtx {
	uint32_t channelMode;
	uint32_t sampleRate;
	uint32_t frameBytes;
	uint32_t outputMode;
} MeAt3WorkCtx;

typedef struct MeAtxWorkCtx {
	uint32_t reservedA;
	uint32_t outputChannels;
	uint32_t jointStereoFlag;
	uint32_t sampleRate;
	uint32_t extraParam;
	uint32_t channelMode;
	uint32_t frameBytes;
	uint32_t outputSamples;
	uint32_t outputMode;
} MeAtxWorkCtx;

typedef struct MeAtxAsyncSlot {
	const uint8_t *esBuf;
	uint32_t       consumedBytes;
	uint32_t       pcmOutBytes;
	MeAtxWorkCtx  *workCtx;
	uint32_t       headerPad[60];
	int16_t        pcmBuf[8192];
} MeAtxAsyncSlot;

typedef struct MeMp3HeaderInfo {
	int syncWord;
	int mpegVersion;
	int layer;
	int protectionBit;
	int bitrateIndex;
	int sampleRateIndex;
	int paddingBit;
	int privateBit;
	int channelMode;
	int modeExtension;
	int copyright;
	int original;
	int emphasis;
	int bitrateKbps;
	int sampleRateHz;
	int channels;
} MeMp3HeaderInfo;

typedef struct MeMp3WorkCtx {
	int             mpegVersion;
	int             channels;
	int             esSize;
	int             needSyncSearch;
	int             syncOffset;
	MeMp3HeaderInfo header;
} MeMp3WorkCtx;

typedef struct MeMp3OutInfo {
	int versionFlag;
	int channelCount;
	int reservedA;
	int bitrateIndex;
	int sampleRateIndex;
	int paddingBit;
	int privateBit;
	int channelMode;
	int modeExtension;
	int copyright;
	int original;
	int emphasis;
} MeMp3OutInfo;

typedef struct MeAacWorkCtx {
	uint32_t sampleRate;
	uint8_t  rateCode;
	uint8_t  optionFlag;
	uint8_t  padBytes[2];
} MeAacWorkCtx;

typedef struct MeCodecPacket {
	uint32_t headerWords[2];
	uint32_t errorCode;
	void    *workCtx;
	uint32_t reservedWords[2];
	void    *esBuf;
	uint32_t consumedBytes;
	void    *pcmBuf;
	uint32_t pcmOutBytes;
	uint32_t extraWords[16];
} MeCodecPacket;

typedef struct MeAtxCodecPacket {
	uint32_t      headerWords[2];
	uint32_t      errorCode;
	MeAtxWorkCtx *workCtx;
	uint32_t      reservedWords[2];
	const uint8_t *esBuf;
	uint32_t      consumedBytes;
	int16_t      *pcmBuf;
	uint32_t      pcmOutBytes;
	uint8_t       headerByteA;
	uint8_t       headerByteB;
	uint8_t       padBytes[2];
	uint32_t      outputChannels;
	uint32_t      hasHeader;
	uint32_t      sampleRate;
	uint32_t      extraParam;
	uint32_t      channelMode;
	uint32_t      frameBytes;
} MeAtxCodecPacket;

typedef struct MeMp3CodecPacket {
	uint32_t      headerWords[2];
	uint32_t      errorCode;
	MeMp3WorkCtx *workCtx;
	uint32_t      reservedWords[2];
	const uint8_t *esBuf;
	uint32_t      consumedBytes;
	int16_t      *pcmBuf;
	uint32_t      pcmOutBytes;
	uint32_t      esInputSize;
	uint32_t      syncBitOffset;
	uint32_t      reservedExtra[2];
	MeMp3OutInfo  outInfo;
} MeMp3CodecPacket;

static const MeAt3ModeEntry g_at3ModeTable[16] = {
	{ 0, 256 }, { 1, 256 }, { 0, 212 }, { 1, 212 },
	{ 0, 192 }, { 1, 192 }, { 0, 152 }, { 1, 152 },
	{ 0, 136 }, { 1, 136 }, { 0,  96 }, { 1,  96 },
	{ 1,  68 }, { 1,  48 }, { 2, 192 }, { 2, 152 }
};

static const int g_atxSampleRateTable[8] = {
	32000, 44100, 48000, 88200, 96000, 96000, 96000, 96000
};

static const int g_mp3ChannelTable[4] = { 2, 2, 2, 1 };

static const int g_mp3SampleRateTable[4][4] = {
	{ 11025, 12000,  8000, 0 },
	{     0,     0,     0, 0 },
	{ 22050, 24000, 16000, 0 },
	{ 44100, 48000, 32000, 0 }
};

static const int g_mp3BitrateTable[4][16] = {
	{ 0,  8, 16, 24, 32, 40, 48, 56, 64,  80,  96, 112, 128, 144, 160, 0 },
	{ 0,  0,  0,  0,  0,  0,  0,  0,  0,   0,   0,   0,   0,   0,   0, 0 },
	{ 0,  8, 16, 24, 32, 40, 48, 56, 64,  80,  96, 112, 128, 144, 160, 0 },
	{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0 }
};

static const uint8_t g_mp3LayerModeTable[4] = { 2, 0, 0, 1 };
static const int g_g729BitrateTable[3] = { 8, 10, 15 };

/* Initialized global (0x81067170) */
int g_mp3BackendMode = ME_ADEC_BACKEND_AUDIOCODEC;

/* BSS state */
uint8_t        g_adecLruOrder[ME_ADEC_MAX_SLOTS];
MeAdecSlot     g_adecSlots[ME_ADEC_MAX_SLOTS];
int            g_adecKeepSlotsOnDelete;
uint32_t       g_adecSlotWorkSize;
uint8_t       *g_adecWorkBufBase;
SceUID         g_adecWorkMemId;
int16_t        g_at3MonoTempBuf[2048];
int16_t        g_atxMonoTempBuf[4096];
int16_t        g_mp3MonoTempBuf[1152];
int            g_at3SilenceOnError;
int            g_atxCorruptFilterEnabled;
SceUID         g_atxAsyncEventFlag;
SceUID         g_atxAsyncSema;
int            g_atxCorruptMuteActive;
int            g_atxCorruptHeaderSeen;
MeAtxAsyncSlot g_atxAsyncSlots[3];
int            g_atxAsyncEnabled;
MeAtxWorkCtx  *g_atxAsyncActiveCtx;
int16_t        g_atxAsyncWarmupBuf[4096];
int            g_mp3CompatBitEnabled;
uint32_t       g_sasBufferBytes;

static int __attribute__((noinline, noclone)) meAt3SetCompatFlags(int flags);
static int __attribute__((noinline, noclone)) meAtxSetCompatFlags(int flags);
static int __attribute__((noinline, noclone)) meMp3SetCompatFlags(int flags);

int meWrapperApiInit(void)
{
	SceAudiodecInitParam initParam;
	uint32_t workProbe[24];

	memset(g_adecSlots, 0, sizeof(g_adecSlots));
	g_adecSlotWorkSize = 0U;
	g_adecWorkBufBase = NULL;
	g_adecWorkMemId = 0;

	for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++)
		g_adecLruOrder[i] = (uint8_t)i;

	int compatFlags = pspemuCompatGetParam(12);
	if (compatFlags <= 0)
		compatFlags = 0;

	uint32_t maxWork = 0x10000U;
	workProbe[2] = 0x10000U;

	if ((compatFlags & 0x20) != 0) {
		initParam.mp3.size = sizeof(SceAudiodecInitStreamParam);
		initParam.mp3.totalStreams = 4U;
		sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_MP3, &initParam);
	} else {
		if (SceAudiocodec_E2AF3A54(workProbe, ME_ADEC_CODEC_MP3) < 0)
			return -1;
		if (workProbe[2] > maxWork)
			maxWork = workProbe[2];
	}

	if ((compatFlags & 0x40) != 0) {
		initParam.aac.size = sizeof(SceAudiodecInitStreamParam);
		initParam.aac.totalStreams = 4U;
		if (sceAudiodecInitLibrary(SCE_AUDIODEC_TYPE_AAC, &initParam) < 0)
			return -1;
	} else {
		if (SceAudiocodec_E2AF3A54(workProbe, ME_ADEC_CODEC_AAC) < 0)
			return -1;
		if (workProbe[2] > maxWork)
			maxWork = workProbe[2];
	}

	uint32_t g729EncSize = (uint32_t)sceG729EncodeGetContextSize();
	if (g729EncSize > maxWork)
		maxWork = g729EncSize;
	uint32_t g729DecSize = (uint32_t)sceG729DecodeGetContextSize();
	if (g729DecSize > maxWork)
		maxWork = g729DecSize;

	uint32_t alignedWork = (maxWork + 0xFFU) & ~0xFFU;
	if (alignedWork != 0U) {
		g_adecSlotWorkSize = alignedWork;
		uint32_t totalMem = (ME_ADEC_MAX_SLOTS * alignedWork + 0xFF000U) & ~0xFFFFFU;
		SceUID memId = sceKernelAllocMemBlock("ScePspemuAdecWork", PSPEMU_MEMBLOCK_PHYCONT_RW, totalMem, NULL);
		g_adecWorkMemId = memId;
		if (memId < 0)
			return -1;
		if (sceKernelGetMemBlockBase(memId, (void **)&g_adecWorkBufBase) != 0)
			return -2;
	}

	int ret = meAt3SetCompatFlags(compatFlags);
	if (ret != 0)
		return ret;
	ret = meAtxSetCompatFlags(compatFlags);
	if (ret != 0)
		return ret;
	return meMp3SetCompatFlags(compatFlags);
}

static int __attribute__((noinline, noclone)) meAdecDeleteSlotByKey(void *keyPtr)
{
	for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++) {
		MeAdecSlot *slot = &g_adecSlots[g_adecLruOrder[i]];
		if (slot->keyPtr == keyPtr) {
			if (slot->backendType == ME_ADEC_BACKEND_AUDIODEC)
				sceAudiodecDeleteDecoder(&slot->dec.audiodec.ctrl);
			if (g_adecKeepSlotsOnDelete == 0) {
				slot->keyPtr = NULL;
				slot->keySize = 0U;
				slot->backendType = ME_ADEC_BACKEND_NONE;
			}
			return i;
		}
	}
	return 0;
}

static MeAdecSlot *__attribute__((noinline, noclone)) meAdecFindSlot(void *keyPtr, size_t keySize, uint32_t backendType)
{
	for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++) {
		MeAdecSlot *slot = &g_adecSlots[g_adecLruOrder[i]];
		if (slot->keyPtr == keyPtr &&
		    slot->keySize == keySize &&
		    slot->backendType == backendType &&
		    (keySize == 0U || memcmp(slot->keySnapshot.bytes, keyPtr, keySize) == 0)) {
			return slot;
		}
	}
	return NULL;
}

static MeAdecSlot *__attribute__((noinline, noclone)) meAdecAcquireSlot(
	void *keyPtr, size_t keySize, uint32_t backendType, int *outCreated)
{
	MeAdecSlot *slot = meAdecFindSlot(keyPtr, keySize, backendType);
	*outCreated = 0;

	if (slot != NULL) {
		for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++) {
			uint8_t idx = g_adecLruOrder[i];
			if (&g_adecSlots[idx] == slot) {
				for (int k = i; k > 0; k--)
					g_adecLruOrder[k] = g_adecLruOrder[k - 1];
				g_adecLruOrder[0] = idx;
				break;
			}
		}
		return slot;
	}

	slot = meAdecFindSlot(keyPtr, keySize, ME_ADEC_BACKEND_NONE);
	if (slot != NULL) {
		int activeCount = 0;
		for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++) {
			MeAdecSlot *cand = &g_adecSlots[g_adecLruOrder[i]];
			if (cand->backendType != ME_ADEC_BACKEND_NONE && cand->keySize == keySize) {
				activeCount += 2;
				if (activeCount > 5) {
					if (cand->backendType == ME_ADEC_BACKEND_AUDIODEC)
						sceAudiodecDeleteDecoder(&cand->dec.audiodec.ctrl);
					cand->backendType = ME_ADEC_BACKEND_NONE;
				}
			}
		}
		for (int i = 0; i < ME_ADEC_MAX_SLOTS; i++) {
			uint8_t idx = g_adecLruOrder[i];
			if (&g_adecSlots[idx] == slot) {
				for (int k = i; k > 0; k--)
					g_adecLruOrder[k] = g_adecLruOrder[k - 1];
				g_adecLruOrder[0] = idx;
				break;
			}
		}
	} else {
		uint8_t victimIdx = g_adecLruOrder[ME_ADEC_MAX_SLOTS - 1];
		slot = &g_adecSlots[victimIdx];
		if (slot->backendType != ME_ADEC_BACKEND_NONE) {
			if (slot->backendType == ME_ADEC_BACKEND_AUDIODEC)
				sceAudiodecDeleteDecoder(&slot->dec.audiodec.ctrl);
			slot->backendType = ME_ADEC_BACKEND_NONE;
		}
		for (int k = ME_ADEC_MAX_SLOTS - 1; k > 0; k--)
			g_adecLruOrder[k] = g_adecLruOrder[k - 1];
		g_adecLruOrder[0] = victimIdx;
		slot->keyPtr = keyPtr;
		slot->keySize = keySize;
		if (keySize != 0U)
			memcpy(slot->keySnapshot.bytes, keyPtr, keySize);
	}

	*outCreated = -1;
	if (backendType == ME_ADEC_BACKEND_AUDIODEC) {
		slot->dec.audiodec.ctrl.size = sizeof(SceAudiodecCtrl);
		slot->dec.audiodec.ctrl.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;
		slot->dec.audiodec.ctrl.pInfo = &slot->dec.audiodec.info;
	}
	return slot;
}

static int __attribute__((noinline, noclone)) meAdecAssignWorkBuf(MeAdecSlot *slot)
{
	if (g_adecSlotWorkSize < slot->dec.audiocodec.workSize)
		return -1;
	int index = (int)(slot - g_adecSlots);
	slot->dec.audiocodec.workBuf = g_adecWorkBufBase + (size_t)index * g_adecSlotWorkSize;
	return 0;
}

static int __attribute__((noinline, noclone)) meAt3FindModeIndex(int channelMode, int frameBytes)
{
	for (int i = 0; i < 16; i++) {
		if (g_at3ModeTable[i].channelMode == channelMode &&
		    g_at3ModeTable[i].frameBytes == frameBytes) {
			return i;
		}
	}
	return -1;
}

int __attribute__((noinline, noclone)) meAt3DecodeInternal(
	const void *esBuf, int16_t *pcmOut, uint32_t *outConsumed, uint32_t *outSamples,
	void *workCtxPtr, int forceStereoDirect)
{
	MeAt3WorkCtx *workCtx = (MeAt3WorkCtx *)workCtxPtr;
	int created = 0;
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, sizeof(*workCtx), ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	if (created != 0) {
		int modeIdx = meAt3FindModeIndex((int)workCtx->channelMode, (int)workCtx->frameBytes);
		if (modeIdx < 0)
			return -1;
		slot->dec.audiocodec.configWords[0] = (uint32_t)modeIdx;
		if (SceAudiocodec_E2AF3A54(slot, ME_ADEC_CODEC_AT3) < 0 ||
		    meAdecAssignWorkBuf(slot) < 0 ||
		    SceAudiocodec_1F9A3722(slot, ME_ADEC_CODEC_AT3) < 0) {
			return -1;
		}
		slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
	}
	slot->internalError = 0U;

	int expandMono = (slot->dec.audiocodec.configWords[3] == 1U &&
	                  workCtx->outputMode != 1U &&
	                  forceStereoDirect == 0);
	slot->dec.audiocodec.esBuf = esBuf;
	slot->dec.audiocodec.pcmBuf = expandMono ? g_at3MonoTempBuf : pcmOut;

	int decRet = SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3);
	if (g_at3SilenceOnError != 0 && decRet == ME_AT3_ERROR_BAD_FRAME)
		memset(slot->dec.audiocodec.pcmBuf, 0, (size_t)(2 - expandMono) << 10);
	if (created != 0)
		memset(slot->dec.audiocodec.pcmBuf, 0, (size_t)(2 - expandMono) << 10);

	*outConsumed = slot->dec.audiocodec.consumedBytes;
	uint32_t halfWords = slot->dec.audiocodec.pcmOutBytes >> 1;
	if (halfWords > 2048U)
		halfWords = 2048U;
	*outSamples = halfWords;

	if (expandMono) {
		for (uint32_t i = 0; i < halfWords; i++) {
			int16_t sample = g_at3MonoTempBuf[i];
			pcmOut[2U * i] = sample;
			pcmOut[2U * i + 1U] = sample;
		}
		*outSamples = halfWords * 2U;
	}
	return 0;
}

int meAt3DecodeFrame(void *codecBuf)
{
	uint8_t fallbackBuf[4160];
	MeCodecPacket *pkt = (MeCodecPacket *)codecBuf;
	int16_t *pcmDst = (int16_t *)pkt->pcmBuf;

	if (pcmDst == NULL)
		pcmDst = (int16_t *)(((uintptr_t)fallbackBuf + 63U) & ~63U);

	int ret = meAt3DecodeInternal(pkt->esBuf, pcmDst, &pkt->consumedBytes, &pkt->pcmOutBytes,
	                              (MeAt3WorkCtx *)pkt->workCtx, 0);
	pkt->pcmOutBytes *= 2U;
	return ret;
}

int meAt3GetInternalError(uint32_t *outErr, void *workCtx)
{
	MeAdecSlot *slot = meAdecFindSlot(workCtx, sizeof(MeAt3WorkCtx), ME_ADEC_BACKEND_AUDIOCODEC);
	if (slot != NULL)
		*outErr = slot->internalError;
	return 0;
}

int meAt3InitContext(uint32_t channelMode, uint32_t sampleRate, uint32_t frameBytes, void *workCtx, uint32_t outputMode)
{
	MeAt3WorkCtx *ctx = (MeAt3WorkCtx *)workCtx;
	meAdecDeleteSlotByKey(ctx);
	memset(ctx, 0, sizeof(*ctx));
	ctx->channelMode = channelMode;
	ctx->sampleRate = sampleRate;
	ctx->frameBytes = frameBytes;
	ctx->outputMode = outputMode;
	return 0;
}

static int __attribute__((noinline, noclone)) meAt3SetCompatFlags(int flags)
{
	if ((flags & 0x200) != 0)
		g_at3SilenceOnError = 1;
	return 0;
}

static int meAtxAsyncDecodeThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	do {
		if (sceKernelWaitSema(g_atxAsyncSema, 1, NULL) != 0) {
			sceKernelExitDeleteThread(0);
			return 0;
		}

		MeAtxAsyncSlot *asyncSlot = NULL;
		if (g_atxAsyncSlots[0].workCtx == g_atxAsyncActiveCtx)
			asyncSlot = &g_atxAsyncSlots[0];
		else if (g_atxAsyncSlots[1].workCtx == g_atxAsyncActiveCtx)
			asyncSlot = &g_atxAsyncSlots[1];
		else if (g_atxAsyncSlots[2].workCtx == g_atxAsyncActiveCtx)
			asyncSlot = &g_atxAsyncSlots[2];

		if (asyncSlot != NULL) {
			int created;
			MeAdecSlot *slot = meAdecAcquireSlot(asyncSlot->workCtx, sizeof(MeAtxWorkCtx), ME_ADEC_BACKEND_AUDIOCODEC, &created);
			if (slot != NULL) {
				slot->dec.audiocodec.esBuf = asyncSlot->esBuf;
				slot->dec.audiocodec.pcmBuf = asyncSlot->pcmBuf;
				SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3PLUS);
				asyncSlot->consumedBytes = slot->dec.audiocodec.consumedBytes;
				asyncSlot->pcmOutBytes = slot->dec.audiocodec.pcmOutBytes;
			}
		}
	} while (sceKernelSetEventFlag(g_atxAsyncEventFlag, 1U) >= 0);

	return -1;
}

int meAtxDecodeFrame(void *codecBuf)
{
	uint8_t fallbackBuf[8256];
	MeAtxCodecPacket *pkt = (MeAtxCodecPacket *)codecBuf;
	const uint8_t *esBuf = pkt->esBuf;
	int16_t *pcmDst = pkt->pcmBuf;

	if (pkt->hasHeader != 0U) {
		pkt->workCtx->jointStereoFlag = 1U;
		uint32_t bA = pkt->headerByteA;
		uint32_t bB = pkt->headerByteB;
		pkt->channelMode = (bA >> 2) & 7U;
		pkt->sampleRate = (uint32_t)g_atxSampleRateTable[bA >> 5];
		pkt->frameBytes = 8U * (bB + 1U + ((bA & 3U) << 8));
		pkt->workCtx->outputChannels = pkt->outputChannels;
	}

	if (pcmDst == NULL)
		pcmDst = (int16_t *)(((uintptr_t)fallbackBuf + 63U) & ~63U);

	MeAtxWorkCtx *workCtx = pkt->workCtx;
	int created = 0;
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, sizeof(*workCtx), ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL) {
		pkt->consumedBytes = 0U;
		pkt->pcmOutBytes = 0U;
		return -1;
	}

	int needsWarmup = 0;
	if (created != 0) {
		uint16_t rateBits;
		switch (workCtx->sampleRate) {
		case 32000U: rateBits = 0x0000U; break;
		case 44100U: rateBits = 0x2000U; break;
		case 48000U: rateBits = 0x4000U; break;
		case 88200U: rateBits = 0x6000U; break;
		default:     rateBits = 0x8000U; break;
		}
		uint16_t cfg = (uint16_t)(rateBits | ((workCtx->frameBytes >> 3) - 1U) | ((uint16_t)workCtx->channelMode << 10));
		uint8_t *cfgBytes = (uint8_t *)slot->dec.audiocodec.configWords;
		cfgBytes[0] = (uint8_t)(cfg >> 8);
		cfgBytes[1] = (uint8_t)(cfg & 0xFFU);

		if (SceAudiocodec_E2AF3A54(slot, ME_ADEC_CODEC_AT3PLUS) < 0 ||
		    meAdecAssignWorkBuf(slot) < 0) {
			pkt->consumedBytes = 0U;
			pkt->pcmOutBytes = 0U;
			return -1;
		}
		slot->dec.audiocodec.reservedA = 1U;
		slot->dec.audiocodec.configWords[1] = workCtx->outputChannels;
		if (SceAudiocodec_1F9A3722(slot, ME_ADEC_CODEC_AT3PLUS) < 0) {
			pkt->consumedBytes = 0U;
			pkt->pcmOutBytes = 0U;
			return -1;
		}
		slot->dec.audiocodec.configWords[4] = pkt->extraParam;
		workCtx->extraParam = pkt->extraParam;
		slot->dec.audiocodec.configWords[2] = workCtx->jointStereoFlag;
		slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
		needsWarmup = created;
	}

	slot->internalError = 0U;
	int expandMono = (slot->dec.audiocodec.configWords[8] == 1U && workCtx->outputMode != 1U);

	if (g_atxAsyncEnabled != 0 &&
	    sceKernelWaitEventFlag(g_atxAsyncEventFlag, 1U, 5U, NULL, NULL) >= 0) {
		MeAtxAsyncSlot *readySlot = NULL;
		for (int i = 0; i < 3; i++) {
			if (g_atxAsyncSlots[i].workCtx == workCtx) {
				readySlot = &g_atxAsyncSlots[i];
				break;
			}
		}
		if (readySlot != NULL) {
			if (expandMono) {
				int monoSamples = (int)(readySlot->pcmOutBytes / 2U);
				for (int i = 0; i < monoSamples; i++) {
					int16_t sample = readySlot->pcmBuf[i];
					pcmDst[2 * i] = sample;
					pcmDst[2 * i + 1] = sample;
				}
				readySlot->pcmOutBytes = (uint32_t)(4 * monoSamples);
			} else {
				memcpy(pcmDst, readySlot->pcmBuf, 0x2000U);
			}
			memset(readySlot, 0, sizeof(*readySlot));
		}

		MeAtxAsyncSlot *freeSlot = NULL;
		for (int i = 0; i < 3; i++) {
			if (g_atxAsyncSlots[i].workCtx == NULL) {
				freeSlot = &g_atxAsyncSlots[i];
				break;
			}
		}
		if (freeSlot != NULL) {
			freeSlot->workCtx = workCtx;
			g_atxAsyncActiveCtx = workCtx;
			freeSlot->esBuf = esBuf;
		}
		if (needsWarmup != 0 && freeSlot != NULL) {
			slot->dec.audiocodec.esBuf = freeSlot->esBuf;
			slot->dec.audiocodec.pcmBuf = g_atxAsyncWarmupBuf;
			SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3PLUS);
			if (expandMono)
				SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3PLUS);
		}
		if (sceKernelSignalSema(g_atxAsyncSema, 1) == 0)
			return 0;
		needsWarmup = created;
	}

	if (g_atxCorruptFilterEnabled != 0 && pcmDst != NULL && esBuf != NULL) {
		if (needsWarmup != 0)
			g_atxCorruptHeaderSeen = 0;
		if (esBuf[0] == 0x3AU && esBuf[1] == 0x64U && esBuf[2] == 0xEBU && esBuf[3] == 0x96U && esBuf[4] == 0x2BU) {
			g_atxCorruptMuteActive = 0;
			g_atxCorruptHeaderSeen = 1;
		} else if (g_atxCorruptHeaderSeen != 0) {
			if (g_atxCorruptMuteActive != 0) {
				memset(pcmDst, 0, 0x2000U);
				return 0;
			}
			int suspectCount = 0;
			int triggerMute = 0;
			for (int i = 0; i < 372; i++) {
				uint8_t cur = esBuf[i];
				if (cur == 0xFFU) {
					if (esBuf[i + 1] == 0xFFU)
						suspectCount++;
					if (esBuf[i + 4] == 0xFFU && esBuf[i + 8] == 0xFFU &&
					    esBuf[i + 12] == 0xFFU && esBuf[i + 16] == 0xFFU) {
						triggerMute = 1;
						break;
					}
				} else if (cur == 0U && esBuf[i + 1] == 0U && esBuf[i + 2] == 0U) {
					if (esBuf[i + 3] == 0U) {
						if (esBuf[i + 4] == 0U)
							suspectCount++;
					} else if (esBuf[i + 3] == 0xD4U && esBuf[i + 4] == 0U &&
					           esBuf[i + 5] == 0U && esBuf[i + 6] == 0U && esBuf[i + 7] == 0xD4U) {
						triggerMute = 1;
						break;
					}
				}
			}
			if (triggerMute || suspectCount > 3) {
				g_atxCorruptMuteActive = 1;
				memset(pcmDst, 0, 0x2000U);
				return 0;
			}
		}
	}

	slot->dec.audiocodec.pcmBuf = expandMono ? g_atxMonoTempBuf : pcmDst;
	slot->dec.audiocodec.esBuf = esBuf;
	if (needsWarmup != 0)
		SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3PLUS);
	SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AT3PLUS);

	pkt->consumedBytes = slot->dec.audiocodec.consumedBytes;
	pkt->pcmOutBytes = slot->dec.audiocodec.pcmOutBytes;

	if (expandMono) {
		int monoSamples = (int)(pkt->pcmOutBytes / 2U);
		for (int i = 0; i < monoSamples; i++) {
			int16_t sample = g_atxMonoTempBuf[i];
			pcmDst[2 * i] = sample;
			pcmDst[2 * i + 1] = sample;
		}
		pkt->pcmOutBytes = (uint32_t)(4 * monoSamples);
	}
	return 0;
}

int meAtxParseHeader(uint32_t *outSampleRate, uint32_t *outChannelMode, uint32_t *outFrameBytes, const uint8_t *headerBytes)
{
	uint32_t bA = headerBytes[0];
	uint32_t blocks = headerBytes[1] + 1U + ((bA & 3U) << 8);
	*outSampleRate = (uint32_t)g_atxSampleRateTable[bA >> 5];
	*outChannelMode = (bA >> 2) & 7U;
	*outFrameBytes = 8U * blocks;
	return 0;
}

int meAtxGetOutputSize(uint32_t *outSize, const void *workCtx)
{
	const MeAtxWorkCtx *ctx = (const MeAtxWorkCtx *)workCtx;
	*outSize = ctx->outputSamples;
	return 0;
}

int meAtxInitContext(uint32_t sampleRate, uint32_t channelMode, uint32_t frameBytes, uint32_t outputMode, void *workCtx)
{
	MeAtxWorkCtx *ctx = (MeAtxWorkCtx *)workCtx;
	meAdecDeleteSlotByKey(ctx);
	memset(ctx, 0, sizeof(*ctx));
	ctx->sampleRate = sampleRate;
	ctx->channelMode = channelMode;
	ctx->frameBytes = frameBytes;
	ctx->outputSamples = (sampleRate * frameBytes) / 256000U;
	ctx->outputMode = outputMode;

	if (g_atxAsyncEnabled != 0) {
		for (int i = 0; i < 3; i++) {
			if (g_atxAsyncSlots[i].workCtx == ctx)
				memset(&g_atxAsyncSlots[i], 0, sizeof(g_atxAsyncSlots[i]));
		}
	}
	return 0;
}

int meAtxSetChannels(const uint32_t *inChannels, void *workCtx)
{
	MeAtxWorkCtx *ctx = (MeAtxWorkCtx *)workCtx;
	ctx->outputChannels = *inChannels;
	return 0;
}

int meAtxGetInternalError(uint32_t *outErr, void *workCtx)
{
	MeAdecSlot *slot = meAdecFindSlot(workCtx, sizeof(MeAtxWorkCtx), ME_ADEC_BACKEND_AUDIOCODEC);
	if (slot != NULL)
		*outErr = slot->internalError;
	return 0;
}

static int __attribute__((noinline, noclone)) meAtxSetCompatFlags(int flags)
{
	memset(g_atxMonoTempBuf, 0, sizeof(g_atxMonoTempBuf));
	memset(g_atxAsyncSlots, 0, sizeof(g_atxAsyncSlots));

	if ((flags & 0x80) != 0) {
		g_audioFadeDir = -1;
		g_audioFadeShift = 0;
	}
	if ((flags & 0x100) != 0)
		g_atxCorruptFilterEnabled = 1;
	if ((flags & 0x400) != 0)
		g_adecKeepSlotsOnDelete = 1;

	if ((flags & 2) == 0)
		return 0;

	g_atxAsyncEnabled = 1;
	g_atxAsyncSema = sceKernelCreateSema("ScePspemuAtxdeocde", 0, 0, 1, NULL);
	if (g_atxAsyncSema <= 0)
		return g_atxAsyncSema;
	g_atxAsyncEventFlag = sceKernelCreateEventFlag("ScePspemuAtx", 0x1000, 1U, NULL);
	if (g_atxAsyncEventFlag <= 0)
		return g_atxAsyncEventFlag;

	SceUID thid = sceKernelCreateThread(
		"ScePspemuRemoteMeAtracDecode",
		meAtxAsyncDecodeThread,
		97,
		0x40000U,
		0,
		0x40000,
		NULL);
	if (thid >= 0)
		return sceKernelStartThread(thid, 0, NULL);
	return thid;
}

static int __attribute__((noinline, noclone)) meMp3ParseFrameHeader(MeMp3HeaderInfo *outHdr, const uint8_t *frameBytes)
{
	memset(outHdr, 0, sizeof(*outHdr));
	int syncWord = ((int)frameBytes[0] << 4) | ((frameBytes[1] & 0xE0) >> 4);
	int version = (frameBytes[1] >> 3) & 3;
	int layer = (frameBytes[1] >> 1) & 3;
	int brIdx = frameBytes[2] >> 4;
	int srIdx = (frameBytes[2] >> 2) & 3;
	int chMode = frameBytes[3] >> 6;

	outHdr->syncWord = syncWord;
	outHdr->mpegVersion = version;
	outHdr->layer = layer;
	outHdr->protectionBit = frameBytes[1] & 1;
	outHdr->bitrateIndex = brIdx;
	outHdr->sampleRateIndex = srIdx;
	outHdr->paddingBit = (frameBytes[2] & 2) != 0;
	outHdr->privateBit = frameBytes[2] & 1;
	outHdr->channelMode = chMode;
	outHdr->modeExtension = (frameBytes[3] >> 4) & 3;
	outHdr->copyright = (frameBytes[3] & 8) != 0;
	outHdr->original = (frameBytes[3] & 4) != 0;
	outHdr->emphasis = frameBytes[3] & 3;
	outHdr->bitrateKbps = g_mp3BitrateTable[version][brIdx];
	outHdr->sampleRateHz = g_mp3SampleRateTable[version][srIdx];
	outHdr->channels = g_mp3ChannelTable[chMode];

	if (syncWord != 0xFFE)
		return -2;
	if (srIdx == 3)
		return -4;
	return 0;
}

int meMp3FindSyncAndParse(const uint8_t *esBuf, uint32_t *outBitOffset, void *outInfoPtr, void *workCtxPtr)
{
	MeMp3WorkCtx *ctx = (MeMp3WorkCtx *)workCtxPtr;
	MeMp3OutInfo *outInfo = (MeMp3OutInfo *)outInfoPtr;

	if (ctx == NULL || esBuf == NULL)
		return -1;

	if (ctx->needSyncSearch != 0) {
		if (memcmp(esBuf, "RIFF", 4U) == 0)
			return -1;

		if (memcmp(esBuf, "ID3", 3U) == 0) {
			uint32_t tagSize = (((uint32_t)(esBuf[6] & 0x7FU)) << 21) |
			                   (((uint32_t)(esBuf[7] & 0x7FU)) << 14) |
			                   (((uint32_t)(esBuf[8] & 0x7FU)) << 7) |
			                   ((uint32_t)(esBuf[9] & 0x7FU));
			if (tagSize >= 1440U)
				return -1;
			int found = -1;
			for (int pos = (int)tagSize; pos < 1440; pos++) {
				if (esBuf[pos] == 0xFFU && (esBuf[pos + 1] & 0xE0U) == 0xE0U) {
					found = pos;
					break;
				}
			}
			if (found <= 0)
				return -1;
			ctx->syncOffset = found;
		} else {
			for (int pos = 0; pos < ctx->esSize - 1; pos++) {
				if (esBuf[pos] == 0xFFU && (esBuf[pos + 1] & 0xE0U) == 0xE0U) {
					ctx->syncOffset = pos;
					break;
				}
			}
			ctx->needSyncSearch = 0;
		}
	}

	*outBitOffset = (uint32_t)(8 * ctx->syncOffset);
	int ret = meMp3ParseFrameHeader(&ctx->header, &esBuf[ctx->syncOffset]);
	if (ret != 0)
		return -1;

	outInfo->versionFlag = ctx->header.mpegVersion & 1;
	outInfo->channelCount = (ctx->header.layer != 0) ? 3 : 0;
	outInfo->bitrateIndex = ctx->header.bitrateIndex;
	outInfo->sampleRateIndex = ctx->header.sampleRateIndex;
	outInfo->paddingBit = ctx->header.paddingBit;
	outInfo->privateBit = ctx->header.privateBit;
	outInfo->channelMode = ctx->header.channelMode;
	outInfo->modeExtension = ctx->header.modeExtension;
	outInfo->copyright = ctx->header.copyright;
	outInfo->original = ctx->header.original;
	outInfo->emphasis = ctx->header.emphasis;

	ctx->mpegVersion = ctx->header.mpegVersion;
	ctx->channels = ctx->header.channels;
	return 0;
}

int meMp3DecodeFrame(void *codecBuf)
{
	MeMp3CodecPacket *pkt = (MeMp3CodecPacket *)codecBuf;
	MeMp3WorkCtx *ctx = pkt->workCtx;

	ctx->esSize = (int)pkt->esInputSize;
	int parseRet = meMp3FindSyncAndParse(pkt->esBuf, &pkt->syncBitOffset, &pkt->outInfo, ctx);
	uint32_t byteOff = pkt->syncBitOffset >> 3;
	if (parseRet != 0) {
		pkt->consumedBytes = (parseRet == -2) ? pkt->esInputSize : (byteOff + 1U);
		return parseRet;
	}

	const uint8_t *frameSrc = pkt->esBuf + byteOff;
	int16_t *pcmDst = pkt->pcmBuf;
	int created = 0;
	MeAdecSlot *slot = meAdecAcquireSlot(ctx, 8U, (uint32_t)g_mp3BackendMode, &created);
	if (slot == NULL)
		return -1;

	if (created != 0) {
		if (g_mp3BackendMode == ME_ADEC_BACKEND_AUDIODEC) {
			slot->dec.audiodec.info.mp3.size = sizeof(SceAudiodecInfoMp3);
			slot->dec.audiodec.info.mp3.ch = (SceUInt32)ctx->channels;
			slot->dec.audiodec.info.mp3.version = (SceUInt32)ctx->mpegVersion;
			slot->dec.audiodec.ctrl.size = sizeof(SceAudiodecCtrl);
			slot->dec.audiodec.ctrl.wordLength = SCE_AUDIODEC_WORD_LENGTH_16BITS;
			slot->dec.audiodec.ctrl.pInfo = &slot->dec.audiodec.info;
			if (sceAudiodecCreateDecoder(&slot->dec.audiodec.ctrl, SCE_AUDIODEC_TYPE_MP3) < 0)
				return -1;
		} else if (g_mp3BackendMode == ME_ADEC_BACKEND_AUDIOCODEC) {
			if (SceAudiocodec_E2AF3A54(slot, ME_ADEC_CODEC_MP3) < 0 ||
			    meAdecAssignWorkBuf(slot) < 0) {
				return -1;
			}
			slot->dec.audiocodec.configWords[1] = g_mp3LayerModeTable[ctx->mpegVersion];
			slot->dec.audiocodec.configWords[0] = (uint32_t)ctx->esSize;
			if (SceAudiocodec_1F9A3722(slot, ME_ADEC_CODEC_MP3) < 0)
				return -1;
		}
		slot->backendType = (uint32_t)g_mp3BackendMode;
	}

	slot->internalError = 0U;
	if (g_mp3BackendMode == ME_ADEC_BACKEND_AUDIODEC) {
		slot->dec.audiodec.ctrl.pEs = (SceUInt8 *)frameSrc;
		slot->dec.audiodec.ctrl.pPcm = pcmDst;
		if (sceAudiodecDecode(&slot->dec.audiodec.ctrl) < 0) {
			sceAudiodecGetInternalError(&slot->dec.audiodec.ctrl, (SceInt32 *)&slot->internalError);
			return -1;
		}
		pkt->consumedBytes = slot->dec.audiodec.ctrl.inputEsSize;
		pkt->pcmOutBytes = slot->dec.audiodec.ctrl.outputPcmSize;
		return 0;
	}

	if (g_mp3BackendMode == ME_ADEC_BACKEND_AUDIOCODEC) {
		int isMono = (ctx->channels == 1);
		slot->dec.audiocodec.esBuf = frameSrc;
		slot->dec.audiocodec.pcmBuf = isMono ? g_mp3MonoTempBuf : pcmDst;
		if (created != 0)
			SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_MP3);
		if (SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_MP3) < 0) {
			slot->internalError = slot->dec.audiocodec.errorCode;
			return -1;
		}

		uint32_t outBytes = slot->dec.audiocodec.pcmOutBytes;
		if (isMono) {
			if (outBytes > sizeof(g_mp3MonoTempBuf))
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			uint32_t samples = outBytes >> 1;
			for (uint32_t i = 0; i < samples; i++) {
				int16_t sample = g_mp3MonoTempBuf[i];
				pcmDst[2U * i] = sample;
				pcmDst[2U * i + 1U] = sample;
			}
			outBytes *= 2U;
		}
		pkt->consumedBytes = slot->dec.audiocodec.consumedBytes;
		pkt->pcmOutBytes = outBytes;
	}
	return 0;
}

int meMp3SetFrameSize(uint32_t esSize, void *workCtx)
{
	MeMp3WorkCtx *ctx = (MeMp3WorkCtx *)workCtx;
	ctx->esSize = (int)esSize;
	return 0;
}

int meMp3ResetContext(void *workCtx)
{
	MeMp3WorkCtx *ctx = (MeMp3WorkCtx *)workCtx;
	meAdecDeleteSlotByKey(ctx);
	ctx->needSyncSearch = 1;
	ctx->syncOffset = 0;
	return 0;
}

static int __attribute__((noinline, noclone)) meMp3SetCompatFlags(int flags)
{
	memset(g_mp3MonoTempBuf, 0, sizeof(g_mp3MonoTempBuf));
	if ((flags & 0x20) != 0)
		g_mp3BackendMode = ME_ADEC_BACKEND_AUDIODEC;
	if ((flags & 4) != 0)
		g_mp3CompatBitEnabled = 1;
	return 0;
}

int meAacInitContext(uint32_t sampleRate, void *workCtx)
{
	MeAacWorkCtx *ctx = (MeAacWorkCtx *)workCtx;
	meAdecDeleteSlotByKey(ctx);
	ctx->rateCode = 0U;
	ctx->optionFlag = 0U;
	ctx->padBytes[0] = 0U;
	ctx->padBytes[1] = 0U;
	ctx->sampleRate = sampleRate;
	return 0;
}

int meAacDecodeFrame(const void *esBuf, uint32_t *outConsumed, void *pcmBuf, uint32_t *outPcmBytes, void *workCtx)
{
	MeAacWorkCtx *ctx = (MeAacWorkCtx *)workCtx;
	int created = 0;
	MeAdecSlot *slot = meAdecAcquireSlot(ctx, sizeof(*ctx), ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	if (created != 0) {
		if (SceAudiocodec_E2AF3A54(slot, ME_ADEC_CODEC_AAC) < 0 ||
		    meAdecAssignWorkBuf(slot) < 0) {
			return -1;
		}
		slot->dec.audiocodec.configWords[2] = ctx->sampleRate;
		((uint8_t *)&slot->dec.audiocodec.configWords[3])[0] = 0U;
		((uint8_t *)&slot->dec.audiocodec.configWords[0])[0] = ctx->rateCode;
		if (SceAudiocodec_1F9A3722(slot, ME_ADEC_CODEC_AAC) < 0)
			return -1;
		slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
		slot->internalError = 0U;
		slot->dec.audiocodec.esBuf = esBuf;
		slot->dec.audiocodec.pcmBuf = pcmBuf;
		SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AAC);
		SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AAC);
	} else {
		slot->internalError = 0U;
		slot->dec.audiocodec.esBuf = esBuf;
		slot->dec.audiocodec.pcmBuf = pcmBuf;
	}

	if (SceAudiocodec_E827E04F(slot, ME_ADEC_CODEC_AAC) < 0) {
		slot->internalError = slot->dec.audiocodec.errorCode;
		return -1;
	}

	*outConsumed = slot->dec.audiocodec.consumedBytes;
	int pcmBytes = (int)slot->dec.audiocodec.pcmOutBytes;
	if (pcmBytes > 2048) {
		*outPcmBytes = (uint32_t)pcmBytes;
	} else {
		*outPcmBytes = (uint32_t)(2 * pcmBytes);
		int16_t *pcm = (int16_t *)pcmBuf;
		for (int i = (pcmBytes / 2) - 1; i >= 0; i--) {
			int16_t sample = pcm[i];
			pcm[2 * i] = sample;
			pcm[2 * i + 1] = sample;
		}
	}
	return 0;
}

int meAacGetInternalError(uint32_t *outErr, void *workCtx)
{
	MeAdecSlot *slot = meAdecFindSlot(workCtx, sizeof(MeAacWorkCtx), ME_ADEC_BACKEND_AUDIOCODEC);
	if (slot != NULL)
		*outErr = slot->internalError;
	return 0;
}

int meAacSetSampleRate(uint8_t rateCode, void *workCtx)
{
	MeAacWorkCtx *ctx = (MeAacWorkCtx *)workCtx;
	ctx->rateCode = rateCode;
	return 0;
}

int meAacSetOption(uint32_t optA, uint32_t optB, uint32_t optC, void *workCtx)
{
	(void)optA;
	(void)optB;
	(void)optC;
	MeAacWorkCtx *ctx = (MeAacWorkCtx *)workCtx;
	ctx->optionFlag = 1U;
	return 0;
}

int meG729EncodeInit(uint32_t *workCtx, uint32_t mode, uint32_t flags)
{
	int created;
	meAdecDeleteSlotByKey(workCtx);
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, 0U, ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	slot->dec.audiocodec.workSize = (uint32_t)sceG729EncodeGetContextSize();
	if (meAdecAssignWorkBuf(slot) < 0)
		return -1;

	slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
	uint32_t rateVal = (mode <= 2U) ? (uint32_t)g_g729BitrateTable[mode] : 0U;
	slot->keySnapshot.words[0] = rateVal;
	int ret = sceG729EncodeInit(slot->dec.audiocodec.workBuf, mode, flags);
	workCtx[0] = mode;
	workCtx[1] = flags;
	return ret;
}

int meG729EncodeCore(uint32_t *workCtx, const void *pcmIn, void *bitstreamOut)
{
	int created;
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, 0U, ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	if (created != 0) {
		slot->dec.audiocodec.workSize = (uint32_t)sceG729EncodeGetContextSize();
		if (meAdecAssignWorkBuf(slot) < 0)
			return -1;
		slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
		uint32_t mode = workCtx[0];
		uint32_t flags = workCtx[1];
		uint32_t rateVal = (mode <= 2U) ? (uint32_t)g_g729BitrateTable[mode] : 0U;
		slot->keySnapshot.words[0] = rateVal;
		if (sceG729EncodeInit(slot->dec.audiocodec.workBuf, mode, flags) < 0)
			return -1;
	}
	return sceG729EncodeCore(slot->dec.audiocodec.workBuf, pcmIn, bitstreamOut);
}

int meG729DecodeInit(void *workCtx)
{
	int created;
	meAdecDeleteSlotByKey(workCtx);
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, 0U, ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	slot->dec.audiocodec.workSize = (uint32_t)sceG729DecodeGetContextSize();
	if (meAdecAssignWorkBuf(slot) < 0)
		return -1;

	slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
	return sceG729DecodeInit(slot->dec.audiocodec.workBuf);
}

int meG729DecodeCore(void *workCtx, int frameType, int frameLen, const void *bitstreamIn, void *pcmOut)
{
	int created;
	MeAdecSlot *slot = meAdecAcquireSlot(workCtx, 0U, ME_ADEC_BACKEND_AUDIOCODEC, &created);
	if (slot == NULL)
		return -1;

	if (created != 0) {
		slot->dec.audiocodec.workSize = (uint32_t)sceG729DecodeGetContextSize();
		if (meAdecAssignWorkBuf(slot) < 0)
			return -1;
		slot->backendType = ME_ADEC_BACKEND_AUDIOCODEC;
		if (sceG729DecodeInit(slot->dec.audiocodec.workBuf) < 0)
			return -1;
	}
	return sceG729DecodeCore(slot->dec.audiocodec.workBuf, frameType, frameLen, bitstreamIn, pcmOut);
}

int meAudioMixerMix(const void *mixerParams, void **inStreams, int16_t *outStereo, uint32_t streamCount, int sampleCount)
{
	(void)mixerParams;

	for (uint32_t s = 0; s < streamCount; s++) {
		const int16_t *src = (const int16_t *)inStreams[s];
		if (src == NULL || sampleCount <= 0)
			continue;

		int hasNonZero = 0;
		for (int i = 0; i < sampleCount; i++) {
			if (src[i] != 0) {
				hasNonZero = 1;
				break;
			}
		}
		if (!hasNonZero)
			continue;

		for (int i = 0; i < sampleCount; i++) {
			int16_t sample = src[i];
			int mixed = (int)sample + (int)outStereo[2 * i];
			if (sample < 0) {
				if (mixed < -32768)
					mixed = -32768;
			} else {
				if (mixed > 32767)
					mixed = 32767;
			}
			outStereo[2 * i] = (int16_t)mixed;
			outStereo[2 * i + 1] = (int16_t)mixed;
		}
	}
	return 0;
}

int meSasInit(void *ctx, uint32_t optA, uint32_t optB)
{
	(void)ctx;
	(void)optA;
	(void)optB;
	return 0;
}

int meSasSetOutputSize(void *ctx, uint32_t grainSamples, uint32_t optA, uint32_t optB, uint32_t optC, uint32_t optD)
{
	(void)ctx;
	(void)optA;
	(void)optB;
	(void)optC;
	(void)optD;
	g_sasBufferBytes = 4U * grainSamples;
	return 0;
}

int meSasCopyBuffer(void *ctx, const void *srcBuf, void *dstBuf)
{
	(void)ctx;
	memcpy(dstBuf, srcBuf, g_sasBufferBytes);
	return 0;
}

int meSasTerm(void *ctx, uint32_t optA, uint32_t optB)
{
	(void)ctx;
	(void)optA;
	(void)optB;
	return 0;
}
