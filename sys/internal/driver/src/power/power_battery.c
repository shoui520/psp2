/*
 * ScePower: battery and charging state.
 */

#include "power_private.h"
#include <power/power.h>

/* exported, and inlined in this unit as in the original */

static inline __attribute__((always_inline)) void notifyBatteryWorker(void);

static inline __attribute__((always_inline)) void powerInvalidateBatteryCache(void)
{
	if (!s_powerBatteryBase.state.status)
		return;
	s_powerBatteryBase.state.status = 0;
	s_powerBatteryBase.state.remainCapacity = UINT32_MAX;
	s_powerBatteryBase.state.lifePercent = UINT32_MAX;
	s_powerBatteryBase.state.calculatedLifePercent = UINT32_MAX;
	s_powerBatteryBase.state.fullCapacity = UINT32_MAX;
	s_powerBatteryBase.state.lifeTime = -1;
	s_powerBatteryBase.state.temperature = UINT32_MAX;
	s_powerBatteryBase.state.electricCurrent = UINT32_MAX;
	s_powerBatteryBase.state.voltage = UINT32_MAX;
	powerNotifyCallback(SCE_POWER_CALLBACKARG_BATTERYEXIST, 0, 0);
	notifyBatteryWorker();
}

int powerBatteryWorker(SceSize argumentSize, void *argument)
{
	uint32_t eventMask = 0x40003700;
	uint32_t timeout = 0;
	uint32_t events;
	uint32_t value;
	uint32_t other;
	uint32_t source;
	uint32_t previous;
	int result;

	(void)argumentSize;
	(void)argument;
	s_powerBatteryBase.workerPaused = 0;
	s_powerBatteryBase.initialScan = 1;
	s_powerBatteryBase.scanStep = 0;
	for (;;) {
		events = 0;
		if (timeout) {
			result = sceKernelWaitEventFlag(s_powerBatteryBase.eventFlag,
					eventMask | 0x90000000, SCE_KERNEL_EVF_WAITMODE_OR, &events,
					timeout == UINT32_MAX ? NULL : &timeout);
			if ((events & 0x10000000) && !s_powerBatteryBase.initialScan) {
				s_powerBatteryBase.initialScan = 1;
				s_powerBatteryBase.scanStep = 0;
			}
			if (result < 0 && result != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
				return 0;
		} else {
			sceKernelPollEventFlag(s_powerBatteryBase.eventFlag,
					eventMask | 0x80000000, SCE_KERNEL_EVF_WAITMODE_OR,
					&events);
		}
		if (events & 0x80000000)
			return 0;
		if (events & 0x200) {
			SceSysconForDriver_596B17B7(1);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x200U);
		}
		if (events & 0x40000000) {
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag,
						       ~0x40000000U);
			timeout = UINT32_MAX;
			s_powerBatteryBase.workerPaused = 1;
			eventMask = 0x60000100;
			continue;
		}
		if (events & 0x20000000) {
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag,
						       ~0x20000000U);
			eventMask = 0x40000700;
			timeout = 0;
			s_powerBatteryBase.workerPaused = 0;
			s_powerBatteryBase.scanStep = 0;
			s_powerBatteryBase.state.status = 0;
			continue;
		}
		if (events & 0x100) {
			SceSysconForDriver_596B17B7(0);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x100U);
		}
		if (s_powerBatteryBase.workerPaused)
			continue;
		source = SceSysconForDriver_9DA2A5AB();
		if (events & 0x800) {
			SceSysconForDriver_2A4B0437(1, 1);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x800U);
		} else if (events & 0x400) {
			SceSysconForDriver_2A4B0437(1, 0);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x400U);
		} else if (events & 0x1000) {
			SceSysconForDriver_59DC5938(s_powerBatteryBase.state.usbStatus);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x1000U);
		} else if (events & 0x2000) {
			SceSysconForDriver_9CA6EB70(s_powerBatteryBase.state.sysconLedOff);
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x2000U);
		}
		timeout = 5000000;
		if (!s_powerBatteryBase.state.status) {
			if (s_powerBatteryBase.scanStep - 2 < 12)
				s_powerBatteryBase.scanStep = 0;
		} else if (s_powerBatteryBase.state.status == 1) {
			timeout = 20000;
		} else if (s_powerBatteryBase.state.status == 2) {
			s_powerBatteryBase.state.calculatedLifePercent = __aeabi_idiv(
				(int32_t)(s_powerBatteryBase.state.remainCapacity * 100U),
				(int32_t)s_powerBatteryBase.state.fullCapacity);
		}
		if (s_powerBatteryBase.initialScan)
			timeout = 20000;
		s_powerBatteryBase.powerSource = source;
		switch (s_powerBatteryBase.scanStep) {
		case 0:
			s_powerBatteryBase.scanStep = 1;
			timeout = 0;
			break;
		case 1:
			result = SceSysconForDriver_A2FE9BF9(&value);
			if (result < 0)
				break;
			if ((SceSysconForDriver_CBD6D8BC() & 0xff0000) == 0x310000)
				value &= ~0x40U;
			s_powerBatteryBase.hardwareStatus = value;
			if (!(value & 0x40)) {
				powerInvalidateBatteryCache();
				s_powerBatteryBase.initialScan = 0;
				s_powerBatteryBase.scanStep = 0;
			} else {
				if (!s_powerBatteryBase.state.status) {
					timeout = 20000;
					s_powerBatteryBase.state.status = 1;
				}
				s_powerBatteryBase.scanStep = 3;
			}
			break;
		case 3:
			result = SceSysconForDriver_C562AF3A(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.remainCapacity = value;
			s_powerBatteryBase.scanStep =
				(uint32_t)SceSysconForDriver_FF86F4C5() < 0x01040105 ? 5 : 4;
			break;
		case 4:
			result = SceSysconForDriver_26F9D729(&value, &other);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.remainLevel = other;
			s_powerBatteryBase.state.remainMaxLevel = value;
			s_powerBatteryBase.scanStep = 5;
			break;
		case 5:
			result = SceSysconForDriver_AD0A8275(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.lifePercent = value;
			s_powerBatteryBase.scanStep = 6;
			break;
		case 6:
			if ((int32_t)s_powerBatteryBase.state.fullCapacity >= 0) {
				s_powerBatteryBase.scanStep = 7;
				timeout = 0;
				break;
			}
			result = SceSysconForDriver_F93CF833(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.fullCapacity = value;
			s_powerBatteryBase.scanStep = 7;
			break;
		case 7:
			if (s_powerBatteryBase.state.stateOfHealth) {
				s_powerBatteryBase.scanStep = 8;
				timeout = 0;
				break;
			}
			result = SceSysconForDriver_91D3B7A3(&other, &value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.stateOfHealth = other;
			s_powerBatteryBase.state.healthStatus = value;
			s_powerBatteryBase.scanStep = 8;
			break;
		case 8:
			if (s_powerBatteryBase.state.cycleCount) {
				s_powerBatteryBase.scanStep = 9;
				timeout = 0;
				break;
			}
			result = SceSysconForDriver_CD73079D(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.cycleCount = value;
			s_powerBatteryBase.scanStep = 9;
			break;
		case 9:
			if (SceSysconForDriver_9DA2A5AB()) {
				s_powerBatteryBase.state.lifeTime = -1;
				s_powerBatteryBase.scanStep = 10;
				timeout = 0;
				break;
			}
			result = SceSysconForDriver_B841C141(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.lifeTime = value;
			s_powerBatteryBase.scanStep = 10;
			break;
		case 10:
			result = SceSysconForDriver_9070F139(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.temperature = value;
			previous = s_powerBatteryBase.state.previousTemperature;
			if (s_powerBatteryBase.state.temperatureCallback) {
				previous = value;
				if (((value - s_powerBatteryBase.state.temperatureThreshold) ^
				     (s_powerBatteryBase.state.previousTemperature -
				      s_powerBatteryBase.state.temperatureThreshold)) & 0x80000000)
					s_powerBatteryBase.state.temperatureCallback(value,
							s_powerBatteryBase.state.temperatureArgument);
			}
			s_powerBatteryBase.state.previousTemperature = previous;
			s_powerBatteryBase.scanStep = 11;
			break;
		case 11:
			result = SceSysconForDriver_0826BA07((int32_t *)&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.electricCurrent = value;
			s_powerBatteryBase.scanStep = 12;
			break;
		case 12:
			result = SceSysconForDriver_03F11220(&value);
			if (result < 0)
				goto invalidate;
			s_powerBatteryBase.state.voltage = value;
			if (s_powerBatteryBase.state.status == 1)
				s_powerBatteryBase.state.status = 2;
			s_powerBatteryBase.initialScan = 0;
			s_powerBatteryBase.scanStep = 0;
			break;
		default:
			break;
		}
		continue;
invalidate:
		powerInvalidateBatteryCache();
	}
}

int powerInitializeBatteryControl(void)
{
	memset((void *)&s_powerBatteryBase, 0, sizeof(s_powerBatteryBase));
	s_powerBatteryBase.usbChargingEnabled = UINT32_MAX;
	s_powerBatteryBase.powerSource = -1;
	s_powerBatteryBase.state.fullCapacity = UINT32_MAX;
	s_powerBatteryBase.chargingAlarm = -1;
	s_powerBatteryBase.eventFlag = sceKernelCreateEventFlag("ScePowerBattery",
							       0, 0, NULL);
	s_powerBatteryBase.worker = sceKernelCreateThread("ScePowerBattery",
					powerBatteryWorker, 0x40, 0x2000, 0, 0x80000, NULL);
	sceKernelStartThread(s_powerBatteryBase.worker, 0, NULL);
	return 0;
}

int powerSuspendBatteryControl(void)
{
	SceKernelIntrStatus intrState;
	uint32_t events = 0x40000000;

	intrState = SceCpuForDriver_821FC0EE();
	if (s_powerBatteryBase.chargingAlarm > 0) {
		events |= 0x200;
		sceKernelCancelAlarm(s_powerBatteryBase.chargingAlarm);
		s_powerBatteryBase.chargingAlarm = -1;
	}
	if (s_powerBatteryBase.chargingInhibitCount)
		events |= 0x200;
	sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x20000000U);
	sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, events);
	sceKernelCpuResumeIntr(intrState);
	return 0;
}

/* Pulse the battery worker while preserving both syscall and IRQ context. */
static inline __attribute__((always_inline)) void notifyBatteryWorker(void)
{
	uint32_t context = sceSyscallEnter();
	SceKernelIntrStatus intrState = SceCpuForDriver_821FC0EE();

	sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x10000000);
	sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x10000000U);
	sceKernelCpuResumeIntr(intrState);
	sceSyscallLeave(context);
}

int scePowerBatteryUpdateInfo(void)
{
	notifyBatteryWorker();
	return 0;
}

int ScePowerForDriver_166922EC(void)
{
	SceKernelIntrStatus intrState;

	intrState = SceCpuForDriver_821FC0EE();
	if (!s_powerBatteryBase.chargingInhibitCount) {
		if (s_powerBatteryBase.chargingAlarm > 0) {
			sceKernelCancelAlarm(s_powerBatteryBase.chargingAlarm);
			s_powerBatteryBase.chargingAlarm = -1;
		}
		sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x200U);
		sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x100);
	}
	s_powerBatteryBase.chargingInhibitCount++;
	notifyBatteryWorker();
	sceKernelCpuResumeIntr(intrState);
	return 0;
}

