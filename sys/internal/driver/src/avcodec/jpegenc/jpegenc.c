/* No JPEG encoder source-file name survives in this module's debug records. */
#include "../audiodec/audiodec_internal.h"
#include <vnz_wrapper/venezia_rpc.h>
#include <scejpegenc.h>
#include "jpegenc_internal.h"
#include "../jpeg/jpeg_internal.h"
#include <avcodec/avcodec.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <lowio/iftu.h>
#include <string.h>

int _lockProcessSuspendCore(void);
int _unlockProcessSuspendCore(void);
int jpegLockEncodeBuffer(unsigned int *physicalAddress, const void *buffer,
			 SceSize size, unsigned int lockMode, int cacheMode,
			 unsigned int allowCached);

static unsigned int jpegEncoderChannel;

/* Nine-bit fixed-point RGB to JPEG YCbCr coefficients. */
static SceIftuCscParams jpegEncoderRgbToYCbCr = {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 1023,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 153, 300, 58 },
		{ 0xfaa, 0xf57, 256 },
		{ 256, 0xf2a, 0xfd7 },
	},
};

void jpegEncoderFinishCommand(VeneziaRpcThunk *thunk)
{
	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
}

int jpegEncoderPackCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			   unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	packed[0] = input[0];
	option.flags = 1;
	option.allocationSize = 384;
	packed[2] = input[2];
	packed[3] = input[3];
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, (void *)input[1], 384, &option);
	SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
	ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	packed[1] = thunk->memories[0].physicalBase;
	return ret;
}

int initializeJpegEncoder(void)
{
	jpegEncoderChannel = SceCodecEngineWrapperForDriver_6A1184EA(0);
	return 0;
}

int jpegEncoderProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	return 0;
}

int jpegEncoderProcessExitEntry(SceUID pid, SceUInt32 type)
{
	return 0;
}

int SceAvcodecForDriver_468949D7(SceJpegEncoderContext context, const void *pYCbCr)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderState *state = context;
	JpegEncoderControl *control = &state->control;
	unsigned char *pJpeg;
	unsigned char *pJpegEnd;
	unsigned int width, height, chromaPitch, fullChromaHeight;
	unsigned int inputSize, outputSize;
	unsigned long long wideInputSize;
	unsigned int physicalInput, physicalOutput;
	unsigned int command[5];
	unsigned int alignedOutput;
	int ret;

	ret = _lockProcessSuspendCore();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (context == NULL || ((unsigned int)context & 3) != 0)
		goto invalidPointer;
	width = control->iFrameWidth;
	height = control->iFrameHeight;
	pJpeg = control->pJpeg;
	pJpegEnd = control->pJpegEnd;
	fullChromaHeight = (control->pixelFormat & 9) == 9;
	chromaPitch = control->chromaPitchPadding + (width >> 1);
	if (width - 1 > 65534 || height - 1 > 65534 || (width & 15) != 0 ||
	    (height & ((16 >> fullChromaHeight) - 1)) != 0 ||
	    width < control->validWidth || height < control->validHeight ||
	    control->chromaPitchPadding > 15 || (chromaPitch & 7) != 0)
		goto invalidSize;
	wideInputSize = (unsigned long long)height * width +
		       (unsigned long long)chromaPitch * (height << fullChromaHeight);
	inputSize = height * width + chromaPitch * (height << fullChromaHeight);
	if (wideInputSize != inputSize)
		goto invalidSize;
	if (pYCbCr == NULL || ((unsigned int)pYCbCr & 15) != 0 ||
	    inputSize == 0 || pJpeg == NULL || ((unsigned int)pJpeg & 255) != 0)
		goto invalidPointer;
	outputSize = (unsigned int)pJpegEnd - (unsigned int)pJpeg;
	if (outputSize == 0 || (outputSize & 255) != 0)
		goto invalidPointer;
	ret = jpegLockEncodeBuffer(&physicalInput, pYCbCr, inputSize, 16, 1,
				   control->option);
	if (ret >= 0) {
		ret = jpegLockEncodeBuffer(&physicalOutput, pJpeg, outputSize, 32, 2,
					   control->option);
		if (ret >= 0) {
			control->pJpeg = (unsigned char *)physicalOutput;
			control->pJpegEnd = (unsigned char *)(physicalOutput + outputSize);
			command[0] = jpegEncoderChannel;
			command[1] = (unsigned int)state;
			command[2] = physicalInput;
			command[3] = inputSize;
			ret = SceCodecEngineWrapperForDriver_391BD4FD(453, command,
					jpegEncoderPackCommand, jpegEncoderFinishCommand);
			control->pJpeg = pJpeg;
			control->pJpegEnd = pJpegEnd;
			alignedOutput = (unsigned int)pJpeg & ~255U;
			if ((control->option & 1) != 0 && ret >= 0)
				SceCpuForDriver_02796361((void *)alignedOutput,
					((unsigned int)pJpeg + 255 - alignedOutput + ret) & ~255U);
			SceSysmemForDriver_22CBE925(32, (void *)alignedOutput,
				((unsigned int)pJpeg + 255 - alignedOutput + outputSize) & ~255U);
		}
		SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pYCbCr & ~255U),
			((unsigned int)pYCbCr + 255 - ((unsigned int)pYCbCr & ~255U) + inputSize) & ~255U);
	}
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
invalidSize:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return SCE_JPEGENC_ERROR_IMAGE_SIZE;
invalidPointer:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return SCE_JPEGENC_ERROR_INVALID_POINTER;
}

