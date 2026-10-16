/*
 * AAC decoding for the ARM's audio codec (RPCs 192-197). As for MP3: a
 * 128-byte control block per stream, the decoder in memory the ARM gives,
 * the engine (aacEngine) decoding a frame at a time on the stream's VThread
 * context.
 */
#include "application.h"

/* Sony's SDK, audiodec.h */
#define SCE_AUDIODEC_ERROR_OUT_OF_MEMORY		((int32_t)0x807f0004)
#define SCE_AUDIODEC_ERROR_INVALID_PTR			((int32_t)0x807f0008)
#define SCE_AUDIODEC_ERROR_INVALID_NSTREAMS		((int32_t)0x807f0010)
#define SCE_AUDIODEC_AAC_MAX_NSTREAMS			6

#define CTRL_SIZE			128
/* the memory the ARM allocates for a decoder, without and with SBR */
#define DECODER_MEMORY_SIZE		0x15bc0
#define DECODER_MEMORY_SIZE_SBR		0x22680
#define CONTEXT_JOBS			8
#define BATCH_JOB			7
#define DONE_FLAG			0x80000000
#define MAX_OUTPUTS			6

/* the engine's settings */
#define AAC_KEY_MEMORY_SIZE		4
#define AAC_KEY_FORMAT			(-21)
#define AAC_KEY_OUTPUT			(-31)
#define AAC_KEY_STEREO			(-204)
#define AAC_KEY_SBR			(-400)
#define AAC_KEY_SAMPLING_RATE		(-200)
#define AAC_KEY_CHANNELS		(-201)
#define AAC_FORMAT_RAW			122
#define AAC_FORMAT_ADTS			128
/* the memory size query: the layout's channels, with or without SBR */
#define AAC_MEMORY_PLAIN		0x10000
#define AAC_MEMORY_SBR			0x20000
#define AAC_MEMORY_MONO			0x1
#define AAC_MEMORY_STEREO		0x2
#define AAC_MEMORY_MULTICHANNEL		0x6

/* SceAudiodecInfoAac, and the SBR setting the ARM adds */
typedef struct AacInfo {
	uint32_t size;
	uint32_t isAdts;
	uint32_t ch;
	uint32_t samplingRate;
	uint32_t isSbr;
	int32_t sbrSetting;		/* only with SBR memory */
} AacInfo;

/* the decoder, in memory the ARM gives; the engine's instance follows it */
typedef struct AacDecoder {
	uint32_t esSize;		/* the bytes given; then the bytes used */
	uint32_t pcmSize;
	VkVThreadContext *context;
	int32_t result;			/* the last decode's */
	void *handle;
	uint32_t ch;
	uint32_t samplingRate;
	int32_t format;
	uint32_t isSbr;
	int32_t sbrSetting;
	uint32_t reserved[5];
	uint32_t memorySize;
	uint32_t engine[];
} AacDecoder;

/* the ARM's control block */
typedef struct AacCtrl {
	uint32_t reserved;
	uint32_t pEs;
	uint32_t esSize;		/* the bytes given; then the bytes used */
	uint32_t pPcm;
	uint32_t pcmSize;		/* the bytes given; then the bytes written */
	uint32_t reserved2;
	uint32_t time;			/* timer ticks the call took */
	int32_t result;
	uint32_t decoder;
	uint32_t decoderSize;
	uint32_t reserved3[2];
	AacInfo info;
	uint32_t reserved4[14];
} AacCtrl;

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

static uint32_t aacMemoryLayout(uint32_t ch, uint32_t isSbr)
{
	if (isSbr) {
		switch (ch) {
		case 1:
		case 2:
			return AAC_MEMORY_SBR | AAC_MEMORY_STEREO;
		case 3:
		case 4:
		case 5:
		case 6:
			return AAC_MEMORY_SBR | AAC_MEMORY_MULTICHANNEL;
		}
		return AAC_MEMORY_SBR;
	}
	switch (ch) {
	case 1:
		return AAC_MEMORY_PLAIN | AAC_MEMORY_MONO;
	case 2:
		return AAC_MEMORY_PLAIN | AAC_MEMORY_STEREO;
	case 3:
	case 4:
	case 5:
	case 6:
		return AAC_MEMORY_PLAIN | AAC_MEMORY_MULTICHANNEL;
	}
	return AAC_MEMORY_PLAIN;
}

