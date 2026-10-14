/*
 * The ATRAC9 decoder (0x8809d8-0x889cc0), shared by the audio decoder
 * (RPCs 128-137) and the NGS AT9 player. It decodes the "frames" of the
 * ARM's superframes of 12, 24 or 48 kHz: mono, two mono or stereo, a
 * frame of 64, 128 or 256 samples a channel, 16-bit PCM out.
 *
 * Scalar code reads the bitstream (block parameters, scale factors, the
 * spectra); the dequantisation, the inverse MDCT and the output are VLIW
 * kernels. The names describe the steps; the format's field names (band,
 * quantization unit, gradient, codebook set, precision) are the community's.
 */
#pragma once

#include "../application.h"
#include "../ivc2.h"

#ifndef max
#define max(a, b)			((a) > (b) ? (a) : (b))
#define min(a, b)			((a) < (b) ? (a) : (b))
#endif

/* the reader's window: the last 1024 bytes of a superframe */
#define AT9_WINDOW_BYTES		1024
#define AT9_WINDOW_BITS			(AT9_WINDOW_BYTES * 8)
#define AT9_MAX_UNITS			32
#define AT9_MAX_BLOCKS			2
#define AT9_MAX_CHANNELS		2
#define AT9_FRAME_SYNC			0xfe

/* block types of the channel configurations */
#define AT9_BLOCK_MONO			0
#define AT9_BLOCK_STEREO		1

/* sample rate indices the Vita decodes: samples a frame 64, 128, 256 */
#define AT9_RATE_12000			1
#define AT9_RATE_24000			4
#define AT9_RATE_48000			7

/* the decoder's errors, negative */
#define AT9_ERROR_RATE_INDEX		(-513)
#define AT9_ERROR_CHANNEL_CONFIG	(-514)
#define AT9_ERROR_FRAME_BYTES		(-515)
#define AT9_ERROR_FRAMES		(-516)
#define AT9_ERROR_FLAGS			(-517)
#define AT9_ERROR_WORD_LENGTH		(-518)
#define AT9_ERROR_SYNC			(-519)
#define AT9_ERROR_RESERVED		(-520)
#define AT9_ERROR_PREVIOUS_MISSING	(-270)
#define AT9_ERROR_SCALE_FACTOR_MODE	(-271)
#define AT9_ERROR_FIRST_FRAME_REUSE	(-280)
#define AT9_ERROR_BAND_COUNT		(-281)
#define AT9_ERROR_STEREO_BAND		(-282)
#define AT9_ERROR_EXTENSION_BAND	(-283)
#define AT9_ERROR_GRADIENT_BOUNDARY	(-261)
#define AT9_ERROR_GRADIENT_START	(-262)
#define AT9_ERROR_GRADIENT_END		(-263)
#define AT9_ERROR_GRADIENT_ORDER	(-264)
#define AT9_ERROR_SCALE_FACTOR_RANGE	(-267)
#define AT9_ERROR_FRAME_SEQUENCE	(-571)
#define AT9_ERROR_FRAME_END		(-574)
#define AT9_ERROR_PADDING		(-577)
#define AT9_ERROR_WORD_LENGTH_CONFIG	(-560)
#define AT9_ERROR_RANGE			(-562)

/* what the decoder keeps of a block between frames (28 bytes) */
typedef struct At9Block {
	int32_t quantUnits;		/* the quantization units used */
	int32_t quantUnitsPrevious;	/* of the last frame */
	int32_t stereoUnits;		/* the units before the intensity stereo ones */
	int32_t hasExtension;
	int32_t extensionUnits;
	uint8_t *work[AT9_MAX_CHANNELS];	/* each channel's 544 bytes of state */
} At9Block;

/* the bit reader: data + bit / 8 is the next byte */
typedef struct At9Reader {
	const uint8_t *data;
	uint32_t bit;
} At9Reader;

/* the decoder (96 bytes), with the bit reader first */
typedef struct At9DecoderState {
	At9Reader reader;
	int32_t rateIndex;
	int32_t channelConfig;
	int32_t superframeBytes;
	int32_t framesPerSuperframe;
	int32_t wordLength;		/* 16 */
	int32_t frameIndex;		/* in the superframe */
	int32_t consumed;		/* bytes of the superframe read */
	At9Block block[AT9_MAX_BLOCKS];
	int32_t physical;		/* its own and the work memory are physical addresses */
} At9DecoderState;

typedef char at9DecoderStateSize[sizeof(At9DecoderState) == 96 ? 1 : -1];

/* a channel's state kept between frames (544 bytes) */
typedef struct At9Work {
	int32_t overlap[128];
	uint8_t scaleFactors[AT9_MAX_UNITS];	/* of the last frame; at +512 */
} At9Work;

