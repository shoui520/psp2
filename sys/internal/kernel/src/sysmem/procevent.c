/*
 * SceSysmem: process event handlers. Each handler is a UID object of
 * SceUIDProcEventClass on one list, called for process creation, exit,
 * kill, suspend, resume and other notifications; slow handlers are
 * reported.
 */

#include "sysmem_internal.h"
#include <sysmem/procevent.h>

/* report a handler slower than this, in microseconds */
#define PROC_EVENT_SLOW			100000

/* a handler's list link */
typedef struct ProcEventLink {
	struct ProcEventLink *next;
	struct ProcEventLink *prev;
	SceUID uid;
	void *common;
	ProcEventCreateHandler create;		/* 0x10 */
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

/* debug builds time each handler and report the slow ones */
#if FW_DEBUG
#define PROC_EVENT_TIMER_START()	(start = SysrootGetTimeLow())
#define PROC_EVENT_TIMER_CHECK(limit, line, what) do {				\
	time = SysrootGetTimeLow() - start;						\
	if (time >= (limit))								\
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, line, "sceKernelInvokeProcEventHandler", \
					  "  == ALERT == : [%s] process %s handler (phase 0x%08x) took %d usec\n", \
					  SceSysmemForDriver_E655852F(link->uid), what, phase, time);	\
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

	state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
	link->next->prev = link->prev;
	link->prev->next = link->next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return 0;
}

int sceKernelProcessEventInit(void)
{
	int ret;

	s_procEventList[0] = (ProcEventLink *)s_procEventList;
	s_procEventList[1] = (ProcEventLink *)s_procEventList;
	ret = sceUIDClassInitClass(&s_procEventClass, "SceUIDProcEventClass",
				    SceSysmemForDriver_85336A1C(), sizeof(ProcEventObject), ProcEventCreate,
				    ProcEventDestroy);
	if (ret < 0) {
		SCE_DBG_ASSERT_FAIL(108);
		return ret;
	}
	return 0;
}

SceUID sceKernelRegisterProcEventHandler(const char *name,
					  const SceKernelProcEventHandler *handler, void *common)
{
	ProcEventObject *object;
	ProcEventLink *link, *tail;
	SceUID uid;
	int state;

	if (handler->size != sizeof(*handler))
		SCE_DBG_PRINTF_FUNC(0xb, 124, "sceKernelRegisterProcEventHandler",
				    "[%s], invalid structure size %d\n", name, handler->size);
	uid = sceGUIDKernelCreate(&s_procEventClass, name, (SceObjectBase **)&object);
	if (uid < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 136, "sceKernelRegisterProcEventHandler",
				    "Create Process event handler UID failed 0x%08x\n", uid);
		SCE_DBG_ASSERT_CHECK_FUNC(0, 137, "sceKernelRegisterProcEventHandler");
		return uid;
	}
	object->link.create = handler->create;
	object->link.exit = handler->exit;
	object->link.kill = handler->kill;
	object->link.suspend = handler->suspend;
	object->link.resume = handler->resume;
	object->link.uid = uid;
	object->link.common = common;
	object->link.notify = handler->size == sizeof(*handler) ? handler->notify : NULL;
	state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
	link = &object->link;
	tail = s_procEventList[1];
	link->next = (ProcEventLink *)s_procEventList;
	s_procEventList[1] = link;
	link->prev = tail;
	tail->next = link;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return uid;
}

int SceProcEventForDriver_3DED57CC(SceUID uid)
{
	return sceGUIDClose(uid);
}

/*
 * The handlers of an event in order, up to the one with UID stop; the first
 * failure stops it when the caller wants to know which (*failed).
 */
int sceKernelInvokeProcEventHandler(SceUID pid, int event, int phase, void *info,
				     SceUID *failed, SceUID stop)
{
	const char *name __attribute__((unused)) = s_eventNames[event - 1];
	ProcEventLink *link;
#if FW_DEBUG
	SceUInt32 start, time;
#endif
	int state, ret = 0;

	state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
	for (link = s_procEventList[0]; link != (ProcEventLink *)s_procEventList;
	     link = link->next) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
		if (link->uid == stop) {
			state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
			break;
		}
		switch (event) {
		case PROC_EVENT_CREATE:
			if (link->create == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->create(pid, info, link->common);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 225, "CREATE");
			break;
		case PROC_EVENT_EXIT:
			if (link->exit == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->exit(pid, info, link->common);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 233, "EXIT");
			break;
		case PROC_EVENT_KILL:
			if (link->kill == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->kill(pid, info, link->common);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 241, "KILL");
			break;
		case PROC_EVENT_SUSPEND:
			if (link->suspend == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->suspend(pid, phase, info, link->common);
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
			ret = link->resume(pid, phase, info, link->common);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 267, "RESUME");
			break;
		default:
			if (link->notify == NULL)
				goto next;
			PROC_EVENT_TIMER_START();
			ret = link->notify(event, phase, info, link->common);
			PROC_EVENT_TIMER_CHECK(PROC_EVENT_SLOW, 275, name);
			break;
		}
		if (ret < 0) {
			if (failed != NULL) {
				*failed = link->uid;
				state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
				break;
			}
			SCE_DBG_PRINTF_FUNC(0xb, 287, "sceKernelInvokeProcEventHandler",
					    "process %s handler [%s] failed, res 0x%08x\n", name,
					    SceSysmemForDriver_E655852F(link->uid), ret);
		}
next:
		state = SceCpuForDriver_D32ACE9E(&s_procEventLock);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_procEventLock, state);
	return ret;
}
