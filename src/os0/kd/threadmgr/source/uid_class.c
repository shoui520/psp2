/*
 * SceKernelThreadMgr (threadmgr.c): which of the thread manager's classes
 * an object belongs to, as the small number the user libraries know.
 */

#include "private.h"
#include "sce/debug.h"
#include "sce/sysmem.h"
#include "syscall_context.h"

#undef __FILE_NAME__
#define __FILE_NAME__ "threadmgr.c"

#define THREADMGR_UID_CLASS_THREAD		1
#define THREADMGR_UID_CLASS_SEMAPHORE		2
#define THREADMGR_UID_CLASS_EVENT_FLAG		3
#define THREADMGR_UID_CLASS_MUTEX		4
#define THREADMGR_UID_CLASS_COND		5
#define THREADMGR_UID_CLASS_TIMER		6
#define THREADMGR_UID_CLASS_MSG_PIPE		7
#define THREADMGR_UID_CLASS_CALLBACK		8
#define THREADMGR_UID_CLASS_THREAD_EVENT	9
#define THREADMGR_UID_CLASS_LW_MUTEX		10
#define THREADMGR_UID_CLASS_LW_COND		11
#define THREADMGR_UID_CLASS_RW_LOCK		12
#define THREADMGR_UID_CLASS_SIMPLE_EVENT	13
#define THREADMGR_UID_CLASS_EXCEPTION		0x20
#define THREADMGR_UID_CLASS_FAST_MUTEX		0x40
#define THREADMGR_UID_CLASS_DELAY		0x41
#define THREADMGR_UID_CLASS_ALARM		0x42
#define THREADMGR_UID_CLASS_UNK2374		0x43
#define THREADMGR_UID_CLASS_WORK_TASK		0x44

SceClass *SceSysmemForKernel_368F1C36(SceUID uid);

/* ksceKernelGetThreadmgrUIDClass: the internal classes have no number */
int ksceKernelGetThreadmgrUIDClass(SceUID uid)
{
	volatile struct SceKernelThreadmgrPrivate *manager;
	SceClass *cls;
	uint32_t saved;
	int ret;

	saved = ThreadmgrUserRoWord();
	ThreadmgrSetUserRoWord(saved << 16);
	cls = SceSysmemForKernel_368F1C36(uid);
	manager = s_threadmgrRuntime.manager;
	if (cls == NULL)
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	else if (manager->threadClass == cls)
		ret = THREADMGR_UID_CLASS_THREAD;
	else if (manager->semaphoreClass == cls)
		ret = THREADMGR_UID_CLASS_SEMAPHORE;
	else if (manager->eventFlagClass == cls)
		ret = THREADMGR_UID_CLASS_EVENT_FLAG;
	else if (manager->mutexClass == cls)
		ret = THREADMGR_UID_CLASS_MUTEX;
	else if (manager->condClass == cls)
		ret = THREADMGR_UID_CLASS_COND;
	else if (manager->timerClass == cls)
		ret = THREADMGR_UID_CLASS_TIMER;
	else if (manager->msgPipeClass == cls)
		ret = THREADMGR_UID_CLASS_MSG_PIPE;
	else if (manager->callbackClass == cls)
		ret = THREADMGR_UID_CLASS_CALLBACK;
	else if (manager->threadEventClass == cls)
		ret = THREADMGR_UID_CLASS_THREAD_EVENT;
	else if (manager->lwMutexClass == cls)
		ret = THREADMGR_UID_CLASS_LW_MUTEX;
	else if (manager->lwCondClass == cls)
		ret = THREADMGR_UID_CLASS_LW_COND;
	else if (manager->rwLockClass == cls)
		ret = THREADMGR_UID_CLASS_RW_LOCK;
	else if (manager->simpleEventClass == cls)
		ret = THREADMGR_UID_CLASS_SIMPLE_EVENT;
	else if (manager->exceptionClass == cls)
		ret = THREADMGR_UID_CLASS_EXCEPTION;
	else if (manager->fastMutexClass == cls)
		ret = THREADMGR_UID_CLASS_FAST_MUTEX;
	else if (manager->delayClass == cls)
		ret = THREADMGR_UID_CLASS_DELAY;
	else if (manager->alarmClass == cls)
		ret = THREADMGR_UID_CLASS_ALARM;
	else if (manager->workQueueClass == cls)
		ret = THREADMGR_UID_CLASS_UNK2374;
	else if (manager->workTaskClass == cls)
		ret = THREADMGR_UID_CLASS_WORK_TASK;
	else {
		if (manager->cacheClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8455, "sceKernelGetThreadmgrUIDClass");
		else if (manager->waitableClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8458, "sceKernelGetThreadmgrUIDClass");
		else if (manager->registerCallbackClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8461, "sceKernelGetThreadmgrUIDClass");
		else if (manager->eventClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8464, "sceKernelGetThreadmgrUIDClass");
		else if (manager->cpuTimerClass == cls)
			SCE_DBG_ASSERT_CHECK_FUNC(0, 8467, "sceKernelGetThreadmgrUIDClass");
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ThreadmgrSetUserRoWord(saved);
	return ret;
}
