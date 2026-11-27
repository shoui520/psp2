/*
 * sas2_core.c: PSP SAS2 32-voice software audio synthesizer and reverb DSP
 * (ADSR, VAG ADPCM, LFSR noise, triangle/square, PCM, ATRAC3, 9-preset reverb)
 * (0x810185ac..0x81019c6f).
 */
#include "host_common.h"

#include <string.h>

#define SAS2_NUM_VOICES         32
#define SAS2_BLOCK_SAMPLES      32
#define SAS2_ENV_MAX            0x40000000
#define SAS2_ENV_MIN_CUTOFF     0x4000
#define SAS2_SAMPLE_RATE        44100

typedef struct Sas2VoiceState {
	union {
		struct {
			uint16_t flagsOrFrameSize;
			int16_t vagSamples[29];
		} vag;
		struct {
			uint16_t frameBytes;
			uint16_t pad;
			uint8_t *pcmBuf;
			void *workCtx;
			int16_t skipSamples;
		} at3;
	} u;
	uint32_t curAddr;
	uint32_t endAddr;
	uint32_t loopAddr;
	int32_t phase;
} Sas2VoiceState;

_Static_assert(offsetof(Sas2VoiceState, curAddr) == 0x3C, "Sas2VoiceState curAddr");
_Static_assert(offsetof(Sas2VoiceState, phase) == 0x48, "Sas2VoiceState phase");
_Static_assert(sizeof(Sas2VoiceState) == 0x4C, "Sas2VoiceState size");

typedef struct Sas2Voice {
	uint32_t startAddr;
	uint32_t endOrClock;
	uint8_t voiceType;
	uint8_t loopMode;
	uint16_t pitch;
	int16_t dryLeft;
	int16_t dryRight;
	int16_t wetLeft;
	int16_t wetRight;
	Sas2VoiceState *state;
	int32_t adsrRates[4];
	int32_t sustainLevel;
	uint8_t adsrCurves[4];
	uint8_t padA;
	uint8_t padB;
	uint8_t padC;
	int8_t adsrStage;
	int32_t envHeight;
} Sas2Voice;

_Static_assert(offsetof(Sas2Voice, state) == 0x14, "Sas2Voice state");
_Static_assert(offsetof(Sas2Voice, adsrStage) == 0x33, "Sas2Voice adsrStage");
_Static_assert(offsetof(Sas2Voice, envHeight) == 0x34, "Sas2Voice envHeight");
_Static_assert(sizeof(Sas2Voice) == 0x38, "Sas2Voice size");

typedef struct Sas2ReverbState {
	int32_t prevOutL;
	int32_t prevOutR;
	uint16_t wetVolL;
	uint16_t wetTargetL;
	int16_t wetDeltaL;
	uint16_t wetStepsL;
	uint16_t wetVolR;
	uint16_t wetTargetR;
	int16_t wetDeltaR;
	uint16_t wetStepsR;
	uint16_t dryGain;
	uint16_t dryTarget;
	int16_t dryDelta;
	uint16_t drySteps;
	uint16_t wetGain;
	uint16_t wetTarget;
	int16_t wetDelta;
	uint16_t wetSteps;
	int16_t *apfBufL;
	int16_t *apfBufR;
	int16_t *apfBufOut;
	uint16_t combIdx;
	uint16_t combMask;
	uint16_t apfIdxL;
	uint16_t apfIdxR;
	uint16_t apfIdxOut;
	uint16_t combMaxDelay[4];
	uint16_t apfLenL;
	uint16_t apfLenR;
	uint16_t apfLenOut;
	int16_t apfCoeffA;
	int16_t iirAlpha;
	int16_t combGain[4];
	int16_t wallFeedback;
	uint16_t tapDelay[12];
	int16_t combBuffer[1];
} Sas2ReverbState;

_Static_assert(offsetof(Sas2ReverbState, apfBufL) == 0x28, "Sas2ReverbState apfBufL");
_Static_assert(offsetof(Sas2ReverbState, combIdx) == 0x34, "Sas2ReverbState combIdx");
_Static_assert(offsetof(Sas2ReverbState, combMaxDelay) == 0x3E, "Sas2ReverbState combMaxDelay");
_Static_assert(offsetof(Sas2ReverbState, apfLenL) == 0x46, "Sas2ReverbState apfLenL");
_Static_assert(offsetof(Sas2ReverbState, apfLenOut) == 0x4A, "Sas2ReverbState apfLenOut");
_Static_assert(offsetof(Sas2ReverbState, apfCoeffA) == 0x4C, "Sas2ReverbState apfCoeffA");
_Static_assert(offsetof(Sas2ReverbState, iirAlpha) == 0x4E, "Sas2ReverbState iirAlpha");
_Static_assert(offsetof(Sas2ReverbState, tapDelay) == 0x5A, "Sas2ReverbState tapDelay");
_Static_assert(offsetof(Sas2ReverbState, combBuffer) == 0x72, "Sas2ReverbState combBuffer");

typedef struct Sas2Context {
	Sas2ReverbState *reverb;
	uint32_t reservedHeader;
	uint8_t grainBlocks;
	uint8_t outputMode;
	uint8_t reservedArea[1802];
	Sas2Voice voices[SAS2_NUM_VOICES];
	uint32_t endFlags;
	uint8_t dryWetEnable;
	uint8_t padTail;
	uint16_t reverbVolL;
	uint16_t reverbVolR;
} Sas2Context;

_Static_assert(offsetof(Sas2Context, voices) == 1812, "Sas2Context voices");
_Static_assert(offsetof(Sas2Context, endFlags) == 3604, "Sas2Context endFlags");
_Static_assert(offsetof(Sas2Context, reverbVolL) == 3610, "Sas2Context reverbVolL");

typedef struct Sas2CompatVoiceTracker {
	int32_t muteCountdown;
	int32_t matchState;
} Sas2CompatVoiceTracker;

static const int16_t g_sas2CompatVagSignature[28] = {
	0, 0, 1024, 2976, 4969, 6938, 8311, 9338,
	9497, 9093, 8430, 7289, 6247, 5330, 4559, 4205,
	4253, 4421, 4696, 5063, 5504, 5489, 5303, 4706,
	3487, 1979, -9, -2129
};

static const uint8_t g_sas2VagPredictCoeff[10] = {
	0, 60, 115, 98, 122,
	0, 0, 52, 55, 60
};

