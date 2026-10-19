/* SceGps: device registration, acquisition state and callback queue.
 * Internal names unknown: descriptive names follow the observed behavior.
 */
#include "gps_internal.h"
#include <sysmem/sysevent_ids.h>
#include <intrmgr/syscall.h>

#define GPS_ERROR(code) ((int)(0x80390000U | (code)))
GpsGlobals s_gps;
_Static_assert(sizeof(GpsGlobals) == 0x448, "GPS global layout");

/* State transitions are shared by every firmware. */
static const unsigned apiErrors[8][9] = {
	{1,2,0,5,11,5,5,5,11}, {1,2,4,0,11,7,7,7,11},
	{1,2,4,6,11,0,0,0,11}, {1,0,0,0,11,7,7,7,11},
	{1,2,4,6,11,0,0,0,11}, {1,2,0,0,11,0,0,0,11},
	{1,2,0,11,11,11,11,11,11}, {1,2,4,0,11,0,0,0,11}
};

static const unsigned registerErrors[9] = {1,0,0,3,11,3,3,3,11};
static const unsigned startErrors[9] = {1,2,4,6,0,7,7,7,11};
static const unsigned stopErrors[9] = {1,2,4,6,11,7,7,7,0};
static const unsigned resultErrors[9] = {1,2,4,6,11,0,0,0,0};
static const unsigned unregisterErrors[9] = {1,2,0,5,11,7,7,7,11};
static const unsigned publishErrors[9] = {1,2,4,6,11,0,0,0,11};

static int stateError(unsigned code)
{
	return code ? GPS_ERROR(code) : 0;
}

__attribute__((noinline)) void gpsLock(unsigned which)
{
	if (s_gps.active && (which == 1 || which == 2))
		sceKernelLockMutex(which == 1 ? s_gps.active->apiMutex : s_gps.active->dataMutex, 1, NULL);
}

__attribute__((noinline)) void gpsUnlock(unsigned which)
{
	if (s_gps.active && (which == 1 || which == 2))
		sceKernelUnlockMutex(which == 1 ? s_gps.active->apiMutex : s_gps.active->dataMutex, 1);
}

unsigned gpsState(void)
{
	unsigned state = 0;
	if (s_gps.active) {
		gpsDataLock();
		state = s_gps.active->state;
		gpsDataUnlock();
	}
	return state;
}

int gpsSetState(unsigned state)
{
	if (!s_gps.active)
		return GPS_ERROR(1);
	gpsDataLock();
	s_gps.active->state = state;
	gpsDataUnlock();
	return 0;
}

static unsigned deviceState(void)
{
	unsigned state = 0;
	if (s_gps.active) {
		gpsDataLock();
		state = s_gps.active->deviceState;
		gpsDataUnlock();
	}
	return state;
}

static int setDeviceState(unsigned state)
{
	if (!s_gps.active)
		return GPS_ERROR(1);
	gpsDataLock();
	s_gps.active->deviceState = state;
	gpsDataUnlock();
	return 0;
}

static int checkState(unsigned api)
{
	unsigned state = gpsState();
	if (s_gps.active->suspended)
		return GPS_ERROR(12);
	return stateError(apiErrors[api][state]);
}

static int suspended(void)
{
	return s_gps.active->suspended ? GPS_ERROR(12) : 0;
}

static void removeNotice(GpsNotice *notice)
{
	GpsQueue *queue = s_gps.activeQueue;
	if (notice->prev)
		notice->prev->next = notice->next;
	else
		queue->head = notice->next;
	if (notice->next)
		notice->next->prev = notice->prev;
	else {
		queue->tail = notice->prev;
		if (notice->prev)
			notice->prev->next = NULL;
	}
	if (!queue->head || !queue->tail)
		queue->head = queue->tail = NULL;
}

static void clearNotices(void)
{
	GpsNotice *notice;
	while ((notice = s_gps.activeQueue->head)) {
		removeNotice(notice);
		if (s_gps.active)
			sceKernelFreeHeapMemory(s_gps.active->heap, notice);
	}
}