int ScePowerForDriver_DD3D4DAC(void)
{
	SceKernelIntrStatus intrState;

	intrState = SceCpuForDriver_821FC0EE();
	if ((int32_t)s_powerBatteryBase.chargingInhibitCount > 0) {
		s_powerBatteryBase.chargingInhibitCount--;
		if (!s_powerBatteryBase.chargingInhibitCount) {
			sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x100U);
			s_powerBatteryBase.chargingAlarm = SceThreadmgrForDriver_C58DF384(
					"ScePowerAlarm0", 5000000,
					powerPermitChargeTimer, NULL);
		}
	}
	notifyBatteryWorker();
	sceKernelCpuResumeIntr(intrState);
	return 0;
}

int ScePowerForDriver_E4E3C316(uint32_t status)
{
	volatile struct PowerBatteryState *state = &s_powerBatteryBase.state;
	SceKernelIntrStatus intrState;
	int result;

	if (s_powerBatteryBase.workerPaused)
		return 0;
	intrState = SceCpuForDriver_821FC0EE();
	state->usbStatus = status;
	if (state->usbStatusPending) {
		result = -1;
		sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x1000);
	} else {
		state->usbStatusPacket.tx[3] = status;
		state->usbStatusPacket.tx[0] = 0x9a;
		state->usbStatusPacket.tx[1] = 8;
		state->usbStatusPacket.tx[2] = 3;
		result = SceSysconForDriver_C2224E82((SceSysconPacket *)&state->usbStatusPacket,
				1, powerChargingCommandComplete,
				(void *)&state->usbStatusPending);
		if (result >= 0)
			state->usbStatusPending = 1;
		else
			sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x1000);
	}
	sceKernelCpuResumeIntr(intrState);
	return result;
}

