/*
 * SceAvcodec skeleton: export stubs.
 *
 * Generated (T2). Every body is a placeholder.
 */

#include <skeleton.h>
#include "audiodec/audiodec_internal.h"
#include "audioenc/audioenc_internal.h"
#include "jpeg/jpeg_internal.h"
#include "jpegenc/jpegenc_internal.h"
#include <avcodec/avcodec.h>
#include <sysmem/sysmem.h>
#include <string.h>

STUB(SceAvcodecForDriver_01387A93)
STUB(SceAvcodecForDriver_0CC84413)
STUB(SceAvcodecForDriver_13C530E4)
STUB(SceAvcodecForDriver_16B501C9)
STUB(SceAvcodecForDriver_20892154)
STUB(SceAvcodecForDriver_20C83C00)
STUB(SceAvcodecForDriver_27211864)
STUB(SceAvcodecForDriver_3CB7661A)
STUB(SceAvcodecForDriver_427914D5)
STUB(SceAvcodecForDriver_43A9CD35)
STUB(SceAvcodecForDriver_48DECA56)
STUB(SceAvcodecForDriver_6E250A5F)
STUB(SceAvcodecForDriver_73401748)
STUB(SceAvcodecForDriver_78BA8E98)
STUB(SceAvcodecForDriver_866B49BF)
STUB(SceAvcodecForDriver_90F394E6)
STUB(SceAvcodecForDriver_A2085B6B)
STUB(SceAvcodecForDriver_B8A9632A)
STUB(SceAvcodecForDriver_BC244D21)
STUB(SceAvcodecForDriver_C114C499)
STUB(SceAvcodecForDriver_C518A109)
STUB(SceAvcodecForDriver_C8366BB8)
STUB(SceAvcodecForDriver_C9EF7057)
STUB(SceAvcodecForDriver_D2ACCE22)
STUB(SceAvcodecForDriver_D46B8463)
STUB(SceAvcodecForDriver_D5A34236)
STUB(SceAvcodecForDriver_D7CD7F82)
STUB(SceAvcodecForDriver_D9FCDF3E)
STUB(SceAvcodecForDriver_DFC0AAC6)
STUB(SceAvcodecForDriver_E355CAAE)
STUB(SceAvcodecForDriver_E4F668C5)
STUB(SceAvcodecForDriver_E7E14DE4)
STUB(SceAvcodecForDriver_EA07A3B7)
STUB(SceAvcodecForDriver_EAEB16D5)
STUB(SceAvcodecForDriver_F714DB9A)
STUB(SceAvcodecForDriver_F8BCBE20)
STUB(_sceVideodecQueryInstanceNongameapp)
STUB(_sceAvcdecCscInternal)

int _sceAudiodecInitLibrary(SceUInt32 codecType, SceAudiodecInitParam *param)
{
	return sceAudiodecInitLibraryForUser(codecType, param);
}

int _sceAudiodecTermLibrary(SceUInt32 codecType)
{
	return sceAudiodecTermLibraryForUser(codecType);
}

int _sceAudiodecCreateDecoder(SceAudiodecCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudiodecCreateDecoderForUser(ctrl, codecType);
}

int _sceAudiodecCreateDecoderResident(SceAudiodecCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudiodecCreateDecoderResidentForUser(ctrl, codecType);
}

int _sceAudiodecGetContextSize(const SceAudiodecCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudiodecGetContextSizeForUser(ctrl, codecType);
}

int _sceAudiodecCreateDecoderExternal(SceAudiodecCtrl *ctrl, SceUInt32 codecType,
				    SceUIntVAddr vaContext, SceUInt32 contextSize)
{
	return sceAudiodecCreateDecoderExternalForUser(ctrl, codecType, vaContext, contextSize);
}

int _sceAudiodecDeleteDecoder(SceAudiodecCtrl *ctrl)
{
	return sceAudiodecDeleteDecoderForUser(ctrl);
}

int _sceAudiodecDeleteDecoderResident(SceAudiodecCtrl *ctrl)
{
	return sceAudiodecDeleteDecoderResidentForUser(ctrl);
}

