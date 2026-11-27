/*
 * pops_spu.c: ScePspemu PlayStation 1 SPU 24-voice ADPCM/ADSR/noise/CD-XA/reverb
 * audio synthesizer (0x8102d638..0x8102f0d7).
 */
#include "host_common.h"

#include <string.h>

#define SPU_VOICE_COUNT          24
#define SPU_RAM_SIZE             0x80000U
#define SPU_RAM_GUEST_ADDR       0x09F40000U
#define SPU_CAPTURE_MASK         0x1FFU
#define SPU_BLOCK_CYCLE_STEP     114688
#define SPU_CDDA_SECTOR_BYTES    2352
#define SPU_XA_SECTOR_BYTES      2304
#define SPU_XA_HEADER_BYTES      24

typedef struct PopsSpuVoiceRegs {
	uint32_t volumeLr;
	uint16_t pitch;
	uint16_t startAddr;
	uint32_t adsrReg;
	uint16_t currentAdsr;
	uint16_t repeatAddr;
} PopsSpuVoiceRegs;

_Static_assert(sizeof(PopsSpuVoiceRegs) == 16U, "PopsSpuVoiceRegs size");

typedef struct PopsSpuReverbRegs {
	uint16_t dApf1;
	uint16_t dApf2;
	int16_t  vIir;
	int16_t  vComb1;
	int16_t  vComb2;
	int16_t  vComb3;
	int16_t  vComb4;
	int16_t  vWall;
	int16_t  vApf1;
	int16_t  vApf2;
	uint16_t mLSame;
	uint16_t mRSame;
	uint16_t mLComb1;
	uint16_t mRComb1;
	uint16_t mLComb2;
	uint16_t mRComb2;
	uint16_t dLSame;
	uint16_t dRSame;
	uint16_t mLDiff;
	uint16_t mRDiff;
	uint16_t mLComb3;
	uint16_t mRComb3;
	uint16_t mLComb4;
	uint16_t mRComb4;
	uint16_t dLDiff;
	uint16_t dRDiff;
	uint16_t mLApf1;
	uint16_t mRApf1;
	uint16_t mLApf2;
	uint16_t mRApf2;
	int16_t  vLIn;
	int16_t  vRIn;
} PopsSpuReverbRegs;

_Static_assert(sizeof(PopsSpuReverbRegs) == 64U, "PopsSpuReverbRegs size");

typedef struct PopsSpuControlRegs {
	uint32_t mainVolLr;
	int16_t  reverbOutVolL;
	int16_t  reverbOutVolR;
	uint32_t keyOnMask;
	uint32_t keyOffMask;
	uint32_t pitchModMask;
	uint32_t noiseModeMask;
	uint32_t reverbEnableMask;
	uint32_t endxMask;
	uint16_t reverbBasePad;
	uint16_t reverbWorkBase;
	uint16_t irqAddr;
	uint16_t transferAddr;
	uint16_t transferFifo;
	uint16_t spuControl;
	uint16_t transferCtrl;
	uint16_t spuStatus;
	int16_t  cdVolL;
	int16_t  cdVolR;
	int16_t  extVolL;
	int16_t  extVolR;
	uint32_t currentMainVolLr;
	uint32_t reservedTail;
} PopsSpuControlRegs;

_Static_assert(sizeof(PopsSpuControlRegs) == 64U, "PopsSpuControlRegs size");

typedef struct PopsSpuSyncState {
	uint32_t pendingKeyOn;
	uint32_t pendingKeyOff;
	uint32_t voiceDirtyMask;
	uint8_t  cdMixMatrix[4];
	uint8_t  cdWriteIdx;
	uint8_t  cdReadIdx;
	uint8_t  clearIrqFlag;
	int8_t   cdAudioMode;
	uint32_t repeatAddrWrittenMask;
	uint32_t sampleCounter;
	int8_t   syncVoiceId;
	int8_t   syncVoiceDelta;
	uint8_t  irqAddrMode;
	uint8_t  hostFlags;
} PopsSpuSyncState;

_Static_assert(sizeof(PopsSpuSyncState) == 32U, "PopsSpuSyncState size");

typedef struct PopsSpuSharedRam {
	PopsSpuVoiceRegs   voices[SPU_VOICE_COUNT];    /* +0x000 */
	PopsSpuControlRegs ctrl;                       /* +0x180 */
	PopsSpuReverbRegs  reverb;                     /* +0x1C0 */
	uint32_t           voiceCurrentVolLr[24];      /* +0x200 */
	uint32_t           cdSectorGuestAddr[8];       /* +0x260 */
	PopsSpuSyncState   sync;                       /* +0x280 */
	uint32_t           lockReserved;               /* +0x2A0 */
	volatile uint8_t   guestLock;                  /* +0x2A4 */
	volatile uint8_t   guestTurn;                  /* +0x2A5 */
	volatile uint16_t  hostLockWord;               /* +0x2A6 */
	uint8_t            alignPad[0x18];             /* +0x2A8 */
	uint8_t            spuRam[SPU_RAM_SIZE];       /* +0x2C0 (704) */
} PopsSpuSharedRam;

_Static_assert(offsetof(PopsSpuSharedRam, ctrl) == 384U, "ctrl offset");
_Static_assert(offsetof(PopsSpuSharedRam, reverb) == 448U, "reverb offset");
_Static_assert(offsetof(PopsSpuSharedRam, voiceCurrentVolLr) == 512U, "voiceCurrentVolLr offset");
_Static_assert(offsetof(PopsSpuSharedRam, cdSectorGuestAddr) == 608U, "cdSectorGuestAddr offset");
_Static_assert(offsetof(PopsSpuSharedRam, sync) == 640U, "sync offset");
_Static_assert(offsetof(PopsSpuSharedRam, guestLock) == 676U, "guestLock offset");
_Static_assert(offsetof(PopsSpuSharedRam, spuRam) == 704U, "spuRam offset");

typedef struct PopsSpuSweepChannel {
	int16_t  currentVol;
	int16_t  targetVol;
	int16_t  step;
	uint16_t counter;
	uint16_t period;
} PopsSpuSweepChannel;

_Static_assert(sizeof(PopsSpuSweepChannel) == 10U, "PopsSpuSweepChannel size");

typedef struct PopsSpuVoiceState {
	int16_t             decodedPcm[31];
	int8_t              mutedFlag;
	PopsSpuSweepChannel sweepL;
	PopsSpuSweepChannel sweepR;
	uint32_t            cachedVolLr;
	uint32_t            cachedAdsrReg;
	uint8_t             adsrPhase;
	uint8_t             adsrExpFlag;
	uint16_t            adsrTarget;
	uint16_t            adsrCounter;
	int16_t             adsrStep;
	uint16_t            adsrPeriod;
	int16_t             adsrLevel;
	int32_t             pitchPhase;
	uint16_t            currentAddr;
	uint8_t             repeatLocked;
	uint8_t             blockFlags;
	uint16_t            pitchReg;
	uint16_t            repeatAddr;
} PopsSpuVoiceState;

_Static_assert(sizeof(PopsSpuVoiceState) == 116U, "PopsSpuVoiceState size");

typedef struct PopsSpuReverbChannel {
	int32_t  inputGain;
	int16_t *sameWritePtr;
	int16_t *combReadPtrA;
	int16_t *combReadPtrB;
	int16_t *sameReadPtr;
	int16_t *diffWritePtr;
	int16_t *combReadPtrC;
	int16_t *combReadPtrD;
	int16_t *diffReadPtr;
	int16_t *apfWritePtrA;
	int16_t *apfWritePtrB;
	int16_t *apfReadPtrA;
	int16_t *apfReadPtrB;
	int32_t  firHistory[3];
} PopsSpuReverbChannel;

_Static_assert(sizeof(PopsSpuReverbChannel) == 64U, "PopsSpuReverbChannel size");

