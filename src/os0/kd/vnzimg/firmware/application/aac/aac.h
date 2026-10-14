/*
 * The AAC decoder (the engine behind aacEngine): MPEG-2/4 AAC LC from ADTS
 * or raw blocks, with SBR, in fixed point. The scalar code parses the
 * stream (ISO/IEC 13818-7 and 14496-3 syntax; the names follow it) and
 * reconstructs the spectra; the spectral data's Huffman codes, the filter
 * bank and the output conversion are VLIW kernels.
 */
#pragma once

#include "../application.h"

#define AAC_MAX_CHANNELS	6
#define AAC_FRAME_SAMPLES	1024
#define AAC_MAX_SFB		120	/* 8 windows' bands */
#define AAC_MAX_SECTIONS	120
#define AAC_CODEBOOKS		12	/* the scale factors' and 11 spectral */
#define AAC_OVERLAP_WORDS	512

/* the memory layouts (aacMemorySize) */
#define AAC_LAYOUT_PLAIN	0x10000
#define AAC_LAYOUT_SBR		0x20000

/* syntactic elements (id_syn_ele) */
#define AAC_ID_SCE		0
#define AAC_ID_CPE		1
#define AAC_ID_CCE		2
#define AAC_ID_LFE		3
#define AAC_ID_DSE		4
#define AAC_ID_PCE		5
#define AAC_ID_FIL		6
#define AAC_ID_END		7

/* window sequences and codebooks */
#define AAC_ONLY_LONG_SEQUENCE		0
#define AAC_LONG_START_SEQUENCE		1
#define AAC_EIGHT_SHORT_SEQUENCE	2
#define AAC_LONG_STOP_SEQUENCE		3
#define AAC_ZERO_HCB		0
#define AAC_ESC_HCB		11
#define AAC_RESERVED_HCB	12
#define AAC_NOISE_HCB		13
#define AAC_INTENSITY_HCB2	14
#define AAC_INTENSITY_HCB	15

/* the stream formats found (AacStream.format) */
#define AAC_FORMAT_ADIF		1
#define AAC_FORMAT_ADTS		2
#define AAC_FORMAT_RAW		4

/* a frame's results (AacDecoder.lastError) */
#define AAC_STATUS_OK			0
#define AAC_STATUS_UNDERFLOW		1	/* the bits ran out */
#define AAC_STATUS_ESCAPE		2	/* an escape of more than 12 bits */
#define AAC_STATUS_CHANNELS		4	/* more channels than the instance has */
#define AAC_STATUS_CHANNEL_CHANGE	5
#define AAC_STATUS_ELEMENT		6
#define AAC_STATUS_SAMPLING_RATE	7
#define AAC_STATUS_NO_SYNC		8
#define AAC_STATUS_LAYER		10
#define AAC_STATUS_CRC			11
#define AAC_STATUS_PREDICTION		12
#define AAC_STATUS_SHORT_PULSE		13
#define AAC_STATUS_GAIN_CONTROL		14
#define AAC_STATUS_MAX_SFB		15
#define AAC_STATUS_SCE_INTENSITY	17
#define AAC_STATUS_MS_RESERVED		18
#define AAC_STATUS_TNS_ORDER		19
#define AAC_STATUS_FRAME_LENGTH		20
#define AAC_STATUS_NO_CHANNELS		21
#define AAC_STATUS_CODEBOOK		22
#define AAC_STATUS_SECTION		23
#define AAC_STATUS_PULSE_START		24
#define AAC_STATUS_RAW_BLOCKS		25
#define AAC_STATUS_PROFILE_MAIN		26
#define AAC_STATUS_PROFILE_SSR		27
#define AAC_STATUS_PROFILE_LTP		28
#define AAC_STATUS_ADIF			30

/* a compile-time check of a field's offset (the VLIW kernels use them) */
#define AAC_OFFSET(type, field, offset) \
	typedef char aacOffset_##type##_##field[offsetof(type, field) == (offset) ? 1 : -1]
#define AAC_SIZE(type, size) \
	typedef char aacSize_##type[sizeof(type) == (size) ? 1 : -1]

