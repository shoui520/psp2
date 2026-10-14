/*
 * SILK encoding for the ARM's audio encoder (RPCs 280-284; the ARM's
 * audioenc type 0x2007, not in the SDK), one stream. A 192-byte control
 * block and the encoder in memory the ARM gives; the SILK SDK's encoder runs
 * on the stream's VThread context.
 */
#include "application.h"

/* Sony's SDK, audioenc.h */
#define SCE_AUDIOENC_ERROR_OUT_OF_MEMORY		((int32_t)0x80860004)
#define SCE_AUDIOENC_ERROR_INVALID_PTR			((int32_t)0x80860008)

#define CTRL_SIZE			192
#define ENGINE_MEMORY_SIZE		0xdd00
/* the memory the ARM allocates for an encoder */
#define ENCODER_MEMORY_SIZE		0xdd40
#define CONTEXT_JOBS			1
#define DONE_FLAG			0x80000000

/* the stream's settings, as the ARM gives them */
typedef struct SilkEncInfo {
	uint32_t size;
	uint32_t sampleRate;
	uint32_t maxInternalSampleRate;
	int32_t bitRate;
	int32_t packetLossPercentage;
	int32_t complexity;
	int32_t useInBandFEC;
} SilkEncInfo;

/* the encoder, in memory the ARM gives */
typedef struct SilkEncoder {
	uint32_t engine[ENGINE_MEMORY_SIZE / 4];
	uint32_t pcmSize;		/* the bytes given */
	uint32_t esSize;		/* the bytes given; then the bytes written */
	VkVThreadContext *context;
	int32_t result;			/* the last job's */
	uint32_t sampleRate;
	uint32_t maxInternalSampleRate;
	int32_t bitRate;
	int32_t packetLossPercentage;
	int32_t complexity;
	int32_t useInBandFEC;
} SilkEncoder;

/* the ARM's control block */
typedef struct SilkEncCtrl {
	uint32_t reserved;
	uint32_t pPcm;
	uint32_t pcmSize;		/* the bytes given; then the bytes used */
	uint32_t pEs;
	uint32_t esSize;		/* the bytes given; then the bytes written */
	uint32_t reserved2;
	uint32_t time;			/* timer ticks the call took */
	int32_t result;
	uint32_t encoder;
	uint32_t encoderSize;
	uint32_t reserved3[2];
	SilkEncInfo info;
	uint32_t reserved4[29];
} SilkEncCtrl;

/* the SILK SDK's encoder (0x858f22-) */
typedef struct SKP_SILK_SDK_EncControlStruct {
	int32_t API_sampleRate;
	int32_t maxInternalSampleRate;
	int32_t packetSize;
	int32_t bitRate;
	int32_t packetLossPercentage;
	int32_t complexity;
	int32_t useInBandFEC;
	int32_t useDTX;
} SKP_SILK_SDK_EncControlStruct;

int32_t SKP_Silk_SDK_Get_Encoder_Size(int32_t *encSizeBytes);
int32_t SKP_Silk_SDK_InitEncoder(void *encState, SKP_SILK_SDK_EncControlStruct *encStatus);
int32_t SKP_Silk_SDK_Encode(void *encState, const SKP_SILK_SDK_EncControlStruct *encControl,
	const int16_t *samplesIn, int32_t nSamplesIn, uint8_t *outData, uint32_t *nBytesOut);

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

/* a new encoder gets its context */
static int32_t silkEncSetup(SilkEncCtrl *ctrl, VkVThreadResource *resource,
	SilkEncoder *encoder, uint32_t create)
{
	if (create) {
		memset(encoder, 0, 4);
		encoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	}
	if (encoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	return 0;
}

static int32_t silkEncRelease(SilkEncoder *encoder)
{
	if (encoder->context) {
		vkVThreadContextDelete(encoder->context);
		encoder->context = NULL;
	}
	return 0;
}

/* the encoder (re)started, on the VThread */
static void silkEncStartJob(uint32_t encoderAddress, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	SKP_SILK_SDK_EncControlStruct status;
	VnzExtMemory encoderMemory;
	SilkEncoder *encoder;
	int32_t result, size;

	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(SilkEncoder));
	encoder = (SilkEncoder *)encoderAddress;
	if (result < 0)
		goto out;
	size = 0;
	result = SKP_Silk_SDK_Get_Encoder_Size(&size);
	if (result < 0)
		goto out;
	if (size < 1 || size > ENGINE_MEMORY_SIZE)
		result = SCE_AUDIOENC_ERROR_OUT_OF_MEMORY;
	else
		result = SKP_Silk_SDK_InitEncoder(encoder->engine, &status);
out:
	encoder->result = result;
	vnzExtMemoryClose(&encoderMemory, &encoderAddress);
}

