/*
 * SceSysmem: SceKernelSuspendForDriver. The power service (ScePower) lends a
 * table of callbacks for the suspend lock and the power requests; drivers
 * register suspend and resume handlers (32 each), and debug tools can hook
 * the calls.
 */

#include <sysmem/cp15.h>
#include "sysmem_internal.h"
#include <sysmem/sysevent.h>
#include <sysmem/suspend.h>
#include "sysevent_internal.h"

#define SUSPEND_HANDLERS		32

typedef struct SuspendHandler {
	int (*handler)(int value, void *args);
	SceUInt32 reserved;
	void *args;
} SuspendHandler;

/* the hooks of the debug tools */
static void (*s_lockSuspendHook)(int arg, const SuspendPowerCallbacks *callbacks,
				 int (*lockSuspend)(SceUInt32 arg));
static void (*s_unlockSuspendHook)(int arg, const SuspendPowerCallbacks *callbacks, int result,
				   int count);
static void (*volatile s_powerTickHook)(int fromProcess, int type, SceUID pid, SceUID tid,
			       const void *caller);

static void (*s_lockSuspendResultHook)(const SuspendPowerCallbacks *callbacks,
				       int (*lockSuspend)(SceUInt32 arg), int result, int count);
static void (*s_tickProcessHook)(SceUID targetPid, SceUInt32 mask, SceUID pid, SceUID tid);

static SceKernelSpinlock s_handlerLock;
static SuspendHandler s_suspendHandlers[SUSPEND_HANDLERS];
static const SuspendPowerCallbacks *s_callbacks;
/* suspend locks taken before the power service registered */
static SceInt32 s_earlyLockCount;
static SuspendHandler s_resumeHandlers[SUSPEND_HANDLERS];

void SceKernelSuspendForDriver_F2B07167(void *hook)
{
	s_powerTickHook = hook;
}

void SceKernelSuspendForDriver_8B3F02B8(void *hook)
{
	s_tickProcessHook = hook;
}

void SceKernelSuspendForDriver_4E5A3A23(void *hook)
{
	s_lockSuspendHook = hook;
}

void SceKernelSuspendForDriver_C00826AC(void *hook)
{
	s_lockSuspendResultHook = hook;
}

void SceKernelSuspendForDriver_0DE3CC02(void *hook)
{
	s_unlockSuspendHook = hook;
}

/* name unknown */
int SuspendInit(void)
{
	unsigned int i;

	for (i = 0; i < SUSPEND_HANDLERS; i++)
		s_suspendHandlers[i].handler = NULL;
	for (i = 0; i < SUSPEND_HANDLERS; i++)
		s_resumeHandlers[i].handler = NULL;
	s_handlerLock = 0;
	s_earlyLockCount = 0;
	s_callbacks = NULL;
	s_powerTickHook = NULL;
	s_tickProcessHook = NULL;
	s_lockSuspendHook = NULL;
	s_lockSuspendResultHook = NULL;
	s_unlockSuspendHook = NULL;
	g_sysEventPreHook = NULL;
	g_sysEventPostHook = NULL;
	return 0;
}

/* the power service's callbacks; returns the locks taken before */
int SceKernelSuspendForDriver_250ACD90(const SuspendPowerCallbacks *callbacks)
{
	s_callbacks = callbacks;
	return s_earlyLockCount;
}

/* sceKernelLockSuspend */
int SceKernelSuspendForDriver_4DF40893(int arg)
{
	int ret;

	if (s_lockSuspendHook != NULL)
		s_lockSuspendHook(arg, s_callbacks,
				  s_callbacks != NULL ? s_callbacks->lockSuspend : NULL);
	if (s_callbacks != NULL) {
		ret = s_callbacks->lockSuspend(arg);
		if (s_lockSuspendResultHook != NULL)
			s_lockSuspendResultHook(s_callbacks, s_callbacks->lockSuspend, ret, 0);
		return ret;
	}
	SceCpuForDriver_1F157DC3(&s_earlyLockCount, 1, 0x7fffffff);
	if (s_lockSuspendResultHook != NULL)
		s_lockSuspendResultHook(NULL, NULL, 0, s_earlyLockCount);
	return 0;
}

/* sceKernelPowerLock */
int SceKernelSuspendForDriver_254525F8(SceUInt32 kind)
{
	if (s_callbacks != NULL)
		return s_callbacks->powerLock(kind);
	return 0;
}

/* sceKernelUnlockSuspend */
int SceKernelSuspendForDriver_2BB92967(int arg)
{
	int ret;

	if (s_callbacks == NULL) {
		SceCpuForDriver_2A71B03C(&s_earlyLockCount);
		if (s_unlockSuspendHook != NULL)
			s_unlockSuspendHook(arg, NULL, 0, s_earlyLockCount);
		return 0;
	}
	ret = s_callbacks->unlockSuspend(arg);
	if (s_unlockSuspendHook != NULL)
		s_unlockSuspendHook(arg, s_callbacks, ret, 0);
	return ret;
}