/* a place in the stream: the byte, and the bits of it still unread */
typedef struct AacBitPosition {
	uint32_t byte;
	int32_t bits;
} AacBitPosition;

/* a Huffman codebook: its largest value, tree, dimension and signedness */
typedef struct AacCodebook {
	uint16_t lav;
	const uint16_t *tree;
	uint8_t dimension;
	uint8_t isUnsigned;
} AacCodebook;

static inline int32_t aacMin(int32_t a, int32_t b)
{
	return a < b ? a : b;
}

static inline int32_t aacMax(int32_t a, int32_t b)
{
	return a > b ? a : b;
}

/* cpnorm.w: the redundant sign bits, 31 for 0 and -1 */
static inline int32_t aacSignBits32(int32_t value)
{
	uint32_t bits = value < 0 ? ~(uint32_t)value : (uint32_t)value;
	int32_t count = 0;

	while (count < 31 && !(bits & 0x40000000)) {
		bits <<= 1;
		count++;
	}
	return count;
}

/* cpabsz.w: |value|, saturating */
static inline int32_t aacAbs(int32_t value)
{
	if (value == (int32_t)0x80000000)
		return 0x7fffffff;
	return value < 0 ? -value : value;
}

/* cpsla3.w: a left shift saturating to 32 bits */
static inline int32_t aacShiftSaturate(int32_t value, int32_t count)
{
	int64_t wide = (int64_t)value << (count & 31);

	if (wide > 0x7fffffff)
		return 0x7fffffff;
	if (wide < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)wide;
}

/* cpsra3.w by a positive count, otherwise cpsla3.w by its negation */
static inline int32_t aacShiftDown(int32_t value, int32_t count)
{
	if (count >= 1)
		return value >> (count & 31);
	return aacShiftSaturate(value, -count);
}

/* cpsadd3.w, cpssub3.w */
static inline int32_t aacAddSaturate(int32_t a, int32_t b)
{
	int64_t sum = (int64_t)a + b;

	if (sum > 0x7fffffff)
		return 0x7fffffff;
	if (sum < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)sum;
}

static inline int32_t aacSubtractSaturate(int32_t a, int32_t b)
{
	int64_t difference = (int64_t)a - b;

	if (difference > 0x7fffffff)
		return 0x7fffffff;
	if (difference < -(int64_t)0x80000000)
		return (int32_t)0x80000000;
	return (int32_t)difference;
}

/* cpmulsl*.w: the product doubled, saturating to 64 bits */
static inline int64_t aacProduct(int32_t a, int32_t b)
{
	if (a == (int32_t)0x80000000 && b == (int32_t)0x80000000)
		return 0x7fffffffffffffffLL;
	return (int64_t)a * b * 2;
}

/* cpsmadsl*.w: a 64-bit sum saturating */
static inline int64_t aacAddWide(int64_t a, int64_t b)
{
	int64_t sum = (int64_t)((uint64_t)a + (uint64_t)b);

	if (a >= 0 && b >= 0 && sum < 0)
		return 0x7fffffffffffffffLL;
	if (a < 0 && b < 0 && sum >= 0)
		return (int64_t)0x8000000000000000ULL;
	return sum;
}

/* cpsmsb*.w: a 64-bit difference saturating */
static inline int64_t aacSubtractWide(int64_t a, int64_t b)
{
	int64_t difference = (int64_t)((uint64_t)a - (uint64_t)b);

	if (a >= 0 && b < 0 && difference < 0)
		return 0x7fffffffffffffffLL;
	if (a < 0 && b >= 0 && difference >= 0)
		return (int64_t)0x8000000000000000ULL;
	return difference;
}

#include "aactables.h"
#include "aacsbr.h"

/* perceptual noise substitution's generator */
typedef struct AacNoise {
	const int32_t *inverseSqrt;
	const int32_t *negativePowers;
	uint32_t seed;
} AacNoise;

