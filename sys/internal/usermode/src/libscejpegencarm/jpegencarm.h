/*
 * SceJpegEncArm: the JPEG encoder that runs on the ARM cores (baseline, 8-bit, YCbCr 4:2:0 and
 * 4:2:2 strips, Huffman tables fixed). The context the caller supplies is 0x180 bytes.
 */
#ifndef JPEGENCARM_H
#define JPEGENCARM_H

#include <scetypes.h>

#define SCE_JPEG_ARM_ENCODER_CONTEXT_SIZE		0x180

#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_SIZE		((int)0x80650300)
#define SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL		((int)0x80650301)
#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_RATIO	((int)0x80650302)
#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_FORMAT	((int)0x80650303)
#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_MODE		((int)0x80650304)
#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT	((int)0x80650305)
#define SCE_JPEG_ARM_ENCODER_ERROR_INVALID_STRIP	((int)0x80650307)

/* pixel format bits: 8/9 (bit 0: 4:2:2 strips of 8 rows) and 0x10/0x11 (bit 4: odd chroma pad) */
#define SCE_JPEG_ARM_ENCODER_FORMAT_MASK		0x30003

typedef struct SceJpegArmEncoderContext {
	SceUInt8 *pOut;			/* +0x00 next output byte */
	SceUInt8 *pOutLimit;		/* +0x04 last byte the encoder may write */
	SceUInt32 bitBuffer;		/* +0x08 */
	SceInt8 bitCount;		/* +0x0C */
	SceUInt8 ratio;			/* +0x0D compression ratio, 0x40 after Init */
	SceInt16 dcPred[3];		/* +0x0E previous DC value of each component */
	SceUInt16 validWidth;		/* +0x14 */
	SceUInt16 validHeight;		/* +0x16 */
	SceUInt16 width;		/* +0x18 */
	SceUInt16 height;		/* +0x1A */
	SceUInt8 format;		/* +0x1C */
	SceInt8 headerMode;		/* +0x1D */
	SceUInt8 headerRatio;		/* +0x1E ratio the tables were built for */
	SceUInt8 chromaPad;		/* +0x1F */
	SceUInt8 reserved20[4];		/* +0x20 */
	SceInt32 stripRow;		/* +0x24 strip rows done by the strip-wise encoder */
	SceInt32 stripColumn;		/* +0x28 MCUs done in the current row (single-MCU calls) */
	SceUInt32 outputBase;		/* +0x2C output already handed back to the caller */
	SceUInt8 reserved30[0x50];	/* +0x30 */
	SceUInt16 lumaRecip[64];	/* +0x80 reciprocal of the luma quantiser, in scan order */
	SceUInt16 chromaRecip[64];	/* +0x100 the same for chroma */
} SceJpegArmEncoderContext;

/* one image component of a strip (10 words): blocks of 8x8 taken in the order advance[] steps */
typedef struct JpegEncArmComponent {
	SceInt32 tableId;		/* quantisation and Huffman table set */
	SceInt32 blockCount;		/* blocks per MCU, 0 ends the list */
	const SceUInt8 *pBlock;		/* current block */
	SceInt32 stride;		/* bytes per row of the plane */
	SceInt32 advance[4];		/* step to the next block, per block of the MCU */
	const SceUInt8 *pBase;		/* first block of the strip row */
	SceInt32 rowStep;		/* step to the next strip row */
} JpegEncArmComponent;

extern int jpegEncArmWriteBlock(SceJpegArmEncoderContext *context, SceInt16 *block, SceInt32 component,
				const SceInt8 *huffman);
extern SceUInt8 *jpegEncArmWriteHeader(SceJpegArmEncoderContext *context);
extern int SceJpegEncArm_4EE1BAED(SceJpegArmEncoderContext *context, const void *pY, const void *pCb,
				  const void *pCr, SceUInt32 flags, void *outBuffer, SceSize outSize,
				  SceSize *pOutSize);
extern SceInt32 jpegEncArmFlushBits(SceJpegArmEncoderContext *context);
extern void jpegEncArmEncodeStrip(SceJpegArmEncoderContext *context, SceInt32 mcuCount,
				  JpegEncArmComponent *components);

#endif /* JPEGENCARM_H */