typedef struct PopsSpuHostState {
	uint32_t             endxStatus;
	uint16_t             captureWritePos;
	uint16_t             cdSectorOffset;
	uint8_t             *cdSectorPtr;
	int32_t              xaSampleCursor;
	int32_t              xaResamplePhase;
	int8_t               xaPitchStep;
	uint8_t              xaStereoFlag;
	int16_t              cachedSpuControl;
	uint16_t             noiseFeedbackAddr;
	int16_t              noiseLevel;
	int16_t              noiseTimer;
	uint8_t              noiseStepPattern;
	uint8_t              irqHitFlag;
	uint8_t             *irqTargetPtr;
	uint8_t             *cachedAdpcmPtr;
	PopsSpuSharedRam    *sharedRam;
	PopsSpuSweepChannel  mainSweepL;
	PopsSpuSweepChannel  mainSweepR;
	uint32_t             cachedMainVolLr;
	int16_t             *reverbWorkBasePtr;
	int16_t              vIir;
	int16_t              vComb1;
	int16_t              vComb2;
	int16_t              vComb3;
	int16_t              vComb4;
	int16_t              vWall;
	int16_t              vApf1;
	int16_t              vApf2;
	PopsSpuReverbChannel reverbCh[2];
	PopsSpuVoiceState    voices[SPU_VOICE_COUNT];
	int16_t              xaFilterTable[35];
	uint8_t              vagPosCoeff[5];
	uint8_t              vagNegCoeff[5];
	uint16_t             adsrTargetTable[4];
	int16_t              gaussTable[1024];
	int16_t              xaRingLeft[228];
	int16_t              xaRingRight[228];
	uint8_t              cdSectorCache[2352];
} PopsSpuHostState;

static PopsSpuHostState g_spuState;

static const int16_t g_spuGaussHalfTable[512] = {
	  4807,  22963,   4871,     -1,   4744,  22962,   4935,     -1,
	  4681,  22960,   5000,     -1,   4619,  22957,   5065,     -1,
	  4557,  22953,   5131,     -1,   4495,  22948,   5197,     -1,
	  4435,  22942,   5264,     -1,   4374,  22935,   5332,     -1,
	  4315,  22927,   5399,     -1,   4255,  22918,   5468,     -1,
	  4197,  22908,   5536,     -1,   4138,  22897,   5606,     -1,
	  4081,  22885,   5676,     -1,   4023,  22872,   5746,     -1,
	  3967,  22857,   5817,     -1,   3910,  22842,   5888,     -1,
	  3855,  22826,   5959,      0,   3799,  22809,   6032,      0,
	  3745,  22791,   6104,      0,   3691,  22772,   6177,      0,
	  3637,  22752,   6251,      0,   3584,  22731,   6325,      0,
	  3531,  22709,   6400,      0,   3479,  22686,   6475,      1,
	  3427,  22662,   6550,      1,   3376,  22637,   6626,      1,
	  3325,  22611,   6702,      1,   3275,  22584,   6779,      2,
	  3225,  22556,   6856,      2,   3176,  22527,   6934,      2,
	  3128,  22498,   7012,      3,   3079,  22467,   7091,      3,
	  3035,  22435,   7170,      3,   2985,  22402,   7249,      4,
	  2938,  22369,   7329,      4,   2892,  22334,   7409,      5,
	  2846,  22299,   7490,      5,   2801,  22262,   7571,      6,
	  2756,  22225,   7653,      7,   2712,  22187,   7735,      7,
	  2668,  22148,   7817,      8,   2624,  22107,   7900,      9,
	  2582,  22066,   7983,      9,   2539,  22025,   8066,     10,
	  2497,  21982,   8150,     11,   2456,  21938,   8234,     12,
	  2415,  21893,   8319,     13,   2374,  21848,   8404,     14,
	  2334,  21801,   8489,     15,   2295,  21754,   8575,     16,
	  2256,  21706,   8661,     17,   2217,  21657,   8748,     18,
	  2179,  21607,   8834,     19,   2141,  21556,   8922,     21,
	  2104,  21505,   9009,     22,   2067,  21452,   9097,     24,
	  2031,  21399,   9185,     25,   1995,  21345,   9273,     27,
	  1959,  21290,   9362,     28,   1924,  21235,   9451,     30,
	  1890,  21178,   9541,     32,   1856,  21121,   9630,     33,
	  1822,  21063,   9720,     35,   1789,  21004,   9811,     37,
	  1756,  20944,   9901,     39,   1723,  20884,   9992,     41,
	  1691,  20822,  10083,     44,   1660,  20760,  10174,     46,
	  1628,  20698,  10266,     48,   1598,  20634,  10358,     51,
	  1567,  20570,  10450,     53,   1537,  20505,  10542,     56,
	  1508,  20439,  10635,     58,   1479,  20373,  10727,     61,
	  1450,  20306,  10820,     64,   1422,  20238,  10913,     67,
	  1394,  20169,  11007,     70,   1366,  20100,  11100,     73,
	  1339,  20030,  11194,     77,   1312,  19959,  11288,     80,
	  1286,  19888,  11382,     84,   1260,  19816,  11476,     87,
	  1234,  19744,  11571,     91,   1209,  19671,  11665,     95,
	  1184,  19597,  11760,     99,   1160,  19522,  11855,    103,
	  1136,  19447,  11950,    107,   1112,  19372,  12045,    111,
	  1089,  19295,  12140,    116,   1066,  19219,  12236,    120,
	  1043,  19141,  12331,    125,   1020,  19063,  12427,    130,
	   999,  18985,  12522,    135,    977,  18905,  12618,    140,
	   956,  18826,  12714,    145,    935,  18746,  12809,    150,
	   914,  18665,  12905,    156,    894,  18584,  13001,    161,
	   874,  18502,  13097,    167,    854,  18420,  13193,    173,
	   835,  18337,  13289,    179,    816,  18254,  13385,    186,
	   797,  18170,  13481,    192,    779,  18086,  13577,    199,
	   761,  18001,  13673,    205,    743,  17916,  13769,    212,
	   726,  17830,  13865,    219,    708,  17744,  13961,    227,
	   692,  17658,  14056,    234,    675,  17571,  14152,    242,
	   659,  17484,  14248,    250,    643,  17396,  14343,    257,
	   627,  17308,  14439,    266,    612,  17220,  14534,    274,
	   597,  17131,  14630,    283,    582,  17042,  14725,    291,
	   567,  16953,  14820,    300,    553,  16863,  14915,    309,
	   539,  16773,  15010,    319,    525,  16682,  15104,    328,
	   512,  16592,  15199,    338,    498,  16500,  15293,    348,
	   485,  16409,  15387,    358,    473,  16317,  15481,    369,
	   460,  16226,  15575,    379,    448,  16133,  15669,    390,
	   436,  16041,  15762,    401,    424,  15948,  15855,    412
};

static const int16_t g_spuXaFilterTable[35] = {
	 2403, 13885, 13917,  2435,     0,
	 1423, 12572, 14894,  3747,     2,
	  739, 10926, 15550,  5394,    30,
	  337,  9123, 15861,  7197,   121,
	  121,  7197, 15861,  9123,   337,
	   30,  5394, 15550, 10926,   739,
	    2,  3747, 14894, 12572,  1423
};

static const uint8_t g_spuVagPredictorTable[10] = {
	0, 60, 115, 98, 122,
	0,  0,  52, 55,  60
};

static inline int16_t spuClamp16(int32_t value)
{
	if (value > 32767)
		return 32767;
	if (value < -32768)
		return -32768;
	return (int16_t)value;
}

static inline void spuConfigureSweep(PopsSpuSweepChannel *ch, uint16_t regVal, uint32_t *cachedReg,
                                     uint32_t clearBit)
{
	if ((regVal & 0x8000U) == 0U) {
		ch->counter = 0;
		ch->currentVol = (int16_t)(regVal << 1);
		return;
	}

	{
		uint32_t shift = (regVal >> 2) & 0x1FU;
		int32_t base = ((regVal & 0x2000U) != 0U) ? -8 : 7;
		int32_t stepVal = base ^ (int32_t)(regVal & 3U);
		uint16_t period;

		if (shift <= 11U) {
			stepVal = stepVal << (11U - shift);
			period = 1U;
		} else {
			uint32_t expShift = shift - 11U;
			period = (uint16_t)(1U << expShift);
			if (expShift > 15U) {
				if ((regVal & 0x7FU) == 0x7FU)
					*cachedReg &= ~clearBit;
				else
					period = 0x8000U;
			}
		}

		ch->step = (int16_t)stepVal;
		ch->targetVol = ((regVal & 0x2000U) != 0U) ? 0 : 0x7FFF;
		ch->period = period;
		ch->counter = period;
	}
}

