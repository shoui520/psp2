/* Shared frame buffers; Sony's source file is sharedfb.c. */

#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <appmgr/sharedfb.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>
#include <psp2kern/kernel/proc_event.h>
#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/registrymgr.h>
#include <stddef.h>
#include <string.h>
#include <sysmem/atomic.h>

struct SharedFbProcess {
	SceUID pid;
	SceUID shareMemBlock;
	signed char vsync;
	unsigned char reserved[3];
	unsigned int state;
};

struct SharedFbPort {
	int index;
	SceUID memBlock;
	SceUID ownerPid;
	unsigned int creationArgument;
	SceUID beginEvent;
	SceUID endEvent;
	SceSharedFbInfo info;
	struct SharedFbProcess processes[16];
	union {
		struct {
			SceUID assignedPids[8];
			SceUID requestedPids[8];
		};
		SceUID updatePids[16];
	};
};

struct SharedFbManager {
	struct SharedFbPort ports[2];
	unsigned int updateIndex;
	struct SharedFbPort *currentPort;
	SceKernelFastMutex mutex;
	int shellRenderPort;
};

_Static_assert(sizeof(struct SharedFbProcess) == 16, "shared FB process");
_Static_assert(offsetof(struct SharedFbProcess, vsync) == 8, "shared FB vsync");
_Static_assert(offsetof(struct SharedFbProcess, state) == 12, "shared FB state");
_Static_assert(sizeof(struct SharedFbPort) == 432, "shared FB port");
_Static_assert(offsetof(struct SharedFbPort, info) == 24, "shared FB information");
_Static_assert(offsetof(struct SharedFbPort, processes) == 112, "shared FB processes");
_Static_assert(offsetof(struct SharedFbPort, assignedPids) == 368, "shared FB assigned PIDs");
_Static_assert(offsetof(struct SharedFbPort, requestedPids) == 400, "shared FB requested PIDs");
_Static_assert(offsetof(struct SharedFbPort, updatePids) == 368, "shared FB update PIDs");
_Static_assert(sizeof(((struct SharedFbPort *)0)->updatePids) == 64, "shared FB PID storage");
_Static_assert(sizeof(struct SharedFbManager) == 940, "shared FB manager");
_Static_assert(offsetof(struct SharedFbManager, updateIndex) == 864, "shared FB update index");
_Static_assert(offsetof(struct SharedFbManager, currentPort) == 868, "shared FB current port");
_Static_assert(offsetof(struct SharedFbManager, mutex) == 872, "shared FB mutex");
_Static_assert(offsetof(struct SharedFbManager, shellRenderPort) == 936, "shared FB shell port");

struct SharedFbManager gSharedFbManager;

extern int _sceAppMgrIsExistAbortedAppOrDebugSuspendedApp(void);
extern int _sceAppMgrAbort(SceUID pid);
extern int SceSysrootForDriver_1B3FA457(SceUID pid);
extern int sceKernelKillProcess(SceUID pid, int status);

int sharedFbProcessCreate(SceUID pid, ProcEventCreateParam *param, int flags);
int sharedFbProcessExit(SceUID pid, ProcEventParam *param, int flags);
int sharedFbProcessKill(SceUID pid, ProcEventParam *param, int flags);
int sharedFbProcessStop(SceUID pid, int event, ProcEventParam *param, int flags);
int sharedFbProcessStart(SceUID pid, int event, ProcEventParam *param, int flags);

/* ProcEventParam and the SDK invocation types describe the same provider ABI. */
const SceProcEventHandler gSharedFbProcessHandler = {
	.size = sizeof(SceProcEventHandler),
	.create = (int (*)(SceUID, SceProcEventInvokeParam2 *, int))sharedFbProcessCreate,
	.exit = (int (*)(SceUID, SceProcEventInvokeParam1 *, int))sharedFbProcessExit,
	.kill = (int (*)(SceUID, SceProcEventInvokeParam1 *, int))sharedFbProcessKill,
	.stop = (int (*)(SceUID, int, SceProcEventInvokeParam1 *, int))sharedFbProcessStop,
	.start = (int (*)(SceUID, int, SceProcEventInvokeParam1 *, int))sharedFbProcessStart,
	.switch_process = NULL,
};

