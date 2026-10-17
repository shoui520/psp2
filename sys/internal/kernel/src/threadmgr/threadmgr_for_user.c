/*
 * SceKernelThreadMgr: threadmgr_for_user.c, the user side of the thread
 * manager's objects. User code names objects by PUID; these translate them
 * to GUIDs and call the kernel functions, with errors marked for the
 * syscall (SYSCALL_ERROR_BIT). Some live with their objects' files.
 */

#include <string.h>
#include "private.h"
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include "event.h"
#include "lw_mutex.h"
#include "scheduler.h"
#include "syscall_context.h"
#include "affinity.h"
#include "priority_change.h"
#include <intrmgr/intrmgr.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <psp2kern/kernel/sysmem/uid_puid.h>
#include "event_flag.h"
#include "timer.h"
#include <threadmgr/uid_waitable.h>

int ksceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask);
int ksceKernelChangeThreadPriority(SceUID uid, uint32_t priority);
int ksceKernelGetThreadCpuAffinityMask(SceUID uid);
int __attribute__((noinline)) sceKernelWaitSignal(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout);
int __attribute__((noinline)) sceKernelWaitSignalCB(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout);
int sendSignal(SceUID threadId);

/* PUID attribute: the PUID came from an open, not a create */
#define SDK_VERSION_CLOSE_MISMATCH	0x03100000	/* SDKs before 3.10 may close what they created */
#define EVENT_CLOSE_DELETE		0x00000008	/* closeMsgPipe/SimpleEvent: a delete */
#define TLS_SLOT_VFP_EXCEPTION		4
#define WME_MAX_USER_EVENTS		64
#define WME_MODE_MASK			0x00000003
#define THREAD_ID_ALL_USER		0x00010027	/* PLACEHOLDER: name; every user thread */

/* the user errors: the kernel's with SYSCALL_ERROR_BIT */
#define USER_ERROR_UNKNOWN_THREAD_ID	(SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT)
#define USER_ERROR_UNKNOWN_SEMA_ID	0xc0028101
#define USER_ERROR_UNKNOWN_MUTEX_ID	0xc0028141
#define USER_ERROR_UNKNOWN_CALLBACK_ID	0xc00280a1
#define USER_ERROR_UNKNOWN_THREAD_EVENT_ID	0xc0028061
#define SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID	0x80028201
#define SCE_KERNEL_ERROR_ILLEGAL_ADDR	0x80020006
#define SCE_KERNEL_ERROR_ILLEGAL_SIZE	0x8002000b
#define SCE_KERNEL_ERROR_ILLEGAL_MODE	0x80020010
#define SCE_KERNEL_ERROR_ILLEGAL_PATTERN	0x8002000d
#define USER_ERROR_UNKNOWN_COND_ID	0xc00281a1
#define USER_ERROR_UNKNOWN_LW_MUTEX_ID	0xc0028181
#define USER_ERROR_UNKNOWN_LW_COND_ID	0xc00281c1
#define USER_ERROR_ILLEGAL_ADDR		(SCE_KERNEL_ERROR_ILLEGAL_ADDR | SYSCALL_ERROR_BIT)
#define MSG_PIPE_CLOSE_DELETE		0x00000008	/* closeMsgPipeForUser's patterns */
#define MSG_PIPE_CLOSE_CLOSE		0x00000200
#define SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS	0x80028002
#define UID_NAME_CHECK_LENGTH		0x80	/* copyinUIDNameWithAttr: a name too long is an error */
#define EVENT_PATTERN_KERNEL_MASK	0x00ffffff	/* bits the user may not pulse or set */
#define SCE_KERNEL_ERROR_NO_MEMORY	0x8002710b
#define USER_ERROR_UNKNOWN_RW_LOCK_ID	0xc00281e1
#define USER_ERROR_UNKNOWN_MSG_PIPE_ID	0xc0028221
#define USER_ERROR_UNKNOWN_SIMPLE_EVENT_ID	0xc0028261

extern int scePUIDGetAttr(SceUID processId, SceUID uid, uint32_t *attributes);
extern int kscePUIDGetClass(SceUID processId, SceUID uid, SceClass **cls);
extern int kscePUIDClose(SceUID processId, SceUID uid);
extern SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern int SceSysrootForDriver_CAE820B2(SceUID processId, SceUInt32 *version);
extern void *ksceKernelGetTLSAddr(unsigned int index);
int copyinUIDNameWithAttr(char *name, const char *userName, uint32_t attributes);
int ksceKernelDeleteThread(SceUID threadId);
int ksceKernelStartThread(SceUID threadId, SceSize argSize, const void *arg);
int ksceKernelGetThreadStackFreeSize(SceUID threadId);
int ksceKernelGetThreadInfo(SceUID threadId, void *info);
int SceThreadmgrForDriver_33E85E9E(void *status);
int SceThreadmgrForDriver_7E280B69(void *info);
int ksceKernelGetThreadmgrUIDClass(SceUID uid);
uint32_t ksceKernelChangeThreadVfpException(uint32_t clear, uint32_t set);
SceUID ksceKernelCreateCallback(const char *name, SceUInt32 attr, uintptr_t entry, void *common);
int ksceKernelDeleteCallback(SceUID callbackId);
int ksceKernelNotifyCallback(SceUID callbackId, int arg);
int SceThreadmgrForDriver_FDC044AC(SceUID callbackId, ThreadmgrCallbackInfo *info);
extern SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid);
int ksceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId, uint32_t notifyId);
int ksceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId);
int ksceKernelUnregisterCallbackFromEventAll(SceUID eventId);
SceUID registerThreadEventHandler(const char *name, SceUID target, SceUID pid, uint32_t mask,
				  uintptr_t handler, uint32_t arg, int user);
int sceKernelUnregisterThreadEventHandler(SceUID uid);
int SceThreadmgrForDriver_6657429E(SceUID uid, void *info);
int ksceKernelWaitEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			uint64_t *outUserData, SceUInt32 *timeout);
int ksceKernelWaitEventCB(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			  uint64_t *outUserData, SceUInt32 *timeout);
int sceKernelPollEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
		       uint64_t *outUserData);
int sceKernelCancelEvent(SceUID eventId, int *numWaitThreads);
extern void *ksceKernelAllocHeapMemory2(SceUID pid, SceSize size, SceAllocOpt *opt);
extern int SceSysrootForKernel_7968F091(SceUID pid, void *ptr);
int SceThreadmgrForDriver_20C228E4(void);
int ksceKernelPulseEvent(SceUID eventId, uint32_t pattern, uint64_t userData);
int ksceKernelPulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern, uint64_t userData,
					   uint32_t arg);
int sceKernelCancelEventWithSetPattern(SceUID eventId, uint32_t setPattern, uint64_t userData,
				       int *numWaitThreads);
int sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern);
int SceThreadmgrForDriver_CBA7FAAA(SceUID eventId, ThreadmgrEventInfo *info);
int ksceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			    SceUInt32 *timeout);
int ksceKernelWaitEventFlagCB(SceUID evfId, uint32_t pattern, uint32_t mode, uint32_t *outPattern,
			      SceUInt32 *timeout);
extern SceUID SceSysmemForKernel_BDE00106(const char *name);
extern SceClass *SceSysmemForKernel_368F1C36(SceUID uid);
extern int ksceKernelStrncpyFromUser(char *dest, const char *source, unsigned int size);
int ksceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout);
int SceThreadmgrForDriver_F55E4D86(SceUID semaId, int needCount, SceUInt32 *timeout);
int sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads);
int ksceKernelGetSemaInfo(SceUID semaId, void *info);
int ksceKernelPollSema(SceUID semaId, int needCount);
int ksceKernelSignalSema(SceUID semaId, int signalCount);
SceUID ksceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount, const void *option);
SceUID ksceKernelCreateCond(const char *name, SceUInt32 attr, SceUID mutexId, const void *option);
int ksceKernelWaitCond(SceUID condId, SceUInt32 *timeout);
int waitCondCB(SceUID condId, SceUInt32 *timeout);
int SceThreadmgrForDriver_D1FBDFA6(SceUID condId, void *info);
int ksceKernelSignalCond(SceUID condId);
int ksceKernelSignalCondAll(SceUID condId);
int ksceKernelSignalCondTo(SceUID condId, SceUID threadId);
SceUID SceThreadmgrForDriver_88206D29(const char *name);
int ksceKernelSendMsgPipeVector(SceUID msgPipeId, const void *vector, int count, uint32_t mode,
				SceSize *result, SceUInt32 *timeout);
int SceThreadmgrForDriver_2FC0FFF6(SceUID msgPipeId, const void *vector, int count, uint32_t mode,
				   SceSize *result, SceUInt32 *timeout);
int ksceKernelTrySendMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				   uint32_t mode, SceSize *result);
int ksceKernelReceiveMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				   uint32_t mode, SceSize *result, SceUInt32 *timeout);
int ksceKernelReceiveMsgPipeVectorCB(SceUID msgPipeId, const void *vector, int count,
				     uint32_t mode, SceSize *result, SceUInt32 *timeout);
int ksceKernelTryReceiveMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				      uint32_t mode, SceSize *result);
int ksceKernelCancelMsgPipe(SceUID msgPipeId, int *numSendWaitThreads,
			    int *numReceiveWaitThreads);
int ksceKernelGetMsgPipeInfo(SceUID msgPipeId, void *info);
int __sceKernelCreateLwCond(void *work, const char *name, SceUInt32 attr, SceUID lwMutexPuid,
			    const void *option);
int __sceKernelWaitLwCond(SceUID condId, SceUInt32 *timeout, int callbacks);
int __sceKernelSignalLwCond(SceUID condId);
int __sceKernelSignalLwCondAll(SceUID condId);
int __sceKernelSignalLwCondTo(SceUID condId, SceUID threadId);
SceUID sceKernelCreateTimer(const char *name, SceUInt32 attr, const void *option);
extern void sceKernelApiDeclareTypeUInt32(void);
extern void sceKernelApiDeclareTypeUInt64(void);
extern uint32_t SceSysrootForKernel_41636522(void);
extern uint64_t SceSysrootForKernel_E20F6FC8(void);
int SceThreadmgrForDriver_3F5E1D56(SceUID rwLockId, SceUInt32 *timeout);
int SceThreadmgrForDriver_CFA2FFAF(SceUID rwLockId, SceUInt32 *timeout);
int ksceKernelTryLockReadRWLock(SceUID rwLockId);
int ksceKernelUnlockReadRWLock(SceUID rwLockId);
int SceThreadmgrForDriver_A64B00F6(SceUID rwLockId, SceUInt32 *timeout);
int SceThreadmgrForDriver_54566860(SceUID rwLockId, SceUInt32 *timeout);
int ksceKernelTryLockWriteRWLock(SceUID rwLockId);
int ksceKernelUnlockWriteRWLock(SceUID rwLockId);
int sceKernelCancelRWLock(SceUID rwLockId, int *numReadWaitThreads, int *numWriteWaitThreads,
			  SceUInt32 type);
int sceKernelGetRWLockInfo(SceUID rwLockId, void *info);
int SceThreadmgrForKernel_170F69D6(SceUID threadId, uint32_t counter, uint32_t *value);
void *ksceKernelGetThreadTLSAddr(SceUID uid, unsigned int index);
int _sceKernelLockLwMutex(void *work, int count, SceUInt32 *timeout, int callbacks);
int _sceKernelUnlockLwMutex(void *work, int count);
int ksceKernelWaitThreadEnd(SceUID threadId, int *status, SceUInt32 *timeout);
int ksceKernelWaitThreadEndCB(SceUID threadId, int *status, SceUInt32 *timeout);
int sceKernelGetThreadExitStatus(SceUID uid, int *status);
int ksceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout);
int ksceKernelLockMutexCB_089(SceUID mutexId, int count, SceUInt32 *timeout);
int ksceKernelTryLockMutex(SceUID mutexId, int count);
int ksceKernelUnlockMutex(SceUID mutexId, int count);
int ksceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads);
extern SceUID kscePUIDOpenByGUIDWithFlags(SceUID pid, SceUID guid, int flags);
SceUID ksceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				 const void *option);
SceUID ksceKernelCreateSema(const char *name, SceUInt32 attr, int initCount, int maxCount,
			    const void *option);
SceUID sceKernelCreateSema_16XX(const char *name, SceUInt32 attr, int initCount, int maxCount,
				const void *option);
SceUID sceKernelCreateMsgPipeWithLR(const char *name, int memType, SceUInt32 attr,
				    SceSize bufferSize, const void *option, const void *lr);
SceUID sceKernelCreateRWLock(const char *name, SceUInt32 attr, const void *option);
SceUID ksceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				   const void *option);
int _sceKernelCreateLwMutexForUser(void *work, const char *name, SceUInt32 attr, int initCount,
				   const void *option);
int sceKernelDeleteLwMutexForUser(void *work);
int setThreadContextForVMForUser(SceUID threadId, const void *cpu, const void *vfp);
int getThreadContextForVMForUser(SceUID threadId, void *cpu, void *vfp);
int waitExceptionForUser(uint32_t mask, void *info, SceUInt32 *timeout);
int waitExceptionCBForUser(uint32_t mask, void *info, SceUInt32 *timeout);
int waitMultipleEvents(ThreadmgrWaitMultipleEvents *wme, SceUInt32 *timeout, int callbacks,
		       uint32_t *outPattern);

/*
 * Deletes (pattern EVENT_CLOSE_DELETE) or closes a user's message pipe; the
 * pipe's event is set with the pattern so its waiters see it go.
 */
int closeMsgPipeForUser(SceUID uid, uint32_t pattern)
{
	struct ThreadCB *thread;
	ThreadmgrEvent *event;
	uint32_t attributes, version;
	SceUID guid;
	int intr, ret, wake;

	intr = ksceKernelCpuSuspendIntr();
	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0 || ksceGUIDReferObjectWithClass(guid,
		s_threadmgrRuntime.manager->msgPipeClass, (SceObjectBase **)&event) < 0) {
		ret = USER_ERROR_UNKNOWN_MSG_PIPE_ID;
		goto resume;
	}
	if (scePUIDGetAttr(0, uid, &attributes) < 0)
		goto invalid;
	if (!(pattern & EVENT_CLOSE_DELETE))
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			wake = 0;
			ret = USER_ERROR_UNKNOWN_MSG_PIPE_ID;
			SCE_DBG_PRINTF(0, 2638,
				"*** error: closeMsgPipeForUser(0x%08X, 0x%08X), delete/close mismatch\n",
				uid, pattern);
			goto release;
		}
	}
	if (kscePUIDClose(0, uid) < 0)
		goto invalid;
	thread = currentThreadContext();
	ret = 0;
	ksceKernelSpinlockLowLock(&event->lock);
	wake = setEventWithNotifyCallback(guid, pattern,
		((uint64_t)thread->userThreadId << 32) | (int64_t)currentThreadContext()->processId,
		pattern, event);
	ksceKernelSpinlockLowUnlock(&event->lock);
	goto release;
