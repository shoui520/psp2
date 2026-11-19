/*
 * SceJpegEncArm: the ARM-side JPEG encoder's context setup (Init, the setters), in the
 * original's order. The encoding itself (Encode, the strip entry point 0x4EE1BAED, the
 * bit writer, the DCT and the header writer) is still the skeleton's stubs below.
 */

#include <skeleton.h>
#include <stddef.h>

#include "jpegencarm.h"

extern void *sceClibMemset(void *dst, int ch, SceSize len);

SceSize sceJpegArmEncoderGetContextSize(void)
{
	return SCE_JPEG_ARM_ENCODER_CONTEXT_SIZE;
}

int sceJpegArmEncoderInit(SceJpegArmEncoderContext *context, SceUInt32 inWidth,
			  SceUInt32 inHeight, SceUInt32 pixelformat, void *outBuffer,
			  SceSize outSize)
{
	SceInt32 ret;
	SceUInt32 limit;

	ret = SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (context == NULL || ((SceUInt32)context & 3) != 0)
		return ret;
	if (pixelformat - 8 > 0x11 || ((1 << (pixelformat - 8)) & SCE_JPEG_ARM_ENCODER_FORMAT_MASK) == 0) {
		ret = SCE_JPEG_ARM_ENCODER_ERROR_INVALID_FORMAT;
		return ret;
	}
	if ((inWidth & 15) != 0) {
		ret = SCE_JPEG_ARM_ENCODER_ERROR_INVALID_SIZE;
		return ret;
	}
	ret = (pixelformat & 9) == 9 ? 7 : 15;
	ret &= inHeight;
	if (ret != 0 || inWidth - 1 > 0xFFFE || inHeight - 1 > 0xFFFE) {
		ret = SCE_JPEG_ARM_ENCODER_ERROR_INVALID_SIZE;
		return ret;
	}
	sceClibMemset(context, ret, SCE_JPEG_ARM_ENCODER_CONTEXT_SIZE);
	context->format = pixelformat;
	context->width = inWidth;
	context->validWidth = inWidth;
	context->height = inHeight;
	context->validHeight = inHeight;
	context->ratio = 0x40;
	if (pixelformat & 0x10)
		context->chromaPad = (inWidth >> 1) & 15;
	else
		context->chromaPad = ret;
	if (outBuffer != NULL) {
		if (outSize < 4) {
			ret = SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
			return ret;
		}
		limit = outSize - 4;
	} else {
		limit = 0x262;
	}
	context->pOut = outBuffer;
	context->pOutLimit = (SceUInt8 *)outBuffer + limit;
	return ret;
}

int sceJpegArmEncoderEnd(SceJpegArmEncoderContext *context)
{
	return 0;
}

int sceJpegArmEncoderSetOutputAddr(SceJpegArmEncoderContext *context, void *outBuffer,
				   SceSize outSize)
{
	if (context == NULL || ((SceUInt32)context & 3) != 0)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (outBuffer == NULL) {
		context->pOut = NULL;
		context->pOutLimit = (SceUInt8 *)0x262;
		return 0;
	}
	if (outSize < 4)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	context->pOut = outBuffer;
	context->pOutLimit = (SceUInt8 *)outBuffer + outSize - 4;
	return 0;
}

int sceJpegArmEncoderSetCompressionRatio(SceJpegArmEncoderContext *context, SceUInt32 ratio)
{
	if (context == NULL || ((SceUInt32)context & 3) != 0)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (ratio > 0xFF)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_RATIO;
	context->ratio = ratio;
	return 0;
}

int sceJpegArmEncoderSetValidRegion(SceJpegArmEncoderContext *context, SceInt32 regionWidth,
				    SceInt32 regionHeight)
{
	if (context == NULL || ((SceUInt32)context & 3) != 0)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (regionWidth <= 0 || regionWidth > context->width || regionHeight <= 0 ||
	    regionHeight > context->height)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_SIZE;
	context->validWidth = regionWidth;
	context->validHeight = regionHeight;
	return 0;
}

