/*
 * CELP decoding for the ARM's audio codec (RPCs 256-261): MPEG-4 CELP at
 * 8 kHz. A 128-byte control block per stream and the decoder in memory the
 * ARM gives; the engine runs on the stream's VThread context.
 */
#include "application.h"

/* Sony's SDK, audiodec.h */
#define SCE_AUDIODEC_ERROR_INVALID_PTR			((int32_t)0x807f0008)
#define SCE_AUDIODEC_ERROR_INVALID_NSTREAMS		((int32_t)0x807f0010)
#define SCE_AUDIODEC_CELP_MAX_NSTREAMS			7

#define CTRL_SIZE			128
#define ENGINE_MEMORY_SIZE		17408
/* the memory the ARM allocates for a decoder */
#define DECODER_MEMORY_SIZE		17472
#define CONTEXT_JOBS			8
#define BATCH_JOB			7
#define DONE_FLAG			0x80000000

/* SceAudiodecInfoCelp */
typedef struct CelpInfo {
	uint32_t size;
	uint32_t excitationMode;
	uint32_t samplingRate;
	uint32_t bitRate;
	uint32_t lostCount;
} CelpInfo;

/* the decoder, in memory the ARM gives */
typedef struct CelpDecoder {
	uint32_t engine[ENGINE_MEMORY_SIZE / 4];
	uint32_t esSize;		/* the bytes given; then the bytes used */
	uint32_t pcmSize;
	VkVThreadContext *context;
	int32_t result;			/* the last job's */
	int32_t bitRateIndex;		/* the engine's for the bit rate, or -1 */
	uint32_t lostCount;
} CelpDecoder;

/* the ARM's control block */
typedef struct CelpCtrl {
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
	CelpInfo info;
	uint32_t reserved4[15];
} CelpCtrl;

/* the engine (0x85471a-) */
int32_t celpDecoderInit(void *engine, uint32_t size);
int32_t celpDecoderReset(void *engine);
int32_t celpDecoderSetBitRate(void *engine, const int32_t *bitRateIndex);
int32_t celpDecode(void *engine, const uint8_t *es, uint32_t esSize, uint32_t *esUsed,
	int16_t *pcm, uint32_t *pcmSize, uint32_t lostCount);

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

static int32_t celpBitRateIndex(uint32_t bitRate)
{
	switch (bitRate) {
	case 12200:
		return 26;
	case 11800:
		return 24;
	case 10700:
		return 21;
	case 9900:
		return 18;
	case 8700:
		return 15;
	case 7300:
		return 12;
	case 6600:
		return 9;
	case 5700:
		return 6;
	case 4650:
		return 2;
	case 3850:
		return 0;
	}
	return -1;
}

/* the stream's bit rate; a new decoder also gets its context */
static int32_t celpSetup(CelpCtrl *ctrl, VkVThreadResource *resource, CelpDecoder *decoder,
	uint32_t create)
{
	if (create)
		memset(decoder, 0, 4);
	decoder->bitRateIndex = celpBitRateIndex(ctrl->info.bitRate);
	if (create)
		decoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	if (decoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	return 0;
}

static int32_t celpRelease(CelpDecoder *decoder)
{
	if (decoder->context) {
		vkVThreadContextDelete(decoder->context);
		decoder->context = NULL;
	}
	return 0;
}

/* the engine started (or restarted) at the stream's bit rate, on the VThread */
static void celpStart(uint32_t decoderAddress, uint32_t create)
{
	VnzExtMemory decoderMemory;
	CelpDecoder *decoder;
	int32_t result, bitRateIndex;

	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(CelpDecoder));
	decoder = (CelpDecoder *)decoderAddress;
	if (result < 0)
		goto out;
	if (create)
		result = celpDecoderInit(decoder->engine, ENGINE_MEMORY_SIZE);
	else
		result = celpDecoderReset(decoder->engine);
	if (result < 0)
		goto out;
	bitRateIndex = decoder->bitRateIndex;
	result = celpDecoderSetBitRate(decoder->engine, &bitRateIndex);
out:
	decoder->result = result;
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

static void celpInitJob(uint32_t decoder, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	celpStart(decoder, 1);
}

static void celpResetJob(uint32_t decoder, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	celpStart(decoder, 0);
}

/* a frame; the decoder and its input and output are mapped here, on the VThread */
static void celpDecodeFrame(uint32_t decoderAddress, uint32_t esAddress, uint32_t pcmAddress)
{
	VnzExtMemory decoderMemory, esMemory, pcmMemory;
	CelpDecoder *decoder;
	int32_t result;

	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(CelpDecoder));
	decoder = (CelpDecoder *)decoderAddress;
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
	result = celpDecode(decoder->engine, (const uint8_t *)esAddress, decoder->esSize,
		&decoder->esSize, (int16_t *)pcmAddress, &decoder->pcmSize, decoder->lostCount);
	if (result < 0) {
		decoder->esSize = 0;
		decoder->pcmSize = 0;
	}
out:
	decoder->result = result;
	vnzExtMemoryClose(&pcmMemory, &pcmAddress);
	vnzExtMemoryClose(&esMemory, &esAddress);
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

static void celpDecodeJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	celpDecodeFrame(decoder, es, pcm);
}