static int notify(unsigned event, unsigned result)
{
	unsigned value = event | (result & 0xff);
	int ret = 0;
	if (s_gps.active->suspended)
		return GPS_ERROR(12);
	if (!s_gps.active->queuedCallbacks)
		return s_gps.active->callback > 0 ? sceKernelNotifyCallback(s_gps.active->callback, value) : 0;
	lockFastMutex(&s_gps.activeQueue->mutex);
	if (!s_gps.active->callbackPending) {
		if (s_gps.active->callback > 0) {
			ret = sceKernelNotifyCallback(s_gps.active->callback, value);
			s_gps.active->callbackPending = 1;
		}
	} else {
		GpsNotice *notice = sceKernelAllocHeapMemory(s_gps.active->heap, sizeof(*notice));
		if (notice) {
			unsigned priority = 0;
			switch (event) {
			case 0x70000: priority = 1; break;
			case 0x50000: priority = 2; break;
			case 0x80000: priority = 3; break;
			case 0x60000: priority = 4; break;
			case 0x20000: priority = 5; break;
			case 0x10000: priority = 6; break;
			case 0x40000: priority = 8; break;
			case 0x30000: priority = 7; break;
			}
			notice->event = event;
			notice->result = result;
			notice->priority = priority;
			GpsQueue *queue = s_gps.activeQueue;
			if (!queue->head) {
				queue->head = queue->tail = notice;
				notice->prev = notice->next = NULL;
			} else {
				GpsNotice *after = queue->tail;
				while (after && after->priority > priority)
					after = after->prev;
				if (after && after->priority == priority && priority) {
					sceKernelFreeHeapMemory(s_gps.active->heap, notice);
				} else if (!after) {
					notice->prev = NULL;
					notice->next = queue->head;
					queue->head->prev = notice;
					queue->head = notice;
				} else {
					notice->prev = after;
					notice->next = after->next;
					if (after->next)
						after->next->prev = notice;
					else
						queue->tail = notice;
					after->next = notice;
				}
			}
		}
	}
	sceKernelUnlockFastMutex(&s_gps.activeQueue->mutex);
	return ret;
}

static int resumeCallback(void)
{
	int ret = 0;
	if (!s_gps.active)
		return GPS_ERROR(1);
	lockFastMutex(&s_gps.activeQueue->mutex);
	GpsNotice *notice = s_gps.activeQueue->head;
	if (s_gps.active->callback > 0 && notice) {
		unsigned event = notice->event, result = notice->result;
		removeNotice(notice);
		if (s_gps.active)
			sceKernelFreeHeapMemory(s_gps.active->heap, notice);
		ret = sceKernelNotifyCallback(s_gps.active->callback, event | (result & 0xff));
	}
	s_gps.active->callbackPending = 0;
	sceKernelUnlockFastMutex(&s_gps.activeQueue->mutex);
	return ret;
}

static int signal(unsigned command, unsigned mode)
{
	if (!s_gps.active)
		return GPS_ERROR(1);
	unsigned flag;
	if (command == 2)
		flag = 4;
	else if (command == 7)
		flag = 0x20;
	else if (command == 1) {
		s_gps.active->mode = mode & 15;
		s_gps.active->queuedCallbacks = (mode >> 4) & 1;
		s_gps.active->callbackPending = 0;
		flag = 1;
	} else
		return GPS_ERROR(32);
	sceKernelSetEventFlag(s_gps.active->event, flag);
	return 0;
}

static int clearPending(void)
{
	if (!s_gps.active)
		return GPS_ERROR(1);
	gpsDataLock();
	GpsPending *pending = s_gps.pending;
	if (!pending) {
		gpsDataUnlock();
		return GPS_ERROR(11);
	}
	memset(pending->data, 0, pending->size);
	if (s_gps.active)
		sceKernelFreeHeapMemory(s_gps.active->heap, pending->data);
	pending->command = 0;
	pending->data = NULL;
	pending->size = 0;
	s_gps.pending = NULL;
	gpsDataUnlock();
	return 0;
}

