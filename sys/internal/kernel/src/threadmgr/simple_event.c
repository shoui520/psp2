/*
 * SceKernelThreadMgr: simple_event.c, the simple events
 * (SceUIDSimpleEventClass, a subclass of SceUIDEventClass). Creating,
 * opening, deleting and a process's exit set bits on the event itself
 * (0x4 created, 0x8 deleted, 0x100 opened, 0x200 its process exited) with
 * the caller as its user data.
 */

#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include "scheduler.h"
#include "event.h"
#include "syscall_context.h"
#include <sysmem/uid_class.h>

#if FW_DEBUG
/* Native data retains this record without an executable reference. */
static const SceKernelDebugInfo s_openSimpleEventDebugInfo
__attribute__((section(".sce_dbginfo"), used)) = {
	SCE_DBGINFO_FIELDS(488, "sceKernelOpenSimpleEvent")
};

static const SceKernelDebugInfoSource s_openSimpleEventDebugSource
__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&s_openSimpleEventDebugInfo, __FILE_NAME__, 488, "sceKernelOpenSimpleEvent",
};
#endif

#define SIMPLE_EVENT_ITEM_SIZE		0x80	/* the class's object allocation size */
#define SIMPLE_EVENT_ATTR_LW		0x00000080
#define SIMPLE_EVENT_ATTR_UNLIMITED	0x00004000
#define SIMPLE_EVENT_KERNEL_ATTR_MASK	0x0000a980	/* what kernel callers may pass */
#define SIMPLE_EVENT_USER_ATTR_MASK	0x00002980	/* what a syscall may pass */
#define SIMPLE_EVENT_OPTION_MAX_SIZE	4
#define SIMPLE_EVENT_PATTERN_MASK	0x00ffffff	/* the bits the system sets */
#define SIMPLE_EVENT_CREATED		0x00000004
#define SIMPLE_EVENT_DELETED		0x00000008
#define SIMPLE_EVENT_OPENED		0x00000100
#define SIMPLE_EVENT_PROCESS_EXITED	0x00000200
#define SIMPLE_EVENT_UID_ATTR_LW	0x0000c000	/* PLACEHOLDER: name */
#define THREAD_ATTR_UNLIMITED_OBJECTS	0x00004000	/* PLACEHOLDER: name */
#define PUID_VECTOR_LEVEL		0x00050000	/* PLACEHOLDER: name */
#define UID_MAX_PER_PROCESS		0x800
#define UID_LW_MAX_PER_PROCESS		0x100
#define SCE_KERNEL_ERROR_ILLEGAL_ATTR	0x8002000e
#define SCE_KERNEL_ERROR_UNKNOWN_SIMPLE_EVENT_ID 0x80028261	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b

/* SceUIDSimpleEventObject (0x3c) */
typedef struct ThreadmgrSimpleEvent {
	ThreadmgrEvent event;
	SceUID uid;				/* 0x38: its own GUID */
} ThreadmgrSimpleEvent;

_Static_assert(sizeof(ThreadmgrSimpleEvent) == 0x3c, "SceUIDSimpleEventObject size");

#define SIMPLE_EVENT_CLASS()	(s_threadmgrRuntime.manager->simpleEventClass)

/* the user data of the bits a thread sets: its process, and its thread ID above */
static inline uint64_t SimpleEventCaller(SceUID threadId, SceUID pid)
{
	return (uint64_t)(int64_t)threadId << 32 | (uint64_t)(int64_t)pid;
}

static int uidSimpleEvent_Constructor(void *object)
{
	__builtin_memset((char *)object + offsetof(ThreadmgrSimpleEvent, uid), 0, sizeof(SceUID));
	return 0;
}

