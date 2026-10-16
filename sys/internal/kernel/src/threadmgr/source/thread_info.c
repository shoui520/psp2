/*
 * SceKernelThreadMgr (threadmgr.c): what the thread manager tells about its
 * threads and CPUs: a thread's info and free stack, the threads running on
 * each CPU, the CPUs' idle times and the thread ID lists.
 */

#include <string.h>
#include "private.h"
#include "sce/cpu.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define CPU_COUNT			4
#define THREAD_ATTR_USER		0x80000000
#define THREAD_ATTR_UNK10000000		0x10000000	/* PLACEHOLDER: its stack is only measured with DIPSW 0xd8 */
#define WAIT_TYPE_WITH_CALLBACKS	0x80000000	/* SceKernelThreadInfo.waitType: the wait runs callbacks */
#define DIPSW_STACK_CHECK		0xd8
#define THREAD_ID_LIST_ALL		0x00010021	/* PLACEHOLDER: name; every thread */
#define UID_VECTOR_LEVEL		7		/* PLACEHOLDER: name */
#define STACK_FILL			0xffffffff
#define SDK_VERSION_DEFAULT_AFFINITY	0x02100000	/* older SDKs see the process default */
#define SDK_VERSION_RUN_STATUS_PUID	0x01800000	/* newer SDKs see PUIDs in the run status */
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR	0x80020006
#define SCE_KERNEL_ERROR_ILLEGAL_SIZE	0x8002000b
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021
#define SCE_KERNEL_ERROR_NOT_SUPPORTED	0x80020004	/* PLACEHOLDER: name */
#define SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT	0x80020005	/* PLACEHOLDER: name */

/* SceKernelThreadInfo (0x80) */
typedef struct ThreadmgrThreadInfo {
	SceSize size;
	SceUID processId;
	char name[32];
	SceUInt32 attr;				/* 0x28 */
	SceUInt32 status;
	uintptr_t entry;			/* 0x30 */
	uintptr_t stack;
	SceSize stackSize;
	int initPriority;			/* 0x3c */
	int currentPriority;
	uint32_t initCpuAffinityMask;
	uint32_t currentCpuAffinityMask;	/* 0x48 */
	int currentCpuId;
	int lastExecutedCpuId;
	uint32_t waitType;			/* 0x54 */
	SceUID waitId;
	int exitStatus;
	uint64_t runClocks;			/* 0x60 */
	uint32_t intrPreemptCount;		/* 0x68 */
	uint32_t threadPreemptCount;
	uint32_t threadReleaseCount;
	uint32_t changeCpuCount;		/* 0x74 */
	int fNotifyCallback;
	uint32_t reserved;
} ThreadmgrThreadInfo;

/* SceThreadmgrForDriver_7E280B69: the CPUs' idle threads (0x48) */
typedef struct ThreadmgrSystemInfo {
	SceSize size;
	uint32_t activeCpuMask;
	struct {
		uint64_t idleClock;		/* the idle thread's run time */
		uint32_t idleCount;		/* PLACEHOLDER: its intrPreemptCount */
		uint32_t threadSwitchCount;	/* the CPU's */
	} cpu[CPU_COUNT];
} ThreadmgrSystemInfo;

_Static_assert(sizeof(ThreadmgrThreadInfo) == 0x80 && sizeof(ThreadmgrRunStatus) == 0x34 &&
	       sizeof(ThreadmgrSystemInfo) == 0x48, "thread info layouts");

extern int ksceKernelCountFillValueFromUserProc(SceUID pid, const void *addr, uint32_t value,
						SceSize size);
extern int ksceKernelCheckDipsw(unsigned int bit);
extern const char *ksceGUIDName(SceUID guid);
extern char *strncpy(char *dst, const char *src, SceSize n);
extern int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector,
				   SceSize num, SceSize *count);
extern int ksceGUIDGetUIDVectorByClass(SceClass *cls, int level, SceUID *vector, SceSize num,
				       SceSize *count);
