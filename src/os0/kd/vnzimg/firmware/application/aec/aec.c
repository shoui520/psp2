/*
 * The echo canceller's RPCs (416-424, 0x84b4b0-0x84bbfc). A 24-byte
 * control block per canceller and the canceller in memory the ARM gives;
 * a call runs on the canceller's VThread context.
 */
#include "aec.h"

/* Sony's SDK, audioin.h */
#define SCE_AUDIO_IN_ERROR_INVALID_SIZE		((int32_t)0x80260102)
#define SCE_AUDIO_IN_ERROR_INVALID_POINTER	((int32_t)0x80260105)

#define CTRL_SIZE			24
#define CURVE_SIZE			64
#define DONE_FLAG			0x80000000

/* the ARM's control block */
typedef struct AecCtrl {
	uint32_t pNear;			/* the microphone, in and out */
	uint32_t pFar;
	uint32_t size;			/* the bytes written */
	uint32_t time;			/* timer ticks the call took */
	uint32_t work;
	uint32_t workSize;
} AecCtrl;

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

/* RPC 416: the memory the ARM allocates */
int32_t rpcAecSizes(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory;
	int32_t result;

	vnzExtMemoryCreate(&ctrlMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result >= 0)
		((AecCtrl *)ctrlAddress)->workSize = AEC_MEMORY_SIZE;
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* a new canceller gets its context */
static int32_t aecSetup(VkVThreadResource *resource, AecWork *work, uint32_t create)
{
	if (create) {
		memset(work, 0, 4);
		work->context = vkVThreadContextCreate(1, resource);
	}
	if (work->context == NULL)
		return VNZ_THREAD_CONTEXT_ERROR_CREATE;
	return 0;
}

static int32_t aecRelease(AecWork *work)
{
	if (work->context) {
		vkVThreadContextDelete(work->context);
		work->context = NULL;
	}
	return 0;
}

/* a canceller made (with its context, on the resource) or reset */
static int32_t aecInit(uint32_t ctrlAddress, VkVThreadResource *resource, uint32_t create)
{
	VnzExtMemory ctrlMemory, workMemory;
	uint32_t workAddress = 0;
	int32_t result;
	AecCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&workMemory);
	if (create && resource == NULL)
		return SCE_AUDIO_IN_ERROR_INVALID_POINTER;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AecCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	workAddress = ctrl->work;
	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &workAddress,
		AEC_WORK_SIZE);
	if (result >= 0)
		result = aecSetup(resource, (AecWork *)workAddress, create);
	/* the original does this even when the work could not be mapped */
	if (result < 0 && create)
		aecRelease((AecWork *)workAddress);
	result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	if (result < 0)
		goto close;
	/* not a job: the reset runs here, on the closed mapping */
	aecReset((AecWork *)workAddress);
	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &workAddress,
		AEC_WORK_SIZE);
	if (result >= 0)
		result = ((AecWork *)workAddress)->result;
	result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 417 */
int32_t rpcAecInit(uint32_t ctrlAddress, VkVThreadResource *resource)
{
	return aecInit(ctrlAddress, resource, 1);
}

/* RPC 419 */
int32_t rpcAecReset(uint32_t ctrlAddress)
{
	return aecInit(ctrlAddress, NULL, 0);
}

/* RPC 418 */
int32_t rpcAecRelease(uint32_t ctrlAddress)
{
	VnzExtMemory ctrlMemory, workMemory;
	uint32_t workAddress = 0;
	int32_t result;
	AecCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&workMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AecCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	workAddress = ctrl->work;
	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &workAddress,
		AEC_WORK_SIZE);
	if (result >= 0)
		result = aecRelease((AecWork *)workAddress);
	result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* a call; the canceller and the two signals are mapped here, on the VThread */
static void aecJob(uint32_t workAddress, uint32_t nearAddress, uint32_t farAddress,
	uint32_t argument4)
{
	VnzExtMemory workMemory, nearMemory, farMemory;
	AecWork *work;
	int32_t result;

	vnzExtMemoryCreate(&workMemory);
	vnzExtMemoryCreate(&nearMemory);
	vnzExtMemoryCreate(&farMemory);
	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &workAddress,
		AEC_WORK_SIZE);
	work = (AecWork *)workAddress;
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&nearMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &nearAddress,
		work->size);
	if (result < 0)
		goto out;
	result = vnzExtMemoryOpen(&farMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &farAddress,
		work->size);
	if (result < 0)
		goto out;
	result = aecProcess(work, (int16_t *)nearAddress, (const int16_t *)farAddress);
out:
	work->result = result;
	vnzExtMemoryClose(&farMemory, &farAddress);
	vnzExtMemoryClose(&nearMemory, &nearAddress);
	vnzExtMemoryClose(&workMemory, &workAddress);
}

