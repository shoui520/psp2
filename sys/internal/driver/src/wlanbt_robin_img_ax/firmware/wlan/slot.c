/*
 * Channel-time request scheduler. The filename is inferred; no source
 * filename survives.
 */
#include "robin/slot.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/wlantask.h"

struct WlanSlotScheduler wlanSlotScheduler __attribute__((section(".dtcm.bss")));
static struct OsaSemaphore *wlanSlotLockHandle __attribute__((section(".dtcm.bss")));
static uint32_t wlanSlotLockObject[7] __attribute__((section(".dtcm.bss")));

/* Unconverted callees. */
void wlanReportEvent(struct WlanConnection *connection, const void *data, int code);	/* 0x10316 */
int macTxPostEvent(uint32_t events);							/* 0x9956 */
int macMgmtUpdateContext(int a, int b, int c, int d);					/* 0x355d2 */
void macMgmtValidateContext(void);							/* 0x3565e */
void macMgmtCheckContext(void);								/* 0x3564a */
int macMgmtSyncContext(struct WlanConnection *connection);				/* 0x3576a */
void uapSqramRecordEntry(unsigned int posture);						/* SQRAM via 0x52d3c */

#define SCHED	wlanSlotScheduler

/*
 * Submits a request, or is the timer callback that does so after the request
 * delay; the timer must be the one stored in the request. A request that finds
 * another one running waits in the FIFO.
 */
int wlanSlotStart(uint32_t timer, struct WlanSlotRequest *request)
{
	if (request->handle == 0) {
		++SCHED.rejectedInvalid;
		return SCHED.rejectedInvalid;
	}
	if (request->timer != timer) {
		++SCHED.rejectedToken;
		return SCHED.rejectedToken;
	}
	if (SCHED.current == NULL) {
		SCHED.current = request;
		request->state = WLAN_SLOT_CURRENT;
	} else if (SCHED.current != request) {
		++SCHED.queued;
		request->state = WLAN_SLOT_QUEUED;
		if (SCHED.tail != NULL)
			SCHED.tail->next = request;
		else
			SCHED.head = request;
		SCHED.tail = request;
		request->next = NULL;
		return 0;
	}
	SCHED.current->timer = 0;
	return osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
}

/* Timer: the window of the running request is over. */
void wlanSlotWindowExpired(uint32_t timer, struct WlanSlotRequest *request)
{
	if (request->handle != 0 && SCHED.window == timer) {
		if (SCHED.current != NULL && SCHED.current == request) {
			SCHED.window = 0;
			osaEventFlagsSet(macMgmtEventFlags, 0x80, 7);
		}
	}
}

/* Cancels a request that is not the current one. */
int wlanSlotCancel(struct WlanSlotRequest *request)
{
	int result = WLAN_SLOT_FAILED;
	UINT posture = sysIrqDisable();

	if (SCHED.current != request) {
		result = WLAN_SLOT_OK;
		if (request->state == WLAN_SLOT_QUEUED) {
			request->state = WLAN_SLOT_CANCELLED;
			request->callback(request->handle, WLAN_SLOT_EVENT_CANCEL, request->argument);
		} else if (osaCancelTimer(request->timer) == 1) {
			request->callback(request->handle, WLAN_SLOT_EVENT_CANCEL, request->argument);
			request->timer = 0;
			request->handle = 0;
			request->state = WLAN_SLOT_IDLE;
		}
	}
	sysIrqRestore(posture);
	return result;
}

/* Reports the context, handle and argument of the running request. */
void wlanSlotGetRunning(uint32_t *context, uint32_t *handle, uint32_t *argument)
{
	UINT posture;

	*context = 0;
	posture = sysIrqDisable();
	if (SCHED.current != NULL && SCHED.current->state == WLAN_SLOT_RUNNING &&
	    SCHED.current->context != 0) {
		*context = SCHED.current->context;
		*handle = SCHED.current->handle;
		*argument = SCHED.current->argument;
	}
	sysIrqRestore(posture);
}

/* Asks the running request how it is doing. */
int wlanSlotPoll(void)
{
	++SCHED.polls;
	if (SCHED.current != NULL) {
		++SCHED.activePolls;
		return SCHED.current->callback(SCHED.current->handle, WLAN_SLOT_EVENT_POLL,
					       SCHED.current->argument);
	}
	return 0;
}

/* Lets a request that waits for the state semaphore continue. */
int wlanSlotResume(void)
{
	if (SCHED.current != NULL && SCHED.current->phase == 3) {
		wlanReleaseStateSemaphore();
		SCHED.current->phase = 1;
		return osaEventFlagsSet(macMgmtEventFlags, 0x40, 7);
	}
	return 0;
}

int wlanSlotNotifyTick(void)
{
	if (SCHED.current != NULL && SCHED.current->state == WLAN_SLOT_RUNNING)
		return SCHED.current->callback(SCHED.current->handle, WLAN_SLOT_EVENT_TICK,
					       SCHED.current->argument);
	return WLAN_SLOT_FAILED;
}