extern int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level,
				       SceUID *vector, SceSize num, SceSize *count);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);

/* the error for a thread ID the caller cannot name */
static inline int unknownThreadError(void)
{
	if (ThreadmgrInSyscall())
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
}

/* the calling process's SDK version (kernel callers count as the newest) */
static inline void callerSdkVersion(SceUInt32 *version)
{
	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		*version = SDK_VERSION_KERNEL_THREAD;
	else if (SceSysrootForDriver_CAE820B2(currentThreadContext()->processId, version) < 0)
		*version = 0;
}

/* ksceKernelGetThreadStackFreeSize: the stack the thread never touched (0 is the caller) */
int ksceKernelGetThreadStackFreeSize(SceUID threadId)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	uint32_t saved, *stack;
	SceSize size, words, i;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (threadId == 0) {
		if (currentThreadContext() == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto restore;
		}
		threadId = currentThreadContext()->threadId;
	}
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		goto resume;
	}
	thread = &object->context;
	if ((thread->currentAttr & THREAD_ATTR_UNK10000000) &&
	    !ksceKernelCheckDipsw(DIPSW_STACK_CHECK)) {
		ret = SCE_KERNEL_ERROR_NOT_SUPPORTED;
		goto release;
	}
	if ((int)thread->currentAttr < 0) {
		stack = (uint32_t *)thread->pUStack;
		size = thread->uStackSize;
	} else {
		stack = (uint32_t *)thread->pKStack;
		size = thread->kStackSize;
	}
	ksceKernelCpuResumeIntr(intr);
	if ((int)thread->currentAttr < 0) {
		/* the untouched words past the stack's first */
		ret = ksceKernelCountFillValueFromUserProc(thread->processId, stack + 1, STACK_FILL,
							   size - 4);
		if (ret < 0)
			goto release;	/* Sony resumes the interrupts a second time here */
		ret = (ret + 4) * 4;
	} else {
		words = size >> 2;
		if (words < 2 || stack[1] != STACK_FILL) {
			ret = 4;
		} else {
			for (i = 2; i < words && stack[i] == STACK_FILL; i++)
				;
			ret = i * 4;
		}
	}
	ksceGUIDReleaseObject(threadId);
	goto restore;
release:
	ksceGUIDReleaseObject(threadId);