int sceJpegArmEncoderSetHeaderMode(SceJpegArmEncoderContext *context, SceUInt32 mode)
{
	if (context == NULL || ((SceUInt32)context & 3) != 0)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (mode > 1)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_MODE;
	context->headerMode = mode;
	return 0;
}

int sceJpegArmEncoderEncode(SceJpegArmEncoderContext *context, const void *inBuffer)
{
	JpegEncArmComponent comp[4];
	SceUInt8 *pStart;
	SceInt32 mcuCount, stripCount, rowStep, chromaStride, y, n;
	SceInt32 planeRows;
	SceUInt32 width, height;
	const SceUInt8 *pY, *pCb, *pCr;
	const SceUInt8 *pBaseCb, *pBaseCr;
	SceUInt8 *pEnd;
	SceInt32 ret;

	if (context == NULL || ((SceUInt32)context & 3) != 0 || inBuffer == NULL ||
	    ((SceUInt32)inBuffer & 7) != 0 || (pStart = context->pOut) == NULL)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (context->pOutLimit - pStart <= 609)
		return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
	mcuCount = (context->validWidth + 15) >> 4;
	width = context->width;
	height = context->height;
	if ((context->format & 9) == 9) {
		rowStep = 8 * width;
		stripCount = (context->validHeight + 7) >> 3;
		planeRows = height >> 3;
		comp[0].blockCount = 2;
		comp[0].advance[0] = 8;
		comp[0].advance[1] = 8;
		comp[0].advance[2] = 0;
		comp[0].advance[3] = 0;
	} else {
		rowStep = 16 * width;
		stripCount = (context->validHeight + 15) >> 4;
		planeRows = height >> 4;
		comp[0].blockCount = 4;
		comp[0].advance[0] = 8;
		comp[0].advance[1] = 8 * width - 8;
		comp[0].advance[2] = 8;
		comp[0].advance[3] = 8 - 8 * width;
	}
	comp[0].tableId = 0;
	comp[0].pBlock = NULL;
	comp[0].stride = width;
	comp[0].pBase = inBuffer;
	comp[0].rowStep = rowStep;
	chromaStride = context->chromaPad + (width >> 1);
	pBaseCb = (const SceUInt8 *)inBuffer + planeRows * rowStep;
	pBaseCr = pBaseCb + planeRows * 8 * chromaStride;
	comp[1].tableId = 1;
	comp[1].blockCount = 1;
	comp[1].pBlock = NULL;
	comp[1].stride = chromaStride;
	comp[1].advance[0] = 8;
	comp[1].advance[1] = 0;
	comp[1].advance[2] = 0;
	comp[1].advance[3] = 0;
	comp[1].pBase = pBaseCb;
	comp[1].rowStep = 8 * chromaStride;
	comp[2].tableId = 2;
	comp[2].blockCount = 1;
	comp[2].pBlock = NULL;
	comp[2].stride = chromaStride;
	comp[2].advance[0] = 8;
	comp[2].advance[1] = 0;
	comp[2].advance[2] = 0;
	comp[2].advance[3] = 0;
	comp[2].pBase = pBaseCr;
	comp[2].rowStep = 8 * chromaStride;
	comp[3].blockCount = 0;
	context->bitCount = -32;
	context->bitBuffer = 0;
	pEnd = jpegEncArmWriteHeader(context);
	context->pOut = pEnd;
	if (stripCount != 0) {
		if (pEnd >= context->pOutLimit)
			return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
		pY = comp[0].pBase;
		pCb = comp[1].pBase;
		pCr = comp[2].pBase;
		for (n = 0;;) {
			comp[0].pBlock = pY;
			comp[1].pBlock = pCb;
			comp[2].pBlock = pCr;
			n++;
			jpegEncArmEncodeStrip(context, mcuCount, comp);
			pY = comp[0].pBase += comp[0].rowStep;
			pCb = comp[1].pBase += comp[1].rowStep;
			pCr = comp[2].pBase += comp[2].rowStep;
			if (n == stripCount)
				break;
			if (context->pOut >= context->pOutLimit)
				return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
		}
	}
	ret = jpegEncArmFlushBits(context);
	if (ret < 0)
		return ret;
	pEnd = context->pOut;
	context->pOut = pEnd + 2;
	if (context->pOut > context->pOutLimit + 8)
		return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
	pEnd[0] = 0xFF;
	context->pOut[-1] = 0xD9;
	ret = context->pOut - pStart;
	context->headerRatio = context->ratio;
	return ret;
}

