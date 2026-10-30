/* SceGps: the context, its mutexes, the acquisition state and the callback queue.
 * Internal names unknown: descriptive names follow the observed behavior.
 */
#include "gps_internal.h"
#include <sysmem/sysevent_ids.h>

#define STATE_ERROR(code)	((code) ? GPS_ERROR(code) : 0)

static GpsQueue s_queue;
static GpsContext s_context;
static GpsPending *s_pending;
static GpsQueue *s_activeQueue;
static GpsContext *s_active;
static GpsPending s_request;

/* the order in which queued notices are delivered: lower first */
static const struct GpsPriority {
	unsigned event, priority;
} s_priorities[] = {
	{ 0x70000, 1 }, { 0x50000, 2 }, { 0x80000, 3 }, { 0x60000, 4 }, { 0x20000, 5 },
	{ 0x10000, 6 }, { 0x40000, 8 }, { 0x30000, 7 }, { 0, 0 }
};

/* the error each API returns in each state (gpsCheckState) */
static const int s_apiErrors[8][9] = {
#define E(code) STATE_ERROR(code)
	{ E(1), E(2), E(0), E(5), E(11), E(5), E(5), E(5), E(11) },
	{ E(1), E(2), E(4), E(0), E(11), E(7), E(7), E(7), E(11) },
	{ E(1), E(2), E(4), E(6), E(11), E(0), E(0), E(0), E(11) },
	{ E(1), E(0), E(0), E(0), E(11), E(7), E(7), E(7), E(11) },
	{ E(1), E(2), E(4), E(6), E(11), E(0), E(0), E(0), E(11) },
	{ E(1), E(2), E(0), E(0), E(11), E(0), E(0), E(0), E(11) },
	{ E(1), E(2), E(0), E(11), E(11), E(11), E(11), E(11), E(11) },
	{ E(1), E(2), E(4), E(0), E(11), E(0), E(0), E(0), E(11) },
#undef E
};

/* Sony inlines these within this file and keeps a copy for the others */
#define INLINE static inline __attribute__((always_inline))

INLINE void lock(unsigned which)
{
	if (!s_active)
		return;
	if (which == GPS_LOCK_API)
		sceKernelLockMutex(s_active->apiMutex, 1, NULL);
	else if (which == GPS_LOCK_DATA)
		sceKernelLockMutex(s_active->dataMutex, 1, NULL);
}

INLINE void unlock(unsigned which)
{
	if (!s_active)
		return;
	if (which == GPS_LOCK_API)
		sceKernelUnlockMutex(s_active->apiMutex, 1);
	else if (which == GPS_LOCK_DATA)
		sceKernelUnlockMutex(s_active->dataMutex, 1);
}

INLINE unsigned state(void)
{
	unsigned state = 0;

	if (s_active) {
		lock(GPS_LOCK_DATA);
		state = s_active->state;
		unlock(GPS_LOCK_DATA);
	}
	return state;
}

INLINE int setState(unsigned state)
{
	if (!s_active)
		return GPS_ERROR(1);
	lock(GPS_LOCK_DATA);
	s_active->state = state;
	unlock(GPS_LOCK_DATA);
	return 0;
}

INLINE int checkState(unsigned api)
{
	unsigned current = state();

	if (s_active->suspended)
		return GPS_ERROR(12);
	return s_apiErrors[api][current];
}

INLINE int signal(unsigned command, unsigned mode)
{
	unsigned flag;

	if (!s_active)
		return GPS_ERROR(1);
	if (command == 2)
		flag = 4;
	else if (command == 7)
		flag = 0x20;
	else if (command == 1) {
		s_active->mode = mode & 15;
		s_active->queuedCallbacks = (mode >> 4) & 1;
		s_active->callbackPending = 0;
		flag = 1;
	} else
		return GPS_ERROR(32);
	sceKernelSetEventFlag(s_active->event, flag);
	return 0;
}

