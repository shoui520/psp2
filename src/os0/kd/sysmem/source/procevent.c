/*
 * SceSysmem: process event handlers. Each handler is a UID object of
 * SceUIDProcEventClass on one list, called for process creation, exit,
 * kill, suspend, resume and other notifications; slow handlers are
 * reported.
 */

#include "sysmem_internal.h"

#define PROC_EVENT_CREATE		1
#define PROC_EVENT_EXIT			2
#define PROC_EVENT_KILL			3
#define PROC_EVENT_SUSPEND		4
#define PROC_EVENT_RESUME		5

/* report a handler slower than this, in microseconds */
#define PROC_EVENT_SLOW			100000

typedef int (*ProcEventHandler)(SceUID pid, void *info, void *args);
typedef int (*ProcEventPhaseHandler)(SceUID pid, int phase, void *info, void *args);
typedef int (*ProcEventNotifyHandler)(int event, int phase, void *info, void *args);

/* what a driver registers (size 0x18, or 0x1c with notify) */
typedef struct SceKernelProcEventHandler {
	SceSize size;
	ProcEventHandler create;
	ProcEventHandler exit;
	ProcEventHandler kill;
	ProcEventPhaseHandler suspend;		/* 0x10 */
	ProcEventPhaseHandler resume;
	ProcEventNotifyHandler notify;
} SceKernelProcEventHandler;

/* a handler's list link */
typedef struct ProcEventLink {
	struct ProcEventLink *next;
	struct ProcEventLink *prev;
	SceUID uid;
	void *args;
	ProcEventHandler create;		/* 0x10 */
	ProcEventHandler exit;
	ProcEventHandler kill;
	ProcEventPhaseHandler suspend;
	ProcEventPhaseHandler resume;		/* 0x20 */
	ProcEventNotifyHandler notify;
} ProcEventLink;

typedef struct ProcEventObject {
	SceObjectBase base;
	ProcEventLink link;			/* 0x08 */
} ProcEventObject;

_Static_assert(sizeof(ProcEventObject) == 0x30, "ProcEventObject size");

static const char s_eventNames[][13] = {
	"CREATE", "EXIT", "KILL", "SUSPEND", "RESUME", "EVENT NOTIFY",
};

static SceKernelSpinlock s_procEventLock;
static SceClass s_procEventClass;
/* the list head (next, prev) */
static ProcEventLink *s_procEventList[2];

uint32_t SysrootGetTimeLow(void);

/* debug builds time each handler and report the slow ones */
#if FW_DEBUG
#define PROC_EVENT_TIMER_START()	(start = SysrootGetTimeLow())
#define PROC_EVENT_TIMER_CHECK(limit, line, what) do {				\
	time = SysrootGetTimeLow() - start;						\
	if (time >= (limit))								\
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, line, "sceKernelInvokeProcEventHandler", \
					  "  == ALERT == : [%s] process %s handler (phase 0x%08x) took %d usec\n", \
					  ksceGUIDName(link->uid), what, phase, time);	\
} while (0)
#else
#define PROC_EVENT_TIMER_START()	do { } while (0)
#define PROC_EVENT_TIMER_CHECK(limit, line, what)	do { } while (0)
#endif

static int ProcEventCreate(void *object)
{
	return 0;
}

static int ProcEventDestroy(void *object)
{
	ProcEventLink *link = &((ProcEventObject *)object)->link;
	int state;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
	link->next->prev = link->prev;
	link->prev->next = link->next;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return 0;
}

int sceKernelProcessEventInit(void)
{
	int ret;

	s_procEventList[0] = (ProcEventLink *)s_procEventList;
	s_procEventList[1] = (ProcEventLink *)s_procEventList;
	ret = ksceUIDClassInitClass(&s_procEventClass, "SceUIDProcEventClass",
				    ksceKernelGetUIDClass(), sizeof(ProcEventObject), ProcEventCreate,
				    ProcEventDestroy);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(108);
		return ret;
	}
	return 0;
}

SceUID ksceKernelRegisterProcEventHandler(const char *name,
					  const SceKernelProcEventHandler *handler, void *args)
{
	ProcEventObject *object;
	ProcEventLink *link;
	SceUID uid;
	int state;

	if (handler->size != sizeof(*handler))
		SCE_DBG_PRINTF_FUNC(0xb, 124, "sceKernelRegisterProcEventHandler",
				    "[%s], invalid structure size %d\n", name, handler->size);
	uid = ksceGUIDKernelCreate(&s_procEventClass, name, (SceObjectBase **)&object);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 136, "sceKernelRegisterProcEventHandler",
				    "Create Process event handler UID failed 0x%08x\n", uid);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 137, "sceKernelRegisterProcEventHandler");
		return uid;
	}
	link = &object->link;
	link->create = handler->create;
	link->exit = handler->exit;
	link->kill = handler->kill;
	link->suspend = handler->suspend;
	link->resume = handler->resume;
	link->uid = uid;
	link->args = args;
	link->notify = handler->size == sizeof(*handler) ? handler->notify : NULL;
	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
	link->next = (ProcEventLink *)s_procEventList;
	link->prev = s_procEventList[1];
	s_procEventList[1]->next = link;
	s_procEventList[1] = link;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return uid;
}

int ksceKernelUnregisterProcEventHandler(SceUID uid)
{
	return ksceGUIDClose(uid);
}

/*
 * The handlers of an event in order, up to the one with UID stop; the first
 * failure stops it when the caller wants to know which (*failed).
 */
int ksceKernelInvokeProcEventHandler(SceUID pid, int event, int phase, void *info,
				     SceUID *failed, SceUID stop)
{
	const char *name __attribute__((unused)) = s_eventNames[event - 1];
	ProcEventLink *link;
#if FW_DEBUG
	SceUInt32 start, time;
#endif
	int state, ret = 0;

	state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
	for (link = s_procEventList[0]; link != (ProcEventLink *)s_procEventList;
	     link = link->next) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
		if (link->uid == stop) {
			state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
			break;
		}
		switch (event) {
		case PROC_EVENT_CREATE:
			if (link->create == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->create(pid, info, link->args);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 225, "CREATE");
			break;
		case PROC_EVENT_EXIT:
			if (link->exit == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->exit(pid, info, link->args);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 233, "EXIT");
			break;
		case PROC_EVENT_KILL:
			if (link->kill == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->kill(pid, info, link->args);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 241, "KILL");
			break;
		case PROC_EVENT_SUSPEND:
			if (link->suspend == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->suspend(pid, phase, info, link->args);
			/* the suspend phases 0x200 and 0x202 have tighter limits */
			if (phase == 0x200)
				PROC_EVENT_TIMER_CHECK(1000, 251, "SUSPEND");
			else if (phase == 0x202)
				PROC_EVENT_TIMER_CHECK(100, 254, "SUSPEND");
			else
				PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 257, "SUSPEND");
			break;
		case PROC_EVENT_RESUME:
			if (link->resume == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->resume(pid, phase, info, link->args);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 267, "RESUME");
			break;
		default:
			if (link->notify == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->notify(event, phase, info, link->args);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 275, name);
			break;
		}
		if (ret < 0) {
			if (failed != NULL) {
				*failed = link->uid;
				state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
				break;
			}
			SCE_DBG_PRINTF_FUNC(0xb, 287, "sceKernelInvokeProcEventHandler",
					    "process %s handler [%s] failed, res 0x%08x\n", name,
					    ksceGUIDName(link->uid), ret);
		}
next:
		state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_procEventLock);
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return ret;
}