uint32_t ScePowerForDriver_733F973B(void)
{
	SceKernelIntrStatus intrState;
	uint32_t previous;

	if (s_powerBatteryBase.workerPaused)
		return 0;
	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(
					(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock);
	previous = s_powerBatteryBase.usbChargingEnabled;
	if (previous != 1) {
		s_powerBatteryBase.usbChargingEnabled = 1;
		sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x800);
		sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x400U);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(
				(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock, intrState);
	return previous;
}

int ScePowerForDriver_90285886(void)
{
	SceKernelIntrStatus intrState;

	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(
					(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock);
	if (s_powerBatteryBase.usbChargingEnabled) {
		s_powerBatteryBase.usbChargingEnabled = 0;
		sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x400);
		sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, ~0x800U);
	}
	notifyBatteryWorker();
	sceKernelSpinlockLowUnlockCpuResumeIntr(
				(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock, intrState);
	return 0;
}

int ScePowerForDriver_ACC857A4(void)
{
	volatile struct PowerBatteryState *state = &s_powerBatteryBase.state;
	SceKernelIntrStatus intrState;
	int previous, result;

	intrState = sceKernelSpinlockLowLockCpuSuspendIntr(
				(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock);
	if (s_powerBatteryBase.usbChargingEnabled) {
		previous = SceCpuForDriver_CDA96E81(
				(SceInt32 *)&state->usbStopPending, 0, 1);
		if (!previous) {
			state->usbStopPacket.tx[2] = 1;
			state->usbStopPacket.tx[0] = 4;
			state->usbStopPacket.tx[1] = 8;
			result = SceSysconForDriver_C2224E82(
					(SceSysconPacket *)&state->usbStopPacket,
					1, powerChargingCommandComplete,
					(void *)&state->usbStopPending);
			if (result >= 0)
				state->usbStopPending = 0;
		}
	}
	notifyBatteryWorker();
	sceKernelSpinlockLowUnlockCpuResumeIntr(
				(SceKernelSpinlock *)&s_powerBatteryBase.chargingLock, intrState);
	return 0;
}

/* Sony names for these cached battery control/status exports are unresolved. */
int ScePowerForDriver_38415146(uint32_t value)
{
	uint32_t busy = s_powerBatteryBase.workerPaused;

	s_powerBatteryBase.state.sysconLedOff = value;
	if (!busy)
		sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, 0x2000);
	return 0;
}

int ScePowerForDriver_8C0D2051(uint32_t *out)
{
	uint32_t status = s_powerBatteryBase.hardwareStatus;
	uint32_t fakeEnabled = s_powerBatteryBase.state.fakeEnabled;

	*out = fakeEnabled ? 0 : status;
	return 0;
}

int scePowerIsPowerOnline(void)
{
	uint32_t context = sceSyscallEnter();
	int online;

	/* The native fake-battery path leaves the shifted context in place. */
	if (s_powerBatteryBase.state.fakeEnabled)
		return 0;
	online = SceSysconForDriver_9DA2A5AB();
	sceSyscallLeave(context);
	return online;
}

int scePowerIsBatteryExist(void)
{
	if (s_powerBatteryBase.state.fakeEnabled)
		return 1;
	return s_powerBatteryBase.state.status != 0;
}

int scePowerIsBatteryCharging(void)
{
	uint32_t context, innerContext, status;
	int result;

	if (s_powerBatteryBase.state.fakeEnabled)
		return 0;
	context = sceSyscallEnter();
	result = 0;
	if (s_powerBatteryBase.state.fakeEnabled)
		goto restoreContext;
	innerContext = sceSyscallEnter();
	status = s_powerBatteryBase.state.status;
	if (status == 0) {
		result = SCE_POWER_ERROR_NO_BATTERY;
	} else if (status == 1) {
		result = SCE_POWER_ERROR_DETECTING;
	} else if (!s_powerBatteryBase.chargingInhibitCount) {
		if (SceSysconForDriver_9DA2A5AB()) {
			result = (s_powerBatteryBase.hardwareStatus >> 1) & 1;
		} else if (s_powerBatteryBase.usbChargingEnabled == 1) {
			/* The native path still reads this volatile word before
			 * returning zero, regardless of its bits.
			 */
			(void)s_powerBatteryBase.hardwareStatus;
		}
	}
	sceSyscallLeave(innerContext);
restoreContext:
	sceSyscallLeave(context);
	return result;
}

int scePowerGetBatteryChargingStatus(void)
{
	uint32_t context, status;
	int result;

	if (s_powerBatteryBase.state.fakeEnabled)
		return 0;
	context = sceSyscallEnter();
	status = s_powerBatteryBase.state.status;
	if (status == 0) {
		result = SCE_POWER_ERROR_NO_BATTERY;
	} else if (status == 1) {
		result = SCE_POWER_ERROR_DETECTING;
	} else if (s_powerBatteryBase.chargingInhibitCount) {
		result = 2;
	} else {
		result = SceSysconForDriver_9DA2A5AB();
		if (result)
			result = (s_powerBatteryBase.hardwareStatus >> 1) & 1;
		else if (s_powerBatteryBase.usbChargingEnabled == 1)
			result = (s_powerBatteryBase.hardwareStatus & 0x18) ? 3 : 0;
	}
	sceSyscallLeave(context);
	return result;
}

/* Caller already holds the shifted syscall context. */
static inline __attribute__((always_inline)) int powerGetBatteryLowState(void)
{
	uint32_t duration;
	uint64_t now, started;

	if (s_powerBatteryBase.state.fakeEnabled) {
		now = sceKernelGetSystemTimeWide();
		started = s_powerBatteryBase.state.fakeStartTime;
		duration = s_powerBatteryBase.state.fakeDuration;
		if (now - started >= duration)
			return 1;
	}
	if (s_powerBatteryBase.state.status == 2)
		return SceSysconForDriver_27758A64();
	return 0;
}

int scePowerIsLowBattery(void)
{
	uint32_t context = sceSyscallEnter();
	int result = powerGetBatteryLowState();

	sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_0D56C601(void)
{
	uint32_t context = sceSyscallEnter();
	int result = 0;

	if (s_powerBatteryBase.state.fakeEnabled &&
	    sceKernelGetSystemTimeWide() - s_powerBatteryBase.state.fakeStartTime >=
	    s_powerBatteryBase.state.fakeDuration)
		result = 1;
	else if (s_powerBatteryBase.state.status == 2)
		result = SceSysconForDriver_ACEE1C70();
	sceSyscallLeave(context);
	return result;
}

int ScePowerForDriver_627A89C6(void)
{
	uint32_t context = sceSyscallEnter();
	int result = 0;

	if (s_powerBatteryBase.state.fakeEnabled &&
	    sceKernelGetSystemTimeWide() - s_powerBatteryBase.state.fakeStartTime >=
	    s_powerBatteryBase.state.fakeDuration)
		result = 1;
	else if (s_powerBatteryBase.state.status == 2)
		result = SceSysconForDriver_4A184B7C();
	sceSyscallLeave(context);
	return result;
}

int scePowerIsLowBatteryInhibitUpdateReboot(void)
{
	uint32_t context = sceSyscallEnter();
	int result = SceSysconForDriver_1A0C140F();

	sceSyscallLeave(context);
	return result;
}

int scePowerIsLowBatteryInhibitUpdateDownload(void)
{
	uint32_t context = sceSyscallEnter();
	int result = SceSysconForDriver_1E3130EE();

	sceSyscallLeave(context);
	return result;
}

int scePowerIsSuspendRequired(void)
{
	uint32_t context = sceSyscallEnter();
	uint32_t innerContext;
	int result = 0;

	if (s_powerBatteryBase.state.fakeEnabled &&
	    sceKernelGetSystemTimeWide() - s_powerBatteryBase.state.fakeStartTime >=
	    (uint32_t)(s_powerBatteryBase.state.fakeDuration + 60000000)) {
		result = 1;
	} else if (!SceSysconForDriver_D7F5A797() && !SceSysconForDriver_9DA2A5AB() &&
		   s_powerBatteryBase.state.fakeEnabled) {
		innerContext = sceSyscallEnter();
		(void)sceKernelGetSystemTimeWide();
		sceSyscallLeave(innerContext);
	}
	sceSyscallLeave(context);
	return result;
}

/* Explicit EABI calls preserve the native zero-denominator helper behavior.
 * C integer division would leave that case undefined.
 */
int scePowerGetBatteryRemainCapacity(void)
{
	uint32_t context, duration, status;
	uint64_t now, elapsed, started;
	uint32_t result;

	if (!s_powerBatteryBase.state.fakeEnabled) {
		status = s_powerBatteryBase.state.status;
		if (status == 0)
			return SCE_POWER_ERROR_NO_BATTERY;
		if (status == 1)
			return SCE_POWER_ERROR_DETECTING;
		return s_powerBatteryBase.state.remainCapacity;
	}
	context = sceSyscallEnter();
	now = sceKernelGetSystemTimeWide();
	duration = s_powerBatteryBase.state.fakeDuration;
	started = s_powerBatteryBase.state.fakeStartTime;
	if (!duration) {
		result = s_powerBatteryBase.state.fakeRemainCapacity;
	} else {
		elapsed = now - started;
		result = 0;
		if (elapsed <= duration)
			result = __aeabi_uldivmod((duration - elapsed) *
				s_powerBatteryBase.state.fakeRemainCapacity, duration);
	}
	sceSyscallLeave(context);
	return result;
}

int powerResumeBatteryControl(const struct PowerResumeInformation *information)
{
	SceKernelIntrStatus intrState;
	uint32_t events;

	intrState = SceCpuForDriver_821FC0EE();
	s_powerBatteryBase.hardwareStatus = information->powerInfo;
	s_powerBatteryBase.state.fakeStatusSize = 0;
	s_powerBatteryBase.state.fakeRemainCapacity = 0;
	s_powerBatteryBase.state.fakeFullCapacity = 0;
	s_powerBatteryBase.state.fakeDuration = 0;
	s_powerBatteryBase.state.fakeEnabled = 0;
	s_powerBatteryBase.workerPaused = 0;
	s_powerBatteryBase.state.status = 0;
	s_powerBatteryBase.powerSource = -1;
	events = s_powerBatteryBase.chargingInhibitCount ? 0x20000100 : 0x20000000;
	sceKernelClearEventFlag(s_powerBatteryBase.eventFlag, 0xffffc5ff);
	sceKernelSetEventFlag(s_powerBatteryBase.eventFlag, events);
	notifyBatteryWorker();
	sceKernelCpuResumeIntr(intrState);
	return 0;
}

int scePowerGetBatteryFullCapacity(void)
{
	uint32_t status;

	if (s_powerBatteryBase.state.fakeEnabled)
		return s_powerBatteryBase.state.fakeFullCapacity;
	status = s_powerBatteryBase.state.status;
	if (status == 0)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.fullCapacity;
}

inline __attribute__((always_inline)) int scePowerGetBatteryLifePercent(void)
{
	uint32_t context, duration, status, result, scaledCapacity, fullCapacity;
	uint64_t now, elapsed, started;

	if (!s_powerBatteryBase.state.fakeEnabled) {
		status = s_powerBatteryBase.state.status;
		if (status == 0)
			return SCE_POWER_ERROR_NO_BATTERY;
		if (status == 1)
			return SCE_POWER_ERROR_DETECTING;
		return s_powerBatteryBase.state.lifePercent;
	}
	context = sceSyscallEnter();
	now = sceKernelGetSystemTimeWide();
	duration = s_powerBatteryBase.state.fakeDuration;
	started = s_powerBatteryBase.state.fakeStartTime;
	if (!duration) {
		scaledCapacity = s_powerBatteryBase.state.fakeRemainCapacity;
		fullCapacity = s_powerBatteryBase.state.fakeFullCapacity;
		result = __aeabi_uidiv(scaledCapacity * 100, fullCapacity);
	} else {
		elapsed = now - started;
		result = 0;
		if (elapsed <= duration) {
			scaledCapacity = s_powerBatteryBase.state.fakeRemainCapacity;
			fullCapacity = s_powerBatteryBase.state.fakeFullCapacity;
			scaledCapacity *= 100;
			result = __aeabi_uldivmod((duration - elapsed) * scaledCapacity,
				(uint64_t)fullCapacity * duration);
		}
	}
	sceSyscallLeave(context);
	return result;
}

int scePowerGetBatteryLifeTime(void)
{
	uint32_t context = sceSyscallEnter();
	uint32_t duration;
	uint64_t now, started, elapsed;
	int32_t remaining;
	int result = 0;

	if (s_powerBatteryBase.state.fakeEnabled) {
		now = sceKernelGetSystemTimeWide();
		duration = s_powerBatteryBase.state.fakeDuration;
		started = s_powerBatteryBase.state.fakeStartTime;
		if (!duration)
			goto restoreContext;
		elapsed = now - started;
		if (elapsed > duration)
			goto restoreContext;
		/* The native signed multiply-high sequence divides the low
		 * remaining word by one million, truncating toward zero.
		 */
		remaining = (int32_t)(duration - (uint32_t)elapsed);
		result = remaining / 1000000;
	} else if (!SceSysconForDriver_9DA2A5AB()) {
		result = s_powerBatteryBase.state.lifeTime;
		if (result < 1)
			result = 1;
	}
restoreContext:
	sceSyscallLeave(context);
	return result;
}

int scePowerGetBatteryRemainLevel(void)
{
	uint32_t context, status, version;
	int level, low;

	version = SceSysconForDriver_FF86F4C5();
	if (version > 0x01040104) {
		status = s_powerBatteryBase.state.status;
		if (status == 0)
			return SCE_POWER_ERROR_NO_BATTERY;
		if (status == 1)
			return SCE_POWER_ERROR_DETECTING;
		return s_powerBatteryBase.state.remainLevel;
	}
	level = scePowerGetBatteryLifePercent();
	if (level < 0)
		return level;
	context = sceSyscallEnter();
	low = powerGetBatteryLowState();
	sceSyscallLeave(context);
	if (low)
		return 0;
	if (level <= 34)
		return 1;
	if (level <= 54)
		return 2;
	if (level <= 69)
		return 3;
	return 4;
}

int scePowerGetBatteryRemainMaxLevel(void)
{
	uint32_t version = SceSysconForDriver_FF86F4C5();

	if (version > 0x01040104)
		return s_powerBatteryBase.state.remainMaxLevel;
	return 4;
}

int scePowerGetBatteryTemp(void)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.temperature;
}

int scePowerGetBatteryElec(uint32_t *current)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	*current = s_powerBatteryBase.state.electricCurrent;
	return 0;
}