int sharedFbProcessCreate(SceUID pid, ProcEventCreateParam *param, int flags)
{
	(void)pid;
	(void)param;
	(void)flags;
	return 0;
}

#define SHARED_FB_SELECT_PROCESS(port, processId, index, missing) \
	do { \
		if (SHARED_FB_PROCESS_MATCHES(port, processId, 0)) index = 0; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 1)) index = 1; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 2)) index = 2; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 3)) index = 3; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 4)) index = 4; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 5)) index = 5; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 6)) index = 6; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 7)) index = 7; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 8)) index = 8; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 9)) index = 9; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 10)) index = 10; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 11)) index = 11; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 12)) index = 12; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 13)) index = 13; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 14)) index = 14; \
		else if (SHARED_FB_PROCESS_MATCHES(port, processId, 15)) index = 15; \
		else goto missing; \
	} while (0)

#define SHARED_FB_CONTAINS_PID(port, processId) \
	((port)->processes[0].pid == (processId) || (port)->processes[1].pid == (processId) || \
	 (port)->processes[2].pid == (processId) || (port)->processes[3].pid == (processId) || \
	 (port)->processes[4].pid == (processId) || (port)->processes[5].pid == (processId) || \
	 (port)->processes[6].pid == (processId) || (port)->processes[7].pid == (processId) || \
	 (port)->processes[8].pid == (processId) || (port)->processes[9].pid == (processId) || \
	 (port)->processes[10].pid == (processId) || (port)->processes[11].pid == (processId) || \
	 (port)->processes[12].pid == (processId) || (port)->processes[13].pid == (processId) || \
	 (port)->processes[14].pid == (processId) || (port)->processes[15].pid == (processId))

#define SHARED_FB_PROCESS_MATCHES(port, processId, index) \
	((port)->processes[index].pid != -1 && \
	 (port)->processes[index].pid == (processId))

int sharedFbProcessStart(SceUID processId, int event,
			 ProcEventParam *param, int flags)
{
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	unsigned int portIndex, index;

	(void)param;
	(void)flags;
	if (event != 0x20000)
		return 0;
	if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
		portIndex = 0;
	else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
		portIndex = 1;
	else
		return 0;
	port = &gSharedFbManager.ports[portIndex];
	if (SHARED_FB_PROCESS_MATCHES(port, processId, 0))
		index = 0;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 1))
		index = 1;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 2))
		index = 2;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 3))
		index = 3;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 4))
		index = 4;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 5))
		index = 5;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 6))
		index = 6;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 7))
		index = 7;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 8))
		index = 8;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 9))
		index = 9;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 10))
		index = 10;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 11))
		index = 11;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 12))
		index = 12;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 13))
		index = 13;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 14))
		index = 14;
	else if (SHARED_FB_PROCESS_MATCHES(port, processId, 15))
		index = 15;
	else
		return 0;
	process = &port->processes[index];
	if (process)
		process->state = 1;
	return 0;
}

int sharedFbProcessStop(SceUID processId, int event,
			ProcEventParam *param, int flags)
{
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	unsigned int portIndex, index, previous;

	(void)flags;
	if (event != 0x202)
		return 0;
	if (param && (param->kind & 0x50)) {
		if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
			portIndex = 0;
		else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
			portIndex = 1;
		else
			goto suspend;
		port = &gSharedFbManager.ports[portIndex];
		SHARED_FB_SELECT_PROCESS(port, processId, index, suspend);
		process = &port->processes[index];
		if (process) {
			previous = __atomic_exchange_n(&process->state, 0, __ATOMIC_RELAXED);
			if (previous == 2 || previous == 4)
				ksceKernelSetEventFlag(port->endEvent, 1u << index);
		}
	}
suspend:
	if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
		portIndex = 0;
	else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
		portIndex = 1;
	else
		return 0;
	port = &gSharedFbManager.ports[portIndex];
	SHARED_FB_SELECT_PROCESS(port, processId, index, done);
	process = &port->processes[index];
	if (process) {
		do {
			previous = AtomicLoadReserved32(&process->state);
			if (previous == 4)
				return -1;
		} while (AtomicStoreReserved32(&process->state, 3));
		process->vsync = 0;
		if (previous == 2)
			ksceKernelSetEventFlag(port->endEvent, 1u << index);
	}
done:
	return 0;
}