static inline int16_t spuStepSweep(PopsSpuSweepChannel *ch, uint16_t regVal)
{
	int32_t vol = ch->currentVol;

	if (ch->counter != 0U) {
		ch->counter--;
		if (ch->counter == 0U) {
			if (vol < 0)
				vol = -vol;
			if (((regVal >> 13) & 3U) == 3U)
				vol = (vol * ch->step) >> 15;
			else
				vol += ch->step;

			if (((uint32_t)vol >> 15) != 0U) {
				vol = ch->targetVol;
				ch->counter = 0;
			} else {
				ch->counter = ch->period;
			}

			if ((regVal & 0x1000U) != 0U)
				vol = -vol;
			ch->currentVol = (int16_t)vol;
		}
	}
	return (int16_t)vol;
}

static inline void spuStartReleasePhase(PopsSpuVoiceState *voice)
{
	uint32_t relBits = (voice->cachedAdsrReg >> 16) & 0xFFU;
	uint32_t shift = relBits & 0x1FU;
	uint16_t period;
	int32_t step;
	uint8_t isExp;

	voice->adsrPhase = 24;
	voice->adsrTarget = 0;

	if (shift <= 11U) {
		period = 1U;
		step = (int32_t)((uint32_t)(-8) << (11U - shift));
		isExp = (uint8_t)((step != 0) & ((relBits >> 5) & 1U));
	} else {
		period = (uint16_t)(1U << (shift - 11U));
		if (period == 0U) {
			voice->adsrPeriod = 0;
			voice->adsrExpFlag = 0;
			voice->adsrStep = 0;
			voice->adsrCounter = 0;
			return;
		}
		step = -8;
		isExp = (uint8_t)((relBits >> 5) & 1U);
	}

	voice->adsrPeriod = period;
	voice->adsrExpFlag = isExp;
	voice->adsrStep = (int16_t)(isExp ? (step & 0x7FF8) : step);
	voice->adsrCounter = period;
}

static inline int16_t *spuReverbPtr(uint8_t *ramBase, uint32_t wordOffset)
{
	if (wordOffset >= 0xFFFFU)
		wordOffset = 0xFFFFU;
	return (int16_t *)(ramBase + (wordOffset * 8U));
}

static inline int16_t *spuWrapReverbPtr(int16_t *ptr, int16_t *ramEnd, int16_t *workBase)
{
	ptr++;
	return (ptr == ramEnd) ? workBase : ptr;
}

static inline __attribute__((always_inline)) void spuLoadReverbConfig(const PopsSpuReverbRegs *rev,
                                                                      uint16_t reverbWorkBase,
                                                                      uint8_t hostFlags)
{
	uint8_t *ramBase = g_spuState.sharedRam->spuRam;
	uint32_t base = reverbWorkBase;
	const uint16_t *mPairs = &rev->mLSame;
	int16_t **leftPtrs[10] = {
		&g_spuState.reverbCh[0].sameWritePtr,
		&g_spuState.reverbCh[0].combReadPtrA,
		&g_spuState.reverbCh[0].combReadPtrB,
		&g_spuState.reverbCh[0].sameReadPtr,
		&g_spuState.reverbCh[0].diffWritePtr,
		&g_spuState.reverbCh[0].combReadPtrC,
		&g_spuState.reverbCh[0].combReadPtrD,
		&g_spuState.reverbCh[0].diffReadPtr,
		&g_spuState.reverbCh[0].apfWritePtrA,
		&g_spuState.reverbCh[0].apfWritePtrB
	};
	int16_t **rightPtrs[10] = {
		&g_spuState.reverbCh[1].sameWritePtr,
		&g_spuState.reverbCh[1].combReadPtrA,
		&g_spuState.reverbCh[1].combReadPtrB,
		&g_spuState.reverbCh[1].sameReadPtr,
		&g_spuState.reverbCh[1].diffWritePtr,
		&g_spuState.reverbCh[1].combReadPtrC,
		&g_spuState.reverbCh[1].combReadPtrD,
		&g_spuState.reverbCh[1].diffReadPtr,
		&g_spuState.reverbCh[1].apfWritePtrA,
		&g_spuState.reverbCh[1].apfWritePtrB
	};
	int i;

	g_spuState.reverbWorkBasePtr = (int16_t *)(ramBase + base * 8U);
	g_spuState.vIir   = rev->vIir;
	g_spuState.vComb1 = rev->vComb1;
	g_spuState.vComb2 = rev->vComb2;
	g_spuState.vComb3 = rev->vComb3;
	g_spuState.vComb4 = rev->vComb4;
	g_spuState.vWall  = rev->vWall;
	g_spuState.vApf1  = rev->vApf1;
	g_spuState.vApf2  = rev->vApf2;

	for (i = 0; i < 10; i++) {
		*leftPtrs[i]  = spuReverbPtr(ramBase, base + mPairs[i * 2]);
		*rightPtrs[i] = spuReverbPtr(ramBase, base + mPairs[i * 2 + 1]);
	}

	g_spuState.reverbCh[0].apfReadPtrA = spuReverbPtr(ramBase, base + rev->mLApf1 - rev->dApf1);
	g_spuState.reverbCh[1].apfReadPtrA = spuReverbPtr(ramBase, base + rev->mRApf1 - rev->dApf1);
	g_spuState.reverbCh[0].apfReadPtrB = spuReverbPtr(ramBase, base + rev->mLApf2 - rev->dApf2);
	g_spuState.reverbCh[1].apfReadPtrB = spuReverbPtr(ramBase, base + rev->mRApf2 - rev->dApf2);

	g_spuState.reverbCh[0].inputGain = rev->vLIn;
	g_spuState.reverbCh[1].inputGain = rev->vRIn;

	if ((hostFlags & 1U) != 0U)
		g_spuState.captureWritePos = 0;
}