static const int16_t g_sas2ReverbPresets[9][30] = {
	{ 512, 125, 91, 1752, 2711, -2086, 0, 0, 17792, 22528, 21248, 1238, 819, 1008, 551, 884, 495, 822, 439, 821, 438, 820, 437, 820, 437, 820, 437, 436, 310, 184 },
	{ 256, 51, 37, 1807, 2549, -2148, 2178, -2018, 25600, 21120, 20160, 996, 795, 932, 687, 882, 614, 796, 605, 604, 398, 559, 309, 466, 183, 399, 181, 180, 128, 76 },
	{ 512, 177, 127, 1807, 2549, -2148, 2210, -2082, 19264, 21120, 20160, 2308, 1899, 2084, 1631, 1954, 1558, 1900, 1517, 1516, 1070, 1295, 773, 1122, 695, 1071, 613, 612, 434, 256 },
	{ 1024, 227, 169, 1782, 2549, -2148, 2210, -2082, 22912, 22144, 21184, 3579, 2904, 3337, 2620, 3033, 2419, 2905, 2266, 2265, 1513, 2028, 1200, 1775, 978, 1514, 797, 796, 568, 340 },
	{ 1024, 421, 313, 1536, 2560, 2432, -2304, -2176, 16384, 24576, 23552, 5562, 4539, 5314, 4285, 4540, 3521, 4544, 3523, 3520, 2497, 3012, 1985, 2560, 1741, 2498, 1473, 1472, 1050, 628 },
	{ 2048, 829, 561, 2016, 2560, -2432, -2560, 2432, 20480, 24576, 21504, 7894, 6705, 7444, 6203, 7106, 5810, 6706, 5615, 5614, 4181, 4916, 3885, 4598, 3165, 4182, 2785, 2784, 1954, 1124 },
	{ 4096, 3, 3, 2048, 4096, 0, 0, 0, 32512, 0, 0, 8189, 4093, 4105, 9, 0, 0, 4105, 9, 8191, 8191, 8190, 8190, 8190, 8190, 8190, 8190, 4104, 4100, 8 },
	{ 4096, 3, 3, 2048, 4096, 0, 0, 0, 0, 0, 0, 8189, 4093, 4105, 9, 0, 0, 4105, 9, 8191, 8191, 8190, 8190, 8190, 8190, 8190, 8190, 4104, 4100, 8 },
	{ 256, 23, 19, 1807, 2549, -2148, 2210, -2082, 31488, 24448, 21696, 881, 687, 741, 479, 688, 471, 856, 618, 470, 286, 301, 177, 287, 89, 416, 227, 88, 64, 40 }
};

static int g_sas2CompatCurrentVoice;
static int g_sas2CompatOwnerVoice;
static int g_sas2CompatSilenceState;
static Sas2CompatVoiceTracker g_sas2CompatVoices[SAS2_NUM_VOICES];
static int g_sas2CompatVagMode;

static inline int16_t sas2Clamp16(int32_t sample)
{
	if (sample >= 32768)
		return 32767;
	if (sample < -32768)
		return -32768;
	return (int16_t)sample;
}

static inline uint32_t sas2Ror32(uint32_t value, unsigned int shift)
{
	return (value >> shift) | (value << (32U - shift));
}

static int sas2AccumulateAndStepAdsr(int32_t *mixQuad, int32_t sample, Sas2Voice *voice)
{
	int32_t envQ18 = voice->envHeight >> 12;
	uint8_t stage = (uint8_t)voice->adsrStage;
	uint8_t curve = voice->adsrCurves[stage];
	uint32_t envVal = (uint32_t)voice->envHeight;
	int32_t rate = voice->adsrRates[stage];
	int32_t nextEnv;

	mixQuad[0] += (sample * ((envQ18 * voice->dryLeft) >> 16)) >> 5;
	mixQuad[1] += (sample * ((envQ18 * voice->dryRight) >> 16)) >> 5;
	mixQuad[2] += (sample * ((envQ18 * voice->wetLeft) >> 16)) >> 5;
	mixQuad[3] += (sample * ((envQ18 * voice->wetRight) >> 16)) >> 5;

	switch (curve) {
	case 0:
		nextEnv = (int32_t)envVal + rate;
		break;
	case 1:
		nextEnv = (int32_t)envVal - rate;
		break;
	case 2:
		if (envVal >= 0x30000000U)
			rate = (int32_t)((uint32_t)rate >> 2);
		nextEnv = (int32_t)envVal + rate;
		break;
	case 3:
		if (rate != 0)
			nextEnv = (int32_t)(((uint64_t)(uint32_t)(-rate) * (uint64_t)envVal) >> 32);
		else
			nextEnv = (int32_t)envVal + rate;
		break;
	case 4:
		nextEnv = (int32_t)((((uint64_t)(uint32_t)rate * (uint64_t)(0x40000000U - envVal)) >> 32) +
		                    0x4000U + envVal);
		break;
	case 5:
		nextEnv = rate;
		break;
	default:
		nextEnv = (int32_t)((envVal >> 1) + (envVal >> 3));
		break;
	}

	if (stage == 1U) {
		if (voice->sustainLevel >= nextEnv) {
			if (nextEnv < 0)
				nextEnv = 0;
			voice->adsrStage = 2;
		}
		voice->envHeight = nextEnv;
		return 1;
	}

	if (nextEnv >= SAS2_ENV_MAX)
		nextEnv = SAS2_ENV_MAX;

	if (stage == 0U) {
		voice->envHeight = nextEnv;
		voice->adsrStage = (nextEnv == SAS2_ENV_MAX) ? 1 : 0;
		return 1;
	}

	if ((curve & 1U) != 0U && nextEnv < SAS2_ENV_MIN_CUTOFF)
		return -1;

	voice->envHeight = nextEnv;
	return 1;
}

static uint16_t *sas2ApplySilenceCompat(uint16_t *buf, size_t bytes)
{
	if (g_sas2CompatSilenceState == 2) {
		if (buf[0] == 0xFFFCU && buf[1] == 0xFFFCU && buf[2] == 0xFFFCU &&
		    buf[3] == 0xFFFCU && buf[4] == 0xFFFCU && buf[5] == 0xFFFCU &&
		    buf[6] == 0xFFFCU && buf[7] == 0xFFFCU && buf[8] == 0xFFFCU) {
			g_sas2CompatSilenceState = 3;
			return (uint16_t *)memset(buf, 0, bytes);
		}
		return buf;
	}
	if (g_sas2CompatSilenceState == 3)
		return (uint16_t *)memset(buf, 0, bytes);
	return buf;
}

