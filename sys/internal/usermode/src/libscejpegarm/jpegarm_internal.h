/*
 * SceJpegArm internal definitions and decoder context layout.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <arm_neon.h>
#include <scejpeg.h>

/* SceJpegArm error codes (0x806501xx) */
#define SCE_JPEGARM_ERROR_IMAGE_EMPTY			((int)0x80650103)
#define SCE_JPEGARM_ERROR_BAD_MARKER_LENGTH		((int)0x80650104)
#define SCE_JPEGARM_ERROR_BAD_DHT_COUNTS		((int)0x80650105)
#define SCE_JPEGARM_ERROR_BAD_DHT_INDEX			((int)0x80650106)
#define SCE_JPEGARM_ERROR_BAD_DQT_INDEX			((int)0x80650107)
#define SCE_JPEGARM_ERROR_DECODE_ERROR			((int)0x80650109)
#define SCE_JPEGARM_ERROR_INVALID_POINTER		((int)0x80650110)
#define SCE_JPEGARM_ERROR_BAD_COMPONENT_ID		((int)0x80650111)
#define SCE_JPEGARM_ERROR_UNSUPPORT_COLORSPACE		((int)0x80650113)
#define SCE_JPEGARM_ERROR_BAD_MCU_SIZE			((int)0x80650114)
#define SCE_JPEGARM_ERROR_BAD_PRECISION			((int)0x80650115)
#define SCE_JPEGARM_ERROR_UNSUPPORT_SAMPLING		((int)0x80650116)
#define SCE_JPEGARM_ERROR_COMPONENT_COUNT		((int)0x80650117)
#define SCE_JPEGARM_ERROR_EOI_EXPECTED			((int)0x80650119)
#define SCE_JPEGARM_ERROR_UNSUPPORT_IMAGE_SIZE		((int)0x80650120)
#define SCE_JPEGARM_ERROR_NO_HUFF_TABLE			((int)0x80650121)
#define SCE_JPEGARM_ERROR_NO_QUANT_TABLE		((int)0x80650122)
#define SCE_JPEGARM_ERROR_NO_SOI			((int)0x80650123)
#define SCE_JPEGARM_ERROR_BAD_DQT_MARKER		((int)0x80650124)
#define SCE_JPEGARM_ERROR_BAD_DHT_MARKER		((int)0x80650125)
#define SCE_JPEGARM_ERROR_BAD_DRI_MARKER		((int)0x80650126)
#define SCE_JPEGARM_ERROR_BAD_SOF_MARKER		((int)0x80650127)
#define SCE_JPEGARM_ERROR_BAD_SOS_MARKER		((int)0x80650128)
#define SCE_JPEGARM_ERROR_SOF_DUPLICATE			((int)0x80650129)
#define SCE_JPEGARM_ERROR_NO_LOSSLESS_SUPPORT		((int)0x80650131)
#define SCE_JPEGARM_ERROR_NO_ARITH_SUPPORT		((int)0x80650132)
#define SCE_JPEGARM_ERROR_UNKNOWN_MARKER		((int)0x80650135)
#define SCE_JPEGARM_ERROR_BAD_RESTART_MARKER		((int)0x80650136)
#define SCE_JPEGARM_ERROR_UNEXPECTED_MARKER		((int)0x80650137)
#define SCE_JPEGARM_ERROR_INVALID_REGION		((int)0x80650138)
#define SCE_JPEGARM_ERROR_INVALID_STATE			((int)0x80650139)
#define SCE_JPEGARM_ERROR_CANNOT_CONTINUE		((int)0x80650140)
#define SCE_JPEGARM_ERROR_MEMORY_SIZE			((int)0x80650141)
#define SCE_JPEGARM_ERROR_INVALID_COLOR_FORMAT		((int)0x80650150)
#define SCE_JPEGARM_ERROR_INVALID_DECODE_MODE		((int)0x80650160)
#define SCE_JPEGARM_ERROR_BAD_PROGRESSIVE_PARAM		((int)0x80650161)
#define SCE_JPEGARM_ERROR_HEADER_DONE			((int)0x80650163)
#define SCE_JPEGARM_ERROR_INPUT_SUSPENDED		((int)0x80650164)
#define SCE_JPEGARM_ERROR_INPUT_DATA_TOO_BIG		((int)0x80650165)
#define SCE_JPEGARM_ERROR_INVALID_DATA_SIZE		((int)0x80650166)
#define SCE_JPEGARM_ERROR_INVALID_INIT_PARAM		((int)0x80650167)

