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
STUB(sceJpegArmEncoderEncode)

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