static int __attribute__((noinline, noclone)) sas2RenderVagVoice(int32_t *mixBuf, Sas2Voice *voice)
{
	Sas2VoiceState *st = voice->state;
	uint32_t pitch = voice->pitch;
	int32_t phase = st->phase;
	uint32_t curAddr = st->curAddr;
	int32_t *mixCur = mixBuf;
	int32_t *mixEnd = mixBuf + SAS2_BLOCK_SAMPLES * 4;

	for (;;) {
		int32_t sample;
		int32_t sampleIdx;
		int32_t frac;
		const int16_t *pcmPtr;

		while (phase >= 0) {
			const uint8_t *vagBlock;
			uint8_t flags;
			uint32_t nextAddr;
			uint8_t predictShift;
			unsigned int shift;
			unsigned int predict;
			int32_t coeffA;
			int32_t coeffB;
			int32_t prevPrev;
			int32_t prev;
			int byteIdx;

			if (curAddr == 0U || curAddr >= voice->state->endAddr)
				return -1;

			vagBlock = (const uint8_t *)remotefsTranslateUserGuestAddr(curAddr, 1, 16U);
			flags = vagBlock[1];
			nextAddr = curAddr + 16U;
			if (flags == 7U)
				return -1;

			st = voice->state;
			if (st->u.vag.flagsOrFrameSize != 0U) {
				if (flags == 6U)
					st->loopAddr = curAddr;
				else if (flags == 3U)
					nextAddr = st->loopAddr;
			}

			predictShift = vagBlock[0];
			shift = (unsigned int)(predictShift & 0x0FU) + 10U;
			predict = (unsigned int)(predictShift >> 4);
			coeffA = g_sas2VagPredictCoeff[predict];
			coeffB = g_sas2VagPredictCoeff[predict + 5U];
			prevPrev = st->u.vag.vagSamples[27];
			prev = st->u.vag.vagSamples[28];
			st->u.vag.vagSamples[0] = (int16_t)prev;

			for (byteIdx = 0; byteIdx < 14; byteIdx++) {
				uint32_t packed = sas2Ror32((uint32_t)vagBlock[byteIdx + 2], 4U);
				int32_t nibLo = (int32_t)packed >> shift;
				int32_t nibHi = (int32_t)(packed << 28) >> shift;

				prevPrev = (prev * coeffA - prevPrev * coeffB + nibLo) >> 6;
				prevPrev = sas2Clamp16(prevPrev);
				st->u.vag.vagSamples[byteIdx * 2 + 1] = (int16_t)prevPrev;

				prev = (prevPrev * coeffA - prev * coeffB + nibHi) >> 6;
				prev = sas2Clamp16(prev);
				st->u.vag.vagSamples[byteIdx * 2 + 2] = (int16_t)prev;
			}

			phase -= 28 << 12;
			curAddr = nextAddr;
		}

		st = voice->state;
		frac = phase & 0xFFF;
		sampleIdx = (phase >> 12) + 28;
		phase += (int32_t)pitch;
		pcmPtr = &st->u.vag.vagSamples[sampleIdx];
		sample = pcmPtr[0] - ((frac * (pcmPtr[0] - pcmPtr[1])) >> 12);

		if (g_sas2CompatVagMode != 0) {
			int curVoice = g_sas2CompatCurrentVoice;
			int32_t matchSt = g_sas2CompatVoices[curVoice].matchState;

			if (matchSt == 0) {
				if (g_sas2CompatVagMode == 1 &&
				    memcmp(pcmPtr, g_sas2CompatVagSignature, sizeof(g_sas2CompatVagSignature)) == 0) {
					g_sas2CompatVoices[curVoice].matchState = 1;
					matchSt = 1;
				} else {
					g_sas2CompatVoices[curVoice].matchState = -1;
				}
			}
			if (matchSt == 1) {
				if (g_sas2CompatVoices[curVoice].muteCountdown > 0) {
					g_sas2CompatVoices[curVoice].muteCountdown--;
					voice->dryLeft = 0;
					voice->dryRight = 0;
				} else {
					voice->dryLeft = 4096;
					voice->dryRight = 4079;
				}
			}
		}

		if (sas2AccumulateAndStepAdsr(mixCur, sample, voice) < 0)
			return -1;

		mixCur += 4;
		if (mixCur == mixEnd) {
			voice->state->curAddr = curAddr;
			voice->state->phase = phase;
			return 1;
		}
	}
}

static void __attribute__((noinline, noclone)) sas2ConfigureReverb(Sas2Context *ctx, uint32_t revFlags);
static int16_t *__attribute__((noinline, noclone)) sas2ProcessReverb(Sas2Context *ctx, int32_t *mixBuf, int16_t *outStereo);