invalid:
	wake = 0;
	ret = USER_ERROR_UNKNOWN_MSG_PIPE_ID;
release:
	if (ksceGUIDReleaseObject(guid) < 0)
		SCE_PANIC(2664);
	if (wake & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* the same for a simple event */
int closeSimpleEventForUser(SceUID uid, uint32_t pattern)
{
	struct ThreadCB *thread;
	ThreadmgrEvent *event;
	uint32_t attributes, version;
	SceUID guid;
	int intr, ret, wake;

	intr = ksceKernelCpuSuspendIntr();
	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0 || ksceGUIDReferObjectWithClass(guid,
		s_threadmgrRuntime.manager->simpleEventClass,
		(SceObjectBase **)&event) < 0) {
		ret = USER_ERROR_UNKNOWN_SIMPLE_EVENT_ID;
		goto resume;
	}
	if (scePUIDGetAttr(0, uid, &attributes) < 0)
		goto invalid;
	if (!(pattern & EVENT_CLOSE_DELETE))
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			wake = 0;
			ret = USER_ERROR_UNKNOWN_SIMPLE_EVENT_ID;
			SCE_DBG_PRINTF(0, 4129,
				"*** error: closeSimpleEventForUser(0x%08X, 0x%08X), delete/close mismatch\n",
				uid, pattern);
			goto release;
		}
	}
	if (kscePUIDClose(0, uid) < 0)
		goto invalid;
	thread = currentThreadContext();
	ret = 0;
	ksceKernelSpinlockLowLock(&event->lock);
	wake = setEventWithNotifyCallback(guid, pattern,
		((uint64_t)thread->userThreadId << 32) | (int64_t)currentThreadContext()->processId,
		pattern, event);
	ksceKernelSpinlockLowUnlock(&event->lock);
	goto release;
invalid:
	wake = 0;
	ret = USER_ERROR_UNKNOWN_SIMPLE_EVENT_ID;
release:
	if (ksceGUIDReleaseObject(guid) < 0)
		SCE_PANIC(4155);
	if (wake & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intr);
	return ret;
}

/* deletes or closes a user's semaphore */
int closeSemaForUser(SceUID uid, int delete)
{
	SceClass *cls;
	uint32_t attributes, version;

	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->semaphoreClass ||
	    scePUIDGetAttr(0, uid, &attributes) < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	if (!delete)
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			SCE_DBG_PRINTF(0, 1842,
				"*** error: closeSemaForUser(0x%08X, 0x%08X), delete/close mismatch\n",
				uid, delete);
			return USER_ERROR_UNKNOWN_SEMA_ID;
		}
	}
	return kscePUIDClose(0, uid);
}

/* deletes or closes a user's mutex */
int closeMutexForUser(SceUID uid, int delete)
{
	SceClass *cls;
	uint32_t attributes, version;

	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->mutexClass ||
	    scePUIDGetAttr(0, uid, &attributes) < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	if (!delete)
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			SCE_DBG_PRINTF(0, 2114,
				"*** error: closeMutexForUser(0x%08X, 0x%08X), delete/close mismatch\n",
				uid, delete);
			return USER_ERROR_UNKNOWN_MUTEX_ID;
		}
	}
	return kscePUIDClose(0, uid);
}

/* deletes or closes a user's rw lock */
int closeRWLockForUser(SceUID uid, int delete)
{
	SceClass *cls;
	uint32_t attributes, version;

	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->rwLockClass ||
	    scePUIDGetAttr(0, uid, &attributes) < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	if (!delete)
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			SCE_DBG_PRINTF(0, 3824,
				"*** error: closeRWLockForUser(0x%08X, 0x%08X), delete/close mismatch\n",
				uid, delete);
			return USER_ERROR_UNKNOWN_RW_LOCK_ID;
		}
	}
	return kscePUIDClose(0, uid);
}

/* sceKernelDeleteThread (user) */
int sceKernelDeleteThread(SceUID threadId)
{
	SceUID guid;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	return ksceKernelDeleteThread(guid);
}

/* sceKernelStartThread (user) */
int sceKernelStartThreadForUser(SceUID threadId, SceSize argSize, const void *arg)
{
	SceUID guid;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	return ksceKernelStartThread(guid, argSize, arg);
}

/* sceKernelGetThreadStackFreeSize (user): 0 is the calling thread */
int sceKernelGetThreadStackFreeSize(SceUID threadId)
{
	SceUID guid = 0;

	if (threadId != 0) {
		guid = kscePUIDtoGUID(0, threadId);
		if (guid < 0)
			return USER_ERROR_UNKNOWN_THREAD_ID;
	}
	return ksceKernelGetThreadStackFreeSize(guid);
}

/* the core of _sceKernelGetThreadInfo: 0 is the calling thread */
int sceKernelGetThreadInfoForUser(SceUID threadId, void *info)
{
	SceUID guid = 0;

	if (threadId != 0) {
		guid = kscePUIDtoGUID(0, threadId);
		if (guid < 0)
			return USER_ERROR_UNKNOWN_THREAD_ID;
	}
	return ksceKernelGetThreadInfo(guid, info);
}

/* the core of _sceKernelGetThreadRunStatus */
int sceKernelGetThreadRunStatusForUser(void *status)
{
	return SceThreadmgrForDriver_33E85E9E(status);
}

/* the core of _sceKernelGetSystemInfo */
int sceKernelGetSystemInfoForUser(void *info)
{
	return SceThreadmgrForDriver_7E280B69(info);
}

/* sceKernelGetThreadmgrUIDClass (user) */
int sceKernelGetThreadmgrUIDClass(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return guid | SYSCALL_ERROR_BIT;
	return ksceKernelGetThreadmgrUIDClass(guid);
}

/* sceKernelChangeThreadVfpException (user): the new state also goes to the user TLS */
int sceKernelChangeThreadVfpException(uint32_t clear, uint32_t set)
{
	struct ThreadCB *thread = currentThreadContext();
	void *tls;
	int ret, err;

	ret = ksceKernelChangeThreadVfpException(clear, set);
	if (ret < 0)
		return ret;
	SCE_DBG_ASSERT_CHECK_FUNC(thread != NULL, 410, "sceKernelChangeThreadVfpExceptionForUser");
	tls = ksceKernelGetTLSAddr(TLS_SLOT_VFP_EXCEPTION);
	SCE_DBG_ASSERT_CHECK_FUNC(tls != NULL, 413, "sceKernelChangeThreadVfpExceptionForUser");
	err = ksceKernelCopyToUser(tls, &thread->vfpException, sizeof(thread->vfpException));
	if (err < 0) {
		SCE_DBG_ASSERT_CHECK_FUNC(0, 417, "sceKernelChangeThreadVfpExceptionForUser");
		ret = err;
	}
	return ret;
}

/* sceKernelCreateCallback (user) */
SceUID sceKernelCreateCallback(const char *userName, SceUInt32 attr, uintptr_t entry,
			       void *common)
{
	char name[32];
	int ret;

	ret = copyinUIDNameWithAttr(name, userName, attr);
	if (ret < 0)
		return ret;
	return ksceKernelCreateCallback(name, attr, entry, common);
}

/* sceKernelDeleteCallback (user) */
int sceKernelDeleteCallback(SceUID callbackId)
{
	SceUID guid = kscePUIDtoGUID(0, callbackId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_CALLBACK_ID;
	return ksceKernelDeleteCallback(guid);
}

/* sceKernelNotifyCallback (user) */
int sceKernelNotifyCallback(SceUID callbackId, int arg)
{
	SceUID guid = kscePUIDtoGUID(0, callbackId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_CALLBACK_ID;
	return ksceKernelNotifyCallback(guid, arg);
}

/* the core of _sceKernelGetCallbackInfo: the user sees its own PUID */
int sceKernelGetCallbackInfoForUser(SceUID callbackId, ThreadmgrCallbackInfo *info)
{
	SceUID guid = kscePUIDtoGUID(0, callbackId);
	int ret;

	if (guid < 0)
		return USER_ERROR_UNKNOWN_CALLBACK_ID;
	ret = SceThreadmgrForDriver_FDC044AC(guid, info);
	if (ret >= 0)
		info->callbackId = callbackId;
	return ret;
}

/* sceKernelRegisterCallbackToEvent (user): the event's PUID is the notify ID */
int sceKernelRegisterCallbackToEvent(SceUID eventId, SceUID callbackId)
{
	SceUID event, callback;

	event = kscePUIDtoGUID(0, eventId);
	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	callback = kscePUIDtoGUID(0, callbackId);
	if (callback < 0)
		return USER_ERROR_UNKNOWN_CALLBACK_ID;
	return ksceKernelRegisterCallbackToEvent(event, callback, eventId);
}

/* sceKernelUnregisterCallbackFromEvent (user) */
int sceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID callbackId)
{
	SceUID event, callback;

	event = kscePUIDtoGUID(0, eventId);
	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	callback = kscePUIDtoGUID(0, callbackId);
	if (callback < 0)
		return USER_ERROR_UNKNOWN_CALLBACK_ID;
	return ksceKernelUnregisterCallbackFromEvent(event, callback);
}

/* sceKernelUnregisterCallbackFromEventAll (user) */
int sceKernelUnregisterCallbackFromEventAll(SceUID eventId)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return ksceKernelUnregisterCallbackFromEventAll(event);
}

/*
 * The core of _sceKernelRegisterThreadEventHandler: a handler on one thread
 * (PUID) or on all user threads; the user gets a PUID of the handler.
 */
SceUID sceKernelRegisterThreadEventHandlerForUser(const char *userName, SceUID target,
						  uint32_t mask, uintptr_t handler, uint32_t arg)
{
	char name[32];
	SceUID pid, uid;

	uid = copyinUIDNameWithAttr(name, userName, 0);
	if (uid < 0)
		return uid;
	if (target == THREAD_ID_ALL_USER) {
		pid = 0;
	} else {
		if (target != 0) {
			target = kscePUIDtoGUID(0, target);
			if (target < 0)
				return USER_ERROR_UNKNOWN_THREAD_ID;
		}
		pid = -1;
	}
	uid = registerThreadEventHandler(name, target, pid, mask, handler, arg, 1);
	if (uid >= 0)
		uid = kscePUIDOpenByGUID(currentThreadContext()->processId, uid);
	return uid;
}

/* sceKernelUnregisterThreadEventHandler (user): its PUID closes too */
int sceKernelUnregisterThreadEventHandlerForUser(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);
	int ret;

	if (guid < 0)
		return USER_ERROR_UNKNOWN_THREAD_EVENT_ID;
	ret = sceKernelUnregisterThreadEventHandler(guid);
	if (ret >= 0) {
		ret = kscePUIDClose(0, uid);
		SCE_DBG_ASSERT_CHECK(ret >= 0, 698);
	}
	return ret;
}

/* the core of _sceKernelGetThreadEventInfo */
int sceKernelGetThreadEventInfoForUser(SceUID uid, void *info)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_THREAD_EVENT_ID;
	return SceThreadmgrForDriver_6657429E(guid, info);
}

/* the cores of _sceKernelWaitEvent and _sceKernelWaitEventCB */
int sceKernelWaitEventForUser(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			      uint64_t *outUserData, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return ksceKernelWaitEvent(guid, pattern, outPattern, outUserData, timeout);
}

int sceKernelWaitEventCBForUser(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
				uint64_t *outUserData, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return ksceKernelWaitEventCB(guid, pattern, outPattern, outUserData, timeout);
}

/* the core of _sceKernelPollEvent */
int sceKernelPollEventForUser(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			      uint64_t *outUserData)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return sceKernelPollEvent(guid, pattern, outPattern, outUserData);
}

/* the core of _sceKernelCancelEvent */
int sceKernelCancelEventForUser(SceUID eventId, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return sceKernelCancelEvent(guid, numWaitThreads);
}

/* a common info header: its size, then the object's UID */
struct ThreadmgrUserInfoHeader {
	SceSize size;
	SceUID uid;
	char name[32];
	uint32_t attr;				/* 0x28 */
};

enum ThreadmgrUserObjectClass {
	USER_OBJECT_SEMAPHORE,
	USER_OBJECT_MUTEX,
	USER_OBJECT_COND,
	USER_OBJECT_RW_LOCK,
};

/* opens the named object in the calling process */
static inline __attribute__((always_inline)) SceUID openUIDByName(
	const char *userName, enum ThreadmgrUserObjectClass kind)
{
	UIDCreateOpt opt;
	char name[32];
	SceClass *cls, *actualClass;
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, UID_NAME_CHECK_LENGTH);
	if (uid < 0)
		return uid;
	uid = SceSysmemForKernel_BDE00106(name);
	if (uid < 0)
		return uid;
	actualClass = SceSysmemForKernel_368F1C36(uid);
	switch (kind) {
	case USER_OBJECT_SEMAPHORE:
		cls = s_threadmgrRuntime.manager->semaphoreClass;
		break;
	case USER_OBJECT_MUTEX:
		cls = s_threadmgrRuntime.manager->mutexClass;
		break;
	case USER_OBJECT_COND:
		cls = s_threadmgrRuntime.manager->condClass;
		break;
	case USER_OBJECT_RW_LOCK:
		cls = s_threadmgrRuntime.manager->rwLockClass;
		break;
	}
	if (cls != actualClass)
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	opt = (UIDCreateOpt){ .flags = UID_CREATE_OPT_ATTR, .attr = PUID_ATTR_OPENED };
	return SceSysmemForKernel_1FF2749A(currentThreadContext()->processId, uid, &opt);
}

/* a syscall's extra arguments for an info call: the size of the user's buffer */
typedef struct ThreadmgrSyscallInfoArgs {
	SceSize size;
	uint32_t reserved;
} ThreadmgrSyscallInfoArgs;

/* a syscall's extra arguments: the ones past the fourth, read from the user */
typedef struct ThreadmgrSyscallArgs {
	uint32_t arg[4];
} ThreadmgrSyscallArgs;

#define VM_CPU_CONTEXT_SIZE		0x58
#define VM_VFP_CONTEXT_SIZE		0x108
#define EXCEPTION_INFO_SIZE		0x18
#define LW_COND_WORK_SIZE		0x10
#define LW_MUTEX_WORK_SIZE		0x20
#define LW_COND_INFO_SIZE		0x38
#define MSG_PIPE_OPT_SIZE		0x14

