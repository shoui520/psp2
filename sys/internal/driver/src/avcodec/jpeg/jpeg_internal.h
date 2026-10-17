#pragma once

#include <vnz_wrapper/venezia_rpc.h>
#include "../jpeg.h"
#include <threadmgr/fast_mutex_object.h>

/* Complete host-side output-information query transferred through RPC. */
typedef struct JpegQueryCommand {
	SceUInt32 reservedControl;
	const unsigned char *pJpeg;
	SceSize isize;
	unsigned int samplingFlags;
	unsigned int downscale;
	int outputFormat;
	struct SceJpegOutputInfo *pOutputInfo;
	SceUInt32 reservedTail[4];
} JpegQueryCommand;

_Static_assert(sizeof(JpegQueryCommand) == 44, "JPEG query command ABI");

typedef struct JpegResident {
	struct ThreadmgrFastMutex mutex;
	unsigned int channel;
	int processStorageKey;
} JpegResident;

_Static_assert(sizeof(JpegResident) == 72, "JPEG resident ABI");

typedef struct JpegProcess {
	int state;
	unsigned int maxSplitDecoder;
	unsigned int allocatedDecoders;
	unsigned int busyDecoders;
	int option;
	void *pWorkBuffer;
	unsigned int decoderContexts[8];
} JpegProcess;

_Static_assert(sizeof(JpegProcess) == 56, "JPEG process ABI");

/* RPC448 ordinary decode packet, distinct from the RPC449 query. */
typedef struct JpegDecodeCommand {
	SceUInt32 reservedControl;
	const unsigned char *pJpeg;
	SceSize isize;
	unsigned char *pYCbCr;
	SceSize osize;
	unsigned int samplingFlags;
	unsigned int downscale;
	unsigned int colorSpace;
	void *pCoefBuffer;
	SceSize coefBufferSize;
	SceUInt32 reservedTail;
} JpegDecodeCommand;

_Static_assert(sizeof(JpegDecodeCommand) == 44, "JPEG decode command ABI");

/* RPC454 converts packed YCbCr444 input without decoding a JPEG stream. */
typedef struct JpegCscCommand {
	SceUInt32 reservedControl;
	const unsigned char *pYCbCr;
	SceSize isize;
	void *pRGBA;
	SceSize osize;
	unsigned int colorFormat;
	int xysize;
	int iFrameWidth;
	unsigned int conversionMode;
	SceUInt32 reservedTail[2];
} JpegCscCommand;

_Static_assert(sizeof(JpegCscCommand) == 44, "JPEG color conversion command ABI");

int SceAvcodecForDriver_48247A20(unsigned int maxSplitDecoder);
int SceAvcodecForDriver_04080710(void);
int SceAvcodecForDriver_80F63723(const unsigned char *pJpeg, SceSize isize,
			      void *pRGBA, SceSize osize, int decodeMode,
			      void *pTempBuffer, SceSize tempBufferSize,
			      void *pCoefBuffer, SceSize coefBufferSize);

/* Complete trailing-argument packet copied by the public syscall wrapper. */
typedef struct JpegDecodeArguments {
	SceSize osize;
	int decodeMode;
	void *pTempBuffer;
	SceSize tempBufferSize;
	void *pCoefBuffer;
	SceSize coefBufferSize;
	SceUInt32 reserved[2];
} JpegDecodeArguments;

_Static_assert(sizeof(JpegDecodeArguments) == 32, "JPEG syscall decode arguments ABI");

typedef struct JpegYCbCrDecodeArguments {
	SceSize osize;
	int decodeMode;
	void *pCoefBuffer;
	SceSize coefBufferSize;
	SceUInt32 reserved[2];
} JpegYCbCrDecodeArguments;

_Static_assert(sizeof(JpegYCbCrDecodeArguments) == 24, "JPEG syscall YCbCr arguments ABI");

int SceAvcodecForDriver_2A0AF392(const unsigned char *pJpeg, SceSize isize,
			      unsigned char *pYCbCr, SceSize osize, int decodeMode,
			      void *pCoefBuffer, SceSize coefBufferSize);

typedef struct JpegCscArguments {
	int iFrameWidth;
	int colorOption;
	int sampling;
	SceUInt32 reserved;
} JpegCscArguments;

_Static_assert(sizeof(JpegCscArguments) == 16, "JPEG syscall CSC arguments ABI");

int SceAvcodecForDriver_EAAE7737(void *pRGBA, const unsigned char *pYCbCr,
			      int xysize, int iFrameWidth, int colorOption, int sampling);

typedef struct JpegOutputInfoArguments {
	int decodeMode;
	SceJpegOutputInfo *pOutputInfo;
	SceUInt32 reserved[2];
} JpegOutputInfoArguments;

_Static_assert(sizeof(JpegOutputInfoArguments) == 16, "JPEG syscall query arguments ABI");

int SceAvcodecForDriver_F305E3B3(const unsigned char *pJpeg, SceSize isize,
			      int outputFormat, int decodeMode, SceJpegOutputInfo *pOutputInfo);
int SceAvcodecForDriver_361238E2(SceJpegSplitDecodeCtrl *pCtrl);
int SceAvcodecForDriver_864FC454(SceJpegSplitDecodeCtrl *pCtrl);
int SceAvcodecForDriver_B6882013(SceJpegSplitDecodeCtrl *pCtrl);
int SceAvcodecForDriver_6F1CF77F(const SceJpegMJpegInitParam *pInitParam);
int jpegCscForUser(void *pRGBA, const unsigned char *pYCbCr, int xysize,
		   int iFrameWidth, int colorOption, int sampling);

typedef struct JpegInitArguments {
	SceSize size;
	SceUInt32 reserved;
} JpegInitArguments;

_Static_assert(sizeof(JpegInitArguments) == 8, "JPEG syscall init arguments ABI");