/* the bitstream reader (in AacStream at 0x500) */
typedef struct AacBitstream {
	uint16_t crc;
	uint16_t reserved;
	uint32_t reserved2;
	uint32_t randomSeed;
	uint32_t end;			/* the last byte that may be read */
	const uint8_t *buffer;
	uint32_t reserved3;
	const uint32_t *samplingRates;
	const AacTables *tables;
	AacBitPosition position;
	int32_t *overlap[AAC_MAX_CHANNELS];	/* the filter bank's previous halves */
	int32_t *output[AAC_MAX_CHANNELS];
	uint8_t windowSequence[AAC_MAX_CHANNELS];	/* the previous frame's */
	uint8_t windowShape[AAC_MAX_CHANNELS];
	uint8_t samplingIndex;
	uint8_t reserved4;
	int8_t id;			/* 1: MPEG-2 */
	uint8_t reserved5;
	uint32_t reserved6;
	uint32_t bytesRead;
	const uint8_t *cursor;
	uint32_t reserved7[3];
	AacNoise noise;
	uint32_t reserved8[27];
} AacBitstream;

/* the ADTS header */
typedef struct AdtsHeader {
	AacBitPosition start;
	uint16_t syncword;
	uint16_t reserved;
	uint8_t id;
	uint8_t layer;
	uint8_t protectionAbsent;
	uint8_t profile;
	uint8_t samplingIndex;
	uint8_t privateBit;
	uint8_t channelConfiguration;
	uint8_t original;
	uint8_t home;
	uint8_t copyrightIdBit;
	uint8_t copyrightIdStart;
	uint8_t rawDataBlocks;		/* number_of_raw_data_blocks_in_frame */
	uint16_t frameLength;
	uint16_t bufferFullness;
	uint16_t crc;
	uint16_t reserved2;
} AdtsHeader;

typedef struct AacSectionData {
	AacBitPosition start;
	uint8_t sectCb[AAC_MAX_SECTIONS];
	uint8_t sectLen[AAC_MAX_SECTIONS];
	uint8_t sfbCb[AAC_MAX_SFB];	/* each band's codebook, by group */
} AacSectionData;

typedef struct AacIcsInfo {
	AacBitPosition start;
	uint8_t reservedBit;
	uint8_t windowSequence;
	uint8_t windowShape;
	uint8_t maxSfb;
	uint8_t scaleFactorGrouping;
	uint8_t numWindowGroups;
	uint8_t predictorDataPresent;
	uint8_t predictorReset;
	uint8_t windowGroupLength[8];
	uint8_t predictionUsed[42];
	uint8_t predictorResetGroupNumber;
	uint8_t reserved;
} AacIcsInfo;

typedef struct AacPulseData {
	AacBitPosition start;
	uint8_t pulseOffset[4];
	uint8_t pulseAmp[4];
	uint8_t numberPulse;
	uint8_t pulseStartSfb;
	uint8_t reserved[2];
} AacPulseData;

#define AAC_TNS_FILTERS		8
#define AAC_TNS_MAX_ORDER	12

typedef struct AacTnsData {
	AacBitPosition start;
	uint8_t nFilt[8];
	uint8_t coefRes[8];
	uint8_t length[AAC_TNS_FILTERS];
	uint8_t order[AAC_TNS_FILTERS];
	uint8_t direction[AAC_TNS_FILTERS];
	uint8_t coefCompress[AAC_TNS_FILTERS];
	int16_t shift[AAC_TNS_FILTERS];		/* each filter's coefficients' */
	int32_t lpc[63];			/* the filters', one after another */
	uint8_t coef[64];
} AacTnsData;

/* individual_channel_stream */
typedef struct AacIcs {
	AacBitPosition start;
	uint8_t globalGain;
	uint8_t pulseDataPresent;
	uint8_t tnsDataPresent;
	uint8_t gainControlDataPresent;
	AacSectionData section;
	AacBitPosition scaleFactorStart;
	int16_t scaleFactors[AAC_MAX_SFB];
	AacIcsInfo info;
	AacPulseData pulse;
	AacTnsData tns;
	AacBitPosition spectralStart;	/* the spectral data kernel's */
	int32_t *spectrum;		/* 1024 coefficients */
	AacBitPosition scaleFactorDataStart;
	AacBitPosition spectralDataStart;
} AacIcs;