/* Clears the scheduler, creates its lock and resets the channel-access queue 4. */
int wlanSlotInit(void)
{
	__aeabi_memclr4(&SCHED, sizeof(SCHED));
	wlanSlotLockHandle = (struct OsaSemaphore *)wlanSlotLockObject;
	SCHED.lock = wlanSlotLockHandle;
	osaSemaphoreInit(&SCHED.lock, 1, 12);
	romEdcaSetCwMin(4, 0);
	romEdcaSetCwMax(4, 0);
	romEdcaSetAifsn(4, 0);
	return romEdcaSetTxop(4, 0);
}

int wlanSlotHasCurrent(void)
{
	return SCHED.current != NULL;
}

int wlanSlotLock(void)
{
	return osaSemaphoreWait(SCHED.lock, 0xffffffff);
}

int wlanSlotUnlock(void)
{
	return osaSemaphoreRelease(SCHED.lock);
}

/* Takes the next request off the FIFO, dropping cancelled ones. */
struct WlanSlotRequest *wlanSlotPopNext(void)
{
	struct WlanSlotRequest *request;

	for (;;) {
		request = SCHED.head;
		if (request == NULL)
			return NULL;
		++SCHED.dequeued;
		SCHED.head = request->next;
		if (SCHED.head == NULL)
			SCHED.tail = NULL;
		if (request->state != WLAN_SLOT_CANCELLED)
			return request;
		++SCHED.cancelledQueued;
		request->timer = 0;
		request->handle = 0;
		request->state = WLAN_SLOT_IDLE;
	}
}

struct WlanSlotRequest *wlanSlotFind(uint32_t handle)
{
	unsigned int i;

	for (i = 0; i < WLAN_SLOT_REQUESTS; i++) {
		if (SCHED.requests[i].handle == handle)
			return &SCHED.requests[i];
	}
	return NULL;
}

/* Cancels every request whose flags carry bit 1 (first) or bit 0 (second). */
int wlanSlotCancelByFlags(int first, int second)
{
	struct WlanSlotRequest *request;
	unsigned int i;

	wlanSlotLock();
	for (i = 0; i < WLAN_SLOT_REQUESTS; i++) {
		request = &SCHED.requests[i];
		if (request->handle != 0 &&
		    ((first != 0 && (request->flags & 2)) || (second != 0 && (request->flags & 1))))
			wlanSlotCancel(request);
	}
	return wlanSlotUnlock();
}

int wlanSlotCancelFlag0(void)
{
	return wlanSlotCancelByFlags(0, 1);
}

int wlanSlotHasCurrentAlias(void)
{
	return SCHED.current != NULL;
}

/* Ends the current request, reports it and starts the next one after a short delay. */
int wlanSlotFinish(void)
{
	UINT posture;

	macTxPostEvent(1);
	SCHED.current->callback(SCHED.current->handle, SCHED.current->phase, SCHED.current->argument);
	if (SCHED.current->connection->interfaceType != WLAN_IF_AMP)
		wlanReleaseStateSemaphore();
	wlanSlotLock();
	posture = sysIrqDisable();
	SCHED.current->handle = 0;
	wlanSlotUnlock();
	SCHED.current = wlanSlotPopNext();
	uapSqramRecordEntry(sysIrqRestore(posture));
	if (SCHED.current != NULL)
		osaCreateTimer((OsaTimerCallback)wlanSlotStart, (uint32_t)SCHED.current, 100000,
			       &SCHED.current->timer, SCHED.current->timerFlags);
	return WLAN_SLOT_OK;
}

/* Changes the duration of the current request and restarts the timer that ends it. */
int wlanSlotSetDuration(uint32_t handle, uint32_t duration)
{
	int result = WLAN_SLOT_UNKNOWN;
	struct WlanSlotRequest *request = wlanSlotFind(handle);

	if (SCHED.current == request) {
		if (request->timer != 0) {
			osaCancelTimer(request->timer);
			request->timer = 0;
		}
		request->duration = duration;
		if (duration != 0) {
			result = WLAN_SLOT_OK;
			if (osaCreateTimer((OsaTimerCallback)wlanSlotStart, (uint32_t)request,
					   duration, &request->timer, 0) == 0)
				return WLAN_SLOT_FAILED;
		}
	}
	return result;
}

/* Stops the timer of the current request. */
int wlanSlotClearTimer(uint32_t handle)
{
	int result = WLAN_SLOT_UNKNOWN;
	struct WlanSlotRequest *request = wlanSlotFind(handle);

	if (SCHED.current == request && request->timer != 0) {
		osaCancelTimer(request->timer);
		result = WLAN_SLOT_OK;
		request->timer = 0;
	}
	return result;
}

/*
 * The running request registers a second callback in its context field; it
 * takes the request handle, three words from the caller, a fourth, and the
 * request's argument.
 */
typedef int (*WlanSlotRunningCallback)(uint32_t handle, uint32_t event, uint32_t first,
				       uint32_t second, uint32_t third, uint32_t argument);

static int wlanSlotCallRunning(uint32_t event, uint32_t first, uint32_t second, uint32_t third)
{
	uint32_t context = 0;
	uint32_t handle = 0;
	uint32_t argument = 0;
	int result = 0;

	wlanSlotGetRunning(&context, &handle, &argument);
	if (context != 0)
		result = ((WlanSlotRunningCallback)context)(handle, event, first, second, third,
							    argument);
	return result;
}

