/*
 * CELP encoding for the ARM's audio encoder (RPCs 264-269): MPEG-4 CELP at
 * 8 kHz, one stream. A 192-byte control block and the encoder in memory the
 * ARM gives; the engine runs on the stream's VThread context.
 */
#include "application.h"
#include "celp/celp.h"

/* Sony's SDK, audioenc.h */
#define SCE_AUDIOENC_ERROR_INVALID_PTR			((int32_t)0x80860008)

#define CTRL_SIZE			192
#define ENGINE_MEMORY_SIZE		0x1c000
/* the memory the ARM allocates for an encoder */
#define ENCODER_MEMORY_SIZE		0x1c040
#define CONTEXT_JOBS			1
#define DONE_FLAG			0x80000000
#define HEADER_SIZE			32
#define ENCODER_VERSION			0x20000

/* SceAudioencInfoCelp */
typedef struct CelpEncInfo {
	uint32_t size;
	uint32_t excitationMode;
	uint32_t samplingRate;
	uint32_t bitRate;
} CelpEncInfo;

/* SceAudioencOptInfoCelp */
typedef struct CelpEncOptInfo {
	uint32_t size;
	uint8_t header[HEADER_SIZE];
	uint32_t headerSize;
	uint32_t encoderVersion;
} CelpEncOptInfo;

/* the encoder, in memory the ARM gives */
typedef struct CelpEncoder {
	uint32_t engine[ENGINE_MEMORY_SIZE / 4];
	uint32_t pcmSize;		/* the bytes given; then the bytes used */
	uint32_t esSize;		/* the bytes given; then the bytes written */
	VkVThreadContext *context;
	int32_t result;			/* the last job's */
	uint32_t bitRate;
	uint8_t header[HEADER_SIZE];
	uint32_t headerSize;
} CelpEncoder;

/* the ARM's control block */
typedef struct CelpEncCtrl {
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
	CelpEncInfo info;
	uint32_t reserved4[12];
	CelpEncOptInfo optInfo;
	uint32_t reserved5[9];
} CelpEncCtrl;