/*
 * A channel while its frame is decoded (1144 bytes). The codebook set (a
 * byte a unit) starts 8 bytes before the end of the spectrum, over its last
 * two words; units below 8 never use it. At9CodebookSet gives its address.
 */
typedef struct At9Channel {
	int32_t spectrum[256];
	int8_t codebookSetHigh[24];		/* units 8 to 31 */
	int8_t scaleFactors[AT9_MAX_UNITS];
	int8_t precision[AT9_MAX_UNITS];
	int8_t precisionFine[AT9_MAX_UNITS];
} At9Channel;

typedef char at9ChannelSize[sizeof(At9Channel) == 1144 ? 1 : -1];

#define AT9_CODEBOOK_SET(channel)	((int8_t *)(channel) + 1016)

/* the block parameters of the frame being read (the original's stack frame, 132 bytes) */
typedef struct At9Params {
	int8_t gradient[AT9_MAX_UNITS];
	int8_t precisionMask[AT9_MAX_UNITS];
	At9Block *block;
	int32_t notFirst;			/* the frame is not the superframe's first */
	int32_t gradientMode;
	int8_t stereoSigns[AT9_MAX_UNITS];	/* the sign of the intensity stereo units */
	int32_t gradientStartUnit;
	int32_t gradientEndUnit;
	int32_t gradientStartValue;
	int32_t gradientEndValue;
	int32_t gradientBoundary;
	int32_t primaryChannel;			/* the stereo block's coded channel */
} At9Params;

typedef char at9ParamsSize[sizeof(At9Params) == 132 ? 1 : -1];

/* at9bits.c */
void at9ReaderStart(At9DecoderState *decoder, const uint8_t *data);
uint32_t at9Read(At9Reader *reader, uint32_t count);
uint32_t at9ReadWide(At9Reader *reader, uint32_t count);
uint32_t at9ReadBit(At9Reader *reader);
uint32_t at9Peek(const At9Reader *reader, uint32_t count);
uint32_t at9PeekWide(const At9Reader *reader, uint32_t count);
void at9Align(At9Reader *reader);
int32_t at9ReaderEnd(At9DecoderState *decoder);
int32_t at9ReaderBytes(const At9DecoderState *decoder);

/* at9tables.c */
typedef struct At9ScalarBook {
	uint8_t limit;			/* the largest symbol */
	uint8_t width;			/* bits looked up at a time */
	const uint8_t *lengths;		/* of the codes, by symbol, from the middle of its range */
	const int8_t *table;		/* the symbol of the next `width` bits */
} At9ScalarBook;

typedef struct At9VectorBook {
	uint8_t limit;
	uint8_t width;
	const uint8_t *table;		/* the record of the next `width` bits */
	const int8_t *records;		/* the code's length, then its 2 or 4 values */
} At9VectorBook;

extern const At9ScalarBook at9ScaleFactorBook[6];
extern const At9ScalarBook at9ScaleFactorSignedBook[4];
extern const At9ScalarBook at9SpectrumLowBook[3];
extern const At9ScalarBook at9SpectrumBook[36];
extern const At9VectorBook at9PairBook[3];
extern const At9VectorBook at9QuadBook[6];
extern const At9VectorBook at9PairHighBook[12];
extern const uint8_t at9GradientCurveBoundary[16][32];
extern const int16_t at9GradientCurves[48][30];
extern const uint8_t at9ScaleFactorOffsets[8][30];
extern const uint8_t at9UnitCoefficientCounts[30];
extern const uint16_t at9UnitCoefficientIndex[31];
extern const int32_t at9SpectrumScale[16];
extern const int32_t at9SpectrumScaleFine[16];
extern const uint16_t at9OutputOrder[256];
extern const uint8_t at9MaxBand[16];
extern const uint8_t at9BandToQuantUnits[16];

/* at9decoder.c */
int32_t at9FrameSamples(int32_t rateIndex);

/* at9block.c */
int32_t at9ChannelsOfBlock(int32_t type);
int32_t at9DecodeBlock(At9DecoderState *decoder, At9Block *block, At9Channel *channels,
	int32_t type);

/* the inverse MDCT of a channel's spectrum with its previous frame's overlap (at9imdct.c) */
void at9Imdct256(At9Channel *channel, At9Work *work);
void at9Imdct128(At9Channel *channel, At9Work *work);
void at9Imdct64(At9Channel *channel, At9Work *work);

/* the frame's channels as 16-bit PCM, `count` samples from `skip` (at9output.c) */
void at9Output(int32_t channelConfig, int16_t *pcm, At9Channel *channels, int32_t skip,
	int32_t count);
