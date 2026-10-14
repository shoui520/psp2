/*
 * The JPEG codec (sceJpeg, sceJpegEnc): Sony's decoder and encoder, run for
 * the ARM's RPCs 448-454.
 */
#pragma once

#include "../application.h"

/* Sony's SDK, scejpeg.h */
#define SCE_JPEG_ERROR_IMAGE_EMPTY		((int32_t)0x80650003)
#define SCE_JPEG_ERROR_BAD_MARKER_LENGTH	((int32_t)0x80650004)
#define SCE_JPEG_ERROR_BAD_DHT_COUNTS		((int32_t)0x80650005)
#define SCE_JPEG_ERROR_BAD_DHT_INDEX		((int32_t)0x80650006)
#define SCE_JPEG_ERROR_BAD_DQT_INDEX		((int32_t)0x80650007)
#define SCE_JPEG_ERROR_DECODE_ERROR		((int32_t)0x80650009)
#define SCE_JPEG_ERROR_INVALID_POINTER		((int32_t)0x80650010)
#define SCE_JPEG_ERROR_BAD_COMPONENT_ID		((int32_t)0x80650011)
#define SCE_JPEG_ERROR_UNSUPPORT_COLORSPACE	((int32_t)0x80650013)
#define SCE_JPEG_ERROR_BAD_MCU_SIZE		((int32_t)0x80650014)
#define SCE_JPEG_ERROR_BAD_PRECISION		((int32_t)0x80650015)
#define SCE_JPEG_ERROR_UNSUPPORT_SAMPLING	((int32_t)0x80650016)
#define SCE_JPEG_ERROR_COMPONENT_COUNT		((int32_t)0x80650017)
#define SCE_JPEG_ERROR_EOI_EXPECTED		((int32_t)0x80650019)
#define SCE_JPEG_ERROR_UNSUPPORT_IMAGE_SIZE	((int32_t)0x80650020)
#define SCE_JPEG_ERROR_NO_HUFF_TABLE		((int32_t)0x80650021)
#define SCE_JPEG_ERROR_NO_QUANT_TABLE		((int32_t)0x80650022)
#define SCE_JPEG_ERROR_NO_SOI			((int32_t)0x80650023)
#define SCE_JPEG_ERROR_BAD_DQT_MARKER		((int32_t)0x80650024)
#define SCE_JPEG_ERROR_BAD_DHT_MARKER		((int32_t)0x80650025)
#define SCE_JPEG_ERROR_BAD_DRI_MARKER		((int32_t)0x80650026)
#define SCE_JPEG_ERROR_BAD_SOF_MARKER		((int32_t)0x80650027)
#define SCE_JPEG_ERROR_BAD_SOS_MARKER		((int32_t)0x80650028)
#define SCE_JPEG_ERROR_SOF_DUPLICATE		((int32_t)0x80650029)
#define SCE_JPEG_ERROR_NO_LOSSLESS_SUPPORT	((int32_t)0x80650031)
#define SCE_JPEG_ERROR_NO_ARITH_SUPPORT		((int32_t)0x80650032)
#define SCE_JPEG_ERROR_UNKNOWN_MARKER		((int32_t)0x80650035)
#define SCE_JPEG_ERROR_BAD_RESTART_MARKER	((int32_t)0x80650036)
#define SCE_JPEG_ERROR_UNEXPECTED_MARKER	((int32_t)0x80650037)
#define SCE_JPEG_ERROR_INVALID_REGION		((int32_t)0x80650038)
#define SCE_JPEG_ERROR_INVALID_STATE		((int32_t)0x80650039)
#define SCE_JPEG_ERROR_CANNOT_CONTINUE		((int32_t)0x80650040)
#define SCE_JPEG_ERROR_MEMORY_SIZE		((int32_t)0x80650041)
#define SCE_JPEG_ERROR_INVALID_DECODE_MODE	((int32_t)0x80650060)
#define SCE_JPEG_ERROR_BAD_PROGRESSIVE_PARAM	((int32_t)0x80650061)
#define SCE_JPEG_ERROR_INIT_DONE		((int32_t)0x80650063)
#define SCE_JPEG_ERROR_INPUT_SUSPENDED		((int32_t)0x80650064)
#define SCE_JPEG_ERROR_INPUT_DATA_TOO_BIG	((int32_t)0x80650065)
#define SCE_JPEG_ERROR_INVALID_DATA_SIZE	((int32_t)0x80650066)

#define SCE_JPEG_MJPEG_ANY_SAMPLING_WITHOUT_DHT	2
#define SCE_JPEG_MJPEG_DOWNSCALE_MASK		0x70

