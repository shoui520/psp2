/*
 * The MP3 decoder ("SoCrates MP3 Decoder FIXED-1-16", the engine behind
 * mp3Engine): MPEG-1/2/2.5 Layer III in fixed point. The scalar code here
 * parses the stream, keeps the bit reservoir and decodes the Huffman
 * codes; requantization, stereo, the IMDCT and the synthesis filter bank
 * are VLIW kernels. Field names follow ISO/IEC 11172-3 and 13818-3.
 */
#pragma once

#include "../application.h"
#include "../ivc2.h"

#define MP3_GRANULE_SAMPLES	576
#define MP3_RESERVOIR_WORDS	512
#define MP3_HUFFMAN_TABLES	34
#define MP3_COUNT1_TABLE	32
#define MP3_LONG_BANDS		22
#define MP3_SHORT_BANDS		13
#define MP3_INPUT_SIZE		1516
#define MP3_LSF_BANDS		39
#define MP3_POW43_ENTRIES	1027

/* frame header modes */
#define MP3_MODE_JOINT_STEREO	1
#define MP3_MODE_MONO		3
#define MP3_MODE_EXT_INTENSITY	1
#define MP3_MODE_EXT_MS		2
#define MP3_BLOCK_SHORT		2
#define MP3_SAMPLING_8000	8

/* the frame's results (Mp3Decoder.status); 22 and over are only warnings */
#define MP3_STATUS_OK			0
#define MP3_STATUS_NO_DECODER		1
#define MP3_STATUS_NO_INPUT		2
#define MP3_STATUS_HALTED		3
#define MP3_STATUS_NO_OUTPUT		4
#define MP3_STATUS_SHORT_INPUT		5
#define MP3_STATUS_LAYER		6
#define MP3_STATUS_NO_SYNC		7
#define MP3_STATUS_FREE_FORMAT		8
#define MP3_STATUS_CRC			9
#define MP3_STATUS_SAMPLING_RATE	11
#define MP3_STATUS_BIG_VALUES		12
#define MP3_STATUS_BLOCK_TYPE		13
#define MP3_STATUS_PART2_3		14
#define MP3_STATUS_STEREO		15	/* mono with a mode extension; granules disagree */
#define MP3_STATUS_TABLE		16
#define MP3_STATUS_VERSION_CHANGE	17
#define MP3_STATUS_LAYER_CHANGE		18
#define MP3_STATUS_RESERVOIR		19
#define MP3_STATUS_ID			21
#define MP3_STATUS_WARNING		22	/* reserved or CCITT emphasis */
#define MP3_STATUS_PADDING		23
#define MP3_STATUS_BAD_HEADER		25	/* both */

/* a compile-time check of a field's offset (the VLIW kernels use them) */
#define MP3_OFFSET(type, field, offset) \
	typedef char mp3Offset_##field[offsetof(type, field) == (offset) ? 1 : -1]

/* the frame header (32 bytes) */
typedef struct Mp3Header {
	uint8_t id;			/* 1: MPEG-1 */
	uint8_t layer;
	uint8_t protection;		/* 0: a CRC follows the header */
	uint8_t bitrateIndex;
	uint8_t samplingIndex;		/* 0-2 MPEG-1, 3-5 MPEG-2, 6-8 MPEG-2.5 */
	uint8_t padding;
	uint8_t privateBit;
	uint8_t mode;
	uint8_t modeExtension;
	uint8_t copyright;
	uint8_t original;
	uint8_t emphasis;
	uint8_t initialized;		/* set when the decoder is made */
	uint8_t privateBits;		/* the side information's */
	uint8_t reserved[2];
	uint32_t bitrate;
	uint32_t samplingRate;
	uint32_t frameSize;		/* bytes */
	uint32_t mainDataSize;		/* bytes after the header, CRC and side information */
} Mp3Header;