/* the instance size for the channels */
static uint32_t aacMemorySize(uint32_t ch, uint32_t isSbr)
{
	const AudioDecoderEngine *engine = aacEngine();
	int32_t key, size, result;
	uint32_t work[4];

	key = AAC_KEY_MEMORY_SIZE;
	size = aacMemoryLayout(ch, isSbr);
	result = engine->control(NULL, &key, 1, &size, work);
	if (result)
		return result;
	return size;
}

/* the engine's settings for the stream */
static int32_t aacConfigure(AacDecoder *decoder, const AudioDecoderEngine *engine)
{
	int32_t keys[8], values[8];
	uint32_t work[4];

	keys[0] = AAC_KEY_FORMAT;
	values[0] = decoder->format;
	keys[1] = AAC_KEY_OUTPUT;
	values[1] = 101;
	keys[2] = AAC_KEY_STEREO;
	values[2] = !(decoder->isSbr && decoder->ch == 1);
	keys[3] = -407;
	values[3] = 0;
	keys[4] = AAC_KEY_SBR;
	values[4] = decoder->sbrSetting;
	keys[5] = AAC_KEY_SAMPLING_RATE;
	values[5] = decoder->samplingRate;
	keys[6] = AAC_KEY_CHANNELS;
	values[6] = decoder->ch;
	keys[7] = -45;
	values[7] = 0;
	return engine->control(decoder->handle, keys, 8, values, work);
}

static int32_t aacInit(AacCtrl *ctrl, VkVThreadResource *resource, AacDecoder *decoder)
{
	const AudioDecoderEngine *engine = aacEngine();
	uint32_t size;
	int32_t result;

	memset(decoder, 0, ctrl->decoderSize);
	decoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (decoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	decoder->format = ctrl->info.isAdts ? AAC_FORMAT_ADTS : AAC_FORMAT_RAW;
	decoder->samplingRate = ctrl->info.samplingRate;
	decoder->ch = ctrl->info.ch;
	decoder->isSbr = ctrl->info.isSbr;
	if (ctrl->decoderSize >= DECODER_MEMORY_SIZE_SBR)
		decoder->sbrSetting = ctrl->info.sbrSetting;
	decoder->memorySize = ctrl->decoderSize;
	size = aacMemorySize(decoder->ch, decoder->isSbr);
	if (ctrl->decoderSize < size + sizeof(AacDecoder))
		return SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
	result = engine->create(&decoder->handle, decoder->engine, size);
	if (result)
		return result;
	return aacConfigure(decoder, engine);
}

static int32_t aacRelease(AacDecoder *decoder)
{
	const AudioDecoderEngine *engine = aacEngine();
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
static int32_t aacReset(AacDecoder *decoder)
{
	const AudioDecoderEngine *engine = aacEngine();
	uint32_t size;
	int32_t result;

	result = engine->destroy(decoder->handle);
	if (result)
		return result;
	size = aacMemorySize(decoder->ch, decoder->isSbr);
	if (decoder->memorySize < size + sizeof(AacDecoder))
		return SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
	memset(decoder->engine, 0, size);
	result = engine->create(&decoder->handle, decoder->engine, size);
	if (result)
		return result;
	return aacConfigure(decoder, engine);
}

/*
 * A frame; the decoder (its header first, for its size) and its input and
 * output are mapped here, on the VThread.
 */
static void aacDecode(uint32_t decoderAddress, uint32_t esAddress, uint32_t pcmAddress)
{
	const AudioDecoderEngine *engine = aacEngine();
	VnzExtMemory decoderMemory, esMemory, pcmMemory;
	uint32_t outputSizes[MAX_OUTPUTS], frames = 1, memorySize;
	int16_t *outputs[MAX_OUTPUTS];
	AacDecoder *decoder;
	int32_t result;

	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		sizeof(AacDecoder));
	decoder = (AacDecoder *)decoderAddress;
	if (result < 0)
		goto out;
	memorySize = decoder->memorySize;
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		memorySize);
	decoder = (AacDecoder *)decoderAddress;
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
	outputs[0] = (int16_t *)pcmAddress;
	outputs[1] = outputs[2] = outputs[3] = outputs[4] = outputs[5] = NULL;
	outputSizes[0] = decoder->pcmSize;
	outputSizes[1] = outputSizes[2] = outputSizes[3] = outputSizes[4] = outputSizes[5] = 0;
	result = engine->decode(decoder->handle, (const uint8_t **)&esAddress, &decoder->esSize, 1,
		outputs, outputSizes, &frames);
	decoder->pcmSize = outputSizes[0];
out:
	decoder->result = result;
	vnzExtMemoryClose(&pcmMemory, &pcmAddress);
	vnzExtMemoryClose(&esMemory, &esAddress);
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

static void aacDecodeJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	aacDecode(decoder, es, pcm);
}

