/*
 * ePSP 6.61 sceAudio_Driver (kermit_audio.prx)
 *
 * Reimplemented from the 6.61 kermit_audio.prx binary, which adapts the
 * PSP sceAudio_Driver to tunnel PCM/SRC audio output and microphone input
 * over Kermit RPC (modes KERMIT_MODE_AUDIOOUT and KERMIT_MODE_AUDIOIN).
 */

#include <kermit.h>

/* Error codes */
#define SCE_ERROR_PRIV_REQUIRED            0x80000001
#define SCE_ERROR_NOT_SUPPORTED            0x80000003
#define SCE_ERROR_INVALID_SIZE             0x80000104

#define SCE_AUDIO_ERROR_NOT_INITIALIZED    0x80260001
#define SCE_AUDIO_ERROR_OUTPUT_BUSY        0x80260002
#define SCE_AUDIO_ERROR_INVALID_CH         0x80260003
#define SCE_AUDIO_ERROR_PRIV_REQUIRED      0x80260004
#define SCE_AUDIO_ERROR_NOT_FOUND          0x80260005
#define SCE_AUDIO_ERROR_INVALID_SIZE       0x80260006
#define SCE_AUDIO_ERROR_INVALID_FORMAT     0x80260007
#define SCE_AUDIO_ERROR_NOT_RESERVED       0x80260008
#define SCE_AUDIO_ERROR_NOT_OUTPUT         0x80260009
#define SCE_AUDIO_ERROR_INVALID_FREQUENCY  0x8026000A
#define SCE_AUDIO_ERROR_INVALID_VOLUME     0x8026000B
#define SCE_AUDIO_ERROR_INPUT_BUSY         0x80260010
#define SCE_AUDIO_ERROR_SRC_RESERVED       0x80268002

/* Kermit audio commands */
#define KERMIT_AUDIOOUT_PANNED_OUT         0x3DF
#define KERMIT_AUDIOOUT_GET_PARAM          0x3E0
#define KERMIT_AUDIOOUT_RESUME             0x3E1
#define KERMIT_AUDIOOUT_SUSPEND            0x3E2
#define KERMIT_AUDIOOUT_INIT               0x3E3
#define KERMIT_AUDIOOUT_SRC_OUT            0x3E4

#define KERMIT_AUDIOIN_INIT                0x770
#define KERMIT_AUDIOIN_STOP                0x771
#define KERMIT_AUDIOIN_INPUT               0x772

/* Shared memory buffers for Kermit audio */
#define KERMIT_AUDIOOUT_BUF_ADDR           0xABD28000u
#define KERMIT_AUDIOIN_BUF_ADDR            0xABD58000u
#define KERMIT_AUDIO_BUF_SIZE              0x8000u

/* Additional kernel imports not in kermit.h */
void *memset(void *s, int c, SceSize n);
int sceKernelGetCompiledSdkVersion(void);
s64 sceKernelGetSystemTimeWide(void);
SceUID sceKernelGetThreadId(void);
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
			     int init_priority, int stack_size, SceUInt attr, void *opt);
int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
int sceKernelExitThread(int status);
void sceDdrFlush(int type);
int sceKernelDmaOpQuit(void *op);
int sceKernelDmaOpFree(void *op);

/* Address space helpers */
#define UCACHED(p)   ((void *)((u32)(p) & 0x1FFFFFFFu))
#define KUNCACHED(p) ((void *)(0xA0000000u | ((u32)(p) & 0x1FFFFFFFu)))

static inline u32 pspShiftK1(void)
{
	u32 old_k1 = pspGetK1();
	pspSetK1(old_k1 << 11);
	return old_k1;
}

static inline int pspK1StaBufOk(const void *buf, u32 size)
{
	return (s32)(((u32)buf | ((u32)buf + size)) & pspGetK1()) >= 0;
}

static inline int pspK1DynBufOk(const void *buf, u32 size)
{
	return (s32)(((u32)buf | ((u32)buf + size) | size) & pspGetK1()) >= 0;
}

static inline s32 pspMin(s32 a, s32 b)
{
	return (a < b) ? a : b;
}

static inline s32 pspMax(s32 a, s32 b)
{
	return (a > b) ? a : b;
}

/* Audio structures */
typedef struct SceAudioInputParams {
	s32 unk0;
	s32 gain;
	s32 unk2;
	s32 unk3;
	s32 unk4;
	s32 unk5;
} SceAudioInputParams;

typedef struct SceAudioChannel {
	void *buf;              /* 0x00 */
	s32 curSampleCnt;       /* 0x04 */
	u16 sampleCount;        /* 0x08 */
	u8 unk10;               /* 0x0A */
	u8 bytesPerSample;      /* 0x0B */
	u16 leftVol;            /* 0x0C */
	u16 rightVol;           /* 0x0E */
} SceAudioChannel;

typedef struct SceAudio {
	u8 buf0[240];           /* 0x000 */
	u8 buf240[272];         /* 0x0F0 */
	u8 buf512[240];         /* 0x200 */
	u8 buf752[272];         /* 0x2F0 */
	u32 hwBuf[48];          /* 0x400 */
	void *dmaPtr[3];        /* 0x4C0 */
	SceUID evFlagId;        /* 0x4CC */
	SceAudioChannel chans[8]; /* 0x4D0 */
	u16 freq;               /* 0x550 */
	u16 hwFreq;             /* 0x552 */
	u8 flags;               /* 0x554 */
	s8 volumeOffset;        /* 0x555 */
	u8 delayShift;          /* 0x556 */
	u8 pad557;              /* 0x557 */
	s64 srcLastTime;        /* 0x558 */
	u16 srcChFreq;          /* 0x560 */
	u16 srcChSampleCnt;     /* 0x562 */
	u16 srcVol;             /* 0x564 */
	u8 numChans;            /* 0x566 */
	s8 srcQueuedCnt;        /* 0x567 */
	u32 inputOrigSampleCnt; /* 0x568 */
	u32 inputCurSampleCnt;  /* 0x56C */
	u16 *inputBuf;          /* 0x570 */
	s8 unkInput0;           /* 0x574 */
	s8 inputGain;           /* 0x575 */
	s8 unkInput2;           /* 0x576 */
	s8 unkInput3;           /* 0x577 */
	s8 unkInput4;           /* 0x578 */
	s8 unkInput5;           /* 0x579 */
	u8 inputHwFreq;         /* 0x57A */
	s8 inputIsWaiting;      /* 0x57B */
	s8 inputInited;         /* 0x57C */
	s8 unkCodecArg;         /* 0x57D */
	s8 unkCodecRet;         /* 0x57E */
	s8 unkCodecArgSet;      /* 0x57F */
} SceAudio;

