/* SceNetPs platform adapters, adjacent to Sony's system_os.c debug records. */
#include <psp2/registrymgr.h>
#include <acmgr/acmgr.h>
#include <sysmem/sblaimgr.h>
#include <psp2kern/kernel/rtc.h>
#include <psp2kern/kernel/ssmgr.h>
#include <sysmem/procevent.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot_netps.h>
#include <sysmem/sysevent.h>
#include "../../../../net_ps_internal.h"
#include "../../../../net_ps_atomic.h"
#include <processmgr/processmgr.h>
#include <sysmem/sysroot.h>

int ksceKernelIsGameBudget(void);
int ksceKernelUnregisterThreadEventHandler(SceUID handlerId);
int sceNetPsProcessCreateHandler(SceUID processId, const void *param,
				SceNetPsProcessHooks *hooks);
int sceNetPsProcessExitHandler(SceUID processId, const void *param,
			      SceNetPsProcessHooks *hooks);
int sceNetPsProcessKillHandler(SceUID processId, const void *param,
			      SceNetPsProcessHooks *hooks);
int sceNetPsProcessSuspendHandler(SceUID processId, int event,
		const void *param, SceNetPsProcessHooks *hooks);
int sceNetPsProcessResumeHandler(SceUID processId, int event,
		const void *param, SceNetPsProcessHooks *hooks);
int SceNetPsForDriver_83A12CCC(void *interface);
int SceNetPsForDriver_C000CA8F(void *interface);

int gNetPsFake3gInterface = -1;
SceNetPsSyncList gNetPsSyncList;
void (*gNetPsCryptoRelease)(void *);
void *(*gNetPsCryptoAllocate)(SceSize);
SceNetPsThreadEventHooks *gNetPsThreadEventHooks;

int sceNetPsTimingSafeCompare(const void *first, const void *second,
			    SceSize length)
{
	return timingsafe_memcmp(first, second, length);
}

/* SCE: install the allocator callbacks used by the crypto adapters. */
int init_crypto(void *(*allocate)(SceSize), void (*release)(void *))
{
	gNetPsCryptoAllocate = allocate;
	gNetPsCryptoRelease = release;
	return 0;
}

int sceNetPsThreadEntry(SceSize args, SceNetPsThreadParam *param)
{
	param->entry(param->arg);
	return 0;
}

int sceNetPsThreadEventHandler(int event, SceUID threadId, int argument,
			       void *common)
{
	SceNetPsThreadEventHooks *hooks = gNetPsThreadEventHooks;

	if (event != 8)
		return 0;
	if (hooks->exit)
		return hooks->exit(threadId);
	return 0;
}

int sceNetPsSysEventHandler(int resume, int event, void *args,
			    SceNetPsSysEventHooks *hooks)
{
	if (resume != 0) {
		if (resume != 1 || event != 0x400000)
			return 0;
		if (hooks->resume)
			return hooks->resume();
		return 0;
	}
	if (event != 0x200)
		return 0;
	if (hooks->suspend)
		return hooks->suspend();
	return 0;
}

int sceNetPsProcessAffinityHandler(int event, SceUID processId, void *args,
				  SceNetPsProcessHooks *hooks)
{
	switch (event) {
	case 16:
		return ksceKernelChangeThreadCpuAffinityMask(hooks->softThread,
							   0x80000);
	case 17:
		return ksceKernelChangeThreadCpuAffinityMask(hooks->softThread,
							   0xf0000);
	default:
		return 0;
	}
}

void sce_psnet_panic(const char *message, ...)
{
	SCE_PANIC(110);
}

int sceNetPsIsRetriedSyscall(void)
{
	return SceThreadmgrForDriver_20C228E4();
}

int sceNetPsIsApplicationProgram(int *error)
{
	if (error)
		*error = (int)0x80010058;
	if (!sceSblACMgrIsSystemProgram(0) && !sceSblACMgrIsKernelProgram(0))
		return 1;
	if (error)
		*error = 0;
	return 0;
}

int sceNetPsIsGameBudget(void)
{
	return ksceKernelIsGameBudget() != 0;
}

int sceNetPsIsDevelopmentMode(void)
{
	return SceSblACMgrForDriver_E87D1777() != 0;
}

int sceNetPsIsCEX(void)
{
	return SceSblAIMgrForDriver_D78B04A2() != 0;
}