/* Internal decode flags (stored in JpegArmDecoder.flags) */
#define JPEGARM_FLAG_JFIF_ONLY		0x01
#define JPEGARM_FLAG_DEFAULT_DHT	0x02
#define JPEGARM_FLAG_OUTPUT_RGBA	0x04
#define JPEGARM_FLAG_EXACT_SIZE		0x08

/* Huffman lookup table (792 bytes) */
typedef struct JpegArmHuffTable {
	int8_t fastBits[256];
	int8_t fastVal[256];
	uint8_t slowSym[247];
	uint8_t valid;
	uint16_t maxCode[8];
	uint16_t valOffset[8];
} JpegArmHuffTable;

_Static_assert(sizeof(JpegArmHuffTable) == 792, "JpegArmHuffTable");

/* Per-component decode state (44 bytes) */
typedef struct JpegArmComponent {
	uint8_t id;
	uint8_t blocksPerMcu;
	int16_t dcPred;
	int16_t *quantTable;
	JpegArmHuffTable *dcTable;
	JpegArmHuffTable *acTable;
	uint8_t *outPtr;
	uint32_t outStride;
	uint32_t outHeight;
	uint8_t downscale;
	uint8_t downscaleShift;
	uint16_t reserved;
	int32_t outRowStep;
	int16_t *coefPtr;
	int32_t coefRowStep;
} JpegArmComponent;

_Static_assert(sizeof(JpegArmComponent) == 44, "JpegArmComponent");

/* Split decode checkpoint saved before each MCU */
typedef struct JpegArmMcuCheckpoint {
	const uint8_t *streamCur;
	uint32_t bitBuf;
	int8_t bitCount;
	uint8_t pad;
	uint16_t eobRun;
	int16_t dcPred0;
	uint16_t padAlpha;
	uint8_t *outPlaneA;
	int16_t *coefPlaneA;
	int16_t dcPred1;
	uint16_t padBeta;
	uint8_t *outPlaneB;
	int16_t *coefPlaneB;
	int16_t dcPred2;
	uint16_t padGamma;
	uint8_t *outPlaneC;
	int16_t *coefPlaneC;
	int16_t dcPred3;
	uint16_t padDelta;
	uint8_t *outPlaneD;
	int16_t *coefPlaneD;
} JpegArmMcuCheckpoint;

/* Main decoder context (128-byte aligned, 7428 bytes) */
typedef struct JpegArmDecoder {
	int16_t quantTables[4][64];			/* 0x0000 */
	JpegArmHuffTable huffTables[4][2];		/* 0x0200 */
	JpegArmComponent *mcuCompPtrs[25];		/* 0x1AC0 */
	int32_t mcuBlockSteps[24];			/* 0x1B24 */
	JpegArmComponent components[4];			/* 0x1B84 */
	const uint8_t *streamCur;			/* 0x1C34 */
	const uint8_t *streamEnd;			/* 0x1C38 */
	uint8_t *streamBase;				/* 0x1C3C */
	uint32_t skipBytes;				/* 0x1C40 */
	uint32_t bitBuf;				/* 0x1C44 */
	int8_t bitCount;				/* 0x1C48 */
	uint8_t flags;					/* 0x1C49 */
	int16_t curMcuRow;				/* 0x1C4A */
	uint16_t regionStartX;				/* 0x1C4C */
	uint16_t regionStartY;				/* 0x1C4E */
	uint16_t regionWidthMcu;			/* 0x1C50 */
	uint16_t regionEndY;				/* 0x1C52 */
	uint16_t mcuCols;				/* 0x1C54 */
	uint16_t mcuRows;				/* 0x1C56 */
	uint16_t restartInterval;			/* 0x1C58 */
	uint16_t restartCountdown;			/* 0x1C5A */
	uint8_t nextRestartMarker;			/* 0x1C5C */
	uint8_t colorSpaceCode;				/* 0x1C5D */
	uint8_t scanColorSpaceCode;			/* 0x1C5E */
	uint8_t isProgressive;				/* 0x1C5F */
	uint8_t scanStart;				/* 0x1C60 */
	uint8_t scanEnd;				/* 0x1C61 */
	uint8_t approxLow;				/* 0x1C62 */
	uint8_t approxHigh;				/* 0x1C63 */
	void *coefBufBase;				/* 0x1C64 */
	int (*decodeRowFn)(struct JpegArmDecoder *);	/* 0x1C68 */
	int16_t scanCoeffCount;				/* 0x1C6C */
	int16_t progCoeffDone;				/* 0x1C6E */
	int16_t totalCoeffCount;			/* 0x1C70 */
	uint16_t compMcuCols;				/* 0x1C72 */
	uint16_t scanMcuCols;				/* 0x1C74 */
	uint16_t scanMcuRows;				/* 0x1C76 */
	int32_t coefBlockSteps[24];			/* 0x1C78 */
	uint16_t imageWidth;				/* 0x1CD8 */
	uint16_t imageHeight;				/* 0x1CDA */
	int16_t mcuColOffset;				/* 0x1CDC */
	uint16_t progEobRun;				/* 0x1CDE */
	int8_t progErrorFlag;				/* 0x1CE0 */
	uint8_t progPhase;				/* 0x1CE1 */
	uint8_t splitState;				/* 0x1CE2 */
	uint8_t downscaleFactor;			/* 0x1CE3 */
	uint8_t *rgbaOutPtr;				/* 0x1CE4 */
	uint8_t *rgbaCurPtr;				/* 0x1CE8 */
	uint32_t rgbaCurX;				/* 0x1CEC */
	uint32_t rgbaCurY;				/* 0x1CF0 */
	int16_t rgbaRowHeight;				/* 0x1CF4 */
	uint8_t hSampMask;				/* 0x1CF6 */
	uint8_t vSampMask;				/* 0x1CF7 */
	uint32_t mcuPixelWidth;				/* 0x1CF8 */
	uint32_t rgbaWidth;				/* 0x1CFC */
	uint32_t rgbaHeight;				/* 0x1D00 */
} JpegArmDecoder;

