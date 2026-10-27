/*
 * SceKernelThreadMgr: thread_event.c, thread event handlers
 * (SceUIDThreadEventClass): a handler runs on a thread, as a callback, when
 * the thread meets one of the events in its mask.
 */

#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "mutex.h"
#include "syscall_context.h"
#include <sysmem/sysclib.h>
#include <sysmem/uid_class.h>

#define THREAD_EVENT_ITEM_SIZE		0x80	/* the class's object allocation size */
#define THREAD_EVENT_TARGET_ALL		0x00010025	/* every thread */
#define THREAD_EVENT_TARGET_PROCESS	0x00010027	/* every thread of a process */
#define THREAD_EVENT_MASK_VALID		0x0000000c
#define THREAD_EVENT_MASK_SELF		0x00000008	/* the only event a thread may watch on itself */
#define THREAD_EVENT_ATTR_UNLIMITED	0x00004000

/* SceUIDThreadEventObject (0x38) */
typedef struct SceUIDThreadEventObject {
	SceUID ownerPid;			/* 0x00: the object base's first word */
	SceClass *cls;
	ListNode link;		/* 0x08: in the handler list; cleared from here */
	int deleted;				/* 0x10 */
	SceUID guid;				/* 0x14 */
	SceUID pid;				/* 0x18: the process the target is in */
	SceUID target;				/* 0x1c: a thread, or TARGET_ALL / TARGET_PROCESS */
	int user;				/* 0x20: a user-mode handler */
	uintptr_t handler;			/* 0x24 */
	uint32_t mask;				/* 0x28 */
	uint32_t arg;				/* 0x2c */
	uint32_t attr;				/* 0x30 */
	int running;				/* 0x34: calls in progress */
} SceUIDThreadEventObject;

_Static_assert(sizeof(SceUIDThreadEventObject) == 0x38, "SceUIDThreadEventObject size");

#define THREAD_EVENT_FROM_LINK(l) \
	((SceUIDThreadEventObject *)((uintptr_t)(l) - offsetof(SceUIDThreadEventObject, link)))

/* SceUIDThreadEventClass's constructor */
static int uidThreadEvent_Constructor(void *object)
{
	SceUIDThreadEventObject *event = object;

	memset(&event->link, 0, sizeof(*event) - offsetof(SceUIDThreadEventObject, link));
	return 0;
}

/* SceUIDThreadEventClass's destructor */
static int uidThreadEvent_Destructor(void *object)
{
	SceUIDThreadEventObject *event = object;
	SceProcessObject *process;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 1017);
	intr = sceKernelCpuSuspendIntr();
	pid = event->ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != SCE_KERNEL_PROCESS_ID) {
			sceKernelSpinlockLowLock(&process->resourceLock);
			if (event->attr & THREAD_EVENT_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->threadEventCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	sceKernelCpuResumeIntr(intr);
	memset(&event->link, 0, sizeof(*event) - offsetof(SceUIDThreadEventObject, link));
	return 0;
}

/* a handler's information (the info exports' core) */
static int getThreadEventInfo(SceUID uid, ThreadmgrThreadEventInfo *info)
{
	SceUIDThreadEventObject *event;
	int intr, ret;

	*info = (ThreadmgrThreadEventInfo){ .size = sizeof(*info) };
	intr = sceKernelCpuSuspendIntr();
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->threadEventClass,
					 (SceObjectBase **)&event) < 0) {
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_EVENT_ID;
	} else {
		info->uid = uid;
		ret = 0;
		strncpy(info->name, SceSysmemForDriver_E655852F(uid), sizeof(info->name) - 1);
		sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
		info->attr = event->attr;
		info->mask = event->mask;
		info->target = event->target;
		info->pid = event->pid;
		info->handler = event->handler;
		info->arg = event->arg;
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadEventLock);
		sceGUIDReleaseObject(uid);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceUIDThreadEventClass and the handler list */