/* Forward declarations */
static int audioEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int audioOutput(SceAudioChannel *channel, int leftVol, int rightVol, void *buf);
static int audioSRCGetRestDelay(void);
static int audioSRCOutput(int vol, void *buf);
static int audioInputThread(SceSize args, void *argp);
static int audioInput(int sampleCount, int freq, void *buf);
static int kermitAudioOutInit(void);
static int kermitAudioOutPanned(void *buf);
static int kermitAudioOutSRC(void *buf, u32 sampleCount, int vol, u32 freq);
static int kermitAudioOutSuspend(void);
static int kermitAudioOutPreSuspend(void);
static int kermitAudioOutResume(void);
static int kermitAudioOutPostResume(void);
static int kermitAudioOutIsSuspended(void);
static int kermitAudioOutIntrHandler(void);
static int kermitAudioOutSRCIntrHandler(void);
static int kermitAudioInInit(void);
static int kermitAudioInTransfer(void *buf);
static int kermitAudioInIntrHandler(void);
int sceAudioSRCChReserve(int sampleCount, int freq, int numChans);
int sceAudioSRCChRelease(void);
int sceAudioSRCOutputBlocking(int vol, void *buf);
int sceAudio_driver_95AE5A2B(u32 id, int val);

/* Data / BSS globals matching original layout */
static SceSysEventHandler g_audioEvent = {
	sizeof(SceSysEventHandler),
	"SceAudio",
	0x00FFFF00,
	audioEventHandler,
	0,
	0,
	NULL,
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

static int g_srcReleaseCheckBusy = 1;            /* 0x33C0 */
static int g_mixerBufIdx;                        /* 0x33D0 */
static int g_clearUnk10OnBusy;                   /* 0x33D4 */
static int g_srcRestTimeOffset;                  /* 0x33D8 */
static int g_restLenDelayUs;                     /* 0x33DC */
static int g_output2DelayUs;                     /* 0x33E0 */
static int g_allowBusySetDataLen;                /* 0x33E4 */
static int g_pannedBlockingDelayUs;              /* 0x33E8 */
static int g_srcSampleDurationUs;                /* 0x33EC */
static int g_srcDelayFactor;                     /* 0x33F0 */
static SceUID g_srcLastThreadId;                 /* 0x33F4 */
static u8 g_inputReqBufIdx;                      /* 0x33F8 */
static u8 g_inputReadBufIdx;                     /* 0x33F9 */
static int g_kermitAudioMuted;                   /* 0x33FC */
static int g_kermitAudioSuspended;               /* 0x3400 */
static SceUID g_kermitAudioOutMutex;             /* 0x3404 */
static SceUID g_kermitAudioInMutex;              /* 0x3408 */
static u32 g_kermitAudioOutBuf;                  /* 0x340C */
static u32 g_kermitAudioInBuf;                   /* 0x3410 */
static SceAudio g_audio __attribute__((aligned(64))); /* 0x3440 */

/* 0x0000: Start audio mixer or SRC event flag */
static int updateAudioBuf(int arg)
{
	g_audio.flags |= (u8)(arg + 1);
	sceKernelSetEventFlag(g_audio.evFlagId, 0x20000000u << arg);
	return 0;
}

/* 0x0044: SceAudioMixer thread */
static int audioMixerThread(SceSize args, void *argp)
{
	s32 mixBuf[128];
	SceAudio *uncachedAudio = (SceAudio *)KUNCACHED(&g_audio);

	memset(mixBuf, 0, sizeof(mixBuf));

	for (;;) {
		u32 doneFlags = 0;
		int hasAudio = 0;
		int j;

		if (sceKernelWaitEventFlag(g_audio.evFlagId, 0x20000000u, 0x21, NULL, NULL) < 0) {
			sceKernelExitThread(0);
			return 0;
		}

		for (j = 0; j < 8; j++) {
			SceAudioChannel *chan = &g_audio.chans[j];
			s16 *buf = (s16 *)chan->buf;

			if (buf != NULL) {
				s32 playedSamples = pspMin(chan->curSampleCnt, 64);
				s32 rem = chan->curSampleCnt - playedSamples;
				u32 step = chan->bytesPerSample;
				u16 leftVol = chan->leftVol;
				u16 rightVol = chan->rightVol;
				s32 i;

				hasAudio = 1;
				for (i = 0; i < playedSamples * 2; i += 2) {
					s16 l = buf[0];
					buf = (s16 *)((u8 *)buf + step);
					s16 r = buf[-1];
					mixBuf[i + 0] += ((s32)l * (s32)leftVol) >> 7;
					mixBuf[i + 1] += ((s32)r * (s32)rightVol) >> 7;
				}

				if (rem == 0) {
					doneFlags |= 1u << j;
					buf = NULL;
				} else {
					chan->curSampleCnt = rem;
				}
				chan->buf = buf;
			}
		}

		if (!hasAudio) {
			int intr = sceKernelCpuSuspendIntr();
			g_audio.flags &= ~1u;
			sceKernelCpuResumeIntr(intr);
		} else {
			int bufIdx = g_mixerBufIdx;
			s8 shift = g_audio.volumeOffset;
			u32 *dstBuf = (u32 *)(uncachedAudio->buf0 + (bufIdx << 8));
			int i;

			g_mixerBufIdx = bufIdx ^ 1;
			for (i = 0; i < 64; i++) {
				s32 l = pspMax(pspMin(mixBuf[i * 2 + 0] >> shift, 0x7FFF), -0x8000);
				s32 r = pspMax(pspMin(mixBuf[i * 2 + 1] >> shift, 0x7FFF), -0x8000);
				mixBuf[i * 2 + 0] = 0;
				mixBuf[i * 2 + 1] = 0;
				dstBuf[i] = ((u32)l & 0xFFFFu) | ((u32)r << 16);
			}

			sceDdrFlush(4);
			kermitAudioOutPanned(dstBuf);

			if ((g_audio.flags & 1) == 0) {
				int intr = sceKernelCpuSuspendIntr();
				updateAudioBuf(0);
				sceKernelCpuResumeIntr(intr);
			}
			sceKernelSetEventFlag(g_audio.evFlagId, doneFlags);
		}
	}
}

/* 0x0290 */
int sceAudioOutput(u32 chanId, int vol, void *buf)
{
	if (vol > 0xFFFF)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	u32 oldK1 = pspShiftK1();
	if (!pspK1StaBufOk(buf, (u32)chan->sampleCount << 2)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	int ret = audioOutput(chan, vol, vol, buf);
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return ret;
}

/* 0x0370 */
int sceAudioOutputBlocking(u32 chanId, int vol, void *buf)
{
	if (vol > 0xFFFF)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	u32 oldK1 = pspShiftK1();
	if (!pspK1StaBufOk(buf, (u32)chan->sampleCount << 2)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	int ret = audioOutput(chan, vol, vol, buf);
	if ((u32)ret == SCE_AUDIO_ERROR_OUTPUT_BUSY) {
		if (chan->unk10 == 0) {
			chan->unk10 = (buf == NULL) ? 0xFF : 1;
			do {
				sceKernelCpuResumeIntr(intr);
				int waitRet = sceKernelWaitEventFlag(g_audio.evFlagId, 1u << chanId, 0x20, NULL, NULL);
				if (waitRet < 0) {
					pspSetK1(oldK1);
					return waitRet;
				}
				intr = sceKernelCpuSuspendIntr();
				ret = audioOutput(chan, vol, vol, buf);
			} while ((u32)ret == SCE_AUDIO_ERROR_OUTPUT_BUSY);
			chan->unk10 = 0;
		}
	}
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return ret;
}

/* 0x04FC */
int sceAudioOutputPanned(u32 chanId, int leftVol, int rightVol, void *buf)
{
	if (rightVol > 0xFFFF || leftVol > 0xFFFF)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	u32 oldK1 = pspShiftK1();
	if (!pspK1StaBufOk(buf, (u32)chan->sampleCount << 2)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	int ret = audioOutput(chan, leftVol, rightVol, buf);
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return ret;
}

/* 0x05F0 */
int sceAudioOutputPannedBlocking(u32 chanId, int leftVol, int rightVol, void *buf)
{
	if ((u32)(leftVol | rightVol) > 0xFFFFu)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	u32 oldK1 = pspShiftK1();
	if (!pspK1StaBufOk(buf, (u32)chan->sampleCount << 2)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	int ret = audioOutput(chan, leftVol, rightVol, buf);
	if (chan->unk10 != 0 && g_clearUnk10OnBusy != 0)
		chan->unk10 = 0;

	if ((u32)ret == SCE_AUDIO_ERROR_OUTPUT_BUSY) {
		if (chan->unk10 == 0) {
			chan->unk10 = (buf == NULL) ? 0xFF : 1;
			do {
				sceKernelCpuResumeIntr(intr);
				int waitRet = sceKernelWaitEventFlag(g_audio.evFlagId, 1u << chanId, 0x20, NULL, NULL);
				if (waitRet < 0) {
					pspSetK1(oldK1);
					return waitRet;
				}
				intr = sceKernelCpuSuspendIntr();
				ret = audioOutput(chan, leftVol, rightVol, buf);
			} while ((u32)ret == SCE_AUDIO_ERROR_OUTPUT_BUSY);
			chan->unk10 = 0;
		}
	}

	sceKernelCpuResumeIntr(intr);
	if (chanId - 5u < (u32)g_audio.delayShift)
		sceKernelDelayThread(3000);
	if (g_pannedBlockingDelayUs != 0)
		sceKernelDelayThread(g_pannedBlockingDelayUs);

	pspSetK1(oldK1);
	return ret;
}

/* 0x07F4 */
int sceAudioChReserve(int channel, int sampleCount, int format)
{
	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();

	if (channel < 0) {
		for (channel = 7; channel >= 0; channel--) {
			if (g_audio.chans[channel].sampleCount == 0 && g_audio.chans[channel].buf == NULL)
				break;
		}
		if (channel < 0) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_NOT_FOUND;
		}
	} else if ((u32)channel >= 8 || g_audio.chans[channel].sampleCount != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (sampleCount <= 0 || (sampleCount & 0x3F) != 0 || (u32)sampleCount > 0xFFC0u) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INVALID_SIZE;
	}

	u8 bytesPerSample = 4;
	if (format != 0) {
		if (format != 0x10) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_INVALID_FORMAT;
		}
		bytesPerSample = 2;
	}

	g_audio.chans[channel].bytesPerSample = bytesPerSample;
	g_audio.chans[channel].sampleCount = (u16)sampleCount;
	g_audio.chans[channel].leftVol = 0;
	g_audio.chans[channel].rightVol = 0;

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return channel;
}

/* 0x095C */
int sceAudioOneshotOutput(int chanId, int sampleCount, int fmt, int leftVol, int rightVol, void *buf)
{
	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();

	if ((u32)leftVol > 0xFFFFu || (u32)rightVol > 0xFFFFu) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	}

	if (chanId < 0) {
		for (chanId = 7; chanId >= 0; chanId--) {
			if (g_audio.chans[chanId].sampleCount == 0 && g_audio.chans[chanId].buf == NULL)
				break;
		}
		if (chanId < 0) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_NOT_FOUND;
		}
	} else if ((u32)chanId >= 8 || g_audio.chans[chanId].sampleCount != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INVALID_CH;
	}

	if (sampleCount <= 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INVALID_SIZE;
	}

	if (!pspK1DynBufOk(buf, (u32)sampleCount << 2)) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	u8 bytesPerSample = 4;
	if (fmt != 0) {
		if (fmt != 0x10) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_INVALID_FORMAT;
		}
		bytesPerSample = 2;
	}

	SceAudioChannel *chan = &g_audio.chans[chanId];
	chan->leftVol = (u16)leftVol;
	chan->rightVol = (u16)rightVol;
	chan->bytesPerSample = bytesPerSample;
	chan->curSampleCnt = sampleCount;
	chan->buf = buf;

	if ((g_audio.flags & 1) == 0 && buf != NULL) {
		u32 *p = (u32 *)KUNCACHED(g_audio.buf0);
		int i;
		for (i = 0; i < 128; i++)
			p[i] = 0;
		updateAudioBuf(0);
	}

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return chanId;
}