_Static_assert(offsetof(JpegArmDecoder, components) == 7044, "components");
_Static_assert(offsetof(JpegArmDecoder, streamCur) == 7220, "streamCur");
_Static_assert(offsetof(JpegArmDecoder, flags) == 7241, "flags");
_Static_assert(offsetof(JpegArmDecoder, colorSpaceCode) == 7261, "colorSpaceCode");
_Static_assert(offsetof(JpegArmDecoder, decodeRowFn) == 7272, "decodeRowFn");
_Static_assert(offsetof(JpegArmDecoder, imageWidth) == 7384, "imageWidth");
_Static_assert(offsetof(JpegArmDecoder, rgbaOutPtr) == 7396, "rgbaOutPtr");
_Static_assert(offsetof(JpegArmDecoder, rgbaHeight) == 7424, "rgbaHeight");
_Static_assert(sizeof(JpegArmDecoder) == 7428, "JpegArmDecoder");

/* Split decoder control block (128 bytes) */
typedef struct SceJpegArmSplitDecodeCtrl {
	uint8_t *pStreamBuffer;
	SceSize streamBufferSize;
	uint8_t *pWriteBuffer;
	SceSize writeBufferSize;
	int isEndOfStream;
	int decodeMode;
	int outputFormat;
	int splitMode;
	SceJpegOutputInfo outputInfo;
	void *pOutputBuffer;
	void *pCoefBuffer;
	SceSize planeSize[4];
	uint16_t regionX;
	uint16_t regionY;
	uint16_t regionWidth;
	uint16_t regionHeight;
	uint32_t state;
	void *decoderCtx;
	struct SceJpegArmSplitDecodeCtrl *selfPtr;
} SceJpegArmSplitDecodeCtrl;

_Static_assert(sizeof(SceJpegArmSplitDecodeCtrl) == 128, "SceJpegArmSplitDecodeCtrl");

/* Shared rodata tables */
extern const uint8_t g_jpegArmZigzagByteOffset[63];
extern const uint8_t g_jpegArmDefaultDht[416];

/* Imported libc helper */
void *sceClibMemset(void *dst, int c, SceSize len);

/* Internal functions */
void *jpegArmMemset(void *dst, int c, SceSize len);
int jpegArmSetupYCbCrPlanes(JpegArmDecoder *dec, uint8_t *outBuf);
int jpegArmSetupRgbaOutput(JpegArmDecoder *dec, uint8_t *outBuf);
int jpegArmSetupCoefBuffer(JpegArmDecoder *dec, void *coefBuf, SceSize *outSize);
int jpegArmSuspendSplitInput(JpegArmDecoder *dec, int err);
int jpegArmAdvanceProgressiveScan(JpegArmDecoder *dec);
int jpegArmDecodeMcuRow(void *ctxBuf);
int jpegArmParseHeaderAndSos(void *ctxBuf, SceSize *coefSizeOut);
int jpegArmInitAndParseHeader(void *ctxBuf, const uint8_t *jpegBuf, SceSize jpegSize,
	int downscale, uint8_t flags, int isSplit, SceSize *coefSizeOut);