/* a granule's side information for one channel (22 bytes) */
typedef struct Mp3Granule {
	uint16_t part23Length;
	uint16_t bigValues;
	uint16_t scalefacCompress;
	uint8_t globalGain;
	uint8_t windowSwitching;
	uint8_t blockType;
	uint8_t mixedBlock;
	uint8_t tableSelect[3];
	uint8_t subblockGain[3];
	uint8_t region0Count;
	uint8_t region1Count;
	uint8_t preflag;
	uint8_t scalefacScale;
	uint8_t count1TableSelect;
	uint8_t reserved;
} Mp3Granule;

typedef struct Mp3SideChannel {
	uint32_t scfsi[4];
	Mp3Granule granule[2];
} Mp3SideChannel;

typedef struct Mp3SideInfo {
	uint32_t mainDataBegin;
	uint8_t privateBits;
	uint8_t reserved[3];
	Mp3SideChannel channel[2];
} Mp3SideInfo;

/* a channel's scale factors (256 bytes) */
typedef struct Mp3Scalefactors {
	int32_t longBand[MP3_LONG_BANDS + 1];
	int32_t shortBand[MP3_SHORT_BANDS][3];
	int32_t reserved[2];
} Mp3Scalefactors;

/* a reader of the side information, in words byte-swapped to big endian */
typedef struct Mp3BitReader {
	const uint32_t *word;
	uint32_t bit;
} Mp3BitReader;

/*
 * The bit reservoir: main data appended by the word into a 512-word ring,
 * read from a bit position.
 */
typedef struct Mp3Reservoir {
	uint32_t writeIndex;
	uint32_t pending;		/* bytes of the last word written */
	int32_t bitCount;		/* bits read */
	int32_t readIndex;		/* the word being read (masked to the ring) */
	int32_t bitOffset;		/* within it */
	uint32_t words[MP3_RESERVOIR_WORDS];
} Mp3Reservoir;

/* the CRC-16 of a protected frame's header and side information */
typedef struct Mp3Crc {
	uint16_t polynomial;
	uint16_t value;
} Mp3Crc;

/* the VLIW kernels' state, after the decoder (16144 bytes) */
typedef struct Mp3Synthesis {
	int32_t xr[2][MP3_GRANULE_SAMPLES];		/* spectral values */
	int32_t overlap[2][MP3_GRANULE_SAMPLES];	/* the IMDCT's */
	int32_t polyphase[2][480];			/* the filter bank's */
	int32_t work[MP3_GRANULE_SAMPLES];
	int32_t imdctWork[192];
	uint8_t blockType[2];
	uint8_t windowSwitching[2];
	uint8_t mixedBlock[2];
	uint8_t samplingIndex;
	uint8_t channels;
	const uint16_t *shortBounds;
	uint32_t polyphaseOffset;
} Mp3Synthesis;