/* a batch's decode, signalling the batch job */
static void aacBatchJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	VkVThreadContext *context;

	aacDecode(decoder, es, pcm);
	context = vkVThreadContextLock((VkVThreadContext *)argument4);
	vkVThreadJobSignal(context, BATCH_JOB);
	vkVThreadContextUnlock(context);
}

static void aacBatchDoneJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
}

static int32_t aacRun(uint32_t decoder, VkVThreadContext *context, uint32_t es, uint32_t pcm)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, aacDecodeJob, decoder, es, pcm, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

static int32_t aacRunBatch(const uint32_t *decoders, VkVThreadContext *context,
	const uint32_t *es, const uint32_t *pcm, uint32_t count)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);
	uint32_t i;

	for (i = 0; i < count; i++)
		vkVThreadJobSet(locked, i, 0, aacBatchJob, decoders[i], es[i], pcm[i],
			(uint32_t)context);
	vkVThreadJobSetNotify(locked, BATCH_JOB, count, aacBatchDoneJob, 0, 0, 0, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 192: the decoder memory the ARM allocates */
int32_t rpcAacSizes(uint32_t ctrlAddress, uint32_t isSbr)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((AacCtrl *)ctrlAddress)->decoderSize = isSbr ? DECODER_MEMORY_SIZE_SBR :
			DECODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 193: a decoder for the stream, on a context of the resource */
int32_t rpcAacInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	AacCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	if (resource == NULL)
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AacCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		ctrl->decoderSize);
	if (result >= 0) {
		result = aacInit(ctrl, resource, (AacDecoder *)decoderAddress);
		if (result < 0)
			aacRelease((AacDecoder *)decoderAddress);
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* the decoder of a control block, released or reset */
static int32_t aacDecoderCall(uint32_t ctrlAddress, int32_t (*call)(AacDecoder *decoder))
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	AacCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AacCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		ctrl->decoderSize);
	if (result >= 0)
		result = call((AacDecoder *)decoderAddress);
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 194 */
int32_t rpcAacRelease(uint32_t ctrlAddress)
{
	return aacDecoderCall(ctrlAddress, aacRelease);
}

/* RPC 197 */
int32_t rpcAacReset(uint32_t ctrlAddress)
{
	return aacDecoderCall(ctrlAddress, aacReset);
}