/* the engine (0x854b9e-) */

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
static int32_t celpEncSetup(CelpEncCtrl *ctrl, VkVThreadResource *resource,
	CelpEncoder *encoder, uint32_t create)
{
	if (create) {
		memset(encoder, 0, 4);
		encoder->context = vkVThreadContextCreate(CONTEXT_JOBS, resource);
	}
	if (encoder->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	return 0;
}

static int32_t celpEncRelease(CelpEncoder *encoder)
{
	if (encoder->context) {
		vkVThreadContextDelete(encoder->context);
		encoder->context = NULL;
	}
	return 0;
}

/* the engine started (or restarted) at the stream's bit rate, on the VThread */
static void celpEncStart(uint32_t encoderAddress, uint32_t create)
{
	VnzExtMemory encoderMemory;
	CelpEncoder *encoder;
	CelpEncoderRate rate;
	int32_t result;

	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	encoder = (CelpEncoder *)encoderAddress;
	if (result < 0)
		goto out;
	if (create)
		result = celpEncoderInit(encoder->engine, ENGINE_MEMORY_SIZE);
	else
		result = celpEncoderReset(encoder->engine);
	if (result < 0)
		goto out;
	rate.mode = -1;
	rate.bitRate = encoder->bitRate;
	result = celpEncoderSetRate(encoder->engine, &rate);
out:
	encoder->result = result;
	vnzExtMemoryClose(&encoderMemory, &encoderAddress);
}

static void celpEncInitJob(uint32_t encoder, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	celpEncStart(encoder, 1);
}

static void celpEncResetJob(uint32_t encoder, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	celpEncStart(encoder, 0);
}

/* a frame; the encoder and its input and output are mapped here, on the VThread */
static void celpEncodeJob(uint32_t encoderAddress, uint32_t pcmAddress, uint32_t esAddress,
	uint32_t argument4)
{
	VnzExtMemory encoderMemory, pcmMemory, esMemory;
	CelpEncoder *encoder;
	int32_t result;

	vnzExtMemoryCreate(&encoderMemory);
	vnzExtMemoryCreate(&pcmMemory);
	vnzExtMemoryCreate(&esMemory);
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	encoder = (CelpEncoder *)encoderAddress;
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
	result = celpEncode(encoder->engine, (const int16_t *)pcmAddress, &encoder->pcmSize,
		(uint8_t *)esAddress, &encoder->esSize);
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

/* the stream's header, its size rounded up to bytes */
static void celpEncHeaderJob(uint32_t encoderAddress, uint32_t argument2, uint32_t argument3,
	uint32_t argument4)
{
	VnzExtMemory encoderMemory;
	CelpEncoder *encoder;
	int32_t result, bits = 0;

	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	encoder = (CelpEncoder *)encoderAddress;
	if (result < 0)
		goto out;
	result = celpEncoderHeader(encoder->engine, encoder->header, HEADER_SIZE, &bits);
	if (result >= 0)
		encoder->headerSize = (bits + 7) >> 3;
out:
	encoder->result = result;
	vnzExtMemoryClose(&encoderMemory, &encoderAddress);
}

/* a job on the context, waited for */
static int32_t celpEncRun(VkVThreadContext *context, VkJobFunction job, uint32_t encoder,
	uint32_t pcm, uint32_t es)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, job, encoder, pcm, es, 0, vnzThreadCurrent(),
		DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 264: the encoder memory the ARM allocates */
int32_t rpcCelpEncSizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((CelpEncCtrl *)ctrlAddress)->encoderSize = ENCODER_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* an encoder made (with its context, on the resource) or restarted */
static int32_t celpEncStartCall(uint32_t ctrlAddress, VkVThreadResource *resource,
	uint32_t create)
{
	VnzExtMemory ctrlMemory, encoderMemory;
	VkVThreadContext *context;
	uint32_t encoderAddress = 0;
	CelpEncoder *encoder;
	CelpEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	if (create && resource == NULL)
		return SCE_AUDIOENC_ERROR_INVALID_PTR;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	encoder = (CelpEncoder *)encoderAddress;
	if (result >= 0)
		result = celpEncSetup(ctrl, resource, encoder, create);
	if (result >= 0) {
		context = encoder->context;
		encoder->bitRate = ctrl->info.bitRate;
	} else {
		context = NULL;
		/* the original does this even when the encoder could not be mapped */
		if (create)
			celpEncRelease(encoder);
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	if (result < 0)
		goto close;
	result = celpEncRun(context, create ? celpEncInitJob : celpEncResetJob, encoderAddress,
		0, 0);
	if (result < 0)
		goto close;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0)
		result = ((CelpEncoder *)encoderAddress)->result;
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 265 */
int32_t rpcCelpEncInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	return celpEncStartCall(ctrlAddress, resource, 1);
}

/* RPC 268 */
int32_t rpcCelpEncReset(uint32_t ctrlAddress)
{
	return celpEncStartCall(ctrlAddress, NULL, 0);
}

/* RPC 266 */
int32_t rpcCelpEncRelease(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, encoderMemory;
	uint32_t encoderAddress = 0;
	CelpEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0)
		result = celpEncRelease((CelpEncoder *)encoderAddress);
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 267: a frame */
int32_t rpcCelpEncode(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, encoderMemory, pcmMemory, esMemory;
	uint32_t encoderAddress = 0, pcm = 0, es = 0, pcmSize, esSize;
	VkVThreadContext *context = NULL;
	CelpEncoder *encoder;
	CelpEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	vnzExtMemoryCreate(&esMemory);
	vnzExtMemoryCreate(&pcmMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	pcm = ctrl->pPcm;
	es = ctrl->pEs;
	pcmSize = ctrl->pcmSize;
	ctrl->pcmSize = 0;
	esSize = ctrl->esSize;
	ctrl->esSize = 0;

	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0) {
		encoder = (CelpEncoder *)encoderAddress;
		context = encoder->context;
		if (context == NULL) {
			result = SCE_AUDIOENC_ERROR_INVALID_PTR;
		} else {
			encoder->pcmSize = pcmSize;
			encoder->esSize = esSize;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&pcmMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &pcm, pcmSize);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&esMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &es, esSize);
		if (result >= 0)
			result = celpEncRun(context, celpEncodeJob, encoderAddress, pcm, es);
		result = keepError(result, vnzExtMemoryClose(&esMemory, &es));
	}
	result = keepError(result, vnzExtMemoryClose(&pcmMemory, &pcm));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0) {
		encoder = (CelpEncoder *)encoderAddress;
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

/* RPC 269: the stream's header (sceAudioencGetOptInfo) */
int32_t rpcCelpEncOptInfo(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, encoderMemory;
	VkVThreadContext *context = NULL;
	uint32_t encoderAddress = 0;
	CelpEncoder *encoder;
	CelpEncCtrl *ctrl;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&encoderMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (CelpEncCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	encoderAddress = ctrl->encoder;
	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0) {
		context = ((CelpEncoder *)encoderAddress)->context;
		if (context == NULL)
			result = SCE_AUDIOENC_ERROR_INVALID_PTR;
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	if (result < 0)
		goto close;
	result = celpEncRun(context, celpEncHeaderJob, encoderAddress, 0, 0);
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&encoderMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &encoderAddress,
		sizeof(CelpEncoder));
	if (result >= 0) {
		encoder = (CelpEncoder *)encoderAddress;
		result = encoder->result;
		if (result >= 0) {
			memcpy(ctrl->optInfo.header, encoder->header, HEADER_SIZE);
			ctrl->optInfo.headerSize = encoder->headerSize;
			ctrl->optInfo.encoderVersion = ENCODER_VERSION;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&encoderMemory, &encoderAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}