int SceAvcodecForDriver_B77B5BFC(SceJpegEncoderContext context, void *pYCbCr,
		      const void *pRGBA, int iFrameWidth,
		      int inputPixelFormat)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderState *state = context;
	JpegEncoderControl *control = &state->control;
	SceIftuConvParams conversion;
	SceIftuPlaneState source;
	SceIftuFrameBuf destination;
	unsigned int width, height, inputSize, lumaSize, chromaSize, outputSize;
	unsigned int inputPhysical, outputPhysical;
	int ret;

	if (context == NULL || ((unsigned int)context & 3) != 0 ||
	    pRGBA == NULL || ((unsigned int)pRGBA & 15) != 0 ||
	    pYCbCr == NULL || ((unsigned int)pYCbCr & 15) != 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_POINTER;
		goto finish;
	}
	width = (control->iFrameWidth + 15U) & ~15U;
	height = (control->iFrameHeight + 7U) & ~7U;
	if (height - 64 > 0x400 || width - 64 > 0x7b0 ||
	    width > (unsigned int)iFrameWidth || (unsigned int)iFrameWidth > 0x7f0 ||
	    (iFrameWidth & 3) != 0 || control->chromaPitchPadding > 15 ||
	    ((control->chromaPitchPadding + (width >> 1)) & 15) != 0) {
		ret = SCE_JPEGENC_ERROR_IMAGE_SIZE;
		goto finish;
	}
	if ((control->pixelFormat & 16) == 0 || (inputPixelFormat & ~4U) != 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_PIXELFORMAT;
		goto finish;
	}
	inputSize = 4 * height * iFrameWidth;
	lumaSize = width * height;
	chromaSize = (height >> ((control->pixelFormat & 9) != 9)) *
		     (((width >> 1) + 15) & ~15U);
	outputSize = lumaSize + 2 * chromaSize;
	ret = jpegLockEncodeBuffer(&inputPhysical, pRGBA, inputSize, 16, 1,
				   control->option);
	if (ret < 0)
		goto finish;
	ret = jpegLockEncodeBuffer(&outputPhysical, pYCbCr, outputSize, 32, 2,
				   control->option);
	if (ret < 0)
		goto unlockInput;
	source.fb.pixelformat = inputPixelFormat != 0 ? SCE_IFTU_PIXELFORMAT_RGBX8888 :
		SCE_IFTU_PIXELFORMAT_BGRX8888;
	source.fb.width = width;
	source.fb.height = height;
	source.fb.leftover_stride = 4 * (iFrameWidth - width);
	source.fb.leftover_align = 0;
	source.fb.paddr0 = inputPhysical;
	source.fb.paddr1 = 0;
	source.fb.paddr2 = 0;
	source.scaleX = 0x10000;
	source.scaleY = 0x10000;
	source.dst_x = 0;
	source.dst_y = 0;
	source.sampleX = 0;
	source.sampleY = 0;
	source.vtop_padding = 0;
	source.vbot_padding = 0;
	source.hleft_padding = 0;
	source.hright_padding = 0;
	conversion.size = sizeof(conversion);
	conversion.filterMode = 1;
	conversion.csc_params1 = &jpegEncoderRgbToYCbCr;
	conversion.csc_params2 = NULL;
	conversion.csc_control = 0;
	conversion.backgroundColor[0] = 0;
	conversion.backgroundColor[1] = 0;
	conversion.backgroundColor[2] = 0;
	conversion.alpha = 255;
	conversion.fieldMode = 0;
	destination.pixelformat = (control->pixelFormat & 9) == 9 ?
		SCE_IFTU_PIXELFORMAT_YUV422 : SCE_IFTU_PIXELFORMAT_YUV420;
	destination.width = width;
	destination.height = height;
	destination.leftover_stride = 0;
	destination.leftover_align = control->chromaPitchPadding;
	destination.paddr0 = outputPhysical;
	destination.paddr1 = lumaSize + outputPhysical;
	destination.paddr2 = chromaSize + lumaSize + outputPhysical;
	ret = SceIftuForDriver_67E37EFC(&destination, &source, &conversion);
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pYCbCr & ~255U),
		((unsigned int)pYCbCr + 255 - ((unsigned int)pYCbCr & ~255U) + outputSize) & ~255U);