static int16_t *sas2RenderBlock(int16_t *outBuf, Sas2Context *ctx, int allowPlanar)
{
	int32_t mixBuf[SAS2_BLOCK_SAMPLES * 4];
	int voiceIdx;

	memset(mixBuf, 0, sizeof(mixBuf));

	for (voiceIdx = SAS2_NUM_VOICES - 1; voiceIdx >= 0; voiceIdx--) {
		Sas2Voice *voice = &ctx->voices[SAS2_NUM_VOICES - 1 - voiceIdx];
		uint32_t pitch = voice->pitch;
		int8_t stage = voice->adsrStage;
		uint8_t vtype;
		int voiceDone = 0;

		if (g_sas2CompatVagMode != 0)
			g_sas2CompatCurrentVoice = voiceIdx;

		if (stage < 0) {
			vtype = voice->voiceType;
			voiceDone = 1;
		} else if (pitch == 0U) {
			continue;
		} else {
			vtype = voice->voiceType;
			switch (vtype) {
			case 17:
				voice->adsrStage = 4;
				voice->pitch = 0U;
				/* fallthrough */
			case 1:
				if (g_sas2CompatVagMode != 0 &&
				    g_sas2CompatVoices[g_sas2CompatCurrentVoice].matchState == 1 &&
				    g_sas2CompatOwnerVoice != voiceIdx) {
					voice->envHeight = 0;
					ctx->endFlags = (uint32_t)stage | (0x80000000U >> voiceIdx);
					voice->adsrStage = -1;
					continue;
				}
				if (sas2RenderVagVoice(mixBuf, voice) < 0)
					voiceDone = 1;
				break;

			case 18:
				voice->adsrStage = 4;
				/* fallthrough */
			case 2: {
				Sas2VoiceState *st = voice->state;
				uint32_t clockBits = st->curAddr;
				int32_t lfsr = st->phase;
				int32_t countdown = (int32_t)st->u.vag.vagSamples[27] |
				                    ((int32_t)st->u.vag.vagSamples[28] << 16);
				int32_t *cur = mixBuf;

				for (;;) {
					if (sas2AccumulateAndStepAdsr(cur, (int16_t)lfsr, voice) < 0) {
						voiceDone = 1;
						break;
					}
					countdown--;
					if (countdown < 0) {
						clockBits = sas2Ror32(clockBits, 1U);
						countdown = (int32_t)voice->endOrClock;
						if ((int32_t)clockBits < 0) {
							uint32_t shifted = (uint32_t)(lfsr << 1);
							uint32_t fb = ((105U >> ((shifted >> 10) & 7U)) ^ (shifted >> 15)) & 1U;
							lfsr = (int32_t)(shifted + fb);
						}
					}
					cur += 4;
					if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
						st->curAddr = clockBits;
						st->phase = lfsr;
						st->u.vag.vagSamples[27] = (int16_t)(countdown & 0xFFFF);
						st->u.vag.vagSamples[28] = (int16_t)(countdown >> 16);
						break;
					}
				}
				break;
			}

			case 19:
				voice->adsrStage = 4;
				/* fallthrough */
			case 3: {
				int32_t *cur = mixBuf;
				int32_t ph = voice->state->phase;

				for (;;) {
					uint32_t duty = voice->startAddr;
					int32_t halfDuty = (int32_t)(duty >> 1);
					int16_t triSample;

					if ((int32_t)(SAS2_SAMPLE_RATE - (duty >> 1)) <= ph)
						ph -= SAS2_SAMPLE_RATE;
					if (halfDuty <= ph) {
						triSample = (int16_t)(0x4000 - (int32_t)(((uint32_t)(ph - halfDuty) << 15) /
						                                         (SAS2_SAMPLE_RATE - duty)));
					} else {
						triSample = (int16_t)(((uint32_t)ph << 15) / duty);
					}
					if (sas2AccumulateAndStepAdsr(cur, triSample, voice) < 0) {
						voiceDone = 1;
						break;
					}
					cur += 4;
					ph += (int32_t)voice->pitch;
					if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
						voice->state->phase = ph;
						break;
					}
				}
				break;
			}

			case 20:
				voice->adsrStage = 4;
				/* fallthrough */
			case 4: {
				int32_t *cur = mixBuf;
				uint32_t ph = (uint32_t)voice->state->phase;

				for (;;) {
					int32_t sqSample = (voice->startAddr > ph) ? 0x2000 : -0x2000;

					if (sas2AccumulateAndStepAdsr(cur, sqSample, voice) < 0) {
						voiceDone = 1;
						break;
					}
					cur += 4;
					ph += (uint32_t)voice->pitch;
					if (ph >= (uint32_t)SAS2_SAMPLE_RATE)
						ph -= (uint32_t)SAS2_SAMPLE_RATE;
					if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
						voice->state->phase = (int32_t)ph;
						break;
					}
				}
				break;
			}

			case 21:
				pitch = 0U;
				voice->pitch = 0U;
				voice->adsrStage = 4;
				/* fallthrough */
			case 5: {
				Sas2VoiceState *st = voice->state;
				const int16_t *pcmPtr = (const int16_t *)(uintptr_t)st->curAddr;
				int32_t ph = st->phase;
				const int16_t *pcmEnd = (const int16_t *)(uintptr_t)st->endAddr;
				int32_t *cur = mixBuf;

				remotefsTranslateUserGuestAddr(voice->startAddr, 1,
				                               2U * (uint32_t)(voice->endOrClock & 0xFFFFU));
				if (pcmPtr == NULL) {
					voiceDone = 1;
					break;
				}

				for (;;) {
					int32_t curSample = pcmPtr[0];
					int32_t nextSample;
					int32_t interp;

					if (pcmPtr == pcmEnd) {
						const int16_t *loopPtr = (const int16_t *)(uintptr_t)voice->state->loopAddr;
						nextSample = (loopPtr != NULL) ? loopPtr[0] : 0;
					} else {
						nextSample = pcmPtr[1];
					}

					while (ph >= 0) {
						ph -= 4096;
						if (pcmPtr == pcmEnd) {
							pcmPtr = (const int16_t *)(uintptr_t)voice->state->loopAddr;
							if (pcmPtr == NULL) {
								voiceDone = 1;
								break;
							}
						} else {
							pcmPtr++;
						}
					}
					if (voiceDone)
						break;

					interp = curSample - (((curSample - nextSample) * (ph & 0xFFF)) >> 12);
					ph += (int32_t)pitch;
					if (sas2AccumulateAndStepAdsr(cur, interp, voice) < 0) {
						voiceDone = 1;
						break;
					}
					cur += 4;
					if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
						voice->state->curAddr = (uint32_t)(uintptr_t)pcmPtr;
						voice->state->phase = ph;
						break;
					}
					if (pcmPtr == NULL) {
						voiceDone = 1;
						break;
					}
				}
				break;
			}

			case 22:
				pitch = 0U;
				voice->pitch = 0U;
				voice->adsrStage = 4;
				/* fallthrough */
			case 6: {
				Sas2VoiceState *st = voice->state;
				int32_t ph = st->phase;
				uint8_t *pcmBuf = st->u.at3.pcmBuf;
				uint8_t *readPtr = (uint8_t *)(uintptr_t)((uint32_t)(uint16_t)st->u.vag.vagSamples[27] |
				                                          ((uint32_t)(uint16_t)st->u.vag.vagSamples[28] << 16));
				uint32_t *hdr = (uint32_t *)remotefsTranslateUserGuestAddr(voice->startAddr, 1, 256U);
				uint8_t *nextBufHost = NULL;
				int32_t *cur = mixBuf;
				int at3Ended = 0;

				if (hdr[43] != 0U) {
					if (hdr[46] != 0U)
						remotefsTranslateUserGuestAddr(hdr[46], 1, hdr[43]);
					if ((hdr[47] - 1U) <= 0xFFFFFFFDU)
						nextBufHost = (uint8_t *)remotefsTranslateUserGuestAddr(hdr[47], 1, hdr[43]);
				}

				for (;;) {
					while (ph < 0) {
						int32_t idx = ph >> 12;
						int32_t frac = ph & 0xFFF;
						const int16_t *pcmSamples = (const int16_t *)(pcmBuf + 2048);
						int32_t sample;

						ph += (int32_t)pitch;
						sample = pcmSamples[idx] -
						         ((frac * (pcmSamples[idx] - pcmSamples[idx + 1])) >> 12);
						if (sas2AccumulateAndStepAdsr(cur, sample, voice) < 0) {
							voiceDone = 1;
							break;
						}
						cur += 4;
						if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
							uint32_t ptrVal = (uint32_t)(uintptr_t)readPtr;
							voice->state->u.vag.vagSamples[27] = (int16_t)(ptrVal & 0xFFFFU);
							voice->state->u.vag.vagSamples[28] = (int16_t)(ptrVal >> 16);
							voice->state->phase = ph;
							at3Ended = -1;
							break;
						}
					}
					if (voiceDone || at3Ended < 0)
						break;

					{
						uint32_t frameBytes = voice->state->u.at3.frameBytes;
						uint8_t *nextRead = readPtr + frameBytes;
						uint32_t endPtr = voice->state->curAddr;

						if ((uintptr_t)nextRead > endPtr) {
							int32_t nextGuest = (int32_t)hdr[47];

							if (nextGuest == 0) {
								at3Ended = 1;
								break;
							}
							if (nextGuest < 0) {
								if (sas2AccumulateAndStepAdsr(cur, 0, voice) < 0) {
									voiceDone = 1;
									break;
								}
								cur += 4;
								if (cur == mixBuf + SAS2_BLOCK_SAMPLES * 4) {
									uint32_t ptrVal = (uint32_t)(uintptr_t)readPtr;
									voice->state->u.vag.vagSamples[27] = (int16_t)(ptrVal & 0xFFFFU);
									voice->state->u.vag.vagSamples[28] = (int16_t)(ptrVal >> 16);
									voice->state->phase = ph;
									at3Ended = -1;
									break;
								}
								continue;
							}
							{
								uint32_t rem = endPtr - (uint32_t)(uintptr_t)readPtr;
								if (rem > hdr[49]) {
									at3Ended = 1;
									break;
								}
								memcpy(pcmBuf, readPtr, rem);
								memcpy(pcmBuf + rem, nextBufHost, frameBytes - rem);
								readPtr = pcmBuf;
								nextRead = nextBufHost + (frameBytes - rem);
								voice->state->curAddr =
									(uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(hdr[47], 2, 0U) +
									hdr[49];
								hdr[47] = 0xFFFFFFFFU;
								remotefsFlushGuestAddr(&hdr[47], 4U);
							}
						}

						{
							uint32_t savedTailWord;
							uint32_t consumedBytes = 0U;
							uint32_t pcmBytesOut = 0U;

							memcpy(&savedTailWord, pcmBuf + 2048, sizeof(savedTailWord));
							if (meAt3DecodeInternal(readPtr, (int16_t *)(pcmBuf + 4), &consumedBytes,
							                        &pcmBytesOut, voice->state->u.at3.workCtx, 1) != 0) {
								at3Ended = 1;
								break;
							}
							{
								int32_t remSkip = voice->state->u.at3.skipSamples - 1024;
								if (remSkip <= 0) {
									voice->state->u.at3.skipSamples = 0;
									ph += remSkip << 12;
									readPtr = nextRead;
									memcpy(pcmBuf, &savedTailWord, sizeof(savedTailWord));
								} else {
									voice->state->u.at3.skipSamples = (int16_t)remSkip;
								}
							}
						}
					}
				}
				if (at3Ended == 1) {
					voice->voiceType = 0U;
					vtype = 0U;
					voiceDone = 1;
				}
				break;
			}

			default:
				voiceDone = 1;
				break;
			}
		}

		if (voiceDone && (vtype & 0x30U) == 0U) {
			int8_t curStage = voice->adsrStage;
			uint32_t bit = 0x80000000U >> voiceIdx;

			voice->envHeight = 0;
			if (curStage == 5 || curStage == -2) {
				Sas2VoiceState *st = voice->state;

				ctx->endFlags &= ~bit;
				voice->adsrStage = 0;
				st->phase = 0;
				st->u.vag.vagSamples[27] = 0;
				st->u.vag.vagSamples[28] = 0;
				st->u.vag.flagsOrFrameSize = voice->loopMode;
				st->curAddr = voice->startAddr;
				st->loopAddr = voice->startAddr;
				st->endAddr = voice->endOrClock;

				if (g_sas2CompatVagMode != 0) {
					g_sas2CompatOwnerVoice = voiceIdx;
					g_sas2CompatVoices[g_sas2CompatCurrentVoice].muteCountdown = 2020;
					g_sas2CompatVoices[g_sas2CompatCurrentVoice].matchState = 0;
				}

				if (g_sas2CompatSilenceState != 0) {
					g_sas2CompatSilenceState = 1;
					if (voice->endOrClock - voice->startAddr == 354608U)
						g_sas2CompatSilenceState = 2;
				}

				if (voice->voiceType == 5U) {
					uint32_t hostStart = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr(st->curAddr, 2, 0U);
					st->curAddr = hostStart;
					st->endAddr = hostStart + 2U * (voice->endOrClock & 0xFFFFU);
					if (voice->loopMode != 0U)
						st->loopAddr = hostStart + 2U * (uint32_t)voice->pitch;
					else
						st->loopAddr = 0U;
				} else if (voice->voiceType == 6U) {
					uint32_t *hdr = (uint32_t *)remotefsTranslateUserGuestAddr(voice->startAddr, 2, 0U);
					int32_t streamGuest = (int32_t)hdr[46];

					if (streamGuest > 0) {
						uint32_t streamHost = (uint32_t)(uintptr_t)remotefsTranslateUserGuestAddr((uint32_t)streamGuest, 2, 0U);
						void *workHost = NULL;
						uint8_t *workBytes;

						st->u.vag.vagSamples[27] = (int16_t)(streamHost & 0xFFFFU);
						st->u.vag.vagSamples[28] = (int16_t)(streamHost >> 16);
						st->curAddr = streamHost + hdr[43];
						st->u.at3.frameBytes = (uint16_t)hdr[38];
						sceJpegWorkGuestToHost(hdr[3], &workHost);
						workBytes = (uint8_t *)workHost;
						st->u.at3.workCtx = workHost;
						st->u.at3.pcmBuf = workBytes + 7100U;
						st->u.at3.skipSamples = (int16_t)hdr[36];
						if (hdr[47] != 0U) {
							hdr[46] = 0U;
							remotefsFlushGuestAddr(&hdr[46], 4U);
						}
					} else {
						voice->adsrStage = -1;
					}
				}
			} else {
				ctx->endFlags |= bit;
				voice->adsrStage = -1;
			}
		}
	}

	if (allowPlanar != 0 && ctx->outputMode != 0U) {
		uint32_t strideSamples = (uint32_t)ctx->grainBlocks << 5;
		int16_t *chDryL = outBuf;
		int16_t *chDryR = outBuf + strideSamples;
		int16_t *chWetL = outBuf + strideSamples * 2U;
		int16_t *chWetR = outBuf + strideSamples * 3U;
		int i;

		for (i = 0; i < SAS2_BLOCK_SAMPLES; i++) {
			chDryL[i] = sas2Clamp16(mixBuf[i * 4 + 0] >> 9);
			chDryR[i] = sas2Clamp16(mixBuf[i * 4 + 1] >> 9);
			chWetL[i] = sas2Clamp16(mixBuf[i * 4 + 2] >> 9);
			chWetR[i] = sas2Clamp16(mixBuf[i * 4 + 3] >> 9);
		}
		return outBuf + SAS2_BLOCK_SAMPLES;
	}

	return sas2ProcessReverb(ctx, mixBuf, outBuf);
}