int sharedFbReleaseProcess(SceUID processId)
{
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	unsigned int portIndex, index, previous;
	int result;

	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		return result;
	if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
		portIndex = 0;
	else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
		portIndex = 1;
	else
		goto done;
	port = &gSharedFbManager.ports[portIndex];
	SHARED_FB_SELECT_PROCESS(port, processId, index, done);
	process = &port->processes[index];
	if (!process)
		goto done;
	result = ksceKernelFreeMemBlock(process->shareMemBlock);
	if (result < 0) {
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		return result;
	}
	previous = __atomic_exchange_n(&process->state, 0, __ATOMIC_RELAXED);
	if (previous == 2 || previous == 4)
		ksceKernelSetEventFlag(port->endEvent, 1u << index);
	memset(process, 0, sizeof(*process));
	port->processes[index].pid = -1;
	process->shareMemBlock = -1;
done:
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	return 0;
}

int sharedFbProcessKill(SceUID processId, ProcEventParam *param, int flags)
{
	(void)param;
	(void)flags;
	return sharedFbReleaseProcess(processId);
}

int sharedFbProcessExit(SceUID processId, ProcEventParam *param, int flags)
{
	(void)param;
	(void)flags;
	return sharedFbReleaseProcess(processId);
}

int sharedFbCancelProcessUpdate(SceUID processId)
{
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	unsigned int portIndex, index, previous;
	int result;

	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		return result;
	if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
		portIndex = 0;
	else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
		portIndex = 1;
	else
		goto done;
	port = &gSharedFbManager.ports[portIndex];
	SHARED_FB_SELECT_PROCESS(port, processId, index, done);
	process = &port->processes[index];
	if (process) {
		previous = __atomic_exchange_n(&process->state, 0, __ATOMIC_RELAXED);
		if (previous == 2 || previous == 4)
			ksceKernelSetEventFlag(port->endEvent, 1u << index);
	}
done:
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	return 0;
}

SceUID sharedFbGetRenderMemBlock(int *size)
{
	struct SharedFbPort *port;
	SceUID memBlock;
	int result, index;

	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	index = result;
	if (result)
		return result;
	if (gSharedFbManager.ports[0].index == 1 ||
	    (index = gSharedFbManager.ports[1].index) == 1) {
		port = &gSharedFbManager.ports[index];
		memBlock = port->memBlock;
		if (size)
			*size = port->info.fb_size;
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		return memBlock;
	}
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	return (int)0x80200004;
}

#define SHARED_FB_INITIALIZE_PROCESS(port, index) \
	do { \
		(port)->processes[index].pid = -1; \
		(port)->processes[index].shareMemBlock = -1; \
		(port)->processes[index].state = 0; \
	} while (0)