int sceNetPsIsSdkVersion2OrLater(void)
{
	SceUInt32 sdkVersion;

	return SceProcessmgrForDriver_D141C076(0, &sdkVersion) >= 0 &&
	       sdkVersion >= 0x02000000;
}

int sceNetPsCopyUserOut(const void *source, void *destination, size_t length)
{
	return SceSysmemForDriver_6D88EF8A(destination, source, length);
}

int sceNetPsCopyUserIn(const void *source, void *destination, size_t length)
{
	return SceSysmemForDriver_BC996A7A(destination, source, length);
}

int sceNetPsCopyUserString(const char *source, char *destination, size_t length)
{
	return SceSysmemForDriver_DB3EC244(destination, source, length);
}

SceInt64 sceNetPsGetSystemTime(SceUInt64 *value)
{
	SceInt64 result = sceKernelGetSystemTimeWide();

	*value = result;
	return result;
}

int sceNetPsGetCurrentTick(SceUInt64 *tick)
{
	SceRtcTick current;
	int result;

	result = ksceRtcGetCurrentTick(&current);
	*tick = current.tick;
	return result;
}

unsigned int sceNetPsGetRandom(void)
{
	unsigned int value;

	ksceSblRngPseudoRandomNumber(&value, sizeof(value));
	return value;
}

int sceNetPsFormatUidName(SceUID uid, char *name, int length)
{
	signed char *cursor;
	int digit;

	if (length > 0) {
		cursor = (signed char *)name + length;
		do {
			while (1) {
				if (*cursor-- == '*')
					break;
				if (--length == 0)
					return uid;
			}
			digit = uid % 16;
			uid /= 16;
			if (digit > 9)
				cursor[1] = digit + 'A' - 10;
			else
				cursor[1] = digit + '0';
		} while (--length != 0);
	}
	return uid;
}

int sceNetPsCreateThread(SceUID *threadId, SceNetPsThreadParam *param,
			 const char *name)
{
	SceUID createdThread;
	int result;

	createdThread = ksceKernelCreateThread(name,
					     (SceKernelThreadEntry)sceNetPsThreadEntry,
					     param->initPriority, param->stackSize,
					     0, param->cpuAffinityMask, NULL);
	if (createdThread < 0)
		return createdThread;
	result = ksceKernelStartThread(createdThread, sizeof(*param), param);
	if (result >= 0)
		*threadId = createdThread;
	return result;
}

SceUID sceNetPsGetThreadId(void)
{
	SceUID threadId;
	SceUID result;

	threadId = ksceKernelGetThreadId();
	result = threadId & ~(threadId >> 31);
	if (threadId < 0)
		return -1;
	return result;
}

SceUID sceNetPsGetProcessId(void)
{
	SceUID processId;
	SceUID result;

	processId = sceKernelGetProcessId();
	result = processId & ~(processId >> 31);
	if (processId < 0)
		return -1;
	return result;
}

int sceNetPsExitThread(void)
{
	return sceKernelExitThread(0);
}

int sce_psnet_bnet_mutex_create(SceNetPsMutex *mutex, const char *name)
{
	SceUID eventFlag;

	eventFlag = ksceKernelCreateEventFlag(name, 0x1000, 0, NULL);
	if (eventFlag >= 0) {
		mutex->eventFlag = eventFlag;
		mutex->available = 1;
		mutex->waiters = 0;
		return 0;
	}
	return eventFlag;
}

int sce_psnet_bnet_mutex_lock(SceNetPsMutex *mutex, unsigned int *timeout)
{
	unsigned int waiters;
	unsigned int available;
	unsigned int resultBits;
	int result;

	netPsMemoryBarrier();
	do {
		waiters = netPsLoadExclusive(&mutex->waiters);
	} while (netPsStoreExclusive(&mutex->waiters, waiters + 1));
	while (1) {
		netPsMemoryBarrier();
		do {
			available = netPsLoadExclusive(&mutex->available);
		} while (netPsStoreExclusive(&mutex->available, 0));
		if (available)
			break;
		result = ksceKernelWaitEventFlag(mutex->eventFlag, 1, 5,
					       &resultBits, timeout);
		if (result < 0) {
			if (result != (int)0x80028005 &&
			    (result < (int)0x80028005 ||
			     (unsigned int)(result + 0x7ffd7fd4) > 1))
				SCE_PANIC(432);
			if (timeout)
				*timeout = -1;
			break;
		}
	}
	netPsMemoryBarrier();
	do {
		waiters = netPsLoadExclusive(&mutex->waiters);
	} while (netPsStoreExclusive(&mutex->waiters, waiters - 1));
	return available;
}