typedef struct AacSingleChannelElement {
	AacBitPosition start;
	AacBitPosition end;
	AacIcs *ics;
	uint8_t elementInstanceTag;
	uint8_t reserved[3];
} AacSingleChannelElement;

typedef struct AacChannelPairElement {
	AacBitPosition start;
	AacBitPosition end;
	uint8_t msUsed[AAC_MAX_SFB];
	AacIcs *ics[2];
	uint8_t elementInstanceTag;
	uint8_t commonWindow;
	uint8_t msMaskPresent;
	uint8_t reserved;
} AacChannelPairElement;

typedef struct AacCouplingChannelElement {
	AacBitPosition start;
	AacBitPosition end;
	uint8_t ccTargetIsCpe[8];
	uint8_t ccTargetTagSelect[8];
	uint8_t ccLR[8];		/* cc_l and cc_r */
	uint16_t commonGain[16];
	AacIcs *ics;
	uint8_t ccDomain;
	uint8_t gainElementSign;
	uint8_t gainElementScale;
	uint8_t elementInstanceTag;
	uint8_t commonGainElementPresent[15];
	uint8_t indSwCce;
	uint8_t numCoupledElements;
	uint8_t reserved[3];
} AacCouplingChannelElement;

typedef struct AacProgramConfig {
	AacBitPosition start;
	AacBitPosition end;
	uint8_t comment[256];
	uint8_t elementInstanceTag;
	uint8_t objectType;
	uint8_t samplingIndex;
	uint8_t numFrontChannelElements;
	uint8_t numSideChannelElements;
	uint8_t numBackChannelElements;
	uint8_t numLfeChannelElements;
	uint8_t numAssocDataElements;
	uint8_t numValidCcElements;
	uint8_t monoMixdownPresent;
	uint8_t monoMixdownElementNumber;
	uint8_t stereoMixdownPresent;
	uint8_t stereoMixdownElementNumber;
	uint8_t matrixMixdownIdxPresent;
	uint8_t matrixMixdownIdx;
	uint8_t pseudoSurroundEnable;
	uint8_t frontElementIsCpe[16];
	uint8_t frontElementTagSelect[16];
	uint8_t sideElementIsCpe[16];
	uint8_t sideElementTagSelect[16];
	uint8_t backElementIsCpe[16];
	uint8_t backElementTagSelect[16];
	uint8_t ccElementIsIndSw[16];
	uint8_t validCcElementTagSelect[16];
	uint8_t lfeElementTagSelect[16];
	uint8_t assocDataElementTagSelect[8];
	uint8_t commentFieldBytes;
	uint8_t reserved[3];
} AacProgramConfig;

typedef struct AacDataStreamElement {
	AacBitPosition start;
	AacBitPosition end;
	uint16_t count;
	uint16_t reserved;
	uint8_t elementInstanceTag;
	uint8_t dataByteAlignFlag;
	uint8_t reserved2[2];
} AacDataStreamElement;

typedef struct AacFillElement {
	AacBitPosition start;
	AacBitPosition end;
	uint16_t count;
	uint16_t reserved;
	uint8_t extensionPayload[272];
} AacFillElement;

/* an element of the block, in the order read */
typedef struct AacElement {
	uint32_t id;
	uint32_t elementInstanceTag;
	uint32_t channel;		/* its first */
} AacElement;

/* the frame's syntax, in the order read */
typedef struct AacSyntax {
	AdtsHeader header;
	AacSingleChannelElement sce;
	AacChannelPairElement cpe;
	AacCouplingChannelElement *cce;
	AacIcs *ics[2];
	AacProgramConfig pce;
	AacDataStreamElement dse;
	AacFillElement fill;
	AacBitPosition frameStart;
	AacElement elements[AAC_MAX_CHANNELS];
} AacSyntax;