SceUID sceSharedFbCreate(int index, const SceSharedFbCreate *param)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceSharedFbCreate create;
	struct SharedFbPort *port;
	void *base;
	SceUID memBlock, processId, beginEvent, endEvent;
	int result, portIndex;

	if (!ksceSblACMgrIsSystemProgram(0)) {
		result = (int)0x80010058;
		goto done;
	}
	if (index < 0) {
		result = (int)0x80200007;
		goto done;
	}
	result = ksceKernelCopyFromUser(&create, param, sizeof(create));
	if (result)
		goto done;
	memBlock = ksceKernelAllocMemBlock("SceSharedFb",
					SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
					create.AllocMemSize, NULL);
	if (memBlock < 0) {
		result = memBlock;
		goto done;
	}
	base = NULL;
	result = ksceKernelGetMemBlockBase(memBlock, &base);
	if (result)
		goto freeMemBlock;
	processId = sceKernelGetProcessId();
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto freeMemBlock;
	if (!gSharedFbManager.ports[0].memBlock)
		portIndex = 0;
	else if (!gSharedFbManager.ports[1].memBlock)
		portIndex = 1;
	else {
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		result = (int)0x80200002;
		goto freeMemBlock;
	}
	port = &gSharedFbManager.ports[portIndex];
	if (port == &gSharedFbManager.ports[0])
		port->memBlock = 0;
	else
		port->memBlock = -1;
	port->ownerPid = -1;
	port->beginEvent = -1;
	port->endEvent = -1;
	SHARED_FB_INITIALIZE_PROCESS(port, 0);
	SHARED_FB_INITIALIZE_PROCESS(port, 1);
	SHARED_FB_INITIALIZE_PROCESS(port, 2);
	SHARED_FB_INITIALIZE_PROCESS(port, 3);
	SHARED_FB_INITIALIZE_PROCESS(port, 4);
	SHARED_FB_INITIALIZE_PROCESS(port, 5);
	SHARED_FB_INITIALIZE_PROCESS(port, 6);
	SHARED_FB_INITIALIZE_PROCESS(port, 7);
	SHARED_FB_INITIALIZE_PROCESS(port, 8);
	SHARED_FB_INITIALIZE_PROCESS(port, 9);
	SHARED_FB_INITIALIZE_PROCESS(port, 10);
	SHARED_FB_INITIALIZE_PROCESS(port, 11);
	SHARED_FB_INITIALIZE_PROCESS(port, 12);
	SHARED_FB_INITIALIZE_PROCESS(port, 13);
	SHARED_FB_INITIALIZE_PROCESS(port, 14);
	SHARED_FB_INITIALIZE_PROCESS(port, 15);
	memset(port->assignedPids, -1, sizeof(port->assignedPids));
	memset(port->requestedPids, -1, sizeof(port->requestedPids));
	beginEvent = ksceKernelCreateEventFlag("SceSharedFbBegin", 0x9000, 0, NULL);
	endEvent = ksceKernelCreateEventFlag("SceSharedFbEnd", 0x9000, 0, NULL);
	port->index = index;
	port->beginEvent = beginEvent;
	port->endEvent = endEvent;
	port->ownerPid = processId;
	port->creationArgument = create.creationArgument;
	port->memBlock = memBlock;
	port->info.fb_base = base;
	port->info.fb_size = create.AllocMemSize;
	result = kscePUIDOpenByGUID(processId, memBlock);
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	if (result >= 0)
		goto done;
	ksceKernelDeleteEventFlag(beginEvent);
	ksceKernelDeleteEventFlag(endEvent);
freeMemBlock:
	ksceKernelFreeMemBlock(memBlock);
done:
	sceSyscallLeave(syscallState);
	return result;
}

int sceSharedFbDelete(SceUID fbId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	SceUID processId, ownerPid, memBlock;
	int result, portIndex;

	processId = sceKernelGetProcessId();
	memBlock = kscePUIDtoGUID(processId, fbId);
	result = memBlock;
	if (memBlock < 0)
		goto done;
	if (!ksceSblACMgrIsSystemProgram(0)) {
		result = (int)0x80010058;
		goto done;
	}
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto done;
	if (memBlock == gSharedFbManager.ports[0].memBlock)
		portIndex = 0;
	else if (memBlock == gSharedFbManager.ports[1].memBlock)
		portIndex = 1;
	else
		goto missing;
	port = &gSharedFbManager.ports[portIndex];
	ownerPid = port->ownerPid;
	if (ownerPid != processId)
		goto missing;
	ksceKernelDeleteEventFlag(port->beginEvent);
	ksceKernelDeleteEventFlag(port->endEvent);
	port->memBlock = port == &gSharedFbManager.ports[0] ? 0 : -1;
	port->ownerPid = -1;
	port->beginEvent = -1;
	port->endEvent = -1;
	SHARED_FB_INITIALIZE_PROCESS(port, 0);
	SHARED_FB_INITIALIZE_PROCESS(port, 1);
	SHARED_FB_INITIALIZE_PROCESS(port, 2);
	SHARED_FB_INITIALIZE_PROCESS(port, 3);
	SHARED_FB_INITIALIZE_PROCESS(port, 4);
	SHARED_FB_INITIALIZE_PROCESS(port, 5);
	SHARED_FB_INITIALIZE_PROCESS(port, 6);
	SHARED_FB_INITIALIZE_PROCESS(port, 7);
	SHARED_FB_INITIALIZE_PROCESS(port, 8);
	SHARED_FB_INITIALIZE_PROCESS(port, 9);
	SHARED_FB_INITIALIZE_PROCESS(port, 10);
	SHARED_FB_INITIALIZE_PROCESS(port, 11);
	SHARED_FB_INITIALIZE_PROCESS(port, 12);
	SHARED_FB_INITIALIZE_PROCESS(port, 13);
	SHARED_FB_INITIALIZE_PROCESS(port, 14);
	SHARED_FB_INITIALIZE_PROCESS(port, 15);
	memset(port->assignedPids, -1, sizeof(port->assignedPids));
	memset(port->requestedPids, -1, sizeof(port->requestedPids));
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	result = ksceKernelFreeMemBlock(memBlock);
	if (result < 0)
		goto done;
	result = kscePUIDClose(ownerPid, fbId);
	if (result >= 0)
		result = 0;
	goto done;
missing:
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	result = (int)0x80200004;
done:
	sceSyscallLeave(syscallState);
	return result;
}