int pspemuSas2CoreMix(void *ctxPtr, void *outBufPtr, uint32_t revFlags)
{
	Sas2Context *ctx = (Sas2Context *)ctxPtr;
	int16_t *outCur = (int16_t *)outBufPtr;
	int16_t *flushBase = outCur;
	int16_t *lastFlushed = outCur;
	uint32_t blocksLeft;

	if ((revFlags & 0xFF00U) != 0U)
		sas2ConfigureReverb(ctx, revFlags);

	blocksLeft = ctx->grainBlocks;
	do {
		lastFlushed = flushBase;
		outCur = sas2RenderBlock(outCur, ctx, 1);
		if ((blocksLeft & 3U) == 0U) {
			size_t chunkBytes = (size_t)((uint8_t *)outCur - (uint8_t *)flushBase);

			lastFlushed = outCur;
			if (g_sas2CompatSilenceState > 1)
				sas2ApplySilenceCompat((uint16_t *)flushBase, chunkBytes);
			remotefsFlushGuestAddr(flushBase, (uint32_t)chunkBytes);
			flushBase = outCur;
		}
		blocksLeft--;
	} while (blocksLeft != 0U);

	{
		size_t remBytes = (size_t)((uint8_t *)outCur - (uint8_t *)lastFlushed);

		if (g_sas2CompatSilenceState > 1)
			sas2ApplySilenceCompat((uint16_t *)flushBase, remBytes);
		remotefsFlushGuestAddr(flushBase, (uint32_t)remBytes);
	}
	return 0;
}