unlockInput:
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pRGBA & ~255U),
		(inputSize + (unsigned int)pRGBA + 255 - ((unsigned int)pRGBA & ~255U)) & ~255U);
finish:
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_4DB66E3A(void)
{
	SceUInt32 syscallState = sceSyscallEnter();

	sceSyscallLeave(syscallState);
	return sizeof(JpegEncoderState);
}

int SceAvcodecForDriver_1BE9F5A4(SceJpegEncoderContext context, int iFrameWidth,
		       int iFrameHeight, int pixelFormat,
		       void *pJpeg, SceSize oJpegbufSize)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderState *state = context;
	JpegEncoderControl *control = &state->control;
	unsigned int format = (unsigned int)pixelFormat - 8;
	int ret;

	if (context == NULL || ((unsigned int)context & 3) != 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_POINTER;
		goto finish;
	}
	if (format > 17 || ((1U << format) & 0x30003) == 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_PIXELFORMAT;
		goto finish;
	}
	ret = ((pixelFormat & 9) == 9 ? 7 : 15) & iFrameHeight;
	if ((iFrameWidth & 15) != 0 || ret != 0 ||
	    (unsigned int)(iFrameWidth - 1) > 65534 ||
	    (unsigned int)(iFrameHeight - 1) > 65534) {
		ret = SCE_JPEGENC_ERROR_IMAGE_SIZE;
		goto finish;
	}
	memset(state, 0, sizeof(*state));
	control->pixelFormat = pixelFormat;
	control->iFrameWidth = iFrameWidth;
	control->validWidth = iFrameWidth;
	control->iFrameHeight = iFrameHeight;
	control->validHeight = iFrameHeight;
	control->compratio = 64;
	control->chromaPitchPadding = (pixelFormat & 16) != 0 ?
		(iFrameWidth >> 1) & 15 : 0;
	control->option = 0;
	if (pJpeg == NULL || oJpegbufSize <= 7 ||
	    (((unsigned int)pJpeg | oJpegbufSize) & 255) != 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_POINTER;
		goto finish;
	}
	control->pJpeg = pJpeg;
	control->pJpegEnd = (unsigned char *)pJpeg + oJpegbufSize;
finish:
	sceSyscallLeave(syscallState);
	return ret;
}