int sceSharedFbUpdateProcessBegin(SceUID fbId, const void *info,
				unsigned int *processCount)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	SceUID memBlock, processId;
	SceUID firstPid, secondPid, thirdPid, fourthPid;
	unsigned int count, index;
	int result, portIndex;

	if (!ksceSblACMgrIsSystemProgram(0)) {
		result = (int)0x80010058;
		goto done;
	}
	gSharedFbManager.updateIndex = 0;
	gSharedFbManager.currentPort = NULL;
	if (!processCount) {
		result = (int)0x80200007;
		goto done;
	}
	memBlock = kscePUIDtoGUID(sceKernelGetProcessId(), fbId);
	result = memBlock;
	if (memBlock < 0)
		goto done;
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto done;
	if (memBlock == gSharedFbManager.ports[0].memBlock)
		portIndex = 0;
	else if (memBlock == gSharedFbManager.ports[1].memBlock)
		portIndex = 1;
	else {
		result = (int)0x80200004;
		goto unlock;
	}
	port = &gSharedFbManager.ports[portIndex];
	result = ksceKernelCopyFromUser(&port->info.fb_base2, info, 64);
	if (result)
		goto unlock;
	firstPid = port->requestedPids[0];
	secondPid = port->requestedPids[1];
	thirdPid = port->requestedPids[2];
	fourthPid = port->requestedPids[3];
	port->assignedPids[0] = firstPid;
	port->assignedPids[1] = secondPid;
	port->assignedPids[2] = thirdPid;
	port->assignedPids[3] = fourthPid;
	firstPid = port->requestedPids[4];
	secondPid = port->requestedPids[5];
	thirdPid = port->requestedPids[6];
	fourthPid = port->requestedPids[7];
	port->assignedPids[4] = firstPid;
	port->assignedPids[5] = secondPid;
	port->assignedPids[6] = thirdPid;
	port->assignedPids[7] = fourthPid;
	count = 0;
	for (index = 0; index != 8; ++index) {
		processId = port->assignedPids[index];
		if (SHARED_FB_PROCESS_MATCHES(port, processId, 0) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 1) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 2) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 3) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 4) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 5) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 6) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 7) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 8) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 9) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 10) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 11) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 12) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 13) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 14) ||
		    SHARED_FB_PROCESS_MATCHES(port, processId, 15))
			++count;
	}
	result = ksceKernelCopyToUser(processCount, &count, sizeof(count));
	if (!result)
		gSharedFbManager.currentPort = port;
unlock:
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
done:
	sceSyscallLeave(syscallState);
	return result;
}