static int stagePending(unsigned command, const void *data, unsigned size)
{
	if (!s_gps.active)
		return GPS_ERROR(1);
	if (size > 0x2000)
		return GPS_ERROR(32);
	gpsDataLock();
	int ret = GPS_ERROR(11);
	if (!s_gps.pending) {
		ret = GPS_ERROR(35);
		s_gps.request.data = size ? sceKernelAllocHeapMemory(s_gps.active->heap, size) : NULL;
		if (s_gps.request.data) {
			memcpy(s_gps.request.data, data, size);
			s_gps.request.command = command;
			s_gps.request.size = size;
			s_gps.pending = &s_gps.request;
			ret = 0;
		}
	}
	gpsDataUnlock();
	return ret;
}

static int getDeviceInfo(void *info)
{
	if (s_gps.selected - 1U >= 4 || !info)
		return GPS_ERROR(32);
	unsigned *out = info;
	out[0] = s_gps.info[s_gps.selected - 1][0];
	out[1] = s_gps.info[s_gps.selected - 1][1];
	return 0;
}

static int deviceIoctl(unsigned command, void *data, unsigned size)
{
	if (s_gps.selected - 1U >= 4)
		return GPS_ERROR(32);
	GpsDriver *driver = &s_gps.driver[s_gps.selected - 1];
	if (!driver->ioctl)
		return GPS_ERROR(34);
	return driver->ioctl(command, data, size, gpsState());
}

static int specialStop(GpsDriver *driver)
{
	if (!driver)
		return GPS_ERROR(32);
	if (!driver->ioctl)
		return GPS_ERROR(34);
	return driver->ioctl(0x53001000, NULL, 0, gpsState());
}

static int canSetClock(GpsDriver *driver)
{
	unsigned result = 0;
	if (!driver)
		return GPS_ERROR(32);
	if (driver->ioctl && (driver->ioctl(0xa3001000, &result, 4, gpsState()) || !(result & 1)))
		return GPS_ERROR(9);
	return 0;
}

static void clearData(void)
{
	gpsDataLock();
	memset(&s_gps.location, 0, sizeof(s_gps.location));
	memset(&s_gps.satellites, 0, sizeof(s_gps.satellites));
	gpsDataUnlock();
}

static void resetDevices(unsigned device)
{
	gpsDataLock();
	if (!device) {
		memset(s_gps.driver, 0, sizeof(s_gps.driver));
		s_gps.selected = 0;
		for (unsigned i = 0; i < 4; i++)
			s_gps.info[i][0] = s_gps.info[i][1] = 0;
	} else if (device < 5) {
		GpsDriver *driver = &s_gps.driver[device - 1];
		driver->device = driver->flags = 0;
		driver->start = driver->stop = driver->restart = NULL;
		driver->ioctl = NULL;
		s_gps.info[device - 1][0] = s_gps.info[device - 1][1] = 0;
		s_gps.selected = 0;
		for (unsigned i = 0; i < 4; i++)
			if (s_gps.driver[i].device) {
				s_gps.selected = i + 1;
				break;
			}
	}
	memset(&s_gps.location, 0, sizeof(s_gps.location));
	memset(&s_gps.satellites, 0, sizeof(s_gps.satellites));
	gpsDataUnlock();
	gpsSetState(s_gps.selected ? 2 : 1);
}

int SceGpsForDriver_884A5867(const GpsDriver *driver)
{
	if (!driver || !driver->start || !driver->stop || driver->device - 1U >= 4)
		return GPS_ERROR(32);
	unsigned index = driver->device - 1;
	if (s_gps.info[index][0])
		return GPS_ERROR(3);
	GpsDriver *copy = &s_gps.driver[index];
	copy->device = copy->flags = 0;
	copy->start = copy->stop = copy->restart = NULL;
	copy->ioctl = NULL;
	*copy = *driver;
	s_gps.info[index][0] = driver->device;
	s_gps.info[index][1] = driver->flags;
	if (!s_gps.selected)
		s_gps.selected = driver->device;
	int ret = stateError(registerErrors[gpsState()]);
	if (!ret)
		gpsSetState(2);
	return ret;
}