/* the stream's state: its header, its elements and the reader */
typedef struct AacStream {
	uint32_t crcWholeCpe;		/* a CPE's CRC over the whole element */
	uint32_t crcCheck;		/* the CRC is checked */
	uint32_t crcPresent;		/* this frame has one */
	uint32_t crcErrors;
	uint32_t value10;
	uint32_t value14;
	uint32_t format;		/* AAC_FORMAT_ */
	uint32_t frames;
	uint32_t rawDataBlocks;		/* this frame's */
	uint32_t downmix;		/* to stereo */
	uint32_t reserved;
	uint32_t channelMap[AAC_MAX_CHANNELS];
	uint32_t pceFound;
	uint32_t numElements;
	uint32_t value4c;
	uint32_t samplingRate;
	uint32_t rawDataBlock;		/* this frame's, decoded */
	uint32_t value58;
	uint32_t matrixMixdown;		/* the program config's, when it has one */
	uint32_t reserved2;
	uint32_t surroundMix;		/* not the pseudo surround one */
	uint32_t dualMono;		/* which of a dual mono's channels */
	uint32_t reserved5[2];
	uint32_t id;			/* the header's */
	uint32_t channelMode;
	int16_t channels;
	uint16_t reserved3;
	AacSyntax syntax;
	struct AacChannel *channelState[AAC_MAX_CHANNELS];
	int8_t elementChannels[AAC_MAX_CHANNELS];	/* at each element's first */
	uint8_t channelFlags[AAC_MAX_CHANNELS];
	uint32_t reserved4[15];
	AacBitstream bits;
} AacStream;

AAC_SIZE(AacStream, 0x5f8);
AAC_OFFSET(AacStream, channels, 0x7c);
AAC_OFFSET(AacStream, syntax, 0x80);
AAC_OFFSET(AacSyntax, cpe, 0x38);
AAC_OFFSET(AacSyntax, pce, 0xd8);
AAC_OFFSET(AacSyntax, frameStart, 0x3d0);
AAC_SIZE(AacSyntax, 0x420);
AAC_OFFSET(AacStream, channelState, 0x4a0);
AAC_OFFSET(AacStream, elementChannels, 0x4b8);
AAC_OFFSET(AacStream, bits, 0x500);

/* a channel's reconstruction state (0x124 bytes) */
typedef struct AacChannel {
	uint8_t reserved[0x124];
} AacChannel;

/* the instance, in the memory given after the handle */
/*
 * Each channel's buffers, from the channel memory; 3-6 only with SBR
 * (sizes in aacengine.c).
 */
#define AAC_BUFFER_ENVELOPE	0	/* 3 of the SBR envelope adjuster's */
#define AAC_BUFFER_LPC_REAL	3
#define AAC_BUFFER_LPC_IMAG	4
#define AAC_BUFFER_ANALYSIS	5	/* the QMF banks' delay lines */
#define AAC_BUFFER_SYNTHESIS	6
#define AAC_BUFFER_HISTORY	7

typedef struct AacDecoder {
	AacStream *stream;
	int32_t *sbrWork[3];		/* 8 KB each, with SBR */
	int32_t *spectra;		/* the ICSs' spectra */
	uint8_t *channelMemory;
	uint8_t *channelMemoryEnd;
	AacSbr **sbr;		/* each channel's SBR decoder */
	AacSbrChannel **sbrChannels;
	uint32_t sbrParameter;
	uint32_t sbrDownsampled;	/* SBR output at the core's rate */
	uint32_t reserved;
	uint8_t *channelBuffer[8][AAC_MAX_CHANNELS];	/* AAC_BUFFER_ */
	uint32_t rawFormat;		/* raw blocks, not ADTS */
	uint32_t reserved2;
	uint32_t samplingRate;
	uint32_t samples;
	uint32_t reserved3;
	uint32_t channels;
	uint8_t interleaved;
	uint8_t sbrStatus;
	uint8_t bytesPerSample;
	uint8_t bigEndian;
	uint8_t reserved4;
	uint8_t outputBytes;
	uint8_t reserved5[3];
	uint8_t value111;
	uint8_t outputMode;
	uint8_t sbrActive;
	uint8_t ignoreCrc;
	uint8_t reserved6[3];
	int32_t lastError;
	uint8_t reserved7[0x140 - 0x11c];
	AacSbr *sbrSlots[AAC_MAX_CHANNELS];		/* what sbr points at */
	AacSbrChannel *sbrChannelSlots[AAC_MAX_CHANNELS];
	AacSbrChannel sbrChannelData[AAC_MAX_CHANNELS];
	AacChannel channelStates[AAC_MAX_CHANNELS];
	uint8_t reserved8[12];
	AacSbrPatches sbrPatches[AAC_MAX_CHANNELS];
	uint8_t sbrPrevious[0x1688 - 0x10f4];	/* AacSbrPrevious, 240 bytes apart */
	uint32_t parametricStereo;
	uint8_t reserved9[0x16c0 - 0x168c];
	AacSbr sbrData[AAC_MAX_CHANNELS];
	AacSbrBands sbrBands[AAC_MAX_CHANNELS];
	uint8_t rateChanged;
	uint8_t reserved10[3];
	uint32_t sbrUpsampling;
	uint32_t sbrMode;
	uint8_t *sbrState;		/* 2000 bytes */
	int32_t *savedChannel;
	uint32_t numChannels;		/* the layout's */
	uint32_t layout;		/* AAC_LAYOUT_ */
} AacDecoder;

