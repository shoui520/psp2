/*
 * SceAudiocodec (vs0:sys/external/audiocodec.suprx):
 * MPEG-4 HE-AAC v1 (AAC-LC + SBR) decoder.
 */

#include <stddef.h>
#include <stdint.h>

#include "audiocodec_internal.h"
#include "aac_tables.h"

#define AAC_NOINLINE __attribute__((noinline, noclone))
#define AAC_WORK_SIZE 249664u

/* Bitstream reader used by pre-parser (16-bit max read, no end bound) */
typedef struct AacPreBitReader {
	const uint8_t *start;
	const uint8_t *ptr;
	int32_t bitsLeft;
	uint32_t cache;
	uint8_t error;
	uint8_t pad[3];
} AacPreBitReader;

/* Main bitstream reader with end pointer and EOF flag */
typedef struct AacBitReader {
	const uint8_t *start;	/* +0 */
	uint8_t eof;		/* +4 */
	uint8_t pad5[3];
	const uint8_t *end;	/* +8 */
	const uint8_t *ptr;	/* +12 */
	uint32_t cache;		/* +16 */
	int32_t bitsLeft;	/* +20 */
} AacBitReader;

/* PCE channel element list */
typedef struct AacPceElemList {
	int32_t count;
	int32_t isCpe[16];
	int32_t tag[16];
} AacPceElemList;

/* Program Configuration Element (PCE), 1096 bytes */
typedef struct AacPce {
	int32_t profile;		/* +0 */
	int32_t srIdx;			/* +4 */
	AacPceElemList front;		/* +8 (33 dwords = 132B) */
	AacPceElemList side;		/* +140 */
	AacPceElemList back;		/* +272 */
	AacPceElemList lfe;		/* +404 */
	AacPceElemList data;		/* +536 */
	AacPceElemList cc;		/* +668 */
	int32_t monoMixPresent;		/* +800 */
	int32_t monoMixElem;		/* +804 */
	int32_t reservedMonoMix;	/* +808 */
	int32_t stereoMixPresent;	/* +812 */
	int32_t stereoMixElem;		/* +816 */
	int32_t reservedStereoMix;	/* +820 */
	int32_t matrixMixPresent;	/* +824 */
	int32_t matrixMixIdx;		/* +828 */
	int32_t pseudoSurround;		/* +832 */
	uint8_t comment[260];		/* +836..+1095 */
} AacPce;

/* ADTS header fields */
typedef struct AacAdtsHeader {
	int32_t syncword;
	int32_t id;
	int32_t layer;
	int32_t protectionAbsent;
	int32_t profile;
	int32_t srIdx;
	int32_t privateBit;
	int32_t chanCfg;
	int32_t originalCopy;
	int32_t home;
	int32_t copyrightBit;
	int32_t copyrightStart;
	int32_t frameLength;
	int32_t bufferFullness;
	int32_t numRawBlocks;
	int32_t crcCheck;
	int32_t pceTag;
} AacAdtsHeader;

/* ADIF header fields */
typedef struct AacAdifHeader {
	uint8_t id[5];			/* +0 */
	uint8_t reservedId[3];
	int32_t copyIdPresent;		/* +8 */
	uint8_t copyId[10];		/* +12..+21 */
	uint8_t reservedCopyId[2];
	int32_t originalCopy;		/* +24 */
	int32_t home;			/* +28 */
	int32_t bitstreamType;		/* +32 */
	int32_t bitrate;		/* +36 */
	int32_t bufferFullness;		/* +40 */
} AacAdifHeader;

/* Channel slot descriptor (36 bytes) */
typedef struct AacChanSlot {
	int32_t active;		/* +0 */
	int32_t tag;		/* +4 */
	int32_t isCpe;		/* +8 */
	int32_t commonWin;	/* +12 */
	int32_t isLeft;		/* +16 */
	int32_t pairIdx;	/* +20 */
	int32_t winOwner;	/* +24 */
	int32_t reservedSlotA;	/* +28 */
	int32_t reservedSlotB;	/* +32 */
} AacChanSlot;

/* Channel configuration / map (292 bytes starting at core+59716) */
typedef struct AacChanMap {
	int32_t numCh;			/* +0 */
	int32_t numFrontSce;		/* +4 */
	int32_t numFront;		/* +8 */
	int32_t numSide;		/* +12 */
	int32_t numBack;		/* +16 */
	int32_t numLfe;			/* +20 */
	int32_t numAssoc;		/* +24 */
	uint32_t reservedMap[32];	/* +28..+155 */
	int32_t profile;		/* +156 */
	int32_t srIdx;			/* +160 */
	int32_t sampleRate;		/* +164 */
	float sbrRateRatio;		/* +168 */
	int32_t sbrDownsample;		/* +172 */
	int32_t sbrLevel;		/* +176 */
	int32_t sbrEnable;		/* +180 */
	AacChanSlot slots[3];		/* +184..+291 */
} AacChanMap;

/* Huffman codebook descriptor (36 bytes) */
typedef struct AacHuffBook {
	int32_t dim;			/* +0 */
	int32_t lavRange;		/* +4 */
	int32_t offset;			/* +8 */
	int32_t isSigned;		/* +12 */
	float invLav;			/* +16 */
	float invLav2;			/* +20 */
	float invLav3;			/* +24 */
	const void *cb;			/* +28 */
	const AacHuffLutEntry *lut;	/* +32 */
} AacHuffBook;

/* Window / SFB layout info (584 bytes = 0x248) */
typedef struct AacWindowInfo {
	int32_t isLong;			/* +0 */
	int32_t numWin;			/* +4 */
	int32_t binsPerFrame;		/* +8 */
	int32_t totalSfb;		/* +12 */
	int32_t binsPerWin[8];		/* +16..+47 */
	int32_t numSfb[8];		/* +48..+79 */
	int32_t sectLenBits[8];		/* +80..+111 */
	const int16_t *sfbTable[8];	/* +112..+143 */
	const int16_t *sfbWidth;	/* +144 */
	int16_t sfbOffsets[200];	/* +148..+547 */
	int32_t numGroups;		/* +548 */
	int16_t groupLen[8];		/* +552..+567 */
	int16_t reservedWinTail[8];	/* +568..+583 */
} AacWindowInfo;

/* TNS single filter (72 bytes) */
typedef struct AacTnsFilter {
	int32_t startBand;		/* +0 */
	int32_t stopBand;		/* +4 */
	uint8_t order;			/* +8 */
	uint8_t direction;		/* +9 */
	int16_t coef[31];		/* +10..+71 */
} AacTnsFilter;

/* TNS per-window info (220 bytes) */
typedef struct AacTnsWin {
	uint8_t nFilt;			/* +0 */
	uint8_t coefRes;		/* +1 */
	uint8_t pad2[2];
	AacTnsFilter filt[3];		/* +4..+219 */
} AacTnsWin;

/* TNS per-channel frame (1764 bytes) */
typedef struct AacTnsFrame {
	int32_t numWin;			/* +0 */
	AacTnsWin win[8];		/* +4..+1763 */
} AacTnsFrame;

/* DRC parsed packet (240 bytes) */
typedef struct AacDrcPacket {
	int32_t excludedPresent;	/* +0 */
	int32_t excludedMask[7];	/* +4..+31 */
	int32_t numBands;		/* +32 */
	int32_t bandTop[16];		/* +36..+99 */
	int32_t progRefPresent;		/* +100 */
	int32_t progRefLevel;		/* +104 */
	int32_t dynRangeSgn[16];	/* +108..+171 */
	int32_t dynRangeCtl[16];	/* +172..+235 */
	int32_t bandIncr;		/* +236 */
} AacDrcPacket;

/* DRC per-channel active state (204 bytes) */
typedef struct AacDrcChan {
	int32_t numBands;		/* +0 */
	int32_t bandTop[16];		/* +4..+67 */
	int32_t dynRangeSgn[16];	/* +68..+131 */
	int32_t dynRangeCtl[16];	/* +132..+195 */
	int32_t bandIncr;		/* +196 */
	int32_t prevBandIncr;		/* +200 */
} AacDrcChan;

/* DRC controller state (1360 bytes at core+61244) */
typedef struct AacDrcState {
	int32_t enable;			/* +0 (core+61244) */
	int32_t useProgRef;		/* +4 */
	int32_t targetRef;		/* +8 */
	float cut;			/* +12 */
	float boost;			/* +16 */
	int32_t progRef;		/* +20 */
	int32_t numPkts;		/* +24 (core+61268) */
	AacDrcPacket pkts[3];		/* +28..+747 (core+61272) */
	AacDrcChan chans[3];		/* +748..+1359 (core+61992) */
} AacDrcState;

/* Pulse data (44 bytes at core+65664) */
typedef struct AacPulseData {
	int32_t present;
	int32_t numPulse;
	int32_t startSfb;
	int32_t offset[4];
	int32_t amp[4];
} AacPulseData;

/* SBR per-element header & frequency tables (496 bytes) */
typedef struct SbrElement {
	uint8_t pad0[4];		/* +0 */
	uint8_t headerFlag;		/* +4 */
	uint8_t ampRes;			/* +5 */
	uint8_t startFreq;		/* +6 */
	uint8_t stopFreq;		/* +7 */
	uint8_t xoverBand;		/* +8 */
	uint8_t freqScale;		/* +9 */
	uint8_t alterScale;		/* +10 */
	uint8_t noiseBands;		/* +11 */
	uint8_t limiterBands;		/* +12 */
	uint8_t limiterGains;		/* +13 */
	uint8_t interpFreq;		/* +14 */
	uint8_t smoothingMode;		/* +15 */
	uint8_t coupling;		/* +16 */
	uint8_t nMaster;		/* +17 */
	uint8_t fMaster[64];		/* +18..+81 */
	uint8_t nHigh;			/* +82 */
	uint8_t nLow;			/* +83 */
	uint8_t nFreq[2];		/* +84..+85 ([0]=low, [1]=high) */
	uint8_t fTable[2][64];		/* +86..+213 ([0]=low at +86, [1]=high at +150) */
	uint8_t mBand;			/* +214 */
	uint8_t kx;			/* +215 */
	uint8_t k0;			/* +216 */
	uint8_t k2;			/* +217 */
	uint8_t fTableNoise[64];	/* +218..+281 */
	uint8_t fTableLim[64];		/* +282..+345 */
	uint8_t nLim;			/* +346 */
	uint8_t numTimeSlots;		/* +347 */
	uint8_t numTimeSlots2;		/* +348 */
	uint8_t numPatches;		/* +349 */
	uint8_t patchNumSubbands[64];	/* +350..+413 */
	uint8_t patchStartSubband[64];	/* +414..+477 */
	uint8_t tHfAdj;			/* +478 (2) */
	uint8_t tHfGen;			/* +479 (8) */
	uint8_t reservedHf;		/* +480 */
	uint8_t extType;		/* +481 (0) */
	uint8_t extId;			/* +482 */
	uint8_t extData;		/* +483 */
	uint8_t elemType;		/* +484 (0=SCE, 1=CPE) */
	uint8_t prevStartFreq;		/* +485 */
	uint8_t prevStopFreq;		/* +486 */
	uint8_t prevXoverBand;		/* +487 */
	uint8_t prevFreqScale;		/* +488 */
	uint8_t prevAlterScale;		/* +489 */
	uint8_t prevNoiseBands;		/* +490 */
	uint8_t kxPrev;			/* +491 */
	uint8_t mPrev;			/* +492 */
	uint8_t reservedElemTail[3];	/* +493..+495 */
} SbrElement;

/* SBR QMF working buffer (31744 bytes at core+79104) */
typedef struct SbrQmfWork {
	float alpha0[64][2];		/* +0 (512B) */
	float alpha1[64][2];		/* +512 (512B) */
	float xLow[40][32][2];		/* +1024 (10240B) */
	float xHigh[40][64][2];		/* +11264 (20480B) */
} SbrQmfWork;

/* SBR per-channel state (18152 bytes) */
typedef struct SbrChannel {
	uint8_t isSecondCh;		/* +0 */
	uint8_t addHarmonicFlag;	/* +1 */
	uint8_t frameClass;		/* +2 */
	uint8_t numEnv;			/* +3 */
	uint8_t freqRes[8];		/* +4..+11 */
	uint8_t pointer;		/* +12 */
	uint8_t varBord0;		/* +13 */
	uint8_t varBord1;		/* +14 */
	uint8_t numRel0;		/* +15 */
	uint8_t numRel1;		/* +16 */
	uint8_t relBord0[3];		/* +17..+19 */
	uint8_t relBord1[3];		/* +20..+22 */
	uint8_t numNoise;		/* +23 */
	uint8_t dfEnv[8];		/* +24..+31 */
	uint8_t dfNoise[3];		/* +32..+34 */
	uint8_t invfMode[5];		/* +35..+39 */
	uint8_t addHarmonic[64];	/* +40..+103 */
	uint8_t reservedHarm[4];	/* +104..+107 */
	uint8_t le;			/* +108 */
	uint8_t lq;			/* +109 */
	uint8_t nq;			/* +110 */
	uint8_t reservedGrid[9];	/* +111..+119 */
	uint8_t tE[6];			/* +120..+125 */
	uint8_t tQ[3];			/* +126..+128 */
	uint8_t ampRes;			/* +129 */
	uint8_t reservedAmp[2];		/* +130..+131 */
	int32_t envRaw[5][64];		/* +132..+1411 */
	float *envOrig;			/* +1412 */
	int32_t noiseRaw[5][64];	/* +1416..+2695 */
	float *noiseOrig;		/* +2696 */
	int8_t lA;			/* +2700 */
	uint8_t reservedLa[3];
	float qmfSynthDelay[1152];	/* +2704..+7311 */
	float qmfAnalDelay[288];	/* +7312..+8463 */
	SbrQmfWork *qmfWork;		/* +8464 */
	int32_t headerReady;		/* +8468 */
	uint8_t prevInvfMode[5];	/* +8472..+8476 */
	uint8_t prevAddHarmonic[64];	/* +8477..+8540 */
	uint8_t prevAddHarmonicFlag;	/* +8541 */
	uint8_t prevLe;			/* +8542 */
	uint8_t prevLq;			/* +8543 */
	uint8_t reservedPrevGrid[6];	/* +8544..+8549 */
	uint8_t prevTe[6];		/* +8550..+8555 */
	int32_t prevEnvRaw[64];		/* +8556..+8811 */
	int32_t prevNoiseRaw[64];	/* +8812..+9067 */
	int8_t prevLa;			/* +9068 */
	uint8_t reservedPrevLa[3];
	float prevBwArray[64];		/* +9072..+9327 */
	int32_t noiseIndex;		/* +9328 */
	uint8_t sinIndex;		/* +9332 */
	uint8_t prevFreqRes[8];		/* +9333..+9340 */
	uint8_t prevSines[64];		/* +9341..+9404 */
	uint8_t reservedSines[3];
	float gTempHist[5][64];		/* +9408..+10687 */
	float qTempHist[5][64];		/* +10688..+11967 */
	float *gTempPtr[5];		/* +11968..+11987 */
	float *qTempPtr[5];		/* +11988..+12007 */
	float xLowSave[8][32][2];	/* +12008..+14055 */
	float xHighSave[8][64][2];	/* +14056..+18151 */
} SbrChannel;

/* Overlay scratch area at core+66304 (65192 bytes) */
typedef struct SbrScratchMem {
	union {
		struct {
			int32_t specInt[1024];		/* +0 (core+66304) */
			int32_t specDeint[1024];	/* +4096 (core+70400) */
		} spec;
		struct {
			float fftBuf[1024];		/* +0 (core+66304) */
			float shortWork[1152];		/* +4096 (core+70400) */
		} imdct;
		struct {
			int32_t bandDeltaLo[64];	/* +0 (core+66304) */
			int32_t bandEdgeLo[64];		/* +256 (core+66560) */
			int32_t bandEdgeHi[64];		/* +512 (core+66816) */
			int32_t bandDeltaHi[64];	/* +768 (core+67072) */
		} freq;
		struct {
			float eLimSum[5][64];		/* +0 (core+66304) */
			float gMaxBuf[5][64];		/* +1280 (core+67584) */
		} lim;
		struct {
			float analWork[1312];		/* +0 (core+66304..+71551) */
			float pad[736];
			float analRe[64];		/* +8192 (core+74496) */
			float analIm[64];		/* +8448 (core+74752) */
		} qmfAnal;
		struct {
			float synth64Work[5248];	/* +0 (core+66304..+87295) */
			float synth64Re[128];		/* +20992 (core+87296) */
			float synth64Im[128];		/* +21504 (core+87808) */
		} qmfSynth64;
		struct {
			float synth32Work[2624];	/* +0 (core+66304..+76799) */
			float synth32Re[64];		/* +10496 (core+76800) */
			float synth32Im[64];		/* +10752 (core+77056) */
		} qmfSynth32;
		struct {
			uint8_t reservedLim[2560];	/* +0..+2559 */
			float envRefBuf[5][64];		/* +2560 (core+68864) */
			float noiseRefBuf[5][64];	/* +3840 (core+70144) */
			float envCurrBuf[5][64];	/* +5120 (core+71424) */
			float gainLimBuf[5][64];	/* +6400 (core+72704) */
			float noiseLimBuf[5][64];	/* +7680 (core+73984) */
			float sineMagBuf[5][64];	/* +8960 (core+75264) */
			float noiseMagBuf[5][64];	/* +10240 (core+76544) */
			float gainRawBuf[5][64];	/* +11520 (core+77824) */
			SbrQmfWork qmfWork;		/* +12800 (core+79104..+110847) */
			float *bufPtrs[8][5];		/* +44544 (core+110848..+111007) */
			float qmfOut[32][64][2];	/* +44704 (core+111008..+127391) */
		} sbr;
	} u;
	uint8_t tailPad[4104];				/* core+127392..+131495 */
} SbrScratchMem;

/* Core AAC+SBR state (238080 bytes = 0x3A200 at AacDecCtx+128) */
typedef struct AacCore {
	int32_t firstFrame;			/* +0 */
	int32_t prevFrameFirst;			/* +4 */
	int32_t outputReady;			/* +8 */
	uint8_t copyrightBuf[9];		/* +12..+20 */
	uint8_t reservedCopyright[3];
	int32_t copyrightBits;			/* +24 */
	uint8_t crcErrFlag;			/* +28 */
	uint8_t prevChanCfg;			/* +29 */
	uint8_t hasPceFlag;			/* +30 */
	uint8_t reservedFrameFlags;
	float *specPtr[3];			/* +32..+43 */
	float *pcmPtr[3];			/* +44..+55 */
	float overlapBuf[3][1024];		/* +56..+12343 */
	uint8_t msMaskPresent[3];		/* +12344..+12346 */
	uint8_t msUsed[3][128];			/* +12347..+12730 */
	uint8_t winGroupEnd[3][8];		/* +12731..+12754 */
	uint8_t winSeq[3];			/* +12755..+12757 */
	uint8_t maxSfb[3];			/* +12758..+12760 */
	uint8_t sfbCb[3][128];			/* +12761..+13144 */
	uint8_t dseBuf[512];			/* +13145..+13656 */
	uint8_t winShapePrev[3][2];		/* +13657..+13662 */
	uint8_t winShapeCur[3][2];		/* +13663..+13668 */
	uint8_t reservedWinShape;
	int16_t scaleFactors[3][128];		/* +13670..+14437 */
	uint8_t reservedSfAlign[2];
	AacTnsFrame tns[3];			/* +14440..+19731 */
	uint8_t reservedTnsTail[12];
	int32_t adtsRawBlockIdx;		/* +19744 */
	uint8_t reservedAdtsTail[28];
	float chanBuf[3][2048];			/* +19776..+44351 */
	uint8_t reservedChanAlign[4];
	float sinWinLong[1024];			/* +44356..+48451 */
	float sinWinShort[128];			/* +48452..+48963 */
	float transWinSin[1024];		/* +48964..+53059 */
	float transWinKbd[1024];		/* +53060..+57155 */
	const float *winTables[6];		/* +57156..+57179 */
	int32_t cpeSeen;			/* +57180 */
	uint8_t reservedCpeGap[184];
	AacChanSlot prevChanSlots[3];		/* +57368..+57475 */
	float sfPowTab[128];			/* +57476..+57987 */
	int32_t maxSfHalf;			/* +57988 */
	uint8_t reservedHuffPre[36];
	AacHuffBook specHuff[11];		/* +58028..+58423 */
	uint8_t reservedHuffMid[36];
	AacHuffBook sfHuff;			/* +58460..+58495 */
	int32_t useDefaultConfig;		/* +58496 */
	int32_t adifMode;			/* +58500 */
	int32_t adtsMode;			/* +58504 */
	AacAdtsHeader adtsHdr;			/* +58508..+58575 */
	AacAdifHeader adifHdr;			/* +58576..+58619 */
	AacPce pce;				/* +58620..+59715 */
	AacChanMap chanMap;			/* +59716..+60007 */
	AacWindowInfo *winInfoTbl[4];		/* +60008..+60023 */
	AacWindowInfo longWinInfo;		/* +60024..+60607 */
	AacWindowInfo shortWinInfo;		/* +60608..+61191 */
	AacWindowInfo *winInfoBySeq[4];		/* +61192..+61207 */
	int16_t shortSfbWidth[16];		/* +61208..+61239 */
	uint32_t crcReg;			/* +61240 */
	AacDrcState drc;			/* +61244..+62603 */
	uint8_t reservedDrcTail[3060];
	AacPulseData pulse;			/* +65664..+65707 */
	int32_t secErr;				/* +65708 */
	uint32_t pnsState;			/* +65712 */
	int32_t pnsBandSeed[128];		/* +65716..+66227 */
	int32_t hasPns;				/* +66228 */
	AacBitReader br;			/* +66232..+66255 */
	float *curPcmOut;			/* +66256 */
	uint8_t reservedScratchPre[44];
	SbrScratchMem scratch;			/* +66304..+131495 */
	uint8_t reservedSbrPre[3];
	uint8_t sbrResetFlag;			/* +131499 */
	uint32_t sbrSampleRate;			/* +131500 */
	uint8_t sbrSrIdx;			/* +131504 */
	uint8_t reservedSbrSr[3];
	uint8_t curElemChan;			/* +131508 */
	uint8_t numSbrElems;			/* +131509 */
	uint8_t sbrSines[5][64];		/* +131510..+131829 */
	uint8_t sbrSinesBand[5][64];		/* +131830..+132149 */
	uint8_t reservedSbrBw[2];
	float sbrBwArray[64];			/* +132152..+132407 */
	SbrElement sbrElems[4];			/* +132408..+134391 */
	SbrChannel sbrChans[3];			/* +134392..+188847 */
	SbrChannel *sbrChanPtr[3];		/* +188848..+188859 */
	uint8_t sbrChanToElem[4];		/* +188860..+188863 */
	int32_t sbrActiveFlag;			/* +188864 */
	int32_t sbrCurChanIdx;			/* +188868 */
	uint8_t reservedDrcQmf[8];
	float drcQmfGain[3][64][64];		/* +188880..+238031 */
	int32_t outSamplesPerFrame;		/* +238032 */
	uint8_t reservedCoreTail[44];		/* +238036..+238079 */
} AacCore;

/* Full decoder context (238208 bytes = 0x3A280 at AacWork+1216) */
typedef struct AacDecCtx {
	int32_t isAdif;			/* +0 */
	int32_t adifPceTag;		/* +4 */
	int32_t isAdts;			/* +8 */
	int32_t pcmFormat;		/* +12 */
	float drcCut;			/* +16 */
	float drcBoost;			/* +20 */
	int32_t reservedDrc;		/* +24 */
	int32_t drcTargetRef;		/* +28 */
	int32_t initSrIdx;		/* +32 */
	uint8_t downmixMode;		/* +36 */
	uint8_t out3chMode;		/* +37 */
	uint8_t outputDelayMode;	/* +38 */
	uint8_t parseOnlyFlag;		/* +39 */
	int32_t sbrEnable;		/* +40 */
	int32_t sbrDownsample;		/* +44 */
	uint32_t version;		/* +48 (4100) */
	uint32_t magic;			/* +52 (154075136) */
	uint32_t frameCountHi;		/* +56 */
	uint32_t frameCountLo;		/* +60 */
	uint32_t bitrate;		/* +64 */
	uint32_t outputBytes;		/* +68 */
	uint32_t bitsConsumed;		/* +72 */
	int32_t internalError;		/* +76 */
	uint8_t infoChanged;		/* +80 */
	uint8_t statusOk;		/* +81 */
	uint8_t numCh;			/* +82 */
	uint8_t srIdx;			/* +83 */
	uint8_t outPcmFmt;		/* +84 */
	uint8_t outNumCh;		/* +85 */
	uint8_t isMonoOut;		/* +86 */
	uint8_t reservedMono;		/* +87 */
	uint8_t streamType;		/* +88 */
	uint8_t hasData;		/* +89 */
	uint8_t originalCopy;		/* +90 */
	uint8_t hasCopyright;		/* +91 */
	uint8_t copyrightId[9];		/* +92..+100 */
	uint8_t chanCfgError;		/* +101 */
	uint8_t chanCfg;		/* +102 */
	uint8_t hasPce;			/* +103 */
	uint8_t sbrActive;		/* +104 */
	uint8_t outSrIdx;		/* +105 */
	uint8_t adifBitstreamType;	/* +106 */
	uint8_t reservedAdif;		/* +107 */
	uint16_t adtsFrameBytes;	/* +108 */
	uint8_t adtsId;			/* +110 */
	uint8_t adtsProtAbsent;		/* +111 */
	uint8_t adtsChanCfg;		/* +112 */
	uint8_t crcError;		/* +113 */
	uint8_t adtsHome;		/* +114 */
	uint8_t adtsCopyrightBit;	/* +115 */
	uint8_t adtsCopyrightStart;	/* +116 */
	uint8_t reservedDecTail[11];	/* +117..+127 */
	AacCore core;			/* +128..+238207 */
} AacDecCtx;

/* Public status / configuration block (1200 bytes = 0x4B0 at AacWork+0) */
typedef struct AacInfo {
	uint32_t headerWords[10];	/* +0..+39 */
	uint8_t reservedInfoPre[116];	/* +40..+155 */
	uint8_t streamStatus[30];	/* +156..+185 */
	uint8_t reservedInfoMid[778];	/* +186..+963 */
	uint8_t pcmFormat;		/* +964 */
	uint8_t downmixMode;		/* +965 */
	uint8_t reservedInfoCfg[54];	/* +966..+1019 */
	uint8_t streamType;		/* +1020 */
	uint8_t srIdx;			/* +1021 */
	uint8_t reservedInfoSr[2];	/* +1022..+1023 */
	int16_t drcTargetRef;		/* +1024 */
	uint16_t drcCut;		/* +1026 */
	uint16_t drcBoost;		/* +1028 */
	uint8_t out3chMode;		/* +1030 */
	uint8_t outputDelayMode;	/* +1031 */
	uint8_t parseOnlyFlag;		/* +1032 */
	uint8_t sbrEnable;		/* +1033 */
	uint8_t sbrDownsample;		/* +1034 */
	uint8_t reservedInfoTail[165];	/* +1035..+1199 */
} AacInfo;

/* Top-level work memory buffer */
typedef struct AacWork {
	AacInfo info;			/* +0..+1199 */
	uint8_t reservedWorkGap[16];	/* +1200..+1215 */
	AacDecCtx dec;			/* +1216..+239423 */
} AacWork;

_Static_assert(sizeof(AacPce) == 1096, "AacPce size");
_Static_assert(sizeof(AacChanSlot) == 36, "AacChanSlot size");
_Static_assert(sizeof(AacHuffBook) == 36, "AacHuffBook size");
_Static_assert(sizeof(AacWindowInfo) == 584, "AacWindowInfo size");
_Static_assert(sizeof(AacTnsFilter) == 72, "AacTnsFilter size");
_Static_assert(sizeof(AacTnsWin) == 220, "AacTnsWin size");
_Static_assert(sizeof(AacTnsFrame) == 1764, "AacTnsFrame size");
_Static_assert(sizeof(AacDrcPacket) == 240, "AacDrcPacket size");
_Static_assert(sizeof(AacDrcChan) == 204, "AacDrcChan size");
_Static_assert(sizeof(SbrElement) == 496, "SbrElement size");
_Static_assert(sizeof(SbrQmfWork) == 31744, "SbrQmfWork size");
_Static_assert(sizeof(SbrChannel) == 18152, "SbrChannel size");
_Static_assert(offsetof(AacCore, overlapBuf) == 56, "overlapBuf offset");
_Static_assert(offsetof(AacCore, tns) == 14440, "tns offset");
_Static_assert(offsetof(AacCore, chanBuf) == 19776, "chanBuf offset");
_Static_assert(offsetof(AacCore, sinWinLong) == 44356, "sinWinLong offset");
_Static_assert(offsetof(AacCore, winTables) == 57156, "winTables offset");
_Static_assert(offsetof(AacCore, sfPowTab) == 57476, "sfPowTab offset");
_Static_assert(offsetof(AacCore, specHuff) == 58028, "specHuff offset");
_Static_assert(offsetof(AacCore, sfHuff) == 58460, "sfHuff offset");
_Static_assert(offsetof(AacCore, pce) == 58620, "pce offset");
_Static_assert(offsetof(AacCore, chanMap) == 59716, "chanMap offset");
_Static_assert(offsetof(AacCore, longWinInfo) == 60024, "longWinInfo offset");
_Static_assert(offsetof(AacCore, winInfoBySeq) == 61192, "winInfoBySeq offset");
_Static_assert(offsetof(AacCore, drc) == 61244, "drc offset");
_Static_assert(offsetof(AacCore, pulse) == 65664, "pulse offset");
_Static_assert(offsetof(AacCore, br) == 66232, "br offset");
_Static_assert(offsetof(AacCore, scratch) == 66304, "scratch offset");
_Static_assert(offsetof(AacCore, sbrResetFlag) == 131499, "sbrResetFlag offset");
_Static_assert(offsetof(AacCore, sbrElems) == 132408, "sbrElems offset");
_Static_assert(offsetof(AacCore, sbrChans) == 134392, "sbrChans offset");
_Static_assert(offsetof(AacCore, drcQmfGain) == 188880, "drcQmfGain offset");
_Static_assert(offsetof(AacCore, outSamplesPerFrame) == 238032, "outSamplesPerFrame offset");
_Static_assert(sizeof(AacCore) == 0x3A200, "AacCore size");
_Static_assert(sizeof(AacDecCtx) == 0x3A280, "AacDecCtx size");
_Static_assert(sizeof(AacInfo) == 0x4B0, "AacInfo size");
_Static_assert(offsetof(AacWork, dec) == 1216, "AacWork.dec offset");

/* Forward declarations for internal functions */
static int32_t aacOpenCtx(AacDecCtx *dec);
static int32_t aacResetCtx(AacDecCtx *dec);
static int32_t aacInitCtx(AacDecCtx *dec);
static int32_t aacParseFrame(const uint8_t *inBuf, AacDecCtx *dec);
static int32_t aacDecodeFrame(const uint8_t *inBuf, uint8_t *outBuf, AacDecCtx *dec);
static int32_t aacSetConfig(const AacInfo *info, AacDecCtx *dec);
static int32_t aacSetOutputMode(const AacInfo *info, AacDecCtx *dec);
static uint32_t aacPreReadBits(AacPreBitReader *br, uint32_t nbits);
static void aacPreSkipPceElems(AacPceElemList *list, int32_t hasCpeBit, AacPreBitReader *br);
static int32_t aacPreparseFrame(const uint8_t *inBuf, AacDecCtx *dec);
static int32_t aacGetStatus(const AacDecCtx *dec, AacInfo *outInfo);
static int32_t sbrProcessChannel(AacCore *core, SbrElement *elem, SbrChannel *ch, float *outPcm);
static int32_t aacValidateSbrConfig(AacCore *core);
static int32_t aacInitTablesWrapper(AacCore *core);
static int32_t aacParseDse(uint32_t *elemTag, int32_t *countOut, uint8_t *dst, AacCore *core);
static int32_t aacParseFil(uint8_t *dseBuf, AacCore *core);
static void aacInitBuffers(AacCore *core);
static int32_t sbrGetSrIndex(uint32_t sr);
static void aacInitWindows(AacCore *core);
static int32_t aacAssignChannelSlot(int32_t elemType, int32_t tag, int32_t posChar,
				    int32_t commonWin, AacChanMap *map, int32_t *slotOut);
static void aacReadPceElems(AacPceElemList *list, int32_t hasCpeBit, AacCore *core);
static int32_t aacGetTnsMaxBands(int32_t isLong, const AacCore *core);
static int32_t aacGetTnsMaxOrder(int32_t isLong, const AacCore *core);
static int32_t aacParseAdtsHeader(AacCore *core);
static int32_t aacMapElementChannel(int32_t elemType, int32_t tag, int32_t commonWin,
				    AacChanMap *map, int32_t *slotOut, AacCore *core);
static void aacResetChanMap(AacChanMap *map, const AacCore *core);
static int32_t aacCheckChanConfig(const AacChanMap *map, int32_t saveFlag, AacCore *core);
static int32_t aacParsePce(AacPce *pce, int32_t *tagOut, AacCore *core);
static int32_t aacParseAdifHeader(AacCore *core);
static uint32_t aacCrc16Update(const uint8_t *startPtr, uint32_t startBit,
			       const uint8_t *endPtr, uint32_t endBit,
			       uint32_t mode, uint32_t crc);
static uint32_t aacCrc16Finish(AacCore *core);
static uint32_t sbrCrc10Calc(const uint8_t *startPtr, uint32_t startBit,
			     const uint8_t *endPtr, uint32_t endBit,
			     uint32_t mode, uint32_t crc);
static int32_t aacImdct(uint32_t winSeq, uint8_t *winShapePair, float *overlap,
			float *outPcm, AacCore *core);
static void aacSetDrcParams(int32_t targetRef, AacCore *core, float cut, float boost);
static void aacResetDrc(AacCore *core);
static void aacClearDrcPackets(AacCore *core);
static int32_t aacParseDrc(AacCore *core);
static int32_t aacResolveDrc(const AacChanMap *map, AacCore *core);
static void aacApplyDrc(float *spec, int32_t chIdx, int32_t sbrFlag, uint32_t winSeq, AacCore *core);
static int32_t aacParseIcsInfo(uint8_t *winSeq, uint8_t *winShape, uint8_t *groupEnd,
			       uint8_t *maxSfb, AacWindowInfo *winDst, AacCore *core);
static int32_t aacParseChannelElement(int32_t elemType, AacChanMap *map, AacCore *core);
static int32_t aacReadEscapeVal(int32_t val, AacCore *core);
static void aacReadWindowGrouping(const AacWindowInfo *win, uint8_t *groupEnd, AacCore *core);
static uint8_t aacReadMsMask(const AacWindowInfo *win, const uint8_t *groupEnd,
			     int32_t maxSfb, uint8_t *msUsed, AacCore *core);
static int32_t aacDecodeIcs(AacWindowInfo *win, int32_t commonWin, uint8_t *winSeq,
			    uint8_t *winShape, uint8_t *groupEnd, uint8_t *maxSfb,
			    uint8_t *sfbCb, float *specOut, int16_t *globalGain,
			    int16_t *sfOut, AacTnsFrame *tnsOut, int32_t lfeOffset, AacCore *core);
static int32_t aacByteAlign(AacCore *core);
static uint32_t aacGetBitsLong(int32_t nbits, AacCore *core);
static void aacInitSpecHuffBook(AacHuffBook *book, const void *cb, int32_t dim,
				int32_t lav, int32_t isSigned);
static void aacInitSfHuffBook(AacHuffBook *book, const void *cb, int32_t dim,
			      int32_t lav, int32_t isSigned);
static int32_t aacDecodeSfHuff(const AacSfHuffEntry *cb, AacCore *core);
static int32_t aacDecodeSpecHuff(int32_t cbIdx, const AacHuffEntry *cb,
				 const AacHuffLutEntry *lut, AacCore *core);
static void aacUnpackHuffIdx(int32_t *dst, int32_t idx, const AacHuffBook *book);
static void aacApplyHuffSigns(int32_t *vals, uint32_t count, AacCore *core);
static void aacInitCoreTables(AacCore *core);
static int32_t aacApplyIntensityStereo(const AacChanMap *map, const AacWindowInfo *win,
				       int32_t chIdx, int32_t winOwner, AacCore *core);
