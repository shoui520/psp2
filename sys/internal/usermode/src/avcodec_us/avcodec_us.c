/*
 * SceAvcodecUser: the user side of the codec services (audio decoder / encoder,
 * AVC / MPEG-4 video, JPEG, codec engine). Every export calls the kernel's syscall of
 * the same name with an underscore; syscalls take four words, so the JPEG calls with
 * more arguments pass the rest as a block on the stack.
 */

#include <scetypes.h>
#include "avcodec.h"

/* the syscalls (the kernel side declares their real prototypes) */
#define WRAP(_name, _syscall)	int _syscall(int a0, int a1, int a2, int a3);
#include "wrappers.h"
#undef WRAP


/* passed along with a JPEG encoder context: 0 for none (meaning unknown) */
#define JPEGENC_INIT_WORD(_ctx)		((_ctx) ? 0x180 : 0)	/* init / encode / end */
#define JPEGENC_SET_WORD(_ctx)		((_ctx) ? 0x28 : 0)	/* the setters, csc */

/* plain wrappers: export -> syscall */
#define WRAP(_name, _syscall)						\
	int _name(int a0, int a1, int a2, int a3)			\
	{								\
		return _syscall(a0, a1, a2, a3);			\
	}
#include "wrappers.h"
#undef WRAP

/* -- MJPEG decoder ------------------------------------------------------- */

int sceJpegDecodeMJpeg(int a0, int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8)
{
	int args[6];

	args[0] = a3;
	args[1] = a4;
	args[2] = a5;
	args[3] = a6;
	args[4] = a7;
	args[5] = a8;
	return _sceJpegDecodeMJpeg(a0, a1, a2, args);
}

int sceJpegDecodeMJpegYCbCr(int a0, int a1, int a2, int a3, int a4, int a5, int a6)
{
	int args[4];

	args[0] = a3;
	args[1] = a4;
	args[2] = a5;
	args[3] = a6;
	return _sceJpegDecodeMJpegYCbCr(a0, a1, a2, args);
}

int sceJpegMJpegCsc(int a0, int a1, int a2, int a3, int a4, int a5)
{
	int args[3];

	args[0] = a3;
	args[1] = a4;
	args[2] = a5;
	return _sceJpegMJpegCsc(a0, a1, a2, args);
}

int sceJpegGetOutputInfo(int a0, int a1, int a2, int a3, int a4)
{
	int args[2];

	args[0] = a3;
	args[1] = a4;
	return _sceJpegGetOutputInfo(a0, a1, a2, args);
}

/* param: SceJpegMJpegInitParam, its size first */
int sceJpegInitMJpegWithParam(const SceSize *param)
{
	SceSize size;

	size = param ? *param : 0;
	return _sceJpegInitMJpegWithParam(param, &size);
}

int sceJpegCsc(int a0, int a1, int a2, int a3, int a4, int a5)
{
	int args[3];

	args[0] = a3;
	args[1] = a4;
	args[2] = a5;
	return _sceJpegCsc(a0, a1, a2, args);
}

/* -- JPEG encoder -------------------------------------------------------- */

int sceJpegEncoderInit(int context, int inWidth, int inHeight, int pixelFormat, int outBuffer, int outSize)
{
	int args[4];

	args[0] = pixelFormat;
	args[1] = outBuffer;
	args[2] = outSize;
	args[3] = JPEGENC_INIT_WORD(context);
	return _sceJpegEncoderInit(context, inWidth, inHeight, args);
}

int sceJpegEncoderEncode(int context, int inBuffer)
{
	int word = JPEGENC_INIT_WORD(context);

	return _sceJpegEncoderEncode(context, inBuffer, &word);
}

int sceJpegEncoderEnd(int context)
{
	int word = JPEGENC_INIT_WORD(context);

	return _sceJpegEncoderEnd(context, &word);
}

int sceJpegEncoderSetValidRegion(int context, int inWidth, int inHeight)
{
	int word = JPEGENC_SET_WORD(context);

	return _sceJpegEncoderSetValidRegion(context, inWidth, inHeight, &word);
}

int sceJpegEncoderSetCompressionRatio(int context, int ratio)
{
	int word = JPEGENC_SET_WORD(context);

	return _sceJpegEncoderSetCompressionRatio(context, ratio, &word);
}

int sceJpegEncoderSetHeaderMode(int context, int mode)
{
	int word = JPEGENC_SET_WORD(context);

	return _sceJpegEncoderSetHeaderMode(context, mode, &word);
}

int sceJpegEncoderSetOutputAddr(int context, int outBuffer, int outSize)
{
	int word = JPEGENC_SET_WORD(context);

	return _sceJpegEncoderSetOutputAddr(context, outBuffer, outSize, &word);
}

int sceJpegEncoderCsc(int context, int outBuffer, int inBuffer, int inPitch, int inPixelFormat)
{
	int args[3];

	args[0] = inPitch;
	args[1] = inPixelFormat;
	args[2] = JPEGENC_SET_WORD(context);
	return _sceJpegEncoderCsc(context, outBuffer, inBuffer, args);
}

/* initParam: SceJpegEncoderInitParam, its size first */
int sceJpegEncoderInitWithParam(int context, const SceSize *initParam)
{
	int args[2];

	args[0] = JPEGENC_INIT_WORD(context);
	args[1] = initParam ? *initParam : 0;
	return _sceJpegEncoderInitWithParam(context, initParam, args);
}