/* 0x0B90 */
int sceAudioChRelease(u32 channel)
{
	if (channel >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	SceAudioChannel *chan = &g_audio.chans[channel];

	if (chan->sampleCount == 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}
	if (chan->unk10 != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_OUTPUT_BUSY;
	}

	chan->sampleCount = 0;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x0C44 */
int sceAudioGetChannelRestLength(u32 chanId)
{
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	int ret = 0;
	if (chan->buf != NULL)
		ret = chan->curSampleCnt;
	if (chan->unk10 != 0)
		ret += chan->sampleCount;
	if (g_restLenDelayUs != 0)
		sceKernelDelayThread(g_restLenDelayUs);
	return ret;
}

/* 0x0CCC */
int sceAudioSetChannelDataLen(u32 chanId, int sampleCount)
{
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;
	if (sampleCount <= 0 || (sampleCount & 0x3F) != 0 || (u32)sampleCount > 0xFFC0u)
		return SCE_AUDIO_ERROR_INVALID_SIZE;

	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	SceAudioChannel *chan = &g_audio.chans[chanId];

	if (chan->unk10 != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return (g_allowBusySetDataLen != 0) ? 0 : SCE_AUDIO_ERROR_OUTPUT_BUSY;
	}
	if (chan->sampleCount == 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	}

	chan->sampleCount = (u16)sampleCount;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x0DBC */
int sceAudioChangeChannelVolume(u32 chanId, int leftVol, int rightVol)
{
	if (rightVol > 0xFFFF || leftVol > 0xFFFF)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	SceAudioChannel *chan = &g_audio.chans[chanId];

	if (leftVol >= 0)
		chan->leftVol = (u16)leftVol;
	if (rightVol >= 0)
		chan->rightVol = (u16)rightVol;

	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x0E6C */
int sceAudioChangeChannelConfig(u32 chanId, int format)
{
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	u32 oldK1 = pspShiftK1();
	int intr = sceKernelCpuSuspendIntr();
	SceAudioChannel *chan = &g_audio.chans[chanId];

	if (chan->unk10 != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_OUTPUT_BUSY;
	}

	if (sceKernelGetCompiledSdkVersion() <= 0x01FFFFFF) {
		if (chan->curSampleCnt != 0) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_OUTPUT_BUSY;
		}
	} else {
		if (chan->curSampleCnt != 0 && chan->buf != NULL) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_OUTPUT_BUSY;
		}
	}

	if (chan->sampleCount == 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}

	u8 bytesPerSample = 4;
	if (format != 0) {
		if (format != 0x10) {
			sceKernelCpuResumeIntr(intr);
			pspSetK1(oldK1);
			return SCE_AUDIO_ERROR_INVALID_FORMAT;
		}
		bytesPerSample = 2;
	}

	chan->bytesPerSample = bytesPerSample;
	sceKernelCpuResumeIntr(intr);
	pspSetK1(oldK1);
	return 0;
}

/* 0x0FC8 */
int sceAudioOutput2ChangeLength(int sampleCount)
{
	if ((u32)(sampleCount - 17) >= 0xFFFu)
		return SCE_AUDIO_ERROR_INVALID_SIZE;

	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.srcChFreq == 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}
	g_audio.srcChSampleCnt = (u16)sampleCount;
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x1038 */
int sceAudioOutput2GetRestSample(void)
{
	u32 oldK1 = pspGetK1();
	u16 freq = g_audio.srcChFreq;
	if (freq == 0)
		return SCE_AUDIO_ERROR_NOT_RESERVED;

	s8 queued = g_audio.srcQueuedCnt;
	pspSetK1(oldK1 << 11);
	u16 sampleCnt = g_audio.srcChSampleCnt;

	if (queued < 2) {
		s64 lastTime = g_audio.srcLastTime;
		if (lastTime != 0 && freq != 0) {
			s64 now = sceKernelGetSystemTimeWide();
			u64 elapsed = (u64)(now - lastTime);
			u64 dur = ((u64)sampleCnt * 1000000ULL) / (u64)freq + (u32)g_srcRestTimeOffset;
			if (elapsed < dur)
				queued++;
		}
	}

	pspSetK1(oldK1);
	return (int)queued * (int)sampleCnt;
}

/* 0x1158..0x1194: Debug parameter setters */
static void setAudioParam1(int val) { g_clearUnk10OnBusy = val; }
static void setAudioParam2(int val) { g_srcRestTimeOffset = val; }
static void setAudioParam3(int val) { g_restLenDelayUs = val; }
static void setAudioParam4(int val) { g_output2DelayUs = val; }
static void setAudioParam5(int val) { g_allowBusySetDataLen = val; }
static void setAudioParam6(int val) { g_pannedBlockingDelayUs = val; }

/* 0x11A0 */
int sceAudioGetChannelRestLen(u32 chanId)
{
	if (chanId >= 8)
		return SCE_AUDIO_ERROR_INVALID_CH;

	SceAudioChannel *chan = &g_audio.chans[chanId];
	if (chan->unk10 == 0)
		return chan->curSampleCnt;
	return chan->curSampleCnt + chan->sampleCount;
}

/* 0x11E0 */
int sceAudioOutput2Reserve(int sampleCount)
{
	return sceAudioSRCChReserve(sampleCount, 44100, 2);
}

/* 0x1200 */
int sceAudioOutput2OutputBlocking(int vol, void *buf)
{
	if (g_output2DelayUs != 0)
		sceKernelDelayThread(g_output2DelayUs);
	return sceAudioSRCOutputBlocking(vol, buf);
}

/* 0x1258 */
int sceAudioOutput2Release(void)
{
	return sceAudioSRCChRelease();
}

/* 0x1274 */
static int audioOutput(SceAudioChannel *channel, int leftVol, int rightVol, void *buf)
{
	if (channel->sampleCount == 0)
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	if (channel->buf != NULL)
		return SCE_AUDIO_ERROR_OUTPUT_BUSY;

	channel->curSampleCnt = channel->sampleCount;
	if (leftVol >= 0)
		channel->leftVol = (u16)leftVol;
	if (rightVol >= 0)
		channel->rightVol = (u16)rightVol;
	channel->buf = buf;

	if ((g_audio.flags & 1) == 0 && buf != NULL) {
		u32 *p = (u32 *)KUNCACHED(g_audio.buf0);
		int i;
		for (i = 0; i < 128; i++)
			p[i] = 0;
		updateAudioBuf(0);
	}
	return channel->sampleCount;
}

/* 0x1328 */
int sceAudioSetFrequency(int freq)
{
	if (g_audio.freq == freq)
		return 0;
	if (freq == 44100 || freq == 48000)
		return 0;
	return SCE_AUDIO_ERROR_INVALID_FREQUENCY;
}

/* 0x1CEC, 0x1CF8 */
static void setAudioParam7(int val) { g_srcDelayFactor = val; }
static void setAudioParam8(void) { g_srcReleaseCheckBusy = 0; }

/* 0x1360 */
int sceAudio_driver_95AE5A2B(u32 id, int val)
{
	Kprintf("===== audio param - id:%x - val:%x =====\n", id, val);
	switch (id) {
	case 0:
		break;
	case 1:
		g_audio.delayShift = (val != 0) ? 2 : 0;
		break;
	case 2:
		setAudioParam7(val);
		break;
	case 3:
		setAudioParam1(val);
		break;
	case 4:
		setAudioParam2(val);
		break;
	case 5:
		setAudioParam3(val);
		break;
	case 6:
		setAudioParam4(val);
		break;
	case 7:
		setAudioParam5(val);
		break;
	case 8:
		setAudioParam6(val);
		break;
	case 9:
		setAudioParam8();
		break;
	default:
		break;
	}
	return 0;
}

/* 0x145C */
int sceAudioInit(SceSize args, void *argp)
{
	int i;
	int ret;

	memset(&g_audio, 0, sizeof(g_audio));
	g_audio.evFlagId = sceKernelCreateEventFlag("SceAudio", 0x201, 0, NULL);
	g_audio.volumeOffset = 8;
	g_audio.freq = 44100;

	for (i = 0; i < 4; i += 2) {
		int shift = (i / 2) * 0x100;
		u32 *buf = &g_audio.hwBuf[i * 4];
		buf[0] = (u32)UCACHED(g_audio.buf0 + shift);
		buf[1] = (u32)UCACHED(0xBE000060u);
		buf[2] = (u32)UCACHED(&g_audio.hwBuf[i * 4 + 4]);
		buf[3] = 0x0448903Cu;
		buf[4] = (u32)UCACHED(g_audio.buf240 + shift);
		buf[5] = (u32)UCACHED(0xBE000060u);
		buf[6] = 0;
		buf[7] = 0x84489004u;

		buf[17] = (u32)UCACHED(0xBE000070u);
		buf[18] = 0;
		buf[21] = (u32)UCACHED(0xBE000070u);
		buf[22] = 0;

		buf[32] = (u32)UCACHED(0xBE000080u);
		buf[33] = (u32)UCACHED(g_audio.buf512 + shift);
		buf[34] = (u32)UCACHED(&g_audio.hwBuf[i * 4 + 36]);
		buf[35] = 0x0848903Cu;
		buf[36] = (u32)UCACHED(0xBE000080u);
		buf[37] = (u32)UCACHED(g_audio.buf752 + shift);
		buf[38] = 0;
		buf[39] = 0x88489004u;
	}

	ret = kermitAudioOutInit();
	if (ret != 0) {
		Kprintf("Failed to init audio out %08x\n", ret);
		return ret;
	}

	ret = kermitAudioInInit();
	if (ret != 0) {
		Kprintf("Failed to init audio in %08x\n", ret);
		return ret;
	}

	SceUID mixerThid = sceKernelCreateThread("SceAudioMixer", audioMixerThread, 5, 0x600, 0x100000, NULL);
	if (mixerThid < 0 || sceKernelStartThread(mixerThid, 0, NULL) != 0)
		return 1;

	SceUID inputThid = sceKernelCreateThread("SceAudioInput", audioInputThread, 6, 0x400, 0x100000, NULL);
	if (inputThid < 0 || sceKernelStartThread(inputThid, 0, NULL) != 0)
		return 1;

	sceKernelRegisterSysEventHandler(&g_audioEvent);
	sceKernelDcacheWritebackInvalidateRange(&g_audio, sizeof(g_audio));
	g_audio.unkCodecArg = 1;
	g_audio.unkCodecArgSet = 0;
	g_audio.inputInited = 0;
	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("sceAudioInit")));

/* 0x16DC */
int sceAudioEnd(SceSize args, void *argp)
{
	int i;

	sceKernelUnregisterSysEventHandler(&g_audioEvent);
	int intr = sceKernelCpuSuspendIntr();
	sceKernelDeleteEventFlag(g_audio.evFlagId);
	sceKernelReleaseIntrHandler(10);
	for (i = 0; i < 3; i++) {
		sceKernelDmaOpQuit(g_audio.dmaPtr[i]);
		sceKernelDmaOpFree(g_audio.dmaPtr[i]);
	}
	sceKernelCpuResumeIntr(intr);
	return 0;
}

int module_stop(SceSize args, void *argp) __attribute__((alias("sceAudioEnd")));
int module_reboot_before(SceSize args, void *argp) __attribute__((alias("sceAudioEnd")));

/* 0x1768 */
int sceAudio_driver_FF298CE7(int arg)
{
	g_audio.delayShift = (arg != 0) ? 2 : 0;
	return 0;
}

/* 0x1780 */
int sceAudioLoopbackTest(int arg)
{
	return 0;
}

/* 0x1788 */
int sceAudioSetVolumeOffset(int arg)
{
	g_audio.volumeOffset = (s8)(arg + 8);
	return 0;
}

/* 0x179C */
static int audioEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	int ret;

	switch (ev_id) {
	case 0x200:
		ret = kermitAudioOutSuspend();
		if (ret != 0) {
			Kprintf("Failed to suspend audio out %08x\n", ret);
			return ret;
		}
		break;
	case 0x1000:
		sceKernelSetEventFlag(g_audio.evFlagId,
				      (g_audio.inputCurSampleCnt != 0) ? 0xA0000000u : 0x20000000u);
		g_audio.flags = 0;
		ret = kermitAudioOutPreSuspend();
		if (ret != 0) {
			Kprintf("Failed to suspend audio out %08x\n", ret);
			return ret;
		}
		break;
	case 0x1000F:
		ret = kermitAudioOutPostResume();
		if (ret != 0) {
			Kprintf("Failed to resume audio out %08x\n", ret);
			return ret;
		}
		break;
	case 0x100000:
		ret = kermitAudioOutResume();
		if (ret != 0) {
			Kprintf("Failed to resume audio out %08x\n", ret);
			return ret;
		}
		break;
	default:
		break;
	}
	return 0;
}