int scePowerGetBatteryVolt(void)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.voltage;
}

int scePowerGetBatterySOH(void)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.stateOfHealth;
}

int scePowerGetBatteryCycleCount(void)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.cycleCount;
}

int scePowerGetCaseTemp(void)
{
	uint32_t status = s_powerBatteryBase.state.status;

	if (!status)
		return SCE_POWER_ERROR_NO_BATTERY;
	if (status == 1)
		return SCE_POWER_ERROR_DETECTING;
	return s_powerBatteryBase.state.temperature;
}

int ScePowerForDriver_0C6973B8(const volatile struct PowerBatteryFakeStatus *status)
{
	int remaining;
	uint32_t size, capacity, full, duration;

	if (!status) {
		s_powerBatteryBase.state.fakeEnabled = 0;
		s_powerBatteryBase.state.fakeStatusSize = 0;
		s_powerBatteryBase.state.fakeRemainCapacity = 0;
		s_powerBatteryBase.state.fakeFullCapacity = 0;
		s_powerBatteryBase.state.fakeDuration = 0;
		return 0;
	}
	if (status->size != sizeof(*status))
		return SCE_POWER_ERROR_INVALID_VALUE;
	s_powerBatteryBase.state.fakeEnabled = 0;
	dmb();
	capacity = status->remainingCapacity;
	full = status->fullCapacity;
	duration = status->duration;
	size = status->size;
	s_powerBatteryBase.state.fakeStatusSize = size;
	s_powerBatteryBase.state.fakeRemainCapacity = capacity;
	s_powerBatteryBase.state.fakeFullCapacity = full;
	s_powerBatteryBase.state.fakeDuration = duration;
	if (!s_powerBatteryBase.state.fakeFullCapacity)
		s_powerBatteryBase.state.fakeFullCapacity = 2000;
	if (!s_powerBatteryBase.state.fakeRemainCapacity) {
		if (s_powerBatteryBase.state.fakeEnabled) {
			uint32_t context = sceSyscallEnter();
			uint64_t now = sceKernelGetSystemTimeWide();
			uint64_t elapsed = now - s_powerBatteryBase.state.fakeStartTime;

			duration = s_powerBatteryBase.state.fakeDuration;
			remaining = 0;
			if (!duration)
				remaining = s_powerBatteryBase.state.fakeRemainCapacity;
			else if (elapsed <= duration)
				remaining = __aeabi_uldivmod((duration - elapsed) *
					s_powerBatteryBase.state.fakeRemainCapacity, duration);
			sceSyscallLeave(context);
		}
		else if (s_powerBatteryBase.state.status > 1)
			remaining = s_powerBatteryBase.state.remainCapacity;
		else
			remaining = -1;
		if (remaining >= 0)
			s_powerBatteryBase.state.fakeRemainCapacity = remaining;
		else
			s_powerBatteryBase.state.fakeRemainCapacity =
				s_powerBatteryBase.state.fakeFullCapacity;
	}
	s_powerBatteryBase.state.fakeStartTime = sceKernelGetSystemTimeWide();
	dmb();
	s_powerBatteryBase.state.fakeEnabled = 1;
	return 0;
}

int ScePowerForDriver_C62B6164(void (*callback)(uint32_t, void *),
			      uint32_t threshold, void *argument)
{
	dmb();
	s_powerBatteryBase.state.temperatureArgument = argument;
	s_powerBatteryBase.state.temperatureThreshold = threshold;
	s_powerBatteryBase.state.previousTemperature = s_powerBatteryBase.state.temperature;
	dmb();
	s_powerBatteryBase.state.temperatureCallback = callback;
	return 0;
}

int powerBatteryPausePending(void)
{
	return !s_powerBatteryBase.workerPaused;
}