/* the handle, at the memory's start */
typedef struct AacHandle {
	uint32_t size;
	AacDecoder *decoder;
	uint32_t reserved[2];
	int32_t format;
} AacHandle;

AAC_SIZE(AacDecoder, 0x5cb4);
AAC_OFFSET(AacDecoder, channelBuffer, 0x30);
AAC_OFFSET(AacDecoder, rawFormat, 0xf0);
AAC_OFFSET(AacDecoder, interleaved, 0x108);
AAC_OFFSET(AacDecoder, lastError, 0x118);
AAC_OFFSET(AacDecoder, parametricStereo, 0x1688);
AAC_OFFSET(AacDecoder, sbrPatches, 0xefc);
AAC_OFFSET(AacDecoder, sbrPrevious, 0x10f4);
AAC_OFFSET(AacDecoder, sbrData, 0x16c0);
AAC_OFFSET(AacDecoder, sbrBands, 0x58c0);
AAC_SIZE(AacSbrBands, 164);
AAC_SIZE(AacSbrPrevious, 220);
AAC_SIZE(AacSbrAdjust, 44);
AAC_SIZE(AacSbrState, 216);
AAC_SIZE(AacSbr, 0xb00);
AAC_OFFSET(AacSbr, header, 444);
AAC_OFFSET(AacSbr, ps, 484);
AAC_OFFSET(AacPs, headerValid, 0x4d);
AAC_OFFSET(AacPs, borders, 87);
AAC_OFFSET(AacPs, iid, 104);
AAC_OFFSET(AacPs, icc, 342);
AAC_OFFSET(AacPs, qmfDelayReal, 580);
AAC_OFFSET(AacPs, subbandDelay, 912);
AAC_OFFSET(AacPs, power, 1176);
AAC_OFFSET(AacPs, ringPosition, 908);
AAC_OFFSET(AacPs, subband, 1188);
AAC_SIZE(AacPsHybrid, 48);
AAC_OFFSET(AacPs, hybrid, 1204);
AAC_OFFSET(AacPs, delayPosition, 1252);
AAC_OFFSET(AacPs, hStart, 1608);
AAC_OFFSET(AacPs, hDelta, 1960);
AAC_OFFSET(AacPs, h, 1256);
AAC_SIZE(AacPs, 2312);
AAC_OFFSET(AacSbr, work, 2796);
AAC_OFFSET(AacDecoder, sbrChannelData, 0x170);
AAC_OFFSET(AacDecoder, channelStates, 0x818);
AAC_OFFSET(AacDecoder, sbrUpsampling, 0x5c9c);