int pspemuSas2CoreMixWithVoice(void *ctxPtr, void *outBufPtr, uint32_t revFlags,
                               uint32_t gainL, uint32_t gainR)
{
	Sas2Context *ctx = (Sas2Context *)ctxPtr;
	int16_t *dstCur = (int16_t *)outBufPtr;
	int16_t *flushBase = dstCur;
	int16_t *lastFlushed = dstCur;
	int16_t tempStereo[SAS2_BLOCK_SAMPLES * 2];
	uint32_t blocksLeft;

	if ((revFlags & 0xFF00U) != 0U)
		sas2ConfigureReverb(ctx, revFlags);

	blocksLeft = ctx->grainBlocks;
	do {
		int16_t *endPtr = sas2RenderBlock(tempStereo, ctx, 0);
		int16_t *src = tempStereo;
		int16_t *dst = dstCur;

		while (src != endPtr) {
			int32_t mixL = (int32_t)src[0] + (((int32_t)gainL * (int32_t)dst[0]) >> 12);
			int32_t mixR = (int32_t)src[1] + (((int32_t)gainR * (int32_t)dst[1]) >> 12);

			dst[0] = sas2Clamp16(mixL);
			dst[1] = sas2Clamp16(mixR);
			src += 2;
			dst += 2;
		}

		dstCur = dst;
		if ((blocksLeft & 3U) == 0U) {
			size_t chunkBytes = (size_t)((uint8_t *)dstCur - (uint8_t *)flushBase);

			remotefsFlushGuestAddr(flushBase, (uint32_t)chunkBytes);
			flushBase = dstCur;
			lastFlushed = dstCur;
		} else {
			lastFlushed = flushBase;
		}
		blocksLeft--;
	} while (blocksLeft != 0U);

	remotefsFlushGuestAddr(flushBase,
	                       (uint32_t)((uint8_t *)dstCur - (uint8_t *)lastFlushed));
	return 0;
}

int pspemuSas2CoreInit(void *ctxPtr)
{
	int vagParam;
	int audioParam;

	(void)ctxPtr;
	vagParam = pspemuCompatGetParam(19);
	if (vagParam > 0)
		g_sas2CompatVagMode = vagParam;

	audioParam = pspemuCompatGetParam(12);
	if (audioParam <= 0)
		return 0;
	if ((audioParam & 0x800) != 0)
		g_sas2CompatSilenceState = 1;
	return audioParam & 0x10;
}

static void __attribute__((noinline, noclone)) sas2ConfigureReverb(Sas2Context *ctx, uint32_t revFlags)
{
	Sas2ReverbState *rev = ctx->reverb;
	int8_t revType = (int8_t)(revFlags & 0xFFU);

	if ((revFlags & 0x800U) != 0U) {
		uint16_t diff;

		rev->wetTargetL = ctx->reverbVolL;
		diff = (uint16_t)(rev->wetVolL - ctx->reverbVolL);
		rev->wetDeltaL = (int16_t)diff;
		rev->wetStepsL = (diff != 0U) ? 32U : 0U;

		rev->wetTargetR = ctx->reverbVolR;
		diff = (uint16_t)(rev->wetVolR - ctx->reverbVolR);
		rev->wetDeltaR = (int16_t)diff;
		rev->wetStepsR = (diff != 0U) ? 32U : 0U;

		{
			uint16_t dryTgt = (uint16_t)(32U * (ctx->dryWetEnable & 1U));
			diff = (uint16_t)(rev->dryGain - dryTgt);
			rev->dryTarget = dryTgt;
			rev->dryDelta = (int16_t)diff;
			rev->drySteps = (diff != 0U) ? 32U : 0U;
		}
		{
			uint16_t wetTgt = (uint16_t)(16U * (ctx->dryWetEnable & 2U));
			diff = (uint16_t)(rev->wetGain - wetTgt);
			rev->wetTarget = wetTgt;
			rev->wetDelta = (int16_t)diff;
			rev->wetSteps = (diff != 0U) ? 32U : 0U;
		}
	}

	if (revType == -1) {
		rev->apfBufL = NULL;
		return;
	}

	if ((revFlags & 0x300U) != 0U) {
		const int16_t *preset = g_sas2ReverbPresets[revType];
		int16_t combSize = preset[0];
		int16_t apfDelayA = preset[1];
		int16_t apfDelayB = preset[2];
		int16_t wallFb = preset[8];
		int16_t tapBaseA = preset[11];
		int16_t tapBaseB = preset[12];
		int16_t tapOffA = preset[13];
		int16_t tapOffB = preset[14];
		int16_t tapOffC = preset[15];
		int16_t tapOffD = preset[16];
		int16_t tapOffE = preset[17];
		int16_t tapOffF = preset[18];
		int16_t tapBaseC = preset[19];
		int16_t tapBaseD = preset[20];
		int16_t tapOffG = preset[21];
		int16_t tapOffH = preset[22];
		int16_t tapOffI = preset[23];
		int16_t tapOffJ = preset[24];
		int16_t tapOffK = preset[25];
		int16_t tapOffL = preset[26];
		uint16_t lenL = (uint16_t)preset[27];
		uint16_t lenR = (uint16_t)preset[28];
		uint16_t lenExtra = (uint16_t)preset[29];
		int i;

		if ((unsigned int)(revType - 6) <= 1U) {
			int32_t delayUnits = (int32_t)((revFlags >> 16) & 0xFFU) + 1;
			int16_t scale16 = (int16_t)(16 * delayUnits);

			tapBaseB = (int16_t)(scale16 - apfDelayB);
			lenL = (uint16_t)(scale16 + lenExtra);
			lenR = (uint16_t)(scale16 + (lenExtra >> 1));
			wallFb = (int16_t)(((revFlags >> 24) & 0xFFU) << 8);
			tapBaseA = (int16_t)(32 * delayUnits - apfDelayA);
			tapOffA = (int16_t)(scale16 + tapOffB);
			tapOffE = (int16_t)(scale16 + tapOffF);
		}

		rev->tapDelay[0] = (uint16_t)(tapBaseA - tapOffA);
		rev->tapDelay[1] = (uint16_t)(tapBaseC - tapOffG);
		rev->tapDelay[2] = (uint16_t)(tapBaseD - tapOffH);
		rev->tapDelay[3] = (uint16_t)(tapBaseB - tapOffB);
		rev->tapDelay[4] = (uint16_t)(tapBaseA - tapOffC);
		rev->tapDelay[5] = (uint16_t)(tapBaseC - tapOffI);
		rev->tapDelay[6] = (uint16_t)(tapBaseD - tapOffJ);
		rev->tapDelay[7] = (uint16_t)(tapBaseB - tapOffD);
		rev->tapDelay[8] = (uint16_t)(tapBaseA - tapOffE);
		rev->tapDelay[9] = (uint16_t)(tapBaseC - tapOffK);
		rev->tapDelay[10] = (uint16_t)(tapBaseD - tapOffL);
		rev->tapDelay[11] = (uint16_t)(tapBaseB - tapOffF);

		rev->iirAlpha = preset[3];
		rev->combGain[0] = preset[4];
		rev->combGain[1] = preset[5];
		rev->combGain[2] = preset[6];
		rev->combGain[3] = preset[7];
		rev->apfLenOut = (uint16_t)preset[9];
		rev->apfCoeffA = preset[10];
		rev->wallFeedback = wallFb;

		for (i = 0; i < 4; i++) {
			uint16_t dA = rev->tapDelay[i];
			uint16_t dB = rev->tapDelay[i + 4];
			uint16_t dC = rev->tapDelay[i + 8];
			uint16_t m = (dB < dA) ? dA : dB;

			if (m < dC)
				m = dC;
			rev->combMaxDelay[i] = m;
		}

		rev->apfLenL = lenL;
		rev->apfLenR = lenR;
		rev->apfLenOut = (uint16_t)(2 * apfDelayB);

		rev->combIdx = 0U;
		rev->combMask = (uint16_t)(combSize - 1);
		rev->apfIdxL = 0U;
		rev->apfIdxR = 0U;
		rev->apfIdxOut = 0U;

		rev->apfBufL = &rev->combBuffer[(uint32_t)combSize * 4U];
		rev->apfBufR = rev->apfBufL + lenL;
		rev->apfBufOut = rev->apfBufR + lenR;
	}

	if ((revFlags & 0x700U) != 0U) {
		int16_t *clearPtr = rev->combBuffer;
		int16_t *clearEnd = rev->apfBufOut + rev->apfLenOut;

		while (clearPtr < clearEnd)
			*clearPtr++ = 0;
		rev->prevOutL = 0;
		rev->prevOutR = 0;
	}
}

