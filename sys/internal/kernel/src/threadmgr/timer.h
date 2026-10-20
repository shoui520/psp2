/*
 * SceKernelThreadMgr: timer.c's objects. The CPU timers
 * (SceUIDCpuTimerClass, a subclass of SceUIDEventClass) sit in one queue by
 * expiry, served by the LT5 hardware timer's interrupt; delays (timed
 * waits), alarms (kernel callbacks) and timers (user-visible counters that
 * set or pulse an event) derive from them.
 */

#ifndef THREADMGR_TIMER_H
#define THREADMGR_TIMER_H

#include <stdbool.h>
#include <threadmgr/threadmgr_timer.h>
#include "private.h"
#include "event.h"

/* a CPU timer's place in the timer queue (object +0x38) */
struct ThreadmgrTimer {
	struct ThreadmgrTimer *next;
	struct ThreadmgrTimer *prev;
	uint64_t expiry;			/* 0x08: system time */
	uint32_t deleted;			/* 0x10: off the queue for good */
	SceUID uid;				/* 0x14: the object's */
	SceUID pid;				/* 0x18: the process it times for */
	SceProcessObject *process;	/* 0x1c */
};

/* SceUIDCpuTimerObject (0x58) */
typedef struct SceUIDCpuTimerObject {
	SceUIDEventObject event;
	struct ThreadmgrTimer timer;		/* 0x38 */
} SceUIDCpuTimerObject;

#define CPU_TIMER_FROM_TIMER(t) \
	((SceUIDCpuTimerObject *)((uintptr_t)(t) - offsetof(SceUIDCpuTimerObject, timer)))

/* SceUIDDelayObject (0x60): a timed wait's timeout */
typedef struct SceUIDDelayObject {
	SceUIDCpuTimerObject base;
	void (*callback)(struct ThreadCB *thread);	/* 0x58 */
	struct ThreadCB *thread;	/* 0x5c */
} SceUIDDelayObject;

/* SceUIDAlarmObject (0x80) */
typedef struct SceUIDAlarmObject {
	SceUIDCpuTimerObject base;
	ThreadmgrAlarmHandler handler;		/* 0x58 */
	void *arg;				/* 0x5c */
	char name[32];				/* 0x60 */
} SceUIDAlarmObject;

#define TIMER_FLAG_RUNNING		0x00000001
#define TIMER_FLAG_EVENT		0x00000002	/* an event time is set */
#define TIMER_FLAG_REPEAT		0x00000004	/* the event time moves on by the interval */
#define TIMER_FLAG_SUSPENDED		0x00000008	/* its process is not running: off the queue */
#define TIMER_FLAG_RESCHEDULE		0x00000010	/* changed while its event was being handled */

/* SceUIDTimerObject (0x80): a counter of its process's time */
typedef struct SceUIDTimerObject {
	SceUIDCpuTimerObject base;
	uint64_t time;				/* 0x58: stopped, the count; running, where it counts from */
	uint64_t eventTime;			/* 0x60: the count the event comes at */
	uint64_t interval;			/* 0x68 */
	ListNode processLink;	/* 0x70: in its process's timers */
	uint32_t flags;				/* 0x78: TIMER_FLAG_* */
	uint32_t pulse;				/* 0x7c: the event is pulsed, not set */
} SceUIDTimerObject;

_Static_assert(sizeof(SceUIDCpuTimerObject) == 0x58, "SceUIDCpuTimerObject size");
_Static_assert(sizeof(SceUIDDelayObject) == 0x60, "SceUIDDelayObject size");
_Static_assert(sizeof(SceUIDAlarmObject) == 0x80, "SceUIDAlarmObject size");
_Static_assert(sizeof(SceUIDTimerObject) == 0x80, "SceUIDTimerObject size");

#define UID_TIMER_FROM_PROCESS_LINK(link) \
	((SceUIDTimerObject *)((uintptr_t)(link) - offsetof(SceUIDTimerObject, processLink)))

/* SceThreadmgrForDriver_4A038803 (0x38) */
typedef struct ThreadmgrAlarmInfo {
	SceSize size;
	SceUID alarmId;
	char name[32];
	uint64_t expiry;			/* 0x28 */
	ThreadmgrAlarmHandler handler;		/* 0x30 */
	void *arg;
} ThreadmgrAlarmInfo;

/* SceThreadmgrForDriver_19F3E92A, ksceKernelGetTimerInfo (0x60) */
_Static_assert(sizeof(ThreadmgrAlarmInfo) == 0x38 && sizeof(SceKernelTimerInfo) == 0x60,
	       "timer info sizes");

void insertTimer(struct ThreadmgrTimer *timer);
void setNextTimer(void);
void uidDelay_FreeObject(SceUIDDelayObject *delay);
void getWakeupTime(uint64_t *wakeup, uint32_t timeout);
int uidDelay_AllocObject(const uint64_t *wakeup, uintptr_t callback);
void cancelDelayTimer(SceUID uid, struct ThreadCB *thread);
int timerProcessExit(SceProcessObject *process);
int uidDelay_ManageCache(int high, int low, int target);
int uidAlarm_ManageCache(int high, int low, int target);

int releaseThreadWaitState(struct ThreadCB *thread,
			   uintptr_t waitable, int result);
int wakeupAndDistributeThread(struct ThreadCB *thread,
			       uintptr_t waitable, int result);
void insertPriorityQueue(uintptr_t head, struct ThreadCB *thread);
int revalidateWaitState(uintptr_t waitable);
uint32_t wakeupWaitObjectAll(uintptr_t waitable, int result, uint32_t *count);
void waitTimeoutHandler(struct ThreadCB *context);

_Static_assert(offsetof(struct ThreadCB, delayId) == 0x118,
	       "thread delay object identity");

/* timer.c */
void startCpuTimer(struct ThreadMgrCpuCB *cpu);
void suspendCpuTimer(void);
void resumeCpuTimer(void);

#endif