void startThreadEvent(void)
{
	struct SceKernelThreadmgrPrivate *manager = s_threadmgrRuntime.manager;
	SceClass *cls;

	manager->threadEventHandlerNext = (uintptr_t)&manager->threadEventHandlerNext;
	manager->threadEventLock = 0;
	manager->threadEventHandlerPrev = (uintptr_t)&manager->threadEventHandlerNext;
	cls = sceKernelAllocHeapMemory(manager->heapId, 0x2c);
	if (cls == NULL)
		SCE_PANIC(122);
	if (sceUIDClassInitClass(cls, "SceUIDThreadEventClass", SceSysmemForDriver_85336A1C(),
				  THREAD_EVENT_ITEM_SIZE, uidThreadEvent_Constructor,
				  uidThreadEvent_Destructor) < 0)
		SCE_PANIC_FUNC(SCE_LINE(988, 984), "uidThreadEventClassInit");
	s_threadmgrRuntime.manager->threadEventClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 135, " sizeof(SceUIDThreadEventObject)=%d\n",
			     sizeof(SceUIDThreadEventObject));
}

/* whether a handler fires for thread on mask */
static inline int ThreadEventMatches(const SceUIDThreadEventObject *event,
				     const struct ThreadCB *thread, uint32_t mask)
{
	if (!(mask & event->mask) || event->deleted)
		return 0;
	if (event->user && (thread->flags & THREAD_FLAG_EXITING))
		return 0;
	if (event->target == THREAD_EVENT_TARGET_ALL)
		return 1;
	if (event->target == THREAD_EVENT_TARGET_PROCESS)
		return event->pid == thread->processId;
	return event->target == thread->threadId;
}

/*
 * Runs, on the current thread, every handler watching it for mask (the
 * thread lock is held throughout). A handler deleted while it ran, or whose
 * call ended the wait with a result, is freed once nothing runs it.
 */
int callThreadEventHandlers(uint32_t mask)
{
	struct ThreadCB *thread = currentThreadContext();
	ListNode *head, *link, *next;
	ThreadmgrTraceHook hook;
	SceUIDThreadEventObject *event;
	SceUID threadId;
	int ret;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 162);
	sceKernelSpinlockLowLock(THREAD_LOCK(thread));
	if (thread->currentAttr & THREAD_ATTR_COREDUMP_HANDLER)
		goto unlock;
	thread->threadEventMask |= mask;
	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
	head = (ListNode *)&s_threadmgrRuntime.manager->threadEventHandlerNext;
	link = head->next;
	while (link != head) {
		event = THREAD_EVENT_FROM_LINK(link);
		if (!ThreadEventMatches(event, thread, mask)) {
			link = link->next;
			continue;
		}
		thread->threadEventHandler = event;
		threadId = event->user ? thread->userThreadId : thread->threadId;
		event->running++;
		hook = s_threadmgrRuntime.threadTraceHook;
		if (__builtin_expect(hook != NULL, 0))
			hook(10, 6, thread, event->user, event->handler, mask, threadId, event->arg);
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadEventLock);
		ret = callCallbackEntry(thread, event->user, event->handler, mask, threadId, 0,
					event->arg);
		SCE_DBG_ASSERT_CHECK(thread->lock != 0, 224);
		sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
		hook = s_threadmgrRuntime.threadTraceHook;
		if (__builtin_expect(hook != NULL, 0))
			hook(0xf, 3, threadId, thread, thread->waitResult);
		next = link->next;
		event->running--;
		thread->threadEventHandler = NULL;
		if (!event->deleted && thread->waitResult != 0)
			event->deleted = 1;
		if (event->deleted && event->running == 0) {
			ListLinkRemove(link);
			if (sceGUIDClose(event->guid) < 0)
				SCE_PANIC(249);
		}
		if (ret < 0)
			break;
		link = next;
	}
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadEventLock);
	thread->threadEventMask &= ~mask;
unlock:
	sceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	return 0;
}

/*
 * Deletes every handler a process registered. Sony leaves the handler list
 * locked and the handler referenced when one was already deleted; kept.
 */
