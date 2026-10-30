/*
 * Low-level cyclics (Sony's dtrace/sce/solaris_cyclic_low.c): a thread waits on
 * its callback, which a thread manager timer notifies at each interval; an event
 * flag tells the thread to stop.
 */
#include <dtrace/logging.h>
#include <dtrace/heap.h>
#include <dtrace/sce/solaris_cyclic.h>
#include <kernel/error.h>
#include <sysmem/debug.h>
#include <threadmgr/threadmgr.h>

#define CYCLIC_LOW_EVF_STOP	0x1

static int _cyclic_low_callback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	cyc_handler_t *hdlr = common;

	if (hdlr != NULL)
		hdlr->cyh_func(hdlr->cyh_arg);

	return 0;
}

static int _cyclic_low_thread(SceSize args, void *argp)
{
	cyclic_id_t id = *(cyclic_id_t *)argp;
	uint32_t outBits;
	SceUID cb;
	int res;

	cb = sceKernelCreateCallback("SceDeci4pDtracepCyclicLowCallback", 0, _cyclic_low_callback,
				     &id->cy_handler);
	if (cb < 0) {
		DTRACE_LOG_ERROR_AT(52, "failed to sceKernelCreateCallback (0x%08x)", cb);
		return cb;
	}

	res = sceKernelRegisterCallbackToEventForDriver(id->cy_timer, cb, cb);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(60, "failed to sceKernelRegisterCallbackToEventForDriver %p (0x%08x)",
				    id, res);
		return res;
	}

	res = sceKernelWaitEventFlagCB(id->cy_eventflag, CYCLIC_LOW_EVF_STOP,
				       SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				       &outBits, NULL);
	if (res < 0)
		DTRACE_LOG_ERROR_AT(72, "failed to sceKernelWaitEventFlagCB %p (0x%08x)", id, res);

	sceKernelStopTimer(id->cy_timer);
	sceKernelDeleteTimer(id->cy_timer);
	sceKernelDeleteCallback(cb);
	sceKernelDeleteEventFlag(id->cy_eventflag);
	return 0;
}

cyclic_id_t _cyclic_low_alloc(cyc_handler_t *hdlr, cyc_time_t *when, uint32_t cpumask)
{
	cyclic_id_t id;

	id = dtrace_heap_zalloc(sizeof(*id));
	if (id == NULL) {
		DTRACE_LOG_ERROR_AT(92, "failed to dtrace_heap_zalloc");
		return NULL;
	}

	id->cy_handler = *hdlr;
	id->cy_when = *when;
	id->cy_cpumask = cpumask;
	id->cy_timer = -1;
	id->cy_thread = -1;
	id->cy_eventflag = -1;
	return id;
}

void _cyclic_low_free(cyclic_id_t id)
{
	if (id == NULL)
		return;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level <= CY_LOCK_LEVEL, 114);
	dtrace_heap_free(id);
}

int _cyclic_low_init(cyclic_id_t id)
{
	SceKernelThreadOptParam opt;
	SceUInt64 interval;
	SceUID uid;
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	uid = sceKernelCreateTimer("SceDeci4pDtracepCyclicLowTimer",
				   SCE_KERNEL_ATTR_NOTIFY_CB_WAKEUP_ONLY | SCE_KERNEL_EVENT_ATTR_AUTO_RESET,
				   NULL);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(137, "failed to sceKernelCreateTimer %p timer (0x%x)", id, uid);
		id->cy_timer = -1;
		return uid;
	}
	id->cy_timer = uid;

	interval = (SceUInt64)id->cy_when.cyt_interval / 1000;
	res = sceKernelSetTimerEvent(uid, SCE_KERNEL_TIMER_TYPE_PULSE_EVENT, &interval, 1);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(149, "failed to sceKernelSetTimerEvent %p timer event (0x%08xx)",
				    id, res);
		return res;
	}

	opt = (SceKernelThreadOptParam){
		.size = sizeof(opt),
		.attr = 1,
		.kStackMemType = 0x10f0d006,
	};
	uid = sceKernelCreateThread("SceDeci4pDtracepCyclicLowThread", _cyclic_low_thread, 16,
				    0x4000, 0, id->cy_cpumask, &opt);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(169, "failed to sceKernelCreateThread %p (0x%08x)", id, uid);
		id->cy_thread = -1;
		return uid;
	}
	id->cy_thread = uid;

	uid = sceKernelCreateEventFlag("SceDeci4pDtracepCyclicLowEventFlag", SCE_KERNEL_ATTR_MULTI,
				       0, NULL);
	if (uid < 0) {
		DTRACE_LOG_ERROR_AT(182, "failed to sceKernelCreateEventFlag %p (0x%08x)", id, uid);
		return 0;
	}
	id->cy_eventflag = uid;

	res = sceKernelStartThread(id->cy_thread, sizeof(id), &id);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(189, "failed to sceKernelStartThread %p (0x%08x)", id, res);
		return res;
	}

	return 0;
}

int _cyclic_low_start(cyclic_id_t id)
{
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level <= CY_LOCK_LEVEL, 206);
	res = sceKernelStartTimer(id->cy_timer);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(211, "failed to sceKernelStartTimer %p (0x%08x)", id, res);
		return res;
	}

	return res;
}

int _cyclic_low_stop(cyclic_id_t id)
{
	int res;

	if (id == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level <= CY_LOCK_LEVEL, 227);
	res = sceKernelStopTimer(id->cy_timer);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(232, "failed to sceKernelStopTimer %p (0x%08x)", id, res);
		return res;
	}

	return res;
}

void _cyclic_low_remove(cyclic_id_t id)
{
	int status = 0;

	if (id == NULL)
		return;

	SCE_DBG_ASSERT(id->cy_handler.cyh_level <= CY_LOCK_LEVEL, 248);
	sceKernelSetEventFlag(id->cy_eventflag, CYCLIC_LOW_EVF_STOP);
	sceKernelWaitThreadEnd(id->cy_thread, &status, NULL);
	sceKernelDeleteThread(id->cy_thread);
	_cyclic_low_free(id);
}