/* 0x1874 */
int sceAudioSRCChReserve(int sampleCount, int freq, int numChans)
{
	if (numChans != 4 && numChans != 2)
		return SCE_ERROR_INVALID_SIZE;
	if (numChans == 4)
		return SCE_ERROR_NOT_SUPPORTED;

	int totalSamples = (sampleCount * numChans) / 2;
	if ((u32)(totalSamples - 17) >= 0xFFFu)
		return SCE_ERROR_INVALID_SIZE;

	u16 hwFreq;
	if (freq == 0 || freq == g_audio.freq) {
		freq = g_audio.freq;
		hwFreq = 0;
	} else {
		switch (freq) {
		case 8000:
			hwFreq = 1;
			break;
		case 11025:
			hwFreq = 2;
			break;
		case 12000:
			hwFreq = 4;
			break;
		case 16000:
			hwFreq = 8;
			break;
		case 22050:
			hwFreq = 16;
			break;
		case 24000:
			hwFreq = 32;
			break;
		case 32000:
			hwFreq = 64;
			break;
		case 48000:
			hwFreq = 256;
			break;
		default:
			return SCE_AUDIO_ERROR_INVALID_FREQUENCY;
		}
	}

	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.srcChFreq != 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_SRC_RESERVED;
	}

	g_audio.srcChFreq = (u16)freq;
	g_audio.srcChSampleCnt = (u16)sampleCount;
	if (freq != 0)
		g_srcSampleDurationUs = (int)(((s64)sampleCount * 1000000LL) / (s64)freq);
	else
		g_srcSampleDurationUs = 0;

	g_audio.hwFreq = hwFreq;
	g_audio.numChans = (u8)(numChans << 1);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x1A74 */
