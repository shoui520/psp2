/*
 * MP3 decoding for the ARM's audio codec (RPCs 176-181). The ARM keeps a
 * 128-byte control block per stream and the decoder's memory; the engine
 * (mp3Engine) decodes a frame at a time on the stream's VThread context.
 */
#include "application.h"

/* Sony's SDK, audiodec.h */
#define SCE_AUDIODEC_ERROR_OUT_OF_MEMORY		((int32_t)0x807f0004)
#define SCE_AUDIODEC_ERROR_INVALID_PTR			((int32_t)0x807f0008)
#define SCE_AUDIODEC_ERROR_INVALID_NSTREAMS		((int32_t)0x807f0010)
#define SCE_AUDIODEC_MP3_ERROR_INVALID_CH		((int32_t)0x807f2800)
#define SCE_AUDIODEC_MP3_ERROR_INVALID_MPEG_VERSION	((int32_t)0x807f2801)
#define SCE_AUDIODEC_MP3_MAX_NSTREAMS			6

#define CTRL_SIZE			128
#define ENGINE_MEMORY_SIZE		23424
/* the memory the ARM allocates for a decoder */
#define DECODER_MEMORY_SIZE		23488
#define CONTEXT_JOBS			8
#define BATCH_JOB			7
#define DONE_FLAG			0x80000000

/* the engine's settings */
#define MP3_KEY_MEMORY_SIZE		4
#define MP3_KEY_FRAME_STATUS		9
/* a frame the engine could not decode; an error only for some statuses */
#define MP3_ERROR_FRAME			(-98)

#define HEADER_SIZE			4
#define CHANNEL_MODE_MONO		3

/* SceAudiodecInfoMp3 */
typedef struct Mp3Info {
	uint32_t size;
	uint32_t ch;
	uint32_t version;
} Mp3Info;

/* the decoder, in memory the ARM gives */
typedef struct Mp3Decoder {
	uint32_t engine[ENGINE_MEMORY_SIZE / 4];
	uint32_t esSize;		/* the bytes given; then the bytes used */
	uint32_t pcmSize;
	VkVThreadContext *context;
	int32_t result;			/* the last decode's */
	void *handle;
	uint32_t ch;
	uint32_t version;
} Mp3Decoder;

/* the ARM's control block */
typedef struct Mp3Ctrl {
	uint32_t reserved;
	uint32_t pEs;
	uint32_t esSize;		/* the bytes given; then the bytes used (or to skip) */
	uint32_t pPcm;
	uint32_t pcmSize;		/* the bytes given; then the bytes written */
	uint32_t reserved2;
	uint32_t time;			/* timer ticks the call took */
	int32_t result;
	uint32_t decoder;
	uint32_t decoderSize;
	uint32_t reserved3[2];
	Mp3Info info;
	uint32_t reserved4[17];
} Mp3Ctrl;

static uint32_t timerRead(void)
{
	return *(volatile uint32_t *)VNZ_FREE_RUNNING_TIMER;
}

/* a close's error, unless there is one already */
static int32_t keepError(int32_t result, int32_t closed)
{
	if (closed < 0 && result >= 0)
		return closed;
	return result;
}

/* the engine's settings for the Vita's streams */
static int32_t mp3Configure(Mp3Decoder *decoder, const AudioDecoderEngine *engine)
{
	int32_t keys[2], values[2];
	uint32_t work[4];

	keys[0] = -31;
	values[0] = 101;
	keys[1] = -204;
	values[1] = 1;
	return engine->control(decoder->handle, keys, 2, values, work);
}