/* the strip-wise encoder: one strip row (or, with bit 1, one MCU) per call; bit 0 continues an image */
int SceJpegEncArm_4EE1BAED(SceJpegArmEncoderContext *context, const void *pY, const void *pCb,
			   const void *pCr, SceUInt32 flags, void *outBuffer, SceSize outSize,
			   SceSize *pOutSize)
{
	JpegEncArmComponent comp[4];
	SceUInt8 *pStart;
	SceInt32 ret, width, chromaStride, chromaStep, rowStep, rows, validWidth, validHeight, row;

	if (context == NULL || ((SceUInt32)context & 3) != 0 || pY == NULL || ((SceUInt32)pY & 7) != 0 ||
	    pCb == NULL || ((SceUInt32)pCb & 7) != 0 || pCr == NULL || (ret = (SceUInt32)pCr & 7) != 0)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	if (flags > 3)
		return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_STRIP;
	if (outBuffer == NULL) {
		pStart = context->pOut;
		if (pStart == NULL)
			return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
	} else {
		if (pOutSize == NULL || outSize <= 3)
			return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_ARGUMENT;
		context->pOut = outBuffer;
		pStart = outBuffer;
		context->pOutLimit = (SceUInt8 *)outBuffer + outSize - 4;
	}
	if ((flags & 1) == 0 && context->pOutLimit - pStart <= 609)
		return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
	if (flags & 2) {
		chromaStep = 64;
		chromaStride = 8;
	} else {
		width = context->width;
		chromaStride = context->chromaPad;
	}
	validWidth = context->validWidth;
	if (flags & 2) {
		width = 16;
		rowStep = 128;
	} else {
		rowStep = 8 * width;
		chromaStride += (SceUInt32)width >> 1;
	}
	if ((flags & 2) == 0)
		chromaStep = 8 * chromaStride;
	validHeight = context->validHeight;
	if ((context->format & 9) == 9) {
		comp[0].stride = width;
		comp[0].pBase = pY;
		comp[0].rowStep = rowStep;
		comp[0].tableId = 0;
		rows = (validHeight + 7) >> 3;
		comp[0].pBlock = NULL;
		comp[0].blockCount = 2;
		comp[0].advance[0] = 8;
		comp[0].advance[1] = 8;
		comp[0].advance[2] = 0;
		comp[0].advance[3] = 0;
	} else {
		comp[0].stride = width;
		comp[0].pBase = pY;
		comp[0].rowStep = 16 * width;
		comp[0].tableId = 0;
		comp[0].pBlock = NULL;
		rows = (validHeight + 15) >> 4;
		comp[0].blockCount = 4;
		comp[0].advance[0] = 8;
		comp[0].advance[1] = rowStep - 8;
		comp[0].advance[2] = 8;
		comp[0].advance[3] = 8 - rowStep;
	}
	comp[1].stride = chromaStride;
	comp[2].stride = chromaStride;
	comp[1].pBase = pCb;
	comp[2].pBase = pCr;
	comp[1].rowStep = chromaStep;
	comp[2].rowStep = chromaStep;
	comp[1].tableId = 1;
	comp[1].blockCount = 1;
	comp[2].blockCount = 1;
	comp[1].pBlock = NULL;
	comp[2].pBlock = NULL;
	comp[1].advance[0] = 0;
	comp[1].advance[1] = 0;
	comp[1].advance[2] = 0;
	comp[1].advance[3] = 0;
	comp[2].advance[1] = 0;
	comp[2].advance[2] = 0;
	comp[2].advance[3] = 0;
	comp[3].tableId = 0;
	comp[3].blockCount = 0;
	comp[1].advance[0] = 8;
	comp[2].advance[0] = 8;
	comp[2].tableId = 2;
	if ((flags & 1) == 0) {
		context->bitCount = -32;
		context->bitBuffer = 0;
		context->outputBase = outBuffer != NULL ? 0 : (SceUInt32)pStart;
		context->pOut = jpegEncArmWriteHeader(context);
		if (context->pOut >= context->pOutLimit)
			return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
		context->stripRow = 0;
		context->stripColumn = 0;
		row = 0;
	} else {
		if (rows <= context->stripRow)
			return SCE_JPEG_ARM_ENCODER_ERROR_INVALID_STRIP;
		comp[0].pBlock = pY;
		comp[1].pBlock = pCb;
		comp[2].pBlock = pCr;
		if (flags & 2) {
			jpegEncArmEncodeStrip(context, 1, comp);
			if (context->pOut >= context->pOutLimit)
				return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
			context->stripColumn++;
			row = context->stripRow;
			if ((validWidth + 15) >> 4 <= context->stripColumn) {
				context->stripColumn = 0;
				context->stripRow = ++row;
			}
		} else {
			jpegEncArmEncodeStrip(context, (validWidth + 15) >> 4, comp);
			if (context->pOut >= context->pOutLimit)
				return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
			row = ++context->stripRow;
		}
	}
	if (rows > row) {
		if (outBuffer != NULL) {
			*pOutSize = context->pOut - (SceUInt8 *)outBuffer;
			context->outputBase += context->pOut - (SceUInt8 *)outBuffer;
			context->pOut = NULL;
		}
		return ret;
	}
	ret = jpegEncArmFlushBits(context);
	if (ret < 0)
		return ret;
	pStart = context->pOut;
	context->pOut = pStart + 2;
	if (context->pOut > context->pOutLimit + 8)
		return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
	pStart[0] = 0xFF;
	context->pOut[-1] = 0xD9;
	context->headerRatio = context->ratio;
	if (outBuffer != NULL) {
		SceSize size = context->pOut - (SceUInt8 *)outBuffer;

		ret = context->outputBase + size;
		*pOutSize = size;
		context->pOut = NULL;
	} else {
		ret = (SceUInt8 *)context->pOut - (SceUInt8 *)context->outputBase;
	}
	return ret;
}