/* the owner process's object counts drop */
static int uidSimpleEvent_Destructor(void *object)
{
	ThreadmgrSimpleEvent *simple = object;
	SceProcessObject *process;
	SceUID pid;
	int intr;

	SCE_DBG_ASSERT_CHECK(ThreadmgrIntrDisabled(), 877);
	intr = SceCpuForDriver_821FC0EE();
	pid = simple->event.ownerPid;
	if (sceGUIDReferObjectWithClass(pid, s_threadmgrRuntime.manager->processClass,
					 (SceObjectBase **)&process) >= 0) {
		if (process->pid != THREADMGR_KERNEL_PID) {
			SceCpuForDriver_BF82DEB2(&process->resourceLock);
			if (simple->event.attr & SIMPLE_EVENT_ATTR_LW)
				process->uidLwCount--;
			else if (simple->event.attr & SIMPLE_EVENT_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->simpleEventCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
		sceGUIDReleaseObject(pid);
	}
	sceKernelCpuResumeIntr(intr);
	__builtin_memset(&simple->uid, 0, sizeof(simple->uid));
	return 0;
}

int startSimpleEvent(void)
{
	struct ThreadmgrWaitableClass *cls;
	int ret;

	cls = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId, sizeof(*cls));
	if (cls == NULL)
		SCE_PANIC(77);
	ret = sceKernelUIDInheritedEventClassInit(cls, "SceUIDSimpleEventClass", SIMPLE_EVENT_ITEM_SIZE,
					     uidSimpleEvent_Constructor, uidSimpleEvent_Destructor);
	if (ret < 0)
		SCE_PANIC(87);
	s_threadmgrRuntime.manager->simpleEventClass = (SceClass *)cls;
	SCE_DBG_PRINTF_LEVEL(2, 0, 90, " sizeof(SceUIDSimpleEventObject)=%d\n",
			     sizeof(ThreadmgrSimpleEvent));
	return ret;
}

/* the process's simple event PUIDs are closed, each telling its waiters */
int simpleEventProcessExit(SceProcessObject *process)
{
	ThreadmgrSimpleEvent *simple;
	SceUID *vector, guid;
	SceSize got = 0;
	uint32_t wake;
	int num, i, intr, ret;

	num = scePUIDGetUIDVectorByClass(process->pid, SIMPLE_EVENT_CLASS(), PUID_VECTOR_LEVEL,
					  NULL, 0, NULL);
	if (num < 0) {
		SCE_DBG_ASSERT_FAIL(114);
		return 0;
	}
	vector = NULL;
	while (num != 0) {
		vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID));
		SCE_DBG_ASSERT_CHECK(vector != NULL, 121);
		num = scePUIDGetUIDVectorByClass(process->pid, SIMPLE_EVENT_CLASS(), 5, vector,
						  num, &got);
		if ((int)got >= num)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
	}
	for (i = 0; i < num; i++) {
		guid = SceSysmemForDriver_45D22597(process->pid, vector[i]);
		/* Sony's: interrupts stay suspended when the event is gone already */
		intr = SceCpuForDriver_821FC0EE();
		if (sceGUIDReferObjectWithClass(guid, SIMPLE_EVENT_CLASS(),
						 (SceObjectBase **)&simple) < 0)
			continue;
		if (SceSysmemForDriver_84A4AF5E(process->pid, vector[i]) < 0)
			SCE_PANIC(144);
		SceCpuForDriver_BF82DEB2(&simple->event.lock);
		wake = setEventWithNotifyCallback(guid, SIMPLE_EVENT_PROCESS_EXITED,
			(uint64_t)(int64_t)process->pid, SIMPLE_EVENT_PROCESS_EXITED, &simple->event);
		sceKernelSpinlockLowUnlock(&simple->event.lock);
		if ((int)wake < 0)
			SCE_PANIC(151);
		if (sceGUIDReleaseObject(guid) < 0)
			SCE_PANIC(156);
		sceKernelCpuResumeIntr(intr);
	}
	if (vector != NULL) {
		ret = sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 163);
	}
	return 0;
}

/*
 * The early errors return with TPIDRURO still shifted, as Sony's code
 * does (see sceKernelCreateSema).
 */