int SceGpsForDriver_54DBA451(unsigned device)
{
	int ret = device - 1U < 4 ? 0 : GPS_ERROR(32);
	/* The original still indexes the table for invalid device numbers. */
	if (!s_gps.driver[device - 1].device)
		ret = GPS_ERROR(2);
	return ret;
}

int SceGpsForDriver_7F001760(unsigned device)
{
	if (device - 1U >= 4)
		return GPS_ERROR(32);
	if (!s_gps.driver[device - 1].device)
		return GPS_ERROR(2);
	int ret = stateError(unregisterErrors[gpsState()]);
	if (ret == GPS_ERROR(1))
		return ret;
	if (ret)
		sceKernelSetEventFlag(s_gps.active->event, 0x20000000);
	resetDevices(device);
	return 0;
}

int SceGpsForDriver_BE3EB4E9(unsigned device, unsigned mode, const GpsLocation *location)
{
	if (!location || s_gps.selected != device || (mode & 15) > 1)
		return GPS_ERROR(32);
	int ret = stateError(publishErrors[gpsState()]);
	if (ret)
		return ret;
	gpsDataLock();
	s_gps.location = *location;
	SceDateTime date = s_gps.location.time;
	uint8_t tick[12];
	unsigned state = gpsState();
	if (device - 1U < 4 && s_gps.active->setClock == 1) {
		int allowed = device == 2 || (device == 1 && !canSetClock(&s_gps.driver[0]));
		if (allowed && state - 6U < 2 && !s_gps.location.noClock &&
		    SceRtcForDriver_F139E359(&date, (SceRtcTick *)tick) >= 0 &&
		    SceRtcForDriver_A7236656((SceRtcTick *)tick) >= 0)
			s_gps.active->setClock = 0;
	}
	gpsDataUnlock();
	sceKernelSetEventFlag(s_gps.active->event, 8);
	return 0;
}

int SceGpsForDriver_8D5688DA(unsigned device, const GpsSatellites *satellites)
{
	if (!satellites || device != s_gps.selected)
		return GPS_ERROR(32);
	int ret = stateError(publishErrors[gpsState()]);
	if (ret)
		return ret;
	gpsDataLock();
	memcpy(&s_gps.satellites, satellites, sizeof(s_gps.satellites));
	gpsDataUnlock();
	sceKernelSetEventFlag(s_gps.active->event, 0x10);
	return 0;
}

int SceGpsForDriver_092AD9BF(unsigned device, const char *sentence, unsigned size)
{
	if (!sentence || device != s_gps.selected)
		return GPS_ERROR(32);
	int ret = stateError(publishErrors[gpsState()]);
	return ret ? ret : gpsParseSentence(sentence, size);
}

int SceGpsForDriver_F2049C02(unsigned type, unsigned event)
{
	unsigned result = event & 255;
	event &= 0xffff0000;
	if (!type) {
		if (event == 0x10000 || event == 0x80000) {
			gpsDataLock();
			if (event == 0x10000)
				s_gps.active->startResult = result;
			else
				s_gps.active->stopResult = result;
			gpsDataUnlock();
			sceKernelSetEventFlag(s_gps.active->event, event == 0x10000 ? 2 : 0x40);
			return 0;
		}
		if (event != 0x60000)
			return 0;
		int ret = clearPending();
		if (ret)
			return ret;
	} else if (type != 0x10000000 && type != 0x20000000 && type != 0x30000000 && type != 0x40000000)
		return 0;
	return notify(event, result);
}