/* the scan order's step from one coefficient to the next in the transposed block */
static const SceInt8 jpegEncArmZigzagDelta[64] = {
	-14, 2, 14, 14, 16, -14, -14, -14, 2, 14, 14, 14, 14, 16, -14, -14,
	-14, -14, -14, 2, 14, 14, 14, 14, 14, 14, 16, -14, -14, -14, -14, -14,
	-14, -14, 16, 14, 14, 14, 14, 14, 14, 2, -14, -14, -14, -14, -14, 16,
	14, 14, 14, 14, 2, -14, -14, -14, 16, 14, 14, 2, -14, 16, 0, 0,
};

/* the standard luminance and chrominance quantisation tables (quality 50) */
static const SceUInt8 jpegEncArmQuantTable[128] = {
	0x10, 0x0b, 0x0c, 0x0e, 0x0c, 0x0a, 0x10, 0x0e, 0x0d, 0x0e, 0x12, 0x11, 0x10, 0x13, 0x18, 0x28,
	0x1a, 0x18, 0x16, 0x16, 0x18, 0x31, 0x23, 0x25, 0x1d, 0x28, 0x3a, 0x33, 0x3d, 0x3c, 0x39, 0x33,
	0x38, 0x37, 0x40, 0x48, 0x5c, 0x4e, 0x40, 0x44, 0x57, 0x45, 0x37, 0x38, 0x50, 0x6d, 0x51, 0x57,
	0x5f, 0x62, 0x67, 0x68, 0x67, 0x3e, 0x4d, 0x71, 0x79, 0x70, 0x64, 0x78, 0x5c, 0x65, 0x67, 0x63,
	0x11, 0x12, 0x12, 0x18, 0x15, 0x18, 0x2f, 0x1a, 0x1a, 0x2f, 0x63, 0x42, 0x38, 0x42, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
	0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63, 0x63,
};