int sceSharedFbUpdateProcess(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	SceUID processId, beginEvent, endEvent;
	unsigned int index, previous, pattern, timeout, *timeoutPtr;
	unsigned int retryCount;
	int result, enabled, vsync;

	if (!ksceSblACMgrIsSystemProgram(0)) {
		result = (int)0x80010058;
		goto done;
	}
	if (gSharedFbManager.updateIndex == 16) {
		result = (int)0x80200006;
		goto done;
	}
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto done;
	port = gSharedFbManager.currentPort;
	processId = port->updatePids[gSharedFbManager.updateIndex];
	SHARED_FB_SELECT_PROCESS(port, processId, index, missing);
	process = &port->processes[index];
	beginEvent = port->beginEvent;
	endEvent = port->endEvent;
	vsync = (unsigned char)process->vsync;
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	do {
		previous = AtomicLoadReserved32(&process->state);
		if (previous != 1) {
			++gSharedFbManager.updateIndex;
			result = (int)0x80200008;
			goto done;
		}
	} while (AtomicStoreReserved32(&process->state, 2));
	pattern = 1U << index;
	retryCount = 0;
	ksceKernelSetEventFlag(beginEvent, pattern);
	timeout = 20000000;
	timeoutPtr = &timeout;
	for (;;) {
		result = ksceKernelWaitEventFlag(endEvent, pattern, 4, NULL, timeoutPtr);
		if (result != (int)0x80028005)
			break;
		if (_sceAppMgrIsExistAbortedAppOrDebugSuspendedApp()) {
			timeout = 20000000;
			continue;
		}
		SCE_DBG_PRINTF(0, 1772, "[SceSharedFb] Timeout pid=%#x\n", processId);
		if (ksceSblACMgrIsDevelopmentMode() &&
		    (ksceRegMgrGetKeyInt("/DEVENV/TOOL", "enable_sharedfb_timeout",
					 &enabled) || !enabled)) {
			timeoutPtr = NULL;
			continue;
		}
		if (!_sceAppMgrAbort(processId))
			continue;
		if (ksceKernelLockFastMutex(&gSharedFbManager.mutex))
			goto retry;
		SHARED_FB_SELECT_PROCESS(port, processId, index, retryUnlock);
		process = &port->processes[index];
		process->vsync = 0;
		do {
			previous = AtomicLoadReserved32(&process->state);
		} while (AtomicStoreReserved32(&process->state, 0));
		if (previous == 4 || previous == 2) {
			ksceKernelSetEventFlag(port->endEvent, 1U << index);
			if (previous == 4)
				SceSysrootForDriver_1B3FA457(processId);
		}
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		sceKernelKillProcess(processId, 0);
		continue;
retryUnlock:
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
retry:
		if (++retryCount != 1)
			ksceKernelSetEventFlag(beginEvent, pattern);
	}
	if (result) {
		ksceKernelLockFastMutex(&gSharedFbManager.mutex);
		++gSharedFbManager.updateIndex;
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		goto done;
	}
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto done;
	++gSharedFbManager.updateIndex;
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	result = vsync != 0;
	goto done;
missing:
	++gSharedFbManager.updateIndex;
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	result = (int)0x80200004;
done:
	sceSyscallLeave(syscallState);
	return result;
}

int sceSharedFbUpdateProcessEnd(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	SceUID event, processId;
	unsigned int index;
	int result;

	if (!ksceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	port = gSharedFbManager.currentPort;
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	SCE_DBG_ASSERT(result == 0, 1847);
	(void)result;
	event = port->beginEvent;
	for (index = 0; index != 16; ++index) {
		processId = port->processes[index].pid;
		if (processId != -1 && processId != port->assignedPids[0] &&
		    processId != port->assignedPids[1] &&
		    processId != port->assignedPids[2] &&
		    processId != port->assignedPids[3] &&
		    processId != port->assignedPids[4] &&
		    processId != port->assignedPids[5] &&
		    processId != port->assignedPids[6] &&
		    processId != port->assignedPids[7])
			ksceKernelSetEventFlag(event, 1U << index);
	}
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	ksceKernelClearEventFlag(event, 0);
	sceSyscallLeave(syscallState);
	return 0;
}

int sceSharedFbGetShellRenderPort(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int permission, result;

	if (ksceSblACMgrIsSystemProgram(0)) {
		permission = ksceKernelSetPermission(128);
		result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
		if (result) {
			ksceKernelSetPermission(permission);
			sceSyscallLeave(syscallState);
			return result;
		} else {
			int port = gSharedFbManager.shellRenderPort;

			ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
			result = port;
		}
		ksceKernelSetPermission(permission);
		sceSyscallLeave(syscallState);
	} else {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	return result;
}

int sceSharedFbGetRenderingInfo(SceUID processId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	struct SharedFbProcess *process;
	unsigned int portIndex, index;
	int result;

	if (!ksceSblACMgrIsSystemProgram(0)) {
		result = (int)0x80010058;
		goto done;
	}
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result)
		goto done;
	if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[0], processId))
		portIndex = 0;
	else if (SHARED_FB_CONTAINS_PID(&gSharedFbManager.ports[1], processId))
		portIndex = 1;
	else
		goto missing;
	port = &gSharedFbManager.ports[portIndex];
	SHARED_FB_SELECT_PROCESS(port, processId, index, missing);
	process = &port->processes[index];
	if (!process)
		goto missing;
	result = process->vsync ? 0 : (int)0x80200005;
	goto unlock;