int threadEventProcessExit(SceProcessObject *process)
{
	SceSize got = 0;
	SceUIDThreadEventObject *event;
	SceUID *vector;
	SceUID guid;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid,
		s_threadmgrRuntime.manager->threadEventClass, PUID_VECTOR_LEVEL, NULL, 0,
		NULL);
	if (num < 0)
		return 0;
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 298);
		num = scePUIDGetUIDVectorByClass(process->pid,
			s_threadmgrRuntime.manager->threadEventClass, 5, vector, num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < (int)got; i++) {
		guid = scePUIDtoGUID(process->pid, vector[i]);
		if (guid < 0) {
			SCE_DBG_ASSERT_FAIL(316);
			continue;
		}
		intr = sceKernelCpuSuspendIntr();
		if (sceGUIDReferObjectWithClass(guid,
				s_threadmgrRuntime.manager->threadEventClass,
				(SceObjectBase **)&event) < 0) {
			SCE_DBG_ASSERT_FAIL(326);
		} else {
			sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
			if (!event->deleted) {
				event->deleted = 1;
				if (event->running == 0) {
					ListLinkRemove(&event->link);
					if (sceGUIDClose(guid) < 0)
						SCE_PANIC(345);
				}
				sceKernelSpinlockLowUnlock(
					&s_threadmgrRuntime.manager->threadEventLock);
				if (sceGUIDReleaseObject(guid) < 0)
					SCE_PANIC(354);
				ret = scePUIDClose(process->pid, vector[i]);
				if (ret < 0) {
					SCE_DBG_PRINTF(0xb, 360, "()->scePUIDClose(0x%08X, puid=0x%08X)="
						"0x%08X\n", process->pid, vector[i], ret);
					SCE_PANIC(361);
				}
			}
		}
		sceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 368);
	}
	return 0;
}

/*
 * Registers handler for the events in mask of target: a thread (0: the
 * caller, which may only watch its own exit), every thread of a process, or
 * every thread.
 */
SceUID registerThreadEventHandler(const char *name, SceUID target, SceUID pid, uint32_t mask,
				  uintptr_t handler, uint32_t arg, int user)
{
	struct ThreadCB *thread;
	struct SceUIDThreadObject *targetThread;
	SceProcessObject *process, *accounting;
	UIDCreateOpt opt;
	SceUIDThreadEventObject *event;
	SceObjectBase *object;
	SceUID ownerPid, eventPid, uid;
	uint32_t saved, attr;
	int intr, ret;

	thread = currentThreadContext();
	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 405, "*** error: illegal context. "
			"registerThreadEventHandler(), [%s]\n", name);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (thread->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 413, "*** error: illegal context(thread event handler). "
			"registerThreadEventHandler(), [%s]\n", name);
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (name == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (target == THREAD_EVENT_TARGET_PROCESS) {
		eventPid = pid != 0 ? pid : thread->processId;
		ret = sceGUIDGetObjectWithClass(eventPid,
			s_threadmgrRuntime.manager->processClass, &object);
		if (ret < 0)
			goto restore;
	} else if (target == THREAD_EVENT_TARGET_ALL) {
		if (user) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			goto restore;
		}
		eventPid = SCE_KERNEL_PROCESS_ID;
		if (pid != SCE_KERNEL_PROCESS_ID && pid != -1) {
			ret = SCE_KERNEL_ERROR_INVALID_PID;
			goto restore;
		}
	} else if (target != 0 && thread->threadId != target) {
		if (sceGUIDReferObjectWithClass(target,
				s_threadmgrRuntime.manager->threadClass,
				(SceObjectBase **)&targetThread) < 0) {
			ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
			goto restore;
		}
		eventPid = targetThread->context.processId;
		sceGUIDReleaseObject(target);
		if (pid != -1 && eventPid != pid) {
			ret = SCE_KERNEL_ERROR_INVALID_PID;
			goto restore;
		}
	} else {
		if (pid != -1 && thread->processId != pid) {
			ret = SCE_KERNEL_ERROR_INVALID_PID;
			goto restore;
		}
		if (mask != THREAD_EVENT_MASK_SELF) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto restore;
		}
		target = thread->threadId;
		eventPid = thread->processId;
		goto checked;
	}
	if (mask & ~THREAD_EVENT_MASK_VALID) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		goto restore;
	}
