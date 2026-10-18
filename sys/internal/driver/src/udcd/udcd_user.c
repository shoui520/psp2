/*
 * SceUdcd: the state callback of bus 2, the suspend/resume exports, and the
 * user-mode syscalls (Sony's file name unknown). A user caller must be a
 * system program, the QA mini settings, or allowed USB serial.
 */
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <threadmgr/threadmgr.h>
#include <udcd/udcd_imports.h>
#include "udcd_internal.h"
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <udcd/udcd.h>


#define UDCD_ERROR_BUSY			SCE_UDCD_ERROR_WAIT_TIMEOUT

#define UDCD_THREADMGR_CLASS_CALLBACK	8

/* the device state as user mode sees it */
typedef struct UdcdUserDeviceState {
	unsigned int controller;		/* CONTROLLER_STOPPED/STARTED */
	unsigned int activation;		/* DEACTIVATED, ACTIVATED, IS_CHARGING */
	unsigned int cable;			/* CABLE_DISCONNECTED/CONNECTED */
	unsigned int connection;		/* CONNECTION_*, 1 while suspended */
	unsigned int charging;			/* USE_USB_CHARGING */
	unsigned int reserved;			/* not set */
} UdcdUserDeviceState;

/* Original name unknown. Register the bus 2 state callback (one thread at a time). */
int udcdRegisterCallback(SceUID callback, SceUID process, unsigned int mask)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;
	SceUID thread;

	thread = sceKernelGetThreadId();
	if (thread < 0)
		return UDCD_ERROR_NO_THREAD;
	if (sceKernelGetThreadmgrUIDClass(callback) != UDCD_THREADMGR_CLASS_CALLBACK)
		return UDCD_ERROR_INVALID_ARGUMENT;
	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->callbackThread != -1 && thread != state->callbackThread) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		return UDCD_ERROR_BUSY;
	}
	state->callback = callback;
	state->callbackProcess = process;
	state->callbackThread = thread;
	state->callbackMask = mask;
	sceKernelRegisterCallbackToEventForDriver(state->callbackEvent, callback, callback);
	if (state->activationState == SCE_UDCD_STATUS_ACTIVATED) {
		sceKernelSetEventWithNotifyCallback(state->callbackEvent, 0x1000000, 0, 0x200);
		state->notifiedMask = 0x200;
	} else {
		state->notifiedMask = 0x100;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

/* Original name unknown. Unregister it: by its process or by its thread. */
int udcdUnregisterCallback(SceUID process, SceUID thread)
{
	struct udcdBusState *state = &udcdState.buses[2];
	SceKernelIntrStatus status;

	status = SceCpuForDriver_D32ACE9E(&state->lock);
	if (state->callbackProcess != process && state->callbackThread != thread) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
		return UDCD_ERROR_INVALID_ARGUMENT;
	}
	state->callback = 0;
	state->callbackProcess = -1;
	state->callbackThread = -1;
	state->callbackMask = 0;
	sceKernelUnregisterCallbackFromEvent(state->callbackEvent, 0);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&state->lock, status);
	return 0;
}

int SceUdcdForDriver_D01CFD22(void)
{
	udcdSuspendAll();
	return 0;
}

int SceUdcdForDriver_9E7F7EFE(void)
{
	udcdSuspendCharging();
	return 0;
}

int SceUdcdForDriver_1A5A3851(void)
{
	udcdResumeAll();
	return 0;
}

/* the callers user mode may serve */
static inline int udcdUserAllowed(void)
{
	return sceSblACMgrIsSystemProgram(0) || SceSblACMgrForDriver_D0E11C89(0) ||
	       SceSblACMgrForDriver_062CAEB2(0);
}

int sceUdcdRegisterCallback(SceUID callback, unsigned int mask)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID process;
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	process = sceKernelGetProcessId();
	if (process < 0) {
		sceSyscallLeave(t);
		return UDCD_ERROR_NO_THREAD;
	}
	result = udcdRegisterCallback(SceSysmemForDriver_45D22597(process, callback), process, mask);
	sceSyscallLeave(t);
	return result;
}