int _sceAudiodecDeleteDecoderExternal(SceAudiodecCtrl *ctrl, SceUIntVAddr *userContext)
{
	int ret, copied;
	SceUIntVAddr *context;

	if (!userContext)
		return sceAudiodecDeleteDecoderExternalForUser(ctrl, NULL);
	context = __builtin_alloca(sizeof(*context));
	*context = 0;
	ret = sceAudiodecDeleteDecoderExternalForUser(ctrl, context);
	copied = sceKernelCopyToUser(userContext, context, sizeof(*context));
	return copied < 0 ? copied : ret;
}

int _sceAudiodecDecode(SceAudiodecCtrl *ctrl)
{
	return sceAudiodecDecodeForUser(ctrl);
}

int _sceAudiodecDecodeNFrames(SceAudiodecCtrl *ctrl, SceUInt32 nFrames)
{
	return sceAudiodecDecodeNFramesForUser(ctrl, nFrames);
}

int _sceAudiodecDecodeNStreams(SceAudiodecCtrl *const *ctrls, SceUInt32 nStreams)
{
	return sceAudiodecDecodeNStreamsForUser(ctrls, nStreams);
}

int _sceAudiodecPartlyDecode(SceAudiodecCtrl *ctrl, SceUInt32 startSample,
			   SceUInt32 nSamples)
{
	return sceAudiodecPartlyDecodeForUser(ctrl, startSample, nSamples);
}

int _sceAudiodecClearContext(SceAudiodecCtrl *ctrl)
{
	return sceAudiodecClearContextForUser(ctrl);
}

int _sceAudiodecGetInternalError(SceAudiodecCtrl *ctrl, int *userError)
{
	int ret, copied;
	int *internalError;

	if (!userError)
		return sceAudiodecGetInternalErrorForUser(ctrl, NULL);
	internalError = __builtin_alloca(sizeof(*internalError));
	*internalError = 0;
	ret = sceAudiodecGetInternalErrorForUser(ctrl, internalError);
	copied = sceKernelCopyToUser(userError, internalError, sizeof(*internalError));
	return copied < 0 ? copied : ret;
}

int _sceAudioencInitLibrary(SceUInt32 codecType, SceAudioencInitParam *param)
{
	return sceAudioencInitLibraryForUser(codecType, param);
}

int _sceAudioencTermLibrary(SceUInt32 codecType)
{
	return sceAudioencTermLibraryForUser(codecType);
}

int _sceAudioencCreateEncoder(SceAudioencCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudioencCreateEncoderForUser(ctrl, codecType);
}

int _sceAudioencCreateEncoderResident(SceAudioencCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudioencCreateEncoderResidentForUser(ctrl, codecType);
}

int _sceAudioencGetContextSize(const SceAudioencCtrl *ctrl, SceUInt32 codecType)
{
	return sceAudioencGetContextSizeForUser(ctrl, codecType);
}

int _sceAudioencCreateEncoderExternal(SceAudioencCtrl *ctrl, SceUInt32 codecType,
				    SceUIntVAddr context, SceUInt32 contextSize)
{
	return sceAudioencCreateEncoderExternalForUser(ctrl, codecType, context, contextSize);
}

int _sceAudioencDeleteEncoder(SceAudioencCtrl *ctrl)
{
	return sceAudioencDeleteEncoderForUser(ctrl);
}

int _sceAudioencDeleteEncoderResident(SceAudioencCtrl *ctrl)
{
	return sceAudioencDeleteEncoderResidentForUser(ctrl);
}

int _sceAudioencDeleteEncoderExternal(SceAudioencCtrl *ctrl, SceUIntVAddr *userContext)
{
	int ret, copied;
	SceUIntVAddr *context;

	if (!userContext)
		return sceAudioencDeleteEncoderExternalForUser(ctrl, 0);
	context = __builtin_alloca(sizeof(*context));
	*context = 0;
	ret = sceAudioencDeleteEncoderExternalForUser(ctrl, (SceUIntVAddr)context);
	copied = sceKernelCopyToUser(userContext, context, sizeof(*context));
	return copied < 0 ? copied : ret;
}

