/* CodecEngineWrapper process event dispatch (Sony entry.c). */
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <psp2kern/kernel/modulemgr.h>
#include <sysmem/procevent.h>
#include <sysmem/suspend.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>

extern const SceKernelProcEventHandler codecEngineProcessEvents; /* name unknown */

typedef int (*CodecEngineProcExitHandler)(SceUID pid, ProcEventParam *param,
					int flags); /* name unknown */

extern int ScePervasiveForDriver_D58E3A30(void);
extern int ScePervasiveForDriver_E0711D42(void);
extern int sceCodecEngineDsp0IsProcessSuspended(SceUID pid);

#if FW_VERSION >= 0x04000000
extern CodecEngineProcExitHandler *getProcExitHandlers(void);
extern CodecEngineProcExitHandler *getProcKillHandlers(void);
extern int hwdProcessExit(SceUID pid, SceUInt32 type) __asm__("releaseVeneziaVipProcessStorage");
extern int _sceCodecEngineProcessExitEntry(SceUID pid, SceUInt32 type);
extern int _sceVeneziaProcessExitEntry(SceUID pid, SceUInt32 type);
extern int _sceVeneziaProcessCreateEntryInternal2(SceUID pid, SceUInt32 type);
extern int _sceHwd0ProcessCreateEntry(SceUID pid, SceUInt32 type);
extern int _sceCodecEngineProcessCreateEntry(SceUID pid, SceUInt32 type);
extern int suspendSystem(void); /* name unknown */
extern int resumeSystem(void); /* name unknown */
extern int resumeHwd(void) __asm__("resumeHwd"); /* name unknown */
extern int _sceVeneziaStartSystemSuspend(void);
extern int _sceVeneziaSystemResumePhase0(void);
extern int _sceVeneziaIsSystemSuspended(void);
extern void _sceVeneziaPrintBlockingCalls(SceUID pid);
extern int _sceVeneziaStartEntryPhase2(void);
extern int startHwdPhase2(void) __asm__("initializeVeneziaVipProcesses"); /* name unknown */
extern int _sceCodecEngineProcessResume(SceUID pid);
extern int _sceVeneziaProcessResume(SceUID pid);
extern int _sceHwd0ProcessResume(SceUID pid);
extern int _sceVeneziaStartProcessSuspend(SceUID pid);
extern int _sceHwd0StartProcessSuspend(SceUID pid);
extern int _sceVeneziaProcessSuspendPhase0(SceUID pid);
extern int _sceCodecEngineProcessSuspendPhase0(SceUID pid);
extern int isHwdProcessSuspended(SceUID pid) __asm__("isHwdProcessSuspended"); /* name unknown */
extern int _sceVeneziaProcessCreateEntry(void);
extern int initCodecEngine(void); /* name unknown */
#else
extern CodecEngineProcExitHandler *getProcExitHandlers(void);
extern CodecEngineProcExitHandler *getProcKillHandlers(void);
extern int hwdProcessExit(SceUID pid, SceUInt32 type) __asm__("releaseVeneziaVipProcessStorage");
extern int _sceCodecEngineProcessExitEntry(SceUID pid, SceUInt32 type);
extern int _sceVeneziaProcessExitEntry(SceUID pid, SceUInt32 type);
extern int _sceVeneziaProcessCreateEntryInternal2(SceUID pid, SceUInt32 type);
extern int _sceHwd0ProcessCreateEntry(SceUID pid, SceUInt32 type) __asm__("_sceHwd0ProcessCreateEntry");
extern int _sceCodecEngineProcessCreateEntry(SceUID pid, SceUInt32 type);
extern int suspendSystem(void);
extern int resumeSystem(void);
extern int resumeHwd(void) __asm__("resumeHwd");
extern int _sceVeneziaStartSystemSuspend(void);
extern int _sceVeneziaSystemResumePhase0(void);
extern int _sceVeneziaIsSystemSuspended(void);
extern void _sceVeneziaPrintBlockingCalls(SceUID pid);
extern int _sceVeneziaStartEntryPhase2(void);
extern int startHwdPhase2(void) __asm__("initializeVeneziaVipProcesses");
extern int _sceCodecEngineProcessResume(SceUID pid);
extern int _sceVeneziaProcessResume(SceUID pid);
extern int _sceHwd0ProcessResume(SceUID pid) __asm__("_sceHwd0ProcessResume");
extern int _sceVeneziaStartProcessSuspend(SceUID pid);
extern int _sceHwd0StartProcessSuspend(SceUID pid) __asm__("_sceHwd0StartProcessSuspend");
extern int _sceVeneziaProcessSuspendPhase0(SceUID pid);
extern int _sceCodecEngineProcessSuspendPhase0(SceUID pid);
extern int isHwdProcessSuspended(SceUID pid) __asm__("isHwdProcessSuspended");
extern int _sceVeneziaProcessCreateEntry(void);
extern int initCodecEngine(void);
#endif