/* the decoder (23308 bytes) */
typedef struct Mp3Decoder {
	uint8_t bytesPerSample;		/* 2 or 3 */
	uint8_t interleaved;		/* the channels in one buffer */
	uint8_t bigEndianOutput;
	uint8_t reserved;
	Mp3Header header;
	uint32_t frames;
	uint32_t channels;		/* the last frame's */
	uint32_t samples;		/* the last frame's, a channel */
	uint32_t outputBytes;
	uint32_t reserved2[2];
	uint32_t lastFrameSize;
	int32_t warning;		/* a header status of 22 or more */
	int32_t status;			/* the last frame's */
	int32_t bitPosition;		/* in the input */
	int32_t bitEnd;
	int32_t bitsUsed;
	int8_t halted;
	uint8_t reserved3[3];
	uint8_t *input;
	uint32_t reserved4;
	uint8_t **output;
	uint32_t *outputSize;
	uint32_t outputSizes[2];
	uint32_t reserved5[4];
	int32_t kernelWork[MP3_GRANULE_SAMPLES];	/* the VLIW kernels' */
	uint32_t nonzero[2];		/* spectral values decoded, by channel */
	int32_t mainDataFill;		/* bytes put in the reservoir */
	int32_t granuleStart;		/* the reservoir's bit count */
	uint8_t shortWidths[MP3_SHORT_BANDS * 3];
	uint8_t reserved6;
	uint16_t longBounds[MP3_LONG_BANDS + 1];
	uint16_t reserved7;
	Mp3BitReader sideReader;
	uint32_t reserved8[4];
	Mp3Reservoir reservoir;
	uint32_t reserved9[11];
	Mp3SideInfo side;
	uint32_t reserved10[10];
	Mp3Scalefactors scalefactors[2];
	uint8_t frame[20];		/* the frame copied, running into the input */
	uint8_t inputBuffer[MP3_INPUT_SIZE];
	int32_t lsfIllegal[MP3_LSF_BANDS];	/* MPEG-2 intensity positions at their maximum */
	uint32_t headerKey;		/* the stream's fixed header bits */
	uint32_t reserved11;
	uint8_t needSync;
	uint8_t bigEndianHost;
	uint8_t started;
	uint8_t reserved12[25];
	Mp3Crc crc;
	uint16_t crcRead;
	uint16_t reserved13;
	Mp3Synthesis *synthesis;
	uint32_t reserved14[13];
	Mp3Synthesis synthesisState;
	uint16_t staleFilter[4];	/* cleared when de-emphasis is off */
	int8_t emphasis;		/* the filter's, -1 for none yet */
	uint8_t reserved15;
	int16_t filterInput[2];		/* the de-emphasis filter's state */
	int16_t filterOutput[2];
	uint16_t reserved16;
} Mp3Decoder;

MP3_OFFSET(Mp3Decoder, header, 4);
MP3_OFFSET(Mp3Decoder, status, 68);
MP3_OFFSET(Mp3Decoder, input, 88);
MP3_OFFSET(Mp3Decoder, kernelWork, 128);
MP3_OFFSET(Mp3Decoder, nonzero, 2432);
MP3_OFFSET(Mp3Decoder, shortWidths, 2448);
MP3_OFFSET(Mp3Decoder, longBounds, 2488);
MP3_OFFSET(Mp3Decoder, sideReader, 2536);
MP3_OFFSET(Mp3Decoder, reservoir, 2560);
MP3_OFFSET(Mp3Decoder, side, 4672);
MP3_OFFSET(Mp3Decoder, scalefactors, 4840);
MP3_OFFSET(Mp3Decoder, frame, 5352);
MP3_OFFSET(Mp3Decoder, lsfIllegal, 6888);
MP3_OFFSET(Mp3Decoder, headerKey, 7044);
MP3_OFFSET(Mp3Decoder, needSync, 7052);
MP3_OFFSET(Mp3Decoder, crc, 7080);
MP3_OFFSET(Mp3Decoder, synthesis, 7088);
MP3_OFFSET(Mp3Decoder, synthesisState, 7144);
MP3_OFFSET(Mp3Decoder, staleFilter, 23288);
MP3_OFFSET(Mp3Decoder, filterInput, 23298);
typedef char mp3DecoderSize[sizeof(Mp3Decoder) == 23308 ? 1 : -1];

/* the engine's handle, at the start of the memory given (64 bytes) */
typedef struct Mp3Handle {
	uint32_t size;
	uint32_t format;		/* 101/102: 16-bit, 104/105: 24-bit; odd: big endian */
	Mp3Decoder *decoder;
	uint32_t reserved;
	uint32_t reserved2;
	uint32_t reserved3[11];
} Mp3Handle;

typedef struct Mp3HuffmanTable {
	const uint32_t *entries;
	uint8_t index[16];		/* the first entry by the code's top 4 bits */
	int8_t linbits;
} Mp3HuffmanTable;