int _sceAudioencEncode(SceAudioencCtrl *ctrl)
{
	return sceAudioencEncodeForUser(ctrl);
}

int _sceAudioencEncodeNFrames(SceAudioencCtrl *ctrl, SceUInt32 nFrames)
{
	return sceAudioencEncodeNFramesForUser(ctrl, nFrames);
}

int _sceAudioencClearContext(SceAudioencCtrl *ctrl)
{
	return sceAudioencClearContextForUser(ctrl);
}

int _sceAudioencGetOptInfo(SceAudioencCtrl *ctrl)
{
	return sceAudioencGetOptInfoForUser(ctrl);
}

int _sceAudioencGetInternalError(SceAudioencCtrl *ctrl, int *userError)
{
	int ret, copied;
	int *internalError;

	if (!userError)
		return sceAudioencGetInternalErrorForUser(ctrl, NULL);
	internalError = __builtin_alloca(sizeof(*internalError));
	*internalError = 0;
	ret = sceAudioencGetInternalErrorForUser(ctrl, internalError);
	copied = sceKernelCopyToUser(userError, internalError, sizeof(*internalError));
	return copied < 0 ? copied : ret;
}

int _sceJpegInitMJpeg(unsigned int maxSplitDecoder)
{
	return SceAvcodecForDriver_48247A20(maxSplitDecoder);
}

int _sceJpegFinishMJpeg(void)
{
	return SceAvcodecForDriver_04080710();
}

int _sceJpegDecodeMJpeg(const unsigned char *pJpeg, SceSize isize, void *pRGBA,
		      const JpegDecodeArguments *userArguments)
{
	JpegDecodeArguments arguments;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret >= 0)
		ret = SceAvcodecForDriver_80F63723(pJpeg, isize, pRGBA,
			arguments.osize, arguments.decodeMode, arguments.pTempBuffer,
			arguments.tempBufferSize, arguments.pCoefBuffer, arguments.coefBufferSize);
	return ret;
}

int _sceJpegDecodeMJpegYCbCr(const unsigned char *pJpeg, SceSize isize,
			   unsigned char *pYCbCr, const JpegYCbCrDecodeArguments *userArguments)
{
	JpegYCbCrDecodeArguments arguments;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret >= 0)
		ret = SceAvcodecForDriver_2A0AF392(pJpeg, isize, pYCbCr,
			arguments.osize, arguments.decodeMode, arguments.pCoefBuffer,
			arguments.coefBufferSize);
	return ret;
}

int _sceJpegMJpegCsc(void *pRGBA, const unsigned char *pYCbCr, int xysize,
		   const JpegCscArguments *userArguments)
{
	JpegCscArguments arguments;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret >= 0)
		ret = SceAvcodecForDriver_EAAE7737(pRGBA, pYCbCr, xysize,
			arguments.iFrameWidth, arguments.colorOption, arguments.sampling);
	return ret;
}

int _sceJpegGetOutputInfo(const unsigned char *pJpeg, SceSize isize, int outputFormat,
			const JpegOutputInfoArguments *userArguments)
{
	JpegOutputInfoArguments arguments;
	SceJpegOutputInfo *outputInfo;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (arguments.pOutputInfo == NULL)
		return SceAvcodecForDriver_F305E3B3(pJpeg, isize, outputFormat,
			arguments.decodeMode, NULL);
	outputInfo = __builtin_alloca(sizeof(*outputInfo));
	*outputInfo = (SceJpegOutputInfo){ 0 };
	ret = SceAvcodecForDriver_F305E3B3(pJpeg, isize, outputFormat,
		arguments.decodeMode, outputInfo);
	copied = sceKernelCopyToUser(arguments.pOutputInfo, outputInfo, sizeof(*outputInfo));
	return copied < 0 ? copied : ret;
}