/* the call on the context, waited for */
static int32_t aecRun(uint32_t work, VkVThreadContext *context, uint32_t near, uint32_t far)
{
	VkVThreadContext *locked = vkVThreadContextLock(context);

	vkVThreadJobSetNotify(locked, 0, 0, aecJob, work, near, far, 0, vnzThreadCurrent(),
		DONE_FLAG);
	vkVThreadContextUnlock(locked);
	vnzThreadWaitFlags(DONE_FLAG);
	return 0;
}

/* RPC 420: 512 samples of the microphone, against 512 of the far end */
int32_t rpcAecProcess(uint32_t ctrlAddress, int32_t delay, uint32_t flags)
{
	VnzExtMemory ctrlMemory, workMemory, nearMemory, farMemory;
	uint32_t workAddress = 0, near = 0, far = 0;
	VkVThreadContext *context = NULL;
	AecWork *work;
	int32_t result;
	AecCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&workMemory);
	vnzExtMemoryCreate(&nearMemory);
	vnzExtMemoryCreate(&farMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AecCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	workAddress = ctrl->work;
	near = ctrl->pNear;
	far = ctrl->pFar;
	ctrl->size = 0;

	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &workAddress,
		AEC_WORK_SIZE);
	if (result >= 0) {
		work = (AecWork *)workAddress;
		context = work->context;
		if (context == NULL) {
			result = SCE_AUDIO_IN_ERROR_INVALID_POINTER;
		} else {
			aecSetDelay(work, (int16_t)delay, flags);
			work->size = AEC_HALF * 2;
		}
	}
	result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&nearMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &near, AEC_HALF * 2);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&farMemory, 0, VNZ_MEMORY_MODE_PUBLIC, &far,
			AEC_HALF * 2);
		if (result >= 0)
			result = aecRun(workAddress, context, near, far);
		result = keepError(result, vnzExtMemoryClose(&farMemory, &far));
	}
	result = keepError(result, vnzExtMemoryClose(&nearMemory, &near));
	if (result < 0)
		goto close;

	result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &workAddress,
		AEC_WORK_SIZE);
	if (result >= 0) {
		result = ((AecWork *)workAddress)->result;
		if (result >= 0)
			ctrl->size = ((AecWork *)workAddress)->size;
	}
	result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 421: the microphone's gain curve, 16 points */
int32_t rpcAecSetCurve(uint32_t ctrlAddress, uint32_t curve)
{
	VnzExtMemory ctrlMemory, workMemory, curveMemory;
	uint32_t workAddress = 0;
	int32_t result;
	AecCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&workMemory);
	vnzExtMemoryCreate(&curveMemory);
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AecCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	workAddress = ctrl->work;
	result = vnzExtMemoryOpen(&curveMemory, 0, VNZ_MEMORY_MODE_PROTECTED, &curve, CURVE_SIZE);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PRIVATE, &workAddress,
			AEC_WORK_SIZE);
		if (result >= 0) {
			if (((AecWork *)workAddress)->context)
				aecSetCurve((AecWork *)workAddress, (const uint8_t *)curve);
			else
				result = SCE_AUDIO_IN_ERROR_INVALID_POINTER;
		}
		result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	}
	result = keepError(result, vnzExtMemoryClose(&curveMemory, &curve));
	if (result >= 0)
		ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}

/* RPC 424: the bins' state copied out */
int32_t rpcAecGetState(uint32_t ctrlAddress, uint32_t dst, uint32_t size)
{
	VnzExtMemory ctrlMemory, workMemory, dstMemory;
	uint32_t workAddress = 0;
	int32_t result;
	AecCtrl *ctrl;

	vnzExtMemoryCreate(&ctrlMemory);
	vnzExtMemoryCreate(&workMemory);
	vnzExtMemoryCreate(&dstMemory);
	if (size != AEC_STATE_SIZE)
		return SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	result = vnzExtMemoryOpen(&ctrlMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &ctrlAddress, CTRL_SIZE);
	if (result < 0)
		goto close;
	ctrl = (AecCtrl *)ctrlAddress;
	ctrl->time = timerRead();
	workAddress = ctrl->work;
	result = vnzExtMemoryOpen(&dstMemory, 0, VNZ_MEMORY_MODE_PRIVATE, &dst, size);
	if (result >= 0) {
		result = vnzExtMemoryOpen(&workMemory, 1, VNZ_MEMORY_MODE_PROTECTED, &workAddress,
			AEC_WORK_SIZE);
		if (result >= 0)
			memcpy((void *)dst, (const void *)workAddress, AEC_STATE_SIZE);
		result = keepError(result, vnzExtMemoryClose(&workMemory, &workAddress));
	}
	result = keepError(result, vnzExtMemoryClose(&dstMemory, &dst));
	if (result >= 0)
		ctrl->time = timerRead() - ctrl->time;
close:
	return keepError(result, vnzExtMemoryClose(&ctrlMemory, &ctrlAddress));
}