static void aacZeroIntBuf(void *buf, int32_t count);
static int32_t aacGenPnsNoise(float *dst, int32_t len, uint32_t *seed);
static int32_t aacApplyPns(const AacWindowInfo *win, int32_t chIdx, int32_t winOwner, AacCore *core);
static uint32_t aacGetBits(uint8_t nbits, AacCore *core);
static uint32_t aacPeek8Bits(const AacCore *core);
static uint32_t aacPeek16Bits(const AacCore *core);
static uint32_t aacPeek19Bits(const AacCore *core);
static uint32_t aacPeekBits(uint32_t nbits, AacCore *core);
static int32_t aacGetBitCount(const AacCore *core);
static void aacApplyMsStereo(const AacWindowInfo *win, const uint8_t *groupEnd,
			     const uint8_t *msUsed, float *leftSpec, float *rightSpec);
static int32_t aacResolveMsIsPns(const AacWindowInfo *win, const uint8_t *groupEnd,
				 uint8_t *msUsed, uint8_t *sfbCb, uint8_t msMaskPresent);
static int32_t aacApplyTns(float *spec, int32_t maxSfb, const int16_t *sfbOffsets,
			   int32_t isLong, const AacTnsWin *tnsWin, const AacCore *core);
static int32_t sbrReadInvf(AacCore *core, SbrChannel *ch);
static int32_t sbrReadHuff(AacCore *core, int32_t tableIdx, int32_t *outVal);
static int32_t sbrReadEnvelope(AacCore *core, const SbrElement *elem, SbrChannel *ch, int32_t isRight);
static int32_t sbrReadGrid(AacCore *core, SbrChannel *ch);
static int32_t sbrReadDtdf(AacCore *core, SbrChannel *ch);
static int32_t sbrReadNoise(AacCore *core, const uint8_t *couplingFlag, SbrChannel *ch, int32_t isRight);
static int32_t sbrReadSinusoidal(AacCore *core, const uint8_t *nHighPtr, SbrChannel *ch);
static int32_t sbrDecodeExtension(AacCore *core, int32_t countBytes, int32_t hasCrc);
static int sbrCompareInt(const void *a, const void *b);
static int32_t sbrCalcMasterFreqTable(AacCore *core, SbrElement *elem);
static int32_t sbrCalcDerivedFreqTable(AacCore *core, SbrElement *elem);
static int32_t sbrCalcNoiseFreqTable(AacCore *core, SbrElement *elem, SbrChannel *ch);
static int32_t sbrCalcLimiterFreqTable(AacCore *core, SbrElement *elem);
static int32_t sbrCalcTimeBorders(AacCore *core, const SbrElement *elem, SbrChannel *ch);
static int32_t sbrDeltaDecode(AacCore *core, const SbrElement *elem, SbrChannel *ch, int32_t isRight);
static int32_t sbrDequantUncoupled(AacCore *core, const SbrElement *elem, SbrChannel *ch);
static int32_t sbrDequantCoupled(AacCore *core, const SbrElement *elem, SbrChannel *chL, SbrChannel *chR);
static int32_t sbrHfGenerate(AacCore *core, SbrElement *elem, SbrChannel *ch);
static int32_t sbrCalculateGainAndAdjust(AacCore *core, const SbrElement *elem, SbrChannel *ch);
static int32_t sbrQmfAnalysis(AacCore *core, const SbrElement *elem, SbrChannel *ch, const float *inPcm);
static void sbrQmfSynth64(AacCore *core, const SbrElement *elem, SbrChannel *ch,
			  const float *qmfIn, float *outPcm);
static void sbrQmfSynth32(AacCore *core, const SbrElement *elem, SbrChannel *ch,
			  const float *qmfIn, float *outPcm);
static void aacMdctPreTwiddle(const float *inBuf, float *outBuf, int32_t numBlocks, int32_t mdctLen);
static void aacFftRadix4(float *buf, int32_t n);

/* =========================================================================
 * Public SceAudiocodec AAC entrypoints
 * ========================================================================= */

int32_t aacCheckNeedMem(uint32_t *work)
{
	work[1] = 0;
	work[2] = AAC_WORK_SIZE;
	work[0] = 0;
	return 0;
}

int32_t aacInit(uint32_t *work)
{
	AacWork *w = (AacWork *)work[1];

	if (aacInitCtx(&w->dec) < 0 || aacOpenCtx(&w->dec) < 0 || aacResetCtx(&w->dec) < 0)
		return AUDIOCODEC_ERROR_DECODE;

	uint8_t streamType = (uint8_t)work[8];
	if (streamType != 0 && streamType != 1 && streamType != 2)
		return AUDIOCODEC_ERROR_INVALID_ARG;

	w->info.streamType = streamType;
	w->info.pcmFormat = 3;
	w->info.drcCut = 0;
	w->info.drcBoost = 0;
	w->info.drcTargetRef = -1;

	switch (work[10]) {
	case 8000:  w->info.srIdx = 11; break;
	case 11025: w->info.srIdx = 10; break;
	case 12000: w->info.srIdx = 9;  break;
	case 16000: w->info.srIdx = 8;  break;
	case 22050: w->info.srIdx = 7;  break;
	case 24000: w->info.srIdx = 6;  break;
	case 32000: w->info.srIdx = 5;  break;
	case 44100: w->info.srIdx = 4;  break;
	case 48000: w->info.srIdx = 3;  break;
	case 64000: w->info.srIdx = 2;  break;
	case 88200: w->info.srIdx = 1;  break;
	case 96000: w->info.srIdx = 0;  break;
	default:
		return AUDIOCODEC_ERROR_INVALID_ARG;
	}

	w->info.downmixMode = 0;
	w->info.out3chMode = 0;
	w->info.outputDelayMode = 2;
	w->info.parseOnlyFlag = 0;

	if (aacSetConfig(&w->info, &w->dec) < 0)
		return AUDIOCODEC_ERROR_DECODE;

	if ((uint8_t)work[8] == 2)
		w->dec.adifPceTag = (int32_t)work[9];

	w->dec.sbrDownsample = 5;
	int32_t ret = aacSetOutputMode(&w->info, &w->dec);
	if (ret < 0)
		return AUDIOCODEC_ERROR_DECODE;
	return ret;
}

int32_t aacDecode(uint32_t *work)
{
	AacWork *w = (AacWork *)work[1];
	const uint8_t *inBuf = (const uint8_t *)work[4];
	AacInfo statusBuf;

	if (aacPreparseFrame(inBuf, &w->dec) >= 0) {
		if (aacDecodeFrame(inBuf, (uint8_t *)work[6], &w->dec) < 0) {
			work[0] = (uint32_t)w->dec.internalError;
			aacResetCtx(&w->dec);
		} else {
			int32_t ret = aacGetStatus(&w->dec, &statusBuf);
			if (ret >= 0) {
				work[7] = statusBuf.headerWords[5];
				work[5] = statusBuf.headerWords[6] >> 3;
				sceClibMemcpy_safe(&w->info, &statusBuf, sizeof(AacInfo));
				return ret;
			}
		}
	}
	return AUDIOCODEC_ERROR_DECODE;
}

/* =========================================================================
 * Context lifecycle and configuration
 * ========================================================================= */

static AAC_NOINLINE int32_t aacOpenCtx(AacDecCtx *dec)
{
	sceClibMemset(dec, 0, sizeof(AacDecCtx));
	dec->version = 4100u;
	dec->magic = 154075136u;
	return 0;
}

static AAC_NOINLINE int32_t aacResetCtx(AacDecCtx *dec)
{
	AacCore *core = &dec->core;

	dec->version = 4100u;
	dec->magic = 154075136u;
	sceClibMemset(core, 0, sizeof(AacCore));

	core->copyrightBits = 72;
	core->firstFrame = 1;
	core->crcErrFlag = 0;
	core->br.eof = 0;
	core->adtsRawBlockIdx = -1;

	aacInitWindows(core);
	core->winTables[0] = core->sinWinLong;
	core->winTables[1] = kAacKbdWinLong;
	core->winTables[2] = core->sinWinShort;
	core->winTables[3] = kAacKbdWinShort;
	core->winTables[4] = core->transWinSin;
	core->winTables[5] = core->transWinKbd;
	core->crcReg = 0xFFFFu;

	aacInitBuffers(core);
	aacResetDrc(core);
	aacSetDrcParams(-1, core, 0.0f, 0.0f);
	return 0;
}

static AAC_NOINLINE int32_t aacInitCtx(AacDecCtx *dec)
{
	sceClibMemset(dec, 0, sizeof(AacDecCtx));
	dec->version = 4100u;
	dec->magic = 154075136u;
	aacResetCtx(dec);
	return 0;
}

static AAC_NOINLINE int32_t aacSetConfig(const AacInfo *info, AacDecCtx *dec)
{
	dec->adifPceTag = -1;

	if (info->streamType == 1) {
		dec->isAdts = 1;
		dec->isAdif = 0;
	} else if (info->streamType == 2) {
		dec->isAdts = 0;
		dec->isAdif = 1;
	} else if (info->streamType == 0) {
		dec->isAdts = 0;
		dec->isAdif = 0;
	} else {
		dec->internalError = -4000;
		return -110;
	}

	if (info->pcmFormat > 5) {
		dec->internalError = -3900;
		return -110;
	}
	dec->pcmFormat = info->pcmFormat;

	dec->drcCut = (float)info->drcCut * (1.0f / 65535.0f);
	dec->drcBoost = (float)info->drcBoost * (1.0f / 65535.0f);
	dec->drcTargetRef = info->drcTargetRef;

	if (info->srIdx > 12) {
		dec->internalError = -4000;
		return -110;
	}
	dec->initSrIdx = info->srIdx;

	dec->outputDelayMode = info->outputDelayMode;
	int32_t ret = 0;
	if (info->outputDelayMode > 2) {
		dec->internalError = -4000;
		ret = -110;
	}
	dec->sbrEnable = info->sbrEnable;
	dec->sbrDownsample = info->sbrDownsample;
	return ret;
}

static AAC_NOINLINE int32_t aacSetOutputMode(const AacInfo *info, AacDecCtx *dec)
{
	if (info->downmixMode > 2 || info->out3chMode > 1 || info->parseOnlyFlag > 1) {
		dec->internalError = -4100;
		return -110;
	}
	dec->downmixMode = info->downmixMode;
	dec->out3chMode = info->out3chMode;
	dec->parseOnlyFlag = info->parseOnlyFlag;
	return 0;
}

static AAC_NOINLINE int32_t aacGetStatus(const AacDecCtx *dec, AacInfo *outInfo)
{
	sceClibMemcpy_safe(outInfo->headerWords, &dec->version, 0x28u);
	sceClibMemcpy_safe(outInfo->streamStatus, &dec->streamType, 0x1Eu);
	((uint8_t *)outInfo)[33] = (outInfo->headerWords[7] == 0) ? 1 : 0;
	return 0;
}

/* =========================================================================
 * Pre-parser (ADIF/ADTS header scan before decode)
 * ========================================================================= */

static AAC_NOINLINE uint32_t aacPreReadBits(AacPreBitReader *br, uint32_t nbits)
{
	if (br->error != 0)
		return 0;
	if (nbits > 16u) {
		br->error = 1;
		return 0;
	}
	if (nbits == 0)
		return 0;

	uint32_t val = br->cache >> (32u - nbits);
	int32_t rem = br->bitsLeft - (int32_t)nbits;
	if (rem < 0) {
		const uint8_t *p = br->ptr;
		uint32_t w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
			     ((uint32_t)p[2] << 8) | (uint32_t)p[3];
		br->ptr = p + 4;
		br->bitsLeft = rem + 32;
		br->cache = w << (uint32_t)(-rem);
		val |= w >> (uint32_t)(rem + 32);
	} else {
		br->bitsLeft = rem;
		br->cache <<= nbits;
	}
	return val;
}

static AAC_NOINLINE void aacPreSkipPceElems(AacPceElemList *list, int32_t hasCpeBit,
					    AacPreBitReader *br)
{
	for (int32_t i = 0; i < list->count; i++) {
		list->isCpe[i] = hasCpeBit ? (int32_t)aacPreReadBits(br, 1u) : 0;
		list->tag[i] = (int32_t)aacPreReadBits(br, 4u);
	}
}

static AAC_NOINLINE int32_t aacPreparseFrame(const uint8_t *inBuf, AacDecCtx *dec)
{
	AacPreBitReader br;
	uint8_t zeroBuf1[44];
	uint8_t zeroBuf2[64];
	uint8_t changed = 0;

	dec->infoChanged = 0;
	br.start = inBuf;
	br.ptr = inBuf;
	br.bitsLeft = 0;
	br.cache = 0;
	br.error = 0;
	sceClibMemset(zeroBuf1, 0, sizeof(zeroBuf1));
	sceClibMemset(zeroBuf2, 0, sizeof(zeroBuf2));
	dec->statusOk = 1;

	if (dec->isAdif == 1) {
		if (dec->core.firstFrame != 0) {
			if (dec->streamType != 0) {
				dec->streamType = 0;
				changed = 1;
			}
			uint32_t sig = 0;
			for (int32_t i = 0; i < 4; i++)
				sig = (sig << 8) | aacPreReadBits(&br, 8u);
			if (sig != 0x41444946u) {
				dec->internalError = -3700;
				return -110;
			}
			uint8_t adifChanged = 0;
			if (aacPreReadBits(&br, 1u) == 1u) {
				if (dec->hasCopyright != 1) {
					dec->hasCopyright = 1;
					adifChanged = 1;
				}
				for (int32_t i = 0; i < 9; i++) {
					uint8_t b = (uint8_t)aacPreReadBits(&br, 8u);
					if (dec->copyrightId[i] != b) {
						dec->copyrightId[i] = b;
						adifChanged = 1;
					}
				}
			} else if (dec->hasCopyright != 0) {
				dec->hasCopyright = 0;
				adifChanged = 1;
			}
			uint32_t origCopy = aacPreReadBits(&br, 1u);
			if (origCopy > 1u) {
				dec->internalError = -3700;
				return -110;
			}
			if (dec->originalCopy != origCopy) {
				dec->originalCopy = (uint8_t)origCopy;
				adifChanged = 1;
			}
			aacPreReadBits(&br, 1u);
			uint32_t bsType = aacPreReadBits(&br, 1u);
			if (bsType > 1u) {
				dec->internalError = -3700;
				return -110;
			}
			if (dec->adifBitstreamType != bsType) {
				dec->adifBitstreamType = (uint8_t)bsType;
				adifChanged = 1;
			}
			uint32_t brHi = aacPreReadBits(&br, 16u) << 7;
			uint32_t bitrate = brHi + aacPreReadBits(&br, 7u);
			if (dec->bitrate != bitrate) {
				dec->bitrate = bitrate;
				adifChanged = 1;
			}
			dec->infoChanged |= adifChanged;
		}
	} else if (dec->isAdts == 1) {
		if (dec->streamType != 1) {
			dec->streamType = 1;
			changed = 1;
		}
		dec->hasPce = 0;
		uint8_t adtsChanged = 0;
		if (aacPreReadBits(&br, 12u) != 4095u) {
			dec->internalError = -3800;
			return -110;
		}
		uint32_t id = aacPreReadBits(&br, 1u);
		if (id > 1u) {
			dec->internalError = -3800;
			return -110;
		}
		if (dec->adtsId != id) {
			dec->adtsId = (uint8_t)id;
			adtsChanged = 1;
		}
		if (aacPreReadBits(&br, 2u) != 0u) {
			dec->internalError = -3800;
			return -110;
		}
		uint32_t prot = aacPreReadBits(&br, 1u);
		if (prot > 1u) {
			dec->internalError = -3800;
			return -110;
		}
		if (dec->adtsProtAbsent != prot) {
			dec->adtsProtAbsent = (uint8_t)prot;
			adtsChanged = 1;
		}
		aacPreReadBits(&br, 2u);
		uint8_t srMapped = (uint8_t)kAacChanIndexMap[aacPreReadBits(&br, 4u) & 0xFu];
		if (srMapped == 255u) {
			dec->internalError = -3800;
			return -110;
		}
		if (dec->srIdx != srMapped) {
			dec->srIdx = srMapped;
			adtsChanged = 1;
		}
		aacPreReadBits(&br, 1u);
		uint32_t chCfg = kAacChanIdentityMap[aacPreReadBits(&br, 3u) & 7u];
		if (dec->adtsChanCfg != chCfg) {
			dec->adtsChanCfg = (uint8_t)chCfg;
			adtsChanged = 1;
		}
		uint32_t origCopy = aacPreReadBits(&br, 1u);
		if (origCopy > 1u) {
			dec->internalError = -3800;
			return -110;
		}
		if (dec->originalCopy != origCopy) {
			dec->originalCopy = (uint8_t)origCopy;
			adtsChanged = 1;
		}
		aacPreReadBits(&br, 1u);
		aacPreReadBits(&br, 1u);
		aacPreReadBits(&br, 1u);
		uint16_t frameBytes = (uint16_t)aacPreReadBits(&br, 13u);
		if (dec->adtsFrameBytes != frameBytes)
			dec->adtsFrameBytes = frameBytes;
		aacPreReadBits(&br, 11u);
		if (aacPreReadBits(&br, 2u) != 0u) {
			dec->internalError = -3800;
			return -110;
		}
		if (dec->adtsProtAbsent == 0)
			aacPreReadBits(&br, 16u);
		uint32_t firstElem = aacPreReadBits(&br, 3u);
		if (firstElem == 5u)
			dec->hasPce = 1;
		if (dec->parseOnlyFlag == 0) {
			if (chCfg == 0 && firstElem == 5u) {
				AacPce pceTmp;
				aacPreReadBits(&br, 4u);
				pceTmp.profile = (int32_t)aacPreReadBits(&br, 2u);
				pceTmp.srIdx = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.front.count = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.side.count = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.back.count = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.lfe.count = (int32_t)aacPreReadBits(&br, 2u);
				pceTmp.data.count = (int32_t)aacPreReadBits(&br, 3u);
				pceTmp.cc.count = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.monoMixPresent = (int32_t)aacPreReadBits(&br, 1u);
				if (pceTmp.monoMixPresent == 1)
					pceTmp.monoMixElem = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.stereoMixPresent = (int32_t)aacPreReadBits(&br, 1u);
				if (pceTmp.stereoMixPresent == 1)
					pceTmp.stereoMixElem = (int32_t)aacPreReadBits(&br, 4u);
				pceTmp.matrixMixPresent = (int32_t)aacPreReadBits(&br, 1u);
				if (pceTmp.matrixMixPresent == 1) {
					pceTmp.matrixMixIdx = (int32_t)aacPreReadBits(&br, 2u);
					pceTmp.pseudoSurround = (int32_t)aacPreReadBits(&br, 1u);
				}
				aacPreSkipPceElems(&pceTmp.front, 1, &br);
				aacPreSkipPceElems(&pceTmp.side, 1, &br);
				aacPreSkipPceElems(&pceTmp.back, 1, &br);
				aacPreSkipPceElems(&pceTmp.lfe, 0, &br);
				aacPreSkipPceElems(&pceTmp.data, 0, &br);
				aacPreSkipPceElems(&pceTmp.cc, 1, &br);

				if (pceTmp.side.count == 0 && pceTmp.lfe.count == 0) {
					if (pceTmp.front.count == 2 && pceTmp.back.count == 0 &&
					    pceTmp.front.isCpe[0] == 0 && pceTmp.front.isCpe[1] == 0) {
						chCfg = 8u;
					} else if (pceTmp.front.count == 1 && pceTmp.back.count == 1 &&
						   pceTmp.front.isCpe[0] == 1) {
						if (pceTmp.back.isCpe[0] == 0) {
							chCfg = 9u;
						} else if (pceTmp.back.isCpe[0] == 1) {
							chCfg = 10u;
						} else {
							dec->internalError = -3800;
							return -110;
						}
					} else {
						dec->internalError = -3800;
						return -110;
					}
				} else {
					dec->internalError = -3800;
					return -110;
				}
			}
			uint32_t cfgErr = dec->core.prevChanCfg;
			if (cfgErr != 0u)
				cfgErr = ((cfgErr <= 7u || chCfg != 0u) && cfgErr != chCfg) ? 1u : 0u;
			if (dec->chanCfgError != cfgErr) {
				dec->chanCfgError = (uint8_t)cfgErr;
				adtsChanged = 1;
			}
		}
		dec->infoChanged |= adtsChanged;
	} else {
		if (dec->streamType != 2) {
			dec->streamType = 2;
			changed = 1;
		}
	}

	if (br.error == 1)
		return -110;
	if (dec->parseOnlyFlag == 1) {
		int32_t ret = aacParseFrame(inBuf, dec);
		if (ret != 0)
			return ret;
	}
	dec->infoChanged |= changed;
	return 0;
}

/* =========================================================================
 * Bitstream reader and CRC-16 / CRC-10
 * ========================================================================= */

static AAC_NOINLINE uint32_t aacGetBits(uint8_t nbits, AacCore *core)
{
	AacBitReader *br = &core->br;
	uint32_t cache = br->cache;
	int32_t rem = br->bitsLeft - nbits;
	uint32_t val = cache >> (32 - nbits);

	if (rem >= 0) {
		br->cache = cache << nbits;
		br->bitsLeft = rem;
		return val;
	}

	const uint8_t *p = br->ptr;
	const uint8_t *end = br->end;
	if (end >= p + 4) {
		uint32_t w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
			     ((uint32_t)p[2] << 8) | (uint32_t)p[3];
		br->bitsLeft = rem + 32;
		br->ptr = p + 4;
		br->cache = w << (uint32_t)(-rem);
		return val | (w >> (uint32_t)(rem + 32));
	}

	int32_t availBytes = (int32_t)(end - p);
	if (rem >= -8 * availBytes) {
		uint32_t w = 0;
		if (availBytes == 1) {
			w = (uint32_t)p[0] << 24;
		} else if (availBytes == 2) {
			w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16);
		} else if (availBytes == 3) {
			w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8);
		} else {
			br->eof = 1;
			return 0;
		}
		val |= w >> (uint32_t)(rem + 32);
		br->bitsLeft = rem + 8 * availBytes;
		br->ptr = end;
		br->cache = w << (uint32_t)(-rem);
		return val;
	}

	br->eof = 1;
	return 0;
}

static AAC_NOINLINE uint32_t aacPeek8Bits(const AacCore *core)
{
	uint32_t cache = core->br.cache;
	uint32_t bits = (uint32_t)core->br.bitsLeft;
	if (bits <= 7u)
		return ((uint32_t)core->br.ptr[0] >> bits) | (cache >> 24);
	return cache >> 24;
}

static AAC_NOINLINE uint32_t aacPeek16Bits(const AacCore *core)
{
	uint32_t cache = core->br.cache;
	uint32_t bits = (uint32_t)core->br.bitsLeft;
	if (bits <= 15u) {
		const uint8_t *p = core->br.ptr;
		uint32_t next16 = ((uint32_t)p[0] << 8) | (uint32_t)p[1];
		return (next16 >> bits) | (cache >> 16);
	}
	return cache >> 16;
}

static AAC_NOINLINE uint32_t aacPeek19Bits(const AacCore *core)
{
	uint32_t cache = core->br.cache;
	uint32_t bits = (uint32_t)core->br.bitsLeft;
	if (bits > 18u)
		return cache >> 13;
	const uint8_t *p = core->br.ptr;
	uint32_t next24 = ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
	return (next24 >> (bits + 5u)) | (cache >> 13);
}

static AAC_NOINLINE uint32_t aacPeekBits(uint32_t nbits, AacCore *core)
{
	AacBitReader *br = &core->br;
	if (br->eof != 0)
		return 0;
	if (nbits <= 16u) {
		const uint8_t *p = br->ptr;
		uint32_t cache = br->cache;
		int32_t need = (int32_t)nbits - br->bitsLeft;
		uint32_t val = cache >> (32u - nbits);
		if (need <= 0)
			return val;
		if (br->end >= p + 4) {
			uint32_t w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
				     ((uint32_t)p[2] << 8) | (uint32_t)p[3];
			return val | (w >> (32 - need));
		}
		int32_t availBytes = (int32_t)(br->end - p);
		if (need <= 8 * availBytes) {
			uint32_t w = 0;
			if (availBytes == 1) {
				w = (uint32_t)p[0] << 24;
			} else if (availBytes == 2) {
				w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16);
			} else if (availBytes == 3) {
				w = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
				    ((uint32_t)p[2] << 8);
			} else {
				br->eof = 1;
				return 0;
			}
			return val | (w >> (32 - need));
		}
	}
	br->eof = 1;
	return 0;
}

static AAC_NOINLINE int32_t aacGetBitCount(const AacCore *core)
{
	return 8 * (int32_t)(core->br.ptr - core->br.start) - core->br.bitsLeft;
}

static AAC_NOINLINE int32_t aacByteAlign(AacCore *core)
{
	int32_t skip = (-core->br.bitsLeft) & 7;
	if (skip != 0) {
		skip = 8 - skip;
		aacGetBits(skip, core);
	}
	return skip;
}

static AAC_NOINLINE uint32_t aacGetBitsLong(int32_t nbits, AacCore *core)
{
	uint32_t val;
	if (nbits <= 16) {
		val = aacGetBits(nbits, core);
	} else {
		int32_t loBits = nbits - 8;
		uint32_t hi = aacGetBits(8, core);
		val = aacGetBits(loBits, core) | (hi << loBits);
	}
	if (core->br.eof == 1)
		return 0;
	return val;
}

static AAC_NOINLINE uint32_t aacCrc16Update(const uint8_t *startPtr, uint32_t startBit,
					    const uint8_t *endPtr, uint32_t endBit,
					    uint32_t mode, uint32_t crc)
{
	uint32_t zeroPad = mode;
	if (mode != 0) {
		uint32_t totalBits = (8u - startBit) + endBit + 8u * (uint32_t)(endPtr - startPtr - 1);
		uint32_t limit = (mode == 1u) ? 192u : 128u;
		if (limit > totalBits) {
			zeroPad = limit - totalBits;
		} else {
			endPtr = startPtr + (limit >> 3);
			endBit = startBit;
			zeroPad = 0;
		}
	}

	const uint8_t *p = startPtr;
	if (startBit <= 7u) {
		for (int32_t b = (int32_t)(7u - startBit); b >= 0; b--) {
			uint32_t bit = (*p >> b) & 1u;
			uint32_t top = (crc & 0x8000u) != 0u;
			crc <<= 1;
			if (bit != top)
				crc ^= 0x8005u;
		}
	}
	p++;
	while (p != endPtr) {
		uint32_t byteVal = *p++;
		for (int32_t b = 7; b >= 0; b--) {
			uint32_t bit = (byteVal >> b) & 1u;
			uint32_t top = (crc & 0x8000u) != 0u;
			crc <<= 1;
			if (bit != top)
				crc ^= 0x8005u;
		}
	}
	if (endBit != 0u) {
		uint32_t byteVal = *p;
		for (int32_t b = 7; b > (int32_t)(7u - endBit); b--) {
			uint32_t bit = (byteVal >> b) & 1u;
			uint32_t top = (crc & 0x8000u) != 0u;
			crc <<= 1;
			if (bit != top)
				crc ^= 0x8005u;
		}
	}
	for (uint32_t j = 0; j < zeroPad; j++) {
		if ((crc & 0x8000u) != 0u)
			crc = (crc << 1) ^ 0x8005u;
		else
			crc <<= 1;
	}
	return crc;
}

static AAC_NOINLINE uint32_t aacCrc16Finish(AacCore *core)
{
	uint32_t res = (uint16_t)core->crcReg;
	core->crcReg = 0xFFFFu;
	return res;
}

static AAC_NOINLINE uint32_t sbrCrc10Calc(const uint8_t *startPtr, uint32_t startBit,
					  const uint8_t *endPtr, uint32_t endBit,
					  uint32_t mode, uint32_t crc)
{
	uint32_t zeroPad = mode;
	if (mode != 0) {
		if (mode == 1u)
			zeroPad = 192u;
		else if (mode == 2u)
			zeroPad = 128u;
		uint32_t totalBits = (8u - startBit) + endBit + 8u * (uint32_t)(endPtr - startPtr - 1);
		if (zeroPad >= totalBits) {
			zeroPad -= totalBits;
		} else {
			endPtr = startPtr + (zeroPad >> 3);
			endBit = startBit;
			zeroPad = 0;
		}
	}

	const uint8_t *p = startPtr;
	uint32_t curByte = *p;
	if (endPtr >= p + 1) {
		if (startBit <= 7u) {
			for (int32_t b = (int32_t)(7u - startBit); b >= 0; b--) {
				uint32_t bit = (curByte >> b) & 1u;
				uint32_t top = (crc & 0x200u) != 0u;
				crc <<= 1;
				if (bit != top)
					crc ^= 0x233u;
			}
		}
		p++;
		while (p != endPtr) {
			uint32_t byteVal = *p++;
			for (int32_t b = 7; b >= 0; b--) {
				uint32_t bit = (byteVal >> b) & 1u;
				uint32_t top = (crc & 0x200u) != 0u;
				crc <<= 1;
				if (bit != top)
					crc ^= 0x233u;
			}
		}
		curByte = *p;
		startBit = 0;
	}
	if (endBit > startBit) {
		uint32_t rem = endBit - startBit;
		uint32_t topShift = 7u - startBit;
		for (uint32_t k = 0; k < rem; k++) {
			uint32_t top = (crc & 0x200u) != 0u;
			crc <<= 1;
			uint32_t bit = (curByte >> (topShift - k)) & 1u;
			if (top != bit)
				crc ^= 0x233u;
		}
	}
	for (uint32_t j = 0; j < zeroPad; j++) {
		if ((crc & 0x200u) != 0u)
			crc = (crc << 1) ^ 0x233u;
		else
			crc <<= 1;
	}
	return crc & 0x3FFu;
}

/* =========================================================================
 * Buffers, windows, and core table initialization
 * ========================================================================= */

static AAC_NOINLINE void aacInitBuffers(AacCore *core)
{
	for (int32_t ch = 0; ch < 3; ch++) {
		core->sbrChans[ch].envOrig = (float *)core->sbrChans[ch].envRaw;
		core->sbrChans[ch].noiseOrig = (float *)core->sbrChans[ch].noiseRaw;
		core->specPtr[ch] = core->chanBuf[ch];
		core->pcmPtr[ch] = core->chanBuf[ch];
		core->winShapePrev[ch][0] = 0;
		core->winShapePrev[ch][1] = 0;
		core->winShapeCur[ch][1] = 0;
	}
	core->outSamplesPerFrame = 1024;

	for (int32_t e = 0; e < 4; e++) {
		SbrElement *el = &core->sbrElems[e];
		el->extType = 0;
		el->tHfGen = 8;
		el->tHfAdj = 2;
		el->kxPrev = 32;
		el->prevStartFreq = 0xFFu;
		el->freqScale = 2;
		el->alterScale = 1;
		el->noiseBands = 2;
		el->limiterBands = 2;
		el->limiterGains = 2;
		el->interpFreq = 1;
		el->smoothingMode = 1;
		el->startFreq = 5;
		el->ampRes = 1;
		if (core->outSamplesPerFrame == 960) {
			el->numTimeSlots2 = 30;
			el->numTimeSlots = 15;
		} else {
			el->numTimeSlots2 = 32;
			el->numTimeSlots = 16;
		}
	}

	for (int32_t ch = 0; ch < 3; ch++) {
		SbrChannel *sc = &core->sbrChans[ch];
		sc->prevLa = -1;
		sc->qmfWork = &core->scratch.u.sbr.qmfWork;
		sc->headerReady = 0;
		for (int32_t k = 0; k < 5; k++) {
			sc->gTempPtr[k] = sc->gTempHist[k];
			sc->qTempPtr[k] = sc->qTempHist[k];
		}
		for (int32_t t = 0; t < 64; t++) {
			for (int32_t k = 0; k < 64; k++)
				core->drcQmfGain[ch][t][k] = 1.0f;
		}
		core->sbrChanPtr[ch] = sc;
	}

	for (int32_t e = 0; e < 5; e++) {
		core->scratch.u.sbr.bufPtrs[0][e] = core->scratch.u.sbr.envRefBuf[e];
		core->scratch.u.sbr.bufPtrs[1][e] = core->scratch.u.sbr.noiseRefBuf[e];
		core->scratch.u.sbr.bufPtrs[2][e] = core->scratch.u.sbr.envCurrBuf[e];
		core->scratch.u.sbr.bufPtrs[3][e] = core->scratch.u.sbr.gainLimBuf[e];
		core->scratch.u.sbr.bufPtrs[4][e] = core->scratch.u.sbr.noiseLimBuf[e];
		core->scratch.u.sbr.bufPtrs[5][e] = core->scratch.u.sbr.sineMagBuf[e];
		core->scratch.u.sbr.bufPtrs[6][e] = core->scratch.u.sbr.noiseMagBuf[e];
		core->scratch.u.sbr.bufPtrs[7][e] = core->scratch.u.sbr.gainRawBuf[e];
	}
}

static AAC_NOINLINE int32_t sbrGetSrIndex(uint32_t sr)
{
	switch (sr) {
	case 8000u:  return 11;
	case 11025u: return 10;
	case 12000u: return 9;
	case 16000u: return 8;
	case 22050u: return 7;
	case 24000u: return 6;
	case 32000u: return 5;
	case 44100u: return 4;
	case 48000u: return 3;
	case 64000u: return 2;
	case 88200u: return 1;
	case 96000u: return 0;
	default:     return -5000;
	}
}

static AAC_NOINLINE void aacInitWindows(AacCore *core)
{
	for (int32_t i = 0; i < 1024; i++)
		core->sinWinLong[i] = audioSinf(((float)i + 0.5f) * 0.001533980831f);
	for (int32_t i = 0; i < 128; i++)
		core->sinWinShort[i] = audioSinf(((float)i + 0.5f) * 0.01227184664f);

	sceClibMemset(core->transWinSin, 0, 448u * sizeof(float));
	sceClibMemset(core->transWinKbd, 0, 448u * sizeof(float));
	for (int32_t i = 0; i < 128; i++) {
		core->transWinSin[448 + i] = core->sinWinShort[i];
		core->transWinKbd[448 + i] = kAacKbdWinShort[i];
	}
	for (int32_t i = 0; i < 448; i++) {
		core->transWinSin[576 + i] = 1.0f;
		core->transWinKbd[576 + i] = 1.0f;
	}
}

static AAC_NOINLINE int32_t aacInitTablesWrapper(AacCore *core)
{
	aacInitCoreTables(core);
	core->winInfoBySeq[0] = core->winInfoTbl[0];
	core->winInfoBySeq[1] = core->winInfoTbl[0];
	core->winInfoBySeq[2] = core->winInfoTbl[2];
	core->winInfoBySeq[3] = core->winInfoTbl[0];
	return 0;
}

static AAC_NOINLINE void aacInitSpecHuffBook(AacHuffBook *book, const void *cb,
					     int32_t dim, int32_t lav, int32_t isSigned)
{
	int32_t range;
	if (isSigned != 0) {
		book->offset = lav;
		range = 2 * lav + 1;
	} else {
		range = lav + 1;
		book->offset = 0;
	}
	book->lavRange = range;
	book->isSigned = isSigned;
	book->dim = dim;
	book->cb = cb;
	float f = (float)range;
	book->invLav = 1.0f / f;
	book->invLav2 = 1.0f / (f * f);
	book->invLav3 = 1.0f / (f * (f * f));
}

static AAC_NOINLINE void aacInitSfHuffBook(AacHuffBook *book, const void *cb,
					   int32_t dim, int32_t lav, int32_t isSigned)
{
	int32_t range;
	if (isSigned != 0) {
		book->offset = lav;
		range = 2 * lav + 1;
	} else {
		range = lav + 1;
		book->offset = 0;
	}
	book->lavRange = range;
	book->isSigned = isSigned;
	book->dim = dim;
	book->cb = cb;
	float f = (float)range;
	book->invLav = 1.0f / f;
	book->invLav2 = 1.0f / (f * f);
	book->invLav3 = 1.0f / (f * (f * f));
}