/* DQT payload tail, SOF0 and the DHT segments the header carries */
static const SceUInt8 jpegEncArmHuffmanHeader[425] = {
	0xff, 0xc4, 0x01, 0xa2, 0x00, 0x00, 0x01, 0x05, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x10, 0x00, 0x02, 0x01, 0x03, 0x03, 0x02, 0x04, 0x03, 0x05, 0x05, 0x04, 0x04, 0x00, 0x00,
	0x01, 0x7d, 0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51,
	0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08, 0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52,
	0xd1, 0xf0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x25, 0x26,
	0x27, 0x28, 0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47,
	0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67,
	0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x83, 0x84, 0x85, 0x86, 0x87,
	0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
	0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
	0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
	0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
	0xf7, 0xf8, 0xf9, 0xfa, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a,
	0x0b, 0x11, 0x00, 0x02, 0x01, 0x02, 0x04, 0x04, 0x03, 0x04, 0x07, 0x05, 0x04, 0x04, 0x00, 0x01,
	0x02, 0x77, 0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07,
	0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33,
	0x52, 0xf0, 0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34, 0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19,
	0x1a, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x43, 0x44, 0x45, 0x46,
	0x47, 0x48, 0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x63, 0x64, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x82, 0x83, 0x84, 0x85,
	0x86, 0x87, 0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3,
	0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
	0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8,
	0xd9, 0xda, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6,
	0xf7, 0xf8, 0xf9, 0xfa, 0xff, 0xc0, 0x00, 0x11, 0x08,
};

/* SOF0 components and the SOS header */
static const SceUInt8 jpegEncArmScanHeader[24] = {
	0x03, 0x01, 0x22, 0x00, 0x02, 0x11, 0x01, 0x03, 0x11, 0x01, 0xff, 0xda, 0x00, 0x0c, 0x03, 0x01,
	0x00, 0x02, 0x11, 0x03, 0x11, 0x00, 0x3f, 0x00,
};

/* SOI, APP0 (JFIF) and the DQT marker; the quantisation tables follow at +25 */
static const SceUInt8 jpegEncArmSoiHeader[24] = {
	0xff, 0xd8, 0xff, 0xe0, 0x00, 0x10, 0x4a, 0x46, 0x49, 0x46, 0x00, 0x01, 0x02, 0x01, 0x00, 0x80,
	0x00, 0x80, 0x00, 0x00, 0xff, 0xdb, 0x00, 0x84,
};


/*
 * Appends delta bits to the bit buffer: value is already aligned for the rotation, delta is
 * the number of bits it takes (negative for the magnitude's complement count). Once the buffer
 * is full, its 32 bits go out as four bytes, each 0xFF followed by a stuffed 0.
 */
#define JPEG_ENC_ARM_PUT(value, delta)					\
	do {								\
		SceInt32 n_ = (delta);					\
		SceUInt32 t_ = ror32((value), count & 31);		\
		SceUInt32 hi_ = (t_ >> 16) << 16;			\
		count -= n_;						\
		bits ^= t_;						\
		if (__builtin_expect(count >= 0, 0)) {			\
			bits ^= hi_;					\
			if (p >= limit)					\
				goto out;				\
			count -= 32;					\
			*p = bits >> 24;				\
			if ((bits >> 24) == 0xFF)			\
				*++p = 0;				\
			p[1] = bits >> 16;				\
			if ((SceUInt8)(bits >> 16) == 0xFF) {		\
				p[2] = 0;				\
				p++;					\
			}						\
			p[2] = bits >> 8;				\
			if ((SceUInt8)(bits >> 8) == 0xFF) {		\
				p[3] = 0;				\
				p++;					\
			}						\
			p[3] = bits;					\
			if ((SceUInt8)bits == 0xFF) {			\
				p[4] = 0;				\
				p++;					\
			}						\
			p += 4;						\
			bits = hi_;					\
		}							\
	} while (0)