/* RPC 195: a frame */
int32_t rpcAacDecode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory, esMemory, pcmMemory;
	uint32_t decoderAddress = 0, es = 0, pcm = 0, esSize, pcmSize;
	VkVThreadContext *context = NULL;
	AacDecoder *decoder;
	int32_t result;
	AacCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AacCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	es = ctrl->pEs;
	pcm = ctrl->pPcm;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		ctrl->decoderSize);
	if (result >= 0) {
		decoder = (AacDecoder *)decoderAddress;
		context = decoder->context;
		if (context == NULL) {
			result = SCE_AUDIODEC_ERROR_INVALID_PTR;
		} else {
			decoder->esSize = esSize;
			decoder->pcmSize = pcmSize;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
		if (result >= 0)
			result = aacRun(decoderAddress, context, es, pcm);
		result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	}
	result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		ctrl->decoderSize);
	if (result >= 0) {
		decoder = (AacDecoder *)decoderAddress;
		result = decoder->result;
		if (result == 0) {
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
static int32_t streamError(AacCtrl *ctrl, int32_t closed)
{
	if (closed < 0 && ctrl->result >= 0)
		ctrl->result = closed;
	return ctrl->result;
}

/*
 * RPC 196: a frame for each of count streams, decoded in parallel on the
 * first stream's context. Each stream's result is in its control block; the
 * time is the first block's.
 */
int32_t rpcAacDecodeBatch(uint32_t ctrlsAddress, uint32_t count)
{
	VnzExtMemory ctrlsMemory, decoderMemory[SCE_AUDIODEC_AAC_MAX_NSTREAMS];
	VnzExtMemory esMemory[SCE_AUDIODEC_AAC_MAX_NSTREAMS];
	VnzExtMemory pcmMemory[SCE_AUDIODEC_AAC_MAX_NSTREAMS];
	uint32_t decoders[SCE_AUDIODEC_AAC_MAX_NSTREAMS], es[SCE_AUDIODEC_AAC_MAX_NSTREAMS];
	uint32_t pcm[SCE_AUDIODEC_AAC_MAX_NSTREAMS], esSize[SCE_AUDIODEC_AAC_MAX_NSTREAMS];
	uint32_t pcmSize[SCE_AUDIODEC_AAC_MAX_NSTREAMS], i;
	VkVThreadContext *context = NULL;
	int32_t result, closed;
	AacDecoder *decoder;
	AacCtrl *ctrls;

	vnzExtMemoryCreate(&ctrlsMemory);
	for (i = 0; i < SCE_AUDIODEC_AAC_MAX_NSTREAMS; i++) {
		vnzExtMemoryCreate(&decoderMemory[i]);
		vnzExtMemoryCreate(&esMemory[i]);
		vnzExtMemoryCreate(&pcmMemory[i]);
	}
	if (count == 0 || count > SCE_AUDIODEC_AAC_MAX_NSTREAMS)
		return SCE_AUDIODEC_ERROR_INVALID_NSTREAMS;
	result = vnzExtMemoryOpen(&ctrlsMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlsAddress,
		count * CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrls = (AacCtrl *)ctrlsAddress;
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

	for (i = 0; i < count; i++) {
		ctrls[i].result = vnzExtMemoryOpen(&decoderMemory[i], 1, VNZ_MEMORY_MODE_PRIVATE,
			&decoders[i], ctrls[i].decoderSize);
		if (ctrls[i].result >= 0) {
			decoder = (AacDecoder *)decoders[i];
			if (context == NULL)
				context = decoder->context;
			if (context == NULL) {
				ctrls[i].result = SCE_AUDIODEC_ERROR_INVALID_PTR;
			} else {
				decoder->esSize = esSize[i];
				decoder->pcmSize = pcmSize[i];
			}
		}
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
		result = aacRunBatch(decoders, context, es, pcm, count);
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
		ctrls[i].result = vnzExtMemoryOpen(&decoderMemory[i], 1, VNZ_MEMORY_MODE_PROTECTED,
			&decoders[i], ctrls[i].decoderSize);
		if (ctrls[i].result >= 0) {
			decoder = (AacDecoder *)decoders[i];
			ctrls[i].result = decoder->result;
			if (ctrls[i].result == 0) {
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