static inline __attribute__((always_inline)) void spuDecodeXaSector(const uint8_t *sectorData,
                                                                    uint8_t codingInfo)
{
	int isStereo = (codingInfo & 1U) != 0U;
	int isHalfRate = (codingInfo & 4U) != 0U;

	if (g_spuState.xaPitchStep == 0) {
		g_spuState.xaResamplePhase = 0;
		g_spuState.xaRingLeft[224] = 0;
		g_spuState.xaRingLeft[225] = 0;
		g_spuState.xaRingLeft[226] = 0;
		g_spuState.xaRingLeft[227] = 0;
		g_spuState.xaRingRight[224] = 0;
		g_spuState.xaRingRight[225] = 0;
		g_spuState.xaRingRight[226] = 0;
		g_spuState.xaRingRight[227] = 0;
	}

	g_spuState.xaRingLeft[0] = g_spuState.xaRingLeft[224];
	g_spuState.xaRingLeft[1] = g_spuState.xaRingLeft[225];
	g_spuState.xaRingLeft[2] = g_spuState.xaRingLeft[226];
	g_spuState.xaRingLeft[3] = g_spuState.xaRingLeft[227];

	g_spuState.xaStereoFlag = (uint8_t)isStereo;
	g_spuState.xaPitchStep = (int8_t)(isHalfRate ? -30 : -60);

	if (isStereo) {
		int group;
		const uint8_t *src = sectorData;

		g_spuState.xaRingRight[0] = g_spuState.xaRingRight[224];
		g_spuState.xaRingRight[1] = g_spuState.xaRingRight[225];
		g_spuState.xaRingRight[2] = g_spuState.xaRingRight[226];
		g_spuState.xaRingRight[3] = g_spuState.xaRingRight[227];

		for (group = 0; group < 2; group++) {
			int unit;
			for (unit = 0; unit < 4; unit++) {
				int baseIdx = 4 + (group * 4 + unit) * 28;
				int word;

				/* Right channel (high nibble) */
				{
					uint8_t param = src[4 + unit * 2 + 1];
					int filt = (param >> 4) & 3;
					int shift = (param & 0x0F) + 10;
					int16_t coeffPos = g_spuState.vagPosCoeff[filt];
					int16_t coeffNeg = g_spuState.vagNegCoeff[filt];
					int16_t prevOld = g_spuState.xaRingRight[baseIdx - 2];
					int16_t prevNew = g_spuState.xaRingRight[baseIdx - 1];

					for (word = 0; word < 14; word++) {
						uint8_t bEven = src[16 + word * 8 + unit];
						uint8_t bOdd  = src[16 + word * 8 + 4 + unit];
						int32_t sEven = ((int32_t)(((uint32_t)bEven << 24) & 0xF0000000U)) >> shift;
						int32_t sOdd  = ((int32_t)(((uint32_t)bOdd  << 24) & 0xF0000000U)) >> shift;
						prevOld = spuClamp16((prevNew * coeffPos + sEven - prevOld * coeffNeg) >> 6);
						g_spuState.xaRingRight[baseIdx + word * 2] = prevOld;
						prevNew = spuClamp16((prevOld * coeffPos + sOdd - prevNew * coeffNeg) >> 6);
						g_spuState.xaRingRight[baseIdx + word * 2 + 1] = prevNew;
					}
				}

				/* Left channel (low nibble) */
				{
					uint8_t param = src[4 + unit * 2];
					int filt = (param >> 4) & 3;
					int shift = (param & 0x0F) + 10;
					int16_t coeffPos = g_spuState.vagPosCoeff[filt];
					int16_t coeffNeg = g_spuState.vagNegCoeff[filt];
					int16_t prevOld = g_spuState.xaRingLeft[baseIdx - 2];
					int16_t prevNew = g_spuState.xaRingLeft[baseIdx - 1];

					for (word = 0; word < 14; word++) {
						uint8_t bEven = src[16 + word * 8 + unit];
						uint8_t bOdd  = src[16 + word * 8 + 4 + unit];
						int32_t sEven = ((int32_t)((uint32_t)bEven << 28)) >> shift;
						int32_t sOdd  = ((int32_t)((uint32_t)bOdd  << 28)) >> shift;
						prevOld = spuClamp16((prevNew * coeffPos + sEven - prevOld * coeffNeg) >> 6);
						g_spuState.xaRingLeft[baseIdx + word * 2] = prevOld;
						prevNew = spuClamp16((prevOld * coeffPos + sOdd - prevNew * coeffNeg) >> 6);
						g_spuState.xaRingLeft[baseIdx + word * 2 + 1] = prevNew;
					}
				}
			}
			src += 128;
		}
	} else {
		int unit;
		int16_t prevNew = g_spuState.xaRingLeft[3];

		for (unit = 0; unit < 4; unit++) {
			int baseIdx = 4 + unit * 56;
			int word;
			int16_t prevOld = g_spuState.xaRingLeft[baseIdx - 2];

			/* Low nibble sub-block (28 samples) */
			{
				uint8_t param = sectorData[4 + unit * 2];
				int filt = (param >> 4) & 3;
				int shift = (param & 0x0F) + 10;
				int16_t coeffPos = g_spuState.vagPosCoeff[filt];
				int16_t coeffNeg = g_spuState.vagNegCoeff[filt];

				for (word = 0; word < 14; word++) {
					uint8_t bEven = sectorData[16 + word * 8 + unit];
					uint8_t bOdd  = sectorData[16 + word * 8 + 4 + unit];
					int32_t sEven = ((int32_t)((uint32_t)bEven << 28)) >> shift;
					int32_t sOdd  = ((int32_t)((uint32_t)bOdd  << 28)) >> shift;
					prevOld = spuClamp16((prevNew * coeffPos + sEven - prevOld * coeffNeg) >> 6);
					g_spuState.xaRingLeft[baseIdx + word * 2] = prevOld;
					prevNew = spuClamp16((prevOld * coeffPos + sOdd - prevNew * coeffNeg) >> 6);
					g_spuState.xaRingLeft[baseIdx + word * 2 + 1] = prevNew;
				}
			}

			/* High nibble sub-block (28 samples) */
			{
				uint8_t param = sectorData[4 + unit * 2 + 1];
				int filt = (param >> 4) & 3;
				int shift = (param & 0x0F) + 10;
				int16_t coeffPos = g_spuState.vagPosCoeff[filt];
				int16_t coeffNeg = g_spuState.vagNegCoeff[filt];

				for (word = 0; word < 14; word++) {
					uint8_t bEven = sectorData[16 + word * 8 + unit];
					uint8_t bOdd  = sectorData[16 + word * 8 + 4 + unit];
					int32_t sEven = ((int32_t)(((uint32_t)bEven << 24) & 0xF0000000U)) >> shift;
					int32_t sOdd  = ((int32_t)(((uint32_t)bOdd  << 24) & 0xF0000000U)) >> shift;
					prevOld = spuClamp16((prevNew * coeffPos + sEven - prevOld * coeffNeg) >> 6);
					g_spuState.xaRingLeft[baseIdx + 28 + word * 2] = prevOld;
					prevNew = spuClamp16((prevOld * coeffPos + sOdd - prevNew * coeffNeg) >> 6);
					g_spuState.xaRingLeft[baseIdx + 28 + word * 2 + 1] = prevNew;
				}
			}
		}
	}
}

