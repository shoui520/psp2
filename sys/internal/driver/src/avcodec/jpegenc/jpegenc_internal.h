#pragma once

#include <scetypes.h>
#include <scejpegenc.h>

/* The syscall setters copy this complete 40-byte host control record.
 * Opaque arrays are live coprocessor data, not unused padding. */
typedef struct JpegEncoderControl {
	unsigned char *pJpeg;
	unsigned char *pJpegEnd;
	unsigned char opaqueCompressionData[5];
	unsigned char compratio;
	unsigned char opaqueRegionData[6];
	unsigned short validWidth;
	unsigned short validHeight;
	unsigned short iFrameWidth;
	unsigned short iFrameHeight;
	unsigned char pixelFormat;
	unsigned char headerMode;
	unsigned char opaquePitchData;
	unsigned char chromaPitchPadding;
	unsigned char opaqueOptionData[4];
	unsigned int option;
} JpegEncoderControl;

/* Initialization clears all 384 bytes; RPC453 transfers all of them.
 * The host preserves the opaque data across encode calls and user copies. */
typedef struct JpegEncoderState {
	JpegEncoderControl control;
	unsigned char opaqueCoprocessorData[344];
} JpegEncoderState;

_Static_assert(sizeof(JpegEncoderControl) == 40, "JPEG encoder control ABI");
_Static_assert(sizeof(JpegEncoderState) == 384, "JPEG encoder state ABI");

/* Complete syscall packet; its final two words are copied but not read. */
typedef struct JpegEncoderInitArguments {
	int pixelFormat;
	void *pJpeg;
	SceSize oJpegbufSize;
	SceSize contextSize;
	unsigned int reserved[2];
} JpegEncoderInitArguments;

_Static_assert(sizeof(JpegEncoderInitArguments) == 24, "JPEG encoder init arguments ABI");

typedef struct JpegEncoderContextArguments {
	SceSize contextSize;
	unsigned int reserved;
} JpegEncoderContextArguments;

_Static_assert(sizeof(JpegEncoderContextArguments) == 8, "JPEG encoder context arguments ABI");

typedef struct JpegEncoderCscArguments {
	int iFrameWidth;
	int inputPixelFormat;
	SceSize contextSize;
	unsigned int reserved;
} JpegEncoderCscArguments;

typedef struct JpegEncoderInitParamArguments {
	SceSize contextSize;
	SceSize initParamSize;
	unsigned int reserved[2];
} JpegEncoderInitParamArguments;

_Static_assert(sizeof(JpegEncoderCscArguments) == 16, "JPEG encoder CSC arguments ABI");
_Static_assert(sizeof(JpegEncoderInitParamArguments) == 16, "JPEG encoder init-param arguments ABI");

int jpegEncoderInitWithParam(void *context, const SceJpegEncoderInitParam *pInitParam);