int wlanSlotRunningNotify(uint32_t first, uint32_t third)
{
	return wlanSlotCallRunning(0, first, 0, third);
}

int wlanSlotRunningResult(uint32_t second, uint32_t third)
{
	return wlanSlotCallRunning(0, 0, second, third);
}

int wlanSlotRunningEvent(uint32_t event)
{
	return wlanSlotCallRunning(event, 0, 0, 0);
}

/* Arms (or, with a zero delay, disarms) the window timer of the current request. */
int wlanSlotSetWindow(uint32_t handle, uint32_t delay)
{
	int result = WLAN_SLOT_UNKNOWN;
	struct WlanSlotRequest *request = wlanSlotFind(handle);

	if (SCHED.current == request) {
		if (SCHED.window != 0) {
			osaCancelTimer(SCHED.window);
			SCHED.window = 0;
		}
		if (delay != 0) {
			result = WLAN_SLOT_OK;
			if (osaCreateTimer((OsaTimerCallback)wlanSlotWindowExpired, (uint32_t)request,
					   delay, &SCHED.window, 0) == 0)
				return WLAN_SLOT_FAILED;
		}
	}
	return result;
}

int wlanSlotClearWindow(uint32_t handle)
{
	if (SCHED.current == wlanSlotFind(handle)) {
		if (SCHED.window != 0) {
			osaCancelTimer(SCHED.window);
			SCHED.window = 0;
		}
		return WLAN_SLOT_OK;
	}
	return WLAN_SLOT_UNKNOWN;
}

/* Terminates a request with a reason code (7, 8 or any other). */
int wlanSlotAbort(uint32_t handle, int code)
{
	struct WlanSlotRequest *request = wlanSlotFind(handle);
	unsigned int event;

	if (request == NULL)
		return WLAN_SLOT_UNKNOWN;
	if (request->timer != 0) {
		osaCancelTimer(request->timer);
		request->timer = 0;
	}
	if (SCHED.current != request) {
		request->handle = 0;
		return WLAN_SLOT_OK;
	}
	wlanSlotLock();
	if (request->state == WLAN_SLOT_CURRENT || request->state == WLAN_SLOT_RUNNING) {
		request->state = WLAN_SLOT_IDLE;
		wlanSlotUnlock();
		SCHED.current->phase = code;
		if (code == 7) {
			wlanReportEvent(SCHED.current->connection, NULL, 25);
			event = 44;
		} else if (code == 8) {
			wlanReportEvent(SCHED.current->connection, NULL, 26);
			event = 45;
		} else {
			macMgmtSyncContext(SCHED.current->connection);
			wlanReportEvent(SCHED.current->connection, NULL, 24);
			event = 43;
		}
		wlanRaiseEvent(event);
	} else {
		wlanSlotUnlock();
	}
	return WLAN_SLOT_OK;
}

void wlanSlotCancelById(uint32_t handle)
{
	struct WlanSlotRequest *request;

	wlanSlotLock();
	request = wlanSlotFind(handle);
	if (request != NULL)
		wlanSlotCancel(request);
	wlanSlotUnlock();
}

int wlanSlotSetChannel(int channel, int band)
{
	return macMgmtUpdateContext(channel, band, 0, 0) == 0;
}

/* The current request gets the channel: switches, reports and calls back. */
int wlanSlotBegin(void)
{
	macMgmtValidateContext();
	wlanReportEvent(SCHED.current->connection, NULL, 22);
	wlanReportEvent(SCHED.current->connection, NULL, 21);
	if (SCHED.current->mode != 0)
		wlanSlotSetChannel(SCHED.current->bssType, SCHED.current->mode);
	SCHED.current->state = WLAN_SLOT_RUNNING;
	if (SCHED.current->elapsed != NULL)
		*SCHED.current->elapsed = (uint32_t)romGetTimerTimestamp() - SCHED.current->startTime;
	SCHED.current->callback(SCHED.current->handle, WLAN_SLOT_EVENT_START, SCHED.current->argument);
	if (SCHED.current->duration != 0 && SCHED.current->timer == 0)
		osaCreateTimer((OsaTimerCallback)wlanSlotStart, (uint32_t)SCHED.current,
			       SCHED.current->duration, &SCHED.current->timer, 0);
	return WLAN_SLOT_OK;
}

/* Asks the current request whether the channel switch may happen; 2 if it declines. */
int wlanSlotQueryStart(int power)
{
	int result;
	unsigned int event;

	macMgmtCheckContext();
	result = SCHED.current->callback(SCHED.current->handle, SCHED.current->phase,
					 SCHED.current->argument);
	if (result != WLAN_SLOT_OK)
		return 2;
	if (power != 0) {
		wlanReportEvent(SCHED.current->connection, NULL, 22);
		event = 41;
	} else {
		wlanReportEvent(SCHED.current->connection, NULL, 23);
		event = 42;
	}
	wlanRaiseEvent(event);
	return result;
}