/* a packet of all the samples given, on the VThread */
static void silkEncodeJob(uint32_t encoderAddress, uint32_t pcmAddress, uint32_t esAddress,
	uint32_t argument4)
{
	VnzExtMemory encoderMemory, pcmMemory, esMemory;
	SKP_SILK_SDK_EncControlStruct control;
	SilkEncoder *encoder;
	int32_t result;

	vnzExtMemoryCreate(&encoderMemory);
	vnzExtMemoryCreate(&pcmMemory);
	vnzExtMemoryCreate(&esMemory);
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(SilkEncoder));
	encoder = (SilkEncoder *)encoderAddress;
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&pcmMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &pcmAddress,
		encoder->pcmSize);
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&esMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &esAddress,
		encoder->esSize);
	if (result < 0)
		goto out;

	memset(&control, 0, sizeof(control));
	control.API_sampleRate = encoder->sampleRate;
	control.maxInternalSampleRate = encoder->maxInternalSampleRate;
	if (encoder->sampleRate < encoder->maxInternalSampleRate)
		control.maxInternalSampleRate = encoder->sampleRate;
	control.packetSize = encoder->pcmSize / 2;
	control.bitRate = encoder->bitRate;
	control.packetLossPercentage = encoder->packetLossPercentage;
	control.complexity = encoder->complexity;
	control.useInBandFEC = encoder->useInBandFEC;
	result = SKP_Silk_SDK_Encode(encoder->engine, &control, (const int16_t *)pcmAddress,
		control.packetSize, (uint8_t *)esAddress, &encoder->esSize);
	if (result < 0) {
		encoder->pcmSize = 0;
		encoder->esSize = 0;
	}
out:
	encoder->result = result;
	vnzExtMemoryClose(&esMemory, &esAddress);
	vnzExtMemoryClose(&pcmMemory, &pcmAddress);
	vnzExtMemoryClose(&encoderMemory, &encoderAddress);
}

/* a job on the context, waited for */
static int32_t silkEncRun(VkVThreadContext *context, VkJobFunction job, uint32_t encoder,
	uint32_t pcm, uint32_t es)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, job, encoder, pcm, es, 0, vnzThreadCurrent(),
		DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 280: the encoder memory the ARM allocates */
int32_t rpcSilkEncSizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((SilkEncCtrl *)ctrlAddress)->encoderSize = ENCODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* an encoder made (with its context, on the resource) or restarted */
static int32_t silkEncStartCall(uint32_t ctrlAddress, VkVThreadResource *resource,
	uint32_t create)
{
	VnzExtMemory ctrlMemory, encoderMemory;
	VkVThreadContext *context;
	uint32_t encoderAddress = 0;
	SilkEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	if (create && resource == NULL)
		return SCE_AUDIOENC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(SilkEncoder));
	if (result >= 0)
		result = silkEncSetup(ctrl, resource, (SilkEncoder *)encoderAddress, create);
	if (result >= 0) {
		context = ((SilkEncoder *)encoderAddress)->context;
	} else {
		context = NULL;
		/* the original does this even when the encoder could not be mapped */
		if (create)
			silkEncRelease((SilkEncoder *)encoderAddress);
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	if (result < 0)
		goto close;
	result = silkEncRun(context, silkEncStartJob, encoderAddress, 0, 0);
	if (result < 0)
		goto close;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &encoderAddress,
		sizeof(SilkEncoder));
	if (result >= 0)
		result = ((SilkEncoder *)encoderAddress)->result;
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 281 */
int32_t rpcSilkEncInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	return silkEncStartCall(ctrlAddress, resource, 1);
}

/* RPC 284 */
int32_t rpcSilkEncReset(uint32_t ctrlAddress)
{
	return silkEncStartCall(ctrlAddress, NULL, 0);
}

/* RPC 282 */
int32_t rpcSilkEncRelease(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, encoderMemory;
	uint32_t encoderAddress = 0;
	SilkEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(SilkEncoder));
	if (result >= 0)
		result = silkEncRelease((SilkEncoder *)encoderAddress);
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 283: a packet */
int32_t rpcSilkEncode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, encoderMemory, pcmMemory, esMemory;
	uint32_t encoderAddress = 0, pcm = 0, es = 0, pcmSize, esSize;
	VkVThreadContext *context = NULL;
	SilkEncoder *encoder;
	SilkEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (SilkEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	pcm = ctrl->pPcm;
	es = ctrl->pEs;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;

	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(SilkEncoder));
	if (result >= 0) {
		encoder = (SilkEncoder *)encoderAddress;
		context = encoder->context;
		if (context == NULL) {
			result = SCE_AUDIOENC_ERROR_INVALID_PTR;
		} else {
			encoder->pcmSize = pcmSize;
			encoder->esSize = esSize;
			encoder->sampleRate = ctrl->info.sampleRate;
			encoder->maxInternalSampleRate = ctrl->info.maxInternalSampleRate;
			encoder->bitRate = ctrl->info.bitRate;
			encoder->packetLossPercentage = ctrl->info.packetLossPercentage;
			encoder->complexity = ctrl->info.complexity;
			encoder->useInBandFEC = ctrl->info.useInBandFEC;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
		if (result >= 0)
			result = silkEncRun(context, silkEncodeJob, encoderAddress, pcm, es);
		result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	}
	result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &encoderAddress,
		sizeof(SilkEncoder));
	if (result >= 0) {
		encoder = (SilkEncoder *)encoderAddress;
		result = encoder->result;
		if (result >= 0) {
			ctrl->pcmSize = encoder->pcmSize;
			ctrl->esSize = encoder->esSize;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}