INLINE void removeNotice(GpsQueue *queue, GpsNotice *notice)
{
	GpsNotice *prev = notice->prev, *next = notice->next;

	if (!prev) {
		queue->head = next;
		if (!next) {
			queue->head = queue->tail = NULL;
			return;
		}
		next->prev = prev;
	} else {
		prev->next = next;
		if (!next) {
			queue->tail = prev;
			prev->next = NULL;
		} else
			next->prev = prev;
	}
	if (!queue->head || !queue->tail)
		queue->head = queue->tail = NULL;
}

INLINE void freeHeap(void *p)
{
	if (s_active)
		sceKernelFreeHeapMemory(s_active->heap, p);
}

INLINE void clearNotices(void)
{
	GpsQueue *queue;
	GpsNotice *notice;

	while ((notice = (queue = s_activeQueue)->head)) {
		removeNotice(queue, notice);
		freeHeap(notice);
	}
}

static int systemEvent(int phase, int event, void *data, void *arg)
{
	GpsContext *context = arg;

	if (context) {
		if (!phase && event == SYSEVENT_SUSPEND_START && !context->suspended) {
			context->suspended = 1;
			if (context->state > 2)
				context->state = 2;
		} else if (phase == 1 && event == SYSEVENT_RESUME_COMPLETE) {
			context->setClock = 1;
			context->suspended = 0;
		}
	}
	return 0;
}

static int acquisitionWorker(SceSize arglen, void *argp);

int module_start(SceSize arglen, void *argp)
{
	GpsContext *context = &s_context;
	int ret = 0;

	memset(context, 0, sizeof(*context));
	context->event = sceKernelCreateEventFlag("SceGps", 0, 0, NULL);
	if (context->event < 0)
		goto failed;
	context->apiMutex = sceKernelCreateMutex("SceGpsApi", 2, 0, NULL);
	if (context->apiMutex < 0)
		goto event;
	context->dataMutex = sceKernelCreateMutex("SceGpsData", 2, 0, NULL);
	if (context->dataMutex < 0)
		goto api;
	ret = sceKernelCreateHeap("SceGps", 0x2000, NULL);
	if (ret < 0)
		goto mutex;
	context->heap = ret;
	ret = sceKernelCreateThread("SceGpsd", acquisitionWorker, 0x5f, 0x2000, 0x10000000, 0x80000, NULL);
	if (ret < 0)
		goto heap;
	context->callback = 0;
	context->mode = 0;
	context->queuedCallbacks = context->callbackPending = 0;
	context->state = context->deviceState = 0;
	context->thread = ret;
	ret = sceKernelRegisterSysEventHandler("SceGpsd", systemEvent, context);
	if (ret < 0)
		goto handler;
	context->suspended = 0;
	s_queue.head = s_queue.tail = NULL;
	s_active = context;
	context->setClock = 1;
	context->systemHandler = ret;
	ret = sceKernelInitializeFastMutex(&s_queue.mutex, "SceGps", 2, NULL);
	if (ret < 0)
		goto thread;
	s_activeQueue = &s_queue;
	ret = sceKernelStartThread(context->thread, 4, &s_active);
	if (ret >= 0)
		return ret;
	clearNotices();
	sceKernelFinalizeFastMutex(&s_activeQueue->mutex);
	s_activeQueue = NULL;
thread:
	sceKernelDeleteThread(context->thread);
handler:
	sceKernelUnregisterSysEventHandler(context->systemHandler);
heap:
	sceKernelDeleteHeap(context->heap);
mutex:
	sceKernelDeleteMutex(context->dataMutex);
api:
	sceKernelDeleteMutex(context->apiMutex);
event:
	sceKernelDeleteEventFlag(context->event);
failed:
	resetDevices(0);
	s_active = NULL;
	memset(context, 0, sizeof(*context));
	return ret;
}

/* wake the worker: 1 start (with the mode), 2 stop, 7 the staged ioctl */
int gpsSignal(unsigned command, unsigned mode)
{
	return signal(command, mode);
}

INLINE const struct GpsPriority *findPriority(unsigned event)
{
	for (unsigned i = 0; i < sizeof(s_priorities) / sizeof(s_priorities[0]); i++)
		if (s_priorities[i].event == event)
			return &s_priorities[i];
	return NULL;
}