static int32_t mp3Init(Mp3Ctrl *ctrl, VkVThreadResource *resource, Mp3Decoder *decoder)
{
	const AudioDecoderEngine *engine = mp3Engine();
	int32_t key, size, result;
	uint32_t work[4];

	memset(decoder, 0, 4);
	decoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (decoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	decoder->ch = ctrl->info.ch;
	decoder->version = ctrl->info.version;
	key = MP3_KEY_MEMORY_SIZE;
	result = engine->control(NULL, &key, 1, &size, work);
	if (result)
		return result;
	if ((uint32_t)size > ENGINE_MEMORY_SIZE)
		return SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
	result = engine->create(&decoder->handle, decoder, ENGINE_MEMORY_SIZE);
	if (result)
		return result;
	return mp3Configure(decoder, engine);
}

static int32_t mp3Release(Mp3Decoder *decoder)
{
	const AudioDecoderEngine *engine = mp3Engine();
	int32_t result = 0, destroyed;

	if (decoder->handle) {
		destroyed = engine->destroy(decoder->handle);
		if (destroyed)
			result = destroyed;
	}
	if (decoder->context) {
		vkVThreadContextDelete(decoder->context);
		decoder->context = NULL;
	}
	return result;
}

/* a new instance in the same memory */
static int32_t mp3Reset(Mp3Decoder *decoder)
{
	const AudioDecoderEngine *engine = mp3Engine();
	int32_t result;

	result = engine->destroy(decoder->handle);
	if (result)
		return result;
	memset(decoder, 0, ENGINE_MEMORY_SIZE);
	result = engine->create(&decoder->handle, decoder, ENGINE_MEMORY_SIZE);
	if (result)
		return result;
	return mp3Configure(decoder, engine);
}

/*
 * The first offset with a header of the stream's MPEG version and channels
 * (the sync word is not checked), or the error of the first header.
 */
static int32_t mp3FindHeader(Mp3Decoder *decoder, const uint8_t *es)
{
	uint8_t header[HEADER_SIZE];
	int32_t result = 0, error;
	uint32_t i, mode;

	for (i = 0; i < decoder->esSize - HEADER_SIZE; i++) {
		header[0] = es[i];
		header[1] = es[i + 1];
		header[2] = es[i + 2];
		header[3] = es[i + 3];
		error = 0;
		if ((uint32_t)((header[1] & 0x18) >> 3) != decoder->version)
			error = SCE_AUDIODEC_MP3_ERROR_INVALID_MPEG_VERSION;
		mode = (header[3] & 0xc0) >> 6;
		if ((decoder->ch == 1) != (mode == CHANNEL_MODE_MONO))
			error = SCE_AUDIODEC_MP3_ERROR_INVALID_CH;
		if (error == 0)
			return i;
		if (i == 0)
			result = error;
	}
	return result;
}

/* a frame; the decoder and its input and output are mapped here, on the VThread */
static void mp3Decode(uint32_t decoderAddress, uint32_t esAddress, uint32_t pcmAddress)
{
	const AudioDecoderEngine *engine = mp3Engine();
	VnzExtMemory decoderMemory, esMemory, pcmMemory;
	uint32_t frames = 1;
	Mp3Decoder *decoder;
	int32_t result;

	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(Mp3Decoder));
	decoder = (Mp3Decoder *)decoderAddress;
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&esMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &esAddress,
		decoder->esSize);
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&pcmMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &pcmAddress,
		decoder->pcmSize);
	if (result < 0)
		goto out;
	result = engine->decode(decoder->handle, (const uint8_t **)&esAddress, &decoder->esSize, 1,
		(int16_t **)&pcmAddress, &decoder->pcmSize, &frames);
out:
	decoder->result = result;
	vnzExtMemoryClose(&pcmMemory, &pcmAddress);
	vnzExtMemoryClose(&esMemory, &esAddress);
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

static void mp3DecodeJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	mp3Decode(decoder, es, pcm);
}

/* a batch's decode, signalling the batch job */
static void mp3BatchJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	VkVThreadContext *context;

	mp3Decode(decoder, es, pcm);
	context = vkVThreadContextLock((VkVThreadContext *)argument4);
	vkVThreadJobSignal(context, BATCH_JOB);
	vkVThreadContextUnlock(context);
}

static void mp3BatchDoneJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
}

static int32_t mp3Run(uint32_t decoder, VkVThreadContext *context, uint32_t es, uint32_t pcm)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, mp3DecodeJob, decoder, es, pcm, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

static int32_t mp3RunBatch(const uint32_t *decoders, VkVThreadContext *context,
	const uint32_t *es, const uint32_t *pcm, uint32_t count)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);
	uint32_t i;

	for (i = 0; i < count; i++)
		vkVThreadJobSet(locked, i, 0, mp3BatchJob, decoders[i], es[i], pcm[i],
			(uint32_t)context);
	vkVThreadJobSetNotify(locked, BATCH_JOB, count, mp3BatchDoneJob, 0, 0, 0, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* the last decode's result; a frame error counts only for some statuses */
static int32_t mp3Result(Mp3Decoder *decoder)
{
	const AudioDecoderEngine *engine = mp3Engine();
	int32_t result = decoder->result, key, status;
	uint32_t work[4];

	key = MP3_KEY_FRAME_STATUS;
	if (engine->control(decoder->handle, &key, 1, &status, work))
		return result;
	if (result == 0)
		return 0;
	if (result != MP3_ERROR_FRAME)
		return result;
	switch (status) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 17:
	case 18:
	case 25:
		return result;
	}
	return 0;
}