/* the markers the decoder acts on */
#define JPEG_MARKER_SOF0			0xc0
#define JPEG_MARKER_SOI				0xd8
#define JPEG_MARKER_EOI				0xd9
#define JPEG_MARKER_SOS				0xda
#define JPEG_MARKER_RST0			0xd0
#define JPEG_MARKER_APP0			0xe0
#define JPEG_MARKER_APP1			0xe1

/* the decoder's working memory, aligned to 256 bytes within this */
#define JPEG_DECODER_WORK_SIZE			8192
#define JPEG_DECODER_ALIGN			256
#define JPEG_DECODER(work) \
	((JpegDecoder *)(((uint32_t)(work) + JPEG_DECODER_ALIGN - 1) & ~(JPEG_DECODER_ALIGN - 1)))

#define JPEG_COMPONENTS				4
#define JPEG_MCU_BLOCKS				24	/* blocks in an MCU, at most */
#define JPEG_BLOCK_COEF_SIZE			128	/* 64 coefficients of 16 bits */

/* the decoder's flags */
#define JPEG_DECODER_MJPEG_SAMPLING	0x1	/* YCbCr 4:2:0 or 4:2:2 only */
#define JPEG_DECODER_DEFAULT_HUFFMAN	0x2	/* the standard tables before any DHT */

/*
 * The colour space: the kind in the high nibble, the sampling of the first
 * component in the low (5 + 4 * (H >> 1) + (V >> 1): 1x1 5, 1x2 6, 1x4 7,
 * 2x1 9, 2x2 10, 4x1 13).
 */
#define JPEG_CS_GRAYSCALE			0x10
#define JPEG_CS_YCBCR				0x20	/* JFIF, or an APP0/APP1 seen */
#define JPEG_CS_RGB				0x50	/* components 'R', 'G', 'B' */
#define JPEG_CS_KIND_MASK			0xf0
#define JPEG_CS_SAMPLING_MASK			0xf
#define JPEG_CS_H1V1				5
#define JPEG_CS_H2V1				9
#define JPEG_CS_H2V2				10
/* log2 of the first component's sampling, plus one */
#define JPEG_CS_VERTICAL(cs)			((cs) & 3)
#define JPEG_CS_HORIZONTAL(cs)			(((cs) >> 2) & 3)

/* inputState: where the decoder stands, and whether it waits for input */
#define JPEG_INPUT_WHOLE		0	/* the whole stream given at once */
#define JPEG_INPUT_SPLIT		1	/* split, before the SOI */
#define JPEG_INPUT_HEADER		2	/* split, reading the headers */
#define JPEG_INPUT_DECODE		3	/* split, decoding */
#define JPEG_INPUT_TRAILER		4	/* split, looking for the EOI */
#define JPEG_INPUT_TRAILER_END		5	/* the same, with the stream's end given */
#define JPEG_INPUT_WAITING		0x8	/* suspended for more input */

/* scanPhase, progressive only */
#define JPEG_SCAN_NONE			0
#define JPEG_SCAN_HEADER		1	/* the next SOS to read */
#define JPEG_SCAN_DATA			2
#define JPEG_SCAN_OUTPUT		3	/* the coefficients to pixels, component by component */

#define JPEG_SCAN_FAILED		255	/* scanStatus after a decode error */

/* SceJpegOutputInfo */
typedef struct SceJpegOutputInfo {
	int32_t colorSpace;
	uint16_t imageWidth;
	uint16_t imageHeight;
	uint32_t outputBufferSize;
	uint32_t tempBufferSize;
	uint32_t coefBufferSize;
	struct {
		uint32_t x;
		uint32_t y;
	} pitch[4];
} SceJpegOutputInfo;

/*
 * A Huffman table. The next 8 bits of the stream look up a code of up to 8
 * bits. A DC code whose difference bits fit too has its length with them and
 * the difference; otherwise -length and -size. An AC code whose coefficient
 * fits has the run in the low nibble of its length, -(length with the
 * coefficient's bits) in the high, and the coefficient; otherwise the length
 * and the symbol. The bits of longer codes look up 0 (DC) or 40 (AC), and
 * those codes are found by limit and valueOffset, by length from 16 down.
 */
typedef struct JpegHuffmanTable {
	int8_t lookupLength[256];
	int8_t lookupValue[256];
	uint8_t longValue[247];		/* the symbols of codes over 8 bits, from the end */
	int8_t defined;
	uint16_t limit[8];		/* the first code past each length, 16 bits */
	uint16_t valueOffset[8];
} JpegHuffmanTable;

#define JPEG_HUFFMAN_DC			0
#define JPEG_HUFFMAN_AC			1
#define JPEG_HUFFMAN_LONG_AC		40