int sceAudioSRCChRelease(void)
{
	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.srcChFreq == 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}

	if (audioSRCGetRestDelay() != 0 && g_srcReleaseCheckBusy != 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_SRC_RESERVED;
	}

	g_srcSampleDurationUs = 0;
	g_audio.srcQueuedCnt = 0;
	g_audio.srcChFreq = 0;
	sceKernelSetEventFlag(g_audio.evFlagId, 0x40000000u);
	sceKernelCpuResumeIntr(intr);
	return 0;
}

/* 0x1B1C */
int sceAudioSRCOutputBlocking(int vol, void *buf)
{
	if ((u32)vol > 0xFFFFFu)
		return SCE_AUDIO_ERROR_INVALID_VOLUME;

	u32 oldK1 = pspShiftK1();
	if (!pspK1StaBufOk(buf, (u32)g_audio.srcChSampleCnt * (u32)g_audio.numChans)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	if (kermitAudioOutIsSuspended() != 0) {
		s64 now = sceKernelGetSystemTimeWide();
		u32 elapsed = (u32)now - (u32)g_audio.srcLastTime;
		u32 dur = (u32)(((u64)g_audio.srcChSampleCnt * 1000000ULL) / (u64)g_audio.srcChFreq);
		s32 delay = (s32)(dur - elapsed);
		if (delay > 0)
			sceKernelDelayThread((u32)delay);
		g_audio.srcLastTime = sceKernelGetSystemTimeWide();
		pspSetK1(oldK1);
		return g_audio.srcChSampleCnt;
	}

	int ret = audioSRCOutput(vol, buf);
	if (ret > 0) {
		int waitRet = sceKernelWaitEventFlag(g_audio.evFlagId, 0x40000000u, 0x20, NULL, NULL);
		if (waitRet < 0)
			ret = waitRet;
	} else if (ret == 0) {
		int delay = audioSRCGetRestDelay();
		if (delay == 0) {
			g_audio.srcQueuedCnt = 0;
		} else {
			s64 before = g_audio.srcLastTime;
			sceKernelDelayThread((u32)delay);
			if (g_audio.srcLastTime == before)
				g_audio.srcQueuedCnt = 0;
		}
	}

	pspSetK1(oldK1);
	return ret;
}

/* 0x1D04 */
static int audioSRCGetRestDelay(void)
{
	int intr = sceKernelCpuSuspendIntr();
	s64 lastTime = g_audio.srcLastTime;
	int sampleDur = g_srcSampleDurationUs;
	s8 queued = g_audio.srcQueuedCnt;
	sceKernelCpuResumeIntr(intr);

	if (lastTime == 0 || sampleDur <= 0)
		return 0;

	s64 now = sceKernelGetSystemTimeWide();
	int factor = (g_srcDelayFactor != 0) ? g_srcDelayFactor : ((int)queued + 1);
	u64 totalDur = (u64)(u32)(sampleDur * factor);
	u64 elapsed = (u64)(now - lastTime);
	if (elapsed >= totalDur)
		return 0;
	return (int)((u32)totalDur - (u32)elapsed);
}

/* 0x1DD4 */
static int audioSRCOutput(int vol, void *buf)
{
	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.srcChFreq == 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_NOT_RESERVED;
	}

	g_audio.srcLastTime = sceKernelGetSystemTimeWide();
	if (buf == NULL) {
		sceKernelCpuResumeIntr(intr);
		return 0;
	}

	SceUID thid = sceKernelGetThreadId();
	if (g_audio.srcQueuedCnt > 0 && thid != g_srcLastThreadId) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_OUTPUT_BUSY;
	}

	u16 sampleCnt = g_audio.srcChSampleCnt;
	u32 byteSize = (u32)sampleCnt * (u32)g_audio.numChans;
	g_srcLastThreadId = thid;
	if (((0xD3 >> (((u32)buf >> 29) & 7)) & 1) != 0)
		sceKernelDcacheWritebackRange(buf, byteSize);

	g_audio.srcQueuedCnt++;
	sceKernelCpuResumeIntr(intr);

	int ret = kermitAudioOutSRC(buf, sampleCnt, vol, g_audio.srcChFreq);
	if (ret == 0)
		return g_audio.srcChSampleCnt;
	return ret;
}