static int startDevice(unsigned mode)
{
	if (s_gps.selected - 1U >= 4)
		return GPS_ERROR(32);
	GpsDriver *driver = &s_gps.driver[s_gps.selected - 1];
	gpsDataLock();
	int ret = stateError(startErrors[gpsState()]);
	if (!(mode & 15)) {
		if (!ret) {
			clearData();
			ret = driver->start ? driver->start() : GPS_ERROR(32);
			if (!ret) {
				if (!(driver->flags & 8)) {
					setDeviceState(1);
					gpsSetState(5);
				}
				gpsDataUnlock();
				return 0;
			}
		}
	} else if ((mode & 15) == 1) {
		int ready = deviceState() == 2;
		if (!ret)
			ready = 1;
		if (ready) {
			ret = driver->restart ? driver->restart() : GPS_ERROR(34);
			if (!ret) {
				if (!(driver->flags & 8)) {
					setDeviceState(1);
					gpsSetState(5);
				}
				gpsDataUnlock();
				return 0;
			}
		}
	} else
		ret = GPS_ERROR(32);
	setDeviceState(0);
	gpsSetState(3);
	gpsDataUnlock();
	return ret;
}

static int stopDevice(unsigned mode)
{
	if (s_gps.selected - 1U >= 4)
		return GPS_ERROR(32);
	GpsDriver *driver = &s_gps.driver[s_gps.selected - 1];
	gpsDataLock();
	int ret = stateError(stopErrors[gpsState()]);
	if (ret)
		goto failure;
	if (!(mode & 15)) {
		ret = driver->stop ? driver->stop() : GPS_ERROR(32);
		clearData();
		if (ret)
			goto failure;
	} else if ((mode & 15) == 1) {
		if (deviceState() == 1 && driver->device == 1) {
			ret = specialStop(driver);
			clearData();
			if (ret)
				goto failure;
		} else {
			if (driver->flags & 0x10) {
				gpsDataLock();
				s_gps.active->stopResult = 0;
				gpsDataUnlock();
				sceKernelSetEventFlag(s_gps.active->event, 0x40);
			}
			clearData();
		}
	} else {
		clearData();
		ret = GPS_ERROR(32);
		goto failure;
	}
	if (!(driver->flags & 0x10)) {
		setDeviceState(0);
		gpsSetState(3);
	}
	gpsDataUnlock();
	return 0;
failure:
	setDeviceState(1);
	gpsSetState(5);
	gpsDataUnlock();
	return ret;
}

static int applyStopResult(unsigned result)
{
	gpsDataLock();
	int ret = stateError(resultErrors[gpsState()]);
	if (!ret) {
		if (gpsState() == 8) {
			setDeviceState(result ? 1 : 0);
			gpsSetState(result ? 5 : 3);
		} else
			setDeviceState(2);
	}
	gpsDataUnlock();
	return ret;
}