SceUID sceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				  const SceKernelSimpleEventOptParam *option)
{
	struct ThreadCB *thread =
		currentThreadContext();
	SceProcessObject *process = NULL;
	UIDCreateOpt opt;
	ThreadmgrSimpleEvent *simple;
	uint32_t saved;
	SceUID uid, threadId;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF_FUNC(0, 203, "sceKernelCreateSimpleEvent", "*** error: illegal context\n");
		uid = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	if (ThreadmgrInSyscall()) {
		if (attr & ~SIMPLE_EVENT_USER_ATTR_MASK) {
			SCE_DBG_PRINTF_FUNC(0, 217, "sceKernelCreateSimpleEvent", " k: error: sceKernelCreateSimpleEvent(%s), attr=0x%08X\n",
				       name, attr);
			uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
			goto restore;
		}
	} else if (attr & ~SIMPLE_EVENT_KERNEL_ATTR_MASK) {
		SCE_DBG_PRINTF_FUNC(0, 230, "sceKernelCreateSimpleEvent", " k: error: sceKernelCreateSimpleEvent(%s), attr=0x%08X\n",
			       name, attr);
		uid = SCE_KERNEL_ERROR_ILLEGAL_ATTR;
		goto restore;
	}
	if (option != NULL && option->size > SIMPLE_EVENT_OPTION_MAX_SIZE) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	if (initPattern & SIMPLE_EVENT_PATTERN_MASK) {
		uid = SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
		goto restore;
	}
	if (attr & SIMPLE_EVENT_ATTR_LW) {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		opt.attr = SIMPLE_EVENT_UID_ATTR_LW;
	} else {
		memset(&opt, 0, sizeof(opt));
		opt.flags = UID_CREATE_OPT_ATTR;
		if (ThreadmgrInSyscall()) {
			opt.pid = thread->processId;
			opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_PROCESS_HEAP;
			if (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS)
				attr |= SIMPLE_EVENT_ATTR_UNLIMITED;
			opt.heapKind = (thread->currentAttr & THREAD_ATTR_UNLIMITED_OBJECTS) != 0;
		}
	}
	if (!ThreadmgrInSyscall()) {
		intr = SceCpuForDriver_821FC0EE();
	} else {
		process = thread->process;
		intr = SceCpuForDriver_D32ACE9E(&process->resourceLock);
		if (attr & SIMPLE_EVENT_ATTR_LW) {
			if (process->uidLwCount >= UID_LW_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_ASSERT_PRINTF_FUNC(0, 281, "sceKernelCreateSimpleEvent", "sceKernelCreateSimpleEvent(), "
					"SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidLwCount > process->uidLwPeak)
				process->uidLwPeak = process->uidLwCount;
		} else if (attr & SIMPLE_EVENT_ATTR_UNLIMITED) {
			if (++process->uidUnlimitedCount > process->uidUnlimitedPeak)
				process->uidUnlimitedPeak = process->uidUnlimitedCount;
		} else {
			if (process->uidCount >= UID_MAX_PER_PROCESS) {
				sceKernelSpinlockLowUnlock(&process->resourceLock);
				SCE_DBG_PRINTF_FUNC(0xb, 298, "sceKernelCreateSimpleEvent",
					"(), SCE_KERNEL_ERROR_UID_RL_OVERFLOW, processId=0x%08X\n",
					thread->processId);
				uid = SCE_KERNEL_ERROR_UID_RL_OVERFLOW | SYSCALL_ERROR_BIT;
				goto resume;
			}
			if (++process->uidCount > process->uidPeak)
				process->uidPeak = process->uidCount;
		}
		if (++process->simpleEventCount > process->simpleEventPeak)
			process->simpleEventPeak = process->simpleEventCount;
		sceKernelSpinlockLowUnlock(&process->resourceLock);
	}
	uid = sceGUIDKernelCreateWithOpt(SIMPLE_EVENT_CLASS(), name, &opt,
					  (SceObjectBase **)&simple);
	if (uid < 0) {
		if (ThreadmgrInSyscall()) {
			SceCpuForDriver_BF82DEB2(&process->resourceLock);
			if (attr & SIMPLE_EVENT_ATTR_LW)
				process->uidLwCount--;
			else if (attr & SIMPLE_EVENT_ATTR_UNLIMITED)
				process->uidUnlimitedCount--;
			else
				process->uidCount--;
			process->simpleEventCount--;
			sceKernelSpinlockLowUnlock(&process->resourceLock);
		}
	} else {
		simple->uid = uid;
		simple->event.ownerPid = ThreadmgrInSyscall() ? thread->processId :
								THREADMGR_KERNEL_PID;
		threadId = ThreadmgrInSyscall() ? thread->userThreadId : thread->threadId;
		/* sceKernelEventObjectInit unlocks the object the class hands over locked */
		ret = sceKernelEventObjectInit(uid, attr, SimpleEventCaller(threadId, thread->processId),
					       initPattern | SIMPLE_EVENT_CREATED);
		SCE_DBG_ASSERT_CHECK_FUNC(ret >= 0, 334, "sceKernelCreateSimpleEvent");
	}
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return uid;
}

/* a kernel caller closes the event, which sets SIMPLE_EVENT_DELETED */
int sceKernelDeleteSimpleEvent(SceUID simpleEventId)
{
	struct ThreadCB *thread;
	ThreadmgrSimpleEvent *simple;
	uint32_t saved, wake;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 378, "*** error: illegal context. simpleEventId=0x%08x[%s]\n",
				      simpleEventId, SceSysmemForDriver_E655852F(simpleEventId));
		ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
		goto restore;
	}
	intr = SceCpuForDriver_821FC0EE();
	if (sceGUIDReferObjectWithClass(simpleEventId, SIMPLE_EVENT_CLASS(),
					 (SceObjectBase **)&simple) < 0) {
		ret = ThreadmgrInSyscall() ?
			(SCE_KERNEL_ERROR_UNKNOWN_SIMPLE_EVENT_ID | SYSCALL_ERROR_BIT) :
			SCE_KERNEL_ERROR_UNKNOWN_SIMPLE_EVENT_ID;
		goto resume;
	}
	if (sceGUIDClose(simpleEventId) < 0)
		SCE_PANIC(SCE_LINE(407, 400));
	if (ThreadmgrInSyscall()) {
		/* not for syscalls: they have sceKernelDeleteSimpleEventForUser */
		SCE_DBG_ASSERT_FAIL(415);
		wake = 0;
	} else {
		thread = currentThreadContext();
		SceCpuForDriver_BF82DEB2(&simple->event.lock);
		wake = setEventWithNotifyCallback(simpleEventId, SIMPLE_EVENT_DELETED,
			SimpleEventCaller(thread->threadId, thread->processId), SIMPLE_EVENT_DELETED,
			&simple->event);
		sceKernelSpinlockLowUnlock(&simple->event.lock);
		if ((int)wake < 0)
			SCE_PANIC(SCE_LINE(418, 411));
	}
	if (sceGUIDReleaseObject(simpleEventId) < 0)
		SCE_PANIC(SCE_LINE(427, 420));
	ret = 0;
	if (wake & 1)
		switchThread(0);
resume:
	sceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the syscall opening a simple event by name, which sets SIMPLE_EVENT_OPENED */
SceUID sceKernelOpenSimpleEventForUser(const char *userName)
{
	struct ThreadCB *thread =
		currentThreadContext();
	UIDCreateOpt opt;
	SceObjectBase *object;
	char name[32];
	uint32_t wake;
	SceUID puid;
	int ret;

	if (currentThreadContext() == NULL) {
		SCE_DBG_ASSERT_PRINTF(0, 509, "*** error: illegal context\n");
		return SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
	}
	ret = copyinUIDNameWithAttr(name, userName, SIMPLE_EVENT_ATTR_LW);
	if (ret < 0)
		return ret;
	opt.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_CLASS;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.pid = 0;
	opt.heapKind = 0;
	opt.type = 0;
	opt.attr = PUID_ATTR_OPENED;
	opt.ownerPid = 0;
	opt.cls = SIMPLE_EVENT_CLASS();
	puid = SceSysmemForKernel_201E970B(0, name, &opt);
	if (puid < 0)
		return puid;
	ret = SceSysmemForDriver_FE6D7FAE(0, puid, &object);
	if (ret < 0) {
		if (SceSysmemForDriver_84A4AF5E(0, puid) < 0)
			SCE_PANIC(SCE_LINE(539, 532));
		return ret;
	}
	wake = sceKernelSetEventWithNotifyCallback(SceSysmemForDriver_45D22597(0, puid), SIMPLE_EVENT_OPENED,
		SimpleEventCaller(thread->userThreadId, thread->processId), SIMPLE_EVENT_OPENED);
	SCE_DBG_ASSERT_CHECK((int)wake >= 0, 556);
	return puid;
}

/* SceThreadmgrForDriver_4DE77569: the information of every simple event visible at level */
int SceThreadmgrForDriver_4DE77569(int level, SceKernelEventInfo *infos, SceSize size,
				   SceSize *count)
{
	SceKernelEventInfo info;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = sceGUIDGetUIDVectorByClass(SIMPLE_EVENT_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = sceGUIDGetUIDVectorByClass(SIMPLE_EVENT_CLASS(), level, vector, num, &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		if (SceThreadmgrForDriver_CBA7FAAA(vector[i], &info) != 0)
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
	return ret;
}

/* SceThreadmgrForDriver_E0C7BCBB: the same for a process's simple event PUIDs */
int SceThreadmgrForDriver_E0C7BCBB(SceUID pid, int level, SceKernelEventInfo *infos,
				   SceSize size, SceSize *count)
{
	SceKernelEventInfo info;
	SceUInt32 attr;
	SceSize got = 0;
	SceUID *vector;
	int num, total, i, ret;

	if (size >= sizeof(info) && infos == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	num = scePUIDGetUIDVectorByClass(pid, SIMPLE_EVENT_CLASS(), level, NULL, 0, NULL);
	if (num <= 0)
		return num;
	while ((vector = sceKernelAllocHeapMemory(s_threadmgrRuntime.manager->heapId,
						   num * sizeof(SceUID))) != NULL) {
		total = scePUIDGetUIDVectorByClass(pid, SIMPLE_EVENT_CLASS(), level, vector, num,
						    &got);
		if (total <= (int)got)
			break;
		sceKernelFreeHeapMemory(s_threadmgrRuntime.manager->heapId, vector);
		num = total;
	}
	if (vector == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	ret = 0;
	got = 0;
	for (i = 0; i < total; i++) {
		memset(&info, 0, sizeof(info));
		info.size = sizeof(info);
		if (SceThreadmgrForDriver_CBA7FAAA(SceSysmemForDriver_45D22597(pid, vector[i]), &info) != 0)
			continue;
		info.eventId = vector[i];
		if (scePUIDGetAttr(pid, info.eventId, &attr) >= 0 && (attr & PUID_ATTR_OPENED))
			info.attr |= PUID_ATTR_OPENED;
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
	return ret;
}
