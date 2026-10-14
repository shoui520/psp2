/*
 * The JPEG RPCs (448-454): each maps the ARM's parameter block, makes a
 * VThread context on the caller's resource and runs the codec there.
 */
#include "jpeg.h"

#define CONTEXT_JOBS			2
#define ERROR_MEMORY			(-10001)
#define DONE_FLAG			0x80000000

/* the split decoder's states (SceJpegSplitDecodeCtrl.internalData[0]) */
#define SPLIT_HEADER			0	/* nothing read */
#define SPLIT_HEADER_SUSPENDED		1	/* the header waits for more input */
#define SPLIT_READY			2	/* the header read, output info given */
#define SPLIT_DECODE_SUSPENDED		3	/* the image waits for more input */
#define SPLIT_DONE			4	/* finished, or failed */

/* sceJpegDecodeMJpegYCbCr's parameters */
typedef struct JpegDecodeParam {
	int32_t result;
	uint32_t pJpeg;
	uint32_t isize;
	uint32_t pYCbCr;
	uint32_t osize;
	uint32_t flags;
	uint32_t downscale;
	int32_t colorSpace;
	uint32_t pCoefBuffer;
	uint32_t coefBufferSize;
	uint32_t reserved;
} JpegDecodeParam;

/* sceJpegGetOutputInfo's parameters */
typedef struct JpegOutputInfoParam {
	int32_t result;
	uint32_t pJpeg;
	uint32_t isize;
	uint32_t flags;
	uint32_t downscale;
	int32_t outputFormat;
	uint32_t pOutputInfo;
	uint32_t reserved[4];
} JpegOutputInfoParam;

/* SceJpegSplitDecodeCtrl */
typedef struct SceJpegSplitDecodeCtrl {
	uint32_t pStreamBuffer;
	uint32_t streamBufferSize;
	uint32_t pWriteBuffer;		/* where the ARM writes more input */
	uint32_t writeBufferSize;
	uint32_t isEndOfStream;
	uint32_t decodeMode;
	SceJpegOutputInfo outputInfo;
	uint32_t pOutputBuffer;
	uint32_t pCoefBuffer;
	uint32_t state;			/* internalData[0..2] */
	VkVThreadContext *context;
	int32_t result;
} SceJpegSplitDecodeCtrl;

/* sceJpegMJpegCsc's parameters, the work shared by up to two VThreads */
typedef struct JpegCscParam {
	int32_t result;
	uint32_t pYCbCr;
	uint32_t ycbcrSize;
	uint32_t pRGBA;
	uint32_t rgbaSize;
	int32_t colorOption;
	uint32_t xysize;		/* width << 16 | height */
	uint32_t iFrameWidth;
	uint32_t threads;
	uint32_t reserved;
	VkThread *caller;		/* each VThread sets its flag here */
} JpegCscParam;

