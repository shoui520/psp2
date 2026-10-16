/*
 * SILK decoding for the ARM's audio codec (RPCs 272-277; the ARM's
 * audiodec type 0x1007, not in the SDK). A 128-byte control block and the
 * decoder in memory the ARM gives; the SILK SDK's decoder runs on the
 * stream's VThread context.
 */
#include "application.h"

/* Sony's SDK, audiodec.h */
#define SCE_AUDIODEC_ERROR_OUT_OF_MEMORY		((int32_t)0x807f0004)
#define SCE_AUDIODEC_ERROR_INVALID_PTR			((int32_t)0x807f0008)

#define CTRL_SIZE			128
#define ENGINE_MEMORY_SIZE		27392
/* the memory the ARM allocates for a decoder */
#define DECODER_MEMORY_SIZE		27456
#define CONTEXT_JOBS			1
#define DONE_FLAG			0x80000000
/* SILK's packets hold up to five 20 ms frames */
#define MAX_FRAMES			5

/* the decoder, in memory the ARM gives */
typedef struct SilkDecoder {
	uint32_t engine[ENGINE_MEMORY_SIZE / 4];
	uint32_t esSize;		/* the bytes given */
	uint32_t pcmSize;		/* the bytes given; then the bytes written */
	VkVThreadContext *context;
	int32_t result;			/* the last job's */
	int32_t sampleRate;
	int32_t lostFlag;
} SilkDecoder;

/* the ARM's control block */
typedef struct SilkCtrl {
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
	uint32_t infoSize;
	int32_t sampleRate;
	uint32_t reserved4[18];
} SilkCtrl;

/* the SILK SDK's decoder (0x857932-) */
typedef struct SKP_SILK_SDK_DecControlStruct {
	int32_t API_sampleRate;
	int32_t frameSize;
	int32_t framesPerPacket;
	int32_t moreInternalDecoderFrames;
	int32_t inBandFECOffset;
} SKP_SILK_SDK_DecControlStruct;

int32_t SKP_Silk_SDK_Get_Decoder_Size(int32_t *decSizeBytes);
int32_t SKP_Silk_SDK_InitDecoder(void *decState);
int32_t SKP_Silk_SDK_Decode(void *decState, SKP_SILK_SDK_DecControlStruct *decControl,
	int32_t lostFlag, const uint8_t *inData, int32_t nBytesIn, int16_t *samplesOut,
	int32_t *nSamplesOut);

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

/* a new decoder gets its context */
static int32_t silkSetup(SilkCtrl *ctrl, VkVThreadResource *resource, SilkDecoder *decoder,
	uint32_t create)
{
	if (create) {
		memset(decoder, 0, 4);
		decoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	}
	if (decoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	return 0;
}

static int32_t silkRelease(SilkDecoder *decoder)
{
	if (decoder->context) {
		vkVThreadContextDelete(decoder->context);
		decoder->context = NULL;
	}
	return 0;
}

/* the decoder (re)started, on the VThread */
static void silkStartJob(uint32_t decoderAddress, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	VnzExtMemory decoderMemory;
	SilkDecoder *decoder;
	int32_t result, size;

	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(SilkDecoder));
	decoder = (SilkDecoder *)decoderAddress;
	if (result < 0)
		goto out;
	size = 0;
	result = SKP_Silk_SDK_Get_Decoder_Size(&size);
	if (result < 0)
		goto out;
	if (size < 1 || size > ENGINE_MEMORY_SIZE)
		result = SCE_AUDIODEC_ERROR_OUT_OF_MEMORY;
	else
		result = SKP_Silk_SDK_InitDecoder(decoder->engine);
out:
	decoder->result = result;
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

/*
 * A packet: its frames, one call each, until the decoder has no more (or,
 * for a lost packet, for the frames a packet holds).
 */
static void silkDecodeFrame(uint32_t decoderAddress, uint32_t esAddress, uint32_t pcmAddress)
{
	VnzExtMemory decoderMemory, esMemory, pcmMemory;
	SKP_SILK_SDK_DecControlStruct control;
	int32_t result, samples, total, frames;
	SilkDecoder *decoder;
	uint32_t more;

	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(SilkDecoder));
	decoder = (SilkDecoder *)decoderAddress;
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

	memset(&control, 0, sizeof(control));
	control.API_sampleRate = decoder->sampleRate;
	total = 0;
	frames = 0;
	do {
		samples = 0;
		result = SKP_Silk_SDK_Decode(decoder->engine, &control, decoder->lostFlag,
			(const uint8_t *)esAddress, decoder->esSize, (int16_t *)pcmAddress + total,
			&samples);
		if (result < 0) {
			decoder->esSize = 0;
			decoder->pcmSize = 0;
			break;
		}
		total += samples;
		decoder->pcmSize = total * 2;
		frames++;
		if (decoder->lostFlag)
			more = frames < control.framesPerPacket && frames < MAX_FRAMES;
		else
			more = control.moreInternalDecoderFrames && frames < MAX_FRAMES;
	} while (more);
out:
	decoder->result = result;
	vnzExtMemoryClose(&pcmMemory, &pcmAddress);
	vnzExtMemoryClose(&esMemory, &esAddress);
	vnzExtMemoryClose(&decoderMemory, &decoderAddress);
}