/* the caller's PUID of an object it created */
static inline SceUID OpenByGUID(SceUID pid, SceUID guid)
{
	SceUID puid = kscePUIDOpenByGUIDWithFlags(pid, guid, 1);

	SCE_DBG_PRINTF_LEVEL(2, 3, 105, "0x%08x=scePUIDOpenByGUID(0x%08x)\n", puid, guid);
	return puid;
}

/* an event's GUID and the PUID the user named it by */
struct ThreadmgrUserEventId {
	SceUID guid;
	SceUID puid;
};

/*
 * sceKernelWaitMultipleEvents (user): the WME and copies of the
 * user's arrays live in the process's heap; a wait a request broke keeps
 * them on the thread (restartWaitMultipleEvents) for the restarted syscall.
 */
int sceKernelWaitMultipleEventsForUser(const ThreadmgrWaitEvent *userEvents, SceUInt32 count,
				       SceUInt32 mode, ThreadmgrWaitEventResult *userResults,
				       SceUInt32 *userTimeout)
{
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrWaitEventResult *results;
	struct ThreadmgrUserEventId *ids;
	ThreadmgrWaitEvent *events;
	SceUInt32 timeout, *timeoutPtr;
	SceAllocOpt opt;
	SceUID pid, guid;
	int ret, err, done;
	SceUInt32 i, j;

	pid = currentThreadContext()->processId;
	thread = currentThreadContext();
	if (userEvents == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (count > WME_MAX_USER_EVENTS)
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	if (mode & ~WME_MODE_MASK)
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	if (SceThreadmgrForDriver_20C228E4() == 0) {
		events = ksceKernelAllocHeapMemory2(pid, count * sizeof(*events), NULL);
		if (events == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		ret = ksceKernelCopyFromUser(events, userEvents, count * sizeof(*events));
		if (ret < 0) {
			done = 0;
			goto freeEvents;
		}
		ids = ksceKernelAllocHeapMemory2(pid, count * sizeof(*ids), NULL);
		if (ids == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			done = 0;
			goto freeEvents;
		}
		for (i = 0; i < count; i++) {
			ids[i].puid = events[i].eventId;
			guid = kscePUIDtoGUID(pid, events[i].eventId);
			if (guid < 0) {
				events[i].eventId = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
				ids[i].guid = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
			} else {
				ids[i].guid = guid;
				events[i].eventId = guid;
			}
		}
		results = NULL;
		if (userResults != NULL) {
			results = ksceKernelAllocHeapMemory2(pid, count * sizeof(*results), NULL);
			if (results == NULL) {
				ret = SCE_KERNEL_ERROR_NO_MEMORY;
				done = 0;
				goto freeIds;
			}
		}
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = 4;
		opt.mmapBase = 0;
		opt.mmapSize = 0;
		wme = ksceKernelAllocHeapMemory2(pid, WME_SIZE(count), &opt);
		if (wme == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			done = 0;
			goto freeResults;
		}
		memset(wme, 0, offsetof(ThreadmgrWaitMultipleEvents, nodes));
		wme->next = (struct ThreadmgrWaitNode *)wme;
		wme->prev = (struct ThreadmgrWaitNode *)wme;
		for (i = 0; i < count; i++) {
			wme->nodes[i].next = &wme->nodes[i];
			wme->nodes[i].prev = &wme->nodes[i];
		}
		wme->mode = (mode & 1) ? WME_MODE_ANY : WME_MODE_ALL;
		wme->count = count;
		wme->results = results;
		wme->userEventIds = ids;
		wme->resultCount = 0;
		wme->events = events;
		wme->ready[0] = 0;
		wme->ready[1] = 0;
		SCE_DBG_ASSERT_CHECK(thread->restartWaitMultipleEvents == NULL, 940);
		thread->restartWaitMultipleEvents = wme;
	} else {
		/* restarted: the WME the broken wait kept */
		SCE_DBG_ASSERT_CHECK(thread->restartWaitMultipleEvents != NULL, 854);
		wme = thread->restartWaitMultipleEvents;
		events = (ThreadmgrWaitEvent *)wme->events;
		ids = wme->userEventIds;
		results = wme->results;
	}
	if (userTimeout != NULL) {
		ret = ksceKernelCopyFromUser(&timeout, userTimeout, sizeof(timeout));
		if (ret < 0) {
			ret |= SYSCALL_ERROR_BIT;
			done = 0;
			goto freeWme;
		}
		timeoutPtr = &timeout;
	} else {
		timeoutPtr = NULL;
	}
	ret = waitMultipleEvents(wme, timeoutPtr, 0, (uint32_t *)userEvents);
	if (thread->flags & THREAD_FLAG_SIGNAL_DEFERRED) {
		done = 0;
		goto out;
	}
	thread->restartWaitMultipleEvents = NULL;
	done = wme->resultCount;
	if (userResults != NULL) {
		/* the results name the events by the user's PUIDs */
		for (i = 0; (int)i < done; i++) {
			for (j = 0; j < count; j++) {
				if (ids[j].guid == results[i].eventId) {
					results[i].eventId = ids[j].puid;
					break;
				}
			}
		}
		err = ksceKernelCopyToUser(userResults, results, count * sizeof(*results));
		if (err < 0)
			ret = err | SYSCALL_ERROR_BIT;
	}
	if (userTimeout != NULL) {
		err = ksceKernelCopyToUser(userTimeout, timeoutPtr, sizeof(timeout));
		if (err < 0)
			ret = err | SYSCALL_ERROR_BIT;
	}
freeWme:
	SceSysrootForKernel_7968F091(pid, wme);
freeResults:
	SceSysrootForKernel_7968F091(pid, results);
freeIds:
	SceSysrootForKernel_7968F091(pid, ids);
freeEvents:
	SceSysrootForKernel_7968F091(pid, events);
out:
	return ret < 0 ? ret : done;
}

/*
 * sceKernelWaitMultipleEventsCB (user): the WME and copies of the
 * user's arrays live in the process's heap; a wait a request broke keeps
 * them on the thread (restartWaitMultipleEvents) for the restarted syscall.
 */
int sceKernelWaitMultipleEventsCBForUser(const ThreadmgrWaitEvent *userEvents, SceUInt32 count,
				       SceUInt32 mode, ThreadmgrWaitEventResult *userResults,
				       SceUInt32 *userTimeout)
{
	struct ThreadCB *thread;
	ThreadmgrWaitMultipleEvents *wme;
	ThreadmgrWaitEventResult *results;
	struct ThreadmgrUserEventId *ids;
	ThreadmgrWaitEvent *events;
	SceUInt32 timeout, *timeoutPtr;
	SceAllocOpt opt;
	SceUID pid, guid;
	int ret, err, done;
	SceUInt32 i, j;

	pid = currentThreadContext()->processId;
	thread = currentThreadContext();
	if (userEvents == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (count > WME_MAX_USER_EVENTS)
		return SCE_KERNEL_ERROR_ILLEGAL_SIZE;
	if (mode & ~WME_MODE_MASK)
		return SCE_KERNEL_ERROR_ILLEGAL_MODE;
	if (SceThreadmgrForDriver_20C228E4() == 0) {
		events = ksceKernelAllocHeapMemory2(pid, count * sizeof(*events), NULL);
		if (events == NULL)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		ret = ksceKernelCopyFromUser(events, userEvents, count * sizeof(*events));
		if (ret < 0) {
			done = 0;
			goto freeEvents;
		}
		ids = ksceKernelAllocHeapMemory2(pid, count * sizeof(*ids), NULL);
		if (ids == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			done = 0;
			goto freeEvents;
		}
		for (i = 0; i < count; i++) {
			ids[i].puid = events[i].eventId;
			guid = kscePUIDtoGUID(pid, events[i].eventId);
			if (guid < 0) {
				events[i].eventId = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
				ids[i].guid = SCE_KERNEL_ERROR_UNKNOWN_EVENT_ID;
			} else {
				ids[i].guid = guid;
				events[i].eventId = guid;
			}
		}
		results = NULL;
		if (userResults != NULL) {
			results = ksceKernelAllocHeapMemory2(pid, count * sizeof(*results), NULL);
			if (results == NULL) {
				ret = SCE_KERNEL_ERROR_NO_MEMORY;
				done = 0;
				goto freeIds;
			}
		}
		opt.size = sizeof(opt);
		opt.mmapKind = 0;
		opt.align = 4;
		opt.mmapBase = 0;
		opt.mmapSize = 0;
		wme = ksceKernelAllocHeapMemory2(pid, WME_SIZE(count), &opt);
		if (wme == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			done = 0;
			goto freeResults;
		}
		memset(wme, 0, offsetof(ThreadmgrWaitMultipleEvents, nodes));
		wme->next = (struct ThreadmgrWaitNode *)wme;
		wme->prev = (struct ThreadmgrWaitNode *)wme;
		for (i = 0; i < count; i++) {
			wme->nodes[i].next = &wme->nodes[i];
			wme->nodes[i].prev = &wme->nodes[i];
		}
		wme->mode = (mode & 1) ? WME_MODE_ANY : WME_MODE_ALL;
		wme->count = count;
		wme->results = results;
		wme->userEventIds = ids;
		wme->resultCount = 0;
		wme->events = events;
		wme->ready[0] = 0;
		wme->ready[1] = 0;
		SCE_DBG_ASSERT_CHECK(thread->restartWaitMultipleEvents == NULL, 1144);
		thread->restartWaitMultipleEvents = wme;
	} else {
		/* restarted: the WME the broken wait kept */
		SCE_DBG_ASSERT_CHECK(thread->restartWaitMultipleEvents != NULL, 1058);
		wme = thread->restartWaitMultipleEvents;
		events = (ThreadmgrWaitEvent *)wme->events;
		ids = wme->userEventIds;
		results = wme->results;
	}
	if (userTimeout != NULL) {
		ret = ksceKernelCopyFromUser(&timeout, userTimeout, sizeof(timeout));
		if (ret < 0) {
			ret |= SYSCALL_ERROR_BIT;
			done = 0;
			goto freeWme;
		}
		timeoutPtr = &timeout;
	} else {
		timeoutPtr = NULL;
	}
	ret = waitMultipleEvents(wme, timeoutPtr, 1, (uint32_t *)userEvents);
	if (thread->flags & THREAD_FLAG_SIGNAL_DEFERRED) {
		done = 0;
		goto out;
	}
	thread->restartWaitMultipleEvents = NULL;
	done = wme->resultCount;
	if (userResults != NULL) {
		/* the results name the events by the user's PUIDs */
		for (i = 0; (int)i < done; i++) {
			for (j = 0; j < count; j++) {
				if (ids[j].guid == results[i].eventId) {
					results[i].eventId = ids[j].puid;
					break;
				}
			}
		}
		err = ksceKernelCopyToUser(userResults, results, count * sizeof(*results));
		if (err < 0)
			ret = err | SYSCALL_ERROR_BIT;
	}
	if (userTimeout != NULL) {
		err = ksceKernelCopyToUser(userTimeout, timeoutPtr, sizeof(timeout));
		if (err < 0)
			ret = err | SYSCALL_ERROR_BIT;
	}
freeWme:
	SceSysrootForKernel_7968F091(pid, wme);
freeResults:
	SceSysrootForKernel_7968F091(pid, results);
freeIds:
	SceSysrootForKernel_7968F091(pid, ids);
freeEvents:
	SceSysrootForKernel_7968F091(pid, events);
out:
	return ret < 0 ? ret : done;
}

/* sceKernelPulseEvent (user) */
int sceKernelPulseEvent(SceUID eventId, uint32_t pattern, uint64_t userData)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL_MASK)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelPulseEvent(guid, pattern, userData);
}

/* the core of _sceKernelPulseEventWithNotifyCallback */
int sceKernelPulseEventWithNotifyCallbackForUser(SceUID eventId, uint32_t pattern,
						 uint64_t userData, uint32_t arg)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL_MASK)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelPulseEventWithNotifyCallback(guid, pattern, userData, arg);
}

/* the core of _sceKernelCancelEventWithSetPattern */
int sceKernelCancelEventWithSetPatternForUser(SceUID eventId, uint32_t setPattern,
					      uint64_t userData, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (setPattern & EVENT_PATTERN_KERNEL_MASK)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return sceKernelCancelEventWithSetPattern(guid, setPattern, userData, numWaitThreads);
}