/* RPC 176: the decoder memory the ARM allocates */
int32_t rpcMp3Sizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((Mp3Ctrl *)ctrlAddress)->decoderSize = DECODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 177: a decoder for the stream, on a context of the resource */
int32_t rpcMp3Init(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	Mp3Ctrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	if (resource == NULL)
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (Mp3Ctrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(Mp3Decoder));
	if (result >= 0) {
		result = mp3Init(ctrl, resource, (Mp3Decoder *)decoderAddress);
		if (result < 0)
			mp3Release((Mp3Decoder *)decoderAddress);
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* the decoder of a control block, released or reset */
static int32_t mp3DecoderCall(uint32_t ctrlAddress, int32_t (*call)(Mp3Decoder *decoder))
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	Mp3Ctrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (Mp3Ctrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(Mp3Decoder));
	if (result >= 0)
		result = call((Mp3Decoder *)decoderAddress);
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 178 */
int32_t rpcMp3Release(uint32_t ctrlAddress)
{
	return mp3DecoderCall(ctrlAddress, mp3Release);
}

/* RPC 181 */
int32_t rpcMp3Reset(uint32_t ctrlAddress)
{
	return mp3DecoderCall(ctrlAddress, mp3Reset);
}

/*
 * RPC 179: a frame. When the stream's header is further on, the call only
 * says how many bytes to skip (in esSize).
 */
int32_t rpcMp3Decode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory, esMemory, pcmMemory;
	uint32_t decoderAddress = 0, es = 0, pcm = 0, esSize, pcmSize;
	VkVThreadContext *context = NULL;
	Mp3Decoder *decoder;
	int32_t result;
	Mp3Ctrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (Mp3Ctrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	es = ctrl->pEs;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;
	pcm = ctrl->pPcm;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;

	/* the header, read through protected memory */
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(Mp3Decoder));
	if (result >= 0) {
		result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PROTECTED, &es, esSize);
		if (result >= 0) {
			decoder = (Mp3Decoder *)decoderAddress;
			context = decoder->context;
			if (context == NULL) {
				result = SCE_AUDIODEC_ERROR_INVALID_PTR;
			} else {
				decoder->esSize = esSize;
				decoder->pcmSize = pcmSize;
				result = mp3FindHeader(decoder, (const uint8_t *)es);
				if (result >= 1) {
					ctrl->esSize = result;
					result = 0;
				}
			}
		}
		result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0 || ctrl->esSize != 0)
		goto close;

	result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
		if (result >= 0)
			result = mp3Run(decoderAddress, context, es, pcm);
		result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	}
	result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(Mp3Decoder));
	if (result >= 0) {
		decoder = (Mp3Decoder *)decoderAddress;
		result = mp3Result(decoder);
		if (result >= 0) {
			ctrl->esSize = decoder->esSize;
			ctrl->pcmSize = decoder->pcmSize;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* a close's error into the stream's result */
static int32_t streamError(Mp3Ctrl *ctrl, int32_t closed)
{
	if (closed < 0 && ctrl->result >= 0)
		ctrl->result = closed;
	return ctrl->result;
}

/*
 * RPC 180: a frame for each of count streams, decoded in parallel on the
 * first stream's context. Each stream's result is in its control block; a
 * stream whose header is further on is decoded anyway (its result is the
 * offset). The time is the first block's.
 */
int32_t rpcMp3DecodeBatch(uint32_t ctrlsAddress, uint32_t count)
{
	VnzExtMemory ctrlsMemory, decoderMemory[SCE_AUDIODEC_MP3_MAX_NSTREAMS];
	VnzExtMemory esMemory[SCE_AUDIODEC_MP3_MAX_NSTREAMS];
	VnzExtMemory pcmMemory[SCE_AUDIODEC_MP3_MAX_NSTREAMS];
	uint32_t decoders[SCE_AUDIODEC_MP3_MAX_NSTREAMS], es[SCE_AUDIODEC_MP3_MAX_NSTREAMS];
	uint32_t pcm[SCE_AUDIODEC_MP3_MAX_NSTREAMS], esSize[SCE_AUDIODEC_MP3_MAX_NSTREAMS];
	uint32_t pcmSize[SCE_AUDIODEC_MP3_MAX_NSTREAMS], i;
	VkVThreadContext *context = NULL;
	int32_t result, closed;
	Mp3Decoder *decoder;
	Mp3Ctrl *ctrls;

	vnzExtMemoryCreate(&ctrlsMemory);
	for (i = 0; i < SCE_AUDIODEC_MP3_MAX_NSTREAMS; i++) {
		vnzExtMemoryCreate(&decoderMemory[i]);
		vnzExtMemoryCreate(&esMemory[i]);
		vnzExtMemoryCreate(&pcmMemory[i]);
	}
	if (count == 0 || count > SCE_AUDIODEC_MP3_MAX_NSTREAMS)
		return SCE_AUDIODEC_ERROR_INVALID_NSTREAMS;
	result = vnzExtMemoryOpen(&ctrlsMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlsAddress,
		count * CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrls = (Mp3Ctrl *)ctrlsAddress;
	ctrls[0].time = timerRead();
	for (i = 0; i < count; i++) {
		decoders[i] = ctrls[i].decoder;
		es[i] = ctrls[i].pEs;
		pcm[i] = ctrls[i].pPcm;
		esSize[i] = ctrls[i].esSize;
		pcmSize[i] = ctrls[i].pcmSize;
		ctrls[i].esSize = 0;
		ctrls[i].pcmSize = 0;
	}

	/* each stream's header */
	for (i = 0; i < count; i++) {
		ctrls[i].result = vnzExtMemoryOpen(&decoderMemory[i], 1, VNZ_MEMORY_MODE_PRIVATE,
			&decoders[i], sizeof(Mp3Decoder));
		if (ctrls[i].result >= 0)
			ctrls[i].result = vnzExtMemoryOpen(&esMemory[i], 0, VNZ_MEMORY_MODE_PROTECTED,
				&es[i], esSize[i]);
		if (ctrls[i].result >= 0) {
			decoder = (Mp3Decoder *)decoders[i];
			if (context == NULL)
				context = decoder->context;
			if (context == NULL) {
				ctrls[i].result = SCE_AUDIODEC_ERROR_INVALID_PTR;
			} else {
				decoder->esSize = esSize[i];
				decoder->pcmSize = pcmSize[i];
				ctrls[i].result = mp3FindHeader(decoder, (const uint8_t *)es[i]);
			}
		}
		streamError(&ctrls[i], vnzExtMemoryClose(&esMemory[i], &es[i]));
		streamError(&ctrls[i], vnzExtMemoryClose(&decoderMemory[i], &decoders[i]));
		result = keepError(result, ctrls[i].result);
	}
	if (result < 0)
		goto close;

	for (i = 0; i < count; i++) {
		ctrls[i].result = vnzExtMemoryOpen(&esMemory[i], 0, VNZ_MEMORY_MODE_PUBLIC, &es[i],
			esSize[i]);
		if (ctrls[i].result >= 0)
			ctrls[i].result = vnzExtMemoryOpen(&pcmMemory[i], 0, VNZ_MEMORY_MODE_PUBLIC,
				&pcm[i], pcmSize[i]);
		if (ctrls[i].result < 0) {
			result = ctrls[i].result;
			break;
		}
	}
	if (result >= 0)
		result = mp3RunBatch(decoders, context, es, pcm, count);
	for (i = 0; i < count; i++) {
		closed = vnzExtMemoryClose(&pcmMemory[i], &pcm[i]);
		if (closed < 0)
			result = streamError(&ctrls[i], closed);
		closed = vnzExtMemoryClose(&esMemory[i], &es[i]);
		if (closed < 0)
			result = streamError(&ctrls[i], closed);
	}
	if (result < 0)
		goto close;

	for (i = 0; i < count; i++) {
		ctrls[i].result = vnzExtMemoryOpen(&decoderMemory[i], 1, VNZ_MEMORY_MODE_PRIVATE,
			&decoders[i], sizeof(Mp3Decoder));
		if (ctrls[i].result >= 0) {
			decoder = (Mp3Decoder *)decoders[i];
			ctrls[i].result = mp3Result(decoder);
			if (ctrls[i].result >= 0) {
				ctrls[i].esSize = decoder->esSize;
				ctrls[i].pcmSize = decoder->pcmSize;
			}
		}
		streamError(&ctrls[i], vnzExtMemoryClose(&decoderMemory[i], &decoders[i]));
		result = keepError(result, ctrls[i].result);
	}
	ctrls[0].time = timerRead() - ctrls[0].time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlsMemory, &ctrlsAddress));
}