static inline SceUInt32 ror32(SceUInt32 value, SceUInt32 n)
{
	return (value >> n) | (value << (32 - n));
}

/* Huffman codes one 8x8 block of quantised coefficients (DC difference, AC runs, EOB) */
int jpegEncArmWriteBlock(SceJpegArmEncoderContext *context, SceInt16 *block, SceInt32 component,
			 const SceInt8 *huffman)
{
	SceUInt8 *p;
	SceUInt8 *limit;
	SceUInt32 bits;
	SceInt32 count;
	SceInt32 diff, coef, len, run, delta;
	SceUInt32 nbits;
	const SceInt8 *entry;
	const SceInt8 *zigzag, *runStart;
	SceInt32 code;
	SceInt32 finalLen;
	SceUInt32 finalValue;

	p = context->pOut;
	limit = context->pOutLimit;
	bits = context->bitBuffer;
	count = context->bitCount;
	diff = block[0] - context->dcPred[component];
	nbits = __builtin_clz(__builtin_abs(diff));
	entry = huffman - nbits;
	len = entry[362];
	code = entry[32];
	context->dcPred[component] = block[0];
	runStart = jpegEncArmZigzagDelta;
	for (;;) {
		JPEG_ENC_ARM_PUT(code << (len & 31), len);
		JPEG_ENC_ARM_PUT((diff + (diff >> 31)) << nbits, nbits - 32);
		zigzag = runStart;
		for (;;) {
			coef = block[8];
			delta = *zigzag++;
			block = (SceInt16 *)((char *)block + delta);
			if (coef != 0)
				break;
			if (delta == 0) {
				finalLen = huffman[3];
				finalValue = (SceUInt32)huffman[206] << 29;
				goto final;
			}
		}
		run = zigzag - (runStart + 17);
		diff = coef;
		nbits = __builtin_clz(__builtin_abs(coef));
		if (run >= 0) {
			len = huffman[32];
			do {
				JPEG_ENC_ARM_PUT((SceUInt32)huffman[0] << 19, len);
				run -= 16;
			} while (run >= 0);
		}
		entry = huffman + run - 16 * nbits + 512;
		len = entry[11];
		code = entry[171];
		if (delta == 0) {
			JPEG_ENC_ARM_PUT(code << (len & 31), len);
			finalLen = nbits - 32;
			finalValue = (diff + (diff >> 31)) << nbits;
			goto final;
		}
		runStart = zigzag;
	}
final:
	JPEG_ENC_ARM_PUT(finalValue, finalLen);
out:
	context->bitBuffer = bits;
	context->bitCount = count;
	context->pOut = p;
	return (int)context;
}

extern void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);

/* writes the bit buffer out as four bytes, each 0xFF followed by a stuffed 0 */
int jpegEncArmFlushBits(SceJpegArmEncoderContext *context)
{
	SceUInt8 *p;
	SceUInt32 bits;
	int ret;

	p = context->pOut;
	bits = context->bitBuffer;
	if (context->pOut >= context->pOutLimit)
		return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
	*p = bits >> 24;
	if ((bits >> 24) == 0xFF)
		*++p = 0;
	p[1] = bits >> 16;
	if ((SceUInt8)(bits >> 16) == 0xFF) {
		p[2] = 0;
		p++;
	}
	p[2] = bits >> 8;
	if ((SceUInt8)(bits >> 8) == 0xFF) {
		p[3] = 0;
		p++;
	}
	p[3] = bits;
	ret = 0;
	if ((SceUInt8)bits == 0xFF) {
		p[4] = 0;
		p++;
	}
	context->pOut = &p[(context->bitCount + 39) >> 3];
	return ret;
}