/* a batch's decode, signalling the batch job */
static void celpBatchJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	VkVThreadContext *context;

	celpDecodeFrame(decoder, es, pcm);
	context = vkVThreadContextLock((VkVThreadContext *)argument4);
	vkVThreadJobSignal(context, BATCH_JOB);
	vkVThreadContextUnlock(context);
}

static void celpBatchDoneJob(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
}

/* a job on the context, waited for */
static int32_t celpRun(VkVThreadContext *context, VkJobFunction job, uint32_t decoder,
	uint32_t es, uint32_t pcm)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, job, decoder, es, pcm, 0, vnzThreadCurrent(),
		DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

static int32_t celpRunBatch(const uint32_t *decoders, VkVThreadContext *context,
	const uint32_t *es, const uint32_t *pcm, uint32_t count)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);
	uint32_t i;

	for (i = 0; i < count; i++)
		vkVThreadJobSet(locked, i, 0, celpBatchJob, decoders[i], es[i], pcm[i],
			(uint32_t)context);
	vkVThreadJobSetNotify(locked, BATCH_JOB, count, celpBatchDoneJob, 0, 0, 0, 0,
		vnzThreadCurrent(), DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 256: the decoder memory the ARM allocates */
int32_t rpcCelpSizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((CelpCtrl *)ctrlAddress)->decoderSize = DECODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* a decoder made (with its context, on the resource) or restarted */
static int32_t celpStartCall(uint32_t ctrlAddress, VkVThreadResource *resource, uint32_t create)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	VkVThreadContext *context;
	uint32_t decoderAddress = 0;
	int32_t result;
	CelpCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	if (create && resource == NULL)
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(CelpDecoder));
	if (result >= 0)
		result = celpSetup(ctrl, resource, (CelpDecoder *)decoderAddress, create);
	if (result >= 0) {
		context = ((CelpDecoder *)decoderAddress)->context;
	} else {
		context = NULL;
		/* the original does this even when the decoder could not be mapped */
		if (create)
			celpRelease((CelpDecoder *)decoderAddress);
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0)
		goto close;
	result = celpRun(context, create ? celpInitJob : celpResetJob, decoderAddress, 0, 0);
	if (result < 0)
		goto close;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		sizeof(CelpDecoder));
	if (result >= 0)
		result = ((CelpDecoder *)decoderAddress)->result;
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 257 */
int32_t rpcCelpInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	return celpStartCall(ctrlAddress, resource, 1);
}

/* RPC 261 */
int32_t rpcCelpReset(uint32_t ctrlAddress)
{
	return celpStartCall(ctrlAddress, NULL, 0);
}