/* a component of the frame, and where its output goes */
typedef struct JpegComponent {
	uint8_t id;
	uint8_t blocks;			/* in an MCU */
	int16_t dcPredictor;
	uint16_t *quantTable;
	JpegHuffmanTable *dcTable;
	JpegHuffmanTable *acTable;
	uint8_t *plane;			/* where the next MCU goes */
	uint32_t pitch;
	uint32_t rows;
	uint8_t scale;			/* the downscale: 1, 2, 4 or 8 */
	uint8_t scaleShift;
	uint8_t reserved[2];
	int32_t rowSkip;		/* from an MCU row's end to the next row */
	int16_t *coef;			/* progressive: where the next block's coefficients are */
	int32_t coefRowSkip;
} JpegComponent;

/* the decoder, in its work memory or on the stack */
typedef struct JpegDecoder JpegDecoder;
typedef int32_t (*JpegScanRowFunction)(JpegDecoder *decoder);

struct JpegDecoder {
	uint16_t quantTable[JPEG_COMPONENTS][64];		/* in natural order */
	JpegHuffmanTable huffmanTable[JPEG_COMPONENTS][2];	/* [Th][DC, AC] */
	JpegComponent *mcuBlock[JPEG_MCU_BLOCKS + 1];	/* each block's component, 0 ended */
	int32_t blockStep[JPEG_MCU_BLOCKS];	/* the output from each block to the next */
	JpegComponent component[JPEG_COMPONENTS];
	const uint8_t *streamPosition;	/* the next byte to read */
	const uint8_t *streamEnd;
	const uint8_t *streamBase;	/* the buffer the stream was started in */
	uint32_t skipLength;		/* bytes of a segment still to skip */
	uint32_t bitBuffer;
	int8_t bitCount;
	uint8_t flags;
	int16_t mcuRow;
	uint16_t regionX;		/* the region decoded, in MCUs */
	uint16_t regionY;
	uint16_t regionWidth;
	uint16_t regionEnd;
	uint16_t mcuColumns;
	uint16_t mcuRows;
	uint16_t restartInterval;
	uint16_t restartsLeft;
	uint8_t restartMarker;		/* the RSTn expected next */
	uint8_t colorSpace;
	uint8_t frameColorSpace;	/* as the headers gave it */
	int8_t progressive;
	uint8_t spectralStart;
	uint8_t spectralEnd;
	uint8_t approximationLow;
	uint8_t approximationHigh;
	int16_t *coefBuffer;
	JpegScanRowFunction scanRow;	/* decodes a row of the scan */
	int16_t scanCoefs;		/* coefficients a scan completes */
	int16_t coefsDone;
	int16_t coefsTotal;
	uint16_t blockColumns;
	uint16_t scanColumns;
	uint16_t scanRows;
	int32_t coefStep[JPEG_MCU_BLOCKS];	/* the coefficients from each block to the next */
	uint16_t width;
	uint16_t height;
	int16_t mcuColumn;
	int16_t eobRun;
	uint8_t scanStatus;
	int8_t scanPhase;
	int8_t inputState;
	uint8_t downscale;
};

/* the state saved before an MCU, restored when its input runs out */
typedef struct JpegDecoderSave {
	const uint8_t *streamPosition;
	uint32_t bitBuffer;
	int8_t bitCount;
	uint8_t reserved;
	int16_t eobRun;
	struct {
		int16_t dcPredictor;
		uint16_t reserved;
		uint8_t *plane;
		int16_t *coef;
	} component[JPEG_COMPONENTS];
} JpegDecoderSave;

/* application/jpeg/decoder.c */
extern const uint8_t jpegZigzagOffset[64];
extern const int16_t idctScale8x8[8][4];
extern const int16_t idctScale4x4[2][4];
int32_t jpegDecoderStep(void *work);
int32_t jpegDecoderInit(void *work, const uint8_t *stream, uint32_t size, uint32_t downscale,
	uint32_t flags, uint32_t partial, uint32_t *coefSize);
int32_t jpegDecoderContinue(void *work, uint32_t *coefSize);
int32_t jpegDecoderStart(void *work, uint8_t *output, int32_t x, int32_t y, int32_t width,
	int32_t height, int16_t *coef);
int32_t jpegDecodeMJpegYCbCr(const uint8_t *pJpeg, uint32_t isize, uint8_t *pYCbCr,
	uint32_t osize, uint32_t flags, uint32_t downscale, int32_t *colorSpace, void *pCoefBuffer,
	uint32_t coefBufferSize);