/* 0x1F08: SceAudioInput thread */
static int audioInputThread(SceSize args, void *argp)
{
	for (;;) {
		if (sceKernelWaitEventFlag(g_audio.evFlagId, 0x80000000u, 0x21, NULL, NULL) < 0) {
			sceKernelExitThread(0);
			return 0;
		}

		u8 readIdx = g_inputReadBufIdx;
		s32 curSampleCnt = (s32)g_audio.inputCurSampleCnt;
		s16 *src = (s16 *)KUNCACHED(&g_audio.buf512[(readIdx & 1) << 8]);
		u8 inputHwFreq = g_audio.inputHwFreq;

		if (curSampleCnt == 0) {
			if (g_audio.inputIsWaiting == 0 && g_audio.unkCodecArgSet == 0) {
				kermitAudioInTransfer(NULL);
				int intr = sceKernelCpuSuspendIntr();
				g_audio.flags &= ~4u;
				sceKernelCpuResumeIntr(intr);
			}
		} else {
			u16 *dst = g_audio.inputBuf;
			g_inputReadBufIdx = readIdx + 1;

			if (dst != NULL) {
				if (g_audio.unkCodecRet != 0)
					src++;
				int shift = inputHwFreq >> 2;
				int i;
				for (i = 128; i > 0; i -= inputHwFreq) {
					s32 v = src[0];
					if (inputHwFreq >= 4) {
						v += src[2];
						if (inputHwFreq >= 8)
							v += src[4] + src[6];
					}
					*dst++ = (u16)(v >> shift);
					src += inputHwFreq;
				}
				g_audio.inputBuf = dst;
			}

			curSampleCnt -= (0x40 >> (inputHwFreq >> 2));
			if (curSampleCnt <= 0) {
				curSampleCnt = 0;
				sceKernelSetEventFlag(g_audio.evFlagId, 0x100u);
				g_audio.inputBuf = NULL;
			}
			g_audio.inputCurSampleCnt = (u32)curSampleCnt;

			int intr = sceKernelCpuSuspendIntr();
			if (g_audio.inputInited == 0) {
				g_audio.inputCurSampleCnt = 0;
				sceKernelCpuResumeIntr(intr);
			} else {
				sceKernelCpuResumeIntr(intr);
				u8 reqIdx = g_inputReqBufIdx;
				g_inputReqBufIdx = reqIdx + 1;
				kermitAudioInTransfer(&g_audio.buf512[(reqIdx & 1) << 8]);
			}
		}
	}
}