AAC_SIZE(AacCodebook, 12);
AAC_SIZE(AacTables, 340);
AAC_SIZE(AacBitstream, 0xf8);
AAC_SIZE(AdtsHeader, 32);
AAC_SIZE(AacIcsInfo, 68);
AAC_SIZE(AacTnsData, 388);
AAC_SIZE(AacIcs, 1132);
AAC_OFFSET(AacIcs, section, 0xc);
AAC_OFFSET(AacIcs, scaleFactors, 0x184);
AAC_OFFSET(AacIcs, info, 0x274);
AAC_OFFSET(AacIcs, pulse, 0x2b8);
AAC_OFFSET(AacIcs, tns, 0x2cc);
AAC_OFFSET(AacIcs, spectrum, 0x458);
AAC_OFFSET(AacTnsData, coef, 324);
AAC_OFFSET(AacBitstream, end, 0xc);
AAC_OFFSET(AacBitstream, position, 0x20);
AAC_OFFSET(AacBitstream, samplingIndex, 0x64);
AAC_OFFSET(AacBitstream, cursor, 0x70);
AAC_SIZE(AacSingleChannelElement, 24);
AAC_SIZE(AacChannelPairElement, 148);
AAC_SIZE(AacCouplingChannelElement, 100);
AAC_OFFSET(AacCouplingChannelElement, ics, 72);
AAC_SIZE(AacProgramConfig, 444);
AAC_OFFSET(AacProgramConfig, elementInstanceTag, 272);
AAC_OFFSET(AacProgramConfig, commentFieldBytes, 440);
AAC_SIZE(AacDataStreamElement, 24);
AAC_SIZE(AacFillElement, 292);

/* aacsyntax.c */
void aacOverlapClear(AacBitstream *bits, uint32_t count);
int32_t aacSeek(AacBitstream *bits, const AacBitPosition *position);
int32_t aacReadBits(AacBitstream *bits, uint16_t *value, uint32_t count);
int32_t aacReadByte(AacBitstream *bits, uint8_t *value, uint32_t count);
int32_t aacReadHuffman(AacBitstream *bits, uint16_t *value, uint32_t book);
int32_t aacByteAlign(AacBitstream *bits);
int32_t aacAdtsHeader(AacBitstream *bits, AdtsHeader *header);
int32_t aacProgramConfig(AacBitstream *bits, AacProgramConfig *pce);
int32_t aacDataStreamElement(AacBitstream *bits, AacDataStreamElement *dse);
int32_t aacSingleChannelElement(AacBitstream *bits, AacSingleChannelElement *sce);
int32_t aacLfeChannelElement(AacBitstream *bits, AacSingleChannelElement *lfe);
int32_t aacChannelPairElement(AacBitstream *bits, AacChannelPairElement *cpe);
int32_t aacCouplingChannelElement(AacBitstream *bits, AacCouplingChannelElement *cce);
int32_t aacIndividualChannelStream(AacBitstream *bits, AacIcs *ics, uint32_t commonWindow);
int32_t aacIcsInfo(AacBitstream *bits, AacIcsInfo *info);
int32_t aacSectionData(AacBitstream *bits, AacSectionData *section, const AacIcsInfo *info);
int32_t aacScaleFactorData(AacBitstream *bits, AacIcs *ics);
int32_t aacPulseData(AacBitstream *bits, AacIcs *ics);
int32_t aacTnsData(AacBitstream *bits, AacTnsData *tns, uint32_t windowSequence);
int32_t aacFillElement(AacBitstream *bits, AacFillElement *fill);
int32_t aacAdtsCheck(const AdtsHeader *header);
int32_t aacIcsCheck(const AacBitstream *bits, const AacIcs *ics);
int32_t aacSceCheck(const AacBitstream *bits, const AacSingleChannelElement *sce);
int32_t aacCpeCheck(const AacBitstream *bits, const AacChannelPairElement *cpe);
void aacSpectrumCopy(AacChannelPairElement *cpe);
int32_t aacIcsReset(AacIcs *ics);

/* aacframe.c */
uint32_t aacSamplingIndex(uint32_t rate);
int32_t aacAdtsFrame(AacStream *stream, uint32_t minimum, uint32_t size, uint32_t channels);
int32_t aacRawDataBlock(AacDecoder *decoder, int32_t *sbrFound, int32_t *samples,
	int32_t *channels, int32_t *rate, int32_t interleaved, int32_t *output);
const int32_t *aacChannelConfig(int32_t channels, AacStream *stream, AacSyntax *syntax,
	const int32_t *mix, int32_t *error);
int32_t aacPceChannelMap(int32_t *mode, int32_t channels, AacStream *stream,
	AacSyntax *syntax);