/* RPC 258 */
int32_t rpcCelpRelease(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	CelpCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(CelpDecoder));
	if (result >= 0)
		result = celpRelease((CelpDecoder *)decoderAddress);
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 259: a frame */
int32_t rpcCelpDecode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory, esMemory, pcmMemory;
	uint32_t decoderAddress = 0, es = 0, pcm = 0, esSize, pcmSize;
	VkVThreadContext *context = NULL;
	CelpDecoder *decoder;
	int32_t result;
	CelpCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	es = ctrl->pEs;
	pcm = ctrl->pPcm;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(CelpDecoder));
	if (result >= 0) {
		decoder = (CelpDecoder *)decoderAddress;
		context = decoder->context;
		if (context == NULL) {
			result = SCE_AUDIODEC_ERROR_INVALID_PTR;
		} else {
			decoder->esSize = esSize;
			decoder->pcmSize = pcmSize;
			decoder->lostCount = ctrl->info.lostCount;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
		if (result >= 0)
			result = celpRun(context, celpDecodeJob, decoderAddress, es, pcm);
		result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	}
	result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		sizeof(CelpDecoder));
	if (result >= 0) {
		decoder = (CelpDecoder *)decoderAddress;
		result = decoder->result;
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
static int32_t streamError(CelpCtrl *ctrl, int32_t closed)
{
	if (closed < 0 && ctrl->result >= 0)
		ctrl->result = closed;
	return ctrl->result;
}

/*
 * RPC 260: a frame for each of count streams, decoded in parallel on the
 * first stream's context. Each stream's result is in its control block; the
 * time is the first block's.
 */
int32_t rpcCelpDecodeBatch(uint32_t ctrlsAddress, uint32_t count)
{
	VnzExtMemory ctrlsMemory, decoderMemory[SCE_AUDIODEC_CELP_MAX_NSTREAMS];
	VnzExtMemory esMemory[SCE_AUDIODEC_CELP_MAX_NSTREAMS];
	VnzExtMemory pcmMemory[SCE_AUDIODEC_CELP_MAX_NSTREAMS];
	uint32_t decoders[SCE_AUDIODEC_CELP_MAX_NSTREAMS], es[SCE_AUDIODEC_CELP_MAX_NSTREAMS];
	uint32_t pcm[SCE_AUDIODEC_CELP_MAX_NSTREAMS], esSize[SCE_AUDIODEC_CELP_MAX_NSTREAMS];
	uint32_t pcmSize[SCE_AUDIODEC_CELP_MAX_NSTREAMS], i;
	VkVThreadContext *context = NULL;
	int32_t result, closed;
	CelpDecoder *decoder;
	CelpCtrl *ctrls;

	vnzExtMemoryCreate(&ctrlsMemory);
	for (i = 0; i < SCE_AUDIODEC_CELP_MAX_NSTREAMS; i++) {
		vnzExtMemoryCreate(&decoderMemory[i]);
		vnzExtMemoryCreate(&esMemory[i]);
		vnzExtMemoryCreate(&pcmMemory[i]);
	}
	if (count == 0 || count > SCE_AUDIODEC_CELP_MAX_NSTREAMS)
		return SCE_AUDIODEC_ERROR_INVALID_NSTREAMS;
	result = vnzExtMemoryOpen(&ctrlsMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlsAddress,
		count * CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrls = (CelpCtrl *)ctrlsAddress;
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
			&decoders[i], sizeof(CelpDecoder));
		if (ctrls[i].result >= 0) {
			decoder = (CelpDecoder *)decoders[i];
			if (context == NULL)
				context = decoder->context;
			if (context == NULL) {
				ctrls[i].result = SCE_AUDIODEC_ERROR_INVALID_PTR;
			} else {
				decoder->esSize = esSize[i];
				decoder->pcmSize = pcmSize[i];
				decoder->lostCount = ctrls[i].info.lostCount;
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
		result = celpRunBatch(decoders, context, es, pcm, count);
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
			&decoders[i], sizeof(CelpDecoder));
		if (ctrls[i].result >= 0) {
			decoder = (CelpDecoder *)decoders[i];
			ctrls[i].result = decoder->result;
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