/* 0x20C8 */
int sceAudioWaitInputEnd(void)
{
	int ret = 0;
	if (g_audio.inputOrigSampleCnt == 0)
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;

	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.inputIsWaiting != 0) {
		sceKernelCpuResumeIntr(intr);
		return SCE_AUDIO_ERROR_INPUT_BUSY;
	}
	g_audio.inputIsWaiting = 1;
	sceKernelCpuResumeIntr(intr);

	u32 oldK1 = pspShiftK1();
	while (g_audio.inputCurSampleCnt != 0)
		ret = sceKernelWaitEventFlag(g_audio.evFlagId, 0x100u, 0x20, NULL, NULL);

	g_audio.inputIsWaiting = 0;
	pspSetK1(oldK1);
	return ret;
}

/* 0x2190 */
int sceAudioInputInitEx(SceAudioInputParams *param)
{
	u32 oldK1 = pspGetK1();
	pspSetK1(oldK1 << 11);
	int ok = pspK1StaBufOk(param, sizeof(SceAudioInputParams));
	pspSetK1(oldK1);
	if (!ok)
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;

	g_audio.unkInput0 = (s8)pspMin(pspMax(param->unk0, -29), 0);
	g_audio.inputGain = (s8)pspMin(pspMax(param->gain, -18), 30);
	g_audio.unkInput2 = (s8)pspMin(pspMax(param->unk2, -76), 0);
	g_audio.unkInput3 = (s8)pspMin(pspMax(param->unk3, 0), 15);
	g_audio.unkInput4 = (s8)pspMin(pspMax(param->unk4, 0), 10);
	g_audio.unkInput5 = (s8)pspMin(pspMax(param->unk5, 0), 10);
	g_audio.inputInited = 1;
	g_audio.inputCurSampleCnt = 0;
	g_audio.unkCodecRet = 0;
	g_audio.inputOrigSampleCnt = 0;
	return 0;
}

/* 0x225C */
int sceAudioInputInit(int arg0, int gain, int arg2)
{
	u32 oldK1 = pspShiftK1();
	g_audio.unkInput0 = (s8)pspMin(pspMax(arg0, -29), 0);
	g_audio.inputGain = (s8)pspMin(pspMax(gain, -18), 30);
	g_audio.unkInput2 = (s8)pspMin(pspMax(arg2, -76), 0);
	g_audio.unkInput3 = 0;
	g_audio.unkInput4 = 3;
	g_audio.unkInput5 = 2;
	g_audio.inputInited = 1;
	g_audio.inputCurSampleCnt = 0;
	g_audio.unkCodecRet = 0;
	g_audio.inputOrigSampleCnt = 0;
	pspSetK1(oldK1);
	return 0;
}

/* 0x22D8 */
int sceAudioInputBlocking(int sampleCount, int freq, void *buf)
{
	sceAudioWaitInputEnd();
	return audioInput(sampleCount, freq, buf);
}

/* 0x2324 */
int sceAudioInput(int sampleCount, int freq, void *buf)
{
	return audioInput(sampleCount, freq, buf);
}

/* 0x2340 */
int sceAudioGetInputLength(void)
{
	u32 orig = g_audio.inputOrigSampleCnt;
	if (orig == 0)
		return SCE_AUDIO_ERROR_NOT_INITIALIZED;
	return (int)(orig - g_audio.inputCurSampleCnt);
}

/* 0x2368 */
int sceAudioPollInputEnd(void)
{
	if (g_audio.inputInited == 0)
		return SCE_ERROR_PRIV_REQUIRED;
	if (g_audio.inputOrigSampleCnt == 0)
		return 0;
	return (g_audio.inputCurSampleCnt > 0) ? 1 : 0;
}

/* 0x239C */
int sceAudio_driver_5182B550(int arg)
{
	if (g_audio.unkCodecArg != arg) {
		g_audio.unkCodecArgSet = 1;
		g_audio.unkCodecArg = (s8)arg;
	}
	return 0;
}