/* call the callback, or queue the notice while one is being handled */
int gpsNotify(unsigned event, unsigned result)
{
	GpsQueue *queue;
	const struct GpsPriority *entry;
	GpsNotice *notice, *after, *next;
	unsigned priority;
	int ret;

	if (s_active->suspended)
		return GPS_ERROR(12);
	if (!s_active->queuedCallbacks) {
		if (s_active->callback > 0)
			return sceKernelNotifyCallback(s_active->callback, event | (uint8_t)result);
		return s_active->queuedCallbacks;
	}
	lockFastMutex(&s_activeQueue->mutex);
	if (!s_active->callbackPending) {
		ret = s_active->callbackPending;
		if (s_active->callback > 0) {
			ret = sceKernelNotifyCallback(s_active->callback, event | (uint8_t)result);
			s_active->callbackPending = 1;
		}
		goto unlock;
	}
	ret = 0;
	notice = sceKernelAllocHeapMemory(s_active->heap, sizeof(*notice));
	if (!notice)
		goto unlock;
	notice->event = event;
	notice->result = result;
	notice->priority = 0;
	priority = 0;
	entry = findPriority(event);
	if (entry) {
		priority = entry->priority;
		notice->priority = priority;
	}
	queue = s_activeQueue;
	if (!queue->head) {
		queue->head = notice;
		queue->tail = notice;
		notice->prev = NULL;
		notice->next = NULL;
		goto unlock;
	}
	/* after the last notice of a higher priority; one notice per priority */
	for (after = queue->tail; after; after = after->prev) {
		if ((int)after->priority < (int)priority)
			break;
		if (after->priority == priority) {
			if (!priority)
				break;
			freeHeap(notice);
			goto unlock;
		}
	}
	if (!after) {
		next = queue->head;
		queue->head = notice;
		notice->prev = NULL;
		notice->next = next;
		next->prev = notice;
	} else if ((next = after->next)) {
		next->prev = notice;
		after->next = notice;
		notice->prev = after;
		notice->next = next;
	} else {
		after->next = notice;
		notice->prev = after;
		notice->next = NULL;
		queue->tail = notice;
	}
unlock:
	sceKernelUnlockFastMutex(&s_activeQueue->mutex);
	return ret;
}

int gpsSetCallback(SceUID callback)
{
	if (sceKernelGetThreadmgrUIDClass(callback) != 8)
		return GPS_ERROR(33);
	s_active->callback = callback;
	setState(3);
	return 0;
}

int gpsStart(unsigned mode)
{
	int ret;

	lock(GPS_LOCK_DATA);
	ret = checkState(1);
	if (!ret)
		setState(4);
	unlock(GPS_LOCK_DATA);
	if (!ret)
		ret = signal(1, mode);
	return ret;
}

int gpsStop(void)
{
	int ret;

	lock(GPS_LOCK_DATA);
	ret = checkState(2);
	if (!ret)
		setState(8);
	unlock(GPS_LOCK_DATA);
	if (!ret)
		ret = signal(2, 0);
	return ret;
}

int gpsClose(void)
{
	if (!s_active)
		return GPS_ERROR(1);
	clearNotices();
	s_active->callback = 0;
	if (state() == 3)
		setState(2);
	return 0;
}

/* keep a copy of an ioctl's data for the worker */
int gpsStagePending(unsigned command, const void *data, unsigned size)
{
	int ret;

	if (!s_active)
		return GPS_ERROR(1);
	if (size > 0x2000)
		return GPS_ERROR(32);
	lock(GPS_LOCK_DATA);
	ret = GPS_ERROR(11);
	if (!s_pending) {
		ret = GPS_ERROR(35);
		s_request.data = size ? sceKernelAllocHeapMemory(s_active->heap, size) : NULL;
		if (s_request.data) {
			memcpy(s_request.data, data, size);
			s_request.command = command;
			s_request.size = size;
			s_pending = &s_request;
			ret = 0;
		}
	}
	unlock(GPS_LOCK_DATA);
	return ret;
}