resume:
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* ksceKernelGetThreadInfo: 0 is the calling thread; a syscall sees PUIDs */
int ksceKernelGetThreadInfo(SceUID threadId, ThreadmgrThreadInfo *output)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadCB *thread;
	ThreadmgrThreadInfo info;
	SceUInt32 version, state, waiting;
	SceSize count;
	SceUID puid;
	uint32_t saved;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (threadId == 0) {
		if (currentThreadContext() == NULL) {
			ret = SCE_KERNEL_ERROR_ILLEGAL_CONTEXT;
			goto restore;
		}
		threadId = currentThreadContext()->threadId;
	}
	if (output->size > sizeof(info)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	callerSdkVersion(&version);
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	intr = ksceKernelCpuSuspendIntr();
	ret = ksceGUIDReferObjectWithClass(threadId,
		s_threadmgrRuntime.manager->threadClass, (SceObjectBase **)&object);
	if (ret < 0) {
		ret = unknownThreadError();
		ksceKernelCpuResumeIntr(intr);
		goto restore;
	}
	thread = &object->context;
	strncpy(info.name, ksceGUIDName(threadId), sizeof(info.name) - 1);
	ksceKernelSpinlockLowLock(THREAD_LOCK(thread));
	state = thread->status;
	if (state & SCE_KERNEL_THREAD_STATUS_DELETED) {
		SCE_DBG_PRINTF_FUNC(0, 8002, "sceKernelGetThreadInfo", "error: sceKernelGetThreadInfo(0x%08X), deleted thread\n",
			       threadId);
		ret = SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
		ksceGUIDReleaseObject(threadId);
		ksceKernelCpuResumeIntr(intr);
		goto restore;
	}
	info.processId = thread->processId;
	info.initPriority = thread->initPriority;
	info.initCpuAffinityMask = thread->initCpuAffinityMask;
	if (ThreadmgrInSyscall()) {
		if (info.initCpuAffinityMask == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
			info.initCpuAffinityMask =
				thread->process->cpuAffinity;
		info.initCpuAffinityMask <<= 16;
	}
	info.currentPriority = thread->currentPriority;
	info.lastExecutedCpuId = thread->lastExecutedCpuId;
	info.currentCpuAffinityMask = thread->currentCpuAffinityMask;
	if (ThreadmgrInSyscall()) {
		if (info.currentCpuAffinityMask == 0 && version < SDK_VERSION_DEFAULT_AFFINITY)
			info.currentCpuAffinityMask =
				thread->process->cpuAffinity;
		info.currentCpuAffinityMask <<= 16;
	}
	if (state & THREAD_STATE_OFF_CPU) {
		/* debug-suspended: the state it was in */
		info.status = (state << 1) >> 17;
		if ((info.status & 7) == 0) {
			info.status = SCE_KERNEL_THREAD_STATUS_READY;
			waiting = 0;
			SCE_DBG_ASSERT_CHECK_FUNC(thread->currentCpuId == -1, 8039, "sceKernelGetThreadInfo");
		} else {
			waiting = info.status & SCE_KERNEL_THREAD_STATUS_WAITING;
		}
	} else {
		info.status = state & 0xffff;
		waiting = state & SCE_KERNEL_THREAD_STATUS_WAITING;
	}
	info.attr = thread->currentAttr;
	info.entry = thread->entry;
	info.stack = (int)info.attr < 0 ? thread->pUStack : thread->pKStack;
	info.currentCpuId = thread->currentCpuId;
	if (waiting) {
		info.waitType = thread->waitType;
		if (thread->flags & THREAD_FLAG_WAKE_PENDING)
			info.waitType |= WAIT_TYPE_WITH_CALLBACKS;
	}
	info.waitId = thread->waitId;
	if (ThreadmgrInSyscall() && info.waitId != -1) {
		if (PUIDGetPUIDVectorByGUID(0, info.waitId, 3, &puid, 4, &count) >= 0 &&
		    count != 0)
			info.waitId = puid;
	}
	info.stackSize = (int)info.attr < 0 ? thread->uStackSize : thread->kStackSize;
	ret = 0;
	info.exitStatus = thread->exitStatus;
	info.threadReleaseCount = thread->threadReleaseCount;
	info.fNotifyCallback = (thread->flags & THREAD_FLAG_CALLBACKS) != 0;
	info.runClocks = thread->runClock;
	info.intrPreemptCount = thread->intrPreemptCount;
	info.threadPreemptCount = thread->threadPreemptCount;
	info.changeCpuCount = thread->changeCpuCount;
	ksceKernelSpinlockLowUnlock(THREAD_LOCK(thread));
	ksceGUIDReleaseObject(threadId);
	ksceKernelCpuResumeIntr(intr);
	memcpy(output, &info, output->size < sizeof(info) ? output->size : sizeof(info));
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_33E85E9E: what runs on each CPU */
int SceThreadmgrForDriver_33E85E9E(ThreadmgrRunStatus *output)
{
	struct ThreadmgrThreadObject *object;
	struct ThreadMgrCpuCB *cpu;
	ThreadmgrRunStatus status;
	SceUInt32 version;
	uint32_t saved;
	SceUID threadId;
	int intr, ret, i;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (output->size > sizeof(status)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	status = (ThreadmgrRunStatus){ .size = sizeof(status) };
	intr = ksceKernelCpuSuspendIntr();
	cpu = s_threadmgrRuntime.manager->cpuCB;
	if (currentThreadContext() == NULL ||
	    currentThreadContext()->processId == THREADMGR_KERNEL_PID)
		version = SDK_VERSION_KERNEL_THREAD;
	else if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
		version = 0;
	for (i = 0; i < CPU_COUNT; i++, cpu++) {
		if (ThreadmgrInSyscall() && version >= SDK_VERSION_RUN_STATUS_PUID) {
			/* a user sees only its own threads */
			threadId = cpu->threadId;
			if (ksceGUIDReferObjectWithClass(threadId,
				s_threadmgrRuntime.manager->threadClass,
				(SceObjectBase **)&object) < 0) {
				status.cpu[i].processId = -1;
				status.cpu[i].threadId = -1;
				status.cpu[i].priority = -1;
				continue;
			}
			status.cpu[i].processId = object->context.processId;
			if (object->context.processId == currentThreadContext()->processId)
				status.cpu[i].threadId = object->context.userThreadId;
			else
				status.cpu[i].threadId = -1;
			status.cpu[i].priority = object->context.currentPriority;
			ksceGUIDReleaseObject(threadId);
		} else {
			status.cpu[i].processId = cpu->processId;
			status.cpu[i].threadId = cpu->threadId;
			status.cpu[i].priority = cpu->priority;
		}
	}
	ksceKernelCpuResumeIntr(intr);
	memcpy(output, &status, output->size < sizeof(status) ? output->size : sizeof(status));
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* SceThreadmgrForDriver_7E280B69: the active CPUs and their idle threads' times */
int SceThreadmgrForDriver_7E280B69(ThreadmgrSystemInfo *output)
{
	struct ThreadMgrCpuCB *cpu;
	struct ThreadCB *idle;
	ThreadmgrSystemInfo info;
	uint32_t saved;
	int intr, ret, i;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (output == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	if (output->size > sizeof(info)) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_SIZE;
		goto restore;
	}
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(
		&s_threadmgrRuntime.manager->threadMgrCBLock);
	info.activeCpuMask = s_threadmgrRuntime.manager->activeCpuMask;
	if (ThreadmgrInSyscall())
		info.activeCpuMask <<= 16;
	ksceKernelSpinlockLowUnlock(&s_threadmgrRuntime.manager->threadMgrCBLock);
	for (i = 0; i < CPU_COUNT; i++) {
		cpu = &s_threadmgrRuntime.manager->cpuCB[i];
		idle = cpu->idleThread;
		info.cpu[i].threadSwitchCount = cpu->threadSwitchCount;
		ksceKernelSpinlockLowLock(THREAD_LOCK(idle));
		info.cpu[i].idleCount = idle->intrPreemptCount;
		info.cpu[i].idleClock = idle->runClock;
		ksceKernelSpinlockLowUnlock(THREAD_LOCK(idle));
	}
	ksceKernelCpuResumeIntr(intr);
	memcpy(output, &info, output->size < sizeof(info) ? output->size : sizeof(info));
	ret = 0;
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/*
 * ksceKernelGetThreadIdList: the threads of process pid (THREAD_ID_LIST_ALL:
 * every thread), as GUIDs.
 */
int ksceKernelGetThreadIdList(SceUID pid, SceUID *ids, SceSize size, SceSize *count)
{
	SceSize found;
	int ret, i;

	if (size >= sizeof(SceUID) && ids == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ARGUMENT;
	if (pid == THREAD_ID_LIST_ALL)
		return ksceGUIDGetUIDVectorByClass(
			s_threadmgrRuntime.manager->threadClass, UID_VECTOR_LEVEL, ids,
			size >> 2, count);
	ret = SceSysmemForKernel_08C05493(pid, s_threadmgrRuntime.manager->threadClass,
					  UID_VECTOR_LEVEL, ids, size >> 2, &found);
	if (ret > 0 && (int)found > 0) {
		for (i = 0; i < (int)found; i++)
			ids[i] = kscePUIDtoGUID(pid, ids[i]);
	}
	if (count != NULL)
		*count = found;
	return ret;
}