int _veneziaWrapperModuleProcessExit(SceUID pid, ProcEventParam *param, int flags)
{
	CodecEngineProcExitHandler *handlers = getProcExitHandlers();
	int status = 0, codecStatus, hwdStatus, veneziaStatus;
	unsigned int i;
	SceUInt32 type;

#if FW_DEBUG
	SceDebugForDriver_35A35322(handlers != NULL, SCE_DBGINFO(334), __builtin_return_address(0));
#endif
	for (i = 0; i != 4; i++) {
		if (handlers[i] != NULL) {
			int ret = handlers[i](pid, param, flags);

			if (ret < 0)
				status = ret;
		}
	}
	type = param->type;
	codecStatus = _sceCodecEngineProcessExitEntry(pid, type);
	hwdStatus = hwdProcessExit(pid, type);
	if (hwdStatus >= 0)
		hwdStatus = codecStatus < 0 ? codecStatus : 0;
	veneziaStatus = _sceVeneziaProcessExitEntry(pid, type);
	if (veneziaStatus < 0)
		return veneziaStatus;
	return hwdStatus != 0 ? hwdStatus : status;
}

/* name unknown: the registered process-create callback. */
int codecEngineProcessCreateHandler(SceUID pid, ProcEventCreateParam *param, int flags)
{
	SceUInt32 type = param->type;
	int ret;

	ret = _sceVeneziaProcessCreateEntryInternal2(pid, type);
	if (ret < 0)
		return ret;
	ret = _sceHwd0ProcessCreateEntry(pid, type);
	if (ret < 0)
		return ret;
	return _sceCodecEngineProcessCreateEntry(pid, type);
}

/* name unknown: system suspend/resume event callback. */
unsigned int codecEngineSystemEventHandler(int phase, unsigned int event)
{
	int ret;

	switch (phase) {
	case 0:
		switch (event) {
		case 0x200:
			return _sceVeneziaStartSystemSuspend();
		case 0x401:
			_sceVeneziaPrintBlockingCalls(0);
			return 0;
		case 0x402:
			return _sceVeneziaIsSystemSuspended() ? 0 : -1;
		case 0x400e:
			ret = suspendSystem();
			if (ret < 0)
				return ret;
			return ScePervasiveForDriver_E0711D42();
		}
		break;
	case 1:
		switch (event) {
		case 0x1000e:
			ret = ScePervasiveForDriver_D58E3A30();
			if (ret < 0)
				return ret;
			ret = _sceVeneziaSystemResumePhase0();
			if (ret < 0)
				return ret;
			return resumeHwd();
		case 0x100000:
			return resumeSystem();
		}
		break;
	}
	return 0;
}

/* name unknown: boot thread entry. */
int codecEngineStartPhase2(void)
{
	int ret = ScePervasiveForDriver_D58E3A30();

	if (ret < 0)
		return ret;
	ret = _sceVeneziaStartEntryPhase2();
	if (ret < 0)
		return ret;
	return startHwdPhase2();
}

/* name unknown: registered process-start callback. */
int codecEngineProcessResumeHandler(SceUID pid, unsigned int event)
{
	int ret;

	if (event != 0x10000)
		return 0;
	ret = _sceCodecEngineProcessResume(pid);
	if (ret < 0)
		return ret;
	ret = _sceVeneziaProcessResume(pid);
	if (ret < 0)
		return ret;
	return _sceHwd0ProcessResume(pid);
}

/* name unknown: registered process-stop callback. */
int codecEngineProcessSuspendHandler(SceUID pid, unsigned int event)
{
	int ret;

	switch (event) {
	case 0x10f:
		ret = _sceVeneziaStartProcessSuspend(pid);
		if (ret < 0)
			return ret;
		return _sceHwd0StartProcessSuspend(pid);
	case 0x201:
		_sceVeneziaPrintBlockingCalls(pid);
		return 0;
	case 0x202:
		if (!sceCodecEngineDsp0IsProcessSuspended(pid))
			return -1;
		return isHwdProcessSuspended(pid) ? 0 : -1;
	case 0x800:
		ret = _sceVeneziaProcessSuspendPhase0(pid);
		if (ret < 0)
			return ret;
		return _sceCodecEngineProcessSuspendPhase0(pid);
	}
	return 0;
}

int _veneziaWrapperModuleProcessKill(SceUID pid, ProcEventParam *param, int flags)
{
	CodecEngineProcExitHandler *handlers = getProcKillHandlers();
	int status = 0, codecStatus, hwdStatus, veneziaStatus;
	unsigned int i;
	SceUInt32 type;

#if FW_DEBUG
	SceDebugForDriver_35A35322(handlers != NULL, SCE_DBGINFO(366), __builtin_return_address(0));
#endif
	for (i = 0; i != 4; i++) {
		if (handlers[i] != NULL) {
			int ret = handlers[i](pid, param, flags);

			if (ret < 0)
				status = ret;
		}
	}
	type = param->type;
	codecStatus = _sceCodecEngineProcessExitEntry(pid, type);
	hwdStatus = hwdProcessExit(pid, type);
	if (hwdStatus >= 0)
		hwdStatus = codecStatus < 0 ? codecStatus : 0;
	if (hwdStatus)
		status = hwdStatus;
	veneziaStatus = _sceVeneziaProcessExitEntry(pid, type);
	if (veneziaStatus < 0)
		return veneziaStatus;
	return status;
}


const SceKernelProcEventHandler codecEngineProcessEvents = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = (int (*)(SceUID, ProcEventCreateParam *, int))codecEngineProcessCreateHandler,
	.exit = (int (*)(SceUID, ProcEventParam *, int))_veneziaWrapperModuleProcessExit,
	.kill = (int (*)(SceUID, ProcEventParam *, int))_veneziaWrapperModuleProcessKill,
	.suspend = (int (*)(SceUID, int, ProcEventParam *, int))codecEngineProcessSuspendHandler,
	.resume = (int (*)(SceUID, int, ProcEventParam *, int))codecEngineProcessResumeHandler,
};