/* 0x8102d638 */
uint32_t popsSpuMixSample(void)
{
	PopsSpuSharedRam *shared = g_spuState.sharedRam;
	PopsSpuControlRegs ctrlSnap;
	PopsSpuSyncState syncSnap;
	uint32_t result = 0U;
	uint16_t spuControl;
	uint8_t adpcmBlock[16];

	g_spuState.cachedAdpcmPtr = NULL;

	/* Acquire lock shared with PSP guest */
	shared->hostLockWord = 1U;
	while ((*(volatile uint32_t *)&shared->guestLock & 0xFF00FFFFU) == 0x00000100U)
		;
	__sync_synchronize();

	ctrlSnap = shared->ctrl;
	syncSnap = shared->sync;
	shared->sync.sampleCounter = syncSnap.sampleCounter + 1U;

	spuControl = ctrlSnap.spuControl;
	if ((spuControl & 0x8000U) == 0U) {
		__sync_synchronize();
		*(volatile uint8_t *)&shared->hostLockWord = 0U;
		__sync_synchronize();

		shared->sync.cdWriteIdx = 0U;
		shared->sync.cdReadIdx = 0U;
		if (syncSnap.cdAudioMode == -1)
			shared->sync.cdAudioMode = -2;

		if (g_spuState.cachedSpuControl < 0) {
			int v;
			g_spuState.cachedSpuControl = (int16_t)(spuControl & 0x3FU);
			memset(g_spuState.voices, 0, sizeof(g_spuState.voices));
			for (v = 0; v < SPU_VOICE_COUNT; v++)
				shared->voices[v].currentAdsr = 0U;
			g_spuState.xaPitchStep = 0;
			g_spuState.cdSectorPtr = NULL;
			g_spuState.xaSampleCursor = 0;
			shared->sync.pendingKeyOn = 0U;
			shared->sync.pendingKeyOff = 0U;
		}
		result = 0U;
	} else {
		uint32_t keyOnPending = syncSnap.pendingKeyOn;
		uint32_t keyOffPending = syncSnap.pendingKeyOff;
		uint32_t voiceDirty = syncSnap.voiceDirtyMask;
		uint32_t repeatMask = syncSnap.repeatAddrWrittenMask;
		uint32_t pitchModMask = ctrlSnap.pitchModMask;
		uint32_t noiseModeMask = ctrlSnap.noiseModeMask;
		uint32_t reverbMask = ctrlSnap.reverbEnableMask;
		int32_t dryLeft = 0;
		int32_t dryRight = 0;
		int32_t revLeft = 0;
		int32_t revRight = 0;
		int32_t prevVoiceMod = 0x8000;
		int16_t noiseVal = g_spuState.noiseLevel;
		int voiceIdx;

		shared->ctrl.keyOffMask = ctrlSnap.keyOffMask & ~keyOnPending;
		shared->sync.voiceDirtyMask = 0U;
		shared->sync.repeatAddrWrittenMask = 0U;
		shared->sync.pendingKeyOn = 0U;
		shared->sync.pendingKeyOff = 0U;
		shared->ctrl.keyOnMask = 0U;
		__sync_synchronize();
		*(volatile uint8_t *)&shared->hostLockWord = 0U;
		__sync_synchronize();

		g_spuState.endxStatus = ctrlSnap.endxMask & ~keyOnPending;
		if (syncSnap.clearIrqFlag != 0U) {
			g_spuState.irqHitFlag = 0U;
			shared->sync.clearIrqFlag = 0U;
		}

		if ((spuControl & 0x40U) != 0U)
			g_spuState.irqTargetPtr = shared->spuRam + ((uint32_t)ctrlSnap.irqAddr * 8U);
		else
			g_spuState.irqTargetPtr = shared->spuRam - 2;

		/* Step PS1 SPU noise generator */
		if ((((uint16_t)g_spuState.cachedSpuControl ^ spuControl) & 0x3F00U) != 0U) {
			g_spuState.cachedSpuControl = (int16_t)spuControl;
			g_spuState.noiseTimer = 1;
		} else {
			g_spuState.cachedSpuControl = (int16_t)spuControl;
		}

		g_spuState.noiseTimer--;
		if (g_spuState.noiseTimer <= 0) {
			int16_t reload = (int16_t)(0x4000 >> ((spuControl >> 10) & 0xFU));
			uint8_t pat = g_spuState.noiseStepPattern;
			int stepMask = (-4438) >> (4 * ((spuControl >> 8) & 3U));

			g_spuState.noiseTimer = reload;
			if (reload == 0 || ((g_spuState.noiseStepPattern = (uint8_t)((pat << 7) | (pat >> 1)),
			                     (stepMask & pat) != 0))) {
				int parity = (105 >> (((uint32_t)(uint16_t)noiseVal >> 10) & 7U)) ^ (noiseVal >> 15);
				noiseVal = (int16_t)((noiseVal << 1) | (parity & 1));
				g_spuState.noiseLevel = noiseVal;
			}
		}

		/* Mix all 24 PS1 SPU voices */
		for (voiceIdx = 0; voiceIdx < SPU_VOICE_COUNT; voiceIdx++) {
			PopsSpuVoiceState *voice = &g_spuState.voices[voiceIdx];
			uint32_t bit = 1U << voiceIdx;
			int isKeyOn = (keyOnPending & bit) != 0U;
			int isKeyOff = (keyOffPending & bit) != 0U;
			int isDirty = (voiceDirty & bit) != 0U;
			int isRepeatWritten = (repeatMask & bit) != 0U;
			int isPitchMod = (pitchModMask & bit) != 0U;
			int isNoise = (noiseModeMask & bit) != 0U;
			int isReverb = (reverbMask & bit) != 0U;
			PopsSpuVoiceRegs regSnap;
			int32_t sampleOut = 0;
			int adsrSkipAudio = 0;

			if (isKeyOn || isDirty)
				regSnap = shared->voices[voiceIdx];

			if (isDirty) {
				voice->pitchReg = regSnap.pitch;
				if (isRepeatWritten) {
					voice->repeatLocked = 1U;
					voice->repeatAddr = (uint16_t)(regSnap.repeatAddr & 0xFFFEU);
				}
				if (regSnap.adsrReg == voice->cachedAdsrReg)
					isDirty = 0;
				else
					voice->cachedAdsrReg = regSnap.adsrReg;

				if (regSnap.volumeLr != voice->cachedVolLr) {
					voice->cachedVolLr = regSnap.volumeLr;
					spuConfigureSweep(&voice->sweepL, (uint16_t)regSnap.volumeLr,
					                  &voice->cachedVolLr, 0x00008000U);
					spuConfigureSweep(&voice->sweepR, (uint16_t)(regSnap.volumeLr >> 16),
					                  &voice->cachedVolLr, 0x80000000U);
				}
			}

			if (isKeyOn) {
				voice->currentAddr = (uint16_t)(regSnap.startAddr & 0xFFFEU);
				shared->voices[voiceIdx].currentAdsr = 0U;
				voice->adsrLevel = 0;
				voice->pitchPhase = 0;
				if (voiceIdx == syncSnap.syncVoiceId) {
					int targetIdx = voiceIdx + syncSnap.syncVoiceDelta;
					g_spuState.irqHitFlag = 0U;
					g_spuState.voices[targetIdx].pitchPhase = -SPU_BLOCK_CYCLE_STEP;
					g_spuState.voices[targetIdx].currentAddr = g_spuState.noiseFeedbackAddr;
				}
				isDirty = 0;
				voice->adsrPhase = 5;
				voice->adsrExpFlag = 0;
				voice->adsrTarget = 0;
				voice->adsrCounter = 4;
				voice->adsrStep = 0;
				voice->adsrPeriod = 0;
				voice->decodedPcm[28] = 0;
				voice->decodedPcm[29] = 0;
				voice->decodedPcm[30] = 0;
				voice->mutedFlag = 0;
			}

			/* ADSR envelope state machine */
			if (isKeyOff && voice->adsrPhase < 24) {
				spuStartReleasePhase(voice);
			} else if (voice->adsrCounter > 1U) {
				if (voice->adsrPhase <= 5) {
					voice->adsrCounter--;
					prevVoiceMod = 0x8000;
					sampleOut = 0;
					adsrSkipAudio = 1;
				} else if (isDirty) {
					voice->adsrPhase = (uint8_t)((voice->adsrPhase & ~3U) - 4U);
					voice->adsrCounter = 1U;
					voice->adsrExpFlag = 0;
					voice->adsrStep = 0;
					voice->adsrTarget = (uint16_t)voice->adsrLevel;
				}
			}

			if (!adsrSkipAudio) {
				if (voice->adsrPhase >= 1 && voice->adsrPhase <= 27) {
					voice->adsrCounter--;
					if (voice->adsrCounter == 0U) {
						int32_t cur = voice->adsrLevel;
						int32_t nextVal = voice->adsrExpFlag
							? ((cur * voice->adsrStep) >> 15)
							: (cur + voice->adsrStep);
						int32_t target = (int32_t)voice->adsrTarget;
						int16_t clampedLevel;
						uint16_t nextPeriod;

						if ((nextVal - target) != 0 && ((nextVal - target) ^ (cur - target)) >= 0) {
							clampedLevel = (int16_t)nextVal;
							nextPeriod = voice->adsrPeriod;
						} else {
							int enterExpSlow = 0;

							if (nextVal >= 0x8000) {
								nextVal = 0x7FFF;
								clampedLevel = 0x7FFF;
							} else if (nextVal < 0) {
								clampedLevel = 0;
							} else {
								clampedLevel = (int16_t)nextVal;
							}

							nextPeriod = voice->adsrPeriod;
							switch (voice->adsrPhase) {
							case 5:
								voice->repeatLocked = 0U;
								/* fallthrough */
							case 4: {
								uint32_t atkByte = (voice->cachedAdsrReg >> 8) & 0xFFU;
								uint32_t shift = (atkByte >> 2) & 0x1FU;
								int32_t stepVal = (int32_t)((atkByte & 3U) ^ 7U);
								uint32_t isExpAtk = atkByte >> 7;
								voice->adsrPhase = (uint8_t)(isExpAtk + 8U);
								voice->adsrTarget = g_spuState.adsrTargetTable[isExpAtk];
								if (shift <= 11U) {
									stepVal <<= (11U - shift);
									nextPeriod = 1U;
								} else {
									nextPeriod = (uint16_t)(1U << (shift - 11U));
									if (nextPeriod == 0U)
										stepVal = 0;
								}
								voice->adsrStep = (int16_t)stepVal;
								voice->adsrPeriod = nextPeriod;
								voice->adsrExpFlag = 0;
								if (voice->adsrPhase == 9 && nextVal >= 24576)
									enterExpSlow = 1;
								break;
							}
							case 8: {
								uint32_t decByte = voice->cachedAdsrReg & 0xFFU;
								uint32_t shift = decByte >> 4;
								voice->adsrExpFlag = 1;
								nextPeriod = (shift <= 11U) ? 1U : (uint16_t)(1U << (shift - 11U));
								voice->adsrStep = (int16_t)((0xFFFFC000U >> shift) & 0x7FF8U);
								voice->adsrTarget = (uint16_t)(((decByte & 0xFU) << 11) + 2047U);
								voice->adsrPhase = 12;
								voice->adsrPeriod = nextPeriod;
								break;
							}
							case 9:
							case 18:
								enterExpSlow = 1;
								break;
							case 12: {
								uint32_t susHi = (voice->cachedAdsrReg >> 24) & 0xFFU;
								uint32_t susLo = (voice->cachedAdsrReg >> 16) & 0xFFU;
								uint32_t mode = susHi >> 6;
								uint32_t shift = susHi & 0x1FU;
								int32_t base = ((susHi & 0x40U) != 0U) ? -8 : 7;
								int32_t stepVal = base ^ (int32_t)(susLo >> 6);
								uint8_t expFlag = 0;

								if (shift <= 11U) {
									stepVal <<= (11U - shift);
									nextPeriod = 1U;
								} else {
									nextPeriod = (uint16_t)(1U << (shift - 11U));
									if (nextPeriod == 0U)
										stepVal = 0;
								}
								if (nextPeriod != 0U && mode == 3U && stepVal != 0)
									expFlag = 1;
								voice->adsrExpFlag = expFlag;
								voice->adsrStep = (int16_t)(expFlag ? (stepVal & 0x7FF8) : stepVal);
								voice->adsrPeriod = nextPeriod;
								voice->adsrPhase = (uint8_t)(mode + 16U);
								if (mode == 2U && nextVal >= 24576)
									enterExpSlow = 1;
								else
									voice->adsrTarget = g_spuState.adsrTargetTable[mode];
								break;
							}
							case 16:
								nextPeriod = 0;
								voice->adsrPeriod = 0;
								voice->adsrStep = 0;
								voice->adsrExpFlag = 0;
								break;
							case 17:
							case 19:
							case 24:
								voice->mutedFlag = -1;
								nextPeriod = 0;
								voice->adsrPeriod = 0;
								voice->adsrStep = 0;
								voice->adsrExpFlag = 0;
								break;
							case 20:
								spuStartReleasePhase(voice);
								nextPeriod = voice->adsrPeriod;
								break;
							default:
								break;
							}

							if (enterExpSlow) {
								uint32_t stepAbs = (uint16_t)voice->adsrStep;
								voice->adsrPhase &= (uint8_t)~3U;
								voice->adsrTarget = 0x7FFFU;
								if (stepAbs <= 15U) {
									if ((stepAbs & 8U) != 0U) {
										nextPeriod = 2U;
									} else {
										nextPeriod = 4U;
										stepAbs <<= 1;
									}
									voice->adsrPeriod = nextPeriod;
								}
								voice->adsrStep = (int16_t)(stepAbs >> 2);
							}
						}

						voice->adsrLevel = clampedLevel;
						shared->voices[voiceIdx].currentAdsr =
							(voice->mutedFlag != 0) ? 0U : (uint16_t)nextVal;
						voice->adsrCounter = nextPeriod;
					}
				}

				/* Pitch advance + ADPCM block decode */
				{
					int32_t pitch = voice->pitchReg;
					int32_t phase;

					if (isPitchMod)
						pitch = (pitch * prevVoiceMod) >> 15;
					if (pitch > 0x3FFF)
						pitch = 0x3FFF;
					phase = voice->pitchPhase + pitch;

					if (phase >= 0) {
						uint16_t blkAddr = voice->currentAddr;
						uint8_t *blkPtr;

						if ((voice->blockFlags & 1U) != 0U) {
							g_spuState.endxStatus |= bit;
							blkAddr = voice->repeatAddr;
							if ((voice->blockFlags & 2U) == 0U) {
								voice->mutedFlag = 1;
								shared->voices[voiceIdx].currentAdsr = 0U;
							}
						}

						blkPtr = shared->spuRam + ((uint32_t)blkAddr * 8U);
						if ((uint32_t)(g_spuState.irqTargetPtr - blkPtr) <= 15U) {
							g_spuState.irqHitFlag = 1U;
							if (syncSnap.irqAddrMode != 0U) {
								uint16_t prevAddr = g_spuState.noiseFeedbackAddr;
								if (prevAddr != ctrlSnap.irqAddr)
									g_spuState.noiseFeedbackAddr = ctrlSnap.irqAddr;
								if (prevAddr != 0U)
									shared->ctrl.irqAddr = prevAddr;
							}
						}

						if (blkPtr != g_spuState.cachedAdpcmPtr) {
							g_spuState.cachedAdpcmPtr = blkPtr;
							memcpy(adpcmBlock, blkPtr, 16U);
						}

						voice->currentAddr = (uint16_t)(blkAddr + 2U);
						if ((adpcmBlock[1] & 6U) == 6U && voice->repeatLocked == 0U) {
							voice->repeatAddr = blkAddr;
							shared->voices[voiceIdx].repeatAddr = blkAddr;
						}
						voice->blockFlags = adpcmBlock[1] & 7U;

						if (voice->mutedFlag <= 0) {
							uint32_t filter = adpcmBlock[0] >> 4;
							int shift = (int)(adpcmBlock[0] & 0xFU);
							int b;

							voice->decodedPcm[0] = voice->decodedPcm[28];
							voice->decodedPcm[1] = voice->decodedPcm[29];
							voice->decodedPcm[2] = voice->decodedPcm[30];

							if (filter == 0U || filter > 4U) {
								for (b = 0; b < 14; b++) {
									uint8_t byteVal = adpcmBlock[2 + b];
									int32_t lo = ((int32_t)((uint32_t)(byteVal & 0x0FU) << 28)) >> (shift + 16);
									int32_t hi = ((int32_t)((uint32_t)(byteVal & 0xF0U) << 24)) >> (shift + 16);
									voice->decodedPcm[3 + b * 2] = (int16_t)lo;
									voice->decodedPcm[4 + b * 2] = (int16_t)hi;
								}
							} else {
								int16_t posC = g_spuState.vagPosCoeff[filter];
								int16_t negC = g_spuState.vagNegCoeff[filter];
								int16_t sPrevOld = voice->decodedPcm[1];
								int16_t sPrevNew = voice->decodedPcm[2];

								for (b = 0; b < 14; b++) {
									uint8_t byteVal = adpcmBlock[2 + b];
									int32_t lo = ((int32_t)((uint32_t)(byteVal & 0x0FU) << 28)) >> (shift + 10);
									int32_t hi = ((int32_t)((uint32_t)(byteVal & 0xF0U) << 24)) >> (shift + 10);
									sPrevOld = spuClamp16((sPrevNew * posC + lo - sPrevOld * negC) >> 6);
									voice->decodedPcm[3 + b * 2] = sPrevOld;
									sPrevNew = spuClamp16((sPrevOld * posC + hi - sPrevNew * negC) >> 6);
									voice->decodedPcm[4 + b * 2] = sPrevNew;
								}
							}
						}
						phase -= SPU_BLOCK_CYCLE_STEP;
					}

					if (isNoise) {
						sampleOut = (voice->adsrLevel * noiseVal) >> 15;
						prevVoiceMod = sampleOut + 0x8000;
					} else if (voice->mutedFlag != 0) {
						sampleOut = 0;
						prevVoiceMod = 0x8000;
					} else {
						int idx = 28 + (phase >> 12);
						int gIdx = ((uint8_t)((uint32_t)phase >> 4)) * 4;
						int32_t interp =
							(voice->decodedPcm[idx]     * g_spuState.gaussTable[gIdx]     +
							 voice->decodedPcm[idx + 1] * g_spuState.gaussTable[gIdx + 1] +
							 voice->decodedPcm[idx + 2] * g_spuState.gaussTable[gIdx + 2] +
							 voice->decodedPcm[idx + 3] * g_spuState.gaussTable[gIdx + 3]) >> 15;
						sampleOut = (voice->adsrLevel * interp) >> 15;
						prevVoiceMod = sampleOut + 0x8000;
					}
					voice->pitchPhase = phase;
				}
			}

			/* Voice volume sweep, capture buffer write, and stereo accumulate */
			{
				int16_t volL = spuStepSweep(&voice->sweepL, (uint16_t)voice->cachedVolLr);
				int16_t volR = spuStepSweep(&voice->sweepR, (uint16_t)(voice->cachedVolLr >> 16));
				int32_t mixL = (volL * sampleOut) >> 15;
				int32_t mixR = (volR * sampleOut) >> 15;

				if (voiceIdx == 1 || voiceIdx == 3) {
					int16_t *capBuf = (int16_t *)(shared->spuRam + 0x800U +
					                              (uint32_t)((voiceIdx == 3) ? 0x400U : 0U));
					capBuf[g_spuState.captureWritePos] = (int16_t)sampleOut;
				}

				dryLeft += mixL;
				dryRight += mixR;
				if (isReverb) {
					revLeft += mixL;
					revRight += mixR;
				}

				shared->voiceCurrentVolLr[voiceIdx] =
					((uint32_t)(uint16_t)volL) | ((uint32_t)(uint16_t)volR << 16);
			}
		}

		if ((spuControl & 0x4000U) == 0U) {
			dryLeft = 0;
			dryRight = 0;
			revLeft = 0;
			revRight = 0;
		}

		/* CD-DA / CD-XA audio mixing */
		{
			int16_t *cdCapL = (int16_t *)shared->spuRam;
			int16_t *cdCapR = (int16_t *)(shared->spuRam + 0x400U);
			int hasCdSample = 0;
			int32_t cdRawL = 0;
			int32_t cdRawR = 0;

			if (syncSnap.cdAudioMode <= 0) {
				if (syncSnap.cdAudioMode == -1) {
					shared->sync.cdWriteIdx = 0U;
					shared->sync.cdReadIdx = 0U;
					g_spuState.xaPitchStep = 0;
					g_spuState.cdSectorPtr = NULL;
					g_spuState.xaSampleCursor = 0;
					shared->sync.cdAudioMode = -2;
				}
			} else {
				if (g_spuState.cdSectorPtr == NULL) {
					uint8_t readIdx = syncSnap.cdReadIdx & 7U;
					if (((syncSnap.cdWriteIdx - readIdx) & 7U) != 0U) {
						void *guestSector = remotefsTranslateGuestAddr(
							shared->cdSectorGuestAddr[readIdx], 0U, 0U);
						memcpy(g_spuState.cdSectorCache, guestSector, SPU_CDDA_SECTOR_BYTES);
						g_spuState.cdSectorOffset = 0U;
						g_spuState.cdSectorPtr = g_spuState.cdSectorCache;
					}
				}

				if (g_spuState.cdSectorPtr != NULL) {
					hasCdSample = 1;
					if (syncSnap.cdAudioMode == 1) {
						uint16_t off = g_spuState.cdSectorOffset;
						const int16_t *pcm = (const int16_t *)(g_spuState.cdSectorPtr + off);
						cdRawL = pcm[0];
						cdRawR = pcm[1];
						g_spuState.cdSectorOffset = (uint16_t)(off + 4U);
						if (off == 2348U) {
							g_spuState.cdSectorPtr = NULL;
							shared->sync.cdReadIdx = (uint8_t)(syncSnap.cdReadIdx + 1U);
						}
					} else {
						if (g_spuState.xaSampleCursor == 0) {
							uint16_t off = g_spuState.cdSectorOffset;
							uint8_t coding = g_spuState.cdSectorPtr[19];
							spuDecodeXaSector(g_spuState.cdSectorPtr + SPU_XA_HEADER_BYTES + off, coding);
							off = (uint16_t)(off + (g_spuState.xaStereoFlag ? 256U : 128U));
							g_spuState.cdSectorOffset = off;
							g_spuState.xaSampleCursor = -224;
							if (off == SPU_XA_SECTOR_BYTES)
								shared->sync.cdReadIdx = (uint8_t)(syncSnap.cdReadIdx + 1U);
						}

						{
							int base = 224 + g_spuState.xaSampleCursor;
							int filtIdx = g_spuState.xaResamplePhase / 2;
							const int16_t *taps = &g_spuState.xaFilterTable[filtIdx];
							int32_t accL =
								g_spuState.xaRingLeft[base]     * taps[0] +
								g_spuState.xaRingLeft[base + 1] * taps[1] +
								g_spuState.xaRingLeft[base + 2] * taps[2] +
								g_spuState.xaRingLeft[base + 3] * taps[3] +
								g_spuState.xaRingLeft[base + 4] * taps[4];
							cdRawL = accL >> 15;
							if (g_spuState.xaStereoFlag != 0U) {
								int32_t accR =
									g_spuState.xaRingRight[base]     * taps[0] +
									g_spuState.xaRingRight[base + 1] * taps[1] +
									g_spuState.xaRingRight[base + 2] * taps[2] +
									g_spuState.xaRingRight[base + 3] * taps[3] +
									g_spuState.xaRingRight[base + 4] * taps[4];
								cdRawR = accR >> 15;
							} else {
								cdRawR = cdRawL;
							}

							g_spuState.xaResamplePhase -= g_spuState.xaPitchStep;
							if (g_spuState.xaResamplePhase > 69) {
								g_spuState.xaSampleCursor++;
								g_spuState.xaResamplePhase -= 70;
								if (g_spuState.cdSectorOffset + g_spuState.xaSampleCursor == SPU_XA_SECTOR_BYTES)
									g_spuState.cdSectorPtr = NULL;
							}
						}
					}
				}
			}

			if (hasCdSample) {
				int16_t mixedCdL = spuClamp16((syncSnap.cdMixMatrix[0] * cdRawL +
				                               syncSnap.cdMixMatrix[3] * cdRawR) >> 7);
				int16_t mixedCdR = spuClamp16((syncSnap.cdMixMatrix[1] * cdRawL +
				                               syncSnap.cdMixMatrix[2] * cdRawR) >> 7);
				int32_t scaledCdL = (mixedCdL * ctrlSnap.cdVolL) >> 15;
				int32_t scaledCdR = (mixedCdR * ctrlSnap.cdVolR) >> 15;

				cdCapL[g_spuState.captureWritePos] = mixedCdL;
				cdCapR[g_spuState.captureWritePos] = mixedCdR;
				if ((spuControl & 1U) != 0U) {
					dryLeft += scaledCdL;
					dryRight += scaledCdR;
				}
				if ((spuControl & 4U) != 0U) {
					revLeft += scaledCdL;
					revRight += scaledCdR;
				}
			} else {
				cdCapL[g_spuState.captureWritePos] = 0;
				cdCapR[g_spuState.captureWritePos] = 0;
			}
		}

		/* Reverb configuration update if dirty */
		if ((voiceDirty & 0x80000000U) != 0U) {
			PopsSpuReverbRegs revSnap = shared->reverb;
			spuLoadReverbConfig(&revSnap, ctrlSnap.reverbWorkBase, syncSnap.hostFlags);
		}

		/* Half-rate (22050 Hz) stereo reverb processing + FIR interpolation */
		{
			int chIdx = (int)(g_spuState.captureWritePos & 1U);
			PopsSpuReverbChannel *curCh = &g_spuState.reverbCh[chIdx];
			PopsSpuReverbChannel *otherCh = &g_spuState.reverbCh[chIdx ^ 1];
			int16_t *ramEnd = (int16_t *)(shared->spuRam + SPU_RAM_SIZE);
			int16_t *workBase = g_spuState.reverbWorkBasePtr;
			int16_t *irqPtr = (int16_t *)g_spuState.irqTargetPtr;
			int irqTouched = 0;
			int32_t revInSample = (chIdx == 0) ? revLeft : revRight;
			int writeEnable = (spuControl & 0x80U) != 0U;
			int16_t *nextSameW = spuWrapReverbPtr(curCh->sameWritePtr, ramEnd, workBase);
			int16_t *nextDiffW = spuWrapReverbPtr(curCh->diffWritePtr, ramEnd, workBase);

			if (irqPtr == curCh->sameWritePtr || irqPtr == curCh->sameReadPtr ||
			    irqPtr == curCh->diffWritePtr || irqPtr == curCh->diffReadPtr)
				irqTouched = 1;

			if (writeEnable) {
				int32_t revIn = (curCh->inputGain * spuClamp16(revInSample)) >> 15;
				int32_t samePrev = *curCh->sameWritePtr;
				int32_t diffPrev = *curCh->diffWritePtr;
				int32_t sameFeed = (g_spuState.vWall * (*curCh->sameReadPtr)) >> 15;
				int32_t diffFeed = (g_spuState.vWall * (*curCh->diffReadPtr)) >> 15;

				*nextSameW = spuClamp16(samePrev + (((revIn + sameFeed - samePrev) * g_spuState.vIir) >> 15));
				*nextDiffW = spuClamp16(diffPrev + (((revIn + diffFeed - diffPrev) * g_spuState.vIir) >> 15));
			}

			if (irqPtr == nextSameW || irqPtr == nextDiffW ||
			    irqPtr == curCh->combReadPtrA || irqPtr == curCh->combReadPtrB ||
			    irqPtr == curCh->combReadPtrC || irqPtr == curCh->combReadPtrD ||
			    irqPtr == curCh->apfWritePtrA || irqPtr == curCh->apfReadPtrA ||
			    irqPtr == curCh->apfWritePtrB || irqPtr == curCh->apfReadPtrB)
				irqTouched = 1;

			{
				int64_t combSum =
					(int64_t)(*curCh->combReadPtrA) * g_spuState.vComb1 +
					(int64_t)(*curCh->combReadPtrB) * g_spuState.vComb2 +
					(int64_t)(*curCh->combReadPtrC) * g_spuState.vComb3 +
					(int64_t)(*curCh->combReadPtrD) * g_spuState.vComb4;
				int16_t apfInA = *curCh->apfReadPtrA;
				int16_t apfValA = spuClamp16((int32_t)(combSum >> 15) - ((g_spuState.vApf1 * apfInA) >> 15));
				int16_t apfInB;
				int16_t apfValB;
				int32_t revOut;
				int32_t firPrev;
				int16_t firCur;
				int16_t firOther;
				int16_t outRevL;
				int16_t outRevR;

				if (writeEnable)
					*curCh->apfWritePtrA = apfValA;

				apfInB = *curCh->apfReadPtrB;
				apfValB = spuClamp16((apfInA + ((apfValA * g_spuState.vApf1) >> 15)) -
				                     ((g_spuState.vApf2 * apfInB) >> 15));
				if (writeEnable)
					*curCh->apfWritePtrB = apfValB;

				revOut = spuClamp16(apfInB + ((apfValB * g_spuState.vApf2) >> 15));
				if (!writeEnable && ((syncSnap.hostFlags & 1U) == 0U))
					revOut = 0;

				curCh->sameWritePtr = nextSameW;
				curCh->diffWritePtr = nextDiffW;
				curCh->sameReadPtr  = spuWrapReverbPtr(curCh->sameReadPtr, ramEnd, workBase);
				curCh->diffReadPtr  = spuWrapReverbPtr(curCh->diffReadPtr, ramEnd, workBase);
				curCh->combReadPtrA = spuWrapReverbPtr(curCh->combReadPtrA, ramEnd, workBase);
				curCh->combReadPtrB = spuWrapReverbPtr(curCh->combReadPtrB, ramEnd, workBase);
				curCh->combReadPtrC = spuWrapReverbPtr(curCh->combReadPtrC, ramEnd, workBase);
				curCh->combReadPtrD = spuWrapReverbPtr(curCh->combReadPtrD, ramEnd, workBase);
				curCh->apfWritePtrA = spuWrapReverbPtr(curCh->apfWritePtrA, ramEnd, workBase);
				curCh->apfReadPtrA  = spuWrapReverbPtr(curCh->apfReadPtrA, ramEnd, workBase);
				curCh->apfWritePtrB = spuWrapReverbPtr(curCh->apfWritePtrB, ramEnd, workBase);
				curCh->apfReadPtrB  = spuWrapReverbPtr(curCh->apfReadPtrB, ramEnd, workBase);

				if (irqTouched)
					g_spuState.irqHitFlag = 1U;

				firPrev = curCh->firHistory[0];
				curCh->firHistory[0] = curCh->firHistory[1];
				curCh->firHistory[1] = curCh->firHistory[2];
				curCh->firHistory[2] = revOut;

				firCur = spuClamp16((15901 * (curCh->firHistory[0] + curCh->firHistory[1]) +
				                     418 * (revOut + firPrev)) >> 15);
				firOther = spuClamp16((4839 * (otherCh->firHistory[0] + otherCh->firHistory[1]) +
				                       22962 * otherCh->firHistory[2]) >> 15);

				if (chIdx == 0) {
					outRevL = firCur;
					outRevR = firOther;
				} else {
					outRevL = firOther;
					outRevR = firCur;
				}

				dryLeft  += (ctrlSnap.reverbOutVolL * outRevL) >> 15;
				dryRight += (ctrlSnap.reverbOutVolR * outRevR) >> 15;
			}
		}

		/* Master volume sweep and final stereo output pack */
		if (ctrlSnap.mainVolLr != g_spuState.cachedMainVolLr) {
			g_spuState.cachedMainVolLr = ctrlSnap.mainVolLr;
			spuConfigureSweep(&g_spuState.mainSweepL, (uint16_t)ctrlSnap.mainVolLr,
			                  &g_spuState.cachedMainVolLr, 0x00008000U);
			spuConfigureSweep(&g_spuState.mainSweepR, (uint16_t)(ctrlSnap.mainVolLr >> 16),
			                  &g_spuState.cachedMainVolLr, 0x80000000U);
		}

		{
			int16_t mVolL = spuStepSweep(&g_spuState.mainSweepL, (uint16_t)g_spuState.cachedMainVolLr);
			int16_t mVolR = spuStepSweep(&g_spuState.mainSweepR, (uint16_t)(g_spuState.cachedMainVolLr >> 16));
			int16_t finalL = (int16_t)((spuClamp16(dryLeft) * mVolL) >> 15);
			int16_t finalR = (int16_t)((spuClamp16(dryRight) * mVolR) >> 15);

			shared->ctrl.currentMainVolLr =
				((uint32_t)(uint16_t)mVolL) | ((uint32_t)(uint16_t)mVolR << 16);
			if ((spuControl & 0x4003U) != 0U)
				result = ((uint32_t)(uint16_t)finalL) | ((uint32_t)(uint16_t)finalR << 16);
			else
				result = 0U;
		}
	}

	/* Update SPU status register, capture index, IRQ flag, and host audio flags */
	{
		uint16_t curPos = g_spuState.captureWritePos;
		uint8_t *capPtr = shared->spuRam + ((uint32_t)curPos * 2U);
		uint16_t nextPos = (uint16_t)((curPos + 1U) & SPU_CAPTURE_MASK);

		shared->ctrl.endxMask = g_spuState.endxStatus;
		if ((((uintptr_t)g_spuState.irqTargetPtr - (uintptr_t)capPtr) & 0xFFFFF3FFU) == 0U)
			g_spuState.irqHitFlag |= 1U;

		g_spuState.captureWritePos = nextPos;
		shared->ctrl.spuStatus = (uint16_t)((spuControl & 0x3FU) |
		                                    ((g_spuState.irqHitFlag != 0U) ? 0x40U : 0U) |
		                                    (((nextPos >> 8) & 1U) << 11));
		g_popsSpuAudioFlags = (int)(syncSnap.hostFlags & 0x82U);
	}

	return result;
}

