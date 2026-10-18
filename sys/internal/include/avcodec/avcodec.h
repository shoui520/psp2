#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <scejpegenc.h>

int SceAvcodecForDriver_04080710(void);
int SceAvcodecForDriver_2641888C(unsigned int codecType);
int SceAvcodecForDriver_2A0AF392(const unsigned char *pJpeg, SceSize isize, unsigned char *pYCbCr, SceSize osize, int decodeMode, void *pCoefBuffer, SceSize coefBufferSize);
int SceAvcodecForDriver_48247A20(unsigned int maxSplitDecoder);
int SceAvcodecForDriver_4DB66E3A(void);	/* vitasdk: ksceJpegEncoderGetContextSize */
int SceAvcodecForDriver_58F1E353(unsigned int codecType);
int SceAvcodecForDriver_80F63723(const unsigned char *pJpeg, SceSize isize, void *pRGBA, SceSize osize, int decodeMode, void *pTempBuffer, SceSize tempBufferSize, void *pCoefBuffer, SceSize coefBufferSize);
int SceAvcodecForDriver_C84AFFC4(void (*callback)(void));
int SceAvcodecForDriver_EAAE7737(void *pRGBA, const unsigned char *pYCbCr, int xysize, int iFrameWidth, int colorOption, int sampling);

int _sceAudiodecTermLibrary(SceUInt32 codecType);
int _sceAudioencTermLibrary(SceUInt32 codecType);
int _sceAvcdecDecodeSetUserDataSei1FieldMemSizeNongameapp(unsigned int codecType, unsigned int size);
int _sceAvcdecSetDecodeMode(unsigned int codecType, unsigned int mode);
int _sceAvcdecSetDecodeModeInternal(unsigned int codecType, unsigned int mode);
int _sceAvcdecSetInterlacedStreamMode(unsigned int codecType, unsigned int flag);
int _sceAvcdecSetLowDelayModeNongameapp(unsigned int codecType, unsigned int mode);
int _sceAvcdecSetRecoveryPointSEIMode(unsigned int codecType, unsigned int mode);
int _sceAvcencSetAvailablePreset(unsigned int codecType, unsigned int preset);
int _sceJpegEncoderGetContextSize(void);
int _sceJpegFinishMJpeg(void);
int _sceJpegInitMJpeg(unsigned int maxSplitDecoder);
int _sceVideodecSetConfig(unsigned int codecType, unsigned int flags);
int _sceVideodecSetConfigInternal(unsigned int codecType, unsigned int flags);
int _sceVideodecTermLibrary(unsigned int codecType);
int _sceVideoencTermLibrary(unsigned int codecType);

/* vitasdk: ksceJpegEncoderInit */
int SceAvcodecForDriver_1BE9F5A4(SceJpegEncoderContext context, int iFrameWidth, int iFrameHeight, int pixelFormat, void *pJpeg, SceSize oJpegbufSize);
/* vitasdk: ksceJpegEncoderSetOutputAddr */
int SceAvcodecForDriver_2C8271BA(SceJpegEncoderContext context, void *pJpeg, SceSize oJpegbufSize);
/* vitasdk: ksceJpegEncoderEncode */
int SceAvcodecForDriver_468949D7(SceJpegEncoderContext context, const void *pYCbCr);
/* vitasdk: ksceJpegEncoderSetHeaderMode */
int SceAvcodecForDriver_58835DCB(SceJpegEncoderContext context, int headerMode);
/* vitasdk: ksceJpegEncoderEnd */
int SceAvcodecForDriver_86AD5A4F(SceJpegEncoderContext context);
/* vitasdk: ksceJpegEncoderSetCompressionRatio */
int SceAvcodecForDriver_91B27CF3(SceJpegEncoderContext context, int compratio);
/* vitasdk: ksceJpegEncoderCsc */
int SceAvcodecForDriver_B77B5BFC(SceJpegEncoderContext context, void *pYCbCr, const void *pRGBA, int iFrameWidth, int inputPixelFormat);
/* vitasdk: ksceJpegEncoderSetValidRegion */
int SceAvcodecForDriver_D0647B74(SceJpegEncoderContext context, int iFrameWidth, int iFrameHeight);