int sce_psnet_bnet_mutex_unlock(SceNetPsMutex *mutex)
{
	unsigned int available;

	netPsMemoryBarrier();
	do {
		available = netPsLoadExclusive(&mutex->available);
	} while (netPsStoreExclusive(&mutex->available, available | 1));
	if (mutex->waiters)
		return ksceKernelSetEventFlag(mutex->eventFlag, 1);
	return 0;
}

int sce_psnet_bnet_sync_create(SceNetPsSync *sync, unsigned int events,
			     const char *name)
{
	SceUID eventFlag;

	eventFlag = ksceKernelCreateEventFlag(name, 0x9000, 0, NULL);
	if (eventFlag < 0)
		return eventFlag;
	sync->eventFlag = eventFlag;
	sync->events = events;
	sync->waiters = 0;
	sync->processId = sceNetPsGetProcessId();
	sync->prev = gNetPsSyncList.tail;
	if (sync->prev)
		sync->prev->next = sync;
	else
		gNetPsSyncList.head = sync;
	gNetPsSyncList.tail = sync;
	sync->next = NULL;
	return 0;
}

int sce_psnet_bnet_sync_delete(SceNetPsSync *sync)
{
	SceNetPsSync *next = sync->next;

	if (next)
		next->prev = sync->prev;
	else
		gNetPsSyncList.tail = sync->prev;
	if (sync->prev)
		sync->prev->next = next;
	else
		gNetPsSyncList.head = next;
	ksceKernelDeleteEventFlag(sync->eventFlag);
	return 0;
}

int sce_psnet_bnet_sync_wait(SceNetPsSync *sync,
			   unsigned int *timeout, unsigned int flags)
{
	unsigned int waiters;
	unsigned int events;
	int result;

	netPsMemoryBarrier();
	do {
		waiters = netPsLoadExclusive(&sync->waiters);
	} while (netPsStoreExclusive(&sync->waiters, waiters + 1));
	while (1) {
		netPsMemoryBarrier();
		do {
			events = netPsLoadExclusive(&sync->events);
		} while (netPsStoreExclusive(&sync->events, 0));
		if (events)
			break;
		if (flags & 1)
			result = ksceKernelWaitEventFlagCB(sync->eventFlag, 1, 5,
						 NULL, timeout);
		else
			result = ksceKernelWaitEventFlag(sync->eventFlag, 1, 5,
					       NULL, timeout);
		if (result < 0) {
			if (result != (int)0x80028005 &&
			    (result < (int)0x80028005 ||
			     (unsigned int)(result + 0x7ffd7fd4) > 1))
				SCE_PANIC(575);
			if (timeout)
				*timeout = -1;
			break;
		}
	}
	netPsMemoryBarrier();
	do {
		waiters = netPsLoadExclusive(&sync->waiters);
	} while (netPsStoreExclusive(&sync->waiters, waiters - 1));
	return events;
}

unsigned int sce_psnet_bnet_sync_clear(SceNetPsSync *sync)
{
	unsigned int events;

	netPsMemoryBarrier();
	do {
		events = netPsLoadExclusive(&sync->events);
	} while (netPsStoreExclusive(&sync->events, 0));
	return events;
}

int sce_psnet_bnet_sync_signal(SceNetPsSync *sync, unsigned int mask)
{
	unsigned int events;

	netPsMemoryBarrier();
	do {
		events = netPsLoadExclusive(&sync->events);
	} while (netPsStoreExclusive(&sync->events, events | mask));
	if (sync->waiters)
		return ksceKernelSetEventFlag(sync->eventFlag, 1);
	return 0;
}

int sceNetPsBindCallback(SceNetPsDriverCallback *callback, SceUID callbackId,
			const char *name)
{
	callback->globalCallbackId = SceSysmemForDriver_45D22597(0, callbackId);
	callback->name = name;
	return 0;
}

void sceNetPsUnbindCallback(SceNetPsDriverCallback *callback)
{
}