checked:
	if (ThreadmgrInSyscall()) {
		process = thread->process;
		ownerPid = thread->processId;
		accounting = process;
	} else {
		process = thread->process;
		ownerPid = SCE_KERNEL_PROCESS_ID;
		accounting = s_threadmgrRuntime.manager->kernelProcess;
	}
	ret = sceKernelIsEqualAccessibleRangeProcBySW(process->pid, user ? 0x50 : 5, (const void *)handler, 4);
	if (ret != 0) {
		SCE_DBG_PRINTF(0, 504, "*** error: registerThreadEventHandler() -> "
			"sceKernelIsEqualAccessibleRangeProcBySW()=0x%08X\n", ret);
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (ownerPid == SCE_KERNEL_PROCESS_ID) {
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.pid = 0;
		attr = 0;
		opt.heapKind = 0;
	} else {
		opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
		opt.pid = ownerPid;
		if (thread->currentAttr & THREAD_ATTR_UNLIMITED) {
			attr = THREAD_EVENT_ATTR_UNLIMITED;
			opt.heapKind = 1;
		} else {
			attr = 0;
			opt.heapKind = 0;
		}
	}
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.type = 0;
	opt.attr = 0;
	opt.ownerPid = 0;
	opt.cls = NULL;
	intr = sceKernelCpuSuspendIntr();
	uid = sceGUIDKernelCreateWithOpt(s_threadmgrRuntime.manager->threadEventClass,
		name, &opt, (SceObjectBase **)&event);
	ret = uid;
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 529, " error: sceGUIDKernelCreate(0x%08X)\n", uid);
		goto resume;
	}
	if (ThreadmgrInSyscall()) {
		sceKernelSpinlockLowLock(&accounting->resourceLock);
		if (attr == 0) {
			if (accounting->uidCount >= 0x800) {
				sceKernelSpinlockLowUnlock(&accounting->resourceLock);
				ret = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				SCE_DBG_PRINTF(0xb, 543, "(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, "
					"processId=0x%08X\n", ownerPid);
				goto resume;
			}
			if (++accounting->uidCount > accounting->uidPeak)
				accounting->uidPeak = accounting->uidCount;
		} else if (++accounting->uidUnlimitedCount > accounting->uidUnlimitedPeak) {
			accounting->uidUnlimitedPeak = accounting->uidUnlimitedCount;
		}
		if (++accounting->threadEventCount > accounting->threadEventPeak)
			accounting->threadEventPeak = accounting->threadEventCount;
		sceKernelSpinlockLowUnlock(&accounting->resourceLock);
	}
	event->guid = uid;
	event->ownerPid = ThreadmgrInSyscall() ? thread->processId : SCE_KERNEL_PROCESS_ID;
	event->handler = handler;
	event->mask = mask;
	event->pid = eventPid;
	event->target = target;
	event->arg = arg;
	event->attr = attr;
	event->running = 0;
	if (user)
		event->user = 1;
	sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
	event->link.next = (ListNode *)&s_threadmgrRuntime.manager->threadEventHandlerNext;
	event->link.prev = (ListNode *)s_threadmgrRuntime.manager->threadEventHandlerPrev;
	s_threadmgrRuntime.manager->threadEventHandlerPrev = (uintptr_t)&event->link;
	event->link.prev->next = &event->link;
	sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadEventLock);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_FD6150D5: ksceKernelRegisterThreadEventHandler (kernel handlers) */
SceUID SceThreadmgrForDriver_FD6150D5(const char *name, SceUID target, SceUID pid, uint32_t mask,
				      uintptr_t handler, uint32_t arg)
{
	return registerThreadEventHandler(name, target, pid, mask, handler, arg, 0);
}