/* sceKernelPowerUnlock */
int SceKernelSuspendForDriver_230495ED(SceUInt32 kind)
{
	if (s_callbacks != NULL)
		return s_callbacks->powerUnlock(kind);
	return 0;
}

/* in a process's thread: the hook learns the process's address space too */
int SceKernelSuspendForDriver_E0489831(int type)
{
	ThreadSyscallFrame *frame;
	ThreadCB *thread;

	if (s_powerTickHook != NULL) {
		if (!(readTpidruro() & 0x10000)) {
			s_powerTickHook(0, type, SceSysrootForKernel_7385CADE(),
					SceSysrootForKernel_D441DC34(), __builtin_return_address(0));
		} else {
			thread = getCurrentThreadCB();
			if (thread != NULL) {
				frame = thread->userFrame;
				s_powerTickHook(1, type, SceSysrootForKernel_7385CADE(),
						SceSysrootForKernel_D441DC34(), (const void *)frame->lr);
			}
		}
	}
	if (s_callbacks != NULL)
		return s_callbacks->powerTick(type);
	return 0;
}

/* a power tick on behalf of process pid */
int SceKernelSuspendForDriver_1FA2F8F1(SceUID pid, SceUInt32 mask)
{
	void (*hook)(SceUID, SceUInt32, SceUID, SceUID) = s_tickProcessHook;

	if (hook != NULL)
		hook(pid, mask, SceSysrootForKernel_7385CADE(), SceSysrootForKernel_D441DC34());
	if (s_callbacks != NULL)
		return s_callbacks->tickProcess(pid, mask);
	return 0;
}

int SceKernelSuspendForDriver_E677B343(SceUInt32 timerId, SceUInt32 tickFlags, SceUInt64 interval,
				       PowerIdleCallback callback, void *argument)
{
	if (s_callbacks != NULL)
		return s_callbacks->setIdleCallback(timerId, tickFlags, interval, callback, argument);
	return 0;
}

int SceKernelSuspendForDriver_81D9E41C(SceUInt32 timerId)
{
	if (s_callbacks != NULL)
		return s_callbacks->expireIdleTimer(timerId);
	return 0;
}

int SceKernelSuspendForDriver_B4B13615(SceUInt32 timerId, SceUInt32 mask)
{
	if (s_callbacks != NULL)
		return s_callbacks->addIdleTimerMask(timerId, mask);
	return 0;
}

/* standby */
int SceKernelSuspendForDriver_0A6CA124(void)
{
	if (s_callbacks != NULL)
		return s_callbacks->requestStandby();
	return SCE_KERNEL_ERROR_UNSUP;
}

/* suspend */
int SceKernelSuspendForDriver_0106C0F0(void)
{
	if (s_callbacks != NULL)
		return s_callbacks->requestSuspend();
	return SCE_KERNEL_ERROR_UNSUP;
}

/* cold reset */
int SceKernelSuspendForDriver_AEA9440D(void)
{
	if (s_callbacks != NULL)
		return s_callbacks->requestColdReset();
	return SCE_KERNEL_ERROR_UNSUP;
}

int SceKernelSuspendForDriver_6A503956(SceUInt32 argument)
{
	if (s_callbacks != NULL)
		return s_callbacks->requestShutdown(argument);
	return SCE_KERNEL_ERROR_UNSUP;
}

/* a suspend handler in slot idx */
SceUID SceKernelSuspendForDriver_CE7A2207(SceUInt32 idx, int (*handler)(int value, void *args), void *args)
{
	int state;

	if (idx >= SUSPEND_HANDLERS)
		return -1;
	state = SceCpuForDriver_D32ACE9E(&s_handlerLock);
	s_suspendHandlers[idx].handler = handler;
	s_suspendHandlers[idx].args = args;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_handlerLock, state);
	return 0;
}

/* a resume handler in slot idx */
SceUID SceKernelSuspendForDriver_105C5752(SceUInt32 idx, int (*handler)(int value, void *args), void *args)
{
	int state;

	if (idx >= SUSPEND_HANDLERS)
		return -1;
	state = SceCpuForDriver_D32ACE9E(&s_handlerLock);
	s_resumeHandlers[idx].handler = handler;
	s_resumeHandlers[idx].args = args;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_handlerLock, state);
	return 0;
}

/* the suspend handlers, first to last */
int SceKernelSuspendForDriver_D4958E6F(int value)
{
	int i;

	for (i = 0; i < SUSPEND_HANDLERS; i++) {
		if (s_suspendHandlers[i].handler != NULL)
			s_suspendHandlers[i].handler(value, s_suspendHandlers[i].args);
	}
	return 0;
}

/* the resume handlers, last to first */
int SceKernelSuspendForDriver_FE2118BD(int value)
{
	int i;

	for (i = SUSPEND_HANDLERS - 1; i >= 0; i--) {
		if (s_resumeHandlers[i].handler != NULL)
			s_resumeHandlers[i].handler(value, s_resumeHandlers[i].args);
	}
	return 0;
}