int jpegArmBeginDecodeOutput(void *ctxBuf, uint8_t *outBuf, int valA, int posYA, int valB, int posYB, void *coefBuf);
int jpegArmDecodeFullFrame(const uint8_t *jpegBuf, SceSize jpegSize, uint8_t *outBuf, SceSize outSize,
	uint8_t flags, int downscale, int *outColorSpace, void *coefBuf, SceSize coefSize);
int jpegArmGetSplitWriteBuffer(void *ctxBuf, uint8_t **writePtrOut, SceSize *writeSizeOut);
int jpegArmCommitSplitInput(void *ctxBuf, SceSize writtenSize, int isEndOfStream);
int jpegArmGetOutputInfoInternal(const uint8_t *jpegBuf, SceSize jpegSize, uint8_t flags,
	int downscale, int outputFormat, SceJpegOutputInfo *info);
int jpegArmFillOutputInfo(void *ctxBuf, int outputFormat, SceSize ycbcrSize, SceSize coefSize,
	SceJpegOutputInfo *info);
int jpegArmGetDecodedDimensions(void *ctxBuf);
int jpegArmSetOutputPointers(void *ctxBuf, uint8_t *planes[4]);
int jpegArmParseDht(JpegArmDecoder *dec, int markerLen);

void jpegArmIdct8x8(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride);
void jpegArmIdct8x8Half(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride);
void jpegArmIdct4x4(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride);
void jpegArmIdct2x2(const int16_t *coef, const int16_t *quant, uint8_t *out, int stride);
void jpegArmDequantAndIdctBlock(JpegArmDecoder *dec, int skipOutput);
int jpegArmIdctProgressiveRowYCbCr(JpegArmDecoder *dec);
void jpegArmCscYCbCrH4ToRgba(JpegArmDecoder *dec);
void jpegArmCscYCbCrH2ToRgba(JpegArmDecoder *dec);
void jpegArmCscYCbCrH1ToRgba(JpegArmDecoder *dec);

uint32_t jpegArmRefillBitBuf(const uint8_t **streamCur, const uint8_t *streamEnd, uint32_t curBits);
int jpegArmDecodeBlockBaseline(JpegArmDecoder *dec, int skipOutput);
int jpegArmRestoreMcuCheckpoint(JpegArmDecoder *dec, int err, int16_t mcuCol, const JpegArmMcuCheckpoint *cp);
int jpegArmHandleRestartMarker(JpegArmDecoder *dec, int mcuCol);
int jpegArmDecodeRowProgressiveDcFirst(JpegArmDecoder *dec);
int jpegArmDecodeRowProgressiveAcFirst(JpegArmDecoder *dec);
int jpegArmDecodeRowProgressiveDcRefine(JpegArmDecoder *dec);
int jpegArmIdctProgressiveRowRgba(JpegArmDecoder *dec);
int jpegArmDecodeRowBaselineYCbCr(JpegArmDecoder *dec);
int jpegArmDecodeRowBaselineRgba(JpegArmDecoder *dec);
int jpegArmDecodeRowProgressiveAcRefine(JpegArmDecoder *dec);
int jpegArmSetupProgressiveIdctPass(JpegArmDecoder *dec);
int jpegArmParseSos(JpegArmDecoder *dec);
int jpegArmParseMarkers(JpegArmDecoder *dec);

/* Public exports */
int sceJpegArmDecodeMJpegYCbCr(const SceUInt8 *pJpeg, SceSize isize, SceUInt8 *pYCbCr, SceSize osize,
	int decodeMode, void *pCoefBuffer, SceSize coefBufferSize);
int sceJpegArmDecodeMJpeg(const SceUInt8 *pJpeg, SceSize isize, void *pRGBA, SceSize osize,
	int decodeMode, void *pCoefBuffer, SceSize coefBufferSize);
int sceJpegArmGetOutputInfo(const SceUInt8 *pJpeg, SceSize isize, int outputFormat, int decodeMode,
	SceJpegOutputInfo *pOutputInfo);
int sceJpegArmCreateSplitDecoder(SceJpegArmSplitDecodeCtrl *pCtrl);
int sceJpegArmDeleteSplitDecoder(SceJpegArmSplitDecodeCtrl *pCtrl);
int sceJpegArmSplitDecodeMJpeg(SceJpegArmSplitDecodeCtrl *pCtrl);