static int acquisitionWorker(SceSize arglen, void *argp)
{
	GpsContext *context = *(GpsContext **)argp;
	unsigned events = 0, info[2];
	gpsSetState(1);
	for (;;) {
		if (sceKernelWaitEventFlag(context->event, 0xe003007f,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &events, NULL) < 0)
			return 0;
		if (events & 0x80000000) {
			gpsSetState(0);
			return 0;
		}
		if (events & 1) {
			unsigned ret = startDevice(context->mode);
			if (!ret) {
				getDeviceInfo(info);
				if (!(info[1] & 8))
					notify(0x10000, 0);
			} else
				notify(0x10000, ret >> 16 == 0x8039 ? ret : 0x80390030);
		}
		if (events & 4) {
			unsigned ret = stopDevice(context->mode);
			if (!ret) {
				getDeviceInfo(info);
				if (!(info[1] & 0x10))
					notify(0x20000, 0);
			} else
				notify(0x20000, ret >> 16 == 0x8039 ? ret : 0x80390031);
		}
		if (events & 8) {
			gpsDataLock();
			if (!publishErrors[gpsState()])
				gpsSetState(6);
			gpsDataUnlock();
			notify(0x30000, 0);
		}
		if (events & 0x10)
			notify(0x40000, 0);
		if (events & 0x40) {
			if (gpsState() == 8) {
				applyStopResult(context->stopResult);
				notify(0x20000, context->stopResult);
			} else {
				applyStopResult(context->stopResult);
				if (s_gps.active->suspended != 1)
					notify(0x80000, context->stopResult);
			}
		}
		if (events & 2) {
			if (gpsState() == 4 || deviceState() == 2) {
				gpsDataLock();
				setDeviceState(context->startResult ? 0 : 1);
				gpsSetState(context->startResult ? 3 : 5);
				gpsDataUnlock();
				notify(0x10000, context->startResult);
			}
		}
		if (events & 0x20) {
			unsigned ret = deviceIoctl(s_gps.pending->command, s_gps.pending->data, s_gps.pending->size);
			if ((int)ret < 0) {
				clearPending();
				notify(0x60000, ret >> 16 == 0x8039 ? ret : 0x80390032);
			}
		}
		if (events & 0x20000000)
			notify(0x70000, 0);
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

int module_start(SceSize arglen, void *argp)
{
	GpsContext *context = &s_gps.context;
	int ret = 0;
	memset(context, 0, 0x40);
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
	s_gps.queue.head = s_gps.queue.tail = NULL;
	s_gps.active = context;
	context->setClock = 1;
	context->systemHandler = ret;
	ret = sceKernelInitializeFastMutex(&s_gps.queue.mutex, "SceGps", 2, NULL);
	if (ret < 0)
		goto thread;
	s_gps.activeQueue = &s_gps.queue;
	ret = sceKernelStartThread(context->thread, 4, &s_gps.active);
	if (ret >= 0)
		return ret;
	clearNotices();
	sceKernelFinalizeFastMutex(&s_gps.activeQueue->mutex);
	s_gps.activeQueue = NULL;
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
	s_gps.active = NULL;
	memset(context, 0, 0x40);
	return ret;
}

static int permission(void)
{
	uint8_t mode = 0;
	if (!sceSblACMgrIsSystemProgram(0) && !SceSblPostSsMgrForDriver_128FB35A(0) &&
	    !SceQafMgrForDriver_BFD5E463()) {
		int ret = sceSblPmMgrGetProductMode(&mode);
		if (ret < 0)
			return ret;
		if (mode != 1)
			return (int)SCE_ERROR_ERRNO_ENOSYS;
	}
	return 0;
}

/* Helpers correspond to the kernel-side bodies behind the user-copy wrappers. */
static int openCallback(int callback)
{
	int ret = permission();
	if (ret)
		return ret;
	int uid = SceSysmemForDriver_45D22597(0, callback);
	if (uid < 0)
		return uid | 0x40000000;
	if (!s_gps.active)
		return GPS_ERROR(1);
	ret = checkState(0);
	if (ret)
		return ret;
	gpsLock(1);
	ret = GPS_ERROR(33);
	if (sceKernelGetThreadmgrUIDClass(uid) == 8) {
		s_gps.active->callback = uid;
		gpsSetState(3);
		ret = 0;
	}
	gpsUnlock(1);
	return ret;
}

static int closeCallback(void)
{
	int ret = permission();
	if (ret)
		return ret;
	if (!s_gps.active)
		return GPS_ERROR(1);
	ret = checkState(3);
	if (ret)
		return ret;
	gpsLock(1);
	clearNotices();
	s_gps.active->callback = 0;
	if (gpsState() == 3)
		gpsSetState(2);
	gpsUnlock(1);
	return 0;
}

static int startAcquisition(unsigned mode)
{
	int ret = permission();
	if (ret)
		return ret;
	if ((mode & 15) > 1 || (mode & ~31U))
		return GPS_ERROR(32);
	gpsLock(1);
	gpsDataLock();
	unsigned state = gpsState();
	ret = s_gps.active->suspended ? GPS_ERROR(12) : stateError(apiErrors[1][state]);
	if (!ret)
		gpsSetState(4);
	gpsDataUnlock();
	if (!ret)
		ret = signal(1, mode);
	gpsUnlock(1);
	return ret;
}

static int stopAcquisition(void)
{
	int ret = permission();
	if (ret)
		return ret;
	gpsLock(1);
	gpsDataLock();
	unsigned state = gpsState();
	ret = s_gps.active->suspended ? GPS_ERROR(12) : stateError(apiErrors[2][state]);
	if (!ret)
		gpsSetState(8);
	gpsDataUnlock();
	if (!ret)
		ret = signal(2, 0);
	gpsUnlock(1);
	return ret;
}

static int requestIoctl(unsigned command, void *data, unsigned size)
{
	if (!data || !size)
		return GPS_ERROR(32);
	int ret = permission();
	if (ret)
		return ret;
	ret = checkState(7);
	unsigned info[2];
	if (ret || (ret = getDeviceInfo(info)))
		return ret;
	if ((command & 0xf000) != info[0] * 0x1000)
		return GPS_ERROR(32);
	if ((command & 0xf0000000) == 0xa0000000) {
		/* Sony discards the immediate device result, but retains its output. */
		deviceIoctl(command, data, size);
		return 0;
	}
	if ((command & 0xf0000000) != 0x50000000)
		return GPS_ERROR(34);
	gpsLock(1);
	ret = stagePending(command, data, size);
	if (!ret)
		ret = signal(7, 0);
	gpsUnlock(1);
	return ret;
}

static int readLocation(void *location, void *satellites)
{
	if (!location || !satellites)
		return GPS_ERROR(32);
	int ret = permission();
	if (ret)
		return ret;
	ret = checkState(4);
	if (ret)
		return ret;
	gpsLock(1);
	gpsDataLock();
	/* Byte copies that leave the reserved bytes of the caller's records untouched. */
	uint8_t *to = location;
	const uint8_t *from = (const uint8_t *)&s_gps.location;
	for (unsigned i = 0; i < offsetof(GpsLocation, noClock); i++)
		to[i] = from[i];
	to[offsetof(GpsLocation, noClock)] = s_gps.location.noClock;
	for (unsigned i = offsetof(GpsLocation, latitude); i < sizeof(GpsLocation); i++)
		to[i] = from[i];
	gpsDataUnlock();
	ret = 0;
	if (s_gps.satellites.count > GPS_SATELLITES_MAX)
		ret = GPS_ERROR(32);
	else {
		GpsSatellites *out = satellites;
		gpsDataLock();
		out->count = s_gps.satellites.count;
		for (unsigned i = 0; i < GPS_SATELLITES_MAX; i++)
			for (unsigned j = 0; j < offsetof(GpsSatellite, reserved); j++)
				((uint8_t *)&out->satellite[i])[j] = i < s_gps.satellites.count ?
					((const uint8_t *)&s_gps.satellites.satellite[i])[j] : 0;
		gpsDataUnlock();
	}
	if (s_gps.active->mode == 1 && deviceState() == 2) {
		gpsLock(1);
		ret = signal(1, 1);
		gpsUnlock(1);
	}
	if (gpsState() == 6)
		gpsSetState(7);
	gpsUnlock(1);
	return ret;
}

static int readState(unsigned *state)
{
	if (!state)
		return GPS_ERROR(32);
	int ret = permission();
	if (ret)
		return ret;
	gpsLock(1);
	*state = gpsState();
	ret = suspended();
	gpsUnlock(1);
	return ret;
}

static int readInfo(void *info)
{
	if (!info)
		return GPS_ERROR(32);
	int ret = permission();
	if (ret || (ret = checkState(5)))
		return ret;
	gpsLock(1);
	ret = getDeviceInfo(info);
	gpsUnlock(1);
	return ret;
}

#define SYSCALL_BODY(expression) \
	unsigned saved; \
	saved = sceSyscallEnter(); \
	int ret = (expression); \
	sceSyscallLeave(saved); \
	return ret
int _sceGpsOpen(int callback)
{
	SYSCALL_BODY(openCallback(callback));
}

int _sceGpsClose(void)
{
	SYSCALL_BODY(closeCallback());
}

int _sceGpsStart(unsigned mode)
{
	SYSCALL_BODY(startAcquisition(mode));
}

int _sceGpsStop(void)
{
	SYSCALL_BODY(stopAcquisition());
}

int _sceGpsSelectDevice(unsigned device)
{
	unsigned saved;
	saved = sceSyscallEnter();
	int ret = permission();
	if (!ret && !(ret = checkState(6))) {
		gpsLock(1);
		ret = GPS_ERROR(32);
		if (device - 1U < 4) {
			ret = GPS_ERROR(2);
			if (s_gps.driver[device - 1].device) {
				s_gps.selected = device;
				ret = 0;
			}
		}
		gpsUnlock(1);
	}
	sceSyscallLeave(saved);
	return ret;
}

int _sceGpsResumeCallback(void)
{
	unsigned saved;
	saved = sceSyscallEnter();
	int ret = permission();
	if (!ret) {
		gpsLock(1);
		ret = resumeCallback();
		gpsUnlock(1);
	}
	sceSyscallLeave(saved);
	return ret;
}

int _sceGpsIsDevice(void)
{
	unsigned saved;
	saved = sceSyscallEnter();
	int ret = permission();
	if (!ret) {
		unsigned state = gpsState();
		ret = s_gps.selected - 1U < 4 && state > 1 && s_gps.driver[s_gps.selected - 1].flags != 0;
	}
	sceSyscallLeave(saved);
	return ret;
}

int _sceGpsGetState(unsigned *state)
{
	unsigned saved;
	saved = sceSyscallEnter();
	unsigned value = 0;
	int ret = readState(state ? &value : NULL);
	sceSyscallLeave(saved);
	if (state) {
		int copy = sceKernelCopyToUser(state, &value, 4);
		if (copy < 0)
			ret = copy;
	}
	return ret;
}

int _sceGpsGetDeviceInfo(void *info)
{
	unsigned saved;
	saved = sceSyscallEnter();
	unsigned value[2] = {0, 0};
	int ret = readInfo(info ? value : NULL);
	sceSyscallLeave(saved);
	if (info) {
		int copy = sceKernelCopyToUser(info, value, 8);
		if (copy < 0)
			ret = copy;
	}
	return ret;
}

int _sceGpsGetData(void *location, void *satellites)
{
	uint8_t value[0x3c], satelliteValue[0x104];
	if (location)
		memset(value, 0, sizeof(value));
	if (satellites)
		memset(satelliteValue, 0, sizeof(satelliteValue));
	unsigned saved;
	saved = sceSyscallEnter();
	int ret = readLocation(location ? value : NULL, satellites ? satelliteValue : NULL);
	sceSyscallLeave(saved);
	if (location) {
		int copy = sceKernelCopyToUser(location, value, sizeof(value));
		if (copy < 0)
			return copy;
	}
	if (satellites) {
		int copy = sceKernelCopyToUser(satellites, satelliteValue, sizeof(satelliteValue));
		if (copy < 0)
			ret = copy;
	}
	return ret;
}

int _sceGpsIoctl(unsigned command, void *data, unsigned size, const unsigned *args)
{
	unsigned input[2];
	int ret = sceKernelCopyFromUser(input, args, 8);
	if (ret < 0)
		return ret;
	if (!data) {
		SYSCALL_BODY(requestIoctl(command, NULL, size));
	}
	if (input[0] > 0x640)
		return (int)SCE_KERNEL_ERROR_NO_MEMORY;
	uint8_t buffer[0x640];
	memset(buffer, 0, sizeof(buffer));
	ret = sceKernelCopyFromUser(buffer, data, input[0]);
	if (ret < 0)
		return ret;
	unsigned saved;
	saved = sceSyscallEnter();
	ret = requestIoctl(command, buffer, size);
	sceSyscallLeave(saved);
	int copy = sceKernelCopyToUser(data, buffer, input[0]);
	return copy < 0 ? copy : ret;
}