static AAC_NOINLINE void aacInitCoreTables(AacCore *core)
{
	aacInitSpecHuffBook(&core->specHuff[0],  kAacHuffCb1,  4, 1,  1);
	aacInitSpecHuffBook(&core->specHuff[1],  kAacHuffCb2,  4, 1,  1);
	aacInitSpecHuffBook(&core->specHuff[2],  kAacHuffCb3,  4, 2,  0);
	aacInitSpecHuffBook(&core->specHuff[3],  kAacHuffCb4,  4, 2,  0);
	aacInitSpecHuffBook(&core->specHuff[4],  kAacHuffCb5,  2, 4,  1);
	aacInitSpecHuffBook(&core->specHuff[5],  kAacHuffCb6,  2, 4,  1);
	aacInitSpecHuffBook(&core->specHuff[6],  kAacHuffCb7,  2, 7,  0);
	aacInitSpecHuffBook(&core->specHuff[7],  kAacHuffCb8,  2, 7,  0);
	aacInitSpecHuffBook(&core->specHuff[8],  kAacHuffCb9,  2, 12, 0);
	aacInitSpecHuffBook(&core->specHuff[9],  kAacHuffCb10, 2, 12, 0);
	aacInitSpecHuffBook(&core->specHuff[10], kAacHuffCb11, 2, 16, 0);
	aacInitSfHuffBook(&core->sfHuff, kAacSfHuffCb, 1, 60, 1);

	core->specHuff[0].lut  = kAacHuffLut1;
	core->specHuff[1].lut  = kAacHuffLut2;
	core->specHuff[2].lut  = kAacHuffLut3;
	core->specHuff[3].lut  = kAacHuffLut4;
	core->specHuff[4].lut  = kAacHuffLut5;
	core->specHuff[5].lut  = kAacHuffLut6;
	core->specHuff[6].lut  = kAacHuffLut7;
	core->specHuff[7].lut  = kAacHuffLut8;
	core->specHuff[8].lut  = kAacHuffLut9;
	core->specHuff[9].lut  = kAacHuffLut10;
	core->specHuff[10].lut = kAacHuffLut11;

	for (int32_t i = 0; i < 128; i++)
		core->sfPowTab[i] = audioPowf(2.0f, (float)i * 0.25f);

	const AacSampleRateInfo *sri = &kAacSampleRateInfo[core->chanMap.srIdx];
	core->maxSfHalf = 128;

	core->winInfoTbl[0] = &core->longWinInfo;
	core->longWinInfo.isLong = 1;
	core->longWinInfo.numWin = 1;
	core->longWinInfo.binsPerFrame = 1024;
	core->longWinInfo.numSfb[0] = sri->numSfbLong;
	core->longWinInfo.sectLenBits[0] = 5;
	core->longWinInfo.sfbTable[0] = sri->sfbLong;
	core->longWinInfo.sfbWidth = NULL;
	core->longWinInfo.numGroups = 1;
	core->longWinInfo.groupLen[0] = 1;
	core->longWinInfo.reservedWinTail[0] = 0;

	core->winInfoTbl[2] = &core->shortWinInfo;
	core->shortWinInfo.isLong = 0;
	core->shortWinInfo.numWin = 8;
	core->shortWinInfo.binsPerFrame = 1024;
	for (int32_t w = 0; w < 8; w++) {
		core->shortWinInfo.numSfb[w] = sri->numSfbShort;
		core->shortWinInfo.sectLenBits[w] = 3;
		core->shortWinInfo.sfbTable[w] = sri->sfbShort;
	}
	core->shortWinInfo.sfbWidth = core->shortSfbWidth;
	int16_t prev = 0;
	for (int32_t b = 0; b < sri->numSfbShort; b++) {
		int16_t cur = sri->sfbShort[b];
		core->shortSfbWidth[b] = (int16_t)(cur - prev);
		prev = cur;
	}

	for (int32_t idx = 0; idx < 4; idx++) {
		AacWindowInfo *wi = core->winInfoTbl[idx];
		if (wi == NULL)
			continue;
		wi->totalSfb = 0;
		if (wi->numWin > 0) {
			int32_t binsPerWin = wi->binsPerFrame / wi->numWin;
			int32_t sfbIdx = 0;
			int16_t binOff = 0;
			for (int32_t w = 0; w < wi->numWin; w++) {
				wi->binsPerWin[w] = binsPerWin;
				int32_t nSfb = wi->numSfb[w];
				const int16_t *tbl = wi->sfbTable[w];
				for (int32_t b = 0; b < nSfb; b++)
					wi->sfbOffsets[sfbIdx + b] = (int16_t)(tbl[b] + binOff);
				sfbIdx += nSfb;
				binOff = (int16_t)(binOff + binsPerWin);
			}
			wi->totalSfb = sfbIdx;
		}
	}
}

/* =========================================================================
 * ADTS / ADIF / PCE / DSE / FIL / Channel configuration
 * ========================================================================= */

static AAC_NOINLINE int32_t aacGetTnsMaxBands(int32_t isLong, const AacCore *core)
{
	int32_t idx = 4 * core->chanMap.srIdx + (isLong ? 0 : 1);
	return kAacTnsMaxBands[idx];
}

static AAC_NOINLINE int32_t aacGetTnsMaxOrder(int32_t isLong, const AacCore *core)
{
	if (!isLong)
		return 7;
	if (core->chanMap.profile == 0)
		return 20;
	if (core->chanMap.profile == 1)
		return 12;
	return 0;
}

static AAC_NOINLINE int32_t aacParseAdtsHeader(AacCore *core)
{
	const uint8_t *crcStartPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcStartBit = (uint32_t)(-core->br.bitsLeft) & 7u;

	core->adtsHdr.syncword = (int32_t)aacGetBits(12, core);
	core->adtsHdr.id = (int32_t)aacGetBits(1, core);
	core->adtsHdr.layer = (int32_t)aacGetBits(2, core);
	core->adtsHdr.protectionAbsent = (int32_t)aacGetBits(1, core);
	core->adtsHdr.profile = (int32_t)aacGetBits(2, core);
	core->adtsHdr.srIdx = (int32_t)aacGetBits(4, core);
	core->adtsHdr.privateBit = (int32_t)aacGetBits(1, core);
	core->adtsHdr.chanCfg = (int32_t)aacGetBits(3, core);
	core->adtsHdr.originalCopy = (int32_t)aacGetBits(1, core);
	core->adtsHdr.home = (int32_t)aacGetBits(1, core);
	core->adtsHdr.copyrightBit = (int32_t)aacGetBits(1, core);
	core->adtsHdr.copyrightStart = (int32_t)aacGetBits(1, core);
	core->adtsHdr.frameLength = (int32_t)aacGetBits(13, core);
	core->adtsHdr.bufferFullness = (int32_t)aacGetBits(11, core);
	core->adtsHdr.numRawBlocks = (int32_t)aacGetBits(2, core);

	if (core->br.eof == 1)
		return -10000;

	const uint8_t *crcEndPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcEndBit = (uint32_t)(-core->br.bitsLeft) & 7u;
	core->crcReg = aacCrc16Update(crcStartPtr, crcStartBit, crcEndPtr, crcEndBit, 0, core->crcReg);

	if (core->adtsHdr.numRawBlocks != 0)
		return -200;
	if (core->adtsHdr.protectionAbsent == 0)
		core->adtsHdr.crcCheck = (int32_t)aacGetBits(16, core);
	if (core->br.eof == 1)
		return -10000;

	if (core->adtsHdr.syncword != 4095 || core->adtsHdr.layer != 0 ||
	    core->adtsHdr.profile != 1 || core->adtsHdr.srIdx > 12)
		return -200;

	core->chanMap.profile = 1;
	core->chanMap.srIdx = core->adtsHdr.srIdx;
	core->chanMap.sampleRate = (int32_t)kAacSampleRates[core->adtsHdr.srIdx];
	return 0;
}

static AAC_NOINLINE int32_t aacAssignChannelSlot(int32_t elemType, int32_t tag, int32_t posChar,
						 int32_t commonWin, AacChanMap *map, int32_t *slotOut)
{
	int32_t nCh = (elemType == 1) ? 2 : 1;
	int32_t slot;

	if (posChar == 'f') {
		slot = map->numFront;
		if (slot + nCh > 3)
			return -500;
		if (slot == 0) {
			if (elemType == 1) {
				map->numFront = nCh + 1;
				slot = 1;
				int32_t rSlot = 2;
				map->numCh += nCh;
				AacChanSlot *sl = &map->slots[slot];
				AacChanSlot *sr = &map->slots[rSlot];
				sl->active = 1;
				sl->tag = tag;
				sl->isCpe = 1;
				sl->commonWin = commonWin;
				sl->isLeft = 1;
				sl->pairIdx = rSlot;
				sl->winOwner = slot;
				sr->active = 1;
				sr->tag = tag;
				sr->isCpe = 1;
				sr->commonWin = commonWin;
				sr->isLeft = 0;
				sr->pairIdx = slot;
				sr->winOwner = commonWin ? slot : rSlot;
				*slotOut = slot;
				return 0;
			}
			if ((map->numFrontSce & 1) != 0) {
				map->numFront = nCh;
			} else {
				map->numFront = nCh + 1;
				slot = 1;
			}
			map->numCh += nCh;
			AacChanSlot *s = &map->slots[slot];
			s->active = 1;
			s->tag = tag;
			s->isCpe = 0;
			s->commonWin = commonWin;
			s->winOwner = slot;
			*slotOut = slot;
			return 0;
		}
		map->numFront = slot + nCh;
	} else if (posChar == 's') {
		if (map->numSide + nCh > 0)
			return -500;
		slot = map->numSide + 3;
		map->numSide += nCh;
	} else if (posChar == 'b') {
		if (map->numBack + nCh > 0)
			return -500;
		slot = map->numBack + 3;
		map->numBack += nCh;
	} else if (posChar == 'l') {
		if (map->numLfe + nCh > 0)
			return -500;
		slot = map->numLfe + 3;
		map->numLfe += nCh;
	} else if (posChar == 0) {
		if (elemType == 3)
			return -800;
		int32_t found = -1;
		for (int32_t i = 0; i < 3; i++) {
			if (map->slots[i].active != 0 &&
			    map->slots[i].isCpe == (elemType == 1) &&
			    map->slots[i].tag == tag) {
				found = i;
				break;
			}
		}
		if (found < 0)
			return -800;
		map->slots[found].winOwner = found;
		if (nCh == 2)
			map->slots[found + 1].winOwner = commonWin ? found : (found + 1);
		*slotOut = found;
		return 0;
	} else {
		return -500;
	}

	map->numCh += nCh;
	if (elemType == 1) {
		int32_t rSlot = slot + 1;
		AacChanSlot *sl = &map->slots[slot];
		AacChanSlot *sr = &map->slots[rSlot];
		sl->active = 1;
		sl->tag = tag;
		sl->isCpe = 1;
		sl->commonWin = commonWin;
		sl->isLeft = 1;
		sl->pairIdx = rSlot;
		sl->winOwner = slot;
		sr->active = 1;
		sr->tag = tag;
		sr->isCpe = 1;
		sr->commonWin = commonWin;
		sr->isLeft = 0;
		sr->pairIdx = slot;
		sr->winOwner = commonWin ? slot : rSlot;
	} else {
		AacChanSlot *s = &map->slots[slot];
		s->active = 1;
		s->tag = tag;
		s->isCpe = 0;
		s->commonWin = commonWin;
		s->winOwner = slot;
	}
	*slotOut = slot;
	return 0;
}

static AAC_NOINLINE int32_t aacMapElementChannel(int32_t elemType, int32_t tag, int32_t commonWin,
						 AacChanMap *map, int32_t *slotOut, AacCore *core)
{
	int32_t slot = 0;
	int32_t ret;

	if (core->useDefaultConfig == 0) {
		int32_t t = (elemType == 3) ? (tag + 1) : tag;
		ret = aacAssignChannelSlot(elemType, t, 0, commonWin, map, &slot);
		if (ret != 0)
			return ret;
	} else if (elemType == 0 || elemType == 1) {
		if (map->numFront + map->numSide + map->numBack > 2)
			return -600;
		if (elemType == 1) {
			core->cpeSeen = 1;
		} else if (core->firstFrame != 0 && core->cpeSeen == 0) {
			map->numFrontSce++;
		}
		ret = aacAssignChannelSlot(elemType, tag, 'f', commonWin, map, &slot);
		if (ret != 0)
			return ret;
	} else if (elemType == 3) {
		ret = aacAssignChannelSlot(3, tag + 1, 'l', commonWin, map, &slot);
		if (ret != 0)
			return ret;
	} else {
		slot = 0;
	}

	core->curElemChan = (uint8_t)slot;
	*slotOut = slot;
	return 0;
}

static AAC_NOINLINE void aacResetChanMap(AacChanMap *map, const AacCore *core)
{
	map->slots[0].reservedSlotB = 0;
	map->slots[1].reservedSlotB = 0;
	map->slots[2].reservedSlotB = 0;
	if (core->useDefaultConfig != 0) {
		map->numCh = 0;
		map->numFront = 0;
		map->numSide = 0;
		map->numBack = 0;
		map->numLfe = 0;
		map->numAssoc = 0;
		if (core->firstFrame != 0)
			map->numFrontSce = 0;
		for (int32_t i = 0; i < 3; i++) {
			map->slots[i].active = 0;
			map->slots[i].isCpe = 0;
			map->slots[i].isLeft = 0;
			map->slots[i].pairIdx = 0;
			map->slots[i].winOwner = 0;
			map->slots[i].reservedSlotA = 0;
			map->slots[i].reservedSlotB = 0;
		}
	}
}

static AAC_NOINLINE int32_t aacCheckChanConfig(const AacChanMap *map, int32_t saveFlag, AacCore *core)
{
	if (saveFlag != 0) {
		for (int32_t i = 0; i < 3; i++) {
			core->prevChanSlots[i].active = map->slots[i].active;
			core->prevChanSlots[i].isCpe = map->slots[i].isCpe;
			core->prevChanSlots[i].isLeft = map->slots[i].isLeft;
			core->prevChanSlots[i].pairIdx = map->slots[i].pairIdx;
		}
		return 0;
	}
	int32_t diff = 0;
	for (int32_t i = 0; i < 3; i++) {
		if (core->prevChanSlots[i].active != map->slots[i].active)
			diff = 1;
		if (core->prevChanSlots[i].active != 0) {
			if (core->prevChanSlots[i].isCpe != map->slots[i].isCpe)
				diff = 1;
			if (core->prevChanSlots[i].isLeft != map->slots[i].isLeft)
				diff = 1;
			if (core->prevChanSlots[i].pairIdx != map->slots[i].pairIdx)
				diff = 1;
		}
	}
	return diff ? -700 : 0;
}

static AAC_NOINLINE void aacReadPceElems(AacPceElemList *list, int32_t hasCpeBit, AacCore *core)
{
	for (int32_t i = 0; i < list->count; i++) {
		list->isCpe[i] = hasCpeBit ? (int32_t)aacGetBits(1, core) : 0;
		list->tag[i] = (int32_t)aacGetBits(4, core);
	}
}