int32_t jpegDecoderInputPosition(void *work, uint32_t *position, uint32_t *size);
int32_t jpegDecoderSetInput(void *work, uint32_t size, uint32_t isEndOfStream);
int32_t jpegGetOutputInfo(const uint8_t *pJpeg, uint32_t isize, uint32_t flags,
	uint32_t downscale, int32_t outputFormat, SceJpegOutputInfo *pOutputInfo);
int32_t jpegDecoderOutputInfo(void *work, int32_t outputFormat, int32_t outputSize,
	uint32_t coefSize, SceJpegOutputInfo *pOutputInfo);
int32_t jpegDecoderFinish(void *work);
/* for the scan decoders */
uint32_t jpegStreamFetch(const uint8_t **position, const uint8_t *end, uint32_t bits);
int32_t jpegDecoderRestart(JpegDecoder *decoder, int32_t column);
void jpegDecoderSaveState(JpegDecoder *decoder, JpegDecoderSave *save);
int32_t jpegDecoderMcuResult(JpegDecoder *decoder, int32_t result, int32_t column,
	JpegDecoderSave *save);
void jpegDcOnlyPixel(const int16_t *coef, const int16_t *quant, uint8_t *pixel);

/* application/jpeg/idct.c (VLIW): a block's samples at out, pitch bytes per row */
void jpegIdct8x8(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch);
void jpegIdct4x4(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch);
void jpegIdct2x2(const int16_t *coef, const uint16_t *quant, uint8_t *out, uint32_t pitch);

/* the scan decoders (VLIW) */
int32_t jpegDecodeMcuRow(JpegDecoder *decoder);			/* 0x889f08 */
int32_t jpegDecodeDcFirst(JpegDecoder *decoder);		/* 0x88c448 */
int32_t jpegDecodeDcRefine(JpegDecoder *decoder);		/* 0x88d458 */
int32_t jpegDecodeAcFirst(JpegDecoder *decoder);		/* 0x88da00 */
int32_t jpegDecodeAcRefine(JpegDecoder *decoder);		/* 0x88efa8 */
int32_t jpegOutputComponentRow(JpegDecoder *decoder);		/* 0x890ce8 */

/* sceJpegEnc's errors */
#define SCE_JPEGENC_ERROR_INSUFFICIENT_BUFFER	((int32_t)0x80650201)
#define SCE_JPEGENC_ERROR_INVALID_POINTER	((int32_t)0x80650205)

#define SCE_JPEGENC_PIXEL_YCBCR422		9

/* the encoder's context (SceJpegEncoderContext), in the ARM's memory */
typedef struct JpegEncoderContext {
	uint8_t *position;		/* where the output goes on */
	uint8_t *end;			/* 8 bytes before the buffer's end */
	uint32_t bitBuffer;
	int8_t bitCount;		/* -32 empty */
	uint8_t compRatio;
	int16_t dcPredictor[3];
	uint16_t width;			/* the valid region */
	uint16_t height;
	uint16_t pitch;			/* the luma plane's */
	uint16_t frameHeight;
	int8_t pixelFormat;		/* SCE_JPEGENC_PIXEL_YCBCR420 or 422 */
	int8_t headerMode;		/* SCE_JPEGENC_MODE_JPEG or MJPEG */
	uint8_t tablesCompRatio;	/* the ratio quantReciprocal was made for */
	uint8_t chromaPitchExtra;	/* added to half the pitch for the chroma planes */
	int32_t result;
	uint8_t reserved[92];
	uint16_t quantReciprocal[2][64];	/* 2^14 / q rounded, transposed */
} JpegEncoderContext;

/* a plane of the input, as the encoder walks it */
typedef struct JpegEncoderPlane {
	uint32_t component;		/* 0 luma, 1 Cb, 2 Cr */
	uint32_t blocks;		/* in an MCU, 0 ends the list */
	const uint8_t *block;		/* the next block */
	uint32_t pitch;
	int32_t blockStep[4];
	const uint8_t *row;		/* the MCU row */
	uint32_t rowStep;
} JpegEncoderPlane;

/* application/jpeg/encoder.c */
void jpegEncoderJob(uint32_t argument, uint32_t input, uint32_t inputSize, uint32_t argument4);

/* the encoder's MCU rows (VLIW, 0x891880) */
void jpegEncodeMcuRow(JpegEncoderContext *context, uint32_t columns, JpegEncoderPlane *planes);

/* the colour conversion (VLIW, 0x891778) */
int32_t jpegMJpegCsc(void *pRGBA, const uint8_t *y, const uint8_t *cb, const uint8_t *cr,
	int32_t colorOption, uint32_t xysize, uint32_t iFrameWidth);