int jpegEncoderInitWithParam(SceJpegEncoderContext context,
			     const SceJpegEncoderInitParam *pInitParam)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderState *state = context;
	JpegEncoderControl *control = &state->control;
	unsigned int format;
	int ret;

	if (context == NULL || ((unsigned int)context & 3) != 0 || pInitParam == NULL)
		goto invalidPointer;
	if (pInitParam->size != sizeof(*pInitParam) ||
	    (unsigned int)pInitParam->option > 1) {
		ret = 0x80650207;
		goto finish;
	}
	format = (unsigned int)pInitParam->pixelFormat - 8;
	if (format > 17 || ((1U << format) & 0x30003) == 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_PIXELFORMAT;
		goto finish;
	}
	ret = ((pInitParam->pixelFormat & 9) == 9 ? 7 : 15) & pInitParam->iFrameHeight;
	if ((pInitParam->iFrameWidth & 15) != 0 || ret != 0 ||
	    (unsigned int)(pInitParam->iFrameWidth - 1) > 65534 ||
	    (unsigned int)(pInitParam->iFrameHeight - 1) > 65534) {
		ret = SCE_JPEGENC_ERROR_IMAGE_SIZE;
		goto finish;
	}
	memset(state, 0, sizeof(*state));
	control->compratio = 64;
	control->iFrameWidth = pInitParam->iFrameWidth;
	control->iFrameHeight = pInitParam->iFrameHeight;
	control->pixelFormat = pInitParam->pixelFormat;
	control->validWidth = pInitParam->iFrameWidth;
	control->validHeight = pInitParam->iFrameHeight;
	control->chromaPitchPadding = (pInitParam->pixelFormat & 16) != 0 ?
		(pInitParam->iFrameWidth >> 1) & 15 : 0;
	control->option = pInitParam->option;
	if (pInitParam->pJpeg == NULL || pInitParam->oJpegbufSize <= 7 ||
	    (((unsigned int)pInitParam->pJpeg | pInitParam->oJpegbufSize) & 255) != 0)
		goto invalidPointer;
	control->pJpeg = pInitParam->pJpeg;
	control->pJpegEnd = (unsigned char *)pInitParam->pJpeg + pInitParam->oJpegbufSize;
	goto finish;
invalidPointer:
	ret = SCE_JPEGENC_ERROR_INVALID_POINTER;
finish:
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_86AD5A4F(SceJpegEncoderContext context)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int ret;

	if (context == NULL || (ret = (unsigned int)context & 3) != 0) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	}
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_2C8271BA(SceJpegEncoderContext context, void *pJpeg,
				 SceSize oJpegbufSize)
{
	JpegEncoderControl *control = context;
	SceUInt32 syscallState = sceSyscallEnter();
	int ret;

	if (context == NULL || (ret = (unsigned int)context & 3) != 0 ||
	    pJpeg == NULL || oJpegbufSize <= 7 ||
	    (((unsigned int)pJpeg | oJpegbufSize) & 255) != 0) {
		ret = SCE_JPEGENC_ERROR_INVALID_POINTER;
	} else {
		control->pJpeg = pJpeg;
		control->pJpegEnd = (unsigned char *)pJpeg + oJpegbufSize;
	}
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_91B27CF3(SceJpegEncoderContext context, int compratio)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderControl *control = context;
	int ret;

	if (context == NULL || (ret = (unsigned int)context & 3) != 0) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	}
	if ((unsigned int)compratio > 255) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_INVALID_COMPRATIO;
	}
	control->compratio = compratio;
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_D0647B74(SceJpegEncoderContext context,
				int iFrameWidth, int iFrameHeight)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderControl *control = context;
	int ret;

	if (context == NULL || (ret = (unsigned int)context & 3) != 0) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	}
	if (iFrameWidth <= 0 || iFrameWidth > control->iFrameWidth ||
	    iFrameHeight <= 0 || iFrameHeight > control->iFrameHeight) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_IMAGE_SIZE;
	}
	control->validWidth = iFrameWidth;
	control->validHeight = iFrameHeight;
	sceSyscallLeave(syscallState);
	return ret;
}

int SceAvcodecForDriver_58835DCB(SceJpegEncoderContext context, int headerMode)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegEncoderControl *control = context;
	int ret;

	if (context == NULL || (ret = (unsigned int)context & 3) != 0) {
		sceSyscallLeave(syscallState);
		return SCE_JPEGENC_ERROR_INVALID_POINTER;
	}
	if ((unsigned int)headerMode <= 1)
		control->headerMode = headerMode;
	else
		ret = SCE_JPEGENC_ERROR_INVALID_HEADER_MODE;
	sceSyscallLeave(syscallState);
	return ret;
}