void *jpegEncArmMemcpy(void *dst, const void *src, SceSize len)
{
	return sceClibMemcpy_safe(dst, src, len);
}

SceUInt8 *jpegEncArmWriteHeader(SceJpegArmEncoderContext *context)
{
	SceUInt8 *pHeader;
	SceUInt8 *pTable;
	SceUInt8 *pQuant;
	SceUInt16 *pRecip;
	SceUInt16 width, height;
	SceInt32 ratio, i, q;
	SceInt32 rebuild;
	SceUInt8 *p;

	pHeader = context->pOut;
	width = context->validWidth;
	height = context->validHeight;
	__builtin_memcpy(pHeader, jpegEncArmSoiHeader, sizeof(jpegEncArmSoiHeader));
	if (context->headerMode != 0) {
		pHeader[6] = 'A';
		pHeader[7] = 'V';
		pHeader[8] = 'I';
		pHeader[9] = '1';
	}
	pQuant = pHeader + 25;
	pRecip = context->lumaRecip;
	ratio = context->ratio;
	pHeader[24] = 0;
	rebuild = *pRecip == 0;
	if (context->headerRatio != ratio)
		rebuild |= 1;
	if (ratio > 128)
		ratio = 2 * (ratio - 64);
	i = 0;
	if (rebuild != 0) {
		do {
			for (;;) {
				q = (ratio * jpegEncArmQuantTable[i++] + 192) >> 7;
				q = q < 255 ? q : 255;
				*pQuant++ = q;
				*pRecip = (0x4000 + (q >> 1)) / q;
				if (pHeader != pQuant - 26)
					break;
				pRecip += 8;
			}
			pRecip = (SceUInt16 *)((char *)pRecip + jpegEncArmZigzagDelta[i - 2]);
		} while (i != 64);
	} else {
		do {
			q = (ratio * jpegEncArmQuantTable[i++] + 192) >> 7;
			q = q < 255 ? q : 255;
			*pQuant++ = q;
		} while (i != 64);
	}
	pTable = pQuant;
	pRecip = context->chromaRecip;
	ratio = context->ratio;
	rebuild = *pRecip == 0;
	if (context->headerRatio != ratio)
		rebuild = 1;
	if (ratio > 128)
		ratio = 2 * (ratio - 64);
	p = pTable + 1;
	*pTable = 1;
	i = 0;
	if (rebuild != 0) {
		do {
			for (;;) {
				q = (ratio * jpegEncArmQuantTable[64 + i++] + 192) >> 7;
				q = q < 255 ? q : 255;
				*p++ = q;
				*pRecip = (0x4000 + (q >> 1)) / q;
				if (pTable != p - 2)
					break;
				pRecip += 8;
			}
			pRecip = (SceUInt16 *)((char *)pRecip + jpegEncArmZigzagDelta[i - 2]);
		} while (i != 64);
	} else {
		do {
			q = (ratio * jpegEncArmQuantTable[64 + i++] + 192) >> 7;
			q = q < 255 ? q : 255;
			*p++ = q;
		} while (i != 64);
	}
	context->dcPred[2] = 0;
	context->dcPred[1] = 0;
	context->dcPred[0] = 0;
	if (context->headerMode != 0) {
		__builtin_memcpy(p, &jpegEncArmHuffmanHeader[420], 5);
		p += 5;
	} else {
		jpegEncArmMemcpy(p, jpegEncArmHuffmanHeader, 425);
		p += 425;
	}
	p[0] = height >> 8;
	p[1] = height;
	p[2] = width >> 8;
	p[3] = width;
	jpegEncArmMemcpy(p + 4, jpegEncArmScanHeader, 24);
	p += 4;
	if ((context->format & 9) == 9)
		p[2] = 0x21;
	return p + 24;
}