/* the scale factor bands of a sampling rate */
typedef struct Mp3BandWidths {
	const uint8_t *longWidths;	/* 22 */
	const uint8_t *shortWidths;	/* 13 x 3, by window */
	const uint8_t *mixedWidths;	/* 8 long (6 for MPEG-2), then short from band 3 */
} Mp3BandWidths;

typedef struct Mp3BandBounds {
	uint16_t longBounds[MP3_LONG_BANDS + 1];
	uint16_t shortBounds[MP3_SHORT_BANDS + 1];
} Mp3BandBounds;

/* mp3tables.c */
extern const Mp3HuffmanTable mp3HuffmanTables[MP3_HUFFMAN_TABLES];
extern const Mp3BandWidths mp3BandWidths[9];
extern const Mp3BandBounds mp3BandBounds[9];
extern const uint32_t mp3SamplingRates[3];
extern const uint32_t mp3Bitrates[2][15];
extern const uint8_t mp3ScalefactorLengths[2][16];
extern const uint8_t mp3LsfBands[6][3][4];
extern const int32_t mp3LsfIntensity[2][15];
extern const int16_t mp3EmphasisFilters[3][3];
extern const int32_t mp3Pow43[MP3_POW43_ENTRIES];

/* mp3decoder.c */
int32_t mp3DecoderInit(Mp3Decoder *decoder);
int32_t mp3DecoderReset(Mp3Decoder *decoder);
int32_t mp3DecodeFrame(Mp3Decoder *decoder);
void mp3Copy(void *dst, const void *src, int32_t size);

/* mp3huffman.c */
int32_t mp3Huffman(Mp3Decoder *decoder, uint32_t ch, uint32_t gr, int32_t part2,
	const uint8_t *shortWidths, const uint16_t *longBounds);
uint32_t mp3ReservoirRead(Mp3Reservoir *reservoir, uint32_t count);
void mp3ReservoirRewind(Mp3Reservoir *reservoir, int32_t count);
void mp3ReservoirSkip(Mp3Reservoir *reservoir, int32_t count);
void mp3CrcInit(Mp3Crc *crc);
void mp3CrcUpdate(Mp3Crc *crc, uint32_t value, uint32_t count);

/* VLIW kernels */
void mp3Requantize(Mp3Decoder *decoder, uint32_t ch, uint32_t gr);		/* 0x8e8f48 */
void mp3StereoMs(int32_t *left, int32_t *right, uint32_t count);		/* 0x8e05f8 */
void mp3StereoIntensity(int32_t *left, int32_t *right, int32_t position,
	uint32_t count);							/* 0x8e0690 */
void mp3StereoIntensityLsf(int32_t *left, int32_t *right, int32_t position,
	const int32_t *ratios, uint32_t count);					/* 0x8e0750 */
void mp3Reorder(int32_t *xr, int32_t *work, const uint16_t *shortBounds,
	uint32_t antialiasBands);						/* 0x8e0348 */
void mp3Antialias(int32_t *work, uint16_t boundaries);				/* 0x8e8bf8 */
void mp3Imdct(int32_t *work, int32_t *xr, int32_t *overlap, uint32_t blockType,
	int32_t *imdctWork, uint32_t longBands);				/* 0x8ea238 */
/* not yet written: one subband's windowed 36-point IMDCT, 36 values out */
void mp3ImdctBlock(const int32_t *lines, int32_t *output, uint32_t blockType);	/* 0x8ea688 */
void mp3Polyphase(int32_t *xr, int32_t *work, int32_t *polyphase,
	uint32_t offset);							/* 0x8ea068 */
void mp3PolyphaseWindow(const int32_t *v, int32_t *output);			/* 0x8e9a10 */
void mp3Output(const int32_t *xr, uint8_t *output, uint32_t bytesPerSample,
	uint32_t count, uint32_t stride, uint32_t bigEndian);			/* 0x8e0138 */