int32_t aacElements(AacDecoder *decoder, int32_t *sbrPresent);
int32_t aacCrcCompare(const AacBitstream *bits, const AdtsHeader *header);
uint16_t aacCrcUpdate(uint16_t *crc, uint32_t value, int32_t count);
int32_t aacCrcHeader(AacBitstream *bits, const AdtsHeader *header);
int32_t aacCrcSce(AacBitstream *bits, const AacSingleChannelElement *sce);
int32_t aacCrcCpe(AacBitstream *bits, const AacChannelPairElement *cpe, uint32_t whole);
int32_t aacCrcCce(AacBitstream *bits, const AacCouplingChannelElement *cce);
int32_t aacCrcPce(AacBitstream *bits, const AacProgramConfig *pce);
int32_t aacCrcDse(AacBitstream *bits, const AacDataStreamElement *dse);
int32_t aacCrcLfe(AacBitstream *bits, const AacSingleChannelElement *lfe);

/* aacspectrum.c */
void aacSceReconstruct(AacBitstream *bits, AacSingleChannelElement *sce, uint32_t channel,
	uint32_t value14, int32_t *spectra);
void aacCpeReconstruct(AacBitstream *bits, AacChannelPairElement *cpe, uint32_t channel,
	uint32_t value14, int32_t *spectra);
int32_t aacSync(AacBitPosition *start, AacBitstream *bits, uint32_t minimum, uint32_t size);
void aacBitstreamReset(AacBitstream *bits);

/* aacnoise.c */
void aacNoiseGenerate(AacNoise *noise, int32_t *out, int32_t count, int32_t scale,
	int32_t exponent, int32_t *buffer);

/* aacsbr*.c */
void aacSbrInit(uint32_t channels, AacDecoder *decoder);
void aacSbrChannelReset(AacSbrChannel *channel);
int32_t aacSbrPresent(AacSbrChannel *channel);
uint32_t aacSbrRate(AacSbrHeader *header);
void aacSbrSetRate(AacSbrHeader *header, uint32_t rate, uint32_t samples);
int32_t aacSbrCheck(AacSbrHeader *header, uint32_t what);
AacSbr *aacSbrCreate(uint32_t rate, uint32_t samples, uint32_t downsampled,
	uint32_t upsampling, int32_t *state, int32_t channels, int32_t channel, int32_t *work,
	AacDecoder *decoder);
int32_t aacSbrDecode(AacSbr *sbr, AacSbrChannel *channel, int32_t *output,
	int32_t *channels, uint32_t downsampled, uint32_t parameter, uint32_t one, int32_t *work,
	int32_t pair);
int32_t aacSbrExtension(AacBitstream *bits, AacSbrChannel *channel, uint32_t previous);

/* aactns.c */
void aacTnsDecode(AacBitstream *bits, AacTnsData *tns, uint32_t windowSequence);

/* VLIW kernels */
void aacOutput(int32_t *output, int32_t samples, int32_t channels, uint32_t downmix,
	uint32_t bits, int32_t interleaved, AacDecoder *decoder, int32_t sbrPresent,
	const int32_t *mix);
int32_t aacSpectralData(AacBitstream *bits, AacIcs *ics);
void aacDequantize(AacBitstream *bits, AacIcs *ics, AacChannelPairElement *cpe,
	uint32_t value14, int32_t *work);
void aacStereo(AacBitstream *bits, AacChannelPairElement *cpe);
void aacTnsFilter(AacBitstream *bits, AacIcs *ics);
void aacImdct(AacBitstream *bits, AacIcs *ics, int32_t *work);
void aacChannelOutput(AacBitstream *bits, AacIcs *ics, uint32_t channel);
/* the previous frame's second half, windowed into the work buffer's */
void aacOverlapShort(int32_t *overlap, int32_t *work, const int32_t *window);
void aacOverlapStart(int32_t *overlap, int32_t *work, const int32_t *window);
void aacOverlapLong(int32_t *overlap, int32_t *work, const int32_t *window);
/* this frame's first half, by window sequence */
void aacWindowShort(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *previous, const int32_t *window);
void aacWindowStop(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *window);
void aacWindowLong(const int32_t *spectrum, int32_t *work, int32_t *output,
	const int32_t *window);