int sceUdcdUnregisterCallback(SceUID callback)
{
	SceUInt32 t = sceSyscallEnter();
	SceUID process, thread;
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	process = sceKernelGetProcessId();
	thread = sceKernelGetThreadId();
	if ((process | thread) < 0) {
		sceSyscallLeave(t);
		return UDCD_ERROR_NO_THREAD;
	}
	result = udcdUnregisterCallback(0, thread);
	sceSyscallLeave(t);
	return result;
}

int sceUdcdWaitState(const SceUdcdWaitParam *parameter, unsigned int timeout)
{
	SceUInt32 t = sceSyscallEnter();
	SceUdcdWaitParam kparameter;
	char name[0x40];
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (SceThreadmgrForDriver_20C228E4() != 0) {
		sceSyscallLeave(t);
		return UDCD_ERROR_INTERRUPTED;
	}
	result = SceSysmemForDriver_BC996A7A(&kparameter, parameter, sizeof(kparameter));
	if (result < 0)
		goto out;
	if ((unsigned int)kparameter.connection >> 3 != 0)
		goto invalid;
	if (kparameter.driverName != NULL) {
		result = SceSysmemForDriver_DB3EC244(name, kparameter.driverName, sizeof(name));
		if (result < 0)
			goto out;
		if (result == sizeof(name))
			goto invalid;
		kparameter.driverName = name;
	}
	result = SceUdcdForDriver_D03017C0(&kparameter, timeout);
out:
	sceSyscallLeave(t);
	return result;
invalid:
	sceSyscallLeave(t);
	return UDCD_ERROR_INVALID_ARGUMENT;
}

int SceUdcd_F1A3690B(void)
{
	SceUInt32 t = sceSyscallEnter();
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	result = SceUdcdForDriver_EBB1E86B();
	sceSyscallLeave(t);
	return result;
}

int sceUdcdGetDeviceState(UdcdUserDeviceState *state)
{
	SceUInt32 t = sceSyscallEnter();
	UdcdUserDeviceState kstate;
	struct udcdBusState *bus = &udcdState.buses[2];
	unsigned int value;
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (state == NULL) {
		result = UDCD_ERROR_INVALID_ARGUMENT;
	} else {
		value = SceUdcdForDriver_E054B5E4();
		kstate.controller = value & 0x3000;
		kstate.activation = value & 0x700;
		kstate.cable = value & 0x30;
		kstate.charging = (bus->busPowered == 1 && bus->chargingEnabled == 1) ? 0x800 : 0;
		kstate.connection = (value & 8) ? 1 : value & 7;
		result = SceSysmemForDriver_6D88EF8A(state, &kstate, sizeof(kstate));
		if (result > 0)
			result = 0;
	}
	sceSyscallLeave(t);
	return result;
}

int sceUdcdGetDeviceInfo(SceUdcdDeviceInfo *info)
{
	SceUInt32 t = sceSyscallEnter();
	SceUdcdDeviceInfo kinfo;
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (info == NULL) {
		result = UDCD_ERROR_INVALID_ARGUMENT;
	} else {
		result = SceUdcdForDriver_FBEA3703(&kinfo);
		if (result >= 0) {
			result = SceSysmemForDriver_6D88EF8A(info, &kinfo, sizeof(kinfo));
			if (result > 0)
				result = 0;
		}
	}
	sceSyscallLeave(t);
	return result;
}

int SceUdcd_FCD876FB(unsigned int enabled)
{
	SceUInt32 t = sceSyscallEnter();
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	result = SceUdcdForDriver_2D91F8C3(enabled);
	sceSyscallLeave(t);
	return result;
}

int sceUdcdGetDrvState(const char *driverName)
{
	SceUInt32 t = sceSyscallEnter();
	char name[0x40];
	int result;

	if (!udcdUserAllowed()) {
		sceSyscallLeave(t);
		return SCE_ERROR_ERRNO_ENOSYS;
	}
	if (driverName == NULL)
		goto invalid;
	result = SceSysmemForDriver_DB3EC244(name, driverName, sizeof(name));
	if (result >= 0) {
		if (result == sizeof(name))
			goto invalid;
		result = SceUdcdForDriver_C0CA5DDB(name);
	}
	sceSyscallLeave(t);
	return result;
invalid:
	sceSyscallLeave(t);
	return UDCD_ERROR_INVALID_ARGUMENT;
}