int sceKernelUnregisterThreadEventHandler(SceUID uid)
{
	struct ThreadCB *thread;
	SceUIDThreadEventObject *event;
	int intr, ret;

	thread = currentThreadContext();
	if (currentThreadContext() == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	if (thread->threadEventMask != 0) {
		SCE_DBG_ASSERT_PRINTF(0, 619, "*** error: illegal context(thread event handler). "
			"threadEventId=0x%08x[%s]\n", uid, SceSysmemForDriver_E655852F(uid));
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	intr = sceKernelCpuSuspendIntr();
	if (sceGUIDReferObjectWithClass(uid, s_threadmgrRuntime.manager->threadEventClass,
					 (SceObjectBase **)&event) < 0) {
		SCE_DBG_ASSERT_FAIL(630);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_EVENT_ID;
	} else {
		sceKernelSpinlockLowLock(&s_threadmgrRuntime.manager->threadEventLock);
		if (!event->deleted) {
			event->deleted = 1;
			if (event->running == 0) {
				ListLinkRemove(&event->link);
				if (sceGUIDClose(uid) < 0)
					SCE_PANIC(SCE_LINE(653, 649));
			}
		}
		ret = 0;
		sceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadEventLock);
		sceGUIDReleaseObject(uid);
	}
	sceKernelCpuResumeIntr(intr);
	return ret;
}

/* SceThreadmgrForDriver_6657429E: a handler's information; a syscall sees user thread IDs */
int SceThreadmgrForDriver_6657429E(SceUID uid, ThreadmgrThreadEventInfo *info)
{
	struct SceUIDThreadObject *target;
	SceObjectBase *pObject;
	ThreadmgrThreadEventInfo local;
	uint32_t saved;
	SceSize size;
	SceUID targetId;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (info == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	} else if (info->size > sizeof(local)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	} else {
		ret = getThreadEventInfo(uid, &local);
		if (ret >= 0) {
			targetId = local.target;
			if (targetId != THREAD_EVENT_TARGET_PROCESS && ThreadmgrInSyscall()) {
				if (sceGUIDReferObjectWithClass(targetId,
						s_threadmgrRuntime.manager->threadClass,
						&pObject) < 0) {
					local.target = -1;
				} else {
					target = (struct SceUIDThreadObject *)pObject;
					local.target = target->context.userThreadId;
					sceGUIDReleaseObject(targetId);
				}
			}
			size = info->size;
			if (size > sizeof(local))
				size = sizeof(local);
			memcpy(info, &local, size);
		}
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_5D8F88B1: the information of every handler visible at level */
int SceThreadmgrForDriver_5D8F88B1(int level, ThreadmgrThreadEventInfo *infos, SceSize size,
				   SceSize *count)
{
	SceSize got = 0;
	ThreadmgrThreadEventInfo info;
	SceUID *vector;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = sceGUIDGetUIDVectorByClass(
		s_threadmgrRuntime.manager->threadEventClass, level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = sceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->threadEventClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getThreadEventInfo(vector[i], &info) != 0)
			continue;
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}

/* SceThreadmgrForDriver_C4456EC9: the same for a process's handler PUIDs, with user thread IDs */
int SceThreadmgrForDriver_C4456EC9(SceUID pid, int level, ThreadmgrThreadEventInfo *infos,
				   SceSize size, SceSize *count)
{
	SceSize got = 0;
	struct SceUIDThreadObject *target;
	SceObjectBase *pObject;
	ThreadmgrThreadEventInfo info;
	SceUID *vector, targetId;
	int total, i, ret;

	if (size >= sizeof(info)) {
		if (infos == NULL) {
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
			goto end;
		}
	}
	ret = scePUIDGetUIDVectorByClass(pid,
		s_threadmgrRuntime.manager->threadEventClass, level, NULL, 0, NULL);
	if (ret <= 0)
		goto end;
	while (1) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   ret * sizeof(SceUID));
		if (vector == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto end;
		}
		total = scePUIDGetUIDVectorByClass(pid,
			s_threadmgrRuntime.manager->threadEventClass, level, vector, ret, &got);
		if (total > (int)got) {
			ret = total;
			sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
			continue;
		}
		break;
	}
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		if (getThreadEventInfo(scePUIDtoGUID(pid, vector[i]), &info) != 0)
			continue;
		info.uid = vector[i];
		targetId = info.target;
		if (targetId != THREAD_EVENT_TARGET_PROCESS) {
			if (sceGUIDReferObjectWithClass(targetId,
					s_threadmgrRuntime.manager->threadClass,
					&pObject) < 0) {
				info.target = -1;
			} else {
				target = (struct SceUIDThreadObject *)pObject;
				info.target = target->context.userThreadId;
				sceGUIDReleaseObject(targetId);
			}
		}
		ret++;
		if (size < ret * sizeof(info)) {
			ret += total - i - 1;
			break;
		}
		infos[got++] = info;
	}
	if (count != NULL)
		*count = got;
	sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
end:
	return ret;
}