int sceNetPsNotifyCallback(SceNetPsDriverCallback *callback,
			  unsigned int argument)
{
	return ksceKernelNotifyCallback(callback->globalCallbackId, argument);
}

SceUID bnet_puid_to_guid(SceUID puid)
{
	return SceSysmemForDriver_45D22597(0, puid);
}

unsigned int sceNetPsSyscallEnter(void)
{
	unsigned int state;

	state = sceSyscallEnter();
	sceKernelApiDeclareTypeUInt32();
	return state;
}

void sceNetPsSyscallLeave(unsigned int state)
{
	sceSyscallLeave(state);
}

/* Name unknown; allocate the arena and register platform event adapters. */
int sceNetPsInitializePlatform(int (*initialize)(SceUID, void *, int,
		SceNetPsThreadParam *, SceUID, SceUID, SceUID),
		SceNetPsSysEventHooks *systemHooks,
		SceNetPsThreadEventHooks *threadHooks,
		SceNetPsProcessHooks *processHooks)
{
	SceKernelAllocMemBlockKernelOpt option;
	SceKernelProcEventHandler handler;
	SceNetPsThreadParam thread;
	void *base;
	SceUID memoryId, systemId, threadId, processId;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.attr = 0x200000;
	memoryId = SceSysmemForDriver_C94850C9("SceNetPs", 0x1020d006,
					 0x100000, &option);
	if (memoryId < 0)
		return 2;
	if (SceSysmemForDriver_A841EDDA(memoryId, &base) < 0)
		goto freeMemory;
	systemId = sceKernelRegisterSysEventHandler("SceNetPs",
			(SceKernelSysEventHandler)sceNetPsSysEventHandler,
			systemHooks);
	if (systemId < 0)
		goto freeMemory;
	threadId = SceThreadmgrForDriver_FD6150D5("SceNetPs", 0x10025, -1,
			8, (uintptr_t)sceNetPsThreadEventHandler, 0);
	gNetPsThreadEventHooks = threadHooks;
	if (threadId < 0)
		goto unregisterSystem;
#if FW_VERSION < 0x04000000
	memset(&handler, 0, sizeof(handler));
#endif
	handler.size = sizeof(handler);
	handler.create = (typeof(handler.create))sceNetPsProcessCreateHandler;
	handler.exit = (typeof(handler.exit))sceNetPsProcessExitHandler;
	handler.kill = (typeof(handler.kill))sceNetPsProcessKillHandler;
	handler.suspend = (typeof(handler.suspend))sceNetPsProcessSuspendHandler;
	handler.resume = (typeof(handler.resume))sceNetPsProcessResumeHandler;
	handler.notify =
		(typeof(handler.notify))sceNetPsProcessAffinityHandler;
	processId = sceKernelRegisterProcEventHandler("SceNetPs", &handler,
						     (int)processHooks);
	if (processId < 0)
		goto unregisterThread;
	thread.name = "SceNetSoft";
	thread.initPriority = 63;
	thread.stackSize = 0x2000;
	thread.cpuAffinityMask = 0xf0000;
	if (initialize(memoryId, base, 0x100000, &thread, systemId, threadId,
		       processId) < 0)
		goto unregisterProcess;
	SceSysrootForDriver_ECADF8C3(SceNetPsForDriver_83A12CCC);
	SceSysrootForDriver_A799F22A(SceNetPsForDriver_C000CA8F);
	SceSysrootForDriver_27D30DB2(SceNetPsForDriver_83A12CCC);
	SceSysrootForDriver_D4457D4F(SceNetPsForDriver_C000CA8F);
	return 0;

unregisterProcess:
	SceProcEventForDriver_3DED57CC(processId);
unregisterThread:
	ksceKernelUnregisterThreadEventHandler(threadId);
unregisterSystem:
	sceKernelUnregisterSysEventHandler(systemId);
freeMemory:
	SceSysmemForDriver_009E1C61(memoryId);
	return 2;
}

int sceNetPsGetFake3gInterface(void)
{
	if (gNetPsFake3gInterface >= 0)
		return gNetPsFake3gInterface;
	if (sceRegMgrGetKeyInt("/CONFIG/NET", "fake_3g_if",
			       &gNetPsFake3gInterface) >= 0)
		return gNetPsFake3gInterface;
	gNetPsFake3gInterface = 0;
	return 0;
}