int _sceJpegCreateSplitDecoder(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceJpegSplitDecodeCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_361238E2(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	memset(ctrl, 0, sizeof(*ctrl));
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_361238E2(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceJpegDeleteSplitDecoder(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceJpegSplitDecodeCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_864FC454(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	memset(ctrl, 0, sizeof(*ctrl));
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_864FC454(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceJpegSplitDecodeMJpeg(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceJpegSplitDecodeCtrl *ctrl;
	int ret, copied;

	if (pCtrl == NULL)
		return SceAvcodecForDriver_B6882013(NULL);
	ctrl = __builtin_alloca(sizeof(*ctrl));
	memset(ctrl, 0, sizeof(*ctrl));
	ret = sceKernelCopyFromUser(ctrl, pCtrl, sizeof(*ctrl));
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_B6882013(ctrl);
	copied = sceKernelCopyToUser(pCtrl, ctrl, sizeof(*ctrl));
	return copied < 0 ? copied : ret;
}

int _sceJpegInitMJpegWithParam(const SceJpegMJpegInitParam *pInitParam,
			     const JpegInitArguments *userArguments)
{
	JpegInitArguments arguments;
	SceJpegMJpegInitParam *initParam;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (pInitParam == NULL)
		return SceAvcodecForDriver_6F1CF77F(NULL);
	if (arguments.size > sizeof(*initParam))
		return 0x8002710b;
	initParam = __builtin_alloca(sizeof(*initParam));
	ret = sceKernelCopyFromUser(initParam, pInitParam, arguments.size);
	if (ret >= 0)
		ret = SceAvcodecForDriver_6F1CF77F(initParam);
	return ret;
}

int _sceJpegCsc(void *pRGBA, const unsigned char *pYCbCr, int xysize,
		const JpegCscArguments *userArguments)
{
	JpegCscArguments arguments;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret >= 0)
		ret = jpegCscForUser(pRGBA, pYCbCr, xysize,
			arguments.iFrameWidth, arguments.colorOption, arguments.sampling);
	return ret;
}

int _sceJpegEncoderGetContextSize(void)
{
	return SceAvcodecForDriver_4DB66E3A();
}

int _sceJpegEncoderInit(SceJpegEncoderContext context, int iFrameWidth,
		       int iFrameHeight, const JpegEncoderInitArguments *userArguments)
{
	JpegEncoderInitArguments arguments;
	JpegEncoderState *state;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_1BE9F5A4(NULL, iFrameWidth, iFrameHeight,
			arguments.pixelFormat, arguments.pJpeg, arguments.oJpegbufSize);
	if (arguments.contextSize > sizeof(*state))
		return 0x8002710b;
	state = __builtin_alloca(sizeof(*state));
	memset(state, 0, sizeof(*state));
	ret = SceAvcodecForDriver_1BE9F5A4(state, iFrameWidth, iFrameHeight,
		arguments.pixelFormat, arguments.pJpeg, arguments.oJpegbufSize);
	copied = sceKernelCopyToUser(context, state, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderEncode(SceJpegEncoderContext context, const void *pYCbCr,
			 const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderState *state;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_468949D7(NULL, pYCbCr);
	if (arguments.contextSize > sizeof(*state))
		return 0x8002710b;
	state = __builtin_alloca(sizeof(*state));
	memset(state, 0, sizeof(*state));
	ret = sceKernelCopyFromUser(state, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_468949D7(state, pYCbCr);
	copied = sceKernelCopyToUser(context, state, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderEnd(SceJpegEncoderContext context,
		      const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderState *state;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_86AD5A4F(NULL);
	if (arguments.contextSize > sizeof(*state))
		return 0x8002710b;
	state = __builtin_alloca(sizeof(*state));
	memset(state, 0, sizeof(*state));
	ret = sceKernelCopyFromUser(state, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_86AD5A4F(state);
	copied = sceKernelCopyToUser(context, state, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderSetValidRegion(SceJpegEncoderContext context,
				int iFrameWidth, int iFrameHeight,
				const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderControl *control;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_D0647B74(NULL, iFrameWidth, iFrameHeight);
	if (arguments.contextSize > sizeof(*control))
		return 0x8002710b;
	control = __builtin_alloca(sizeof(*control));
	*control = (JpegEncoderControl){ 0 };
	ret = sceKernelCopyFromUser(control, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_D0647B74(control, iFrameWidth, iFrameHeight);
	copied = sceKernelCopyToUser(context, control, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderSetCompressionRatio(SceJpegEncoderContext context, int compratio,
				      const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderControl *control;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_91B27CF3(NULL, compratio);
	if (arguments.contextSize > sizeof(*control))
		return 0x8002710b;
	control = __builtin_alloca(sizeof(*control));
	*control = (JpegEncoderControl){ 0 };
	ret = sceKernelCopyFromUser(control, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_91B27CF3(control, compratio);
	copied = sceKernelCopyToUser(context, control, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderSetHeaderMode(SceJpegEncoderContext context, int headerMode,
				const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderControl *control;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_58835DCB(NULL, headerMode);
	if (arguments.contextSize > sizeof(*control))
		return 0x8002710b;
	control = __builtin_alloca(sizeof(*control));
	*control = (JpegEncoderControl){ 0 };
	ret = sceKernelCopyFromUser(control, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_58835DCB(control, headerMode);
	copied = sceKernelCopyToUser(context, control, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderSetOutputAddr(SceJpegEncoderContext context, void *pJpeg,
				SceSize oJpegbufSize,
				const JpegEncoderContextArguments *userArguments)
{
	JpegEncoderContextArguments arguments;
	JpegEncoderControl *control;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context == NULL)
		return SceAvcodecForDriver_2C8271BA(NULL, pJpeg, oJpegbufSize);
	if (arguments.contextSize > sizeof(*control))
		return 0x8002710b;
	control = __builtin_alloca(sizeof(*control));
	*control = (JpegEncoderControl){ 0 };
	ret = sceKernelCopyFromUser(control, context, arguments.contextSize);
	if (ret < 0)
		return ret;
	ret = SceAvcodecForDriver_2C8271BA(control, pJpeg, oJpegbufSize);
	copied = sceKernelCopyToUser(context, control, arguments.contextSize);
	return copied < 0 ? copied : ret;
}

int _sceJpegEncoderCsc(SceJpegEncoderContext context, void *pYCbCr,
		      const void *pRGBA, const JpegEncoderCscArguments *userArguments)
{
	JpegEncoderCscArguments arguments;
	JpegEncoderControl *control = context;
	int ret;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context != NULL) {
		if (arguments.contextSize > sizeof(*control))
			return 0x8002710b;
		control = __builtin_alloca(sizeof(*control));
		ret = sceKernelCopyFromUser(control, context, arguments.contextSize);
		if (ret < 0)
			return ret;
	}
	return SceAvcodecForDriver_B77B5BFC(control, pYCbCr, pRGBA,
		arguments.iFrameWidth, arguments.inputPixelFormat);
}

int _sceJpegEncoderInitWithParam(SceJpegEncoderContext context,
			       const SceJpegEncoderInitParam *pInitParam,
			       const JpegEncoderInitParamArguments *userArguments)
{
	JpegEncoderInitParamArguments arguments;
	JpegEncoderState *state = NULL;
	SceJpegEncoderInitParam *initParam = (SceJpegEncoderInitParam *)pInitParam;
	int ret, copied;

	ret = sceKernelCopyFromUser(&arguments, userArguments, sizeof(arguments));
	if (ret < 0)
		return ret;
	if (context != NULL) {
		if (arguments.contextSize > sizeof(*state))
			return 0x8002710b;
		state = __builtin_alloca(sizeof(*state));
		memset(state, 0, sizeof(*state));
	}
	if (pInitParam != NULL) {
		if (arguments.initParamSize > sizeof(*initParam))
			return 0x8002710b;
		initParam = __builtin_alloca(sizeof(*initParam));
		ret = sceKernelCopyFromUser(initParam, pInitParam, arguments.initParamSize);
		if (ret < 0)
			return ret;
	}
	ret = jpegEncoderInitWithParam(state, initParam);
	if (context != NULL) {
		copied = sceKernelCopyToUser(context, state, arguments.contextSize);
		if (copied < 0)
			return copied;
	}
	return ret;
}
