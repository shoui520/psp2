/*
 * SceJpegEncArm: the JPEG encoder on the ARM cores. The context setup (Init, the setters), the
 * whole-image and strip-wise encoders, the Huffman bit writer, the NEON forward DCT and
 * quantisation of each strip, and the header writer.
 */

#include <skeleton.h>
#include <stddef.h>
#include <arm_neon.h>

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
	SceInt32 mcuCount, stripCount, rowStep, chromaStride, n;
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
	SceUInt8 *pStart, *pEnd;
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
	comp[3].blockCount = 0;
	comp[1].advance[0] = 8;
	comp[2].advance[0] = 8;
	comp[2].tableId = 2;
	if ((flags & 1) == 0) {
		context->bitCount = -32;
		context->bitBuffer = 0;
		context->outputBase = outBuffer != NULL ? 0 : (SceUInt32)pStart;
		pEnd = jpegEncArmWriteHeader(context);
		context->pOut = pEnd;
		if (pEnd >= context->pOutLimit)
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
			pEnd = context->pOut;
			if (pEnd >= context->pOutLimit)
				return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
			context->stripColumn++;
			row = context->stripRow;
			if ((validWidth + 15) >> 4 <= context->stripColumn) {
				context->stripColumn = 0;
				context->stripRow = ++row;
			}
		} else {
			jpegEncArmEncodeStrip(context, (validWidth + 15) >> 4, comp);
			pEnd = context->pOut;
			if (pEnd >= context->pOutLimit)
				return SCE_JPEG_ARM_ENCODER_ERROR_OUTPUT_FULL;
			row = ++context->stripRow;
		}
	}
	if (rows > row) {
		if (outBuffer != NULL) {
			SceUInt32 outputBase = context->outputBase;

			*pOutSize = pEnd - (SceUInt8 *)outBuffer;
			context->pOut = NULL;
			context->outputBase = pEnd - (SceUInt8 *)outBuffer + outputBase;
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

/* the luminance Huffman tables WriteBlock codes with: the DC and AC code lengths and codes */
static const SceInt8 jpegEncArmLumaHuffman[342] = {
	-28, -6, -5, -4, -3, -2, -2, -2, -2, -2, -2, -2, -4, -5, -6, -6,
	-7, -7, -8, -9, -9, -9, -10, -10, -11, -16, -16, -2, -5, -8, -9, -10,
	-11, -12, -12, -15, -16, -16, -16, -16, -16, -16, -16, -3, -7, -10, -12, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -4, -9, -12, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -5, -11, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -7, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -8, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -10, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -4, -4, -4, -6, -5,
	-6, -5, -6, -8, -7, -6, -7, -6, -8, -21, -11, -3, -5, -7, -9, -8,
	-9, -10, -9, -64, -66, -57, -48, -39, -30, -20, -10, -4, -7, -9, -11, -106,
	-98, -90, -82, -74, -65, -56, -47, -38, -29, -19, -9, -5, -10, -12, -113, -105,
	-97, -89, -81, -73, -64, -55, -46, -37, -28, -18, -8, -6, -10, -119, -112, -104,
	-96, -88, -80, -72, -63, -54, -45, -36, -27, -17, -7, -8, -124, -118, -111, -103,
	-95, -87, -79, -71, -62, -53, -44, -35, -26, -16, -6, -8, -123, -117, -110, -102,
	-94, -86, -78, -70, -61, -52, -43, -34, -25, -15, -5, -10, -122, -116, -109, -101,
	-93, -85, -77, -69, -60, -51, -42, -33, -24, -14, -4, -126, -121, -115, -108, -100,
	-92, -84, -76, -68, -59, -50, -41, -32, -23, -13, -3, -125, -120, -114, -107, -99,
	-91, -83, -75, -67, -58, -49, -40, -31, -22, -12, -2, -3, -3, -3, -3, -3,
	-4, -5, -6, -7, -8, -9,
};

/* the same for chrominance */
static const SceInt8 jpegEncArmChromaHuffman[342] = {
	-48, -3, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -4, -5, -5, -6,
	-6, -7, -7, -8, -9, -9, -9, -9, -11, -14, -15, -3, -6, -8, -8, -9,
	-10, -11, -11, -16, -16, -16, -16, -16, -16, -16, -16, -4, -8, -10, -10, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -5, -9, -12, -12, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -5, -11, -15, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -6, -12, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -7, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -9, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -10, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -12, -16, -16, -16, -16,
	-16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -16, -3, -5, -6, -5, -6,
	-5, -7, -6, -7, -9, -8, -7, -6, -7, -32, -61, -4, -7, -9, -8, -10,
	-7, -9, -8, -73, -64, -55, -46, -37, -28, -19, -10, -6, -10, -9, -8, -105,
	-97, -89, -81, -72, -63, -54, -45, -36, -27, -18, -9, -8, -11, -10, -9, -104,
	-96, -88, -80, -71, -62, -53, -44, -35, -26, -17, -8, -7, -10, -62, -111, -103,
	-95, -87, -79, -70, -61, -52, -43, -34, -25, -16, -7, -8, -11, -116, -110, -102,
	-94, -86, -78, -69, -60, -51, -42, -33, -24, -15, -6, -8, -120, -115, -109, -101,
	-93, -85, -77, -68, -59, -50, -41, -32, -23, -14, -5, -12, -119, -114, -108, -100,
	-92, -84, -76, -67, -58, -49, -40, -31, -22, -13, -4, -10, -118, -113, -107, -99,
	-91, -83, -75, -66, -57, -48, -39, -30, -21, -12, -3, -12, -117, -112, -106, -98,
	-90, -82, -74, -65, -56, -47, -38, -29, -20, -11, -2, -2, -2, -3, -4, -5,
	-6, -7, -8, -9, -10, -11,
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

/*
 * One pass of the forward DCT over eight vectors of four samples: the tangent-based factorisation
 * (tan(pi/8), tan(pi/16), tan(3pi/16), 1/sqrt(2) in Q15), its outputs scaled per coefficient later.
 */
#define JPEG_ENC_ARM_FDCT(x0, x1, x2, x3, x4, x5, x6, x7)				\
	do {										\
		int16x4_t tmp0 = vqadd_s16(x0, x7), tmp7 = vqsub_s16(x0, x7);		\
		int16x4_t tmp1 = vqadd_s16(x1, x6), tmp6 = vqsub_s16(x1, x6);		\
		int16x4_t tmp2 = vqadd_s16(x2, x5), tmp5 = vqsub_s16(x2, x5);		\
		int16x4_t tmp3 = vqadd_s16(x3, x4), tmp4 = vqsub_s16(x3, x4);		\
		int16x4_t tmp10 = vqadd_s16(tmp0, tmp3), tmp13 = vqsub_s16(tmp0, tmp3);	\
		int16x4_t tmp11 = vqadd_s16(tmp1, tmp2), tmp12 = vqsub_s16(tmp1, tmp2);	\
		int16x4_t sum = vqadd_s16(tmp6, tmp5), diff = vqsub_s16(tmp6, tmp5);	\
		int16x4_t z1 = vqadd_s16(vqrdmulh_lane_s16(sum, k0, 2), tmp7);		\
		int16x4_t z2 = vqadd_s16(vqrdmulh_lane_s16(sum, k0, 3), tmp7);		\
		int16x4_t z3 = vqadd_s16(vqrdmulh_lane_s16(diff, k0, 2), tmp4);		\
		int16x4_t z4 = vqadd_s16(vqrdmulh_lane_s16(diff, k0, 3), tmp4);		\
		x0 = vqadd_s16(tmp10, tmp11);						\
		x4 = vqsub_s16(tmp10, tmp11);						\
		x2 = vqadd_s16(vqrdmulh_lane_s16(tmp12, k0, 0), tmp13);			\
		x6 = vqsub_s16(vqrdmulh_lane_s16(tmp13, k0, 0), tmp12);			\
		x1 = vqadd_s16(vqrdmulh_lane_s16(z3, k0, 1), z1);			\
		x7 = vqsub_s16(vqrdmulh_lane_s16(z1, k0, 1), z3);			\
		x3 = vqadd_s16(vqrdmulh_lane_s16(z4, k1, 1), z2);			\
		x5 = vqadd_s16(vqrdmulh_lane_s16(z2, k1, 0), z4);			\
	} while (0)

/* transposes the 4x4 block of rows a..d: a..d become its columns */
#define JPEG_ENC_ARM_TRANSPOSE(a, b, c, d)						\
	do {										\
		int16x4x2_t ab = vtrn_s16(a, b), cd = vtrn_s16(c, d);			\
		int32x2x2_t ac = vtrn_s32(vreinterpret_s32_s16(ab.val[0]),		\
					  vreinterpret_s32_s16(cd.val[0]));		\
		int32x2x2_t bd = vtrn_s32(vreinterpret_s32_s16(ab.val[1]),		\
					  vreinterpret_s32_s16(cd.val[1]));		\
		a = vreinterpret_s16_s32(ac.val[0]);					\
		b = vreinterpret_s16_s32(bd.val[0]);					\
		c = vreinterpret_s16_s32(ac.val[1]);					\
		d = vreinterpret_s16_s32(bd.val[1]);					\
	} while (0)

/* scales a coefficient vector, quantises it with the reciprocals at recip and rounds to 16 bits */
#define JPEG_ENC_ARM_QUANT(out, scale, x, recip)					\
	((out) = vqshrn_n_s32(vmlal_s16(bias, *(const int16x4_t *)(recip),		\
					vqrdmulh_s16(scale, x)), 14))

/*
 * Encodes mcuCount MCUs of a strip: each component's blocks of 8x8 samples are transformed,
 * quantised into a block in transposed order and Huffman coded. The first component is luma,
 * the rest chroma; the list ends at a component with no blocks.
 */
void jpegEncArmEncodeStrip(SceJpegArmEncoderContext *context, SceInt32 mcuCount,
			   JpegEncArmComponent *components)
{
	/* tan(pi/8), tan(pi/16), 1/sqrt(2), -1/sqrt(2); tan(3pi/16), -tan(3pi/16) */
	const int16x4_t k0 = { 13573, 6518, 23170, -23170 };
	const int16x4_t k1 = { 21895, -21895, 0, 0 };
	/* the scale of each coefficient (v, u), per pair of rows v and half of u */
	const int16x4_t scale04lo = { 4096, 5681, 5352, 4816 };
	const int16x4_t scale04hi = { 4096, 4816, 5352, 5681 };
	const int16x4_t scale17lo = { 5681, 7880, 7423, 6681 };
	const int16x4_t scale17hi = { 5681, 6681, 7423, 7880 };
	const int16x4_t scale26lo = { 5352, 7423, 6992, 6293 };
	const int16x4_t scale26hi = { 5352, 6293, 6992, 7423 };
	const int16x4_t scale35lo = { 4816, 6681, 6293, 5663 };
	const int16x4_t scale35hi = { 4816, 5663, 6293, 6681 };
	int16x4_t block[16];
	JpegEncArmComponent *comp;
	const SceUInt16 *recip;
	const SceInt8 *huffman;
	const SceUInt8 *src;
	SceInt32 stride;
	SceInt32 i;

	for (; mcuCount > 0; mcuCount--) {
		recip = context->lumaRecip;
		huffman = jpegEncArmLumaHuffman;
		comp = components;
		i = 0;
		for (;;) {
			const int32x4_t bias = vdupq_n_s32(0x2000);
			const uint8x8_t offset = vdup_n_u8(0x80);
			int16x4_t lo0, lo1, lo2, lo3, lo4, lo5, lo6, lo7;
			int16x4_t hi0, hi1, hi2, hi3, hi4, hi5, hi6, hi7;
			int16x8_t row;

			src = comp->pBlock;
			stride = comp->stride;
#define JPEG_ENC_ARM_ROW(n)								\
			row = vreinterpretq_s16_u16(vsubl_u8(*(const uint8x8_t *)	\
					(src + (n) * stride), offset));			\
			lo##n = vget_low_s16(row);					\
			hi##n = vget_high_s16(row)
			JPEG_ENC_ARM_ROW(0);
			JPEG_ENC_ARM_ROW(1);
			JPEG_ENC_ARM_ROW(2);
			JPEG_ENC_ARM_ROW(3);
			JPEG_ENC_ARM_ROW(4);
			JPEG_ENC_ARM_ROW(5);
			JPEG_ENC_ARM_ROW(6);
			JPEG_ENC_ARM_ROW(7);
#undef JPEG_ENC_ARM_ROW

			/* down the columns, then the 8x8 transposed as four 4x4 blocks */
			JPEG_ENC_ARM_FDCT(lo0, lo1, lo2, lo3, lo4, lo5, lo6, lo7);
			JPEG_ENC_ARM_FDCT(hi0, hi1, hi2, hi3, hi4, hi5, hi6, hi7);
			JPEG_ENC_ARM_TRANSPOSE(lo0, lo1, lo2, lo3);
			JPEG_ENC_ARM_TRANSPOSE(lo4, lo5, lo6, lo7);
			JPEG_ENC_ARM_TRANSPOSE(hi0, hi1, hi2, hi3);
			JPEG_ENC_ARM_TRANSPOSE(hi4, hi5, hi6, hi7);

			/* along the rows: column x of rows 0-3 is lo<x> (x < 4) or hi<x-4> */
			JPEG_ENC_ARM_FDCT(lo0, lo1, lo2, lo3, hi0, hi1, hi2, hi3);
			JPEG_ENC_ARM_FDCT(lo4, lo5, lo6, lo7, hi4, hi5, hi6, hi7);

			JPEG_ENC_ARM_QUANT(block[0], scale04lo, lo0, &recip[0]);
			JPEG_ENC_ARM_QUANT(block[1], scale04hi, lo4, &recip[4]);
			JPEG_ENC_ARM_QUANT(block[2], scale17lo, lo1, &recip[8]);
			JPEG_ENC_ARM_QUANT(block[3], scale17hi, lo5, &recip[12]);
			JPEG_ENC_ARM_QUANT(block[4], scale26lo, lo2, &recip[16]);
			JPEG_ENC_ARM_QUANT(block[5], scale26hi, lo6, &recip[20]);
			JPEG_ENC_ARM_QUANT(block[6], scale35lo, lo3, &recip[24]);
			JPEG_ENC_ARM_QUANT(block[7], scale35hi, lo7, &recip[28]);
			JPEG_ENC_ARM_QUANT(block[8], scale04lo, hi0, &recip[32]);
			JPEG_ENC_ARM_QUANT(block[9], scale04hi, hi4, &recip[36]);
			JPEG_ENC_ARM_QUANT(block[10], scale35lo, hi1, &recip[40]);
			JPEG_ENC_ARM_QUANT(block[11], scale35hi, hi5, &recip[44]);
			JPEG_ENC_ARM_QUANT(block[12], scale26lo, hi2, &recip[48]);
			JPEG_ENC_ARM_QUANT(block[13], scale26hi, hi6, &recip[52]);
			JPEG_ENC_ARM_QUANT(block[14], scale17lo, hi3, &recip[56]);
			JPEG_ENC_ARM_QUANT(block[15], scale17hi, hi7, &recip[60]);

			comp->pBlock = src + comp->advance[i];
			jpegEncArmWriteBlock(context, (SceInt16 *)block, comp->tableId, huffman);
			if (++i < comp->blockCount)
				continue;
			comp++;
			if (comp->blockCount == 0)
				break;
			recip = context->chromaRecip;
			huffman = jpegEncArmChromaHuffman;
			i = 0;
		}
	}
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