/* 0x23C4 */
static int audioInput(int sampleCount, int freq, void *buf)
{
	int ret = 0;

	if (g_audio.inputInited == 0)
		return SCE_ERROR_PRIV_REQUIRED;
	if (sampleCount <= 0 || (sampleCount & 0x3F) != 0)
		return SCE_AUDIO_ERROR_INVALID_SIZE;

	u8 hwFreq;
	switch (freq) {
	case 11025:
		hwFreq = 8;
		break;
	case 22050:
		hwFreq = 4;
		break;
	case 44100:
		hwFreq = 2;
		break;
	default:
		return SCE_AUDIO_ERROR_INVALID_FREQUENCY;
	}

	if (buf == NULL) {
		g_audio.inputOrigSampleCnt = 0;
		g_audio.inputBuf = NULL;
		return 0;
	}

	u32 oldK1 = pspShiftK1();
	if (!pspK1DynBufOk(buf, (u32)sampleCount << 1)) {
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_PRIV_REQUIRED;
	}

	int intr = sceKernelCpuSuspendIntr();
	if (g_audio.inputCurSampleCnt != 0) {
		sceKernelCpuResumeIntr(intr);
		pspSetK1(oldK1);
		return SCE_AUDIO_ERROR_INPUT_BUSY;
	}

	g_audio.inputBuf = (u16 *)buf;
	g_audio.inputHwFreq = hwFreq;
	g_audio.inputOrigSampleCnt = (u32)sampleCount;
	g_audio.inputCurSampleCnt = (u32)sampleCount;
	sceKernelCpuResumeIntr(intr);

	if ((g_audio.flags & 4) == 0) {
		u8 reqIdx = g_inputReqBufIdx;
		g_inputReadBufIdx = reqIdx;
		g_inputReqBufIdx = reqIdx + 1;
		ret = kermitAudioInTransfer(&g_audio.buf512[(reqIdx & 1) << 8]);
		g_audio.flags |= 4u;
		__asm__ volatile("sync");
		if (ret < 0) {
			pspSetK1(oldK1);
			return ret;
		}
	}

	if (g_audio.unkCodecArgSet != 0)
		g_audio.unkCodecRet = (s8)(ret & 1);
	if (ret >= 0)
		ret = sampleCount;

	pspSetK1(oldK1);
	return ret;
}

/* 0x25A0 */
static int kermitAudioOutInit(void)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelMemset((void *)KERMIT_AUDIOOUT_BUF_ADDR, 0, KERMIT_AUDIO_BUF_SIZE);
	g_kermitAudioOutBuf = KERMIT_AUDIOOUT_BUF_ADDR;
	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);

	SceUID mtx = sceKernelCreateMutex("SceKermitAudioOut", SCE_KERNEL_SA_THPRI, 0, NULL);
	g_kermitAudioOutMutex = mtx;
	if (mtx <= 0)
		return 1;

	if (sceKermit_driver_5280B410(1, kermitAudioOutIntrHandler) != 0)
		return 1;
	if (sceKermit_driver_5280B410(2, kermitAudioOutSRCIntrHandler) != 0)
		return 1;

	if (sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_INIT, 0, 0, &resp) != 0)
		return 1;
	if ((u32)resp != 0)
		return 1;

	if (sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_GET_PARAM, 0, 0, &resp) != 0)
		return 0;

	if ((u32)resp == 1) {
		u32 val = (u32)(resp >> 32);
		if (val == 0)
			sceAudio_driver_95AE5A2B(9, 0);
	}
	return 0;
}

/* 0x270C */
static int kermitAudioOutPanned(void *buf)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);
	if (g_kermitAudioMuted != 0)
		return 0;

	sceKermitMemory_driver_AAF047AC(req, 0, buf, 0x80, 1);
	int ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_PANNED_OUT, 1, 0, &resp);
	if (ret == 0)
		ret = (int)resp;
	return ret;
}

/* 0x27C0 */
static int kermitAudioOutSRC(void *buf, u32 sampleCount, int vol, u32 freq)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);
	if (g_kermitAudioMuted != 0)
		return 0;

	sceKermitMemory_driver_AAF047AC(req, 0, buf, (sampleCount << 2), 1);
	req->args[1] = (u32)sampleCount;
	req->args[2] = (u32)vol;
	req->args[3] = (u32)freq;

	int ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_SRC_OUT, 4, 0, &resp);
	if (ret == 0)
		ret = (int)resp;
	return ret;
}

/* 0x28A4 */
static int kermitAudioOutSuspend(void)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	g_kermitAudioSuspended = 1;
	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);

	int ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_SUSPEND, 0, 0, &resp);
	return (ret != 0) ? ret : (int)resp;
}

/* 0x2924 */
static int kermitAudioOutPreSuspend(void)
{
	g_kermitAudioMuted = 1;
	return 0;
}

/* 0x2938 */
static int kermitAudioOutResume(void)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);

	int ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOOUT, KERMIT_AUDIOOUT_RESUME, 0, 0, &resp);
	if (ret == 0) {
		g_kermitAudioSuspended = 0;
		ret = (int)resp;
	}
	return ret;
}

/* 0x29B0 */
static int kermitAudioOutPostResume(void)
{
	g_kermitAudioMuted = 0;
	return 0;
}

/* 0x29C0 */
static int kermitAudioOutIsSuspended(void)
{
	return g_kermitAudioSuspended;
}

/* 0x29CC */
static int kermitAudioOutIntrHandler(void)
{
	return sceKernelSetEventFlag(g_audio.evFlagId, 0x20000000u);
}

/* 0x29F0 */
static int kermitAudioOutSRCIntrHandler(void)
{
	g_audio.srcQueuedCnt--;
	return sceKernelSetEventFlag(g_audio.evFlagId, 0x40000000u);
}

/* 0x2A24 */
static int kermitAudioInInit(void)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);
	sceKernelMemset((void *)KERMIT_AUDIOIN_BUF_ADDR, 0, KERMIT_AUDIO_BUF_SIZE);
	g_kermitAudioInBuf = KERMIT_AUDIOIN_BUF_ADDR;

	SceUID mtx = sceKernelCreateMutex("SceKermitAudioIn", SCE_KERNEL_SA_THPRI, 0, NULL);
	g_kermitAudioInMutex = mtx;
	if (mtx <= 0)
		return 1;

	if (sceKermit_driver_5280B410(3, kermitAudioInIntrHandler) != 0)
		return 1;

	if (sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOIN, KERMIT_AUDIOIN_INIT, 0, 0, &resp) != 0)
		return 1;

	return ((u32)resp != 0) ? 1 : 0;
}

/* 0x2B0C */
static int kermitAudioInTransfer(void *buf)
{
	u8 req_buf[128];
	u64 resp = 0;
	SceKermitRequest *req = (SceKermitRequest *)(((u32)req_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(req, 0x40);
	req = KERMIT_UNCACHED(req);

	int ret;
	if (buf != NULL) {
		sceKermitMemory_driver_AAF047AC(req, 0, buf, 0x80, 2);
		ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOIN, KERMIT_AUDIOIN_INPUT, 1, 0, &resp);
	} else {
		ret = sceKermit_driver_36666181(req, KERMIT_MODE_AUDIOIN, KERMIT_AUDIOIN_STOP, 0, 0, &resp);
	}

	if (ret == 0)
		ret = (int)resp;
	return ret;
}

/* 0x2BB8 */
static int kermitAudioInIntrHandler(void)
{
	return sceKernelSetEventFlag(g_audio.evFlagId, 0x80000000u);
}