int gpsClearPending(void)
{
	GpsPending *pending;

	if (!s_active)
		return GPS_ERROR(1);
	lock(GPS_LOCK_DATA);
	pending = s_pending;
	if (!pending) {
		unlock(GPS_LOCK_DATA);
		return GPS_ERROR(11);
	}
	memset(pending->data, 0, pending->size);
	freeHeap(pending->data);
	s_pending->command = 0;
	s_pending->data = NULL;
	s_pending->size = 0;
	s_pending = NULL;
	unlock(GPS_LOCK_DATA);
	return 0;
}

static int acquisitionWorker(SceSize arglen, void *argp)
{
	GpsContext *context = *(GpsContext **)argp;
	GpsDeviceInfo info;
	unsigned events = 0;
	int ret;

	setState(1);
	for (;;) {
		if (sceKernelWaitEventFlag(context->event, 0xe003007f,
					   SCE_KERNEL_EVF_WAITMODE_OR |
					   SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &events, NULL) < 0)
			return 0;
		if (events & 0x80000000) {
			setState(0);
			return 0;
		}
		if (events & 1) {
			ret = startDevice(context->mode);
			if (!ret) {
				getDeviceInfo(&info);
				if (!(info.flags & 8))
					gpsNotify(0x10000, 0);
			} else
				gpsNotify(0x10000, ((unsigned)ret & 0xffff0000) == 0x80390000 ? ret : GPS_ERROR(0x30));
		}
		if (events & 4) {
			ret = stopDevice(context->mode);
			if (!ret) {
				getDeviceInfo(&info);
				if (!(info.flags & 0x10))
					gpsNotify(0x20000, 0);
			} else
				gpsNotify(0x20000, ((unsigned)ret & 0xffff0000) == 0x80390000 ? ret : GPS_ERROR(0x31));
		}
		if (events & 8) {
			gpsLocationUpdated();
			gpsNotify(0x30000, 0);
		}
		if (events & 0x10)
			gpsNotify(0x40000, 0);
		if (events & 0x40) {
			if (state() == 8) {
				applyStopResult(context->stopResult);
				gpsNotify(0x20000, context->stopResult);
			} else {
				applyStopResult(context->stopResult);
				if (s_active->suspended != 1)
					gpsNotify(0x80000, context->stopResult);
			}
		}
		if (events & 2) {
			if (state() == 4 || deviceState() == 2) {
				lock(GPS_LOCK_DATA);
				if (context->startResult) {
					setDeviceState(0);
					setState(3);
				} else {
					setDeviceState(1);
					setState(5);
				}
				unlock(GPS_LOCK_DATA);
				gpsNotify(0x10000, context->startResult);
			}
		}
		if (events & 0x20) {
			ret = deviceIoctl(s_pending->command, s_pending->data, s_pending->size);
			if (ret < 0) {
				gpsClearPending();
				gpsNotify(0x60000, ((unsigned)ret & 0xffff0000) == 0x80390000 ? ret : GPS_ERROR(0x32));
			}
		}
		if (events & 0x20000000)
			gpsNotify(0x70000, 0);
	}
}

/* deliver the next queued notice once the callback has handled one */
int gpsResumeCallback(void)
{
	GpsQueue *queue;
	GpsNotice *notice;
	unsigned event, result;
	int ret = 0;

	if (!s_active)
		return GPS_ERROR(1);
	lockFastMutex(&s_activeQueue->mutex);
	queue = s_activeQueue;
	if (s_active->callback > 0 && (notice = queue->head)) {
		event = notice->event;
		result = notice->result;
		removeNotice(queue, notice);
		freeHeap(notice);
		ret = sceKernelNotifyCallback(s_active->callback, event | (uint8_t)result);
	}
	s_active->callbackPending = 0;
	sceKernelUnlockFastMutex(&s_activeQueue->mutex);
	return ret;
}

unsigned gpsState(void)
{
	return state();
}

int gpsSetState(unsigned state)
{
	return setState(state);
}

int gpsCheckState(unsigned api)
{
	return checkState(api);
}

int gpsSuspended(void)
{
	return s_active->suspended ? GPS_ERROR(12) : 0;
}

void gpsLock(unsigned which)
{
	lock(which);
}

void gpsUnlock(unsigned which)
{
	unlock(which);
}

GpsContext *gpsContext(void)
{
	return s_active;
}