static AAC_NOINLINE int32_t aacParsePce(AacPce *pce, int32_t *tagOut, AacCore *core)
{
	const uint8_t *crcStartPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcStartBit = (uint32_t)(-core->br.bitsLeft) & 7u;
	core->hasPceFlag = 1;

	int32_t pceTag = (int32_t)aacGetBits(4, core);
	pce->profile = (int32_t)aacGetBits(2, core);
	pce->srIdx = (int32_t)aacGetBits(4, core);
	pce->front.count = (int32_t)aacGetBits(4, core);
	pce->side.count = (int32_t)aacGetBits(4, core);
	pce->back.count = (int32_t)aacGetBits(4, core);
	pce->lfe.count = (int32_t)aacGetBits(2, core);
	pce->data.count = (int32_t)aacGetBits(3, core);
	pce->cc.count = (int32_t)aacGetBits(4, core);
	if (pce->cc.count != 0)
		return -300;

	pce->monoMixPresent = (int32_t)aacGetBits(1, core);
	pce->monoMixElem = (pce->monoMixPresent == 1) ? (int32_t)aacGetBits(4, core) : 0;
	pce->stereoMixPresent = (int32_t)aacGetBits(1, core);
	pce->stereoMixElem = (pce->stereoMixPresent == 1) ? (int32_t)aacGetBits(4, core) : 0;
	pce->matrixMixPresent = (int32_t)aacGetBits(1, core);
	if (pce->matrixMixPresent == 1) {
		pce->matrixMixIdx = (int32_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		pce->pseudoSurround = (int32_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
	} else {
		pce->matrixMixIdx = 0;
		pce->pseudoSurround = 0;
	}

	aacReadPceElems(&pce->front, 1, core);
	aacReadPceElems(&pce->side, 1, core);
	aacReadPceElems(&pce->back, 1, core);
	aacReadPceElems(&pce->lfe, 0, core);
	aacReadPceElems(&pce->data, 0, core);
	aacReadPceElems(&pce->cc, 1, core);
	aacByteAlign(core);

	int32_t commentLen = (int32_t)aacGetBits(8, core);
	if (commentLen < 0)
		commentLen = 0;
	for (int32_t i = 0; i < commentLen; i++)
		pce->comment[i] = (uint8_t)aacGetBits(8, core);
	pce->comment[commentLen] = 0;
	if (core->br.eof == 1)
		return -10000;

	const uint8_t *crcEndPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcEndBit = (uint32_t)(-core->br.bitsLeft) & 7u;
	core->crcReg = aacCrc16Update(crcStartPtr, crcStartBit, crcEndPtr, crcEndBit, 0, core->crcReg);

	if (core->adtsHdr.pceTag < 0) {
		core->adtsHdr.pceTag = pceTag;
	} else if (core->adtsHdr.pceTag != pceTag) {
		*tagOut = pceTag;
		return 0;
	}

	if (pce->profile != 1 || pce->srIdx > 12)
		return -300;

	core->chanMap.numCh = 0;
	core->chanMap.numFrontSce = 0;
	core->chanMap.numFront = 0;
	core->chanMap.numSide = 0;
	core->chanMap.numBack = 0;
	core->chanMap.numLfe = 0;
	core->chanMap.numAssoc = 0;
	if (pce->srIdx != core->chanMap.srIdx)
		core->chanMap.srIdx = pce->srIdx;
	core->chanMap.profile = 1;
	core->chanMap.sampleRate = (int32_t)kAacSampleRates[pce->srIdx];

	int32_t dummySlot = 0;
	if (pce->front.count > 0) {
		if (pce->front.isCpe[0] == 0)
			core->chanMap.numFrontSce++;
		for (int32_t i = 0; i < pce->front.count; i++) {
			int32_t et = pce->front.isCpe[i] ? 1 : 0;
			int32_t r = aacAssignChannelSlot(et, pce->front.tag[i], 'f', 0, &core->chanMap, &dummySlot);
			if (r != 0)
				return r;
		}
	}
	for (int32_t i = 0; i < pce->side.count; i++) {
		int32_t et = pce->side.isCpe[i] ? 1 : 0;
		int32_t r = aacAssignChannelSlot(et, pce->side.tag[i], 's', 0, &core->chanMap, &dummySlot);
		if (r != 0)
			return r;
	}
	for (int32_t i = 0; i < pce->back.count; i++) {
		int32_t et = pce->back.isCpe[i] ? 1 : 0;
		int32_t r = aacAssignChannelSlot(et, pce->back.tag[i], 'b', 0, &core->chanMap, &dummySlot);
		if (r != 0)
			return r;
	}
	for (int32_t i = 0; i < pce->lfe.count; i++) {
		int32_t r = aacAssignChannelSlot(pce->lfe.isCpe[i], pce->lfe.tag[i] + 1, 'l', 0, &core->chanMap, &dummySlot);
		if (r != 0)
			return r;
	}

	int32_t r = aacCheckChanConfig(&core->chanMap, 1, core);
	if (r != 0)
		return r;
	core->useDefaultConfig = 0;
	*tagOut = pceTag;
	return 0;
}

static AAC_NOINLINE int32_t aacParseAdifHeader(AacCore *core)
{
	AacPce tmpPce;
	int32_t tmpTag;

	for (int32_t i = 0; i < 4; i++)
		core->adifHdr.id[i] = (uint8_t)aacGetBits(8, core);
	if (core->br.eof == 1)
		return -10000;
	core->adifHdr.id[4] = 0;
	if (core->adifHdr.id[0] != 'A' || core->adifHdr.id[1] != 'D' ||
	    core->adifHdr.id[2] != 'I' || core->adifHdr.id[3] != 'F')
		return -100;

	core->adifHdr.copyIdPresent = (int32_t)aacGetBits(1, core);
	if (core->adifHdr.copyIdPresent == 1) {
		for (int32_t i = 0; i < 9; i++)
			core->adifHdr.copyId[i] = (uint8_t)aacGetBits(8, core);
		core->adifHdr.copyId[9] = 0;
	}
	core->adifHdr.originalCopy = (int32_t)aacGetBits(1, core);
	core->adifHdr.home = (int32_t)aacGetBits(1, core);
	core->adifHdr.bitstreamType = (int32_t)aacGetBits(1, core);
	core->adifHdr.bitrate = (int32_t)aacGetBitsLong(23, core);
	int32_t numPceMinus1 = (int32_t)aacGetBits(4, core);
	if (core->br.eof == 1)
		return -10000;

	if (core->adifHdr.bitstreamType != 0) {
		core->adifHdr.bufferFullness = 0;
	} else {
		core->adifHdr.bufferFullness = (int32_t)aacGetBitsLong(20, core);
		if (core->br.eof == 1)
			return -10000;
	}

	if (numPceMinus1 < 0)
		return -100;
	int32_t ret = -100;
	for (int32_t i = 0; i <= numPceMinus1; i++) {
		int32_t r = aacParsePce(&tmpPce, &tmpTag, core);
		if (r != 0)
			return r;
		if (core->adtsHdr.pceTag == tmpTag) {
			ret = 0;
			sceClibMemcpy_safe(&core->pce, &tmpPce, sizeof(AacPce));
		}
	}
	return ret;
}

static AAC_NOINLINE int32_t aacParseDse(uint32_t *elemTag, int32_t *countOut,
					uint8_t *dst, AacCore *core)
{
	const uint8_t *crcStartPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcStartBit = (uint32_t)(-core->br.bitsLeft) & 7u;

	*elemTag = aacGetBits(4, core);
	uint32_t alignFlag = aacGetBits(1, core);
	int32_t cnt = (int32_t)aacGetBits(8, core);
	if (cnt == 255)
		cnt = (int32_t)aacGetBits(8, core) + 255;
	*countOut = cnt;
	if (alignFlag != 0)
		aacByteAlign(core);
	for (int32_t i = 0; i < cnt; i++)
		dst[i] = (uint8_t)aacGetBits(8, core);
	if (core->br.eof == 1)
		return -10000;

	const uint8_t *crcEndPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcEndBit = (uint32_t)(-core->br.bitsLeft) & 7u;
	core->crcReg = aacCrc16Update(crcStartPtr, crcStartBit, crcEndPtr, crcEndBit, 0, core->crcReg);
	return (core->br.eof == 1) ? -10000 : 0;
}

static AAC_NOINLINE int32_t aacParseFil(uint8_t *dseBuf, AacCore *core)
{
	int32_t count = (int32_t)aacGetBits(4, core);
	if (count == 15)
		count = (int32_t)aacGetBits(8, core) + 14;
	if (core->br.eof == 1)
		return -10000;

	while (count > 0) {
		uint32_t extType = aacGetBits(4, core);
		int32_t consumed;
		switch (extType) {
		case 1:
			aacGetBits(4, core);
			for (int32_t i = 0; i < count - 1; i++)
				dseBuf[i] = (uint8_t)aacGetBits(8, core);
			return (core->br.eof == 1) ? -10000 : 0;
		case 2:
			if (aacGetBits(4, core) == 0) {
				int32_t dataLen = 0;
				int32_t escCnt = 0;
				uint32_t b;
				do {
					b = aacGetBits(8, core);
					if (core->br.eof == 1)
						return -10000;
					dataLen += (int32_t)b;
					escCnt++;
				} while (b == 255u);
				for (int32_t i = 0; i < dataLen; i++)
					aacGetBits(8, core);
				consumed = dataLen + escCnt + 1;
				break;
			}
			for (int32_t i = 0; i < count - 1; i++)
				aacGetBits(8, core);
			return (core->br.eof == 1) ? -10000 : 0;
		case 11:
			consumed = aacParseDrc(core);
			break;
		case 13:
			consumed = sbrDecodeExtension(core, count, 0);
			break;
		case 14:
			consumed = sbrDecodeExtension(core, count, 1);
			break;
		default:
			aacGetBits(4, core);
			for (int32_t i = 0; i < count - 1; i++)
				aacGetBits(8, core);
			return (core->br.eof == 1) ? -10000 : 0;
		}
		if (core->br.eof == 1)
			return -10000;
		if (consumed < 0)
			return consumed;
		count -= consumed;
	}
	return 0;
}

static AAC_NOINLINE int32_t aacValidateSbrConfig(AacCore *core)
{
	if (core->numSbrElems == 0 || core->chanMap.sbrEnable == 0) {
		if (core->outSamplesPerFrame != 1024)
			core->outSamplesPerFrame = 1024;
		return 0;
	}

	if (core->firstFrame != 0) {
		int32_t sr = core->chanMap.sampleRate;
		int32_t nCh = core->chanMap.numCh;
		switch (core->chanMap.sbrLevel) {
		case 2:
			if (sr > 24000 || nCh > 2)
				return -5300;
			break;
		case 3:
			if (sr > 24000)
				core->chanMap.sbrDownsample = 1;
			if (nCh > 2)
				return -5300;
			break;
		case 4:
			if (nCh <= 2) {
				if (sr > 48000)
					return -5300;
				if (sr > 24000) {
					core->chanMap.sbrDownsample = 1;
					core->chanMap.sbrRateRatio = 1.0f;
					if (core->outSamplesPerFrame != 1024)
						core->outSamplesPerFrame = 1024;
					return 0;
				}
			} else {
				if (sr > 24000 || nCh > 6 || (nCh == 6 && core->chanMap.numLfe == 0))
					return -5300;
			}
			break;
		case 5:
			if (sr > 48000 || nCh > 6 || (nCh == 6 && core->chanMap.numLfe == 0))
				return -5300;
			break;
		default:
			return -5300;
		}
	}

	float ratio = (float)(2 - core->chanMap.sbrDownsample);
	core->chanMap.sbrRateRatio = ratio;
	int32_t targetSamples = (ratio == 2.0f) ? 2048 : 1024;
	if (core->outSamplesPerFrame != targetSamples)
		core->outSamplesPerFrame = targetSamples;
	return 0;
}

static AAC_NOINLINE int32_t aacParseFrame(const uint8_t *inBuf, AacDecCtx *dec)
{
	AacCore *core = &dec->core;
	uint8_t infoChanged = 0;

	dec->internalError = 0;
	if (core->firstFrame != 0) {
		core->useDefaultConfig = 1;
		core->adtsHdr.copyrightStart = 0;
		core->chanMap.numCh = 0;
		core->adifMode = dec->isAdif;
		core->adtsHdr.pceTag = dec->adifPceTag;
		core->adtsMode = dec->isAdts;
		core->chanMap.profile = 1;
		core->chanMap.srIdx = dec->initSrIdx;
		core->chanMap.sampleRate = (int32_t)kAacSampleRates[dec->initSrIdx];
		core->chanMap.sbrRateRatio = 1.0f;
		core->chanMap.sbrEnable = dec->sbrEnable;
		core->chanMap.sbrDownsample = dec->sbrDownsample;
		core->chanMap.sbrLevel = 2;
		aacSetDrcParams(dec->drcTargetRef, core, dec->drcCut, dec->drcBoost);
	}

	core->br.start = inBuf;
	core->br.ptr = inBuf;
	core->br.cache = 0;
	core->br.bitsLeft = 0;
	core->br.eof = 0;
	core->br.end = inBuf + 1536;
	core->crcErrFlag = 0;
	core->hasPceFlag = 0;

	uintptr_t inAddr = (uintptr_t)inBuf;
	if (inAddr == 0 || inAddr == (uintptr_t)-1536 || inAddr > 0xFFFFFFFCu ||
	    inAddr + 1536u > 0xFFFFFFFCu) {
		dec->internalError = -3500;
		return -110;
	}

	aacByteAlign(core);
	if (core->br.eof == 1) {
		dec->internalError = -10000;
		return -110;
	}

	if (core->adifMode == 1 && core->firstFrame != 0) {
		int32_t r = aacParseAdifHeader(core);
		if (r != 0) {
			dec->internalError = r;
			return -110;
		}
		aacByteAlign(core);
		if (core->br.eof == 1) {
			dec->internalError = -10000;
			return -110;
		}
	} else if (core->adtsMode == 1) {
		core->adtsRawBlockIdx++;
		if (core->adtsRawBlockIdx > core->adtsHdr.numRawBlocks)
			core->adtsRawBlockIdx = 0;
		if (core->adtsRawBlockIdx <= 0) {
			int32_t r = aacParseAdtsHeader(core);
			if (r != 0) {
				dec->internalError = r;
				return -110;
			}
			int32_t fLen = core->adtsHdr.frameLength;
			if (fLen > 1536) {
				dec->internalError = -3500;
				return -110;
			}
			const uint8_t *newEnd = inBuf + fLen;
			core->br.end = newEnd;
			uintptr_t pAddr = (uintptr_t)core->br.ptr;
			uintptr_t eAddr = (uintptr_t)newEnd;
			if (inAddr > pAddr || eAddr <= pAddr || pAddr == 0 || eAddr == 0 ||
			    pAddr > 0xFFFFFFFCu || eAddr > 0xFFFFFFFCu) {
				dec->internalError = -3500;
				return -110;
			}
			if (dec->hasCopyright == 1) {
				dec->hasCopyright = 0;
				infoChanged = 1;
			}
			if (core->adtsHdr.copyrightStart != 0) {
				core->copyrightBits = 1;
				core->copyrightBuf[0] = (uint8_t)((core->copyrightBuf[0] << 1) |
								  (core->adtsHdr.copyrightBit & 1));
			} else if ((uint32_t)core->copyrightBits <= 71u) {
				uint32_t idx = (uint32_t)core->copyrightBits >> 3;
				core->copyrightBuf[idx] = (uint8_t)((core->copyrightBuf[idx] << 1) |
								    (core->adtsHdr.copyrightBit & 1));
				core->copyrightBits++;
				if (core->copyrightBits == 72) {
					for (int32_t i = 0; i < 9; i++) {
						if (dec->copyrightId[i] != core->copyrightBuf[i]) {
							dec->copyrightId[i] = core->copyrightBuf[i];
							infoChanged = 1;
						}
					}
					if (dec->hasCopyright != 1) {
						dec->hasCopyright = 1;
						infoChanged = 1;
					}
				}
			}
			aacByteAlign(core);
			if (core->br.eof == 1) {
				dec->internalError = -10000;
				return -110;
			}
		}
	}

	if (core->firstFrame != 0) {
		int32_t r = aacInitTablesWrapper(core);
		if (r != 0) {
			dec->internalError = r;
			return -110;
		}
	}

	aacResetChanMap(&core->chanMap, core);
	aacClearDrcPackets(core);
	core->numSbrElems = 0;

	uint32_t expectedSeq = 0xFFFFFFFFu;
	if (dec->parseOnlyFlag == 1) {
		uint32_t prevCfg = (uint8_t)(core->prevChanCfg - 1u);
		expectedSeq = (prevCfg <= 9u) ? kAacChanCfgElements[prevCfg] : 0xFFFFFFFFu;
	}

	int32_t totalCh = 0;
	int32_t elemSeq = -1;
	uint32_t dseTag = 0;
	int32_t dseCnt = 0;
	int32_t pceTag = 0;

	for (;;) {
		uint32_t elemId = aacGetBits(3, core);
		if (core->br.eof == 1) {
			dec->internalError = -10000;
			return -110;
		}
		if (elemId == 7u)
			break;
		int32_t r = 0;
		switch (elemId) {
		case 0:
		case 1:
		case 3:
			totalCh += (elemId == 1u) ? 2 : 1;
			if (totalCh > 2) {
				dec->internalError = -3500;
				return -110;
			}
			if (dec->parseOnlyFlag == 1 && core->firstFrame != 1) {
				uint32_t exp = expectedSeq & 0xFu;
				if (exp != elemId) {
					if (dec->chanCfgError != 1) {
						dec->chanCfgError = 1;
						infoChanged = 1;
					}
					dec->infoChanged |= infoChanged;
					dec->internalError = -3500;
					return -110;
				}
				expectedSeq >>= 4;
			}
			elemSeq = (elemSeq << 4) | (int32_t)elemId;
			r = aacParseChannelElement((int32_t)elemId, &core->chanMap, core);
			break;
		case 2:
			dec->internalError = -3500;
			return -110;
		case 4:
			r = aacParseDse(&dseTag, &dseCnt, core->dseBuf, core);
			break;
		case 5:
			r = aacParsePce(&core->pce, &pceTag, core);
			break;
		case 6:
			r = aacParseFil(core->dseBuf, core);
			break;
		default:
			continue;
		}
		if (r != 0) {
			dec->internalError = r;
			return -110;
		}
	}

	if (dec->parseOnlyFlag == 1) {
		uint8_t err = (core->firstFrame != 1 && (expectedSeq & 0xFu) != 7u) ? 1 : 0;
		if (dec->chanCfgError != err) {
			dec->chanCfgError = err;
			infoChanged = 1;
		}
		if (err == 1) {
			dec->infoChanged |= infoChanged;
			dec->internalError = -3500;
			return -110;
		}
	}

	aacByteAlign(core);
	if (core->br.eof == 1) {
		dec->internalError = -10000;
		return -110;
	}

	int32_t sbrRet = aacValidateSbrConfig(core);
	if (sbrRet != 0) {
		dec->internalError = sbrRet;
		return -110;
	}

	if (core->adtsMode == 1 && core->adtsHdr.protectionAbsent == 0 &&
	    core->adtsRawBlockIdx >= core->adtsHdr.numRawBlocks) {
		core->crcErrFlag = (core->adtsHdr.crcCheck != (int32_t)aacCrc16Finish(core)) ? 1 : 0;
	}

	uint8_t chanCfg;
	switch (elemSeq) {
	case -16:    chanCfg = 1;  break;
	case -15:    chanCfg = 2;  break;
	case -255:   chanCfg = 3;  break;
	case -4080:  chanCfg = 4;  break;
	case -4079:  chanCfg = 5;  break;
	case -65261: chanCfg = 6;  break;
	case -256:   chanCfg = 8;  break;
	case -240:   chanCfg = 9;  break;
	case -239:   chanCfg = 10; break;
	default:
		dec->internalError = -3500;
		return -110;
	}

	core->prevChanCfg = chanCfg;
	if (dec->chanCfg != chanCfg) {
		dec->chanCfg = chanCfg;
		infoChanged = 1;
	}
	dec->infoChanged |= infoChanged;
	return 0;
}

static AAC_NOINLINE int32_t aacDecodeFrame(const uint8_t *inBuf, uint8_t *outBuf, AacDecCtx *dec)
{
	AacCore *core = &dec->core;

	if (++dec->frameCountLo == 0u)
		dec->frameCountHi++;

	if (dec->parseOnlyFlag == 0) {
		int32_t r = aacParseFrame(inBuf, dec);
		if (r != 0)
			return r;
	}

	int32_t saveCfg = 0;
	if (core->firstFrame != 0 || core->prevFrameFirst != 0)
		saveCfg = (core->useDefaultConfig != 0) ? 1 : 0;

	int32_t err = aacCheckChanConfig(&core->chanMap, saveCfg, core);
	if (err != 0) {
		dec->internalError = err;
		return -110;
	}
	err = aacResolveDrc(&core->chanMap, core);
	if (err != 0) {
		dec->internalError = err;
		return -110;
	}

	/* M/S stereo on CPE pairs */
	for (int32_t ch = 0; ch < 3; ch++) {
		AacChanSlot *sl = &core->chanMap.slots[ch];
		if (sl->active != 0 && sl->isCpe != 0 && sl->isLeft != 0) {
			int32_t owner = sl->winOwner;
			if (core->msMaskPresent[owner] != 0) {
				int32_t pair = sl->pairIdx;
				uint8_t ws = core->winSeq[owner];
				if (ws > 3u) {
					dec->internalError = -1400;
					return -110;
				}
				const AacWindowInfo *wi = core->winInfoBySeq[ws];
				err = aacResolveMsIsPns(wi, core->winGroupEnd[owner],
							core->msUsed[owner], core->sfbCb[pair],
							core->msMaskPresent[owner]);
				if (err != 0) {
					dec->internalError = err;
					return -110;
				}
				aacApplyMsStereo(wi, core->winGroupEnd[owner], core->msUsed[owner],
						 core->specPtr[pair], core->specPtr[ch]);
			}
		}
	}

	/* PNS and Intensity Stereo */
	for (int32_t ch = 0; ch < 3; ch++) {
		AacChanSlot *sl = &core->chanMap.slots[ch];
		if (sl->active != 0) {
			int32_t owner = sl->winOwner;
			uint8_t ws = core->winSeq[owner];
			if (ws > 3u) {
				dec->internalError = -1400;
				return -110;
			}
			const AacWindowInfo *wi = core->winInfoBySeq[ws];
			if (core->hasPns != 0) {
				err = aacApplyPns(wi, ch, owner, core);
				if (err != 0) {
					dec->internalError = err;
					return -110;
				}
			}
			err = aacApplyIntensityStereo(&core->chanMap, wi, ch, owner, core);
			if (err != 0) {
				dec->internalError = err;
				return -110;
			}
		}
	}

	/* TNS, DRC, and IMDCT */
	for (int32_t ch = 0; ch < 3; ch++) {
		AacChanSlot *sl = &core->chanMap.slots[ch];
		if (sl->active == 0)
			continue;
		int32_t owner = sl->winOwner;
		uint8_t ws = core->winSeq[owner];
		if (ws > 3u) {
			dec->internalError = -1400;
			return -110;
		}
		const AacWindowInfo *wi = core->winInfoBySeq[ws];
		core->curPcmOut = core->pcmPtr[ch];
		if (core->tns[ch].numWin != wi->numWin) {
			dec->internalError = -2900;
			return -110;
		}
		int32_t specOff = 0;
		for (int32_t w = 0; w < core->tns[ch].numWin; w++) {
			err = aacApplyTns(core->specPtr[ch] + specOff, core->maxSfb[owner],
					  wi->sfbTable[w], wi->isLong, &core->tns[ch].win[w], core);
			if (err != 0) {
				dec->internalError = err;
				return -110;
			}
			specOff += wi->binsPerWin[w];
		}

		int32_t drcSbrFlag = (core->numSbrElems != 0) ? 1 : ((core->chanMap.sbrEnable != 0) ? 1 : 0);
		aacApplyDrc(core->specPtr[ch], ch, drcSbrFlag, ws, core);

		uint8_t shape = core->winShapePrev[owner][0];
		core->winShapeCur[ch][0] = shape;
		core->winShapePrev[ch][1] = shape;
		err = aacImdct(ws, core->winShapeCur[ch], core->overlapBuf[ch], core->pcmPtr[ch], core);
		if (err != 0) {
			dec->internalError = err;
			return -110;
		}
		core->winShapeCur[ch][1] = core->winShapeCur[ch][0];
	}

	/* SBR processing */
	for (int32_t ch = 0; ch < 3; ch++) {
		if (core->chanMap.slots[ch].active != 0 && core->numSbrElems != 0) {
			core->sbrCurChanIdx = ch;
			SbrElement *el = &core->sbrElems[core->sbrChanToElem[ch]];
			err = sbrProcessChannel(core, el, core->sbrChanPtr[ch], core->pcmPtr[ch]);
			if (err != 0) {
				dec->internalError = err;
				return -110;
			}
		}
	}
	core->sbrResetFlag = 0;

	/* Shift DRC QMF gain delay buffer */
	for (int32_t ch = 0; ch < 3; ch++) {
		for (int32_t t = 0; t < 32; t++)
			sceClibMemcpy_safe(core->drcQmfGain[ch][t], core->drcQmfGain[ch][t + 32],
					   64u * sizeof(float));
	}

	uint32_t outBytes = 0;
	if (dec->outputDelayMode == 0)
		core->outputReady = 1;

	if (dec->outputDelayMode == 0 || core->outputReady != 0) {
		int32_t mapLR[2] = { 1, 2 };
		int32_t mapMono2[2] = { 0, 0 };
		int32_t mapRL[2] = { 2, 1 };
		int32_t mapDefault[6] = { 1, 0, 2, 3, 4, 5 };
		const int32_t *chOrder = mapLR;
		int32_t nSamples = core->outSamplesPerFrame;
		int32_t downmix = dec->downmixMode;
		int32_t pcmFmt = dec->pcmFormat;
		int32_t is16Bit = (pcmFmt == 3 || pcmFmt == 5);
		int32_t sampleBytes = is16Bit ? 2 : 4;
		int32_t outChCount;
		int32_t loopCh;
		int32_t frameStride;

		if (downmix == 1) {
			frameStride = 6 * sampleBytes;
			chOrder = mapDefault;
			loopCh = 3;
			outChCount = 6;
			sceClibMemset(outBuf, 0, (size_t)(frameStride * nSamples));
		} else if (downmix == 2) {
			frameStride = 2 * sampleBytes;
			outChCount = 2;
			loopCh = 2;
			if (core->chanMap.numCh == 1) {
				chOrder = mapMono2;
			} else if (pcmFmt == 5) {
				chOrder = mapRL;
			}
		} else {
			outChCount = core->chanMap.numCh;
			if (pcmFmt == 5 && core->chanMap.slots[0].active == 0 &&
			    core->chanMap.slots[1].active == 1 && core->chanMap.slots[2].active == 1) {
				mapDefault[0] = 2;
				mapDefault[2] = 1;
			}
			chOrder = mapDefault;
			loopCh = 3;
			frameStride = sampleBytes * outChCount;
		}

		if (is16Bit) {
			int32_t outSlot = -1;
			int32_t strideS16 = frameStride >> 1;
			for (int32_t idx = 0; idx < loopCh; idx++) {
				int32_t c = chOrder[idx];
				if (core->chanMap.slots[c].active == 0)
					continue;
				outSlot = (downmix != 0) ? idx : (outSlot + 1);
				int16_t *dst = (int16_t *)outBuf + outSlot;
				const float *src = core->pcmPtr[c];
				for (int32_t s = 0; s < nSamples; s++) {
					float v = src[s];
					int16_t q;
					if (v >= 0.0f) {
						float r = v + 0.5f;
						q = (r > 32767.0f) ? (int16_t)32767 : (int16_t)(int32_t)r;
					} else {
						float r = v - 0.5f;
						q = (r < -32768.0f) ? (int16_t)-32768 : (int16_t)(int32_t)r;
					}
					*dst = q;
					dst += strideS16;
				}
			}
			outBytes = (uint32_t)(frameStride * nSamples);
		} else if (pcmFmt == 4) {
			int32_t outSlot = -1;
			for (int32_t idx = 0; idx < loopCh; idx++) {
				int32_t c = chOrder[idx];
				if (core->chanMap.slots[c].active == 0)
					continue;
				outSlot = (downmix != 0) ? idx : (outSlot + 1);
				float *dst = (float *)outBuf + outSlot;
				const float *src = core->pcmPtr[c];
				for (int32_t s = 0; s < nSamples; s++) {
					float v = src[s] * (1.0f / 32768.0f);
					if (v < -1.0f)
						v = -1.0f;
					else if (v > 1.0f)
						v = 1.0f;
					*dst = v;
					dst += outChCount;
				}
			}
			outBytes = (uint32_t)(4 * outChCount * nSamples);
		} else {
			float scale, lo, hi;
			int32_t shift;
			if (pcmFmt == 1) {
				scale = 16.0f; shift = 4; lo = -524288.0f; hi = 524287.0f;
			} else if (pcmFmt == 2) {
				scale = 4.0f; shift = 6; lo = -131072.0f; hi = 131071.0f;
			} else {
				scale = 256.0f; shift = 0; lo = -8388608.0f; hi = 8388607.0f;
			}
			int32_t outSlot = -1;
			int32_t strideS32 = frameStride >> 2;
			for (int32_t idx = 0; idx < loopCh; idx++) {
				int32_t c = chOrder[idx];
				if (core->chanMap.slots[c].active == 0)
					continue;
				outSlot = (downmix != 0) ? idx : (outSlot + 1);
				int32_t *dst = (int32_t *)outBuf + outSlot;
				const float *src = core->pcmPtr[c];
				for (int32_t s = 0; s < nSamples; s++) {
					float v = src[s] * scale;
					float r;
					if (v >= 0.0f) {
						r = v + 0.5f;
						if (r > hi)
							r = hi;
					} else {
						r = v - 0.5f;
						if (r < lo)
							r = lo;
					}
					*dst = ((int32_t)r) << shift;
					dst += strideS32;
				}
			}
			outBytes = (uint32_t)(frameStride * nSamples);
		}
	}

	if (core->br.ptr == NULL) {
		dec->internalError = -3600;
		return -110;
	}
	if (core->br.eof == 1) {
		dec->internalError = -10000;
		return -110;
	}

	/* Dual-mono channel-0 overlap/state rotation on first frame when SBR enabled */
	if (core->firstFrame != 0 && core->prevChanCfg == 8 && core->chanMap.sbrEnable != 0) {
		sceClibMemcpy_safe(core->overlapBuf[2], core->overlapBuf[1], 1024u * sizeof(float));
		sceClibMemcpy_safe(core->overlapBuf[1], core->overlapBuf[0], 1024u * sizeof(float));
		uint8_t wp0 = core->winShapePrev[0][0];
		uint8_t wp1 = core->winShapePrev[0][1];
		uint8_t wp2 = core->winShapePrev[1][0];
		uint8_t wp3 = core->winShapePrev[1][1];
		uint8_t wc0 = core->winShapeCur[0][0];
		uint8_t wc2 = core->winShapeCur[1][0];
		core->winShapePrev[1][0] = wp0;
		core->winShapePrev[1][1] = wp1;
		core->winShapePrev[2][0] = wp2;
		core->winShapePrev[2][1] = wp3;
		core->winShapeCur[1][0] = wc0;
		core->winShapeCur[2][0] = wc2;

		uint8_t e0 = core->sbrChanToElem[0];
		uint8_t e1 = core->sbrChanToElem[1];
		core->sbrChanToElem[1] = e0;
		core->sbrChanToElem[2] = e1;
		SbrChannel *c0 = core->sbrChanPtr[0];
		SbrChannel *c1 = core->sbrChanPtr[1];
		SbrChannel *c2 = core->sbrChanPtr[2];
		core->sbrChanPtr[0] = c2;
		core->sbrChanPtr[1] = c0;
		core->sbrChanPtr[2] = c1;
	}

	if (dec->outputDelayMode == 1)
		core->outputReady |= core->firstFrame;
	else if (dec->outputDelayMode == 2)
		core->outputReady |= core->prevFrameFirst;

	core->prevFrameFirst = core->firstFrame;
	dec->outputBytes = outBytes;
	core->firstFrame = 0;
	dec->bitsConsumed = (uint32_t)(8 * (int32_t)(core->br.ptr - inBuf) - core->br.bitsLeft);

	if (dec->isAdts == 1 && dec->adtsFrameBytes != (dec->bitsConsumed >> 3)) {
		dec->internalError = -3600;
		return -110;
	}

	uint8_t infoChanged = 0;
	if (dec->numCh != (uint8_t)core->chanMap.numCh) {
		dec->numCh = (uint8_t)core->chanMap.numCh;
		infoChanged = 1;
	}

	int32_t srIdx = core->chanMap.srIdx;
	if (srIdx < 0 || srIdx > 12) {
		dec->internalError = -3600;
		return -110;
	}
	uint32_t srHz = kAacSampleRates[srIdx];
	uint8_t sbrOutSrIdx;
	if (srIdx <= 2)
		sbrOutSrIdx = 0xFFu;
	else if (srIdx == 12)
		sbrOutSrIdx = 50u;
	else
		sbrOutSrIdx = (uint8_t)(srIdx - 3);

	if (core->adifMode != 1 && dec->srIdx != (uint8_t)srIdx) {
		dec->srIdx = (uint8_t)srIdx;
		infoChanged = 1;
	}
	if (core->numSbrElems != 0) {
		dec->outSrIdx = sbrOutSrIdx;
		srHz *= 2u;
		dec->sbrActive = 1;
	} else {
		dec->outSrIdx = (uint8_t)srIdx;
		dec->sbrActive = 0;
	}

	dec->bitrate = (uint32_t)((float)(dec->bitsConsumed * srHz) * (1.0f / 1024.0f));
	if (dec->pcmFormat < 0 || dec->pcmFormat > 5) {
		dec->internalError = -3600;
		return -110;
	}
	if (dec->outPcmFmt != (uint8_t)dec->pcmFormat) {
		dec->outPcmFmt = (uint8_t)dec->pcmFormat;
		infoChanged = 1;
	}

	uint8_t outCh;
	if (dec->downmixMode == 1)
		outCh = 6;
	else if (dec->downmixMode == 2)
		outCh = 2;
	else if (dec->out3chMode == 1 && dec->chanCfg == 6)
		outCh = 3;
	else
		outCh = (uint8_t)core->chanMap.numCh;

	if (dec->outNumCh != outCh) {
		dec->outNumCh = outCh;
		infoChanged = 1;
	}
	uint8_t monoOut = (core->chanMap.numCh == 1 && dec->downmixMode == 0) ? 1 : 0;
	if (dec->isMonoOut != monoOut) {
		dec->isMonoOut = monoOut;
		infoChanged = 1;
	}
	if (dec->hasData != 1) {
		dec->hasData = 1;
		infoChanged = 1;
	}

	dec->hasPce = (core->hasPceFlag == 1) ? 1 : 0;
	dec->crcError = core->crcErrFlag;
	dec->adtsHome = (uint8_t)core->adtsHdr.home;
	dec->adtsCopyrightBit = (uint8_t)core->adtsHdr.copyrightBit;
	dec->adtsCopyrightStart = (uint8_t)core->adtsHdr.copyrightStart;
	dec->infoChanged |= infoChanged;
	return 0;
}

/* =========================================================================
 * Dynamic Range Control (DRC)
 * ========================================================================= */

static AAC_NOINLINE void aacSetDrcParams(int32_t targetRef, AacCore *core, float cut, float boost)
{
	if (cut < 0.0f)
		cut = 0.0f;
	else if (cut > 1.0f)
		cut = 1.0f;
	if (boost < 0.0f)
		boost = 0.0f;
	else if (boost > 1.0f)
		boost = 1.0f;

	core->drc.cut = cut;
	core->drc.boost = boost;

	if (targetRef > 127) {
		targetRef = 127;
	} else if (targetRef < 0) {
		core->drc.enable = (cut > 0.0f || boost > 0.0f) ? 1 : 0;
		core->drc.useProgRef = 0;
		return;
	}
	core->drc.targetRef = targetRef;
	core->drc.enable = 1;
	core->drc.useProgRef = 1;
	core->drc.progRef = 108;
}

static AAC_NOINLINE void aacResetDrc(AacCore *core)
{
	core->drc.enable = 0;
	core->drc.numPkts = 0;
	core->drc.useProgRef = 1;
	core->drc.targetRef = 108;
	core->drc.progRef = 108;
	for (int32_t ch = 0; ch < 3; ch++) {
		AacDrcChan *dc = &core->drc.chans[ch];
		dc->numBands = 1;
		dc->bandTop[0] = 255;
		dc->dynRangeSgn[0] = 0;
		dc->dynRangeCtl[0] = 0;
		dc->bandIncr = 0;
		dc->prevBandIncr = 0;
	}
}

static AAC_NOINLINE void aacClearDrcPackets(AacCore *core)
{
	core->drc.numPkts = 0;
}

static AAC_NOINLINE int32_t aacParseDrc(AacCore *core)
{
	AacDrcPacket discardPkt;
	AacDrcPacket *pkt;
	int32_t bytesRead = 1;

	if (aacGetBits(1, core) != 0u) {
		int32_t tag = (int32_t)aacGetBits(4, core);
		aacGetBits(4, core);
		bytesRead = 2;
		if (tag == core->adtsHdr.pceTag) {
			pkt = &core->drc.pkts[core->drc.numPkts++];
		} else {
			pkt = &discardPkt;
		}
	} else {
		pkt = &core->drc.pkts[core->drc.numPkts++];
	}

	pkt->excludedPresent = (int32_t)aacGetBits(1, core);
	if (pkt->excludedPresent != 0) {
		for (int32_t i = 0; i < 7; i++)
			pkt->excludedMask[i] = (int32_t)aacGetBits(1, core);
		uint32_t more = aacGetBits(1, core);
		if (core->br.eof == 1)
			return 0;
		int32_t extBytes = 1;
		while (more != 0u) {
			for (int32_t i = 0; i < 7; i++)
				aacGetBits(1, core);
			more = aacGetBits(1, core);
			extBytes++;
			if (core->br.eof == 1)
				return 0;
		}
		bytesRead += extBytes;
	} else if (core->br.eof == 1) {
		return 0;
	}

	pkt->numBands = 1;
	if (aacGetBits(1, core) != 0u) {
		pkt->numBands += (int32_t)aacGetBits(4, core);
		pkt->bandIncr = (int32_t)aacGetBits(4, core);
		if (core->br.eof == 1)
			return 0;
		for (int32_t i = 0; i < pkt->numBands; i++)
			pkt->bandTop[i] = (int32_t)aacGetBits(8, core);
		bytesRead += pkt->numBands + 1;
	}

	pkt->progRefPresent = (int32_t)aacGetBits(1, core);
	if (pkt->progRefPresent != 0) {
		pkt->progRefLevel = (int32_t)aacGetBits(7, core);
		aacGetBits(1, core);
		bytesRead++;
	}
	if (core->br.eof == 1)
		return 0;

	for (int32_t i = 0; i < pkt->numBands; i++) {
		pkt->dynRangeSgn[i] = (int32_t)aacGetBits(1, core);
		pkt->dynRangeCtl[i] = (int32_t)aacGetBits(7, core);
	}
	if (core->br.eof == 1)
		return 0;
	return bytesRead + pkt->numBands;
}

static AAC_NOINLINE int32_t aacResolveDrc(const AacChanMap *map, AacCore *core)
{
	int32_t activePkt[3] = { 0, 0, 0 };
	int32_t exclCount[3] = { 0, 0, 0 };

	int32_t numCh = 0;
	for (int32_t ch = 0; ch < 3; ch++) {
		if (map->slots[ch].active != 0)
			numCh++;
	}
	if (numCh > 7)
		return -1300;

	int32_t nPkts = core->drc.numPkts;
	if (nPkts <= 0)
		return 0;

	int32_t validPkts = 0;
	for (int32_t p = 0; p < nPkts; p++) {
		AacDrcPacket *pkt = &core->drc.pkts[p];
		int32_t ex = 0;
		if (pkt->excludedPresent != 0) {
			for (int32_t c = 0; c < numCh; c++) {
				if (pkt->excludedMask[c] != 0)
					ex++;
			}
		}
		exclCount[p] = ex;
		if (ex != numCh) {
			activePkt[p] = 1;
			validPkts++;
		}
	}
	if (validPkts == 0)
		return 0;

	if (validPkts == 1) {
		int32_t idx = 0;
		while (idx < nPkts && activePkt[idx] == 0)
			idx++;
		if (exclCount[idx] == 0) {
			AacDrcPacket *pkt = &core->drc.pkts[idx];
			if (pkt->progRefPresent != 0)
				core->drc.progRef = pkt->progRefLevel;
			for (int32_t ch = 0; ch < 3; ch++) {
				if (map->slots[ch].active == 0)
					continue;
				AacDrcChan *dc = &core->drc.chans[ch];
				dc->numBands = pkt->numBands;
				if (pkt->numBands == 1) {
					dc->bandTop[0] = 255;
					dc->dynRangeSgn[0] = pkt->dynRangeSgn[0];
					dc->dynRangeCtl[0] = pkt->dynRangeCtl[0];
				} else {
					for (int32_t b = 0; b < pkt->numBands; b++) {
						dc->bandTop[b] = pkt->bandTop[b];
						dc->dynRangeSgn[b] = pkt->dynRangeSgn[b];
						dc->dynRangeCtl[b] = pkt->dynRangeCtl[b];
					}
				}
				dc->bandIncr = pkt->bandIncr;
			}
			return 0;
		}
	} else {
		for (int32_t c = 0; c < numCh; c++) {
			int32_t hit = 0;
			for (int32_t p = 0; p < nPkts; p++) {
				if (activePkt[p] != 0 &&
				    (exclCount[p] == 0 || core->drc.pkts[p].excludedMask[c] == 0))
					hit++;
			}
			if (hit > 1)
				return -1300;
		}
	}

	for (int32_t p = 0; p < nPkts; p++) {
		if (activePkt[p] == 0)
			continue;
		AacDrcPacket *pkt = &core->drc.pkts[p];
		if (pkt->progRefPresent != 0)
			core->drc.progRef = pkt->progRefLevel;
		int32_t activeIdx = 0;
		for (int32_t ch = 0; ch < 3; ch++) {
			if (map->slots[ch].active == 0)
				continue;
			if (pkt->excludedMask[activeIdx] == 0) {
				AacDrcChan *dc = &core->drc.chans[ch];
				dc->numBands = pkt->numBands;
				if (pkt->numBands == 1) {
					dc->bandTop[0] = 255;
					dc->dynRangeSgn[0] = pkt->dynRangeSgn[0];
					dc->dynRangeCtl[0] = pkt->dynRangeCtl[0];
				} else {
					for (int32_t b = 0; b < pkt->numBands; b++) {
						dc->bandTop[b] = pkt->bandTop[b];
						dc->dynRangeSgn[b] = pkt->dynRangeSgn[b];
						dc->dynRangeCtl[b] = pkt->dynRangeCtl[b];
					}
				}
				dc->bandIncr = pkt->bandIncr;
			}
			activeIdx++;
		}
	}
	return 0;
}

static AAC_NOINLINE void aacApplyDrc(float *spec, int32_t chIdx, int32_t sbrFlag,
				     uint32_t winSeq, AacCore *core)
{
	float prevQmfRow[64];
	AacDrcChan *dc = &core->drc.chans[chIdx];

	sceClibMemcpy_safe(prevQmfRow, core->drcQmfGain[chIdx][63], sizeof(prevQmfRow));
	if (core->drc.enable == 0)
		return;

	float baseGain = 1.0f;
	if (core->drc.useProgRef != 0)
		baseGain = audioPowf(0.5f, (float)(core->drc.targetRef - core->drc.progRef) / 24.0f);

	int32_t bottom = 0;
	for (int32_t b = 0; b < dc->numBands; b++) {
		float gain;
		if (dc->dynRangeSgn[b] != 0)
			gain = audioPowf(2.0f, (-core->drc.cut * (float)dc->dynRangeCtl[b]) / 24.0f) * baseGain;
		else
			gain = audioPowf(2.0f, (core->drc.boost * (float)dc->dynRangeCtl[b]) / 24.0f) * baseGain;

		int32_t topRaw = 4 * (dc->bandTop[b] + 1);
		int32_t startBin;
		if (winSeq == 2u) {
			startBin = (bottom / 4) * 4;
			bottom = topRaw;
		} else {
			startBin = (bottom / 32) * 32;
			bottom = (topRaw / 32) * 32;
		}
		for (int32_t k = startBin; k < bottom; k++)
			spec[k] *= gain;

		if (sbrFlag != 0) {
			if (winSeq == 2u) {
				int32_t t0 = (int32_t)(audioFloorf((float)startBin * 0.0078125f) * 32.0f) / 8;
				int32_t t1 = (int32_t)(audioCeilf((float)bottom * 0.0078125f) * 32.0f) / 8;
				int32_t fStart = (startBin % 128) / 4;
				for (int32_t t = t0; t < t1; t++) {
					if (t > t0 && (t & 3) == 0)
						fStart = 0;
					for (int32_t f = fStart; f < 64; f++)
						core->drcQmfGain[chIdx][t + 32][f] = gain;
				}
			} else {
				int32_t qStart = startBin / 32;
				for (int32_t t = 0; t < 48; t++) {
					float alpha;
					if (t >= 16 && t <= 31) {
						if (dc->bandIncr != 0)
							alpha = (t >= kAacDrcBandInc[dc->bandIncr - 1]) ? 1.0f : 0.0f;
						else
							alpha = (float)t * 0.03125f;
					} else {
						alpha = 1.0f;
					}
					float g1 = gain * alpha;
					float g0 = 1.0f - alpha;
					for (int32_t f = qStart; f < 64; f++)
						core->drcQmfGain[chIdx][t + 16][f] = g1 + prevQmfRow[f] * g0;
				}
			}
		}
	}

	dc->prevBandIncr = (winSeq == 2u) ? 8 : dc->bandIncr;
}

/* =========================================================================
 * Huffman decoding, ICS / CPE / SCE parsing, dequantization, M/S, I/S, PNS, TNS
 * ========================================================================= */

static AAC_NOINLINE int32_t aacDecodeSfHuff(const AacSfHuffEntry *cb, AacCore *core)
{
	uint32_t code = aacPeek19Bits(core);
	int32_t leadOnes = 0;
	uint32_t shifted = code << 13;
	while ((shifted & 0x80000000u) != 0u) {
		shifted <<= 1;
		if (++leadOnes == 19)
			break;
	}
	int32_t nextIdx = (leadOnes == 19) ? 20 : (leadOnes + 1);
	int32_t lo = kAacSfHuffLeadingOnes[leadOnes];
	int32_t mid = (lo + kAacSfHuffLeadingOnes[nextIdx]) >> 1;
	if (code <= cb[mid].cw)
		mid = lo;

	int32_t i = mid + 1;
	while (i <= 120 && code >= cb[i].cw)
		i++;

	const AacSfHuffEntry *e = &cb[i - 1];
	uint32_t len = (uint8_t)e->length;
	if (len <= 16u) {
		aacGetBits((int32_t)len, core);
	} else {
		aacGetBits(16, core);
		aacGetBits((int32_t)(len - 16u), core);
	}
	return e->index;
}

static AAC_NOINLINE int32_t aacDecodeSpecHuff(int32_t cbIdx, const AacHuffEntry *cb,
					      const AacHuffLutEntry *lut, AacCore *core)
{
	uint32_t peek8 = aacPeek8Bits(core);
	uint32_t bits = lut[peek8].bits;
	if (bits > 99u) {
		aacGetBits((int32_t)(bits - 100u), core);
		return (core->br.eof == 1) ? 0 : ((int32_t)lut[peek8].value + 200);
	}
	if (bits >= 1u) {
		aacGetBits((int32_t)bits, core);
		return (core->br.eof == 1) ? 0 : (int32_t)lut[peek8].value;
	}

	uint32_t code = aacPeek16Bits(core);
	int32_t leadOnes = 0;
	uint32_t shifted = code << 16;
	while ((shifted & 0x80000000u) != 0u) {
		shifted <<= 1;
		if (++leadOnes == 16)
			break;
	}
	int32_t nextIdx = (leadOnes == 16) ? 17 : (leadOnes + 1);
	const uint8_t *row = &kAacSpecHuffLeadingOnes[18 * (cbIdx - 1)];
	int32_t total = row[0];
	int32_t lo = row[leadOnes];
	int32_t mid = (lo + row[nextIdx]) >> 1;
	if (code <= cb[mid].cw)
		mid = lo;

	int32_t i = mid + 1;
	while (i < total && code >= cb[i].cw)
		i++;

	const AacHuffEntry *e = &cb[i - 1];
	aacGetBits((uint8_t)e->length, core);
	return (core->br.eof == 1) ? 0 : (int32_t)e->index;
}

static AAC_NOINLINE void aacUnpackHuffIdx(int32_t *dst, int32_t idx, const AacHuffBook *book)
{
	int32_t lav = book->lavRange;
	int32_t off = book->offset;
	if (book->dim == 4) {
		int32_t q0 = (int32_t)((float)idx * book->invLav3);
		int32_t rem = idx - lav * lav * lav * q0;
		dst[0] = q0 - off;
		int32_t q1 = (int32_t)((float)rem * book->invLav2);
		rem -= q1 * lav * lav;
		dst[1] = q1 - off;
		int32_t q2 = (int32_t)((float)rem * book->invLav);
		dst[2] = q2 - off;
		dst[3] = rem - q2 * lav - off;
	} else {
		int32_t q0 = (int32_t)((float)idx * book->invLav);
		dst[0] = q0 - off;
		dst[1] = idx - q0 * lav - off;
	}
}

static AAC_NOINLINE void aacApplyHuffSigns(int32_t *vals, uint32_t count, AacCore *core)
{
	while (count != 0u) {
		int32_t v = *vals++;
		if (v != 0) {
			if (aacGetBits(1, core) != 0u)
				*(vals - 1) = -v;
		}
		count--;
	}
}

static AAC_NOINLINE int32_t aacReadEscapeVal(int32_t val, AacCore *core)
{
	int32_t neg;
	if (val < 0) {
		if (val != -16)
			return val;
		neg = 1;
	} else {
		if (val != 16)
			return val;
		neg = 0;
	}

	int32_t n = 4;
	for (;;) {
		uint32_t b = aacGetBits(1, core);
		if (core->br.eof == 1)
			return 0;
		if (b == 0u)
			break;
		n++;
	}

	int32_t mag;
	if (n > 30) {
		mag = 0x7FFFFFFF;
	} else if (n >= 17) {
		uint32_t hi = aacGetBits(n - 16, core) << 16;
		uint32_t lo = aacGetBits(16, core);
		mag = (int32_t)(hi | lo) + (1 << n);
	} else {
		mag = (int32_t)aacGetBits(n, core) + (1 << n);
	}

	if (neg != 0)
		mag = -mag;
	return (core->br.eof == 1) ? 0 : mag;
}

static AAC_NOINLINE void aacReadWindowGrouping(const AacWindowInfo *win, uint8_t *groupEnd,
					       AacCore *core)
{
	if (win->numWin <= 0) {
		*groupEnd = 0;
		return;
	}
	uint8_t *out = groupEnd;
	int32_t first = 1;
	int32_t w = 0;
	for (;;) {
		if (win->binsPerWin[w] > 128) {
			*out++ = (uint8_t)(w + 1);
		} else if (first != 0) {
			first = 0;
		} else {
			uint32_t grp = aacGetBits(1, core);
			if (core->br.eof == 1)
				return;
			if (grp == 0u)
				*out++ = (uint8_t)w;
		}
		w++;
		if (w >= win->numWin)
			break;
	}
	*out = (uint8_t)w;
}

static AAC_NOINLINE uint8_t aacReadMsMask(const AacWindowInfo *win, const uint8_t *groupEnd,
					  int32_t maxSfb, uint8_t *msUsed, AacCore *core)
{
	uint32_t maskPresent = aacGetBits(2, core);
	if (core->br.eof == 1 || maskPresent == 0u)
		return 0;

	if (maskPresent == 2u) {
		int32_t w = 0;
		uint8_t *p = msUsed;
		const uint8_t *g = groupEnd;
		while (w < win->numWin) {
			int32_t nSfb = win->numSfb[w];
			for (int32_t b = 0; b < nSfb; b++)
				*p++ = 1;
			w = *g++;
		}
		return 2;
	}

	int32_t w = 0;
	uint8_t *p = msUsed;
	const uint8_t *g = groupEnd;
	while (w < win->numWin) {
		if (maxSfb != 0) {
			int32_t b = 0;
			do {
				p[b++] = (uint8_t)aacGetBits(1, core);
			} while (b < maxSfb);
			p += maxSfb;
		}
		if (core->br.eof == 1)
			return 0;
		int32_t nSfb = win->numSfb[w];
		for (int32_t b = maxSfb; b < nSfb; b++)
			*p++ = 0;
		w = *g++;
	}
	return 1;
}

static AAC_NOINLINE int32_t aacParseIcsInfo(uint8_t *winSeq, uint8_t *winShape, uint8_t *groupEnd,
					    uint8_t *maxSfb, AacWindowInfo *winDst, AacCore *core)
{
	aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;
	*winSeq = (uint8_t)aacGetBits(2, core);
	*winShape = (uint8_t)aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;

	const AacWindowInfo *wi = core->winInfoBySeq[*winSeq];
	if (wi == NULL)
		return -1500;
	sceClibMemcpy_safe(winDst, wi, sizeof(AacWindowInfo));

	if (wi->isLong == 0) {
		*maxSfb = (uint8_t)aacGetBits(4, core);
		if (core->br.eof == 1)
			return -10000;
		aacReadWindowGrouping(wi, groupEnd, core);
		return (core->br.eof == 1) ? -10000 : 0;
	}

	*maxSfb = (uint8_t)aacGetBits(6, core);
	*groupEnd = 1;
	uint32_t predPresent = aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;
	return (predPresent == 0u) ? 0 : -1500;
}

static AAC_NOINLINE void aacZeroIntBuf(void *buf, int32_t count)
{
	sceClibMemset(buf, 0, (size_t)count * 4u);
}

static AAC_NOINLINE int32_t aacDecodeIcs(AacWindowInfo *win, int32_t commonWin, uint8_t *winSeq,
					 uint8_t *winShape, uint8_t *groupEnd, uint8_t *maxSfb,
					 uint8_t *sfbCb, float *specOut, int16_t *globalGain,
					 int16_t *sfOut, AacTnsFrame *tnsOut, int32_t lfeOffset,
					 AacCore *core)
{
	uint32_t bandCb[128];
	uint8_t secInfo[258];

	(void)lfeOffset;
	*globalGain = (int16_t)aacGetBits(8, core);
	if (core->br.eof == 1)
		return -10000;

	if (commonWin == 0) {
		int32_t r = aacParseIcsInfo(winSeq, winShape, groupEnd, maxSfb, win, core);
		if (r != 0)
			return r;
	}

	int32_t maxSfbVal = *maxSfb;
	int32_t nSfb0 = win->numSfb[0];
	int32_t totalSfb = 0;
	if (maxSfbVal != 0) {
		int32_t g = 0;
		totalSfb = nSfb0;
		while (groupEnd[g] < win->numWin) {
			g++;
			totalSfb += nSfb0;
			if (g == 8)
				return -1800;
		}
	}

	/* Section data */
	int32_t sectBits = win->sectLenBits[0];
	int32_t escVal = (1 << sectBits) - 1;
	int32_t nSec = 0;
	int32_t curSfb = 0;
	uint32_t bytePos = 0;
	core->secErr = 0;

	while (curSfb < totalSfb && nSec < totalSfb) {
		if (bytePos + 2u > 258u) {
			nSec = 0;
			core->secErr = -1900;
			break;
		}
		uint8_t cb = (uint8_t)aacGetBits(4, core);
		int32_t len = (int32_t)aacGetBits(sectBits, core);
		if (core->br.eof == 1)
			return -10000;
		while (len == escVal && curSfb < totalSfb) {
			curSfb += escVal;
			len = (int32_t)aacGetBits(sectBits, core);
			if (core->br.eof == 1)
				return -10000;
		}
		curSfb += len;
		secInfo[bytePos] = cb;
		secInfo[bytePos + 1] = (uint8_t)curSfb;
		if (((uint8_t)curSfb % nSfb0) == maxSfbVal) {
			bytePos += 4u;
			if (bytePos > 258u) {
				nSec = 0;
				core->secErr = -1900;
				break;
			}
			curSfb += nSfb0 - maxSfbVal;
			secInfo[bytePos - 2] = 0;
			secInfo[bytePos - 1] = (uint8_t)curSfb;
			nSec += 2;
		} else {
			bytePos += 2u;
			nSec++;
		}
	}
	if (core->br.eof == 1)
		return -10000;
	if (core->secErr != 0)
		return core->secErr;
	if (curSfb != totalSfb)
		nSec = 0;

	if (nSec > 0) {
		int32_t pos = 0;
		for (int32_t s = 0; s < nSec; s++) {
			uint8_t cb = secInfo[2 * s];
			int32_t end = secInfo[2 * s + 1];
			while (pos < end)
				sfbCb[pos++] = cb;
		}
	} else {
		if (maxSfbVal != 0)
			return -1800;
		sceClibMemset(sfbCb, 0, 128u);
	}

	/* Compute short-window group lengths and interleaved SFB offsets */
	if (win->isLong == 0) {
		int32_t prevEnd = 0;
		int32_t nGrp = 0;
		while (nGrp <= 7 && prevEnd <= 7) {
			win->groupLen[nGrp] = (int16_t)(groupEnd[nGrp] - prevEnd);
			prevEnd = groupEnd[nGrp];
			nGrp++;
		}
		if (nGrp > 7 && prevEnd <= 7)
			return -1600;
		win->numGroups = nGrp;
		int16_t acc = 0;
		int16_t *offOut = win->sfbOffsets;
		const int16_t *sfbW = win->sfbWidth;
		for (int32_t g = 0; g < nGrp; g++) {
			int16_t gLen = win->groupLen[g];
			int32_t nSfb = win->numSfb[g];
			for (int32_t b = 0; b < nSfb; b++) {
				acc = (int16_t)(acc + sfbW[b] * gLen);
				*offOut++ = acc;
			}
		}
	}

	/* Expand section map to per-band codebook array */
	int32_t bPos = 0;
	for (int32_t s = 0; s < nSec; s++) {
		uint32_t cb = secInfo[2 * s];
		int32_t end = secInfo[2 * s + 1];
		while (bPos < end) {
			bandCb[bPos] = cb;
			sfOut[bPos] = 0;
			bPos++;
		}
	}
	while (bPos < 128) {
		bandCb[bPos] = 0;
		sfOut[bPos] = 0;
		bPos++;
	}

	/* Decode scalefactors */
	const AacSfHuffEntry *sfCb = (const AacSfHuffEntry *)core->sfHuff.cb;
	int32_t curSf = *globalGain;
	int32_t curNoise = *globalGain - 90;
	int32_t curIs = 0;
	int32_t noiseFirst = 1;
	int32_t wStart = 0;
	int32_t gIdx = 0;
	int32_t cbOff = 0;
	int16_t *sfPtr = sfOut;

	while (wStart < win->numWin) {
		int32_t wEnd = groupEnd[gIdx++];
		int32_t nSfb = win->numSfb[wStart];
		for (int32_t b = 0; b < nSfb; b++) {
			uint32_t cb = bandCb[cbOff + b];
			if (cb == 0u) {
				continue;
			} else if (cb == 12u) {
				return -2000;
			} else if (cb == 13u) {
				int32_t delta = noiseFirst ? ((int32_t)aacGetBits(9, core) - 256)
							   : (aacDecodeSfHuff(sfCb, core) - 60);
				noiseFirst = 0;
				curNoise += delta;
				sfPtr[b] = (int16_t)curNoise;
				core->hasPns = 1;
				if (core->br.eof == 1)
					return -10000;
			} else if (cb == 14u || cb == 15u) {
				curIs += aacDecodeSfHuff(sfCb, core) - 60;
				sfPtr[b] = (int16_t)curIs;
				if (core->br.eof == 1)
					return -10000;
			} else {
				curSf += aacDecodeSfHuff(sfCb, core) - 60;
				if (curSf < 0 || curSf >= 2 * core->maxSfHalf)
					return -2000;
				if (core->br.eof == 1)
					return -10000;
				sfPtr[b] = (int16_t)curSf;
			}
		}
		if (win->isLong == 0) {
			for (int32_t k = wStart + 1; k < wEnd; k++) {
				for (int32_t b = 0; b < nSfb; b++)
					sfPtr[(k - wStart) * nSfb + b] = sfPtr[b];
			}
			sfPtr += (wEnd - wStart) * nSfb;
		} else {
			sfPtr += nSfb;
		}
		cbOff += nSfb;
		wStart = wEnd;
	}

	/* Pulse data */
	core->pulse.present = (int32_t)aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;
	if (core->pulse.present != 0) {
		if (win->isLong == 0)
			return -1800;
		core->pulse.numPulse = (int32_t)aacGetBits(2, core);
		core->pulse.startSfb = (int32_t)aacGetBits(6, core);
		if (core->br.eof == 1)
			return -10000;
		for (int32_t i = 0; i <= core->pulse.numPulse; i++) {
			core->pulse.offset[i] = (int32_t)aacGetBits(5, core);
			core->pulse.amp[i] = (int32_t)aacGetBits(4, core);
		}
		if (core->br.eof == 1)
			return -10000;
	}

	/* TNS data */
	uint32_t tnsPresent = aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;
	tnsOut->numWin = win->numWin;
	if (tnsPresent == 0u) {
		for (int32_t w = 0; w < win->numWin; w++)
			tnsOut->win[w].nFilt = 0;
	} else {
		int32_t isShort = (win->isLong == 0);
		for (int32_t w = 0; w < win->numWin; w++) {
			AacTnsWin *tw = &tnsOut->win[w];
			tw->nFilt = (uint8_t)aacGetBits(isShort ? 1 : 2, core);
			if (core->br.eof == 1)
				return -10000;
			if (tw->nFilt == 0)
				continue;
			uint8_t coefRes = (uint8_t)(aacGetBits(1, core) + 3u);
			tw->coefRes = coefRes;
			int32_t topBand = win->numSfb[w];
			for (int32_t f = 0; f < tw->nFilt; f++) {
				AacTnsFilter *tf = &tw->filt[f];
				tf->stopBand = topBand;
				topBand -= (int32_t)aacGetBits(isShort ? 4 : 6, core);
				tf->startBand = topBand;
				tf->order = (uint8_t)aacGetBits(isShort ? 3 : 5, core);
				if (tf->order != 0) {
					tf->direction = (uint8_t)aacGetBits(1, core);
					int32_t compress = (int32_t)aacGetBits(1, core);
					int32_t coefBits = (int32_t)coefRes - compress;
					int16_t signMask = kAacTnsCoefSignExt[coefBits + 2];
					int16_t signExt = kAacTnsCoefSignExt[coefBits - 2];
					for (int32_t c = 0; c < tf->order; c++) {
						uint16_t raw = (uint16_t)aacGetBits(coefBits, core);
						tf->coef[c] = (raw & signMask) ? (int16_t)(raw | signExt) : (int16_t)raw;
					}
				}
				if (core->br.eof == 1)
					return -10000;
			}
		}
		if (core->br.eof == 1)
			return -10000;
	}

	/* Gain control (unsupported) */
	if (aacGetBits(1, core) != 0u)
		return (core->br.eof == 1) ? -10000 : -1800;
	if (core->br.eof == 1)
		return -10000;

	/* Spectral Huffman decoding */
	int32_t *specInt = core->scratch.u.spec.specInt;
	aacZeroIntBuf(specInt, 1024);

	int32_t sfbIdx = 0;
	int32_t binIdx = 0;
	for (int32_t s = 0; s < nSec; s++) {
		int32_t cbIdx = secInfo[2 * s];
		int32_t secEnd = secInfo[2 * s + 1];
		if (cbIdx == 0 || (cbIdx >= 13 && cbIdx <= 15)) {
			sfbIdx = secEnd;
			binIdx = win->sfbOffsets[secEnd - 1];
			continue;
		}
		const AacHuffBook *book = &core->specHuff[cbIdx - 1];
		const AacHuffEntry *cbTbl = (const AacHuffEntry *)book->cb;
		const AacHuffLutEntry *lut = book->lut;
		int32_t dim = (cbIdx <= 4) ? 4 : 2;

		if (cbIdx == 11) {
			while (sfbIdx < secEnd) {
				int32_t endBin = win->sfbOffsets[sfbIdx++];
				while (binIdx < endBin) {
					int32_t hIdx = aacDecodeSpecHuff(11, cbTbl, lut, core);
					if (core->br.eof == 1)
						return -10000;
					if ((uint32_t)binIdx >= 1024u)
						return -2100;
					aacUnpackHuffIdx(&specInt[binIdx], hIdx, book);
					aacApplyHuffSigns(&specInt[binIdx], 2, core);
					if (core->br.eof == 1)
						return -10000;
					specInt[binIdx] = aacReadEscapeVal(specInt[binIdx], core);
					if (core->br.eof == 1)
						return -10000;
					specInt[binIdx + 1] = aacReadEscapeVal(specInt[binIdx + 1], core);
					if (core->br.eof == 1)
						return -10000;
					binIdx += 2;
				}
			}
		} else if (book->isSigned != 0) {
			while (sfbIdx < secEnd) {
				int32_t endBin = win->sfbOffsets[sfbIdx++];
				while (binIdx < endBin) {
					int32_t hIdx = aacDecodeSpecHuff(cbIdx, cbTbl, lut, core);
					if (core->br.eof == 1)
						return -10000;
					if ((uint32_t)binIdx >= 1024u)
						return -2100;
					aacUnpackHuffIdx(&specInt[binIdx], hIdx, book);
					binIdx += dim;
				}
			}
		} else {
			while (sfbIdx < secEnd) {
				int32_t endBin = win->sfbOffsets[sfbIdx++];
				while (binIdx < endBin) {
					int32_t hIdx = aacDecodeSpecHuff(cbIdx, cbTbl, lut, core);
					if (core->br.eof == 1)
						return -10000;
					if ((uint32_t)binIdx >= 1024u)
						return -2100;
					aacUnpackHuffIdx(&specInt[binIdx], hIdx, book);
					aacApplyHuffSigns(&specInt[binIdx], dim, core);
					if (core->br.eof == 1)
						return -10000;
					binIdx += dim;
				}
			}
		}
	}

	/* Apply pulse data (long windows only) or deinterleave short windows */
	const int32_t *dequantSrc = specInt;
	if (win->isLong != 0) {
		if (core->pulse.present != 0 && core->pulse.numPulse >= 0) {
			int32_t startBin = (core->pulse.startSfb > 0)
					 ? core->longWinInfo.sfbTable[0][core->pulse.startSfb - 1]
					 : 0;
			uint32_t pos = (uint32_t)(startBin + core->pulse.offset[0]);
			for (int32_t i = 0; i <= core->pulse.numPulse; i++) {
				if (pos >= 1024u)
					return -1700;
				int32_t v = specInt[pos];
				specInt[pos] = (v <= 0) ? (v - core->pulse.amp[i]) : (v + core->pulse.amp[i]);
				if (i < core->pulse.numPulse)
					pos += (uint32_t)core->pulse.offset[i + 1];
			}
		}
	} else {
		int32_t *dst = core->scratch.u.spec.specDeint;
		dequantSrc = dst;
		const int32_t *src = specInt;
		const int16_t *sfbW = win->sfbWidth;
		int32_t nGrp = win->numGroups;
		int32_t prevSfb = 0;
		int32_t binsPerWin = 0;
		for (int32_t g = 0; g < nGrp; g++) {
			int32_t nSfb = win->numSfb[g];
			for (int32_t b = prevSfb; b < nSfb; b++)
				binsPerWin += sfbW[b];
			prevSfb = nSfb;
			int32_t gLen = win->groupLen[g];
			int32_t *dstWin = dst;
			for (int32_t b = 0; b < nSfb; b++) {
				int32_t w = sfbW[b];
				int32_t *d = dstWin;
				for (int32_t k = 0; k < gLen; k++) {
					sceClibMemcpy_safe(d, src, (size_t)w * sizeof(int32_t));
					src += w;
					d += binsPerWin;
				}
				dstWin += w;
			}
			dst += binsPerWin * gLen;
		}
	}

	/* Inverse quantization: sign(x) * |x|^(4/3) * 2^((sf - 100) / 4) */
	int32_t nWin = win->numWin;
	int32_t nSfb = win->numSfb[0];
	const int16_t *sfbTbl = win->sfbTable[0];
	int32_t sfBase = 0;
	float *out = specOut;
	const int32_t *in = dequantSrc;

	for (int32_t w = 0; w < nWin; w++) {
		int32_t k = 0;
		for (int32_t b = 0; b < nSfb; b++) {
			int32_t endBin = sfbTbl[b];
			int32_t sf = sfOut[sfBase + b];
			int32_t diff = sf - 100;
			float gain;
			if ((uint32_t)diff <= 127u)
				gain = core->sfPowTab[diff];
			else if (sf != 0)
				gain = audioPowf(2.0f, (float)diff * 0.25f);
			else
				gain = 0.0f;

			while (k < endBin) {
				int32_t v = *in++;
				float fv = (v >= 0) ? kAudioPow43Table[v] : -kAudioPow43Table[-v];
				*out++ = fv * gain;
				k++;
			}
		}
		sfBase += nSfb;
	}

	return 0;
}

static AAC_NOINLINE int32_t aacParseChannelElement(int32_t elemType, AacChanMap *map, AacCore *core)
{
	AacWindowInfo winBuf;
	int16_t globalGain;
	int32_t slot;
	const uint8_t *crcStartPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
	uint32_t crcStartBit = (uint32_t)(-core->br.bitsLeft) & 7u;

	int32_t tag = (int32_t)aacGetBits(4, core);
	if (core->br.eof == 1)
		return -10000;

	int32_t commonWin = 0;
	int32_t lfeOff = 0;
	int32_t chStart, chEnd;

	if (elemType == 1) {
		commonWin = (int32_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
		int32_t r = aacMapElementChannel(1, tag, commonWin, map, &slot, core);
		if (r != 0)
			return r;
		chStart = slot;
		chEnd = map->slots[slot].pairIdx;
		if (commonWin == 0) {
			core->msMaskPresent[map->slots[chStart].winOwner] = 0;
			core->msMaskPresent[map->slots[chEnd].winOwner] = 0;
		} else {
			int32_t owner = map->slots[chStart].winOwner;
			if (slot == 3)
				core->winSeq[owner] = 0xFFu;
			r = aacParseIcsInfo(&core->winSeq[owner], &core->winShapePrev[owner][0],
					    core->winGroupEnd[owner], &core->maxSfb[owner], &winBuf, core);
			if (r != 0)
				return r;
			if (core->winSeq[owner] > 3u)
				return -1400;
			core->winSeq[chEnd] = core->winSeq[owner];
			core->msMaskPresent[owner] = aacReadMsMask(core->winInfoBySeq[core->winSeq[owner]],
								   core->winGroupEnd[owner],
								   core->maxSfb[owner],
								   core->msUsed[owner], core);
			if (core->br.eof == 1)
				return -10000;
		}
	} else if (elemType == 0 || elemType == 3) {
		int32_t r = aacMapElementChannel(elemType, tag, 0, map, &slot, core);
		if (r != 0)
			return r;
		chStart = slot;
		chEnd = slot;
		int32_t owner = map->slots[slot].winOwner;
		core->msMaskPresent[owner] = 0;
		if (core->winSeq[owner] > 3u)
			return -1400;
		sceClibMemcpy_safe(&winBuf, core->winInfoBySeq[core->winSeq[owner]], sizeof(AacWindowInfo));
		lfeOff = (elemType == 3) ? 12 : 0;
	} else {
		return -1400;
	}

	if (chEnd > 2)
		return -1400;

	core->hasPns = 0;
	const uint8_t *cpeMidPtr = NULL;
	uint32_t cpeMidBit = 0;

	for (int32_t ch = chStart; ch <= chEnd; ch++) {
		int32_t owner = map->slots[ch].winOwner;
		if (ch == chEnd && elemType == 1) {
			cpeMidBit = (uint32_t)(-core->br.bitsLeft) & 7u;
			cpeMidPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
		}
		if (slot > 2 && commonWin == 0)
			core->winSeq[owner] = 0xFFu;
		int32_t r = aacDecodeIcs(&winBuf, commonWin, &core->winSeq[owner],
					 &core->winShapePrev[owner][0], core->winGroupEnd[owner],
					 &core->maxSfb[owner], core->sfbCb[ch], core->specPtr[ch],
					 &globalGain, core->scaleFactors[ch], &core->tns[ch],
					 lfeOff, core);
		if (r != 0)
			return r;
	}

	if (slot != 3) {
		const uint8_t *crcEndPtr = core->br.ptr - ((uint32_t)(core->br.bitsLeft + 7) >> 3);
		uint32_t crcEndBit = (uint32_t)(-core->br.bitsLeft) & 7u;
		core->crcReg = aacCrc16Update(crcStartPtr, crcStartBit, crcEndPtr, crcEndBit, 1u, core->crcReg);
		if (elemType == 1) {
			if (cpeMidPtr == NULL)
				return -1400;
			core->crcReg = aacCrc16Update(cpeMidPtr, cpeMidBit, crcEndPtr, crcEndBit, 2u, core->crcReg);
		}
	}

	if (aacPeekBits(3u, core) == 6u) {
		aacGetBits(3, core);
		core->sbrElems[core->numSbrElems].elemType = (uint8_t)elemType;
		if (core->br.eof == 1)
			return -10000;
		return aacParseFil(core->dseBuf, core);
	}
	return 0;
}

static AAC_NOINLINE int32_t aacApplyIntensityStereo(const AacChanMap *map, const AacWindowInfo *win,
						    int32_t chIdx, int32_t winOwner, AacCore *core)
{
	const AacChanSlot *sl = &map->slots[chIdx];
	if (sl->isCpe == 0 || sl->isLeft != 0)
		return 0;

	int32_t nWin = win->numWin;
	if (nWin <= 0)
		return 0;

	float *dstSpec = core->specPtr[chIdx];
	const float *srcSpec = core->specPtr[sl->pairIdx];
	const uint8_t *cbPtr = core->sfbCb[chIdx];
	const int16_t *sfPtr = core->scaleFactors[chIdx];
	const uint8_t *grpPtr = core->winGroupEnd[winOwner];
	int32_t w = 0;
	int32_t nSfb = win->numSfb[0];
	const int16_t *sfbTbl = win->sfbTable[0];

	for (;;) {
		int32_t wEnd = *grpPtr++;
		for (int32_t ww = w; ww < wEnd; ww++) {
			int32_t k = 0;
			for (int32_t b = 0; b < nSfb; b++) {
				uint8_t cb = cbPtr[b];
				int32_t endBin = sfbTbl[b];
				if (cb == 14u || cb == 15u) {
					float sgn = (cb == 15u) ? 1.0f : -1.0f;
					float scale = sgn * audioPowf(0.5f, (float)sfPtr[b] * 0.25f);
					while (k < endBin - 3) {
						dstSpec[k]     = srcSpec[k]     * scale;
						dstSpec[k + 1] = srcSpec[k + 1] * scale;
						dstSpec[k + 2] = srcSpec[k + 2] * scale;
						dstSpec[k + 3] = srcSpec[k + 3] * scale;
						k += 4;
					}
				}
				k = endBin;
			}
			int32_t step = win->binsPerWin[ww];
			srcSpec += step;
			dstSpec += step;
			sfPtr += nSfb;
		}
		w = wEnd;
		cbPtr += win->numSfb[w - 1];
		if (w >= nWin)
			return 0;
		nSfb = win->numSfb[w];
		sfbTbl = win->sfbTable[w];
		if (grpPtr == &core->winGroupEnd[winOwner][8])
			return -2200;
	}
}

static AAC_NOINLINE int32_t aacGenPnsNoise(float *dst, int32_t len, uint32_t *seed)
{
	float norm0 = audioSqrtf((float)len * 1562500031313870848.0f);
	if (norm0 == 0.0f)
		return -2500;
	float invNorm0 = 1.0f / norm0;
	float energy = 0.0f;
	if (len > 0) {
		uint32_t s = *seed;
		for (int32_t i = 0; i < len; i++) {
			s = 1664525u * s + 1013904223u;
			float v = (float)(int32_t)s * invNorm0;
			dst[i] = v;
			energy += v * v;
		}
		*seed = s;
	}
	float rms = audioSqrtf(energy);
	if (rms == 0.0f)
		return -2500;
	float invRms = 1.0f / rms;
	for (int32_t i = 0; i < len; i++)
		dst[i] *= invRms;
	return 0;
}

static AAC_NOINLINE int32_t aacApplyPns(const AacWindowInfo *win, int32_t chIdx,
					int32_t winOwner, AacCore *core)
{
	int32_t nWin = win->numWin;
	if (nWin <= 0)
		return 0;

	float *spec = core->specPtr[chIdx];
	const uint8_t *cbPtr = core->sfbCb[chIdx];
	const int16_t *sfPtr = core->scaleFactors[chIdx];
	const uint8_t *grpPtr = core->winGroupEnd[winOwner];
	int32_t *seedPtr = core->pnsBandSeed;
	int32_t w = 0;
	int32_t nSfb = win->numSfb[0];
	const int16_t *sfbTbl = win->sfbTable[0];

	for (;;) {
		int32_t wEnd = *grpPtr++;
		for (int32_t ww = w; ww < wEnd; ww++) {
			int32_t k = 0;
			for (int32_t b = 0; b < nSfb; b++) {
				uint8_t cb = cbPtr[b];
				int32_t endBin = sfbTbl[b];
				if (cb == 13u || cb == 113u) {
					int32_t r;
					if (cb == 13u) {
						seedPtr[b] = (int32_t)core->pnsState;
						r = aacGenPnsNoise(&spec[k], endBin - k, &core->pnsState);
					} else {
						r = aacGenPnsNoise(&spec[k], endBin - k, (uint32_t *)&seedPtr[b]);
					}
					if (r != 0)
						return r;
					float gain = audioPowf(2.0f, (float)sfPtr[b] * 0.25f);
					while (k < endBin) {
						spec[k] *= gain;
						k++;
					}
				}
				k = endBin;
			}
			spec += win->binsPerWin[ww];
			sfPtr += nSfb;
		}
		w = wEnd;
		int32_t stepSfb = win->numSfb[w - 1];
		cbPtr += stepSfb;
		seedPtr += stepSfb;
		if (w >= nWin)
			return 0;
		nSfb = win->numSfb[w];
		sfbTbl = win->sfbTable[w];
		if (grpPtr == &core->winGroupEnd[winOwner][8])
			return -2600;
	}
}

static AAC_NOINLINE void aacApplyMsStereo(const AacWindowInfo *win, const uint8_t *groupEnd,
					  const uint8_t *msUsed, float *rightSpec, float *leftSpec)
{
	int32_t nWin = win->numWin;
	if (nWin <= 0)
		return;

	int32_t w = 0;
	const uint8_t *gPtr = groupEnd;
	const uint8_t *mPtr = msUsed;

	while (w < nWin) {
		int32_t nSfb = win->numSfb[w];
		const int16_t *sfbTbl = win->sfbTable[w];
		int32_t wEnd = *gPtr++;
		for (int32_t ww = w; ww < wEnd; ww++) {
			int32_t k = 0;
			for (int32_t b = 0; b < nSfb; b++) {
				int32_t endBin = sfbTbl[b];
				if (mPtr[b] != 0) {
					while (k < endBin) {
						float l = leftSpec[k];
						float r = rightSpec[k];
						leftSpec[k] = l + r;
						rightSpec[k] = l - r;
						k++;
					}
				}
				k = endBin;
			}
			int32_t step = win->binsPerWin[ww];
			leftSpec += step;
			rightSpec += step;
		}
		w = wEnd;
		mPtr += win->numSfb[w - 1];
	}
}

static AAC_NOINLINE int32_t aacResolveMsIsPns(const AacWindowInfo *win, const uint8_t *groupEnd,
					      uint8_t *msUsed, uint8_t *sfbCb, uint8_t msMaskPresent)
{
	int32_t nWin = win->numWin;
	if (nWin <= 0)
		return 0;

	int32_t w = 0;
	const uint8_t *gPtr = groupEnd;
	uint8_t *mPtr = msUsed;
	uint8_t *cbPtr = sfbCb;

	for (;;) {
		int32_t nSfb = win->numSfb[w];
		for (int32_t b = 0; b < nSfb; b++) {
			if (mPtr[b] != 0) {
				uint8_t cb = cbPtr[b];
				if (cb == 15u) {
					if (msMaskPresent == 1u)
						cbPtr[b] = 14u;
					mPtr[b] = 0;
				} else if (cb == 14u) {
					if (msMaskPresent == 1u)
						cbPtr[b] = 15u;
					mPtr[b] = 0;
				} else if (cb == 13u) {
					cbPtr[b] = 113u;
					mPtr[b] = 0;
				}
			}
		}
		mPtr += nSfb;
		cbPtr += nSfb;
		if (gPtr == &groupEnd[8])
			return -2700;
		w = *gPtr++;
		if (w >= nWin)
			return 0;
	}
}

static AAC_NOINLINE int32_t aacApplyTns(float *spec, int32_t maxSfb, const int16_t *sfbOffsets,
					int32_t isLong, const AacTnsWin *tnsWin, const AacCore *core)
{
	float lpc[32];
	float refl[32];
	float state[64];

	int32_t nFilt = tnsWin->nFilt;
	for (int32_t f = 0; f < nFilt; f++) {
		const AacTnsFilter *tf = &tnsWin->filt[f];
		int32_t order = tf->order;
		int32_t maxOrder = aacGetTnsMaxOrder(isLong, core);
		if (order > maxOrder)
			order = maxOrder;
		if (order == 0)
			continue;

		sceClibMemset(refl, 0, sizeof(refl));
		sceClibMemset(state, 0, 32u * sizeof(float));

		float iqHalf = (float)(1 << (tnsWin->coefRes - 1));
		float iqPos = (iqHalf - 0.5f) / 1.5707964f;
		float iqNeg = (iqHalf + 0.5f) / 1.5707964f;

		for (int32_t i = 0; i < order; i++) {
			int16_t c = tf->coef[i];
			float denom = (c < 0) ? iqNeg : iqPos;
			if (denom == 0.0f)
				return -2800;
			refl[i + 1] = audioSinf((float)c / denom);
		}

		/* Levinson-Durbin conversion from reflection coefficients to LPC */
		lpc[0] = 1.0f;
		state[0] = 1.0f;
		for (int32_t m = 1; m <= order; m++) {
			float km = refl[m];
			state[0] = lpc[0];
			for (int32_t i = 1; i < m; i++)
				state[i] = lpc[i] + km * lpc[m - i];
			state[m] = km;
			for (int32_t i = 0; i <= m; i++)
				lpc[i] = state[i];
		}

		int32_t tnsMaxBands = aacGetTnsMaxBands(isLong, core);
		int32_t startBand = tf->startBand;
		if (startBand > tnsMaxBands)
			startBand = tnsMaxBands;
		if (startBand > maxSfb)
			startBand = maxSfb;
		int32_t startBin = (startBand <= 0) ? 0 : sfbOffsets[startBand - 1];

		int32_t stopBand = tf->stopBand;
		if (stopBand > tnsMaxBands)
			stopBand = tnsMaxBands;
		if (stopBand > maxSfb)
			stopBand = maxSfb;
		int32_t stopBin = (stopBand <= 0) ? 0 : sfbOffsets[stopBand - 1];

		int32_t size = stopBin - startBin;
		if (size <= 0)
			continue;

		int32_t inc;
		uint32_t firstBin, lastBin;
		if (tf->direction != 0) {
			inc = -1;
			firstBin = (uint32_t)(startBin + size - 1);
			lastBin = (uint32_t)startBin;
		} else {
			inc = 1;
			firstBin = (uint32_t)startBin;
			lastBin = (uint32_t)(startBin + size - 1);
		}
		if (firstBin >= 1024u || lastBin >= 1024u)
			return -2900;

		float *p = &spec[firstBin];
		sceClibMemset(state, 0, sizeof(state));
		uint32_t ring = 0;

		for (int32_t n = 0; n < size; n++) {
			float y = *p;
			const float *st = &state[ring];
			const float *a = &lpc[1];
			int32_t q = order >> 2;
			int32_t r = order & 3;
			for (int32_t i = 0; i < q; i++) {
				y = (((y - a[0] * st[0]) - a[1] * st[1]) - a[2] * st[2]) - a[3] * st[3];
				a += 4;
				st += 4;
			}
			for (int32_t i = 0; i < r; i++)
				y -= a[i] * st[i];
			ring = (ring - 1u) & 31u;
			*p = y;
			state[ring] = y;
			state[ring + 32] = y;
			p += inc;
		}
	}
	return 0;
}

/* =========================================================================
 * IMDCT and Radix-4 FFT
 * ========================================================================= */

static AAC_NOINLINE void aacMdctPreTwiddle(const float *inBuf, float *outBuf,
					   int32_t numBlocks, int32_t mdctLen)
{
	if (mdctLen == 2048) {
		const float *tw = kAacMdctTwiddleLong;
		const uint16_t *brFwd = kAacMdctBitrevLong;
		const uint16_t *brRev = &kAacMdctBitrevLong[512];
		const float *pLo = inBuf;
		const float *pHi = inBuf + 1023;
		float c0 = 1.0f;
		float s0 = 0.0f;

		for (int32_t i = 0; i < 64; i++) {
			float a0 = -pLo[0], a1 = -pLo[2], a2 = -pLo[4], a3 = -pLo[6];
			float b0 = pHi[0], b1 = pHi[-2], b2 = pHi[-4], b3 = pHi[-6];
			float c1 = tw[2], s1 = tw[3];
			float c2 = tw[4], s2 = tw[5];
			float c3 = tw[6], s3 = tw[7];
			float c4 = tw[8], s4 = tw[9];

			float *d0 = &outBuf[brFwd[0]];
			float *d1 = &outBuf[brFwd[1]];
			float *d2 = &outBuf[brFwd[2]];
			float *d3 = &outBuf[brFwd[3]];
			d0[0] = a0 * c0 - b0 * s0; d0[1] = a0 * s0 + b0 * c0;
			d1[0] = a1 * c1 - b1 * s1; d1[1] = a1 * s1 + b1 * c1;
			d2[0] = a2 * c2 - b2 * s2; d2[1] = a2 * s2 + b2 * c2;
			d3[0] = a3 * c3 - b3 * s3; d3[1] = a3 * s3 + b3 * c3;

			float e0 = pLo[1], e1 = pLo[3], e2 = pLo[5], e3 = pLo[7];
			float f0 = -pHi[-1], f1 = -pHi[-3], f2 = -pHi[-5], f3 = -pHi[-7];
			brRev -= 4;
			float *r0 = &outBuf[brRev[0]];
			float *r1 = &outBuf[brRev[1]];
			float *r2 = &outBuf[brRev[2]];
			float *r3 = &outBuf[brRev[3]];
			r0[0] = s4 * f3 - c4 * e3; r0[1] = c4 * f3 + s4 * e3;
			r1[0] = s3 * f2 - c3 * e2; r1[1] = c3 * f2 + s3 * e2;
			r2[0] = s2 * f1 - c2 * e1; r2[1] = c2 * f1 + s2 * e1;
			r3[0] = s1 * f0 - c1 * e0; r3[1] = c1 * f0 + s1 * e0;

			c0 = c4;
			s0 = s4;
			tw += 8;
			brFwd += 4;
			pLo += 8;
			pHi -= 8;
		}
	} else {
		const float *tw = kAacMdctTwiddleShort;
		const uint16_t *br = kAacMdctBitrevShort;
		const float *pLoBase = inBuf;
		const float *pHiBase = inBuf + 127;

		for (int32_t i = 0; i < 16; i++) {
			float c0 = tw[0], s0 = tw[1];
			float c1 = tw[2], s1 = tw[3];
			float c2 = tw[4], s2 = tw[5];
			float c3 = tw[6], s3 = tw[7];
			int32_t o0 = br[0], o1 = br[1], o2 = br[2], o3 = br[3];
			tw += 8;
			br += 4;

			const float *pLo = pLoBase;
			const float *pHi = pHiBase;
			float *dst = outBuf;
			for (int32_t blk = 0; blk < numBlocks; blk++) {
				float a0 = -pLo[0], b0 = pHi[0];
				float a1 = -pLo[2], b1 = pHi[-2];
				float a2 = -pLo[4], b2 = pHi[-4];
				float a3 = -pLo[6], b3 = pHi[-6];
				dst[o0]     = c0 * a0 - s0 * b0; dst[o0 + 1] = s0 * a0 + c0 * b0;
				dst[o1]     = c1 * a1 - s1 * b1; dst[o1 + 1] = s1 * a1 + c1 * b1;
				dst[o2]     = c2 * a2 - s2 * b2; dst[o2 + 1] = s2 * a2 + c2 * b2;
				dst[o3]     = c3 * a3 - s3 * b3; dst[o3 + 1] = s3 * a3 + c3 * b3;
				pLo += 128;
				pHi += 128;
				dst += 128;
			}
			pLoBase += 8;
			pHiBase -= 8;
		}
	}
}

static AAC_NOINLINE void aacFftRadix4(float *buf, int32_t n)
{
	int32_t n2 = 2 * n;
	int32_t step;
	const float *tw;

	if (n == 512) {
		for (int32_t i = 0; i < n2; i += 8) {
			float r0 = buf[i + 0], i0 = buf[i + 1];
			float r1 = buf[i + 2], i1 = buf[i + 3];
			float r2 = buf[i + 4], i2 = buf[i + 5];
			float r3 = buf[i + 6], i3 = buf[i + 7];
			buf[i + 0] = r0 + r1; buf[i + 2] = r0 - r1;
			buf[i + 1] = i0 + i1; buf[i + 3] = i0 - i1;
			buf[i + 4] = r2 + r3; buf[i + 6] = r2 - r3;
			buf[i + 5] = i2 + i3; buf[i + 7] = i2 - i3;
		}
		step = 2;
		tw = kAacFftTwiddleLong;
	} else {
		if (n <= 1)
			return;
		step = 1;
		tw = kAacFftTwiddleShort;
	}

	do {
		int32_t s2 = 2 * step;
		int32_t s4 = 4 * step;
		int32_t s6 = 6 * step;
		int32_t s8 = 8 * step;

		for (int32_t i = 0; i < n2; i += s8) {
			float r0 = buf[i],      i0 = buf[i + 1];
			float r1 = buf[i + s2], i1 = buf[i + s2 + 1];
			float r2 = buf[i + s4], i2 = buf[i + s4 + 1];
			float r3 = buf[i + s6], i3 = buf[i + s6 + 1];

			float t0r = r0 + r1, t0i = i0 + i1;
			float t1r = r0 - r1, t1i = i0 - i1;
			float t2r = r2 + r3, t2i = i2 + i3;
			float t3r = r2 - r3, t3i = i2 - i3;

			buf[i]          = t0r + t2r; buf[i + 1]      = t0i + t2i;
			buf[i + s2]     = t1r - t3i; buf[i + s2 + 1] = t1i + t3r;
			buf[i + s4]     = t0r - t2r; buf[i + s4 + 1] = t0i - t2i;
			buf[i + s6]     = t1r + t3i; buf[i + s6 + 1] = t1i - t3r;
		}

		if (step != 1) {
			int32_t halfStep = step >> 1;
			int32_t mid = 2 * halfStep;
			for (int32_t k = 2; k < mid; k += 2) {
				float c1 = tw[0], s1 = tw[1];
				tw += 2;
				float c2 = c1 * c1 - s1 * s1;
				float s2w = (c1 + c1) * s1;
				float c3 = c2 * c1 - s2w * s1;
				float s3 = c2 * s1 + s2w * c1;
				int32_t km = s2 - k;

				for (int32_t i = k; i < n2; i += s8) {
					int32_t j = i - k + km;
					/* Forward butterfly at i */
					float a0r = buf[i],      a0i = buf[i + 1];
					float b1r = buf[i + s2], b1i = buf[i + s2 + 1];
					float b2r = buf[i + s4], b2i = buf[i + s4 + 1];
					float b3r = buf[i + s6], b3i = buf[i + s6 + 1];

					float x1r = c2 * b1r - s2w * b1i;
					float x1i = s2w * b1r + c2 * b1i;
					float x2r = c1 * b2r - s1 * b2i;
					float x2i = s1 * b2r + c1 * b2i;
					float x3r = c3 * b3r - s3 * b3i;
					float x3i = s3 * b3r + c3 * b3i;

					float p0r = a0r + x1r, p0i = a0i + x1i;
					float p1r = a0r - x1r, p1i = a0i - x1i;
					float p2r = x2r + x3r, p2i = x2i + x3i;
					float p3r = x2r - x3r, p3i = x2i - x3i;

					buf[i]          = p0r + p2r; buf[i + 1]      = p0i + p2i;
					buf[i + s2]     = p1r - p3i; buf[i + s2 + 1] = p1i + p3r;
					buf[i + s4]     = p0r - p2r; buf[i + s4 + 1] = p0i - p2i;
					buf[i + s6]     = p1r + p3i; buf[i + s6 + 1] = p1i - p3r;

					/* Conjugate mirror butterfly at j */
					float c0r = buf[j],      c0i = buf[j + 1];
					float d1r = buf[j + s2], d1i = buf[j + s2 + 1];
					float d2r = buf[j + s4], d2i = buf[j + s4 + 1];
					float d3r = buf[j + s6], d3i = buf[j + s6 + 1];

					float y1r = (-c2) * d1r - s2w * d1i;
					float y1i = s2w * d1r + (-c2) * d1i;
					float y2r = s1 * d2r - c1 * d2i;
					float y2i = c1 * d2r + s1 * d2i;
					float y3r = (-s3) * d3r + c3 * d3i;
					float y3i = (-c3) * d3r + (-s3) * d3i;

					float q0r = c0r + y1r, q0i = c0i + y1i;
					float q1r = c0r - y1r, q1i = c0i - y1i;
					float q2r = y2r + y3r, q2i = y2i + y3i;
					float q3r = y2r - y3r, q3i = y2i - y3i;

					buf[j]          = q0r + q2r; buf[j + 1]      = q0i + q2i;
					buf[j + s2]     = q1r - q3i; buf[j + s2 + 1] = q1i + q3r;
					buf[j + s4]     = q0r - q2r; buf[j + s4 + 1] = q0i - q2i;
					buf[j + s6]     = q1r + q3i; buf[j + s6 + 1] = q1i - q3r;
				}
			}

			for (int32_t i = mid; i < n2; i += s8) {
				float r0 = buf[i],      i0 = buf[i + 1];
				float r1 = buf[i + s2], i1 = buf[i + s2 + 1];
				float r2 = buf[i + s4], i2 = buf[i + s4 + 1];
				float r3 = buf[i + s6], i3 = buf[i + s6 + 1];

				float y2r = (r2 - i2) * 0.7071068f;
				float y2i = (r2 + i2) * 0.7071068f;
				float y3r = (-r3 - i3) * 0.7071068f;
				float y3i = (r3 - i3) * 0.7071068f;

				float p0r = r0 - i1, p0i = i0 + r1;
				float p1r = r0 + i1, p1i = i0 - r1;
				float p2r = y2r + y3r, p2i = y2i + y3i;
				float p3r = y2r - y3r, p3i = y2i - y3i;

				buf[i]          = p0r + p2r; buf[i + 1]      = p0i + p2i;
				buf[i + s2]     = p1r - p3i; buf[i + s2 + 1] = p1i + p3r;
				buf[i + s4]     = p0r - p2r; buf[i + s4 + 1] = p0i - p2i;
				buf[i + s6]     = p1r + p3i; buf[i + s6 + 1] = p1i - p3r;
			}
		}

		step *= 4;
	} while (step < n);
}

static AAC_NOINLINE int32_t aacImdct(uint32_t winSeq, uint8_t *winShapePair, float *overlap,
				     float *outPcm, AacCore *core)
{
	float *fftBuf = core->scratch.u.imdct.fftBuf;

	if (winSeq == 2u) {
		/* Eight short sequence (8 x 256 IMDCT) */
		float *shortTmp = core->scratch.u.imdct.shortWork;
		aacMdctPreTwiddle(core->curPcmOut, fftBuf, 8, 256);
		for (int32_t b = 0; b < 8; b++)
			aacFftRadix4(&fftBuf[128 * b], 64);

		sceClibMemcpy_safe(shortTmp, &overlap[448], 576u * sizeof(float));
		sceClibMemset(&shortTmp[576], 0, 576u * sizeof(float));

		float sStep = audioSinf(0.02454369329f);
		float cStep = audioSqrtf(1.0f - sStep * sStep);
		float sInit = audioSinf(0.006135923322f);
		float cInit = audioSqrtf(1.0f - sInit * sInit);
		float c = cInit * 0.0078125f;
		float s = sInit * 0.0078125f;

		const float *wPrev = core->winTables[2 + winShapePair[1]];
		const float *wCur = core->winTables[2 + winShapePair[0]];

		/* Block 0: first half (k = 0..31) */
		for (int32_t k = 0; k < 32; k += 2) {
			float c1 = cStep * c - sStep * s;
			float s1 = sStep * c + cStep * s;
			float xr0 = fftBuf[2 * k],     xi0 = fftBuf[2 * k + 1];
			float xr1 = fftBuf[2 * k + 2], xi1 = fftBuf[2 * k + 3];
			float t0 = xr0 * s + xi0 * c;
			float t1 = xr0 * c - xi0 * s;
			float t2 = xr1 * s1 + xi1 * c1;
			float t3 = xr1 * c1 - xi1 * s1;

			shortTmp[64 + k]     += t0 * wPrev[64 + k];
			shortTmp[64 + k + 2] += t2 * wPrev[64 + k + 2];
			shortTmp[63 - k]     -= t0 * wPrev[63 - k];
			shortTmp[61 - k]     -= t2 * wPrev[61 - k];

			shortTmp[192 + k]     += t1 * wCur[63 - k];
			shortTmp[192 + k + 2] += t3 * wCur[61 - k];
			shortTmp[191 - k]     += t1 * wCur[64 + k];
			shortTmp[189 - k]     += t3 * wCur[64 + k + 2];

			c = cStep * c1 - sStep * s1;
			s = sStep * c1 + cStep * s1;
		}
		/* Block 0: second half (k = 0..31) */
		for (int32_t k = 0; k < 32; k += 2) {
			float c1 = cStep * c - sStep * s;
			float s1 = sStep * c + cStep * s;
			float xr0 = fftBuf[64 + 2 * k],     xi0 = fftBuf[64 + 2 * k + 1];
			float xr1 = fftBuf[64 + 2 * k + 2], xi1 = fftBuf[64 + 2 * k + 3];
			float t0 = xr0 * c - xi0 * s;
			float t1 = xr0 * s + xi0 * c;
			float t2 = xr1 * c1 - xi1 * s1;
			float t3 = xr1 * s1 + xi1 * c1;

			shortTmp[k]           -= t0 * wPrev[k];
			shortTmp[k + 2]       -= t2 * wPrev[k + 2];
			shortTmp[127 - k]     += t0 * wPrev[127 - k];
			shortTmp[125 - k]     += t2 * wPrev[125 - k];

			shortTmp[128 + k]     += t1 * wCur[127 - k];
			shortTmp[128 + k + 2] += t3 * wCur[125 - k];
			shortTmp[255 - k]     += t1 * wCur[k];
			shortTmp[253 - k]     += t3 * wCur[k + 2];

			c = cStep * c1 - sStep * s1;
			s = sStep * c1 + cStep * s1;
		}

		/* Blocks 1..7 */
		sInit = audioSinf(0.006135923322f);
		cInit = audioSqrtf(1.0f - sInit * sInit);
		c = cInit * 0.0078125f;
		s = sInit * 0.0078125f;

		for (int32_t k = 0; k < 32; k += 2) {
			float c1 = cStep * c - sStep * s;
			float s1 = sStep * c + cStep * s;
			float w0 = wCur[64 + k],     w1 = wCur[64 + k + 2];
			float w2 = wCur[63 - k],     w3 = wCur[61 - k];
			for (int32_t b = 1; b < 8; b++) {
				int32_t base = 128 * b;
				float xr0 = fftBuf[base + 2 * k],     xi0 = fftBuf[base + 2 * k + 1];
				float xr1 = fftBuf[base + 2 * k + 2], xi1 = fftBuf[base + 2 * k + 3];
				float t0 = xr0 * s + xi0 * c;
				float t1 = xr0 * c - xi0 * s;
				float t2 = xr1 * s1 + xi1 * c1;
				float t3 = xr1 * c1 - xi1 * s1;
				shortTmp[base + 64 + k]      += t0 * w0;
				shortTmp[base + 64 + k + 2]  += t2 * w1;
				shortTmp[base + 63 - k]      -= t0 * w2;
				shortTmp[base + 61 - k]      -= t2 * w3;
				shortTmp[base + 192 + k]     += t1 * w2;
				shortTmp[base + 192 + k + 2] += t3 * w3;
				shortTmp[base + 191 - k]     += t1 * w0;
				shortTmp[base + 189 - k]     += t3 * w1;
			}
			c = cStep * c1 - sStep * s1;
			s = sStep * c1 + cStep * s1;
		}
		for (int32_t k = 0; k < 32; k += 2) {
			float c1 = cStep * c - sStep * s;
			float s1 = sStep * c + cStep * s;
			float w0 = wCur[k],          w1 = wCur[k + 2];
			float w2 = wCur[127 - k],    w3 = wCur[125 - k];
			for (int32_t b = 1; b < 8; b++) {
				int32_t base = 128 * b;
				float xr0 = fftBuf[base + 64 + 2 * k],     xi0 = fftBuf[base + 64 + 2 * k + 1];
				float xr1 = fftBuf[base + 64 + 2 * k + 2], xi1 = fftBuf[base + 64 + 2 * k + 3];
				float t0 = xr0 * c - xi0 * s;
				float t1 = xr0 * s + xi0 * c;
				float t2 = xr1 * c1 - xi1 * s1;
				float t3 = xr1 * s1 + xi1 * c1;
				shortTmp[base + k]           -= t0 * w0;
				shortTmp[base + k + 2]       -= t2 * w1;
				shortTmp[base + 127 - k]     += t0 * w2;
				shortTmp[base + 125 - k]     += t2 * w3;
				shortTmp[base + 128 + k]     += t1 * w2;
				shortTmp[base + 128 + k + 2] += t3 * w3;
				shortTmp[base + 255 - k]     += t1 * w0;
				shortTmp[base + 253 - k]     += t3 * w1;
			}
			c = cStep * c1 - sStep * s1;
			s = sStep * c1 + cStep * s1;
		}

		winShapePair[1] = winShapePair[0];
		sceClibMemcpy_safe(outPcm, overlap, 448u * sizeof(float));
		sceClibMemcpy_safe(&outPcm[448], shortTmp, 576u * sizeof(float));
		sceClibMemcpy_safe(overlap, &shortTmp[576], 576u * sizeof(float));
		sceClibMemset(&overlap[576], 0, 448u * sizeof(float));
		return 0;
	}

	if (winSeq > 3u)
		return -1200;

	/* Long / Long-Start / Long-Stop sequence (2048 IMDCT) */
	aacMdctPreTwiddle(core->curPcmOut, fftBuf, 1, 2048);
	aacFftRadix4(fftBuf, 512);

	uint32_t prevGrp = kAacLongWinPrevShape[winSeq];
	uint32_t nextGrp = kAacLongWinNextShape[winSeq];
	const float *wPrev = core->winTables[2u * prevGrp + winShapePair[1]];
	const float *wNext = core->winTables[2u * nextGrp + winShapePair[0]];

	float sInit = audioSinf(0.0007669904153f);
	float cInit = audioSqrtf(1.0f - sInit * sInit);
	float c = cInit * 0.0009765625f;
	float s = sInit * 0.0009765625f;
	float sStep = audioSinf(0.003067961661f);
	float cStep = audioSqrtf(1.0f - sStep * sStep);

	for (int32_t k = 0; k < 256; k += 2) {
		float c1 = cStep * c - sStep * s;
		float s1 = sStep * c + cStep * s;
		float xr0 = fftBuf[2 * k],     xi0 = fftBuf[2 * k + 1];
		float xr1 = fftBuf[2 * k + 2], xi1 = fftBuf[2 * k + 3];
		float t0 = xr0 * s + xi0 * c;
		float t1 = xr0 * c - xi0 * s;
		float t2 = xr1 * s1 + xi1 * c1;
		float t3 = xr1 * c1 - xi1 * s1;

		outPcm[512 + k]     = overlap[512 + k]     + t0 * wPrev[512 + k];
		outPcm[512 + k + 2] = overlap[512 + k + 2] + t2 * wPrev[512 + k + 2];
		outPcm[511 - k]     = overlap[511 - k]     - t0 * wPrev[511 - k];
		outPcm[509 - k]     = overlap[509 - k]     - t2 * wPrev[509 - k];

		overlap[512 + k]     = t1 * wNext[511 - k];
		overlap[512 + k + 2] = t3 * wNext[509 - k];
		overlap[511 - k]     = t1 * wNext[512 + k];
		overlap[509 - k]     = t3 * wNext[512 + k + 2];

		c = cStep * c1 - sStep * s1;
		s = sStep * c1 + cStep * s1;
	}
	for (int32_t k = 0; k < 256; k += 2) {
		float c1 = cStep * c - sStep * s;
		float s1 = sStep * c + cStep * s;
		float xr0 = fftBuf[512 + 2 * k],     xi0 = fftBuf[512 + 2 * k + 1];
		float xr1 = fftBuf[512 + 2 * k + 2], xi1 = fftBuf[512 + 2 * k + 3];
		float t0 = xr0 * c - xi0 * s;
		float t1 = xr0 * s + xi0 * c;
		float t2 = xr1 * c1 - xi1 * s1;
		float t3 = xr1 * s1 + xi1 * c1;

		outPcm[k]           = overlap[k]           - t0 * wPrev[k];
		outPcm[k + 2]       = overlap[k + 2]       - t2 * wPrev[k + 2];
		outPcm[1023 - k]    = overlap[1023 - k]    + t0 * wPrev[1023 - k];
		outPcm[1021 - k]    = overlap[1021 - k]    + t2 * wPrev[1021 - k];

		overlap[k]          = t1 * wNext[1023 - k];
		overlap[k + 2]      = t3 * wNext[1021 - k];
		overlap[1023 - k]   = t1 * wNext[k];
		overlap[1021 - k]   = t3 * wNext[k + 2];

		c = cStep * c1 - sStep * s1;
		s = sStep * c1 + cStep * s1;
	}

	winShapePair[1] = winShapePair[0];
	return 0;
}


/* =========================================================================
 * SBR (Spectral Band Replication) decoder
 * ========================================================================= */

static AAC_NOINLINE int32_t sbrProcessChannel(AacCore *core, SbrElement *elem,
					      SbrChannel *ch, float *outPcm)
{
	int32_t ret = sbrQmfAnalysis(core, elem, ch, outPcm);
	if (ret != 0)
		return ret;

	if (ch->headerReady == 1) {
		ret = sbrHfGenerate(core, elem, ch);
		if (ret != 0)
			return ret;
		ret = sbrCalculateGainAndAdjust(core, elem, ch);
		if (ret != 0)
			return ret;
	} else {
		for (int32_t t = 0; t < 40; t++)
			sceClibMemset(ch->qmfWork->xHigh[t], 0, sizeof(ch->qmfWork->xHigh[t]));
	}

	int32_t tBorder, kxPrev, kxCur;
	if (ch->headerReady == 1) {
		tBorder = ch->tE[0];
		kxPrev = elem->kxPrev;
		kxCur = elem->kx;
	} else {
		tBorder = 32;
		kxPrev = 32;
		kxCur = 32;
	}

	SbrQmfWork *qw = ch->qmfWork;
	for (int32_t t = 0; t < tBorder; t++) {
		int32_t srcT = (int32_t)elem->tHfAdj + t;
		for (int32_t k = 0; k < kxPrev; k++) {
			core->scratch.u.sbr.qmfOut[t][k][0] = qw->xLow[srcT][k][0];
			core->scratch.u.sbr.qmfOut[t][k][1] = qw->xLow[srcT][k][1];
		}
		for (int32_t k = kxPrev; k < 64; k++) {
			core->scratch.u.sbr.qmfOut[t][k][0] = qw->xHigh[srcT][k][0];
			core->scratch.u.sbr.qmfOut[t][k][1] = qw->xHigh[srcT][k][1];
		}
	}
	for (int32_t t = tBorder; t < 32; t++) {
		int32_t srcT = (int32_t)elem->tHfAdj + t;
		for (int32_t k = 0; k < kxCur; k++) {
			core->scratch.u.sbr.qmfOut[t][k][0] = qw->xLow[srcT][k][0];
			core->scratch.u.sbr.qmfOut[t][k][1] = qw->xLow[srcT][k][1];
		}
		for (int32_t k = kxCur; k < 64; k++) {
			core->scratch.u.sbr.qmfOut[t][k][0] = qw->xHigh[srcT][k][0];
			core->scratch.u.sbr.qmfOut[t][k][1] = qw->xHigh[srcT][k][1];
		}
	}

	sceClibMemcpy_safe(ch->xLowSave, &qw->xLow[elem->numTimeSlots2], 0x800u);
	sceClibMemcpy_safe(ch->xHighSave, &qw->xHigh[elem->numTimeSlots2], 0x1000u);

	if (core->chanMap.sbrDownsample != 0) {
		sbrQmfSynth32(core, elem, ch, &core->scratch.u.sbr.qmfOut[0][0][0], outPcm);
	} else {
		sbrQmfSynth64(core, elem, ch, &core->scratch.u.sbr.qmfOut[0][0][0], outPcm);
	}

	if (ch->headerReady != 1)
		return 0;

	if (elem->elemType == 0 || ch->isSecondCh == 1) {
		elem->kxPrev = elem->kx;
		elem->mPrev = elem->mBand;
	}

	uint8_t le = ch->le;
	ch->prevLe = le;
	ch->prevLq = ch->lq;
	for (int32_t i = 0; i < 5; i++)
		ch->prevInvfMode[i] = ch->invfMode[i];
	for (int32_t i = 0; i < 64; i++)
		ch->prevAddHarmonic[i] = ch->addHarmonic[i];
	ch->prevAddHarmonicFlag = ch->addHarmonicFlag;
	for (int32_t i = 0; i < 6; i++)
		ch->prevTe[i] = ch->tE[i];
	for (int32_t i = 0; i < 64; i++) {
		ch->prevBwArray[i] = core->sbrBwArray[i];
		ch->prevSines[i] = core->sbrSines[le - 1][i];
	}
	for (int32_t i = 0; i < 8; i++)
		ch->prevFreqRes[i] = ch->freqRes[i];
	ch->prevLa = (ch->lA == (int8_t)le) ? 0 : -1;
	return 0;
}

static AAC_NOINLINE int32_t sbrReadInvf(AacCore *core, SbrChannel *ch)
{
	if (ch->nq > 5u)
		return -7700;
	for (int32_t n = 0; n < (int32_t)ch->nq; n++) {
		ch->invfMode[n] = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrReadHuff(AacCore *core, int32_t tableIdx, int32_t *outVal)
{
	uint32_t offset = kSbrHuffOffsets[tableIdx];
	const SbrHuffEntry *entry = kSbrHuffTables[tableIdx];
	uint32_t code = 0;
	int32_t curLen = 0;
	uint32_t limit = 2u * offset + 1u;

	for (uint32_t i = 0; i < limit; i++, entry++) {
		int32_t delta = (int32_t)entry->length - curLen;
		if (delta > 0) {
			uint32_t bits = aacGetBits((uint8_t)delta, core);
			code = bits | (code << delta);
			if (core->br.eof == 1)
				return -10000;
			curLen = (int32_t)entry->length;
		}
		if (entry->cw == code) {
			*outVal = (int32_t)entry->index - (int32_t)offset;
			return 0;
		}
	}
	*outVal = 0;
	return 0;
}

static AAC_NOINLINE int32_t sbrReadEnvelope(AacCore *core, const SbrElement *elem,
					    SbrChannel *ch, int32_t isRight)
{
	int32_t freqTab, timeTab;
	if (elem->coupling != 0 && isRight != 0) {
		if (ch->ampRes != 0) {
			freqTab = 7;
			timeTab = 6;
		} else {
			freqTab = 3;
			timeTab = 2;
		}
	} else if (ch->ampRes != 0) {
		freqTab = 5;
		timeTab = 4;
	} else {
		freqTab = 1;
		timeTab = 0;
	}

	if (ch->numEnv > 5u)
		return -7800;

	for (int32_t e = 0; e < (int32_t)ch->numEnv; e++) {
		int32_t huffVal = 0;
		if (ch->dfEnv[e] == 0) {
			int32_t nbits;
			if (elem->coupling != 0 && isRight != 0)
				nbits = (ch->ampRes != 0) ? 5 : 6;
			else
				nbits = (ch->ampRes != 0) ? 6 : 7;

			ch->envRaw[e][0] = (int32_t)aacGetBits(nbits, core);
			if (core->br.eof == 1)
				return -10000;

			uint32_t nBand = elem->nFreq[ch->freqRes[e]];
			if (nBand > 64u)
				return -7800;
			for (int32_t k = 1; k < (int32_t)elem->nFreq[ch->freqRes[e]]; k++) {
				int32_t ret = sbrReadHuff(core, freqTab, &huffVal);
				if (ret != 0)
					return ret;
				ch->envRaw[e][k] = huffVal;
			}
		} else {
			uint32_t nBand = elem->nFreq[ch->freqRes[e]];
			if (nBand > 64u)
				return -7800;
			for (int32_t k = 0; k < (int32_t)elem->nFreq[ch->freqRes[e]]; k++) {
				int32_t ret = sbrReadHuff(core, timeTab, &huffVal);
				if (ret != 0)
					return ret;
				ch->envRaw[e][k] = huffVal;
			}
		}
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrReadGrid(AacCore *core, SbrChannel *ch)
{
	uint8_t cls = (uint8_t)aacGetBits(2, core);
	ch->frameClass = cls;
	if (core->br.eof == 1)
		return -10000;

	int32_t ret = 0;
	switch (cls) {
	case 0: {
		uint32_t tmp = aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		int32_t nEnv = 1 << tmp;
		if (nEnv <= 3) {
			ch->numEnv = (uint8_t)nEnv;
			if ((uint8_t)nEnv == 1)
				ch->ampRes = 0;
		} else {
			ch->numEnv = 4;
		}
		uint8_t r = (uint8_t)aacGetBits(1, core);
		ch->freqRes[0] = r;
		if (core->br.eof == 1)
			return -10000;
		for (int32_t e = 1; e < (int32_t)ch->numEnv; e++)
			ch->freqRes[e] = ch->freqRes[0];
		break;
	}
	case 1: {
		ch->varBord1 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		uint8_t nRel = (uint8_t)aacGetBits(2, core);
		ch->numRel1 = nRel;
		if (core->br.eof == 1)
			return -10000;
		uint32_t nEnv = (uint8_t)(nRel + 1u);
		if (nEnv >= 4u)
			nEnv = 4u;
		ch->numEnv = (uint8_t)nEnv;
		for (int32_t i = 0; i < (int32_t)ch->numEnv - 1; i++) {
			uint8_t rel = (uint8_t)aacGetBits(2, core);
			if (core->br.eof == 1)
				return -10000;
			ch->relBord1[i] = (uint8_t)(2u * (rel + 1u));
		}
		ch->pointer = (uint8_t)aacGetBits(kSbrPointerBits[ch->numEnv], core);
		if (core->br.eof == 1)
			return -10000;
		for (int32_t e = 0; e < (int32_t)ch->numEnv; e++) {
			ch->freqRes[ch->numEnv - 1 - e] = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;
		}
		break;
	}
	case 2: {
		ch->varBord0 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		uint8_t nRel = (uint8_t)aacGetBits(2, core);
		ch->numRel0 = nRel;
		if (core->br.eof == 1)
			return -10000;
		uint32_t nEnv = (uint8_t)(nRel + 1u);
		if (nEnv >= 4u)
			nEnv = 4u;
		ch->numEnv = (uint8_t)nEnv;
		for (int32_t i = 0; i < (int32_t)ch->numEnv - 1; i++) {
			uint8_t rel = (uint8_t)aacGetBits(2, core);
			if (core->br.eof == 1)
				return -10000;
			ch->relBord0[i] = (uint8_t)(2u * (rel + 1u));
		}
		ch->pointer = (uint8_t)aacGetBits(kSbrPointerBits[ch->numEnv], core);
		if (core->br.eof == 1)
			return -10000;
		for (int32_t e = 0; e < (int32_t)ch->numEnv; e++) {
			ch->freqRes[e] = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;
		}
		break;
	}
	case 3: {
		ch->varBord0 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		ch->varBord1 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		ch->numRel0 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		ch->numRel1 = (uint8_t)aacGetBits(2, core);
		if (core->br.eof == 1)
			return -10000;
		uint32_t nEnv = (uint8_t)(ch->numRel0 + 1u + ch->numRel1);
		if (nEnv >= 5u)
			nEnv = 5u;
		ch->numEnv = (uint8_t)nEnv;
		for (int32_t i = 0; i < (int32_t)ch->numRel0; i++) {
			uint8_t rel = (uint8_t)aacGetBits(2, core);
			if (core->br.eof == 1)
				return -10000;
			ch->relBord0[i] = (uint8_t)(2u * (rel + 1u));
		}
		for (int32_t i = 0; i < (int32_t)ch->numRel1; i++) {
			uint8_t rel = (uint8_t)aacGetBits(2, core);
			if (core->br.eof == 1)
				return -10000;
			ch->relBord1[i] = (uint8_t)(2u * (rel + 1u));
		}
		ch->pointer = (uint8_t)aacGetBits(kSbrPointerBits[ch->numEnv], core);
		if (core->br.eof == 1)
			return -10000;
		for (int32_t e = 0; e < (int32_t)ch->numEnv; e++) {
			ch->freqRes[e] = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;
		}
		break;
	}
	default:
		ret = -7600;
		break;
	}

	uint8_t nEnv = ch->numEnv;
	ch->le = nEnv;
	if (nEnv <= 1u) {
		if (nEnv == 0u)
			ret = -7600;
		ch->numNoise = 1;
		ch->lq = 1;
	} else {
		ch->numNoise = 2;
		ch->lq = 2;
	}
	return ret;
}

static AAC_NOINLINE int32_t sbrReadDtdf(AacCore *core, SbrChannel *ch)
{
	for (int32_t e = 0; e < (int32_t)ch->numEnv; e++) {
		ch->dfEnv[e] = (uint8_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
	}
	for (int32_t q = 0; q < (int32_t)ch->numNoise; q++) {
		ch->dfNoise[q] = (uint8_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrReadNoise(AacCore *core, const uint8_t *couplingFlag,
					 SbrChannel *ch, int32_t isRight)
{
	int32_t freqTab = 5;
	int32_t timeTab = 8;
	if (*couplingFlag != 0 && isRight != 0) {
		freqTab = 7;
		timeTab = 9;
	}

	if (ch->nq > 64u)
		return -7900;

	for (int32_t q = 0; q < (int32_t)ch->numNoise; q++) {
		int32_t huffVal = 0;
		if (ch->dfNoise[q] != 0) {
			for (int32_t k = 0; k < (int32_t)ch->nq; k++) {
				int32_t ret = sbrReadHuff(core, timeTab, &huffVal);
				if (ret != 0)
					return ret;
				ch->noiseRaw[q][k] = huffVal;
			}
		} else {
			ch->noiseRaw[q][0] = (int32_t)aacGetBits(5, core);
			if (core->br.eof == 1)
				return -10000;
			for (int32_t k = 1; k < (int32_t)ch->nq; k++) {
				int32_t ret = sbrReadHuff(core, freqTab, &huffVal);
				if (ret != 0)
					return ret;
				ch->noiseRaw[q][k] = huffVal;
			}
		}
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrReadSinusoidal(AacCore *core, const uint8_t *nHighPtr,
					      SbrChannel *ch)
{
	if (*nHighPtr > 64u)
		return -8000;
	for (int32_t k = 0; k < (int32_t)*nHighPtr; k++) {
		ch->addHarmonic[k] = (uint8_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
	}
	return 0;
}

static inline __attribute__((always_inline)) void
sbrSkipExtensionTail(AacCore *core, int32_t countBytes, int32_t startBits)
{
	int32_t bitsRead = aacGetBitCount(core) - startBits;
	int32_t rem = 8 * countBytes - 4 - bitsRead;
	while (rem > 7) {
		aacGetBits(8, core);
		rem -= 8;
	}
	if (rem > 0)
		aacGetBits((uint8_t)rem, core);
}

static inline __attribute__((always_inline)) int32_t
sbrReadExtData(AacCore *core, SbrElement *elem)
{
	uint32_t extPresent = aacGetBits(1, core);
	if (core->br.eof == 1)
		return -10000;
	if (extPresent == 0)
		return 0;

	uint32_t extSize = aacGetBits(4, core);
	if (core->br.eof == 1)
		return -10000;
	if (extSize == 15u) {
		extSize = aacGetBits(8, core) + 15u;
		if (core->br.eof == 1)
			return -10000;
	}
	int32_t remBits = (int32_t)(8u * extSize);
	while (remBits > 7) {
		elem->extId = (uint8_t)aacGetBits(2, core);
		remBits -= 8;
		if (core->br.eof == 1)
			return -10000;
		elem->extData = (uint8_t)aacGetBits(6, core);
		if (core->br.eof == 1)
			return -10000;
	}
	if (remBits > 0) {
		aacGetBits((uint8_t)remBits, core);
		if (core->br.eof == 1)
			return -10000;
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrDecodeExtension(AacCore *core, int32_t countBytes, int32_t hasCrc)
{
	uint8_t elemIdx = core->numSbrElems;
	int32_t startBits = aacGetBitCount(core);
	uint8_t chSlot = core->curElemChan;
	SbrElement *elem = &core->sbrElems[elemIdx];
	SbrChannel *ch0 = core->sbrChanPtr[chSlot];

	core->sbrChanToElem[chSlot] = elemIdx;
	if (elem->elemType == 1)
		core->sbrChanToElem[chSlot + 1] = elemIdx;
	core->numSbrElems = (uint8_t)(elemIdx + 1u);

	uint32_t sbrSr = 2u * (uint32_t)kAacSampleRateInfo[core->chanMap.srIdx].sampleRate;
	core->sbrSampleRate = sbrSr;
	int32_t srIdx = sbrGetSrIndex(sbrSr);
	if (srIdx < 0)
		return srIdx;
	core->sbrSrIdx = (uint8_t)srIdx;
	core->sbrActiveFlag = 1;

	int32_t crcOk = 1;
	if (hasCrc != 0) {
		uint32_t crcBits = aacGetBits(10, core);
		if (core->br.eof == 1)
			return -10000;
		if (countBytes <= 1)
			return -7300;
		int32_t bitsLeft = core->br.bitsLeft;
		uint32_t payloadBits = (uint32_t)(8 * countBytes - 14);
		uint32_t startBit = (uint32_t)(-bitsLeft) & 7u;
		if (payloadBits < 8u - startBit)
			return -7300;
		const uint8_t *startPtr = core->br.ptr - ((uint32_t)(bitsLeft + 7) >> 3);
		const uint8_t *endPtr = startPtr + (((payloadBits + startBit - 8u) >> 3) + 1u);
		uint32_t calc = sbrCrc10Calc(startPtr, startBit, endPtr,
					     (payloadBits + startBit) & 7u, 0, 0);
		if (crcBits != calc)
			crcOk = 0;
	}

	if (crcOk && elem->extType != 3) {
		uint8_t hdrFlag = (uint8_t)aacGetBits(1, core);
		elem->headerFlag = hdrFlag;
		if (core->br.eof == 1)
			return -10000;
		if (hdrFlag != 0) {
			elem->ampRes = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;
			elem->startFreq = (uint8_t)aacGetBits(4, core);
			if (core->br.eof == 1)
				return -10000;
			elem->stopFreq = (uint8_t)aacGetBits(4, core);
			if (core->br.eof == 1)
				return -10000;
			elem->xoverBand = (uint8_t)aacGetBits(3, core);
			if (core->br.eof == 1)
				return -10000;
			aacGetBits(2, core);
			if (core->br.eof == 1)
				return -10000;
			uint8_t extra1 = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;
			uint8_t extra2 = (uint8_t)aacGetBits(1, core);
			if (core->br.eof == 1)
				return -10000;

			if (extra1 != 0) {
				elem->freqScale = (uint8_t)aacGetBits(2, core);
				if (core->br.eof == 1)
					return -10000;
				elem->alterScale = (uint8_t)aacGetBits(1, core);
				if (core->br.eof == 1)
					return -10000;
				elem->noiseBands = (uint8_t)aacGetBits(2, core);
				if (core->br.eof == 1)
					return -10000;
			} else {
				elem->freqScale = 2;
				elem->alterScale = 1;
				elem->noiseBands = 2;
			}

			if (extra2 != 0) {
				elem->limiterBands = (uint8_t)aacGetBits(2, core);
				if (core->br.eof == 1)
					return -10000;
				elem->limiterGains = (uint8_t)aacGetBits(2, core);
				if (core->br.eof == 1)
					return -10000;
				elem->interpFreq = (uint8_t)aacGetBits(1, core);
				if (core->br.eof == 1)
					return -10000;
				elem->smoothingMode = (uint8_t)aacGetBits(1, core);
				if (core->br.eof == 1)
					return -10000;
			} else {
				elem->limiterBands = 2;
				elem->limiterGains = 2;
				elem->interpFreq = 1;
				elem->smoothingMode = 1;
			}

			if (core->sbrChanPtr[core->curElemChan]->headerReady == 1 &&
			    elem->startFreq == elem->prevStartFreq &&
			    elem->stopFreq == elem->prevStopFreq &&
			    elem->freqScale == elem->prevFreqScale &&
			    elem->alterScale == elem->prevAlterScale &&
			    elem->xoverBand == elem->prevXoverBand &&
			    elem->noiseBands == elem->prevNoiseBands) {
				core->sbrResetFlag = 0;
			} else {
				core->sbrResetFlag = 1;
			}

			elem->prevStartFreq = elem->startFreq;
			elem->prevStopFreq = elem->stopFreq;
			elem->prevFreqScale = elem->freqScale;
			elem->prevAlterScale = elem->alterScale;
			elem->prevXoverBand = elem->xoverBand;
			elem->prevNoiseBands = elem->noiseBands;
		}
	}

	if (core->sbrResetFlag == 1) {
		int32_t ret = sbrCalcMasterFreqTable(core, elem);
		if (ret != 0)
			return ret;
		ret = sbrCalcDerivedFreqTable(core, elem);
		if (ret != 0)
			return ret;
		ret = sbrCalcNoiseFreqTable(core, elem, ch0);
		if (ret != 0)
			return ret;
		ch0->headerReady = 1;
	} else if (ch0->headerReady != 1) {
		sbrSkipExtensionTail(core, countBytes, startBits);
		return countBytes;
	}

	if (elem->extType != 0)
		return -7400;

	if (elem->elemType == 0) {
		/* Single-channel SBR element */
		uint32_t dataExtra = aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
		if (dataExtra != 0) {
			aacGetBits(4, core);
			if (core->br.eof == 1)
				return -10000;
		}
		ch0->ampRes = elem->ampRes;
		int32_t ret = sbrReadGrid(core, ch0);
		if (ret != 0)
			return ret;
		ret = sbrCalcTimeBorders(core, elem, ch0);
		if (ret != 0)
			return ret;
		ret = sbrReadDtdf(core, ch0);
		if (ret != 0)
			return ret;
		ret = sbrReadInvf(core, ch0);
		if (ret != 0)
			return ret;
		ret = sbrReadEnvelope(core, elem, ch0, 0);
		if (ret != 0)
			return ret;
		ret = sbrReadNoise(core, &elem->coupling, ch0, 0);
		if (ret != 0)
			return ret;

		uint8_t harmFlag = (uint8_t)aacGetBits(1, core);
		ch0->addHarmonicFlag = harmFlag;
		if (core->br.eof == 1)
			return -10000;
		if (harmFlag != 0) {
			ret = sbrReadSinusoidal(core, &elem->nHigh, ch0);
			if (ret != 0)
				return ret;
		} else {
			sceClibMemset(ch0->addHarmonic, 0, elem->nHigh);
		}

		ret = sbrReadExtData(core, elem);
		if (ret != 0)
			return ret;

		ret = sbrDeltaDecode(core, elem, ch0, 0);
		if (ret != 0)
			return ret;
		ret = sbrDequantUncoupled(core, elem, ch0);
		if (ret != 0)
			return ret;

		sbrSkipExtensionTail(core, countBytes, startBits);
		return countBytes;
	}

	if (elem->elemType == 1) {
		/* Channel-pair SBR element */
		SbrChannel *ch1 = ch0 + 1;
		ch1->headerReady = 1;
		ch1->isSecondCh = 1;

		uint32_t dataExtra = aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;
		if (dataExtra != 0) {
			aacGetBits(4, core);
			if (core->br.eof == 1)
				return -10000;
			aacGetBits(4, core);
			if (core->br.eof == 1)
				return -10000;
		}
		elem->coupling = (uint8_t)aacGetBits(1, core);
		if (core->br.eof == 1)
			return -10000;

		ch0->ampRes = elem->ampRes;
		ch1->ampRes = elem->ampRes;

		int32_t ret;
		if (elem->coupling != 1) {
			ret = sbrReadGrid(core, ch0);
			if (ret != 0)
				return ret;
			ret = sbrReadGrid(core, ch1);
			if (ret != 0)
				return ret;
			ret = sbrCalcTimeBorders(core, elem, ch0);
			if (ret != 0)
				return ret;
			ret = sbrCalcTimeBorders(core, elem, ch1);
			if (ret != 0)
				return ret;
			ch1->nq = ch0->nq;
			ret = sbrReadDtdf(core, ch0);
			if (ret != 0)
				return ret;
			ret = sbrReadDtdf(core, ch1);
			if (ret != 0)
				return ret;
			ret = sbrReadInvf(core, ch0);
			if (ret != 0)
				return ret;
			ret = sbrReadInvf(core, ch1);
			if (ret != 0)
				return ret;
			ret = sbrReadEnvelope(core, elem, ch0, 0);
			if (ret != 0)
				return ret;
			ret = sbrReadEnvelope(core, elem, ch1, 1);
			if (ret != 0)
				return ret;
			ret = sbrReadNoise(core, &elem->coupling, ch0, 0);
			if (ret != 0)
				return ret;
		} else {
			ret = sbrReadGrid(core, ch0);
			if (ret != 0)
				return ret;
			ret = sbrCalcTimeBorders(core, elem, ch0);
			if (ret != 0)
				return ret;

			ch1->frameClass = ch0->frameClass;
			ch1->le = ch0->le;
			ch1->numEnv = ch0->numEnv;
			ch1->lq = ch0->lq;
			ch1->numNoise = ch0->numNoise;
			ch1->pointer = ch0->pointer;
			ch1->nq = ch0->nq;
			ch1->ampRes = ch0->ampRes;
			for (int32_t e = 0; e <= (int32_t)ch0->le; e++) {
				ch1->tE[e] = ch0->tE[e];
				ch1->freqRes[e] = ch0->freqRes[e];
			}
			for (int32_t q = 0; q <= (int32_t)ch0->lq; q++)
				ch1->tQ[q] = ch0->tQ[q];

			ret = sbrReadDtdf(core, ch0);
			if (ret != 0)
				return ret;
			ret = sbrReadDtdf(core, ch1);
			if (ret != 0)
				return ret;
			ret = sbrReadInvf(core, ch0);
			if (ret != 0)
				return ret;
			if (ch0->nq > 5u)
				return -7500;
			for (int32_t n = 0; n < (int32_t)ch0->nq; n++)
				ch1->invfMode[n] = ch0->invfMode[n];

			ret = sbrReadEnvelope(core, elem, ch0, 0);
			if (ret != 0)
				return ret;
			ret = sbrReadNoise(core, &elem->coupling, ch0, 0);
			if (ret != 0)
				return ret;
			ret = sbrReadEnvelope(core, elem, ch1, 1);
			if (ret != 0)
				return ret;
		}

		ret = sbrReadNoise(core, &elem->coupling, ch1, 1);
		if (ret != 0)
			return ret;

		uint8_t harm0 = (uint8_t)aacGetBits(1, core);
		ch0->addHarmonicFlag = harm0;
		if (core->br.eof == 1)
			return -10000;
		if (harm0 != 0) {
			ret = sbrReadSinusoidal(core, &elem->nHigh, ch0);
			if (ret != 0)
				return ret;
		} else {
			sceClibMemset(ch0->addHarmonic, 0, elem->nHigh);
		}

		uint8_t harm1 = (uint8_t)aacGetBits(1, core);
		ch1->addHarmonicFlag = harm1;
		if (core->br.eof == 1)
			return -10000;
		if (harm1 != 0) {
			ret = sbrReadSinusoidal(core, &elem->nHigh, ch1);
			if (ret != 0)
				return ret;
		} else {
			sceClibMemset(ch1->addHarmonic, 0, elem->nHigh);
		}

		ret = sbrReadExtData(core, elem);
		if (ret != 0)
			return ret;

		ret = sbrDeltaDecode(core, elem, ch0, 0);
		if (ret != 0)
			return ret;
		ret = sbrDeltaDecode(core, elem, ch1, 1);
		if (ret != 0)
			return ret;

		if (elem->coupling == 1) {
			ret = sbrDequantCoupled(core, elem, ch0, ch1);
			if (ret != 0)
				return ret;
		} else {
			ret = sbrDequantUncoupled(core, elem, ch0);
			if (ret != 0)
				return ret;
			ret = sbrDequantUncoupled(core, elem, ch1);
			if (ret != 0)
				return ret;
		}

		sbrSkipExtensionTail(core, countBytes, startBits);
		return countBytes;
	}

	return -7400;
}

static AAC_NOINLINE int sbrCompareInt(const void *a, const void *b)
{
	return *(const int32_t *)a - *(const int32_t *)b;
}

static AAC_NOINLINE int32_t sbrCalcMasterFreqTable(AacCore *core, SbrElement *elem)
{
	uint32_t sr = core->sbrSampleRate;
	int32_t srIdx = sbrGetSrIndex(sr);
	if (srIdx < 0)
		return srIdx;

	uint32_t stopFreq = elem->stopFreq;
	int32_t k0 = (uint8_t)(kSbrStartMin[srIdx] + kSbrStartOffset[16 * srIdx + elem->startFreq]);
	elem->k0 = (uint8_t)k0;

	int32_t k2;
	if (stopFreq <= 13u) {
		k2 = kSbrStopMin[srIdx];
		if (stopFreq != 0u)
			k2 += kSbrStopOffset[13 * srIdx + (int32_t)stopFreq - 1];
		if (k2 > 64)
			k2 = 64;
		else
			k2 = (uint8_t)k2;
		elem->k2 = (uint8_t)k2;
	} else if (stopFreq == 14u) {
		k2 = 2 * k0;
		if (k2 >= 64)
			k2 = 64;
		elem->k2 = (uint8_t)k2;
	} else if (stopFreq == 15u) {
		k2 = 3 * k0;
		if (k2 >= 64)
			k2 = 64;
		elem->k2 = (uint8_t)k2;
	} else {
		return -5700;
	}

	int32_t diff = k2 - k0;
	if (diff > 48)
		return -5700;
	if (sr == 44100u) {
		if (diff > 35)
			return -5700;
	} else if (sr >= 48000u && diff > 32) {
		return -5700;
	}

	int32_t *vDk0 = core->scratch.u.freq.bandDeltaLo;
	int32_t *vk0 = core->scratch.u.freq.bandEdgeLo;
	int32_t *vk1 = core->scratch.u.freq.bandEdgeHi;
	int32_t *vDk1 = core->scratch.u.freq.bandDeltaHi;

	if (elem->freqScale == 0) {
		int32_t dk, numBands;
		if (elem->alterScale != 0) {
			dk = 2;
			numBands = 2 * (int32_t)((float)diff * 0.25f + 0.5f);
		} else {
			dk = 1;
			numBands = 2 * (int32_t)((float)diff * 0.5f);
		}
		int32_t k2Achieved = k0 + dk * numBands;
		int32_t k2Diff = k2 - k2Achieved;
		for (int32_t i = 0; i < numBands; i++)
			vDk0[i] = dk;
		if (k2Diff > 0) {
			int32_t idx = numBands - 1;
			while (k2Diff != 0) {
				vDk0[idx]++;
				idx--;
				k2Diff--;
			}
		} else if (k2Diff < 0) {
			int32_t idx = 0;
			while (k2Diff != 0) {
				vDk0[idx]--;
				idx++;
				k2Diff++;
			}
		}
		elem->fMaster[0] = (uint8_t)k0;
		for (int32_t i = 0; i < numBands; i++)
			elem->fMaster[i + 1] = (uint8_t)(elem->fMaster[i] + vDk0[i]);
		elem->nMaster = (uint8_t)numBands;
		return 0;
	}

	float warp = kSbrWarpFactor[elem->alterScale];
	float bands = (float)kSbrBandsPerOctave[elem->freqScale];
	float ratio = (float)k2 / (float)k0;
	int32_t twoRegions = 0;
	int32_t k1 = k2;
	if (ratio > 2.2449f) {
		twoRegions = 1;
		k1 = 2 * k0;
	}
	float r0 = (float)k1 / (float)k0;
	int32_t nrBand0 = 2 * (int32_t)((bands * audioLogf(r0)) / (2.0f * audioLogf(2.0f)) + 0.5f);
	if ((uint32_t)(nrBand0 - 1) > 62u)
		return -5700;

	for (int32_t i = 0; i < nrBand0; i++) {
		int32_t hi = (int32_t)((float)k0 * audioPowf(r0, (float)(i + 1) / (float)nrBand0) + 0.5f);
		int32_t lo = (int32_t)((float)k0 * audioPowf(r0, (float)i / (float)nrBand0) + 0.5f);
		vDk0[i] = hi - lo;
	}
	audioQsort(vDk0, (size_t)nrBand0, 4u, sbrCompareInt);
	vk0[0] = k0;
	for (int32_t i = 0; i < nrBand0; i++)
		vk0[i + 1] = vk0[i] + vDk0[i];

	if (twoRegions == 0) {
		elem->nMaster = (uint8_t)nrBand0;
		for (int32_t i = 0; i <= nrBand0; i++)
			elem->fMaster[i] = (uint8_t)vk0[i];
		return 0;
	}

	float r1 = (float)k2 / (float)k1;
	int32_t nrBand1 = 2 * (int32_t)((bands * audioLogf(r1)) / (2.0f * audioLogf(2.0f) * warp) + 0.5f);
	if (nrBand1 > 63)
		return -5700;

	for (int32_t i = 0; i < nrBand1; i++) {
		int32_t hi = (int32_t)((float)k1 * audioPowf(r1, (float)(i + 1) / (float)nrBand1) + 0.5f);
		int32_t lo = (int32_t)((float)k1 * audioPowf(r1, (float)i / (float)nrBand1) + 0.5f);
		vDk1[i] = hi - lo;
	}
	if (vDk1[0] < vDk0[nrBand0 - 1]) {
		audioQsort(vDk1, (size_t)nrBand1, 4u, sbrCompareInt);
		int32_t change = vDk0[nrBand0 - 1] - vDk1[0];
		int32_t maxChg = (vDk1[nrBand1 - 1] - vDk1[0]) / 2;
		if (maxChg < change)
			change = maxChg;
		vDk1[0] += change;
		vDk1[nrBand1 - 1] -= change;
	}
	audioQsort(vDk1, (size_t)nrBand1, 4u, sbrCompareInt);
	vk1[0] = k1;
	for (int32_t i = 0; i < nrBand1; i++)
		vk1[i + 1] = vk1[i] + vDk1[i];

	uint32_t total = (uint8_t)(nrBand0 + nrBand1);
	elem->nMaster = (uint8_t)total;
	for (int32_t i = 0; i <= nrBand0; i++)
		elem->fMaster[i] = (uint8_t)vk0[i];
	if (total > 63u)
		return -5700;
	for (int32_t i = 1; i <= (int32_t)total - nrBand0; i++)
		elem->fMaster[nrBand0 + i] = (uint8_t)vk1[i];
	return 0;
}

static AAC_NOINLINE int32_t sbrCalcDerivedFreqTable(AacCore *core, SbrElement *elem)
{
	(void)core;
	int32_t xover = elem->xoverBand;
	uint32_t nHigh = (uint8_t)(elem->nMaster - xover);
	uint32_t nLow = (nHigh & 1u) + (nHigh >> 1);

	elem->nHigh = (uint8_t)nHigh;
	elem->nFreq[1] = (uint8_t)nHigh;
	elem->nLow = (uint8_t)nLow;
	elem->nFreq[0] = (uint8_t)nLow;

	if (nHigh > 63u || (int32_t)(nHigh + (uint32_t)xover) > 63)
		return -5800;

	for (uint32_t i = 0; i <= nHigh; i++)
		elem->fTable[1][i] = elem->fMaster[xover + (int32_t)i];

	elem->kx = elem->fTable[1][0];
	elem->fTable[0][0] = elem->fTable[1][0];
	elem->mBand = (uint8_t)(elem->fTable[1][nHigh] - elem->fTable[1][0]);

	if (nLow > 31u)
		return -5800;

	uint32_t odd = nHigh & 1u;
	for (uint32_t i = 1; i <= nLow; i++)
		elem->fTable[0][i] = elem->fTable[1][2u * i - odd];

	return 0;
}

static AAC_NOINLINE int32_t sbrCalcNoiseFreqTable(AacCore *core, SbrElement *elem, SbrChannel *ch)
{
	int32_t *idxTab = core->scratch.u.freq.bandDeltaLo;
	elem->fTableNoise[0] = elem->fTable[0][0];
	idxTab[0] = 0;

	if (elem->kx == 0)
		return -5900;

	int32_t nq = (int32_t)(((float)elem->noiseBands *
				audioLogf((float)elem->k2 / (float)elem->kx)) /
			       audioLogf(2.0f) + 0.5f);
	if (nq <= 0) {
		nq = 1;
		ch->nq = 1;
	} else {
		nq = (uint8_t)nq;
		ch->nq = (uint8_t)nq;
		if ((uint8_t)nq > 5u)
			return -5900;
		if (nq == 0)
			return 0;
	}

	for (int32_t k = 1; k <= (int32_t)ch->nq; k++) {
		int32_t prev = idxTab[k - 1];
		int32_t cur = prev + ((int32_t)elem->nLow - prev) / ((int32_t)ch->nq + 1 - k);
		idxTab[k] = cur;
		if (cur > 63)
			return -5900;
		elem->fTableNoise[k] = elem->fTable[0][cur];
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrCalcLimiterFreqTable(AacCore *core, SbrElement *elem)
{
	if (elem->limiterBands == 0) {
		elem->fTableLim[0] = elem->fTable[0][0];
		elem->fTableLim[1] = elem->fTable[0][elem->nLow];
		elem->nLim = 1;
		return 0;
	}

	int32_t *patchBorders = core->scratch.u.freq.bandDeltaLo;
	int32_t *limTmp = core->scratch.u.freq.bandEdgeLo;
	float limPerOct = kSbrLimBandsPerOctave[elem->limiterBands];

	uint32_t numPatches = elem->numPatches;
	if (numPatches > 63u)
		return -6000;

	patchBorders[0] = elem->kx;
	for (int32_t k = 1; k <= (int32_t)elem->numPatches; k++)
		patchBorders[k] = patchBorders[k - 1] + elem->patchNumSubbands[k - 1];

	int32_t lowEnd = elem->fTable[0][elem->nLow];
	if (patchBorders[(int16_t)elem->numPatches] != lowEnd)
		patchBorders[(int16_t)elem->numPatches] = lowEnd;

	for (int32_t k = 0; k <= (int32_t)elem->nLow; k++)
		limTmp[k] = elem->fTable[0][k];

	int32_t total = (int32_t)elem->nLow + (int32_t)elem->numPatches;
	if (total > 64)
		return -6000;

	for (int32_t k = 1; k < (int32_t)elem->numPatches; k++)
		limTmp[(int32_t)elem->nLow + k] = patchBorders[k];

	audioQsort(limTmp, (size_t)total, 4u, sbrCompareInt);

	int32_t nLim = (int32_t)elem->nLow + (int32_t)elem->numPatches - 1;
	if ((uint32_t)elem->nLow + (uint32_t)elem->numPatches > 64u)
		return -6000;

	int32_t k = 1;
	while (k <= nLim) {
		int32_t prev = limTmp[k - 1];
		if (prev == 0)
			return -6000;
		int32_t cur = limTmp[k];
		float oct = audioLogf((float)cur / (float)prev) / audioLogf(2.0f);
		if (oct * limPerOct < 0.49f) {
			if (prev == cur) {
				for (int32_t j = k; j < nLim; j++)
					limTmp[j] = limTmp[j + 1];
				nLim--;
				continue;
			}
			int32_t curInPatch = 0;
			for (int32_t p = 0; p <= (int32_t)elem->numPatches; p++) {
				if (cur == patchBorders[p]) {
					curInPatch = 1;
					break;
				}
			}
			if (!curInPatch) {
				for (int32_t j = k; j < nLim; j++)
					limTmp[j] = limTmp[j + 1];
				nLim--;
				continue;
			}
			int32_t prevInPatch = 0;
			for (int32_t p = 0; p <= (int32_t)elem->numPatches; p++) {
				if (prev == patchBorders[p]) {
					prevInPatch = 1;
					break;
				}
			}
			if (!prevInPatch) {
				for (int32_t j = k - 1; j < nLim; j++)
					limTmp[j] = limTmp[j + 1];
				nLim--;
				continue;
			}
		}
		k++;
	}

	elem->nLim = (uint8_t)nLim;
	for (int32_t i = 0; i <= nLim; i++)
		elem->fTableLim[i] = (uint8_t)limTmp[i];
	return 0;
}

static AAC_NOINLINE int32_t sbrCalcTimeBorders(AacCore *core, const SbrElement *elem,
					       SbrChannel *ch)
{
	(void)core;
	int32_t le = ch->le;
	for (int32_t i = 0; i <= le; i++)
		ch->tE[i] = 0;

	uint8_t cls = ch->frameClass;
	switch (cls) {
	case 0: {
		ch->tE[0] = 0;
		ch->tE[le] = elem->numTimeSlots2;
		if (le == 0)
			return -6100;
		uint8_t step = (uint8_t)((uint32_t)elem->numTimeSlots / (uint32_t)le);
		uint8_t pos = (uint8_t)(2u * step);
		for (int32_t i = 1; i < le; i++) {
			ch->tE[i] = pos;
			pos = (uint8_t)(pos + 2u * step);
		}
		break;
	}
	case 1: {
		int32_t vb1 = ch->varBord1;
		ch->tE[0] = 0;
		ch->tE[le] = (uint8_t)(elem->numTimeSlots2 + 2 * vb1);
		if (le == 5)
			return -6100;
		int32_t acc = (int32_t)elem->numTimeSlots + vb1;
		for (int32_t i = 0; i < le - 1; i++) {
			acc -= ch->relBord1[i];
			ch->tE[le - 1 - i] = (uint8_t)(2 * acc);
		}
		break;
	}
	case 2: {
		int32_t acc = ch->varBord0;
		int32_t nRel0 = ch->numRel0;
		ch->tE[0] = (uint8_t)(2 * acc);
		ch->tE[le] = elem->numTimeSlots2;
		for (int32_t i = 1; i <= nRel0; i++) {
			acc += ch->relBord0[i - 1];
			ch->tE[i] = (uint8_t)(2 * acc);
		}
		break;
	}
	case 3: {
		int32_t acc0 = ch->varBord0;
		int32_t vb1 = ch->varBord1;
		int32_t nRel0 = ch->numRel0;
		ch->tE[0] = (uint8_t)(2 * acc0);
		ch->tE[le] = (uint8_t)(elem->numTimeSlots2 + 2 * vb1);
		for (int32_t i = 1; i <= nRel0; i++) {
			acc0 += ch->relBord0[i - 1];
			ch->tE[i] = (uint8_t)(2 * acc0);
		}
		if (le - 1 - nRel0 == 4)
			return -6100;
		int32_t count1 = le - 1 - nRel0;
		int32_t acc1 = (int32_t)elem->numTimeSlots + vb1;
		for (int32_t i = 0; i < count1; i++) {
			acc1 -= ch->relBord1[i];
			ch->tE[le - 1 - i] = (uint8_t)(2 * acc1);
		}
		break;
	}
	default:
		return -6100;
	}

	ch->tQ[0] = ch->tE[0];
	if (le == 1) {
		ch->tQ[1] = ch->tE[1];
		ch->tQ[2] = 0;
		return 0;
	}
	if ((uint32_t)le <= 1u)
		return -6100;

	int32_t midIdx;
	uint32_t ptr = ch->pointer;
	switch (cls) {
	case 0:
		midIdx = (int32_t)((uint32_t)le >> 1);
		break;
	case 1:
	case 3:
		midIdx = (ptr <= 1u) ? (le - 1) : (le + 1 - (int32_t)ptr);
		if (midIdx < 0)
			return midIdx;
		break;
	case 2:
		if (ptr > 1u) {
			midIdx = (int32_t)ptr - 1;
		} else if (ptr == 0u) {
			midIdx = 1;
		} else {
			midIdx = le - 1;
			if (midIdx < 0)
				return midIdx;
		}
		break;
	default:
		return -6200;
	}

	ch->tQ[1] = ch->tE[midIdx];
	ch->tQ[2] = ch->tE[le];
	return 0;
}

static AAC_NOINLINE int32_t sbrDeltaDecode(AacCore *core, const SbrElement *elem,
					   SbrChannel *ch, int32_t isRight)
{
	(void)core;
	int32_t shift = (isRight == 1 && elem->coupling == 1) ? 2 : 1;
	int32_t le = ch->le;

	for (int32_t e = 0; e < le; e++) {
		if (ch->dfEnv[e] == 0) {
			ch->envRaw[e][0] *= shift;
			uint32_t nBand = elem->nFreq[ch->freqRes[e]];
			if (nBand > 64u)
				return -5400;
			for (int32_t k = 1; k < (int32_t)nBand; k++)
				ch->envRaw[e][k] = ch->envRaw[e][k - 1] + ch->envRaw[e][k] * shift;
		} else {
			uint8_t curRes = ch->freqRes[e];
			uint8_t prevRes = (e != 0) ? ch->freqRes[e - 1]
						   : ch->prevFreqRes[ch->prevLe - 1];
			if (curRes == prevRes) {
				uint32_t nBand = elem->nFreq[prevRes];
				if (nBand > 64u)
					return -5400;
				for (int32_t k = 0; k < (int32_t)nBand; k++) {
					int32_t prevVal = (e != 0) ? ch->envRaw[e - 1][k]
								   : ch->prevEnvRaw[k];
					ch->envRaw[e][k] = prevVal + shift * ch->envRaw[e][k];
				}
			} else if (curRes == 1 && prevRes == 0) {
				uint32_t nHigh = elem->nFreq[1];
				if (nHigh > 64u)
					return -5400;
				for (int32_t k = 0; k < (int32_t)nHigh; k++) {
					uint32_t nLow = elem->nLow;
					if (nLow > 64u)
						return -5400;
					for (uint32_t j = 0; j < nLow; j = (uint8_t)(j + 1u)) {
						uint8_t fh = elem->fTable[1][k];
						if (elem->fTable[0][j] <= fh &&
						    fh < elem->fTable[0][j + 1]) {
							int32_t prevVal = (e != 0)
									      ? ch->envRaw[e - 1][j]
									      : ch->prevEnvRaw[j];
							ch->envRaw[e][k] = prevVal + ch->envRaw[e][k] * shift;
						}
					}
				}
			} else if (curRes == 0 && prevRes == 1) {
				uint32_t nLow = elem->nFreq[0];
				if (nLow > 64u)
					return -5400;
				for (int32_t k = 0; k < (int32_t)nLow; k++) {
					uint32_t nHigh = elem->nHigh;
					if (nHigh > 64u)
						return -5400;
					for (uint32_t j = 0; j < nHigh; j = (uint8_t)(j + 1u)) {
						if (elem->fTable[1][j] == elem->fTable[0][k]) {
							int32_t prevVal = (e != 0)
									      ? ch->envRaw[e - 1][j]
									      : ch->prevEnvRaw[j];
							ch->envRaw[e][k] = prevVal + ch->envRaw[e][k] * shift;
						}
					}
				}
			}
		}
	}

	uint32_t nq = ch->nq;
	if (nq > 64u)
		return -5400;
	int32_t lq = ch->lq;
	for (int32_t q = 0; q < lq; q++) {
		if (ch->dfNoise[q] != 0) {
			if (q != 0) {
				for (int32_t k = 0; k < (int32_t)nq; k++)
					ch->noiseRaw[q][k] = ch->noiseRaw[q - 1][k] + ch->noiseRaw[q][k] * shift;
			} else {
				for (int32_t k = 0; k < (int32_t)nq; k++)
					ch->noiseRaw[0][k] = ch->prevNoiseRaw[k] + shift * ch->noiseRaw[0][k];
			}
		} else {
			ch->noiseRaw[q][0] *= shift;
			for (int32_t k = 1; k < (int32_t)nq; k++)
				ch->noiseRaw[q][k] = ch->noiseRaw[q][k - 1] + ch->noiseRaw[q][k] * shift;
		}
	}

	for (int32_t k = 0; k < 64; k++) {
		ch->prevEnvRaw[k] = ch->envRaw[le - 1][k];
		ch->prevNoiseRaw[k] = ch->noiseRaw[lq - 1][k];
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrDequantUncoupled(AacCore *core, const SbrElement *elem,
						SbrChannel *ch)
{
	(void)core;
	float ampDiv = (ch->ampRes != 0) ? 1.0f : 2.0f;
	for (int32_t e = 0; e < (int32_t)ch->le; e++) {
		uint32_t nBand = elem->nFreq[ch->freqRes[e]];
		if (nBand > 64u)
			return -5500;
		float *dst = &ch->envOrig[64 * e];
		for (int32_t k = 0; k < (int32_t)elem->nFreq[ch->freqRes[e]]; k++)
			dst[k] = audioPowf(2.0f, (float)ch->envRaw[e][k] / ampDiv) * 64.0f;
	}

	if (ch->nq > 64u)
		return -5500;
	for (int32_t q = 0; q < (int32_t)ch->lq; q++) {
		float *dst = &ch->noiseOrig[64 * q];
		for (int32_t k = 0; k < (int32_t)ch->nq; k++)
			dst[k] = audioPowf(2.0f, (float)(6 - ch->noiseRaw[q][k]));
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrDequantCoupled(AacCore *core, const SbrElement *elem,
					      SbrChannel *chL, SbrChannel *chR)
{
	(void)core;
	float ampDiv = (chL->ampRes != 0) ? 1.0f : 2.0f;
	float panOff = (chL->ampRes != 0) ? 12.0f : 24.0f;

	if (chL->le > 5u)
		return -5600;
	for (int32_t e = 0; e < (int32_t)chL->le; e++) {
		uint32_t nBand = elem->nFreq[chL->freqRes[e]];
		if (nBand > 64u)
			return -5600;
		float *dstL = &chL->envOrig[64 * e];
		float *dstR = &chR->envOrig[64 * e];
		for (int32_t k = 0; k < (int32_t)elem->nFreq[chL->freqRes[e]]; k++) {
			float e0 = audioPowf(2.0f, (float)chL->envRaw[e][k] / ampDiv + 1.0f) * 64.0f;
			float pan = (panOff - (float)chR->envRaw[e][k]) / ampDiv;
			dstL[k] = e0 / (1.0f + audioPowf(2.0f, pan));
			dstR[k] = e0 / (1.0f + audioPowf(2.0f, -pan));
		}
	}

	if (chL->nq > 64u)
		return -5600;
	for (int32_t q = 0; q < (int32_t)chL->lq; q++) {
		float *dstL = &chL->noiseOrig[64 * q];
		float *dstR = &chR->noiseOrig[64 * q];
		for (int32_t k = 0; k < (int32_t)chL->nq; k++) {
			float q0 = audioPowf(2.0f, (float)(6 - chL->noiseRaw[q][k]) + 1.0f);
			float qPan = 12.0f - (float)chR->noiseRaw[q][k];
			dstL[k] = q0 / (1.0f + audioPowf(2.0f, qPan));
			dstR[k] = q0 / (1.0f + audioPowf(2.0f, -qPan));
		}
	}
	return 0;
}

static AAC_NOINLINE int32_t sbrHfGenerate(AacCore *core, SbrElement *elem, SbrChannel *ch)
{
	int32_t tStart = ch->tE[0];
	int32_t tEnd = ch->tE[ch->le];

	if (ch->nq > 5u)
		return -7000;
	for (int32_t n = 0; n < (int32_t)ch->nq; n++) {
		uint32_t prevMode = ch->prevInvfMode[n];
		uint32_t curMode = ch->invfMode[n];
		if (prevMode > 3u || curMode > 3u)
			return -7000;
		float newBw = kSbrBwTab[4u * prevMode + curMode];
		float prevBw = ch->prevBwArray[n];
		float bw = (newBw >= prevBw)
			       ? (0.09375f * prevBw + 0.90625f * newBw)
			       : (0.25f * prevBw + 0.75f * newBw);
		if (bw < 0.015625f)
			bw = 0.0f;
		else if (bw >= 0.99609375f)
			bw = 0.99609375f;
		core->sbrBwArray[n] = bw;
	}

	if ((int32_t)elem->tHfAdj + tEnd > 40)
		return -6900;
	for (int32_t t = tStart; t < tEnd; t++)
		sceClibMemset(ch->qmfWork->xHigh[(int32_t)elem->tHfAdj + t], 0, 0x200u);

	if (ch->isSecondCh == 0 && core->sbrResetFlag == 1) {
		int32_t k0 = elem->k0;
		int32_t sb = elem->kx;
		int32_t srIdx = sbrGetSrIndex(core->sbrSampleRate);
		if (srIdx < 0)
			return srIdx;

		int32_t goalSb = kSbrGoalSb[srIdx];
		int32_t kx = elem->kx;
		int32_t topSb = kx + elem->mBand;
		int32_t msb = k0;
		int32_t usb = kx;
		int32_t kIdx;
		if (goalSb >= topSb) {
			kIdx = elem->nMaster;
		} else {
			kIdx = 0;
			while (goalSb > (int32_t)elem->fMaster[kIdx]) {
				kIdx++;
				if (kIdx == 64)
					return -7100;
			}
		}

		if (elem->nMaster == 0) {
			elem->numPatches = 0;
			elem->patchNumSubbands[0] = 0;
			elem->patchStartSubband[0] = 0;
		} else {
			if (kIdx > 63)
				return -7100;
			int32_t numPatches = 0;
			int32_t stallCnt = 0;
			int32_t prevSb = 0;
			for (;;) {
				int32_t j = kIdx;
				int32_t odd;
				int32_t cand;
				for (;;) {
					cand = elem->fMaster[j];
					odd = (cand - 2 + k0) % 2;
					if (cand <= msb + k0 - 1 - odd)
						break;
					j--;
					if (j < 0)
						return -7100;
				}
				if (numPatches > 63)
					return -7100;
				int32_t width = cand - usb;
				if (width <= 0) {
					elem->patchNumSubbands[numPatches] = 0;
					elem->patchStartSubband[numPatches] = (uint8_t)(k0 - odd);
					msb = kx;
				} else {
					elem->patchNumSubbands[numPatches] = (uint8_t)width;
					elem->patchStartSubband[numPatches] = (uint8_t)(k0 - odd - width);
					if (width != 0) {
						usb = cand;
						numPatches++;
						msb = cand;
					} else {
						msb = kx;
					}
				}
				if ((int32_t)elem->fMaster[kIdx] - cand <= 2)
					kIdx = elem->nMaster;

				if (cand == prevSb) {
					stallCnt++;
					if (topSb == cand || stallCnt > 15)
						break;
				} else {
					if (topSb == cand)
						break;
					stallCnt = 0;
				}
				prevSb = cand;
				if (kIdx > 63)
					return -7100;
			}
			if (elem->patchNumSubbands[numPatches - 1] <= 2u && numPatches > 1)
				numPatches--;
			if (numPatches > 4)
				numPatches = 5;
			elem->numPatches = (uint8_t)numPatches;
		}
		(void)sb;
	}

	SbrQmfWork *qw = ch->qmfWork;
	int32_t tAdj = elem->tHfAdj;
	if (elem->k0 > 32u || (uint32_t)elem->numTimeSlots2 + 5u + (uint32_t)tAdj > 39u)
		return -7200;

	/* 2nd-order LPC (chirp) coefficients for subbands 1..k0-1 */
	for (int32_t k = 1; k < (int32_t)elem->k0; k++) {
		float x0r = qw->xLow[0][k][0], x0i = qw->xLow[0][k][1];
		float x1r = qw->xLow[1][k][0], x1i = qw->xLow[1][k][1];
		float r22Init = x0r * x0r + x0i * x0i;
		float r12iInit = x0r * x1i - x1r * x0i;
		float r12rInit = x0i * x1i + x0r * x1r;

		float r11 = 0.0f;
		float r01r = 0.0f, r01i = 0.0f;
		float r02r = 0.0f, r02i = 0.0f;
		float lastR22 = r22Init;
		float lastR12r = r12rInit;
		float lastR12i = r12iInit;

		float pr = x0r, pi = x0i;
		float cr = x1r, ci = x1i;
		for (int32_t t = tAdj; t < 40; t += 2) {
			float n0r = qw->xLow[t][k][0], n0i = qw->xLow[t][k][1];
			float n1r = qw->xLow[t + 1][k][0], n1i = qw->xLow[t + 1][k][1];

			lastR12r = n0i * n1i + n0r * n1r;
			lastR22 = n0i * n0i + n0r * n0r;
			r01i = ((n0i * cr - n0r * ci) + r01i) + (n0r * n1i - n1r * n0i);
			r01r = ((n0i * ci + n0r * cr) + r01r) + lastR12r;
			r02r = ((n0i * pi + n0r * pr) + r02r) + (n1i * ci + n1r * cr);
			r02i = ((n0i * pr - n0r * pi) + r02i) + (n1i * cr - n1r * ci);
			r11 = ((ci * ci + cr * cr) + r11) + lastR22;
			lastR12i = n0r * n1i - n1r * n0i;

			pr = n0r; pi = n0i;
			cr = n1r; ci = n1i;
		}

		float r12i = (r12iInit + r01i) - lastR12i;
		float r12r = (r12rInit + r01r) - lastR12r;
		float r22 = (r22Init + r11) - lastR22;
		float det = r11 * r22 - (r12i * r12i + r12r * r12r) * 0.99999905f;

		float a1r = 0.0f, a1i = 0.0f;
		if (det >= 1e-38f) {
			float invDet = 1.0f / det;
			a1r = invDet * ((r01r * r12r - r01i * r12i) - r02r * r11);
			a1i = invDet * ((r01i * r12r + r01r * r12i) - r02i * r11);
		}

		float a0r = 0.0f, a0i = 0.0f;
		if (r11 != 0.0f) {
			float negInvR11 = -(1.0f / r11);
			a0i = negInvR11 * ((r01i + r12r * a1i) - r12i * a1r);
			a0r = ((r01r + r12r * a1r) + r12i * a1i) * negInvR11;
		}

		if (a0i * a0i + a0r * a0r >= 16.0f || a1i * a1i + a1r * a1r >= 16.0f) {
			a0r = 0.0f; a0i = 0.0f;
			a1r = 0.0f; a1i = 0.0f;
		}

		qw->alpha0[k][0] = a0r;
		qw->alpha0[k][1] = a0i;
		qw->alpha1[k][0] = a1r;
		qw->alpha1[k][1] = a1i;
	}

	if (elem->numPatches > 64u || ch->nq > 63u)
		return -7200;

	int32_t curCh = core->sbrCurChanIdx;
	for (int32_t p = 0; p < (int32_t)elem->numPatches; p++) {
		int32_t nSub = elem->patchNumSubbands[p];
		for (int32_t x = 0; x < nSub; x++) {
			int32_t dstBand = elem->kx + x;
			for (int32_t q = 0; q < p; q++)
				dstBand += elem->patchNumSubbands[q];
			int32_t srcBand = elem->patchStartSubband[p] + x;
			if (srcBand > 31)
				return -7200;

			int32_t g = 0;
			for (int32_t n = 0; n < (int32_t)ch->nq; n++) {
				if ((int32_t)elem->fTableNoise[n] <= dstBand &&
				    dstBand < (int32_t)elem->fTableNoise[n + 1]) {
					g = n;
					break;
				}
				g = n + 1;
			}
			float bw = core->sbrBwArray[g];
			float bw2 = bw * bw;
			if (dstBand > 63)
				return -7200;

			int32_t tHi = ch->tE[ch->le] + tAdj;
			if (tHi > 40)
				return -7200;
			int32_t tLo = ch->tE[0] + tAdj;

			if (bw2 <= 0.0f) {
				for (int32_t t = tLo; t < tHi; t++) {
					float xr = qw->xLow[t][srcBand][0];
					float xi = qw->xLow[t][srcBand][1];
					float gain = core->drcQmfGain[curCh][t + 6][srcBand] + 1.0e-12f;
					qw->xHigh[t][dstBand][0] = xr / gain;
					qw->xHigh[t][dstBand][1] = xi / gain;
				}
			} else {
				float c0r = bw * qw->alpha0[srcBand][0];
				float c0i = bw * qw->alpha0[srcBand][1];
				float c1r = bw2 * qw->alpha1[srcBand][0];
				float c1i = bw2 * qw->alpha1[srcBand][1];
				for (int32_t t = tLo; t < tHi; t++) {
					float yr = qw->xLow[t][srcBand][0];
					float yi = qw->xLow[t][srcBand][1];
					float p1r = qw->xLow[t - 1][srcBand][0];
					float p1i = qw->xLow[t - 1][srcBand][1];
					float p2r = qw->xLow[t - 2][srcBand][0];
					float p2i = qw->xLow[t - 2][srcBand][1];
					yr += c0r * p1r - c0i * p1i;
					yi += c0i * p1r + c0r * p1i;
					yr += c1r * p2r - c1i * p2i;
					yi += c1i * p2r + c1r * p2i;
					float gain = core->drcQmfGain[curCh][t + 6][srcBand] + 1.0e-12f;
					qw->xHigh[t][dstBand][0] = yr / gain;
					qw->xHigh[t][dstBand][1] = yi / gain;
				}
			}
		}
	}

	if (core->sbrResetFlag == 1)
		return sbrCalcLimiterFreqTable(core, elem);
	return 0;
}

static AAC_NOINLINE int32_t sbrCalculateGainAndAdjust(AacCore *core, const SbrElement *elem,
						      SbrChannel *ch)
{
	/* Step 1: Map reference envelope E_orig into per-subband buffer bufPtrs[0] */
	for (int32_t l = 0; l < (int32_t)ch->le; l++) {
		uint8_t res = ch->freqRes[l];
		uint32_t nBand = elem->nFreq[res];
		if (nBand > 63u)
			return -6300;
		for (int32_t k = 0; k < (int32_t)nBand; k++) {
			int32_t hi = elem->fTable[res][k + 1];
			if (hi - (int32_t)elem->kx > 64)
				return -6300;
			int32_t lo = elem->fTable[res][k];
			float val = ch->envOrig[64 * l + k];
			for (int32_t m = lo; m < hi; m++)
				core->scratch.u.sbr.bufPtrs[0][l][m - (int32_t)elem->kx] = val;
		}
	}

	/* Step 2: Map noise floor Q_orig into per-subband buffer bufPtrs[1] */
	if (ch->nq > 63u)
		return -6300;
	for (int32_t l = 0; l < (int32_t)ch->le; l++) {
		for (int32_t k = 0; k < (int32_t)ch->nq; k++) {
			int32_t hi = elem->fTableNoise[k + 1];
			if (hi - (int32_t)elem->kx > 64)
				return -6300;
			int32_t lo = elem->fTableNoise[k];
			for (int32_t m = lo; m < hi; m++) {
				core->scratch.u.sbr.bufPtrs[1][l][m - (int32_t)elem->kx] = 0.0f;
				for (int32_t q = 0; q < (int32_t)ch->lq; q++) {
					if (ch->tE[l] >= ch->tQ[q] && ch->tE[l + 1] <= ch->tQ[q + 1]) {
						core->scratch.u.sbr.bufPtrs[1][l][m - (int32_t)elem->kx] =
							ch->noiseOrig[64 * q + k];
					}
				}
			}
		}
	}

	/* Step 3: Additional harmonics map (S_index_mapped / S_mapped) */
	sceClibMemset(core->sbrSines, 0, sizeof(core->sbrSines));
	sceClibMemset(core->sbrSinesBand, 0, sizeof(core->sbrSinesBand));

	switch (ch->frameClass) {
	case 0:
		ch->lA = -1;
		break;
	case 1:
	case 3:
		ch->lA = (ch->pointer == 0) ? -1 : (int8_t)(ch->le + 1 - ch->pointer);
		break;
	case 2:
		ch->lA = (ch->pointer <= 1u) ? -1 : (int8_t)(ch->pointer - 1);
		break;
	default:
		break;
	}

	if (elem->nHigh > 63u)
		return -6400;
	for (int32_t l = 0; l < (int32_t)ch->le; l++) {
		for (int32_t k = 0; k < (int32_t)elem->nHigh; k++) {
			int32_t hi = elem->fTable[1][k + 1];
			int32_t lo = elem->fTable[1][k];
			uint8_t addH = (ch->addHarmonicFlag == 1) ? ch->addHarmonic[k] : 0;
			if (hi - (int32_t)elem->kx > 64)
				return -6400;
			int32_t mid = (int32_t)((float)(lo + hi) * 0.5f);
			for (int32_t m = lo; m < hi; m++) {
				int32_t delta = 1;
				if ((int32_t)ch->lA > l)
					delta = (ch->prevAddHarmonic[k] == 1) ? 1 : 0;
				if (m == mid)
					core->sbrSines[l][m - (int32_t)elem->kx] = (uint8_t)(delta * addH);
				else
					core->sbrSines[l][m - (int32_t)elem->kx] = 0;
			}
		}
	}

	for (int32_t l = 0; l < (int32_t)ch->le; l++) {
		uint8_t res = ch->freqRes[l];
		if (res > 1u)
			return -6400;
		uint32_t nBand = elem->nFreq[res];
		if (nBand > 63u)
			return -6400;
		for (int32_t k = 0; k < (int32_t)nBand; k++) {
			int32_t hi = elem->fTable[res][k + 1];
			int32_t lo = elem->fTable[res][k];
			if (hi - (int32_t)elem->kx > 64)
				return -6400;
			uint8_t hasSine = 0;
			for (int32_t m = lo; m < hi; m++) {
				if (core->sbrSines[l][m - (int32_t)elem->kx] == 1) {
					hasSine = 1;
					break;
				}
			}
			for (int32_t m = lo; m < hi; m++)
				core->sbrSinesBand[l][m - (int32_t)elem->kx] = hasSine;
		}
	}

	/* Step 4: Estimate current envelope E_curr into bufPtrs[2] */
	SbrQmfWork *qw = ch->qmfWork;
	if (elem->interpFreq == 1) {
		for (int32_t l = 0; l < (int32_t)ch->le; l++) {
			float dt = (float)((int32_t)ch->tE[l + 1] - (int32_t)ch->tE[l]);
			if (dt == 0.0f || (uint32_t)elem->kx + (uint32_t)elem->mBand > 64u)
				return -6500;
			int32_t tLo = (int32_t)ch->tE[l] + (int32_t)elem->tHfAdj;
			int32_t tHi = (int32_t)ch->tE[l + 1] - 1 + (int32_t)elem->tHfAdj;
			if (elem->mBand != 0 && tHi > 39)
				return -6500;
			for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
				float sum = 0.0f;
				int32_t sb = (int32_t)elem->kx + m;
				for (int32_t t = tLo; t <= tHi; t++) {
					float re = qw->xHigh[t][sb][0];
					float im = qw->xHigh[t][sb][1];
					sum += im * im + re * re;
				}
				core->scratch.u.sbr.bufPtrs[2][l][m] = sum / dt;
			}
		}
	} else {
		for (int32_t l = 0; l < (int32_t)ch->le; l++) {
			uint8_t res = ch->freqRes[l];
			uint32_t nBand = elem->nFreq[res];
			if (nBand > 63u)
				return -6500;
			for (int32_t k = 0; k < (int32_t)nBand; k++) {
				int32_t lo = elem->fTable[res][k];
				int32_t hi = (uint8_t)(elem->fTable[res][k + 1] - 1u);
				int32_t dt = (int32_t)ch->tE[l + 1] - (int32_t)ch->tE[l];
				float denom = (float)(dt + (hi - lo) * dt);
				if (denom == 0.0f)
					return -6500;
				int32_t tLo = (int32_t)ch->tE[l] + (int32_t)elem->tHfAdj;
				int32_t tHi = (int32_t)ch->tE[l + 1] - 1 + (int32_t)elem->tHfAdj;
				if (tHi > 39)
					return -6500;
				float sum = 0.0f;
				if (tLo <= tHi) {
					if ((uint32_t)hi > 63u)
						return -6500;
					for (int32_t t = tLo; t <= tHi; t++) {
						for (int32_t sb = lo; sb <= hi; sb++) {
							float re = qw->xHigh[t][sb][0];
							float im = qw->xHigh[t][sb][1];
							sum += im * im + re * re;
						}
					}
				}
				if (hi - (int32_t)elem->kx > 63)
					return -6500;
				for (int32_t sb = lo; sb <= hi; sb++)
					core->scratch.u.sbr.bufPtrs[2][l][sb - (int32_t)elem->kx] = sum / denom;
			}
		}
	}

	/* Step 5: Compute Q_M (bufPtrs[6]) and S_M (bufPtrs[5]) */
	int32_t le = ch->le;
	if (le != 0) {
		if (elem->mBand > 64u)
			return -6600;
		for (int32_t l = 0; l < le; l++) {
			for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
				float qVal = core->scratch.u.sbr.bufPtrs[1][l][m];
				float denom = qVal + 1.0f;
				if (denom == 0.0f)
					return -6600;
				float eOrig = core->scratch.u.sbr.bufPtrs[0][l][m];
				core->scratch.u.sbr.bufPtrs[6][l][m] = eOrig * (qVal / denom);
				if (core->sbrSines[l][m] != 0) {
					core->scratch.u.sbr.bufPtrs[5][l][m] =
						eOrig * ((float)core->sbrSines[l][m] / denom);
				} else {
					core->scratch.u.sbr.bufPtrs[5][l][m] = 0.0f;
				}
			}
		}

		/* Step 6: Compute raw gains G (bufPtrs[7]) */
		for (int32_t l = 0; l < le; l++) {
			float delta = (l == (int32_t)ch->lA || l == (int32_t)ch->prevLa) ? 0.0f : 1.0f;
			if (elem->mBand > 64u)
				return -6700;
			for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
				float eOrig = core->scratch.u.sbr.bufPtrs[0][l][m];
				float eCurr = core->scratch.u.sbr.bufPtrs[2][l][m];
				float qVal = core->scratch.u.sbr.bufPtrs[1][l][m];
				if (core->sbrSinesBand[l][m] != 0) {
					float d1 = eCurr + 1.0f;
					float d2 = qVal + 1.0f;
					if (d1 == 0.0f || d2 == 0.0f)
						return -6700;
					core->scratch.u.sbr.bufPtrs[7][l][m] = (eOrig / d1) * (qVal / d2);
				} else {
					float denom = (eCurr + 1.0f) * (delta * qVal + 1.0f);
					if (denom == 0.0f)
						return -6700;
					core->scratch.u.sbr.bufPtrs[7][l][m] = eOrig / denom;
				}
			}
		}

		/* Step 7: Limiter bands (G_max, Q_M_lim, G_lim, G_boost, sqrt) */
		if (elem->nLim > 63u)
			return -6700;
		for (int32_t l = 0; l < le; l++) {
			for (int32_t k = 0; k < (int32_t)elem->nLim; k++) {
				core->scratch.u.lim.eLimSum[l][k] = 1.0e-12f;
				int32_t mLo = (int32_t)elem->fTableLim[k] - (int32_t)elem->kx;
				int32_t mHi = (int32_t)elem->fTableLim[k + 1] - 1 - (int32_t)elem->kx;
				if (mHi > 63)
					return -6700;
				if (mLo > mHi) {
					core->scratch.u.lim.gMaxBuf[l][k] =
						kSbrLimiterGains[elem->limiterGains];
				} else {
					float sumOrig = 1.0e-12f;
					float sumCurr = 1.0e-12f;
					for (int32_t m = mLo; m <= mHi; m++) {
						sumOrig += core->scratch.u.sbr.bufPtrs[0][l][m];
						core->scratch.u.lim.eLimSum[l][k] = sumOrig;
						sumCurr += core->scratch.u.sbr.bufPtrs[2][l][m];
					}
					if (sumCurr != 0.0f) {
						core->scratch.u.lim.gMaxBuf[l][k] =
							kSbrLimiterGains[elem->limiterGains] *
							(sumOrig / sumCurr);
					} else {
						core->scratch.u.lim.gMaxBuf[l][k] = 1.0e10f;
					}
				}
			}
		}

		/* Expand G_max per subband into ch->envOrig scratch */
		for (int32_t l = 0; l < le; l++) {
			for (int32_t k = 0; k < (int32_t)elem->nLim; k++) {
				for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
					int32_t sb = (int32_t)elem->kx + m;
					if ((int32_t)elem->fTableLim[k] <= sb &&
					    sb < (int32_t)elem->fTableLim[k + 1]) {
						float gm = core->scratch.u.lim.gMaxBuf[l][k];
						if (gm >= 1.0e10f)
							gm = 1.0e10f;
						ch->envOrig[64 * l + m] = gm;
					}
				}
			}
		}

		/* Clamp G -> bufPtrs[3], Q_M -> bufPtrs[4] */
		for (int32_t l = 0; l < le; l++) {
			for (int32_t k = 0; k < (int32_t)elem->nLim; k++) {
				for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
					int32_t sb = (int32_t)elem->kx + m;
					if ((int32_t)elem->fTableLim[k] <= sb &&
					    sb < (int32_t)elem->fTableLim[k + 1]) {
						float gMax = ch->envOrig[64 * l + m];
						float gVal = core->scratch.u.sbr.bufPtrs[7][l][m];
						if (gMax <= gVal) {
							if (gVal == 0.0f)
								return -6700;
							core->scratch.u.sbr.bufPtrs[4][l][m] =
								core->scratch.u.sbr.bufPtrs[6][l][m] *
								(gMax / gVal);
						} else {
							core->scratch.u.sbr.bufPtrs[4][l][m] =
								core->scratch.u.sbr.bufPtrs[6][l][m];
						}
						core->scratch.u.sbr.bufPtrs[3][l][m] =
							(gVal < gMax) ? gVal : gMax;
					}
				}
			}
		}

		/* Compute gain boost factor G_boost in ch->noiseOrig scratch */
		for (int32_t l = 0; l < le; l++) {
			for (int32_t k = 0; k < (int32_t)elem->nLim; k++) {
				int32_t mLo = (int32_t)elem->fTableLim[k] - (int32_t)elem->kx;
				int32_t mHi = (int32_t)elem->fTableLim[k + 1] - 1 - (int32_t)elem->kx;
				if (mLo < 0 || mHi > 63)
					return -6700;
				float sumEst = 1.0e-12f;
				if (mLo <= mHi) {
					for (int32_t m = mLo; m <= mHi; m++) {
						float sVal = core->scratch.u.sbr.bufPtrs[5][l][m];
						float gCurr = core->scratch.u.sbr.bufPtrs[2][l][m] *
							      core->scratch.u.sbr.bufPtrs[3][l][m];
						if (sVal != 0.0f || l == (int32_t)ch->lA ||
						    l == (int32_t)ch->prevLa) {
							sumEst += sVal + gCurr;
						} else {
							sumEst += sVal + gCurr +
								  core->scratch.u.sbr.bufPtrs[4][l][m];
						}
					}
					if (sumEst == 0.0f)
						return -6700;
				}
				float boost = core->scratch.u.lim.eLimSum[l][k] / sumEst;
				if (boost >= 2.5118866f)
					boost = 2.5118866f;
				for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
					if (m >= mLo && m <= mHi)
						ch->noiseOrig[64 * l + m] = boost;
				}
			}
		}

		/* Apply boost and take sqrt of G_lim, Q_lim, S_M */
		for (int32_t l = 0; l < le; l++) {
			for (int32_t k = 0; k < (int32_t)elem->nLim; k++) {
				for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
					int32_t sb = (int32_t)elem->kx + m;
					if ((int32_t)elem->fTableLim[k] <= sb &&
					    sb < (int32_t)elem->fTableLim[k + 1]) {
						float boost = ch->noiseOrig[64 * l + m];
						core->scratch.u.sbr.bufPtrs[3][l][m] *= boost;
						core->scratch.u.sbr.bufPtrs[4][l][m] *= boost;
						core->scratch.u.sbr.bufPtrs[5][l][m] *= boost;
					}
				}
			}
		}
		for (int32_t l = 0; l < le; l++) {
			for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
				core->scratch.u.sbr.bufPtrs[3][l][m] =
					audioSqrtf(core->scratch.u.sbr.bufPtrs[3][l][m]);
				core->scratch.u.sbr.bufPtrs[4][l][m] =
					audioSqrtf(core->scratch.u.sbr.bufPtrs[4][l][m]);
				core->scratch.u.sbr.bufPtrs[5][l][m] =
					audioSqrtf(core->scratch.u.sbr.bufPtrs[5][l][m]);
			}
		}
	}

	/* Step 8: Assemble HF signal with smoothing, noise addition, and sinusoids */
	int32_t smoothLen = (elem->smoothingMode != 0) ? 0 : 4;
	int32_t noiseIdx;
	if (core->sbrResetFlag == 1) {
		noiseIdx = 0;
		for (int32_t i = 0; i < 4; i++) {
			ch->gTempPtr[i] = core->scratch.u.sbr.bufPtrs[3][0];
			ch->qTempPtr[i] = core->scratch.u.sbr.bufPtrs[4][0];
		}
	} else {
		noiseIdx = ch->noiseIndex;
		for (int32_t i = 0; i < 4; i++) {
			ch->gTempPtr[i] = ch->gTempHist[i];
			ch->qTempPtr[i] = ch->qTempHist[i];
		}
	}

	uint8_t sinIdx = ch->sinIndex;
	uint8_t curSinIdx = sinIdx;
	int32_t lastNoiseIdx = noiseIdx;
	int32_t prevLa = ch->prevLa;

	for (int32_t l = 0; l < (int32_t)ch->le; l++) {
		int32_t isTrans = (l == (int32_t)ch->lA || l == prevLa) ? 1 : 0;
		int32_t hLen = isTrans ? 0 : smoothLen;
		const float *hFilt = (hLen == 4) ? kSbrSmoothFilter : kSbrSmoothFilterOne;
		float *sM = core->scratch.u.sbr.bufPtrs[5][l];

		for (int32_t t = ch->tE[l]; t < (int32_t)ch->tE[l + 1]; t++) {
			ch->gTempPtr[4] = core->scratch.u.sbr.bufPtrs[3][l];
			ch->qTempPtr[4] = core->scratch.u.sbr.bufPtrs[4][l];

			curSinIdx = (uint8_t)((sinIdx - ch->tE[0] + t) & 3);
			float phiRe = (float)kSbrSinPhaseRe[curSinIdx];
			float phiIm = (float)kSbrSinPhaseIm[curSinIdx];

			for (int32_t m = 0; m < (int32_t)elem->mBand; m++) {
				int32_t kIdx = (noiseIdx + 1 + m +
						(int32_t)elem->mBand * (t - (int32_t)ch->tE[0])) & 511;
				lastNoiseIdx = kIdx;

				float gFilt = 0.0f;
				float qFilt = 0.0f;
				for (int32_t j = 0; j <= hLen; j++) {
					float w = hFilt[j];
					gFilt += w * ch->gTempPtr[4 - j][m];
					qFilt += w * ch->qTempPtr[4 - j][m];
				}

				int32_t sb = (int32_t)elem->kx + m;
				int32_t tSlot = (int32_t)elem->tHfAdj + t;
				float re = gFilt * qw->xHigh[tSlot][sb][0];
				float im = gFilt * qw->xHigh[tSlot][sb][1];
				float smVal = sM[m];
				if (isTrans == 0 && smVal == 0.0f) {
					re += qFilt * kSbrNoiseRe[kIdx];
					im += qFilt * kSbrNoiseIm[kIdx];
				}
				qw->xHigh[tSlot][sb][0] = re + phiRe * smVal;
				qw->xHigh[tSlot][sb][1] = im + (phiIm * smVal) * (float)(int32_t)kSbrHarmonicSign[sb];
			}

			for (int32_t i = 0; i < 4; i++) {
				ch->gTempPtr[i] = ch->gTempPtr[i + 1];
				ch->qTempPtr[i] = ch->qTempPtr[i + 1];
			}
		}
	}

	for (int32_t i = 0; i < 4; i++) {
		sceClibMemcpy_safe(ch->gTempHist[i], ch->gTempPtr[i], 0x100u);
		sceClibMemcpy_safe(ch->qTempHist[i], ch->qTempPtr[i], 0x100u);
	}
	ch->noiseIndex = lastNoiseIdx;
	ch->sinIndex = (uint8_t)((curSinIdx + 1u) & 3u);
	return 0;
}

static AAC_NOINLINE int32_t sbrQmfAnalysis(AacCore *core, const SbrElement *elem,
					   SbrChannel *ch, const float *inPcm)
{
	uint32_t kxLim = (ch->headerReady == 1) ? elem->kx : 32u;
	float *work = core->scratch.u.qmfAnal.analWork;
	float *re = core->scratch.u.qmfAnal.analRe;
	float *im = core->scratch.u.qmfAnal.analIm;

	/* Copy 288-sample delay line to work[1024..1311] */
	float *bufPtr = &work[1024];
	sceClibMemcpy_safe(bufPtr, ch->qmfAnalDelay, 0x480u);

	for (int32_t slot = 0; slot < 32; slot++) {
		bufPtr -= 32;
		const float *src = &inPcm[32 * slot];
		for (int32_t i = 0; i < 32; i++)
			bufPtr[31 - i] = src[i];

		/* Window & fold 320 samples -> 64 complex pre-twiddle inputs */
		for (int32_t k = 0; k < 64; k += 2) {
			float u0 = bufPtr[k]       * kSbrQmfProto[2 * k]       +
				   bufPtr[k + 64]  * kSbrQmfProto[2 * k + 128] +
				   bufPtr[k + 128] * kSbrQmfProto[2 * k + 256] +
				   bufPtr[k + 192] * kSbrQmfProto[2 * k + 384] +
				   bufPtr[k + 256] * kSbrQmfProto[2 * k + 512];
			float u1 = bufPtr[k + 1]   * kSbrQmfProto[2 * k + 2]   +
				   bufPtr[k + 65]  * kSbrQmfProto[2 * k + 130] +
				   bufPtr[k + 129] * kSbrQmfProto[2 * k + 258] +
				   bufPtr[k + 193] * kSbrQmfProto[2 * k + 386] +
				   bufPtr[k + 257] * kSbrQmfProto[2 * k + 514];
			re[k]     = u0 * kSbrQmfAnal32PreCos[k];
			im[k]     = u0 * kSbrQmfAnal32PreSin[k];
			re[k + 1] = u1 * kSbrQmfAnal32PreCos[k + 1];
			im[k + 1] = u1 * kSbrQmfAnal32PreSin[k + 1];
		}

		/* Bit-reversal permutation (28 swaps) */
		for (int32_t i = 0; i < 56; i += 2) {
			uint32_t a = kSbrQmfAnal32Bitrev[i];
			uint32_t b = kSbrQmfAnal32Bitrev[i + 1];
			float tr = re[a], ti = im[a];
			re[a] = re[b]; im[a] = im[b];
			re[b] = tr;    im[b] = ti;
		}

		/* 64-point Radix-4 FFT (3 stages) */
		int32_t twStep = 32;
		int32_t halfStep = 1;
		int32_t span = 2;
		for (int32_t stage = 0; stage < 3; stage++) {
			int32_t nGroups = twStep / 2;
			int32_t twOff = halfStep * nGroups;
			for (int32_t g = 0; g < nGroups; g++) {
				int32_t base = g * 2 * span;
				int32_t t1 = 0;
				int32_t t2 = 0;
				for (int32_t k = 0; k < halfStep; k++) {
					int32_t i0 = base + k;
					int32_t i1 = base + halfStep + k;
					int32_t i2 = base + span + k;
					int32_t i3 = base + span + halfStep + k;

					float c2 = kSbrQmfAnal32FftCos[t2];
					float s2 = kSbrQmfAnal32FftSin[t2];
					float c1 = kSbrQmfAnal32FftCos[t1];
					float s1 = kSbrQmfAnal32FftSin[t1];
					float c3 = kSbrQmfAnal32FftCos[twOff + t1];
					float s3 = kSbrQmfAnal32FftSin[twOff + t1];

					float x1r = re[i1] * c2 - im[i1] * s2;
					float x1i = re[i1] * s2 + im[i1] * c2;
					float x3r = re[i3] * c2 - im[i3] * s2;
					float x3i = re[i3] * s2 + im[i3] * c2;

					float a0r = re[i0] + x1r, a0i = im[i0] + x1i;
					float a1r = re[i0] - x1r, a1i = im[i0] - x1i;
					float a2r = re[i2] + x3r, a2i = im[i2] + x3i;
					float a3r = re[i2] - x3r, a3i = im[i2] - x3i;

					float b2r = a2r * c1 - a2i * s1;
					float b2i = a2r * s1 + a2i * c1;
					float b3r = a3r * c3 - a3i * s3;
					float b3i = a3r * s3 + a3i * c3;

					re[i0] = a0r + b2r; im[i0] = a0i + b2i;
					re[i2] = a0r - b2r; im[i2] = a0i - b2i;
					re[i1] = a1r + b3r; im[i1] = a1i + b3i;
					re[i3] = a1r - b3r; im[i3] = a1i - b3i;

					t1 += nGroups;
					t2 += twStep;
				}
			}
			twStep >>= 2;
			halfStep = 2 * span;
			span *= 4;
		}

		if (elem->kx > 32u)
			return -5100;

		float (*dst)[2] = ch->qmfWork->xLow[(int32_t)elem->tHfGen + slot];
		for (uint32_t k = 0; k < kxLim; k++) {
			float xr = re[k], xi = im[k];
			float c = kSbrQmfAnal32PostCos[k];
			float s = kSbrQmfAnal32PostSin[k];
			dst[k][0] = xr * c - xi * s;
			dst[k][1] = xi * c + xr * s;
		}
		if (ch->headerReady == 1) {
			for (uint32_t k = kxLim; k < 32u; k++) {
				dst[k][0] = 0.0f;
				dst[k][1] = 0.0f;
			}
		}
	}

	sceClibMemcpy_safe(ch->qmfAnalDelay, bufPtr, 0x480u);
	if (elem->kxPrev > 32u)
		return -5100;

	for (int32_t t = 0; t < (int32_t)elem->tHfGen; t++) {
		for (int32_t k = 0; k < 32; k++) {
			ch->qmfWork->xLow[t][k][0] = ch->xLowSave[t][k][0];
			ch->qmfWork->xLow[t][k][1] = ch->xLowSave[t][k][1];
		}
		sceClibMemcpy_safe(ch->qmfWork->xHigh[t], ch->xHighSave[t], 0x200u);
	}
	return 0;
}

static AAC_NOINLINE void sbrQmfSynth64(AacCore *core, const SbrElement *elem,
				       SbrChannel *ch, const float *qmfIn, float *outPcm)
{
	float *work = core->scratch.u.qmfSynth64.synth64Work;
	float *re = core->scratch.u.qmfSynth64.synth64Re;
	float *im = core->scratch.u.qmfSynth64.synth64Im;

	float *bufPtr = &work[4096];
	sceClibMemcpy_safe(bufPtr, ch->qmfSynthDelay, 0x1200u);

	/* Apply DRC QMF gain to HF subbands */
	int32_t tBorder, kxPrev, kxCur;
	if (ch->headerReady == 1) {
		tBorder = ch->tE[0];
		kxPrev = elem->kxPrev;
		kxCur = elem->kx;
	} else {
		tBorder = 32;
		kxPrev = 32;
		kxCur = 32;
	}

	int32_t curCh = core->sbrCurChanIdx;
	for (int32_t t = 0; t < tBorder; t++) {
		for (int32_t k = kxPrev; k < 64; k++)
			core->scratch.u.sbr.qmfOut[t][k][0] *= core->drcQmfGain[curCh][t + 6][k];
	}
	for (int32_t t = tBorder; t < 32; t++) {
		for (int32_t k = kxCur; k < 64; k++)
			core->scratch.u.sbr.qmfOut[t][k][0] *= core->drcQmfGain[curCh][t + 6][k];
	}

	const float *inSlot = qmfIn;
	for (int32_t slot = 0; slot < 32; slot++) {
		bufPtr -= 128;

		/* Pre-twiddle 64 complex inputs into 128-point FFT buffer */
		for (int32_t k = 0; k < 64; k++) {
			float xr = inSlot[2 * k];
			float xi = inSlot[2 * k + 1];
			float c = kSbrQmfSynth64PreCos[k];
			float s = kSbrQmfSynth64PreSin[k];
			re[k] = xr * c - xi * s;
			im[k] = xi * c + xr * s;
			re[k + 64] = 0.0f;
			im[k + 64] = 0.0f;
		}

		/* Bit-reversal permutation (56 swaps) */
		for (int32_t i = 0; i < 112; i += 2) {
			uint32_t a = kSbrQmfSynth64Bitrev[i];
			uint32_t b = kSbrQmfSynth64Bitrev[i + 1];
			float tr = re[a], ti = im[a];
			re[a] = re[b]; im[a] = im[b];
			re[b] = tr;    im[b] = ti;
		}

		/* 128-point FFT: 3 radix-4 stages + 1 radix-2 stage */
		int32_t nGroups = 32;
		int32_t span = 2;
		int32_t nextSpan = 4;
		for (int32_t stage = 0; stage < 3; stage++) {
			int32_t halfStep = span >> 1;
			int32_t twOff = (span * nGroups) >> 1;
			for (int32_t g = 0; g < nGroups; g++) {
				int32_t base = g * 2 * span;
				int32_t t1 = 0;
				int32_t t2 = 0;
				for (int32_t k = 0; k < halfStep; k++) {
					int32_t i0 = base + k;
					int32_t i1 = base + halfStep + k;
					int32_t i2 = base + span + k;
					int32_t i3 = base + span + halfStep + k;

					float c2 = kSbrQmfSynth64FftCos[t2];
					float s2 = kSbrQmfSynth64FftSin[t2];
					float c1 = kSbrQmfSynth64FftCos[t1];
					float s1 = kSbrQmfSynth64FftSin[t1];
					float c3 = kSbrQmfSynth64FftCos[twOff + t1];
					float s3 = kSbrQmfSynth64FftSin[twOff + t1];

					float x1r = re[i1] * c2 - im[i1] * s2;
					float x1i = re[i1] * s2 + im[i1] * c2;
					float x3r = re[i3] * c2 - im[i3] * s2;
					float x3i = re[i3] * s2 + im[i3] * c2;

					float a0r = re[i0] + x1r, a0i = im[i0] + x1i;
					float a1r = re[i0] - x1r, a1i = im[i0] - x1i;
					float a2r = re[i2] + x3r, a2i = im[i2] + x3i;
					float a3r = re[i2] - x3r, a3i = im[i2] - x3i;

					float b2r = a2r * c1 - a2i * s1;
					float b2i = a2r * s1 + a2i * c1;
					float b3r = a3r * c3 - a3i * s3;
					float b3i = a3r * s3 + a3i * c3;

					re[i0] = a0r + b2r; im[i0] = a0i + b2i;
					re[i2] = a0r - b2r; im[i2] = a0i - b2i;
					re[i1] = a1r + b3r; im[i1] = a1i + b3i;
					re[i3] = a1r - b3r; im[i3] = a1i - b3i;

					t1 += nGroups;
					t2 += 2 * nGroups;
				}
			}
			nGroups >>= 2;
			span = 2 * nextSpan;
			nextSpan *= 4;
		}

		/* Final radix-2 butterfly (64 pairs) */
		int32_t half = span >> 1;
		for (int32_t k = 0; k < half; k++) {
			float c = kSbrQmfSynth64FftCos[k];
			float s = kSbrQmfSynth64FftSin[k];
			float tr = re[half + k] * c - im[half + k] * s;
			float ti = re[half + k] * s + im[half + k] * c;
			re[half + k] = re[k] - tr;
			im[half + k] = im[k] - ti;
			re[k] += tr;
			im[k] += ti;
		}

		/* Post-twiddle into 128-sample delay window */
		for (int32_t k = 0; k < 128; k++) {
			bufPtr[k] = kSbrQmfSynth64PostCos[k] * re[k] -
				    kSbrQmfSynth64PostSin[k] * im[k];
		}

		/* Polyphase synthesis filterbank -> 64 output PCM samples */
		float *dst = &outPcm[64 * slot];
		for (int32_t n = 0; n < 64; n++) {
			dst[n] = bufPtr[n]        * kSbrQmfProto[n]       +
				 bufPtr[n + 192]  * kSbrQmfProto[n + 64]  +
				 bufPtr[n + 256]  * kSbrQmfProto[n + 128] +
				 bufPtr[n + 448]  * kSbrQmfProto[n + 192] +
				 bufPtr[n + 512]  * kSbrQmfProto[n + 256] +
				 bufPtr[n + 704]  * kSbrQmfProto[n + 320] +
				 bufPtr[n + 768]  * kSbrQmfProto[n + 384] +
				 bufPtr[n + 960]  * kSbrQmfProto[n + 448] +
				 bufPtr[n + 1024] * kSbrQmfProto[n + 512] +
				 bufPtr[n + 1216] * kSbrQmfProto[n + 576];
		}

		inSlot += 128;
	}

	sceClibMemcpy_safe(ch->qmfSynthDelay, work, 0x1200u);
}

static AAC_NOINLINE void sbrQmfSynth32(AacCore *core, const SbrElement *elem,
				       SbrChannel *ch, const float *qmfIn, float *outPcm)
{
	float *work = core->scratch.u.qmfSynth32.synth32Work;
	float *re = core->scratch.u.qmfSynth32.synth32Re;
	float *im = core->scratch.u.qmfSynth32.synth32Im;

	float *bufPtr = &work[2048];
	sceClibMemcpy_safe(bufPtr, ch->qmfSynthDelay, 0x900u);

	/* Apply DRC QMF gain to HF subbands */
	int32_t tBorder, kxPrev, kxCur;
	if (ch->headerReady == 1) {
		tBorder = ch->tE[0];
		kxPrev = elem->kxPrev;
		kxCur = elem->kx;
	} else {
		tBorder = 32;
		kxPrev = 32;
		kxCur = 32;
	}

	int32_t curCh = core->sbrCurChanIdx;
	for (int32_t t = 0; t < tBorder; t++) {
		for (int32_t k = kxPrev; k < 64; k++)
			core->scratch.u.sbr.qmfOut[t][k][0] *= core->drcQmfGain[curCh][t + 6][k];
	}
	for (int32_t t = tBorder; t < 32; t++) {
		for (int32_t k = kxCur; k < 64; k++)
			core->scratch.u.sbr.qmfOut[t][k][0] *= core->drcQmfGain[curCh][t + 6][k];
	}

	const float *inSlot = qmfIn;
	for (int32_t slot = 0; slot < 32; slot++) {
		bufPtr -= 64;

		/* Pre-twiddle 32 low-band complex inputs into 64-point FFT buffer */
		for (int32_t k = 0; k < 32; k++) {
			float xr = inSlot[2 * k];
			float xi = inSlot[2 * k + 1];
			float c = kSbrQmfSynth32PreCos[k];
			float s = kSbrQmfSynth32PreSin[k];
			re[k] = xr * c - xi * s;
			im[k] = xi * c + xr * s;
		}
		for (int32_t k = 32; k < 64; k++) {
			re[k] = 0.0f;
			im[k] = 0.0f;
		}

		/* Bit-reversal permutation (28 swaps) */
		for (int32_t i = 0; i < 56; i += 2) {
			uint32_t a = kSbrQmfSynth32Bitrev[i];
			uint32_t b = kSbrQmfSynth32Bitrev[i + 1];
			float tr = re[a], ti = im[a];
			re[a] = re[b]; im[a] = im[b];
			re[b] = tr;    im[b] = ti;
		}

		/* 64-point Radix-4 FFT (3 stages) */
		int32_t twStep = 32;
		int32_t halfStep = 1;
		int32_t span = 2;
		for (int32_t stage = 0; stage < 3; stage++) {
			int32_t nGroups = twStep / 2;
			int32_t twOff = halfStep * nGroups;
			for (int32_t g = 0; g < nGroups; g++) {
				int32_t base = g * 2 * span;
				int32_t t1 = 0;
				int32_t t2 = 0;
				for (int32_t k = 0; k < halfStep; k++) {
					int32_t i0 = base + k;
					int32_t i1 = base + halfStep + k;
					int32_t i2 = base + span + k;
					int32_t i3 = base + span + halfStep + k;

					float c2 = kSbrQmfSynth32FftCos[t2];
					float s2 = kSbrQmfSynth32FftSin[t2];
					float c1 = kSbrQmfSynth32FftCos[t1];
					float s1 = kSbrQmfSynth32FftSin[t1];
					float c3 = kSbrQmfSynth32FftCos[twOff + t1];
					float s3 = kSbrQmfSynth32FftSin[twOff + t1];

					float x1r = re[i1] * c2 - im[i1] * s2;
					float x1i = re[i1] * s2 + im[i1] * c2;
					float x3r = re[i3] * c2 - im[i3] * s2;
					float x3i = re[i3] * s2 + im[i3] * c2;

					float a0r = re[i0] + x1r, a0i = im[i0] + x1i;
					float a1r = re[i0] - x1r, a1i = im[i0] - x1i;
					float a2r = re[i2] + x3r, a2i = im[i2] + x3i;
					float a3r = re[i2] - x3r, a3i = im[i2] - x3i;

					float b2r = a2r * c1 - a2i * s1;
					float b2i = a2r * s1 + a2i * c1;
					float b3r = a3r * c3 - a3i * s3;
					float b3i = a3r * s3 + a3i * c3;

					re[i0] = a0r + b2r; im[i0] = a0i + b2i;
					re[i2] = a0r - b2r; im[i2] = a0i - b2i;
					re[i1] = a1r + b3r; im[i1] = a1i + b3i;
					re[i3] = a1r - b3r; im[i3] = a1i - b3i;

					t1 += nGroups;
					t2 += twStep;
				}
			}
			twStep >>= 2;
			halfStep = 2 * span;
			span *= 4;
		}

		/* Post-twiddle into 64-sample delay window */
		for (int32_t k = 0; k < 64; k++) {
			bufPtr[k] = kSbrQmfSynth32PostCos[k] * re[k] -
				    kSbrQmfSynth32PostSin[k] * im[k];
		}

		/* Polyphase synthesis filterbank -> 32 downsampled output PCM samples */
		float *dst = &outPcm[32 * slot];
		for (int32_t n = 0; n < 32; n++) {
			int32_t p = 2 * n;
			dst[n] = bufPtr[n]       * kSbrQmfProto[p]       +
				 bufPtr[n + 96]  * kSbrQmfProto[p + 64]  +
				 bufPtr[n + 128] * kSbrQmfProto[p + 128] +
				 bufPtr[n + 224] * kSbrQmfProto[p + 192] +
				 bufPtr[n + 256] * kSbrQmfProto[p + 256] +
				 bufPtr[n + 352] * kSbrQmfProto[p + 320] +
				 bufPtr[n + 384] * kSbrQmfProto[p + 384] +
				 bufPtr[n + 480] * kSbrQmfProto[p + 448] +
				 bufPtr[n + 512] * kSbrQmfProto[p + 512] +
				 bufPtr[n + 608] * kSbrQmfProto[p + 576];
		}

		inSlot += 128;
	}

	sceClibMemcpy_safe(ch->qmfSynthDelay, work, 0x900u);
}