/* 0x8102eef4 */
int popsSpuReset(void)
{
	PopsSpuSharedRam *shared;
	int i;

	memset(&g_spuState, 0, sizeof(g_spuState));
	shared = (PopsSpuSharedRam *)remotefsTranslateGuestAddr(SPU_RAM_GUEST_ADDR, 0U, 0U);
	g_spuState.sharedRam = shared;
	g_spuState.noiseTimer = 1;
	g_spuState.noiseStepPattern = 0x80U;

	/* Mirror 512-entry half Gaussian table into full 1024-entry table */
	for (i = 0; i < 128; i++) {
		int16_t a = g_spuGaussHalfTable[i * 4 + 0];
		int16_t b = g_spuGaussHalfTable[i * 4 + 1];
		int16_t c = g_spuGaussHalfTable[i * 4 + 2];
		int16_t d = g_spuGaussHalfTable[i * 4 + 3];
		int revIdx = (255 - i) * 4;

		g_spuState.gaussTable[i * 4 + 0] = a;
		g_spuState.gaussTable[i * 4 + 1] = b;
		g_spuState.gaussTable[i * 4 + 2] = c;
		g_spuState.gaussTable[i * 4 + 3] = d;

		g_spuState.gaussTable[revIdx + 0] = d;
		g_spuState.gaussTable[revIdx + 1] = c;
		g_spuState.gaussTable[revIdx + 2] = b;
		g_spuState.gaussTable[revIdx + 3] = a;
	}

	sceClibMemcpy_safe(g_spuState.vagPosCoeff, g_spuVagPredictorTable, sizeof(g_spuVagPredictorTable));
	g_spuState.adsrTargetTable[0] = 0x7FFFU;
	g_spuState.adsrTargetTable[2] = 24576U;
	sceClibMemcpy_safe(g_spuState.xaFilterTable, g_spuXaFilterTable, sizeof(g_spuXaFilterTable));

	memset(shared, 0, sizeof(*shared));
	shared->sync.voiceDirtyMask = 0x80000000U;
	spuLoadReverbConfig(&shared->reverb, shared->ctrl.reverbWorkBase, shared->sync.hostFlags);

	return (int)(uintptr_t)g_spuState.reverbCh[1].apfReadPtrA;
}