/* the parameter block mapped, a context made, the job run on it */
static int32_t jpegCall(VkVThreadResource *resource, VkJobFunction job, uint32_t paramAddress)
{
	VkVThreadContext *context = NULL;
	VnzMemory paramMemory;
	int32_t *param, result;

	vnzMemoryCreate(&paramMemory, paramAddress, sizeof(JpegDecodeParam));
	if (vnzMemoryOpenPublic(&paramMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	param = (int32_t *)paramMemory.vaddrVnz;
	context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (context == NULL) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = vnzVThreadExecute(context, 0, job, (uint32_t)param, 0, 0, 0);
	if (result == 0)
		result = *param;
close:
	if (context)
		vkVThreadContextDelete(context);
	if (paramMemory.vaddrVnz && vnzMemoryClosePublic(&paramMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	return result;
}

/* a job set on the context once its slot is free */
static int32_t jpegJobSet(VkVThreadContext *context, uint32_t index, VkJobFunction job,
	uint32_t argument, uint32_t argument2, uint32_t argument3, uint32_t argument4)
{
	VkVThreadContext *locked;
	int32_t result;

	while (vkVThreadJobState(context, index))
		vnzThreadYield();
	locked = vkVThreadContextLock(context);
	result = vkVThreadJobSet(locked, index, 0, job, argument, argument2, argument3,
		argument4);
	return result | vkVThreadContextUnlock(locked);
}

/* the buffer sizes rounded up to 256 bytes */
static void outputInfoAlign(SceJpegOutputInfo *info)
{
	info->outputBufferSize = (info->outputBufferSize + 255) & ~255;
	info->tempBufferSize = (info->tempBufferSize + 255) & ~255;
	info->coefBufferSize = (info->coefBufferSize + 255) & ~255;
}

/*
 * The stream given must be the one the decoder started in, and hold all it
 * read (exactly that, while it waits for input).
 */
static int32_t splitStreamCheck(SceJpegSplitDecodeCtrl *ctrl, void *work, uint32_t stream,
	uint32_t size)
{
	JpegDecoder *decoder = JPEG_DECODER(work);
	uint32_t used = decoder->streamEnd - decoder->streamBase;

	if (stream != (uint32_t)decoder->streamBase)
		return SCE_JPEG_ERROR_INVALID_POINTER;
	if (decoder->inputState >= 8) {
		if (size != used)
			return SCE_JPEG_ERROR_INVALID_POINTER;
	} else if (size < used) {
		return SCE_JPEG_ERROR_INVALID_POINTER;
	}
	return 0;
}

/* sceJpegDecodeMJpegYCbCr, on the VThread */
static void decodeJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	JpegDecodeParam *param = (JpegDecodeParam *)argument;
	VnzMemory jpegMemory, outputMemory, coefMemory;
	int32_t result;

	vnzMemoryCreate(&jpegMemory, param->pJpeg, param->isize);
	vnzMemoryCreate(&outputMemory, param->pYCbCr, param->osize);
	vnzMemoryCreate(&coefMemory, param->pCoefBuffer, param->coefBufferSize);
	if (vnzMemoryOpenProtected(&jpegMemory) == 0 || vnzMemoryOpenPrivate(&outputMemory) == 0 ||
		(coefMemory.paddr && coefMemory.size && vnzMemoryOpenPrivate(&coefMemory) == 0)) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = jpegDecodeMJpegYCbCr((const uint8_t *)jpegMemory.vaddrVnz, jpegMemory.size,
		(uint8_t *)outputMemory.vaddrVnz, outputMemory.size, param->flags, param->downscale,
		&param->colorSpace, (void *)coefMemory.vaddrVnz, coefMemory.size);
close:
	if (coefMemory.vaddrVnz && vnzMemoryClosePrivate(&coefMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (outputMemory.vaddrVnz && vnzMemoryClosePrivate(&outputMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (jpegMemory.vaddrVnz && vnzMemoryCloseProtected(&jpegMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	param->result = result;
}

/* sceJpegGetOutputInfo, on the VThread */
static void outputInfoJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	JpegOutputInfoParam *param = (JpegOutputInfoParam *)argument;
	VnzMemory jpegMemory, infoMemory;
	SceJpegOutputInfo *info;
	int32_t result;

	vnzMemoryCreate(&jpegMemory, param->pJpeg, param->isize);
	vnzMemoryCreate(&infoMemory, param->pOutputInfo, sizeof(SceJpegOutputInfo));
	if (vnzMemoryOpenProtected(&jpegMemory) == 0 || vnzMemoryOpenPrivate(&infoMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	info = (SceJpegOutputInfo *)infoMemory.vaddrVnz;
	result = jpegGetOutputInfo((const uint8_t *)jpegMemory.vaddrVnz, jpegMemory.size,
		param->flags, param->downscale, param->outputFormat, info);
	if (result == 0)
		outputInfoAlign(info);
close:
	if (infoMemory.vaddrVnz && vnzMemoryClosePrivate(&infoMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (jpegMemory.vaddrVnz && vnzMemoryCloseProtected(&jpegMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	param->result = result;
}

/* RPC 448 */
int32_t rpcJpegDecodeMJpegYCbCr(VkVThreadResource *resource, uint32_t paramAddress)
{
	return jpegCall(resource, decodeJob, paramAddress);
}

/* RPC 449 */
int32_t rpcJpegGetOutputInfo(VkVThreadResource *resource, uint32_t paramAddress)
{
	return jpegCall(resource, outputInfoJob, paramAddress);
}

/* RPC 450: sceJpegCreateSplitDecoder */
int32_t rpcJpegCreateSplitDecoder(VkVThreadResource *resource, uint32_t ctrlAddress)
{
	SceJpegSplitDecodeCtrl *ctrl;
	VkVThreadContext *context;
	VnzMemory ctrlMemory;
	int32_t result;

	vnzMemoryCreate(&ctrlMemory, ctrlAddress, sizeof(SceJpegSplitDecodeCtrl));
	if (vnzMemoryOpenPublic(&ctrlMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	ctrl = (SceJpegSplitDecodeCtrl *)ctrlMemory.vaddrVnz;
	context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (context == NULL) {
		result = ERROR_MEMORY;
		goto close;
	}
	memset(ctrl, 0, offsetof(SceJpegSplitDecodeCtrl, state));
	ctrl->context = context;
	ctrl->state = SPLIT_HEADER;
	result = 0;
close:
	if (ctrlMemory.vaddrVnz && vnzMemoryClosePublic(&ctrlMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	return result;
}

/* RPC 451: sceJpegDeleteSplitDecoder */
int32_t rpcJpegDeleteSplitDecoder(VkVThreadResource *resource, uint32_t ctrlAddress)
{
	SceJpegSplitDecodeCtrl *ctrl;
	VnzMemory ctrlMemory;
	int32_t result;

	vnzMemoryCreate(&ctrlMemory, ctrlAddress, sizeof(SceJpegSplitDecodeCtrl));
	if (vnzMemoryOpenPublic(&ctrlMemory) == 0) {
		result = ERROR_MEMORY;
	} else {
		ctrl = (SceJpegSplitDecodeCtrl *)ctrlMemory.vaddrVnz;
		vkVThreadContextDelete(ctrl->context);
		ctrl->state = SPLIT_HEADER;
		result = 0;
		ctrl->context = NULL;
	}
	if (ctrlMemory.vaddrVnz && vnzMemoryClosePublic(&ctrlMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	return result;
}

/* the header: the decoder started, or continued with more input */
static void splitHeaderJob(uint32_t argument, uint32_t workAddress, uint32_t argument3,
	uint32_t argument4)
{
	SceJpegSplitDecodeCtrl *ctrl = (SceJpegSplitDecodeCtrl *)argument;
	uint32_t state = ctrl->state, consumed = 0, coefSize, position, flags, downscale;
	int32_t result = 0, error = 0;
	VnzMemory streamMemory;
	uint32_t mode;
	void *work;

	vnzMemoryCreate(&streamMemory, ctrl->pStreamBuffer, ctrl->streamBufferSize);
	work = (void *)openPrivateMemory(workAddress, JPEG_DECODER_WORK_SIZE);
	if (work == NULL || vnzMemoryOpenPrivate(&streamMemory) == 0) {
		error = ERROR_MEMORY;
		goto close;
	}
	if (state == SPLIT_HEADER) {
		mode = ctrl->decodeMode & 3;
		flags = mode < SCE_JPEG_MJPEG_ANY_SAMPLING_WITHOUT_DHT;
		if (mode - 1 < 2)
			flags |= 2;
		downscale = (ctrl->decodeMode & SCE_JPEG_MJPEG_DOWNSCALE_MASK) >> 3;
		if (downscale == 0)
			downscale = 1;
		result = jpegDecoderInit(work, (const uint8_t *)streamMemory.vaddrVnz,
			streamMemory.size, downscale, flags, ctrl->isEndOfStream != 1, &coefSize);
	} else {
		error = splitStreamCheck(ctrl, work, streamMemory.vaddrVnz, streamMemory.size);
		if (error < 0)
			goto failed;
		error = jpegDecoderSetInput(work, ctrl->writeBufferSize, ctrl->isEndOfStream);
		if (error < 0)
			goto failed;
		result = jpegDecoderContinue(work, &coefSize);
	}

	if (result >= 0) {
		jpegDecoderOutputInfo(work, -1, result, coefSize, &ctrl->outputInfo);
		outputInfoAlign(&ctrl->outputInfo);
		state = SPLIT_READY;
		ctrl->writeBufferSize = 0;
		result = SCE_JPEG_ERROR_INIT_DONE;
	} else if (result == SCE_JPEG_ERROR_INPUT_SUSPENDED) {
		error = jpegDecoderInputPosition(work, &position, &ctrl->writeBufferSize);
		if (error < 0)
			goto failed;
		state = SPLIT_HEADER_SUSPENDED;
		consumed = position - streamMemory.vaddrVnz;
	} else {
		ctrl->outputInfo.colorSpace = JPEG_DECODER(work)->colorSpace;
		goto failed;
	}
	goto close;
failed:
	state = SPLIT_DONE;
close:
	if (streamMemory.vaddrVnz && vnzMemoryClosePrivate(&streamMemory) == 0 && error >= 0)
		error = ERROR_MEMORY;
	if (work && closePrivateMemory((uint32_t)work, JPEG_DECODER_WORK_SIZE) == 0 && error >= 0)
		error = ERROR_MEMORY;
	ctrl->state = state;
	ctrl->pWriteBuffer = ctrl->pStreamBuffer + consumed;
	ctrl->result = error ? error : result;
}

/* the image: started, or continued with more input */
static void splitDecodeJob(uint32_t argument, uint32_t workAddress, uint32_t argument3,
	uint32_t argument4)
{
	SceJpegSplitDecodeCtrl *ctrl = (SceJpegSplitDecodeCtrl *)argument;
	VnzMemory streamMemory, outputMemory, coefMemory;
	uint32_t state = ctrl->state, consumed = 0, position;
	int32_t result, step = 0;
	void *work;

	vnzMemoryCreate(&streamMemory, ctrl->pStreamBuffer, ctrl->streamBufferSize);
	vnzMemoryCreate(&outputMemory, ctrl->pOutputBuffer, ctrl->outputInfo.outputBufferSize);
	vnzMemoryCreate(&coefMemory, ctrl->pCoefBuffer, ctrl->outputInfo.coefBufferSize);
	work = (void *)openPrivateMemory(workAddress, JPEG_DECODER_WORK_SIZE);
	if (work == NULL || vnzMemoryOpenPrivate(&streamMemory) == 0 ||
		vnzMemoryOpenPrivate(&outputMemory) == 0 ||
		(coefMemory.paddr && coefMemory.size && vnzMemoryOpenPrivate(&coefMemory) == 0)) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = splitStreamCheck(ctrl, work, streamMemory.vaddrVnz, streamMemory.size);
	if (result < 0)
		goto close;
	if (state == SPLIT_READY)
		result = jpegDecoderStart(work, (uint8_t *)outputMemory.vaddrVnz, 0, 0, 0, 0,
			(int16_t *)coefMemory.vaddrVnz);
	else
		result = jpegDecoderSetInput(work, ctrl->writeBufferSize, ctrl->isEndOfStream);
	if (result < 0)
		goto failed;

	do
		step = jpegDecoderStep(work);
	while (step >= 1);
	if (step == 0) {
		ctrl->writeBufferSize = 0;
		result = jpegDecoderFinish(work);
		state = SPLIT_DONE;
		goto close;
	}
	if (step != SCE_JPEG_ERROR_INPUT_SUSPENDED)
		goto failed;
	result = jpegDecoderInputPosition(work, &position, &ctrl->writeBufferSize);
	if (result < 0)
		goto failed;
	state = SPLIT_DECODE_SUSPENDED;
	consumed = position - streamMemory.vaddrVnz;
	goto close;
failed:
	state = SPLIT_DONE;
close:
	if (coefMemory.vaddrVnz && vnzMemoryClosePrivate(&coefMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (outputMemory.vaddrVnz && vnzMemoryClosePrivate(&outputMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (streamMemory.vaddrVnz && vnzMemoryClosePrivate(&streamMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	if (work && closePrivateMemory((uint32_t)work, JPEG_DECODER_WORK_SIZE) == 0 && result >= 0)
		result = ERROR_MEMORY;
	ctrl->state = state;
	ctrl->pWriteBuffer = ctrl->pStreamBuffer + consumed;
	ctrl->result = result ? result : step;
}

/* RPC 452: sceJpegSplitDecodeMJpeg, in the decoder's 8 KB work memory */
int32_t rpcJpegSplitDecodeMJpeg(VkVThreadResource *resource, uint32_t ctrlAddress,
	uint32_t workAddress)
{
	SceJpegSplitDecodeCtrl *ctrl;
	VnzMemory ctrlMemory;
	int32_t result;

	vnzMemoryCreate(&ctrlMemory, ctrlAddress, sizeof(SceJpegSplitDecodeCtrl));
	if (vnzMemoryOpenPublic(&ctrlMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	ctrl = (SceJpegSplitDecodeCtrl *)ctrlMemory.vaddrVnz;
	if (ctrl->state >= SPLIT_DONE) {
		result = SCE_JPEG_ERROR_INVALID_STATE;
		goto close;
	}
	result = vnzVThreadExecute(ctrl->context, 0,
		ctrl->state < SPLIT_READY ? splitHeaderJob : splitDecodeJob, (uint32_t)ctrl,
		workAddress, 0, 0);
	if (result == 0)
		result = ctrl->result;
close:
	if (ctrlMemory.vaddrVnz && vnzMemoryClosePublic(&ctrlMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	return result;
}

/*
 * One VThread's share of sceJpegMJpegCsc: with two, the first converts the
 * top rows (half the height rounded up to 32), the second the rest.
 */
static void cscJob(uint32_t argument, uint32_t index, uint32_t threads, uint32_t flag)
{
	JpegCscParam *param = (JpegCscParam *)argument;
	uint32_t height = param->xysize & 0xffff, width = param->xysize >> 16;
	uint32_t area = width * height, half, bytes;
	uint8_t *rgba = (uint8_t *)param->pRGBA;
	uint8_t *y = (uint8_t *)param->pYCbCr, *cb = y + area, *cr = cb + area;
	int32_t result;

	if (threads >= 2) {
		half = ((height + 31) & ~31) >> 1;
		if (half < height) {
			if (index == 0) {
				height = half;
			} else {
				rgba += param->iFrameWidth * half * 4;
				height -= half;
				y += half * width;
				cb += half * width;
				cr += half * width;
			}
		} else if (index != 0) {
			goto done;
		}
	}
	bytes = (param->iFrameWidth * height * 4 + 63) & ~63;
	vnzCachePurge(param->pYCbCr, (param->ycbcrSize + 63) & ~63);
	vnzCachePurge((uint32_t)rgba, bytes);
	result = jpegMJpegCsc(rgba, y, cb, cr, param->colorOption, width << 16 | height,
		param->iFrameWidth);
	vnzCacheWriteback((uint32_t)rgba, bytes);
	__asm__ __volatile__("syncm");
	if (index == 0)
		param->result = result;
done:
	vnzThreadSetFlags(param->caller, flag);
}

/* RPC 453: the encoder, with its context in the ARM's memory */
int32_t rpcJpegEncoder(VkVThreadResource *resource, uint32_t contextAddress, uint32_t argument,
	uint32_t argument2)
{
	VkVThreadContext *context = NULL;
	JpegEncoderContext *encoder;
	VnzMemory encoderMemory;
	int32_t result;

	vnzMemoryCreate(&encoderMemory, contextAddress, sizeof(JpegEncoderContext));
	if (vnzMemoryOpenPublic(&encoderMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	encoder = (JpegEncoderContext *)encoderMemory.vaddrVnz;
	context = vkVThreadContextCreate(1, resource);
	if (context == NULL) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = vnzVThreadExecute(context, 0, jpegEncoderJob, (uint32_t)encoder, argument,
		argument2, 0);
	if (result == 0)
		result = encoder->result;
close:
	if (context)
		vkVThreadContextDelete(context);
	if (encoderMemory.vaddrVnz && vnzMemoryClosePublic(&encoderMemory) == 0 && result >= 0)
		result = ERROR_MEMORY;
	return result;
}

/* RPC 454: sceJpegMJpegCsc, on one or two VThreads */
int32_t rpcJpegMJpegCsc(VkVThreadResource *resource, uint32_t paramAddress)
{
	VnzMemory paramMemory, ycbcrMemory, rgbaMemory;
	VkVThreadContext *context = NULL;
	uint32_t physical, flags;
	JpegCscParam *param;
	int32_t result;

	vnzMemoryCreate(&paramMemory, paramAddress, sizeof(JpegCscParam));
	if (vnzMemoryOpenPublic(&paramMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	param = (JpegCscParam *)paramMemory.vaddrVnz;
	vnzMemoryCreate(&ycbcrMemory, param->pYCbCr, param->ycbcrSize);
	vnzMemoryCreate(&rgbaMemory, param->pRGBA, param->rgbaSize);
	if (vnzMemoryOpenPublic(&ycbcrMemory) == 0 || vnzMemoryOpenPublic(&rgbaMemory) == 0) {
		result = ERROR_MEMORY;
		goto close;
	}
	/* the VThreads take the buffers by physical address */
	physical = vnzToPhysical(ycbcrMemory.vaddrVnz);
	param->pYCbCr = physical ? physical : ycbcrMemory.vaddrVnz;
	param->ycbcrSize = ycbcrMemory.size;
	physical = vnzToPhysical(rgbaMemory.vaddrVnz);
	param->pRGBA = physical ? physical : rgbaMemory.vaddrVnz;
	param->rgbaSize = rgbaMemory.size;
	param->caller = vnzThreadCurrent();

	context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (context == NULL) {
		result = ERROR_MEMORY;
		goto close;
	}
	result = jpegJobSet(context, 0, cscJob, (uint32_t)param, 0, param->threads, 1);
	if (result)
		goto close;
	if (param->threads >= 2) {
		result = jpegJobSet(context, 1, cscJob, (uint32_t)param, 1, param->threads, 2);
		if (result)
			goto close;
		flags = 0;
	} else {
		flags = 2;
	}
	do
		flags |= vnzThreadWaitFlags(3);
	while (flags != 3);
	result = param->result;
close:
	if (context)
		vkVThreadContextDelete(context);
	if (paramMemory.vaddrVnz) {
		if (rgbaMemory.vaddrVnz && vnzMemoryClosePublic(&rgbaMemory) == 0 && result >= 0)
			result = ERROR_MEMORY;
		if (ycbcrMemory.vaddrVnz && vnzMemoryClosePublic(&ycbcrMemory) == 0 &&
			result >= 0)
			result = ERROR_MEMORY;
		if (vnzMemoryClosePublic(&paramMemory) == 0 && result >= 0)
			result = ERROR_MEMORY;
	}
	return result;
}
