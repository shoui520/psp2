/*
 * SceJpegEncArm: the ARM-side JPEG encoder's context setup (Init, the setters), in the
 * original's order. The encoding itself (Encode, the strip entry point 0x4EE1BAED, the
 * bit writer, the DCT and the header writer) is still the skeleton's stubs below.
 */

#include <skeleton.h>
#include <stddef.h>

#include "jpegencarm.h"

extern void *sceClibMemset(void *dst, int ch, SceSize len);

STUB(SceJpegEncArm_4EE1BAED)

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