static void silkDecodeJob(uint32_t decoder, uint32_t es, uint32_t pcm, uint32_t argument4)
{
	silkDecodeFrame(decoder, es, pcm);
}

/* a job on the context, waited for */
static int32_t silkRun(VkVThreadContext *context, VkJobFunction job, uint32_t decoder,
	uint32_t es, uint32_t pcm)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, job, decoder, es, pcm, 0, vnzThreadCurrent(),
		DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 272: the decoder memory the ARM allocates */
int32_t rpcSilkSizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((SilkCtrl *)ctrlAddress)->decoderSize = DECODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* a decoder made (with its context, on the resource) or restarted */
static int32_t silkStartCall(uint32_t ctrlAddress, VkVThreadResource *resource,
	uint32_t create)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	VkVThreadContext *context;
	uint32_t decoderAddress = 0;
	int32_t result;
	SilkCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	if (create && resource == NULL)
		return SCE_AUDIODEC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(SilkDecoder));
	if (result >= 0)
		result = silkSetup(ctrl, resource, (SilkDecoder *)decoderAddress, create);
	if (result >= 0) {
		context = ((SilkDecoder *)decoderAddress)->context;
	} else {
		context = NULL;
		/* the original does this even when the decoder could not be mapped */
		if (create)
			silkRelease((SilkDecoder *)decoderAddress);
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0)
		goto close;
	result = silkRun(context, silkStartJob, decoderAddress, 0, 0);
	if (result < 0)
		goto close;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		sizeof(SilkDecoder));
	if (result >= 0)
		result = ((SilkDecoder *)decoderAddress)->result;
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 273 */
int32_t rpcSilkInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	return silkStartCall(ctrlAddress, resource, 1);
}

/* RPC 277 */
int32_t rpcSilkReset(uint32_t ctrlAddress)
{
	return silkStartCall(ctrlAddress, NULL, 0);
}

/* RPC 274 */
int32_t rpcSilkRelease(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory;
	uint32_t decoderAddress = 0;
	int32_t result;
	SilkCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(SilkDecoder));
	if (result >= 0)
		result = silkRelease((SilkDecoder *)decoderAddress);
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 275 (and 276): a packet */
int32_t rpcSilkDecode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, decoderMemory, esMemory, pcmMemory;
	uint32_t decoderAddress = 0, es = 0, pcm = 0, esSize, pcmSize;
	VkVThreadContext *context = NULL;
	SilkDecoder *decoder;
	int32_t result;
	SilkCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&decoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	decoderAddress = ctrl->decoder;
	es = ctrl->pEs;
	pcm = ctrl->pPcm;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &decoderAddress,
		sizeof(SilkDecoder));
	if (result >= 0) {
		decoder = (SilkDecoder *)decoderAddress;
		context = decoder->context;
		if (context == NULL) {
			result = SCE_AUDIODEC_ERROR_INVALID_PTR;
		} else {
			decoder->esSize = esSize;
			decoder->pcmSize = pcmSize;
			decoder->sampleRate = ctrl->sampleRate;
			decoder->lostFlag = 0;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&decoderMemory, &decoderAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
		if (result >= 0)
			result = silkRun(context, silkDecodeJob, decoderAddress, es, pcm);
		result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	}
	result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&decoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &decoderAddress,
		sizeof(SilkDecoder));
	if (result >= 0) {
		decoder = (SilkDecoder *)decoderAddress;
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