static int16_t *__attribute__((noinline, noclone)) sas2ProcessReverb(Sas2Context *ctx, int32_t *mixBuf, int16_t *outStereo)
{
	Sas2ReverbState *rev = ctx->reverb;
	int16_t *outEnd = outStereo + SAS2_BLOCK_SAMPLES * 2;
	int i;

	if (rev->apfBufL != NULL) {
		int32_t prevL = rev->prevOutL;
		int32_t prevR = rev->prevOutR;
		int16_t *apfL = rev->apfBufL;
		int16_t *apfR = rev->apfBufR;
		int16_t *apfOut = rev->apfBufOut;

		for (i = 0; i < SAS2_BLOCK_SAMPLES; i += 2) {
			int32_t *evenQuad = &mixBuf[i * 4];
			int32_t *oddQuad = &mixBuf[(i + 1) * 4];
			uint16_t wetGainVal;
			uint16_t wetVolLVal;
			uint16_t wetVolRVal;
			uint16_t dryGainVal;
			uint32_t cIdx;
			uint32_t cMask;
			int32_t revOutL;
			int32_t revOutR;

			if (rev->wetSteps != 0U) {
				uint16_t steps = rev->wetSteps;
				rev->wetSteps = (uint16_t)(steps - 2U);
				wetGainVal = (uint16_t)(rev->wetTarget + ((rev->wetDelta * (int32_t)(steps - 1U)) >> 5));
				rev->wetGain = wetGainVal;
			} else {
				wetGainVal = rev->wetGain;
			}

			cIdx = rev->combIdx;
			cMask = rev->combMask;
			rev->combIdx = (uint16_t)((cIdx + 1U) & cMask);

			{
				int32_t inL = (oddQuad[2] >> 5) * (int32_t)wetGainVal;
				int32_t fbVal = rev->wallFeedback;
				int32_t alpha = rev->iirAlpha;
				int32_t combA;
				int32_t combB;
				int32_t prevA;
				int32_t prevB;
				int32_t tapSum;
				int32_t apfIn;
				int32_t apfDelay;
				int32_t apfMid;

				combA = sas2Clamp16((inL - ((rev->combBuffer[((cIdx - rev->tapDelay[8]) & cMask) * 4 + 0] * fbVal) >> 4)) >> 11);
				prevA = rev->combBuffer[((cIdx - rev->combMaxDelay[0]) & cMask) * 4 + 0];
				rev->combBuffer[cIdx * 4 + 0] = (int16_t)(prevA + ((alpha * (combA - prevA)) >> 11));

				combB = sas2Clamp16((inL - ((rev->combBuffer[((cIdx - rev->tapDelay[9]) & cMask) * 4 + 1] * fbVal) >> 4)) >> 11);
				prevB = rev->combBuffer[((cIdx - rev->combMaxDelay[1]) & cMask) * 4 + 1];
				rev->combBuffer[cIdx * 4 + 1] = (int16_t)(prevB + ((alpha * (combB - prevB)) >> 11));

				tapSum = (rev->combBuffer[((cIdx - rev->tapDelay[0]) & cMask) * 4 + 0] * rev->combGain[0] +
				          rev->combBuffer[((cIdx - rev->tapDelay[4]) & cMask) * 4 + 0] * rev->combGain[1] +
				          rev->combBuffer[((cIdx - rev->tapDelay[1]) & cMask) * 4 + 1] * rev->combGain[2] +
				          rev->combBuffer[((cIdx - rev->tapDelay[5]) & cMask) * 4 + 1] * rev->combGain[3]) >> 12;

				apfDelay = apfL[rev->apfIdxL];
				apfIn = tapSum - ((apfDelay * (int16_t)rev->apfLenOut) >> 15);
				apfMid = ((apfIn * (int16_t)rev->apfLenOut) >> 15) + apfDelay;
				apfL[rev->apfIdxL] = sas2Clamp16(apfIn);
				rev->apfIdxL = (uint16_t)(rev->apfIdxL + 1U);
				if (rev->apfIdxL == rev->apfLenL)
					rev->apfIdxL = 0U;

				apfDelay = apfOut[rev->apfIdxOut];
				apfIn = apfMid - ((apfDelay * rev->apfCoeffA) >> 15);
				revOutL = ((apfIn * rev->apfCoeffA) >> 15) + apfDelay;
				apfOut[rev->apfIdxOut] = sas2Clamp16(apfIn);
				oddQuad[2] = revOutL;
			}

			{
				int32_t inR = (oddQuad[3] >> 5) * (int32_t)wetGainVal;
				int32_t fbVal = rev->wallFeedback;
				int32_t alpha = rev->iirAlpha;
				int32_t combC;
				int32_t combD;
				int32_t prevC;
				int32_t prevD;
				int32_t tapSum;
				int32_t apfIn;
				int32_t apfDelay;
				int32_t apfMid;

				combC = sas2Clamp16((inR - ((rev->combBuffer[((cIdx - rev->tapDelay[11]) & cMask) * 4 + 3] * fbVal) >> 4)) >> 11);
				prevC = rev->combBuffer[((cIdx - rev->combMaxDelay[3]) & cMask) * 4 + 3];
				rev->combBuffer[cIdx * 4 + 3] = (int16_t)(prevC + ((alpha * (combC - prevC)) >> 11));

				combD = sas2Clamp16((inR - ((rev->combBuffer[((cIdx - rev->tapDelay[10]) & cMask) * 4 + 2] * fbVal) >> 4)) >> 11);
				prevD = rev->combBuffer[((cIdx - rev->combMaxDelay[2]) & cMask) * 4 + 2];
				rev->combBuffer[cIdx * 4 + 2] = (int16_t)(prevD + ((alpha * (combD - prevD)) >> 11));

				tapSum = (rev->combBuffer[((cIdx - rev->tapDelay[3]) & cMask) * 4 + 3] * rev->combGain[0] +
				          rev->combBuffer[((cIdx - rev->tapDelay[7]) & cMask) * 4 + 3] * rev->combGain[1] +
				          rev->combBuffer[((cIdx - rev->tapDelay[2]) & cMask) * 4 + 2] * rev->combGain[2] +
				          rev->combBuffer[((cIdx - rev->tapDelay[6]) & cMask) * 4 + 2] * rev->combGain[3]) >> 12;

				apfDelay = apfR[rev->apfIdxR];
				apfIn = tapSum - ((apfDelay * (int16_t)rev->apfLenOut) >> 15);
				apfMid = ((apfIn * (int16_t)rev->apfLenOut) >> 15) + apfDelay;
				apfR[rev->apfIdxR] = sas2Clamp16(apfIn);
				rev->apfIdxR = (uint16_t)(rev->apfIdxR + 1U);
				if (rev->apfIdxR == rev->apfLenR)
					rev->apfIdxR = 0U;

				apfDelay = apfOut[rev->apfIdxOut + 1U];
				apfIn = apfMid - ((apfDelay * rev->apfCoeffA) >> 15);
				revOutR = ((apfIn * rev->apfCoeffA) >> 15) + apfDelay;
				apfOut[rev->apfIdxOut + 1U] = sas2Clamp16(apfIn);
				oddQuad[3] = revOutR;

				rev->apfIdxOut = (uint16_t)(rev->apfIdxOut + 2U);
				if (rev->apfIdxOut == rev->apfLenOut)
					rev->apfIdxOut = 0U;
			}

			if (rev->wetStepsL != 0U) {
				uint16_t steps = rev->wetStepsL;
				rev->wetStepsL = (uint16_t)(steps - 2U);
				wetVolLVal = (uint16_t)(rev->wetTargetL + ((rev->wetDeltaL * (int32_t)(steps - 1U)) >> 5));
				rev->wetVolL = wetVolLVal;
			} else {
				wetVolLVal = rev->wetVolL;
			}

			if (rev->wetStepsR != 0U) {
				uint16_t steps = rev->wetStepsR;
				rev->wetStepsR = (uint16_t)(steps - 2U);
				wetVolRVal = (uint16_t)(rev->wetTargetR + ((rev->wetDeltaR * (int32_t)(steps - 1U)) >> 5));
				rev->wetVolR = wetVolRVal;
			} else {
				wetVolRVal = rev->wetVolR;
			}

			{
				int32_t interpL = (prevL + revOutL * (int32_t)wetVolLVal) >> 1;
				int32_t interpR = (prevR + revOutR * (int32_t)wetVolRVal) >> 1;

				if (rev->drySteps != 0U) {
					rev->drySteps--;
					dryGainVal = (uint16_t)(rev->dryTarget + ((rev->dryDelta * (int32_t)rev->drySteps) >> 5));
					rev->dryGain = dryGainVal;
				} else {
					dryGainVal = rev->dryGain;
				}

				outStereo[i * 2 + 0] = sas2Clamp16((interpL + (int32_t)dryGainVal * (evenQuad[0] >> 4)) >> 10);
				outStereo[i * 2 + 1] = sas2Clamp16((interpR + (int32_t)dryGainVal * (evenQuad[1] >> 4)) >> 10);

				if (rev->drySteps != 0U) {
					rev->drySteps--;
					dryGainVal = (uint16_t)(rev->dryTarget + ((rev->dryDelta * (int32_t)rev->drySteps) >> 5));
					rev->dryGain = dryGainVal;
				}

				outStereo[i * 2 + 2] = sas2Clamp16((prevL + (int32_t)dryGainVal * (oddQuad[0] >> 4)) >> 10);
				outStereo[i * 2 + 3] = sas2Clamp16((prevR + (int32_t)dryGainVal * (oddQuad[1] >> 4)) >> 10);
			}
		}

		rev->prevOutL = prevL;
		rev->prevOutR = prevR;
		return outEnd;
	}

	/* Bypass reverb: scale wet direct path + dry path */
	{
		uint16_t wetTgt = rev->wetTarget;
		uint16_t wetCur = rev->wetGain;
		uint16_t scaledTgtL = (uint16_t)(((uint32_t)wetTgt * rev->wetTargetL) >> 5);
		uint16_t scaledCurL = (uint16_t)(((uint32_t)wetCur * rev->wetVolL) >> 5);
		uint16_t scaledTgtR = (uint16_t)(((uint32_t)wetTgt * rev->wetTargetR) >> 5);
		uint16_t scaledCurR = (uint16_t)(((uint32_t)wetCur * rev->wetVolR) >> 5);
		int16_t deltaL = (int16_t)(scaledTgtL - scaledCurL);
		int16_t deltaR = (int16_t)(scaledTgtR - scaledCurR);

		rev->wetGain = wetTgt;
		rev->wetSteps = 0U;
		rev->wetTargetL = scaledTgtL;
		rev->wetVolL = scaledCurL;
		rev->wetDeltaL = deltaL;
		rev->wetTargetR = scaledTgtR;
		rev->wetVolR = scaledCurR;
		rev->wetDeltaR = deltaR;

		for (i = 0; i < SAS2_BLOCK_SAMPLES; i++) {
			int32_t *quad = &mixBuf[i * 4];
			uint16_t curL;
			uint16_t curR;
			uint16_t curDry;

			if (rev->wetStepsL != 0U) {
				rev->wetStepsL--;
				curL = (uint16_t)(scaledTgtL + (((int32_t)rev->wetStepsL * deltaL) >> 5));
				rev->wetVolL = curL;
			} else {
				curL = rev->wetVolL;
			}

			if (rev->wetStepsR != 0U) {
				rev->wetStepsR--;
				curR = (uint16_t)(scaledTgtR + (((int32_t)rev->wetStepsR * deltaR) >> 5));
				rev->wetVolR = curR;
			} else {
				curR = rev->wetVolR;
			}

			if (rev->drySteps != 0U) {
				rev->drySteps--;
				curDry = (uint16_t)(rev->dryTarget + (((int32_t)rev->drySteps * rev->dryDelta) >> 5));
				rev->dryGain = curDry;
			} else {
				curDry = rev->dryGain;
			}

			outStereo[i * 2 + 0] = sas2Clamp16(((int32_t)curL * (quad[2] >> 12) +
			                                    (int32_t)curDry * (quad[0] >> 5)) >> 9);
			outStereo[i * 2 + 1] = sas2Clamp16(((int32_t)curR * (quad[3] >> 12) +
			                                    (int32_t)curDry * (quad[1] >> 5)) >> 9);
		}
	}

	return outEnd;
}