/* the core of _sceKernelGetEventPattern */
int sceKernelGetEventPatternForUser(SceUID eventId, uint32_t *pattern)
{
	SceUID guid = kscePUIDtoGUID(0, eventId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	return sceKernelGetEventPattern(guid, pattern);
}

/* the core of _sceKernelGetEventInfo: the user's PUID, and whether it created the event */
int sceKernelGetEventInfoForUser(SceUID eventId, ThreadmgrEventInfo *info)
{
	uint32_t attributes;
	SceUID guid;
	int ret;

	guid = kscePUIDtoGUID(0, eventId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	ret = SceThreadmgrForDriver_CBA7FAAA(guid, info);
	if (ret >= 0) {
		info->eventId = eventId;
		ret = scePUIDGetAttr(0, eventId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

/* the cores of _sceKernelWaitEventFlag and _sceKernelWaitEventFlagCB */
int sceKernelWaitEventFlagForUser(SceUID evfId, uint32_t pattern, uint32_t mode,
				  uint32_t *outPattern, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	return ksceKernelWaitEventFlag(guid, pattern, mode, outPattern, timeout);
}

int sceKernelWaitEventFlagCBForUser(SceUID evfId, uint32_t pattern, uint32_t mode,
				    uint32_t *outPattern, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	return ksceKernelWaitEventFlagCB(guid, pattern, mode, outPattern, timeout);
}

/* sceKernelOpenSema (user) */
SceUID sceKernelOpenSema(const char *userName)
{
	return openUIDByName(userName, USER_OBJECT_SEMAPHORE);
}

/* sceKernelDeleteSema (user) */
int sceKernelDeleteSema(SceUID semaId)
{
	return closeSemaForUser(semaId, 1);
}

/* sceKernelCloseSema (user) */
int sceKernelCloseSema(SceUID semaId)
{
	return closeSemaForUser(semaId, 0);
}

/* the cores of _sceKernelWaitSema and _sceKernelWaitSemaCB */
int sceKernelWaitSemaForUser(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, semaId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	return ksceKernelWaitSema(guid, needCount, timeout);
}

int sceKernelWaitSemaCBForUser(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, semaId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	return SceThreadmgrForDriver_F55E4D86(guid, needCount, timeout);
}

/* the core of _sceKernelCancelSema */
int sceKernelCancelSemaForUser(SceUID semaId, int setCount, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, semaId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	return sceKernelCancelSema(guid, setCount, numWaitThreads);
}

/* the core of _sceKernelGetSemaInfo */
int sceKernelGetSemaInfoForUser(SceUID semaId, struct ThreadmgrUserInfoHeader *info)
{
	uint32_t attributes;
	SceUID guid;
	int ret;

	guid = kscePUIDtoGUID(0, semaId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	ret = ksceKernelGetSemaInfo(guid, info);
	if (ret >= 0) {
		info->uid = semaId;
		ret = scePUIDGetAttr(0, semaId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

/* sceKernelPollSema (user) */
int sceKernelPollSema(SceUID semaId, int needCount)
{
	SceUID guid = kscePUIDtoGUID(0, semaId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	return ksceKernelPollSema(guid, needCount);
}

/* sceKernelSignalSema (user) */
int sceKernelSignalSema(SceUID semaId, int signalCount)
{
	SceUID guid = kscePUIDtoGUID(0, semaId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_SEMA_ID;
	return ksceKernelSignalSema(guid, signalCount);
}

/* the core of _sceKernelCreateMutex */
SceUID sceKernelCreateMutexForUser(const char *userName, SceUInt32 attr, int initCount,
				   const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	return ksceKernelCreateMutex(name, attr, initCount, option);
}

/* sceKernelOpenMutex (user) */
SceUID sceKernelOpenMutex(const char *userName)
{
	return openUIDByName(userName, USER_OBJECT_MUTEX);
}

/* sceKernelDeleteMutex (user) */
int sceKernelDeleteMutex(SceUID mutexId)
{
	return closeMutexForUser(mutexId, 1);
}

/* sceKernelCloseMutex (user) */
int sceKernelCloseMutex(SceUID mutexId)
{
	return closeMutexForUser(mutexId, 0);
}

/* the core of _sceKernelCreateCond: its mutex is a PUID too */
SceUID sceKernelCreateCondForUser(const char *userName, SceUInt32 attr, SceUID mutexId,
				  const void *option)
{
	char name[32];
	SceUID mutex;
	int ret;

	if (userName == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = ksceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2298,
			" k: error: sceKernelCreateCondForUser() -> sceKernelStrncpyFromUser()=0x%08X\n",
			ret);
		return ret | SYSCALL_ERROR_BIT;
	}
	mutex = kscePUIDtoGUID(0, mutexId);
	if (mutex < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelCreateCond(name, attr, mutex, option);
}

/* sceKernelDeleteCond (user): a cond has nothing to tell its openers */
int sceKernelDeleteCond(SceUID condId)
{
	SceClass *cls;

	if (kscePUIDGetClass(0, condId, &cls) >= 0 &&
	    cls == s_threadmgrRuntime.manager->condClass)
		return kscePUIDClose(0, condId);
	return USER_ERROR_UNKNOWN_COND_ID;
}

/* sceKernelOpenCond (user) */
SceUID sceKernelOpenCond(const char *userName)
{
	return openUIDByName(userName, USER_OBJECT_COND);
}

/* sceKernelCloseCond (user) */
int sceKernelCloseCond(SceUID condId)
{
	return sceKernelDeleteCond(condId);
}

/* the cores of _sceKernelWaitCond and _sceKernelWaitCondCB */
int sceKernelWaitCondForUser(SceUID condId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, condId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	return ksceKernelWaitCond(guid, timeout);
}

int sceKernelWaitCondCBForUser(SceUID condId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, condId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	return waitCondCB(guid, timeout);
}

/* the core of _sceKernelGetCondInfo */
int sceKernelGetCondInfoForUser(SceUID condId, struct ThreadmgrUserInfoHeader *info)
{
	SceUID guid = kscePUIDtoGUID(0, condId);
	int ret;

	if (guid < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	ret = SceThreadmgrForDriver_D1FBDFA6(guid, info);
	if (ret >= 0)
		info->uid = condId;
	return ret;
}

/* sceKernelSignalCond (user) */
int sceKernelSignalCond(SceUID condId)
{
	SceUID guid = kscePUIDtoGUID(0, condId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	return ksceKernelSignalCond(guid);
}

/* sceKernelSignalCondAll (user) */
int sceKernelSignalCondAll(SceUID condId)
{
	SceUID guid = kscePUIDtoGUID(0, condId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	return ksceKernelSignalCondAll(guid);
}

/* sceKernelSignalCondTo (user) */
int sceKernelSignalCondTo(SceUID condId, SceUID threadId)
{
	SceUID cond, thread;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	cond = kscePUIDtoGUID(0, condId);
	if (cond < 0)
		return USER_ERROR_UNKNOWN_COND_ID;
	thread = kscePUIDtoGUID(0, threadId);
	if (thread < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	return ksceKernelSignalCondTo(cond, thread);
}

/* sceKernelDeleteMsgPipe (user) */
int sceKernelDeleteMsgPipe(SceUID msgPipeId)
{
	return closeMsgPipeForUser(msgPipeId, MSG_PIPE_CLOSE_DELETE);
}

/* sceKernelCloseMsgPipe (user) */
int sceKernelCloseMsgPipe(SceUID msgPipeId)
{
	return closeMsgPipeForUser(msgPipeId, MSG_PIPE_CLOSE_CLOSE);
}

/* sceKernelGetMsgPipeCreatorId (user): by the pipe's name */
SceUID sceKernelGetMsgPipeCreatorId(const char *userName)
{
	char name[32];
	SceUID ret;

	ret = copyinUIDNameWithAttr(name, userName, 0);
	if (ret < 0)
		return ret;
	return SceThreadmgrForDriver_88206D29(name);
}

/* the cores of the _sceKernel*MsgPipeVector* syscalls */
int sceKernelSendMsgPipeVectorForUser(SceUID msgPipeId, const void *vector, int count,
				      uint32_t mode, SceSize *result, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelSendMsgPipeVector(guid, vector, count, mode, result, timeout);
}

int sceKernelSendMsgPipeVectorCBForUser(SceUID msgPipeId, const void *vector, int count,
					uint32_t mode, SceSize *result, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return SceThreadmgrForDriver_2FC0FFF6(guid, vector, count, mode, result, timeout);
}

int sceKernelTrySendMsgPipeVectorForUser(SceUID msgPipeId, const void *vector, int count,
					 uint32_t mode, SceSize *result)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelTrySendMsgPipeVector(guid, vector, count, mode, result);
}

int sceKernelReceiveMsgPipeVectorForUser(SceUID msgPipeId, const void *vector, int count,
					 uint32_t mode, SceSize *result, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelReceiveMsgPipeVector(guid, vector, count, mode, result, timeout);
}

int sceKernelReceiveMsgPipeVectorCBForUser(SceUID msgPipeId, const void *vector, int count,
					   uint32_t mode, SceSize *result, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelReceiveMsgPipeVectorCB(guid, vector, count, mode, result, timeout);
}

int sceKernelTryReceiveMsgPipeVectorForUser(SceUID msgPipeId, const void *vector, int count,
					    uint32_t mode, SceSize *result)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelTryReceiveMsgPipeVector(guid, vector, count, mode, result);
}

/* the core of _sceKernelCancelMsgPipe */
int sceKernelCancelMsgPipeForUser(SceUID msgPipeId, int *numSendWaitThreads,
				  int *numReceiveWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, msgPipeId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	return ksceKernelCancelMsgPipe(guid, numSendWaitThreads, numReceiveWaitThreads);
}

/* the core of _sceKernelGetMsgPipeInfo */
int sceKernelGetMsgPipeInfoForUser(SceUID msgPipeId, struct ThreadmgrUserInfoHeader *info)
{
	uint32_t attributes;
	SceUID guid;
	int ret;

	guid = kscePUIDtoGUID(0, msgPipeId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_MSG_PIPE_ID;
	ret = ksceKernelGetMsgPipeInfo(guid, info);
	if (ret >= 0) {
		info->uid = msgPipeId;
		ret = scePUIDGetAttr(0, msgPipeId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

/* the core of _sceKernelGetLwMutexInfoById */
int sceKernelGetLwMutexInfoByIdForUser(SceUID lwMutexId, ThreadmgrLwMutexInfo *info)
{
	SceUID guid = kscePUIDtoGUID(0, lwMutexId);
	int ret;

	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_MUTEX_ID;
	ret = ksceKernelGetLwMutexInfo(guid, info);
	if (ret >= 0)
		info->uid = lwMutexId;
	return ret;
}

/* the core of _sceKernelCreateLwCond: its lw mutex is named by its work area */
int sceKernelCreateLwCondForUser(ThreadmgrLwCondWork *work, const char *userName,
				 SceUInt32 attr, const ThreadmgrLwMutexWork *lwMutex,
				 const void *option)
{
	char name[32];
	int ret;

	if (work == NULL)
		return USER_ERROR_ILLEGAL_ADDR;
	if (userName == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	ret = ksceKernelStrncpyFromUser(name, userName, sizeof(name));
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2995,
			" k: error: sceKernelCreateLwCondForUser() -> sceKernelStrncpyFromUser()=0x%08X\n",
			ret);
		return ret | SYSCALL_ERROR_BIT;
	}
	if (lwMutex == NULL)
		return USER_ERROR_ILLEGAL_ADDR;
	ret = __sceKernelCreateLwCond(work, name, attr, lwMutex->uid, option);
	return ret < 0 ? ret : 0;
}

/* the core of _sceKernelDeleteLwCond: the work area forgets the PUIDs */
int sceKernelDeleteLwCondForUser(ThreadmgrLwCondWork *work)
{
	SceClass *cls;
	uint32_t saved;
	SceUID uid;
	int intr, ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	if (work == NULL) {
		ret = SCE_KERNEL_ERROR_ILLEGAL_ADDR;
		goto restore;
	}
	uid = work->uid;
	intr = ksceKernelCpuSuspendIntr();
	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->lwCondClass) {
		ret = USER_ERROR_UNKNOWN_LW_COND_ID;
	} else {
		ret = kscePUIDClose(0, uid);
		if (ret >= 0) {
			work->uid = -1;
			work->lwMutexId = -1;
		}
	}
	ksceKernelCpuResumeIntr(intr);
restore:
	ThreadmgrSetUserRoWord(saved);
	return ret;
}

/* the cores of _sceKernelWaitLwCond and _sceKernelWaitLwCondCB */
int sceKernelWaitLwCondForUser(const ThreadmgrLwCondWork *work, SceUInt32 *timeout)
{
	SceUID guid;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	return __sceKernelWaitLwCond(guid, timeout, 0);
}

int sceKernelWaitLwCondCBForUser(const ThreadmgrLwCondWork *work, SceUInt32 *timeout)
{
	SceUID guid;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	return __sceKernelWaitLwCond(guid, timeout, 1);
}

/* the cores of _sceKernelSignalLwCond, All and To */
int sceKernelSignalLwCondForUser(const ThreadmgrLwCondWork *work)
{
	SceUID guid;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	return __sceKernelSignalLwCond(guid);
}

int sceKernelSignalLwCondAllForUser(const ThreadmgrLwCondWork *work)
{
	SceUID guid;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	return __sceKernelSignalLwCondAll(guid);
}

int sceKernelSignalLwCondToForUser(const ThreadmgrLwCondWork *work, SceUID threadId)
{
	SceUID guid, thread;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	thread = kscePUIDtoGUID(0, threadId);
	if (thread < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	return __sceKernelSignalLwCondTo(guid, thread);
}

/* the cores of _sceKernelGetLwCondInfo and _sceKernelGetLwCondInfoById */
int sceKernelGetLwCondInfoForUser(const ThreadmgrLwCondWork *work,
				  ThreadmgrLwCondInfo *info)
{
	SceUID guid;
	int ret;

	if (work == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	guid = kscePUIDtoGUID(0, work->uid);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	ret = sceKernelGetLwCondInfo(guid, info);
	if (ret >= 0)
		info->uid = work->uid;
	return ret;
}

int sceKernelGetLwCondInfoByIdForUser(SceUID condId, void *info)
{
	SceUID guid = kscePUIDtoGUID(0, condId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_LW_COND_ID;
	return sceKernelGetLwCondInfo(guid, info);
}

/* the core of _sceKernelCreateTimer: the user gets a PUID */
SceUID sceKernelCreateTimerForUser(const char *userName, SceUInt32 attr, const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = sceKernelCreateTimer(name, attr, option);
	if (uid < 0)
		return uid;
	return kscePUIDOpenByGUID(currentThreadContext()->processId, uid);
}

/* sceKernelGetSystemTimeLow (user) */
SceUInt32 sceKernelGetSystemTimeLow(void)
{
	sceKernelApiDeclareTypeUInt32();
	return SceSysrootForKernel_41636522();
}

/* sceKernelGetSystemTimeWide (user) */
SceUInt64 sceKernelGetSystemTimeWide(void)
{
	sceKernelApiDeclareTypeUInt64();
	return SceSysrootForKernel_E20F6FC8();
}

/* the core of _sceKernelGetSystemTime */
int sceKernelGetSystemTimeForUser(ThreadmgrSysClock *clock)
{
	return SceSysrootForKernel_1D8DB3A5(clock);
}

/* _sceKernelGetSystemTime: the syscall copies the clock out */
int _sceKernelGetSystemTime(ThreadmgrSysClock *userClock)
{
	ThreadmgrSysClock *clock;
	int ret, err;

	if (userClock == NULL)
		return sceKernelGetSystemTimeForUser(NULL);
	clock = __builtin_alloca(sizeof(*clock));
	*clock = (ThreadmgrSysClock){ 0, 0 };
	ret = sceKernelGetSystemTimeForUser(clock);
	err = ksceKernelCopyToUser(userClock, clock, sizeof(*clock));
	if (err < 0)
		ret = err;
	return ret;
}

/* sceKernelOpenRWLock (user) */
SceUID sceKernelOpenRWLock(const char *userName)
{
	return openUIDByName(userName, USER_OBJECT_RW_LOCK);
}

/* sceKernelDeleteRWLock (user) */
int sceKernelDeleteRWLockForUser(SceUID rwLockId)
{
	return closeRWLockForUser(rwLockId, 1);
}

/* sceKernelCloseRWLock (user) */
int sceKernelCloseRWLock(SceUID rwLockId)
{
	return closeRWLockForUser(rwLockId, 0);
}

/* the cores of _sceKernelLockReadRWLock and _sceKernelLockReadRWLockCB */
int sceKernelLockReadRWLockForUser(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return SceThreadmgrForDriver_3F5E1D56(guid, timeout);
}

int sceKernelLockReadRWLockCBForUser(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return SceThreadmgrForDriver_CFA2FFAF(guid, timeout);
}

/* sceKernelTryLockReadRWLock (user) */
int sceKernelTryLockReadRWLock(SceUID rwLockId)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return ksceKernelTryLockReadRWLock(guid);
}

/* sceKernelUnlockReadRWLock (user) */
int sceKernelUnlockReadRWLock(SceUID rwLockId)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return ksceKernelUnlockReadRWLock(guid);
}

/* the cores of _sceKernelLockWriteRWLock and _sceKernelLockWriteRWLockCB */
int sceKernelLockWriteRWLockForUser(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return SceThreadmgrForDriver_A64B00F6(guid, timeout);
}

int sceKernelLockWriteRWLockCBForUser(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return SceThreadmgrForDriver_54566860(guid, timeout);
}

/* sceKernelTryLockWriteRWLock (user) */
int sceKernelTryLockWriteRWLock(SceUID rwLockId)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return ksceKernelTryLockWriteRWLock(guid);
}

/* sceKernelUnlockWriteRWLock (user) */
int sceKernelUnlockWriteRWLock(SceUID rwLockId)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return ksceKernelUnlockWriteRWLock(guid);
}

/* the core of _sceKernelCancelRWLock */
int sceKernelCancelRWLockForUser(SceUID rwLockId, int *numReadWaitThreads,
				 int *numWriteWaitThreads, SceUInt32 type)
{
	SceUID guid = kscePUIDtoGUID(0, rwLockId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	return sceKernelCancelRWLock(guid, numReadWaitThreads, numWriteWaitThreads, type);
}

/* the core of _sceKernelGetRWLockInfo */
int sceKernelGetRWLockInfoForUser(SceUID rwLockId, struct ThreadmgrUserInfoHeader *info)
{
	uint32_t attributes;
	SceUID guid;
	int ret;

	guid = kscePUIDtoGUID(0, rwLockId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_RW_LOCK_ID;
	ret = sceKernelGetRWLockInfo(guid, info);
	if (ret >= 0) {
		info->uid = rwLockId;
		ret = scePUIDGetAttr(0, rwLockId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

/* sceKernelDeleteSimpleEvent (user) */
int sceKernelDeleteSimpleEventForUser(SceUID simpleEventId)
{
	return closeSimpleEventForUser(simpleEventId, EVENT_CLOSE_DELETE);
}

/* sceKernelCloseSimpleEvent (user) */
int sceKernelCloseSimpleEvent(SceUID simpleEventId)
{
	return closeSimpleEventForUser(simpleEventId, MSG_PIPE_CLOSE_CLOSE);
}

/* the core of _sceKernelPMonThreadGetCounter: 0 is the calling thread */
int sceKernelPMonThreadGetCounterForUser(SceUID threadId, uint32_t counter, uint32_t *value)
{
	SceUID guid = 0;

	if (threadId != 0) {
		guid = kscePUIDtoGUID(0, threadId);
		if (guid < 0)
			return USER_ERROR_UNKNOWN_THREAD_ID;
	}
	return SceThreadmgrForKernel_170F69D6(guid, counter, value);
}

/* sceKernelGetThreadTLSAddr (user): NULL for a thread it does not know */
void *sceKernelGetThreadTLSAddr(SceUID threadId, unsigned int index)
{
	SceUID guid;

	SCE_DBG_ASSERT_CHECK_FUNC(threadId != 0, 4246, "sceKernelGetThreadTLSAddrForUser");
	sceKernelApiDeclareTypeUInt32();
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return NULL;
	return ksceKernelGetThreadTLSAddr(guid, index);
}

/* _sceKernelLockLwMutex: the timeout comes in and goes back out */
int _sceKernelLockLwMutexSyscall(void *work, int count, SceUInt32 *userTimeout, int callbacks)
{
	SceUInt32 timeout;
	int ret, err;

	if (userTimeout == NULL)
		return _sceKernelLockLwMutex(work, count, NULL, callbacks);
	ret = ksceKernelCopyFromUser(&timeout, userTimeout, sizeof(timeout));
	if (ret < 0)
		return ret;
	ret = _sceKernelLockLwMutex(work, count, &timeout, callbacks);
	err = ksceKernelCopyToUser(userTimeout, &timeout, sizeof(timeout));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelUnlockLwMutex */
int _sceKernelUnlockLwMutexSyscall(void *work, int count)
{
	return _sceKernelUnlockLwMutex(work, count);
}

/* _sceKernelWaitThreadEnd: the status and the timeout go back out */
int _sceKernelWaitThreadEnd(SceUID threadId, int *userStatus, SceUInt32 *userTimeout)
{
	SceUInt32 timeout, *timeoutPtr;
	int status, *statusPtr;
	SceUID guid;
	int ret, err;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if (userTimeout == NULL) {
		err = 0;
		timeoutPtr = NULL;
	} else {
		err = ksceKernelCopyFromUser(&timeout, userTimeout, sizeof(timeout));
		if (err < 0)
			return err;
		timeoutPtr = &timeout;
	}
	statusPtr = NULL;
	if (userStatus != NULL)
		statusPtr = &status;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	ret = ksceKernelWaitThreadEnd(guid, statusPtr, timeoutPtr);
	if (ret >= 0 && userStatus != NULL)
		err = ksceKernelCopyToUser(userStatus, statusPtr, sizeof(status));
	if (userTimeout != NULL)
		err = ksceKernelCopyToUser(userTimeout, timeoutPtr, sizeof(timeout));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitThreadEndCB */
int _sceKernelWaitThreadEndCB(SceUID threadId, int *userStatus, SceUInt32 *userTimeout)
{
	SceUInt32 timeout, *timeoutPtr;
	int status, *statusPtr;
	SceUID guid;
	int ret, err;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	if (userTimeout == NULL) {
		err = 0;
		timeoutPtr = NULL;
	} else {
		err = ksceKernelCopyFromUser(&timeout, userTimeout, sizeof(timeout));
		if (err < 0)
			return err;
		timeoutPtr = &timeout;
	}
	statusPtr = NULL;
	if (userStatus != NULL)
		statusPtr = &status;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	ret = ksceKernelWaitThreadEndCB(guid, statusPtr, timeoutPtr);
	if (ret >= 0 && userStatus != NULL)
		err = ksceKernelCopyToUser(userStatus, statusPtr, sizeof(status));
	if (userTimeout != NULL)
		err = ksceKernelCopyToUser(userTimeout, timeoutPtr, sizeof(timeout));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetThreadExitStatus: 0 is the calling thread */
int _sceKernelGetThreadExitStatus(SceUID threadId, int *userStatus)
{
	int status, *statusPtr;
	SceUID guid;
	int ret, err;

	statusPtr = NULL;
	if (userStatus != NULL)
		statusPtr = &status;
	guid = 0;
	if (threadId != 0) {
		guid = kscePUIDtoGUID(0, threadId);
		if (guid < 0)
			return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID;
	}
	ret = sceKernelGetThreadExitStatus(guid, statusPtr);
	if (ret >= 0 && userStatus != NULL) {
		err = ksceKernelCopyToUser(userStatus, statusPtr, sizeof(status));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* the cores of the user's create calls: the kernel object, opened as a PUID */
SceUID sceKernelCreateEventFlagForUser(const char *userName, SceUInt32 attr, uint32_t initPattern,
				       const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = ksceKernelCreateEventFlag(name, attr, initPattern, option);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

SceUID sceKernelCreateSemaForUser(const char *userName, SceUInt32 attr, int initCount,
				  int maxCount, const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = ksceKernelCreateSema(name, attr, initCount, maxCount, option);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

SceUID sceKernelCreateSema_16XXForUser(const char *userName, SceUInt32 attr, int initCount,
				       int maxCount, const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = sceKernelCreateSema_16XX(name, attr, initCount, maxCount, option);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

SceUID sceKernelCreateMsgPipeWithLRForUser(const char *userName, int memType, SceUInt32 attr,
					   SceSize bufferSize, const void *option, const void *lr)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = sceKernelCreateMsgPipeWithLR(name, memType, attr, bufferSize, option, lr);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

SceUID sceKernelCreateRWLockForUser(const char *userName, SceUInt32 attr, const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = sceKernelCreateRWLock(name, attr, option);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

SceUID sceKernelCreateSimpleEventForUser(const char *userName, SceUInt32 attr,
					 uint32_t initPattern, const void *option)
{
	char name[32];
	SceUID uid;

	uid = copyinUIDNameWithAttr(name, userName, attr);
	if (uid < 0)
		return uid;
	uid = ksceKernelCreateSimpleEvent(name, attr, initPattern, option);
	if (uid < 0)
		return uid;
	return OpenByGUID(currentThreadContext()->processId, uid);
}

/* sceKernelLockMutex (user) */
int sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, mutexId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelLockMutex(guid, count, timeout);
}

/* sceKernelLockMutexCB (user) */
int sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt32 *timeout)
{
	SceUID guid = kscePUIDtoGUID(0, mutexId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelLockMutexCB_089(guid, count, timeout);
}

/* sceKernelCancelMutex (user) */
int sceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, mutexId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelCancelMutex(guid, newCount, numWaitThreads);
}

/* sceKernelGetMutexInfo (user) */
int sceKernelGetMutexInfo(SceUID mutexId, ThreadmgrMutexInfo *info)
{
	uint32_t attributes;
	SceUID guid;
	int ret;

	guid = kscePUIDtoGUID(0, mutexId);
	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	ret = ksceKernelGetMutexInfo(guid, info);
	if (ret >= 0) {
		info->mutexId = mutexId;
		ret = scePUIDGetAttr(0, mutexId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

/* sceKernelTryLockMutex (user) */
int sceKernelTryLockMutex(SceUID mutexId, int count)
{
	SceUID guid = kscePUIDtoGUID(0, mutexId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelTryLockMutex(guid, count);
}

/* sceKernelUnlockMutex (user) */
int sceKernelUnlockMutex(SceUID mutexId, int count)
{
	SceUID guid = kscePUIDtoGUID(0, mutexId);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_MUTEX_ID;
	return ksceKernelUnlockMutex(guid, count);
}

/*
 * The syscall stubs: user pointers are replaced by kernel copies on the
 * stack (alloca) around the call.
 */
/* _sceKernelGetThreadInfo */
int _sceKernelGetThreadInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetThreadInfoForUser(uid, userInfo);
	if (args.size > 0x80)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x80);
	memset(info, 0, 0x80);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetThreadInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetThreadRunStatus */
int _sceKernelGetThreadRunStatus(void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetThreadRunStatusForUser(userInfo);
	if (args.size > 0x34)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x34);
	memset(info, 0, 0x34);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetThreadRunStatusForUser(info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetSystemInfo */
int _sceKernelGetSystemInfo(void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetSystemInfoForUser(userInfo);
	if (args.size > 0x48)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x48);
	memset(info, 0, 0x48);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetSystemInfoForUser(info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetCallbackInfo */
int _sceKernelGetCallbackInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetCallbackInfoForUser(uid, userInfo);
	if (args.size > 0x44)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x44);
	memset(info, 0, 0x44);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetCallbackInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCancelEvent */
int _sceKernelCancelEvent(SceUID eventId, int *numWaitThreads)
{
	int *buf;
	int ret, err;

	if (numWaitThreads == NULL)
		return sceKernelCancelEventForUser(eventId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelCancelEventForUser(eventId, buf);
	err = ksceKernelCopyToUser(numWaitThreads, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetEventPattern */
int _sceKernelGetEventPattern(SceUID eventId, uint32_t *pattern)
{
	uint32_t *buf;
	int ret, err;

	if (pattern == NULL)
		return sceKernelGetEventPatternForUser(eventId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelGetEventPatternForUser(eventId, buf);
	err = ksceKernelCopyToUser(pattern, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetEventInfo */
int _sceKernelGetEventInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetEventInfoForUser(uid, userInfo);
	if (args.size > 0x40)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x40);
	memset(info, 0, 0x40);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetEventInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitSema */
int _sceKernelWaitSema(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSemaForUser(semaId, needCount, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSemaForUser(semaId, needCount, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitSemaCB */
int _sceKernelWaitSemaCB(SceUID semaId, int needCount, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitSemaCBForUser(semaId, needCount, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitSemaCBForUser(semaId, needCount, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCancelSema */
int _sceKernelCancelSema(SceUID semaId, int setCount, int *numWaitThreads)
{
	int *buf;
	int ret, err;

	if (numWaitThreads == NULL)
		return sceKernelCancelSemaForUser(semaId, setCount, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelCancelSemaForUser(semaId, setCount, buf);
	err = ksceKernelCopyToUser(numWaitThreads, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetSemaInfo */
int _sceKernelGetSemaInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetSemaInfoForUser(uid, userInfo);
	if (args.size > 0x3c)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x3c);
	memset(info, 0, 0x3c);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetSemaInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockMutex */
int _sceKernelLockMutex(SceUID mutexId, int count, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockMutex(mutexId, count, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockMutex(mutexId, count, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockMutexCB */
int _sceKernelLockMutexCB(SceUID mutexId, int count, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockMutexCB(mutexId, count, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockMutexCB(mutexId, count, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCancelMutex */
int _sceKernelCancelMutex(SceUID mutexId, int newCount, int *numWaitThreads)
{
	int *buf;
	int ret, err;

	if (numWaitThreads == NULL)
		return sceKernelCancelMutex(mutexId, newCount, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelCancelMutex(mutexId, newCount, buf);
	err = ksceKernelCopyToUser(numWaitThreads, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetMutexInfo */
int _sceKernelGetMutexInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetMutexInfo(uid, userInfo);
	if (args.size > 0x40)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x40);
	memset(info, 0, 0x40);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetMutexInfo(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitCond */
int _sceKernelWaitCond(SceUID condId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitCondForUser(condId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitCondForUser(condId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetCondInfo */
int _sceKernelGetCondInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetCondInfoForUser(uid, userInfo);
	if (args.size > 0x34)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x34);
	memset(info, 0, 0x34);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetCondInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCancelMsgPipe */
int _sceKernelCancelMsgPipe(SceUID msgPipeId, int *numSendWaitThreads, int *numReceiveWaitThreads)
{
	int *kNumSendWaitThreads, *kNumReceiveWaitThreads;
	int ret, err;

	kNumSendWaitThreads = numSendWaitThreads;
	if (numSendWaitThreads != NULL) {
		kNumSendWaitThreads = __builtin_alloca(sizeof(*kNumSendWaitThreads));
		*kNumSendWaitThreads = 0;
	}
	kNumReceiveWaitThreads = numReceiveWaitThreads;
	if (numReceiveWaitThreads != NULL) {
		kNumReceiveWaitThreads = __builtin_alloca(sizeof(*kNumReceiveWaitThreads));
		*kNumReceiveWaitThreads = 0;
	}
	ret = sceKernelCancelMsgPipeForUser(msgPipeId, kNumSendWaitThreads, kNumReceiveWaitThreads);
	if (numSendWaitThreads != NULL) {
		err = ksceKernelCopyToUser(numSendWaitThreads, kNumSendWaitThreads, sizeof(*kNumSendWaitThreads));
		if (err < 0)
			return err;
	}
	if (numReceiveWaitThreads != NULL) {
		err = ksceKernelCopyToUser(numReceiveWaitThreads, kNumReceiveWaitThreads, sizeof(*kNumReceiveWaitThreads));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelGetMsgPipeInfo */
int _sceKernelGetMsgPipeInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetMsgPipeInfoForUser(uid, userInfo);
	if (args.size > 0x3c)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x3c);
	memset(info, 0, 0x3c);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetMsgPipeInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetLwMutexInfoById */
int _sceKernelGetLwMutexInfoById(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetLwMutexInfoByIdForUser(uid, userInfo);
	if (args.size > 0x40)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x40);
	memset(info, 0, 0x40);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetLwMutexInfoByIdForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetLwCondInfoById */
int _sceKernelGetLwCondInfoById(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetLwCondInfoByIdForUser(uid, userInfo);
	if (args.size > 0x38)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x38);
	memset(info, 0, 0x38);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetLwCondInfoByIdForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockReadRWLock */
int _sceKernelLockReadRWLock(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockReadRWLockForUser(rwLockId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockReadRWLockForUser(rwLockId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockReadRWLockCB */
int _sceKernelLockReadRWLockCB(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockReadRWLockCBForUser(rwLockId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockReadRWLockCBForUser(rwLockId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockWriteRWLock */
int _sceKernelLockWriteRWLock(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockWriteRWLockForUser(rwLockId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockWriteRWLockForUser(rwLockId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelLockWriteRWLockCB */
int _sceKernelLockWriteRWLockCB(SceUID rwLockId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelLockWriteRWLockCBForUser(rwLockId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelLockWriteRWLockCBForUser(rwLockId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCancelRWLock */
int _sceKernelCancelRWLock(SceUID rwLockId, int *numReadWaitThreads, int *numWriteWaitThreads, SceUInt32 type)
{
	int *kNumReadWaitThreads, *kNumWriteWaitThreads;
	int ret, err;

	kNumReadWaitThreads = numReadWaitThreads;
	if (numReadWaitThreads != NULL) {
		kNumReadWaitThreads = __builtin_alloca(sizeof(*kNumReadWaitThreads));
		*kNumReadWaitThreads = 0;
	}
	kNumWriteWaitThreads = numWriteWaitThreads;
	if (numWriteWaitThreads != NULL) {
		kNumWriteWaitThreads = __builtin_alloca(sizeof(*kNumWriteWaitThreads));
		*kNumWriteWaitThreads = 0;
	}
	ret = sceKernelCancelRWLockForUser(rwLockId, kNumReadWaitThreads, kNumWriteWaitThreads, type);
	if (numReadWaitThreads != NULL) {
		err = ksceKernelCopyToUser(numReadWaitThreads, kNumReadWaitThreads, sizeof(*kNumReadWaitThreads));
		if (err < 0)
			return err;
	}
	if (numWriteWaitThreads != NULL) {
		err = ksceKernelCopyToUser(numWriteWaitThreads, kNumWriteWaitThreads, sizeof(*kNumWriteWaitThreads));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelGetRWLockInfo */
int _sceKernelGetRWLockInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetRWLockInfoForUser(uid, userInfo);
	if (args.size > 0x3c)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x3c);
	memset(info, 0, 0x3c);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetRWLockInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelGetThreadEventInfo */
int _sceKernelGetThreadEventInfo(SceUID uid, void *userInfo, const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	void *info;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	if (userInfo == NULL)
		return sceKernelGetThreadEventInfoForUser(uid, userInfo);
	if (args.size > 0x40)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	info = __builtin_alloca(0x40);
	memset(info, 0, 0x40);
	ret = ksceKernelCopyFromUser(info, userInfo, args.size);
	if (ret < 0)
		return ret;
	ret = sceKernelGetThreadEventInfoForUser(uid, info);
	err = ksceKernelCopyToUser(userInfo, info, args.size);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelPMonThreadGetCounter */
int _sceKernelPMonThreadGetCounter(SceUID threadId, uint32_t counter, uint32_t *value)
{
	uint32_t *buf;
	int ret, err;

	if (value == NULL)
		return sceKernelPMonThreadGetCounterForUser(threadId, counter, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = sceKernelPMonThreadGetCounterForUser(threadId, counter, buf);
	err = ksceKernelCopyToUser(value, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitCondCB */
int _sceKernelWaitCondCB(SceUID condId, SceUInt32 *timeout)
{
	SceUInt32 *buf;
	int ret, err;

	if (timeout == NULL)
		return sceKernelWaitCondCBForUser(condId, NULL);
	buf = __builtin_alloca(sizeof(*buf));
	*buf = 0;
	ret = ksceKernelCopyFromUser(buf, timeout, sizeof(*buf));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitCondCBForUser(condId, buf);
	err = ksceKernelCopyToUser(timeout, buf, sizeof(*buf));
	if (err < 0)
		ret = err;
	return ret;
}


/* _sceKernelWaitEvent: extra arguments (outUserData, timeout) */
int _sceKernelWaitEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint64_t *outUserData, *kUserData;
	SceUInt32 *timeout, *kTimeout;
	uint32_t *kPattern;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	outUserData = (uint64_t *)args.arg[0];
	timeout = (SceUInt32 *)args.arg[1];
	kPattern = outPattern;
	if (outPattern != NULL) {
		kPattern = __builtin_alloca(sizeof(*kPattern));
		*kPattern = 0;
	}
	kUserData = outUserData;
	if (outUserData != NULL) {
		kUserData = __builtin_alloca(sizeof(*kUserData));
		((uint32_t *)kUserData)[0] = 0;
		((uint32_t *)kUserData)[1] = 0;
	}
	kTimeout = timeout;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = sceKernelWaitEventForUser(eventId, pattern, kPattern, kUserData, kTimeout);
	if (outPattern != NULL) {
		err = ksceKernelCopyToUser(outPattern, kPattern, sizeof(*kPattern));
		if (err < 0)
			return err;
	}
	if (outUserData != NULL) {
		err = ksceKernelCopyToUser(outUserData, kUserData, sizeof(*kUserData));
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelWaitEventCB */
int _sceKernelWaitEventCB(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			  const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint64_t *outUserData, *kUserData;
	SceUInt32 *timeout, *kTimeout;
	uint32_t *kPattern;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	outUserData = (uint64_t *)args.arg[0];
	timeout = (SceUInt32 *)args.arg[1];
	kPattern = outPattern;
	if (outPattern != NULL) {
		kPattern = __builtin_alloca(sizeof(*kPattern));
		*kPattern = 0;
	}
	kUserData = outUserData;
	if (outUserData != NULL) {
		kUserData = __builtin_alloca(sizeof(*kUserData));
		((uint32_t *)kUserData)[0] = 0;
		((uint32_t *)kUserData)[1] = 0;
	}
	kTimeout = timeout;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = sceKernelWaitEventCBForUser(eventId, pattern, kPattern, kUserData, kTimeout);
	if (outPattern != NULL) {
		err = ksceKernelCopyToUser(outPattern, kPattern, sizeof(*kPattern));
		if (err < 0)
			return err;
	}
	if (outUserData != NULL) {
		err = ksceKernelCopyToUser(outUserData, kUserData, sizeof(*kUserData));
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelPollEvent */
int _sceKernelPollEvent(SceUID eventId, uint32_t pattern, uint32_t *outPattern,
			uint64_t *outUserData)
{
	uint64_t *kUserData;
	uint32_t *kPattern;
	int ret, err;

	kPattern = outPattern;
	if (outPattern != NULL) {
		kPattern = __builtin_alloca(sizeof(*kPattern));
		*kPattern = 0;
	}
	kUserData = outUserData;
	if (outUserData != NULL) {
		kUserData = __builtin_alloca(sizeof(*kUserData));
		((uint32_t *)kUserData)[0] = 0;
		((uint32_t *)kUserData)[1] = 0;
	}
	ret = sceKernelPollEventForUser(eventId, pattern, kPattern, kUserData);
	if (outPattern != NULL) {
		err = ksceKernelCopyToUser(outPattern, kPattern, sizeof(*kPattern));
		if (err < 0)
			return err;
	}
	if (outUserData != NULL) {
		err = ksceKernelCopyToUser(outUserData, kUserData, sizeof(*kUserData));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelWaitMultipleEvents: extra arguments (results, timeout) */
int _sceKernelWaitMultipleEvents(const ThreadmgrWaitEvent *events, SceUInt32 count,
				 SceUInt32 mode, const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelWaitMultipleEventsForUser(events, count, mode,
		(ThreadmgrWaitEventResult *)args.arg[0], (SceUInt32 *)args.arg[1]);
}

/* _sceKernelWaitMultipleEventsCB */
int _sceKernelWaitMultipleEventsCB(const ThreadmgrWaitEvent *events, SceUInt32 count,
				   SceUInt32 mode, const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelWaitMultipleEventsCBForUser(events, count, mode,
		(ThreadmgrWaitEventResult *)args.arg[0], (SceUInt32 *)args.arg[1]);
}

/* _sceKernelPulseEventWithNotifyCallback: extra arguments (userData, arg) */
int _sceKernelPulseEventWithNotifyCallback(SceUID eventId, uint32_t pattern,
					   const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelPulseEventWithNotifyCallbackForUser(eventId, pattern,
		(uint64_t)args.arg[1] << 32 | args.arg[0], args.arg[2]);
}

/* _sceKernelCancelEventWithSetPattern: extra arguments (userData, numWaitThreads) */
int _sceKernelCancelEventWithSetPattern(SceUID eventId, uint32_t setPattern,
					const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int *numWaitThreads, *kNum;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	numWaitThreads = (int *)args.arg[2];
	if (numWaitThreads == NULL)
		return sceKernelCancelEventWithSetPatternForUser(eventId, setPattern,
			(uint64_t)args.arg[1] << 32 | args.arg[0], NULL);
	kNum = __builtin_alloca(sizeof(*kNum));
	*kNum = 0;
	ret = sceKernelCancelEventWithSetPatternForUser(eventId, setPattern,
		(uint64_t)args.arg[1] << 32 | args.arg[0], kNum);
	err = ksceKernelCopyToUser(numWaitThreads, kNum, sizeof(*kNum));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCreateEventFlag: the option's first word only */
SceUID _sceKernelCreateEventFlag(const char *name, SceUInt32 attr, uint32_t initPattern,
				 const void *option)
{
	uint32_t *kOption;
	int ret;

	kOption = (uint32_t *)option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateEventFlagForUser(name, attr, initPattern, kOption);
}

/* _sceKernelWaitEventFlag: extra arguments (outPattern, timeout) */
int _sceKernelWaitEventFlag(SceUID evfId, uint32_t pattern, uint32_t mode,
			    const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint32_t *outPattern, *kPattern;
	SceUInt32 *timeout, *kTimeout;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	outPattern = (uint32_t *)args.arg[0];
	timeout = (SceUInt32 *)args.arg[1];
	kPattern = outPattern;
	if (outPattern != NULL) {
		kPattern = __builtin_alloca(sizeof(*kPattern));
		*kPattern = 0;
	}
	kTimeout = timeout;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = sceKernelWaitEventFlagForUser(evfId, pattern, mode, kPattern, kTimeout);
	if (outPattern != NULL) {
		err = ksceKernelCopyToUser(outPattern, kPattern, sizeof(*kPattern));
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelWaitEventFlagCB */
int _sceKernelWaitEventFlagCB(SceUID evfId, uint32_t pattern, uint32_t mode,
			      const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint32_t *outPattern, *kPattern;
	SceUInt32 *timeout, *kTimeout;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	outPattern = (uint32_t *)args.arg[0];
	timeout = (SceUInt32 *)args.arg[1];
	kPattern = outPattern;
	if (outPattern != NULL) {
		kPattern = __builtin_alloca(sizeof(*kPattern));
		*kPattern = 0;
	}
	kTimeout = timeout;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = sceKernelWaitEventFlagCBForUser(evfId, pattern, mode, kPattern, kTimeout);
	if (outPattern != NULL) {
		err = ksceKernelCopyToUser(outPattern, kPattern, sizeof(*kPattern));
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelCreateSema: extra arguments (maxCount, option) */
SceUID _sceKernelCreateSema(const char *name, SceUInt32 attr, int initCount,
			    const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint32_t *option, *kOption;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	option = (uint32_t *)args.arg[1];
	kOption = option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateSemaForUser(name, attr, initCount, args.arg[0], kOption);
}

/* _sceKernelCreateSema_16XX */
SceUID _sceKernelCreateSema_16XX(const char *name, SceUInt32 attr, int initCount,
				 const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint32_t *option, *kOption;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	option = (uint32_t *)args.arg[1];
	kOption = option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateSema_16XXForUser(name, attr, initCount, args.arg[0], kOption);
}

/* _sceKernelCreateMutex: the option's first two words */
SceUID _sceKernelCreateMutex(const char *name, SceUInt32 attr, int initCount,
			     const void *option)
{
	uint64_t *kOption;
	int ret;

	kOption = (uint64_t *)option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateMutexForUser(name, attr, initCount, kOption);
}

/* _sceKernelCreateCond: the option's first word */
SceUID _sceKernelCreateCond(const char *name, SceUInt32 attr, SceUID mutexId,
			    const void *option)
{
	uint32_t *kOption;
	int ret;

	kOption = (uint32_t *)option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateCondForUser(name, attr, mutexId, kOption);
}

/* _sceKernelCreateMsgPipeWithLR: extra arguments (bufferSize, option, lr) */
SceUID _sceKernelCreateMsgPipeWithLR(const char *name, int memType, SceUInt32 attr,
				     const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	void *option, *kOption;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	option = (void *)args.arg[1];
	kOption = option;
	if (option != NULL) {
		kOption = __builtin_alloca(MSG_PIPE_OPT_SIZE);
		ret = ksceKernelCopyFromUser(kOption, option, MSG_PIPE_OPT_SIZE);
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateMsgPipeWithLRForUser(name, memType, attr, args.arg[0], kOption,
						   (const void *)args.arg[2]);
}

/* the msg pipe vector syscalls: extra arguments (mode, result, timeout) */
int _sceKernelSendMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelSendMsgPipeVectorForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1], (SceUInt32 *)args.arg[2]);
}

int _sceKernelSendMsgPipeVectorCB(SceUID msgPipeId, const void *vector, int count,
				  const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelSendMsgPipeVectorCBForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1], (SceUInt32 *)args.arg[2]);
}

int _sceKernelTrySendMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				   const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelTrySendMsgPipeVectorForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1]);
}

int _sceKernelReceiveMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				   const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelReceiveMsgPipeVectorForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1], (SceUInt32 *)args.arg[2]);
}

int _sceKernelReceiveMsgPipeVectorCB(SceUID msgPipeId, const void *vector, int count,
				     const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelReceiveMsgPipeVectorCBForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1], (SceUInt32 *)args.arg[2]);
}

int _sceKernelTryReceiveMsgPipeVector(SceUID msgPipeId, const void *vector, int count,
				      const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelTryReceiveMsgPipeVectorForUser(msgPipeId, vector, count, args.arg[0],
		(SceSize *)args.arg[1]);
}

/* __sceKernelCreateLwMutex: extra arguments (initCount, option) */
int __sceKernelCreateLwMutex(void *work, const char *name, SceUInt32 attr,
			     const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	uint32_t *option, *kOption;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	option = (uint32_t *)args.arg[1];
	kOption = option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return _sceKernelCreateLwMutexForUser(work, name, attr, args.arg[0], kOption);
}

/* _sceKernelDeleteLwMutex */
int _sceKernelDeleteLwMutex(void *work)
{
	return sceKernelDeleteLwMutexForUser(work);
}

/* _sceKernelCreateLwCond: extra arguments (lwMutex work, option) */
int _sceKernelCreateLwCond(ThreadmgrLwCondWork *work, const char *name,
			   SceUInt32 attr, const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	void *lwMutex, *kLwMutex;
	uint32_t *option, *kOption;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	lwMutex = (void *)args.arg[0];
	kLwMutex = lwMutex;
	if (lwMutex != NULL) {
		kLwMutex = __builtin_alloca(LW_MUTEX_WORK_SIZE);
		ret = ksceKernelCopyFromUser(kLwMutex, lwMutex, LW_MUTEX_WORK_SIZE);
		if (ret < 0)
			return ret;
	}
	option = (uint32_t *)args.arg[1];
	kOption = option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateLwCondForUser(work, name, attr, kLwMutex, kOption);
}

/* _sceKernelDeleteLwCond: the work area comes in and goes back out */
int _sceKernelDeleteLwCond(ThreadmgrLwCondWork *work)
{
	ThreadmgrLwCondWork *kWork;
	int ret, err;

	if (work == NULL)
		return sceKernelDeleteLwCondForUser(NULL);
	kWork = __builtin_alloca(sizeof(*kWork));
	ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
	if (ret < 0)
		return ret;
	ret = sceKernelDeleteLwCondForUser(kWork);
	err = ksceKernelCopyToUser(work, kWork, sizeof(*kWork));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitLwCond */
int _sceKernelWaitLwCond(const ThreadmgrLwCondWork *work, SceUInt32 *timeout)
{
	ThreadmgrLwCondWork *kWork;
	SceUInt32 *kTimeout;
	int ret, err;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	if (timeout == NULL)
		return sceKernelWaitLwCondForUser(kWork, NULL);
	kTimeout = __builtin_alloca(sizeof(*kTimeout));
	*kTimeout = 0;
	ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitLwCondForUser(kWork, kTimeout);
	err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelWaitLwCondCB */
int _sceKernelWaitLwCondCB(const ThreadmgrLwCondWork *work, SceUInt32 *timeout)
{
	ThreadmgrLwCondWork *kWork;
	SceUInt32 *kTimeout;
	int ret, err;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	if (timeout == NULL)
		return sceKernelWaitLwCondCBForUser(kWork, NULL);
	kTimeout = __builtin_alloca(sizeof(*kTimeout));
	*kTimeout = 0;
	ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
	if (ret < 0)
		return ret;
	ret = sceKernelWaitLwCondCBForUser(kWork, kTimeout);
	err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelSignalLwCond */
int _sceKernelSignalLwCond(const ThreadmgrLwCondWork *work)
{
	ThreadmgrLwCondWork *kWork;
	int ret;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	return sceKernelSignalLwCondForUser(kWork);
}

/* _sceKernelSignalLwCondAll */
int _sceKernelSignalLwCondAll(const ThreadmgrLwCondWork *work)
{
	ThreadmgrLwCondWork *kWork;
	int ret;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	return sceKernelSignalLwCondAllForUser(kWork);
}

/* _sceKernelSignalLwCondTo */
int _sceKernelSignalLwCondTo(const ThreadmgrLwCondWork *work, SceUID threadId)
{
	ThreadmgrLwCondWork *kWork;
	int ret;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	return sceKernelSignalLwCondToForUser(kWork, threadId);
}

/* _sceKernelGetLwCondInfo */
int _sceKernelGetLwCondInfo(const ThreadmgrLwCondWork *work, void *userInfo)
{
	ThreadmgrLwCondWork *kWork;
	uint32_t *info;
	int ret, err, i;

	kWork = (ThreadmgrLwCondWork *)work;
	if (work != NULL) {
		kWork = __builtin_alloca(sizeof(*kWork));
		ret = ksceKernelCopyFromUser(kWork, work, sizeof(*kWork));
		if (ret < 0)
			return ret;
	}
	if (userInfo == NULL)
		return sceKernelGetLwCondInfoForUser(kWork, NULL);
	info = __builtin_alloca(LW_COND_INFO_SIZE);
	for (i = 0; i < LW_COND_INFO_SIZE / 4; i++)
		info[i] = 0;
	ret = ksceKernelCopyFromUser(info, userInfo, LW_COND_INFO_SIZE);
	if (ret < 0)
		return ret;
	ret = sceKernelGetLwCondInfoForUser(kWork, (ThreadmgrLwCondInfo *)info);
	err = ksceKernelCopyToUser(userInfo, info, LW_COND_INFO_SIZE);
	if (err < 0)
		ret = err;
	return ret;
}

/* _sceKernelCreateRWLock: the option's first word */
SceUID _sceKernelCreateRWLock(const char *name, SceUInt32 attr, const void *option)
{
	uint32_t *kOption;
	int ret;

	kOption = (uint32_t *)option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateRWLockForUser(name, attr, kOption);
}

/* _sceKernelCreateSimpleEvent: the option's first word */
SceUID _sceKernelCreateSimpleEvent(const char *name, SceUInt32 attr, uint32_t initPattern,
				   const void *option)
{
	uint32_t *kOption;
	int ret;

	kOption = (uint32_t *)option;
	if (option != NULL) {
		kOption = __builtin_alloca(sizeof(*kOption));
		ret = ksceKernelCopyFromUser(kOption, option, sizeof(*kOption));
		if (ret < 0)
			return ret;
	}
	return sceKernelCreateSimpleEventForUser(name, attr, initPattern, kOption);
}

/* _sceKernelRegisterThreadEventHandler: extra arguments (handler, arg) */
SceUID _sceKernelRegisterThreadEventHandler(const char *name, SceUID target, uint32_t mask,
					    const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	return sceKernelRegisterThreadEventHandlerForUser(name, target, mask, args.arg[0],
							   args.arg[1]);
}

/* _sceKernelSetThreadContextForVM: extra arguments (cpu size, vfp size) */
int _sceKernelSetThreadContextForVM(SceUID threadId, const void *userCpu, const void *userVfp,
				    const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	void *cpu, *vfp;
	int ret;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	cpu = NULL;
	if (userCpu != NULL) {
		if (args.arg[0] > VM_CPU_CONTEXT_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		cpu = __builtin_alloca(VM_CPU_CONTEXT_SIZE);
		ret = ksceKernelCopyFromUser(cpu, userCpu, args.arg[0]);
		if (ret < 0)
			return ret;
	}
	vfp = NULL;
	if (userVfp != NULL) {
		if (args.arg[1] > VM_VFP_CONTEXT_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		vfp = __builtin_alloca(VM_VFP_CONTEXT_SIZE);
		ret = ksceKernelCopyFromUser(vfp, userVfp, args.arg[1]);
		if (ret < 0)
			return ret;
	}
	return setThreadContextForVMForUser(threadId, cpu, vfp);
}

/* _sceKernelGetThreadContextForVM: extra arguments (cpu size, vfp size) */
int _sceKernelGetThreadContextForVM(SceUID threadId, void *userCpu, void *userVfp,
				    const ThreadmgrSyscallArgs *userArgs)
{
	ThreadmgrSyscallArgs args;
	void *cpu, *vfp;
	int ret, err;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	cpu = NULL;
	if (userCpu != NULL) {
		if (args.arg[0] > VM_CPU_CONTEXT_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		cpu = __builtin_alloca(VM_CPU_CONTEXT_SIZE);
		memset(cpu, 0, VM_CPU_CONTEXT_SIZE);
		ret = ksceKernelCopyFromUser(cpu, userCpu, args.arg[0]);
		if (ret < 0)
			return ret;
	}
	vfp = NULL;
	if (userVfp != NULL) {
		if (args.arg[1] > VM_VFP_CONTEXT_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		vfp = __builtin_alloca(VM_VFP_CONTEXT_SIZE);
		memset(vfp, 0, VM_VFP_CONTEXT_SIZE);
		ret = ksceKernelCopyFromUser(vfp, userVfp, args.arg[1]);
		if (ret < 0)
			return ret;
	}
	ret = getThreadContextForVMForUser(threadId, cpu, vfp);
	if (userCpu != NULL) {
		err = ksceKernelCopyToUser(userCpu, cpu, args.arg[0]);
		if (err < 0)
			return err;
	}
	if (userVfp != NULL) {
		err = ksceKernelCopyToUser(userVfp, vfp, args.arg[1]);
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelWaitException: extra arguments (info size) */
int _sceKernelWaitException(uint32_t mask, void *userInfo, SceUInt32 *timeout,
			    const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	SceUInt32 *kTimeout;
	uint32_t *info;
	int ret, err, i;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	info = NULL;
	if (userInfo != NULL) {
		if (args.size > EXCEPTION_INFO_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		info = __builtin_alloca(EXCEPTION_INFO_SIZE);
		for (i = 0; i < EXCEPTION_INFO_SIZE / 4; i++)
			info[i] = 0;
		ret = ksceKernelCopyFromUser(info, userInfo, args.size);
		if (ret < 0)
			return ret;
	}
	kTimeout = NULL;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = waitExceptionForUser(mask, info, kTimeout);
	if (userInfo != NULL) {
		err = ksceKernelCopyToUser(userInfo, info, args.size);
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* _sceKernelWaitExceptionCB */
int _sceKernelWaitExceptionCB(uint32_t mask, void *userInfo, SceUInt32 *timeout,
			      const ThreadmgrSyscallInfoArgs *userArgs)
{
	ThreadmgrSyscallInfoArgs args;
	SceUInt32 *kTimeout;
	uint32_t *info;
	int ret, err, i;

	ret = ksceKernelCopyFromUser(&args, userArgs, sizeof(args));
	if (ret < 0)
		return ret;
	info = NULL;
	if (userInfo != NULL) {
		if (args.size > EXCEPTION_INFO_SIZE)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		info = __builtin_alloca(EXCEPTION_INFO_SIZE);
		for (i = 0; i < EXCEPTION_INFO_SIZE / 4; i++)
			info[i] = 0;
		ret = ksceKernelCopyFromUser(info, userInfo, args.size);
		if (ret < 0)
			return ret;
	}
	kTimeout = NULL;
	if (timeout != NULL) {
		kTimeout = __builtin_alloca(sizeof(*kTimeout));
		*kTimeout = 0;
		ret = ksceKernelCopyFromUser(kTimeout, timeout, sizeof(*kTimeout));
		if (ret < 0)
			return ret;
	}
	ret = waitExceptionCBForUser(mask, info, kTimeout);
	if (userInfo != NULL) {
		err = ksceKernelCopyToUser(userInfo, info, args.size);
		if (err < 0)
			return err;
	}
	if (timeout != NULL) {
		err = ksceKernelCopyToUser(timeout, kTimeout, sizeof(*kTimeout));
		if (err < 0)
			ret = err;
	}
	return ret;
}

/* sceKernelChangeThreadCpuAffinityMask (user): the old mask in bits 16-19 */
int sceKernelChangeThreadCpuAffinityMask(SceUID uid, uint32_t mask)
{
	int result = 0;

	if (uid != 0)
		result = kscePUIDtoGUID(0, uid);
	if (result < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	result = ksceKernelChangeThreadCpuAffinityMask(result, mask);
	return result < 0 ? result : (int)((uint32_t)result << 16);
}

/* sceKernelGetThreadCpuAffinityMask (user): the mask in bits 16-19 */
int SceThreadmgr_sceKernelGetThreadCpuAffinityMask(SceUID uid)
{
	int result;

	if (uid != 0) {
		result = kscePUIDtoGUID(0, uid);
		if (result < 0)
			return USER_ERROR_UNKNOWN_THREAD_ID;
		uid = result;
	}
	result = ksceKernelGetThreadCpuAffinityMask(uid);
	return result < 0 ? result : (int)((uint32_t)result << 16);
}

/* sceKernelChangeThreadPriority2 (user): the old priority */
int sceKernelChangeThreadPriority2(SceUID uid, uint32_t priority)
{
	int result = 0;

	if (uid != 0)
		result = kscePUIDtoGUID(0, uid);
	if (result < 0)
		return USER_ERROR_UNKNOWN_THREAD_ID;
	result = ksceKernelChangeThreadPriority(result, priority);
	SCE_DBG_ASSERT_CHECK_FUNC(result != 0, 267, "sceKernelChangeThreadPriority2ForUser");
	return result;
}

/* sceKernelChangeThreadPriority (user): 0 or an error */
int sceKernelChangeThreadPriority(SceUID uid, uint32_t priority)
{
	int result = sceKernelChangeThreadPriority2(uid, priority);

	return result < 0 ? result : 0;
}

int sceKernelSetEvent(SceUID eventId, uint32_t pattern, uint32_t userDataLow,
		      uint32_t userDataHigh)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelSetEvent(event, pattern, (uint64_t)userDataHigh << 32 | userDataLow);
}

int sceKernelClearEvent(SceUID eventId, uint32_t keep)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (keep & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return ksceKernelClearEvent(event, keep);
}

int sceKernelSetEventWithNotifyCallbackForUser(SceUID eventId, uint32_t pattern,
					       uint32_t userDataLow, uint32_t userDataHigh,
					       uint32_t arg)
{
	SceUID event = kscePUIDtoGUID(0, eventId);

	if (event < 0)
		return USER_ERROR_UNKNOWN_EVENT_ID;
	if (pattern & EVENT_PATTERN_KERNEL)
		return SCE_KERNEL_ERROR_ILLEGAL_PATTERN;
	return sceKernelSetEventWithNotifyCallback(event, pattern,
						   (uint64_t)userDataHigh << 32 | userDataLow, arg);
}

int sceKernelPollEventFlagForUser(SceUID evfId, uint32_t pattern, uint32_t mode,
				  uint32_t *outPattern)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  ksceKernelPollEventFlag(guid, pattern, mode, outPattern);
}

int sceKernelCancelEventFlagForUser(SceUID evfId, uint32_t pattern, int *numWaitThreads)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  sceKernelCancelEventFlag(guid, pattern, numWaitThreads);
}

int sceKernelGetEventFlagInfoForUser(SceUID evfId, ThreadmgrEventFlagInfo *info)
{
	uint32_t attributes;
	int ret = kscePUIDtoGUID(0, evfId);

	if (ret < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	ret = ksceKernelGetEventFlagInfo(ret, info);
	if (ret >= 0) {
		info->evfId = evfId;
		ret = scePUIDGetAttr(0, evfId, &attributes);
		if (ret >= 0 && (attributes & PUID_ATTR_OPENED))
			info->attr |= PUID_ATTR_OPENED;
	}
	return ret;
}

int sceKernelClearEventFlag(SceUID evfId, uint32_t pattern)
{
	SceUID guid = kscePUIDtoGUID(0, evfId);

	return guid < 0 ? (int)USER_ERROR_UNKNOWN_EVENT_FLAG_ID :
			  ksceKernelClearEventFlag(guid, pattern);
}

/*
 * SceKernelThreadMgr (threadmgr_for_user.c): the user side of event flag
 * lifetimes (open, close, delete by PUID) and the user name copy every
 * create and open uses.
 */

#define UID_NAME_SIZE			32
#define UID_ATTR_NAME_CHECKED		0x00000080	/* a name too long is an error */
#define USER_ERROR_UID_NAME_TOO_LONG	0xc0024002

/* copyinUIDNameWithAttr: a user's object name; with UID_ATTR_NAME_CHECKED, at most 31 chars */
int copyinUIDNameWithAttr(char *name, const char *userName, uint32_t attributes)
{
	int result;

	if (userName == NULL) {
		SCE_DBG_PRINTF(0, 118, " k: error: copyinUIDNameWithAttr() pName_us=NULL\n");
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	}
	result = ksceKernelStrncpyFromUser(name, userName, UID_NAME_SIZE);
	if (result < 0) {
		SCE_DBG_PRINTF(0, 123, " k: error: copyinUIDNameWithAttr() -> "
			       "sceKernelStrncpyFromUser()=0x%08X\n", result);
		return result;
	}
	if (!(attributes & UID_ATTR_NAME_CHECKED) || result < UID_NAME_SIZE)
		return 0;
	name[UID_NAME_SIZE - 1] = '\0';
	SCE_DBG_PRINTF(0, 129, "***\n***\n***\n*** error: copyinUIDNameWithAttr(\"%s\") = "
		       "SCE_KERNEL_ERROR_UID_NAME_TOO_LONG\n***\n***\n***\n", name);
	return USER_ERROR_UID_NAME_TOO_LONG;
}

/* deletes or closes a user's event flag */
int closeEventFlagForUser(SceUID uid, int delete)
{
	SceClass *cls;
	uint32_t attributes, version;

	if (kscePUIDGetClass(0, uid, &cls) < 0 ||
	    cls != s_threadmgrRuntime.manager->eventFlagClass ||
	    scePUIDGetAttr(0, uid, &attributes) < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	if (!delete)
		attributes ^= PUID_ATTR_OPENED;
	if (attributes & PUID_ATTR_OPENED) {
		if (SceSysrootForDriver_CAE820B2(0, &version) < 0)
			version = SDK_VERSION_CLOSE_MISMATCH;
		if (version >= SDK_VERSION_CLOSE_MISMATCH) {
			SCE_DBG_PRINTF(0, 1523, "*** error: closeEventFlagForUser(0x%08X, 0x%08X), "
				       "delete/close mismatch\n", uid, delete);
			return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
		}
	}
	return kscePUIDClose(0, uid);
}

/* sceKernelDeleteEventFlag (user) */
int sceKernelDeleteEventFlag(SceUID uid)
{
	return closeEventFlagForUser(uid, 1);
}

/* sceKernelCloseEventFlag (user) */
int sceKernelCloseEventFlag(SceUID uid)
{
	return closeEventFlagForUser(uid, 0);
}

/* sceKernelOpenEventFlag (user): a PUID for the event flag named userName */
int sceKernelOpenEventFlag(const char *userName)
{
	UIDCreateOpt opt;
	char name[UID_NAME_SIZE];
	int result;

	result = copyinUIDNameWithAttr(name, userName, UID_ATTR_NAME_CHECKED);
	if (result < 0)
		return result;
	result = SceSysmemForKernel_BDE00106(name);
	if (result < 0)
		return result;
	if (s_threadmgrRuntime.manager->eventFlagClass !=
	    SceSysmemForKernel_368F1C36(result))
		return SCE_KERNEL_ERROR_DIFFERENT_UID_CLASS;
	opt = (UIDCreateOpt){
		.flags = UID_CREATE_OPT_ATTR,
		.attr = PUID_ATTR_OPENED,
	};
	return SceSysmemForKernel_1FF2749A(currentThreadContext()->processId, result, &opt);
}

/* sceKernelSetEventFlag (user) */
int sceKernelSetEventFlag(SceUID uid, uint32_t pattern)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return USER_ERROR_UNKNOWN_EVENT_FLAG_ID;
	return ksceKernelSetEventFlag(guid, pattern);
}

#define TIMER_PUID_ATTR_OPENED		0x80000
extern int copyinUIDNameWithAttr(char *name, const char *userName, uint32_t attributes);
int ksceKernelStartTimer(SceUID uid);
int ksceKernelStopTimer(SceUID uid);
int sceKernelGetTimerBase(SceUID uid, uint64_t *value);
int sceKernelGetTimerTime(SceUID uid, uint64_t *value);
uint64_t ksceKernelGetTimerBaseWide(SceUID uid);
uint64_t ksceKernelGetTimerTimeWide(SceUID uid);
int sceKernelSetTimerTime(SceUID uid, uint64_t *value);
uint64_t ksceKernelSetTimerTimeWide(SceUID uid, uint64_t value);
int sceKernelSetTimerEvent(SceUID uid, uint32_t type, const volatile uint64_t *interval, int repeat);
int sceKernelCancelTimer(SceUID uid, uint32_t *count);
int sceKernelGetTimerEventRemainingTime(SceUID uid, uint64_t *remaining);

/* name unknown */
int cancelTimerForUser(SceUID uid, uint32_t *count)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelCancelTimer(guid, count);
}

int createTimerForUser(const char *name, uint32_t attributes, const uint32_t *options)
{
	char copied[0x20];
	int result = copyinUIDNameWithAttr(copied, name, attributes);

	if (result >= 0) {
		result = sceKernelCreateTimer(copied, attributes, options);
		if (result >= 0)
			result = kscePUIDOpenByGUID(currentThreadContext()->processId, result);
	}
	return result;
}

/* name unknown */
int setTimerEventForUser(SceUID uid, uint32_t type, const volatile uint64_t *interval, uint32_t repeat)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelSetTimerEvent(guid, type, interval, repeat);
}

/* name unknown */
int getTimerEventRemainingTimeForUser(SceUID uid, uint64_t *output)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? (int)0xc0028241u : sceKernelGetTimerEventRemainingTime(guid, output);
}

/* name unknown */
int getTimerInfoForUser(SceUID uid, ThreadmgrTimerInfo *output)
{
	SceUID guid = kscePUIDtoGUID(0, uid);
	uint32_t attributes;
	int result;

	if (guid < 0)
		return 0xc0028241u;
	result = SceThreadmgrForDriver_19F3E92A(guid, output);
	if (result >= 0) {
		output->timerId = uid;
		result = scePUIDGetAttr(0, uid, &attributes);
		if (result >= 0 && (attributes & 0x80000))
			output->attr |= TIMER_PUID_ATTR_OPENED;
	}
	return result;
}

int sceKernelOpenTimer(const char *userName)
{
	struct ThreadCB *thread = currentThreadContext();
	char name[32];
	UIDCreateOpt options = {
		.flags = UID_CREATE_OPT_ATTR | UID_CREATE_OPT_CLASS,
		.attr = TIMER_PUID_ATTR_OPENED,
	};
	int result;

	result = copyinUIDNameWithAttr(name, userName, 0x80);
	if (result < 0)
		return result;
	options.cls = s_threadmgrRuntime.manager->timerClass;
	result = SceSysmemForKernel_201E970B(0, name, &options);
	if (result >= 0) {
		uint32_t low = thread->processId;
		uint32_t high = thread->userThreadId | ((int32_t)low >> 31);
		SceUID guid = kscePUIDtoGUID(0, result);
		int wake = sceKernelSetEventWithNotifyCallback(guid, 0x100, (uint64_t)high << 32 | low, 0x100);
		SCE_DBG_ASSERT_CHECK_FUNC(wake >= 0, 3328, "sceKernelOpenTimerForUser");
	}
	return result;
}

int sceKernelCloseTimer(SceUID uid)
{
	SceKernelIntrStatus intrState = ksceKernelCpuSuspendIntr();
	ThreadmgrUidTimer *object;
	struct ThreadCB *thread;
	SceUID guid;
	uint32_t attributes;
	SceUInt32 version;
	uint32_t wake = 0;
	int result = 0xc0028241u;

	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0 || ksceGUIDReferObjectWithClass(guid,
		s_threadmgrRuntime.manager->timerClass, (SceObjectBase **)&object) < 0)
		goto resume;
	if (scePUIDGetAttr(0, uid, &attributes) < 0)
		goto release;
	if (!(attributes & 0x80000)) {
		int status = SceSysrootForDriver_CAE820B2(0, &version);
		if (status < 0 || version >= 0x03100000) {
			SCE_DBG_PRINTF_FUNC(0, 3375, "sceKernelCloseTimerForUser",
				"*** error: sceKernelCloseTimerForUser(0x%08X), delete/close mismatch\n", uid);
			goto release;
		}
	}
	if (kscePUIDClose(0, uid) < 0)
		goto release;
	thread = currentThreadContext();
	uint32_t high = thread->userThreadId;
	thread = currentThreadContext();
	uint32_t low = thread->processId;
	high |= (int32_t)low >> 31;
	ksceKernelSpinlockLowLock(&object->base.event.lock);
	wake = setEventWithNotifyCallback(guid, 0x200, (uint64_t)high << 32 | low,
					  0x200, &object->base.event);
	ksceKernelSpinlockLowUnlock(&object->base.event.lock);
	result = 0;
release:
	if (ksceGUIDReleaseObject(guid) < 0)
		SCE_PANIC_FUNC(3401, "sceKernelCloseTimerForUser");
	if (wake & 1)
		switchThread(0);
resume:
	ksceKernelCpuResumeIntr(intrState);
	return result;
}

/* name unknown */
int getTimerBaseForUser(SceUID uid, uint64_t *value)
{
	int guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return 0xc0028241u;
	return sceKernelGetTimerBase(guid, value);
}

/* name unknown */
int getTimerTimeForUser(SceUID uid, uint64_t *value)
{
	int guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return 0xc0028241u;
	return sceKernelGetTimerTime(guid, value);
}

uint64_t sceKernelGetTimerBaseWide(SceUID uid)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelGetTimerBaseWide(guid);
	SCE_DBG_PRINTF_FUNC(0, 3481, "sceKernelGetTimerBaseWideForUser",
		"*** error: sceKernelGetTimerBaseWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}

uint64_t sceKernelGetTimerTimeWide(SceUID uid)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelGetTimerTimeWide(guid);
	SCE_DBG_PRINTF_FUNC(0, 3522, "sceKernelGetTimerTimeWideForUser",
		"*** error: sceKernelGetTimerTimeWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}

int sceKernelStartTimer(SceUID uid)
{
	int guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? 0xc0028241u : ksceKernelStartTimer(guid);
}

int sceKernelStopTimer(SceUID uid)
{
	int guid = kscePUIDtoGUID(0, uid);

	return guid < 0 ? 0xc0028241u : ksceKernelStopTimer(guid);
}

/* name unknown */
int setTimerTimeForUser(SceUID uid, uint64_t *userValue)
{
	uint64_t value;
	SceUInt32 version;
	int result, guid;

	result = ksceKernelCopyFromUser(&value, userValue, sizeof(value));
	if (result < 0)
		return result;
	guid = kscePUIDtoGUID(0, uid);
	if (guid < 0)
		return 0xc0028241u;
	result = sceKernelSetTimerTime(guid, &value);
	if (result >= 0 && SceSysrootForDriver_CAE820B2(0, &version) >= 0 && version >= 0x02000000)
		return ksceKernelCopyToUser(userValue, &value, sizeof(value));
	return result;
}

uint64_t sceKernelSetTimerTimeWide(SceUID uid, uint64_t value)
{
	int guid;

	sceKernelApiDeclareTypeUInt64();
	guid = kscePUIDtoGUID(0, uid);
	if (guid >= 0)
		return ksceKernelSetTimerTimeWide(guid, value);
	SCE_DBG_PRINTF_FUNC(0, 3587, "sceKernelSetTimerTimeWideForUser",
		"*** error: sceKernelSetTimerTimeWideForUser(0x%08X), scePUIDtoGUID()=0x%08X\n", uid, guid);
	return UINT64_MAX;
}

#define SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID	0x80028022
#define SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID	0x80028021

/* the cores of _sceKernelWaitSignal and _sceKernelWaitSignalCB */
int sceKernelWaitSignalForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return sceKernelWaitSignal(arg0, arg1, timeout);
}

int sceKernelWaitSignalCBForUser(uint32_t arg0, uint32_t arg1, SceUInt32 *timeout)
{
	return sceKernelWaitSignalCB(arg0, arg1, timeout);
}

/* sceKernelSendSignal (user) */
int sceKernelSendSignal(SceUID threadId)
{
	SceUID guid;

	if (threadId == 0)
		return SCE_KERNEL_ERROR_ILLEGAL_THREAD_ID;
	guid = kscePUIDtoGUID(0, threadId);
	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_THREAD_ID | SYSCALL_ERROR_BIT;
	return sendSignal(guid);
}

/* Callback user syscalls: PUID to GUID, then the kernel export. */
int ksceKernelGetCallbackCount(SceUID uid);
int ksceKernelCancelCallback(SceUID uid);

int sceKernelGetCallbackCount(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | 0x40000000;
	return ksceKernelGetCallbackCount(guid);
}

int sceKernelCancelCallback(SceUID uid)
{
	SceUID guid = kscePUIDtoGUID(0, uid);

	if (guid < 0)
		return SCE_KERNEL_ERROR_UNKNOWN_CALLBACK_ID | 0x40000000;
	return ksceKernelCancelCallback(guid);
}