missing:
	result = (int)0x80200004;
unlock:
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
done:
	sceSyscallLeave(syscallState);
	return result;
}

int sharedFbInitialize(void)
{
	int result;

	memset(&gSharedFbManager, 0, sizeof(gSharedFbManager));
	gSharedFbManager.ports[0].index = -1;
	gSharedFbManager.ports[1].index = -1;
	result = ksceKernelInitializeFastMutex(&gSharedFbManager.mutex,
					     "SceSharedFb", 2, NULL);
	if (result) {
		ksceKernelFinalizeFastMutex(&gSharedFbManager.mutex);
		return result;
	}
	ksceKernelRegisterProcEventHandler("SceSharedFb", &gSharedFbProcessHandler, 0);
	return 0;
}

int sharedFbGetMemBlockByIndex(int index, SceUID *memBlock)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result, portIndex;

	if (!memBlock) {
		sceSyscallLeave(syscallState);
		return (int)0x80200007;
	}
	*memBlock = -1;
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	portIndex = result;
	if (result) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (index != gSharedFbManager.ports[0].index) {
		if (index != gSharedFbManager.ports[1].index) {
			ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
			sceSyscallLeave(syscallState);
			return (int)0x80200004;
		}
		portIndex = 1;
	}
	*memBlock = gSharedFbManager.ports[portIndex].memBlock;
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	sceSyscallLeave(syscallState);
	return 0;
}

int sharedFbSetRequestedPids(SceUID memBlock, const SceUID pids[8])
{
	SceUInt32 syscallState = sceSyscallEnter();
	struct SharedFbPort *port;
	SceUID firstPid, secondPid, thirdPid, fourthPid;
	int result, portIndex;

	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (result) {
		sceSyscallLeave(syscallState);
		return result;
	}
	if (memBlock == gSharedFbManager.ports[0].memBlock)
		portIndex = 0;
	else if (memBlock == gSharedFbManager.ports[1].memBlock)
		portIndex = 1;
	else {
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		sceSyscallLeave(syscallState);
		return (int)0x80200004;
	}
	port = &gSharedFbManager.ports[portIndex];
	firstPid = pids[0];
	secondPid = pids[1];
	thirdPid = pids[2];
	fourthPid = pids[3];
	port->requestedPids[0] = firstPid;
	port->requestedPids[1] = secondPid;
	port->requestedPids[2] = thirdPid;
	port->requestedPids[3] = fourthPid;
	firstPid = pids[4];
	secondPid = pids[5];
	thirdPid = pids[6];
	fourthPid = pids[7];
	port->requestedPids[4] = firstPid;
	port->requestedPids[5] = secondPid;
	port->requestedPids[6] = thirdPid;
	port->requestedPids[7] = fourthPid;
	ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
	sceSyscallLeave(syscallState);
	return 0;
}

int sharedFbSetShellRenderPort(unsigned int port)
{
	int result;

	if (port > 1)
		return (int)0x80200007;
	result = ksceKernelLockFastMutex(&gSharedFbManager.mutex);
	if (!result) {
		gSharedFbManager.shellRenderPort = port;
		ksceKernelUnlockFastMutex(&gSharedFbManager.mutex);
		return 0;
	}
	return result;
}
