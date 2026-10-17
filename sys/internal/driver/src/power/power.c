#line 1 "power.c"
/*
 * ScePower: callbacks, notifications, module start.
 */

#include "power_private.h"

static inline __attribute__((always_inline)) int powerCreateProcessStorage(void);

/* exported, and inlined in this unit as in the original */
int kscePowerUnregisterCallback(SceUID callback);

int powerProcessSuspendNotification(SceUID pid, int phase, ProcEventParam *param, int args);

int powerProcessKillCallback(SceUID pid, ProcEventParam *param, int args);

int powerProcessExitCallback(SceUID pid, ProcEventParam *param, int args);

int powerProcessCreateCallback(SceUID pid, ProcEventCreateParam *param, int args);

int powerProcessResumeNotification(SceUID pid, int phase, ProcEventParam *param, int args);

struct PowerCallbackControl s_powerCallbackControl;

struct PowerBatteryBase s_powerBatteryBase;

SceKernelSpinlock s_powerIdleLock;
volatile uint32_t s_powerCpuResumeHandshake;
extern const SceKernelProcEventHandler s_powerProcessEventHandlers;

/* the debugger's power event notifiers (event 49): stored, never read */
static volatile uintptr_t s_powerDebugNotifier0;
static volatile uintptr_t s_powerDebugNotifier1;

int s_processStorageKey;

extern SceKernelSpinlock s_powerIdleLock;

const SceKernelProcEventHandler s_powerProcessEventHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = powerProcessCreateCallback,
	.exit = powerProcessExitCallback,
	.kill = powerProcessKillCallback,
	.suspend = powerProcessSuspendNotification,
	.resume = powerProcessResumeNotification,
};
int powerStartupWorker(void)
{
	uint8_t storage[4], command[2];
	int ret;

	ksceSysconWaitInitialized();
	powerInitializeClockRuntime();
	s_powerCallbackControl.sysconStatus[0] = SceSysconForDriver_27758A64();
	if (s_powerCallbackControl.sysconStatus[0] == 1)
		s_powerCallbackControl.stateFlags |= 0x100;
	if (SceSysconForDriver_9DA2A5AB())
		s_powerCallbackControl.stateFlags |= 0x1000;
	s_powerCallbackControl.sysconStatus[1] = SceSysconForDriver_D7F5A797();
	s_powerCallbackControl.sysconStatus[2] = SceSysconForDriver_03C50DC3();
	s_powerCallbackControl.sysconStatus[3] = SceSysconForDriver_27758A64();
	s_powerCallbackControl.thermalAlert = SceSysconForDriver_50CAE242();
	ret = ksceIdStorageLookup(0x117, 0, storage, sizeof(storage));
	if (ret >= 0) {
		command[0] = storage[0];
		command[1] = storage[1];
		SceSysconForDriver_4E0EB57B(0x78, command, sizeof(command));
	}
	powerInitializeSwitchHardware();
	return 1;
}

int powerProcessSuspendNotification(SceUID pid, int phase, ProcEventParam *param, int args)
{
	powerProcessSuspendEvent(pid, phase);
	return 0;
}

static void _scePowerNotifyCallbackProc(SceUID pid, uint32_t clearMask,
					uint32_t setMask, uint32_t events);

int _scePowerRemoveProcessCallback(SceUID pid)
{
	struct PowerCallbackRecord *record, *previous;
	SceKernelIntrStatus intrState;
	int result;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	for (;;) {
		intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
		previous = NULL;
		for (record = s_powerCallbackControl.head; record; record = record->next) {
			if (record->pid == pid)
				break;
			previous = record;
		}
		if (!record) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
			return 0;
		}
		if (previous)
			previous->next = record->next;
		else
			s_powerCallbackControl.head = record->next;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		result = sceKernelFreeRemoteProcessHeap(pid, record);
#if FW_DEBUG
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_821A2D59(1, result == 0,
				SCE_DBGINFO_FUNC(282, "_scePowerRemoveProcessCallback"), caller,
				"sceKernelFreeRemoteProcessHeap(0x%08X, %p) error: 0x%08X\n",
				pid, record, result);
#else
		(void)result;
#endif
	}
}

int powerProcessKillCallback(SceUID pid, ProcEventParam *param, int args)
{
	powerUnlinkKilledProcess(pid);
	powerReleaseProcessSwitchLocks(pid);
	_scePowerRemoveProcessCallback(pid);
	return 0;
}

int powerProcessExitCallback(SceUID pid, ProcEventParam *param, int args)
{
	powerUnlinkExitedProcess(pid);
	powerReleaseProcessSwitchLocks(pid);
	_scePowerRemoveProcessCallback(pid);
	return 0;
}

int powerProcessCreateCallback(SceUID pid, ProcEventCreateParam *param, int args)
{
	powerCreateProcessState(pid);
	return 0;
}

int powerSuspendTimerCallback(void *argument)
{
	scePowerRequestSuspend();
	return 0;
}

int powerSysEventHandler(int phase, int event, void *args, void *opt)
{
	const struct PowerResumeEvent *resume = args;
	const struct PowerResumeInformation *information;
	uint32_t state;
	int powered;

	if (phase == 0) {
		if (event == SYSEVENT_SUSPEND_IDLE_ENTER)
			powerSuspendBatteryControl();
		else if (event == SYSEVENT_SUSPEND_IDLE_QUERY)
			return powerBatteryPausePending() ? -1 : 0;
		else if (event == SYSEVENT_SUSPEND_DEVICE(9))
			powerClockSysEventHook();
		return 0;
	}
	if (phase != 1)
		return 0;
	if (event == SYSEVENT_RESUME_START) {
		powerResumeBatteryControl(resume->resumeInformation);
		return 0;
	}
	if (event != SYSEVENT_RESUME_DEVICE(9))
		return 0;
	information = resume->resumeInformation;
	powered = SceSysconForDriver_9DA2A5AB();
	state = powered ? 0x1000 : 0;
	if (information->powerInfo & 0x40)
		state = powered ? 0x1080 : 0x80;
	s_powerCallbackControl.sysconStatus[0] = SceSysconForDriver_27758A64();
	if (s_powerCallbackControl.sysconStatus[0])
		state |= 0x100;
	s_powerCallbackControl.sysconStatus[1] = SceSysconForDriver_D7F5A797();
	s_powerCallbackControl.sysconStatus[2] = SceSysconForDriver_03C50DC3();
	if (s_powerCallbackControl.sysconStatus[1] ||
	    s_powerCallbackControl.sysconStatus[2])
		state |= 0x400;
	s_powerCallbackControl.sysconStatus[3] = SceSysconForDriver_27758A64();
	if (s_powerCallbackControl.sysconStatus[1])
		state |= 0x800;
	s_powerCallbackControl.thermalAlert = SceSysconForDriver_50CAE242();
	if (s_powerCallbackControl.thermalAlert)
		state |= 0x200;
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID, ~state & POWER_CALLBACKARG_SYSCON_STATE, state, 0);
	powerRestoreClockRuntime();
	return 0;
}

void powerOnlineCallback(int enable, void *argp)
{
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID,
				    enable ? 0 : SCE_POWER_CALLBACKARG_POWERONLINE,
				    enable ? SCE_POWER_CALLBACKARG_POWERONLINE : 0, 0);
	scePowerBatteryUpdateInfo();
	powerRefreshDisplayBrightness();
}

void powerLowBatteryCallback(int enable, void *argp)
{
	if (s_powerCallbackControl.sysconStatus[0] == enable)
		return;
	s_powerCallbackControl.sysconStatus[0] = enable;
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID,
				    enable ? 0 : SCE_POWER_CALLBACKARG_LOWBATTERY,
				    enable ? SCE_POWER_CALLBACKARG_LOWBATTERY : 0, 0);
	scePowerBatteryUpdateInfo();
}

/* argp: this level's status byte (sysconStatus[1] or [2]) */
void powerLowBatterySuspendCallback(int enable, void *argp)
{
	uint8_t *status = argp;

	if (*status == enable)
		return;
	*status = enable;
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID,
				    enable ? 0 : POWER_CALLBACKARG_LOW_BATTERY_SUSPEND,
				    enable ? POWER_CALLBACKARG_LOW_BATTERY_SUSPEND : 0, 0);
	if (enable)
		ksceKernelRegisterTimer("ScePowerLowBatterySuspend", 5000000,
					powerSuspendTimerCallback, NULL);
}

void powerUsbPowerCallback(int enable, void *argp)
{
	if (s_powerCallbackControl.sysconStatus[3] == enable)
		return;
	s_powerCallbackControl.sysconStatus[3] = enable;
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID,
				    enable ? 0 : POWER_CALLBACKARG_USB_POWER,
				    enable ? POWER_CALLBACKARG_USB_POWER : 0, 0);
}

void powerThermalCallback(int enable, void *argp)
{
	if (s_powerCallbackControl.thermalAlert == enable)
		return;
	s_powerCallbackControl.thermalAlert = enable;
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID,
				    enable ? 0 : POWER_CALLBACKARG_THERMAL_SUSPEND,
				    enable ? POWER_CALLBACKARG_THERMAL_SUSPEND : 0, 0);
	if (enable)
		ksceKernelRegisterTimer("ScePowerThermalAlertSuspend", 5000000,
					powerSuspendTimerCallback, NULL);
}

int powerInitializeDefaultIdleTimer(void)
{
	kscePowerSetIdleCallback(0, 0, 0,
				powerDefaultIdleCallback, NULL);
	return 0;
}


int powerInitializeModule(void)
{
	int result;

	powerStartLedControl();
	memset((void *)&s_powerCallbackControl, 0, sizeof(s_powerCallbackControl));
	s_powerCallbackControl.notificationMask = UINT32_MAX;
	result = powerCreateProcessStorage();
	if (result < 0)
		return result;
	powerInitializeClockControl();
	powerInitializeKernelProcessState();
	powerInitializeSwitchControl();
	powerInitializeBatteryControl();
	powerInitializeIdleControl();
	powerInitializeDisplayControl();
	powerInitializeDefaultIdleTimer();
	ksceSysconSetAlarmCallback(powerOnlineCallback, NULL);
	SceSysconForDriver_80D6E061(powerLowBatteryCallback, NULL);
	ksceSysconSetLowBatteryCallback(powerLowBatterySuspendCallback,
					&s_powerCallbackControl.sysconStatus[1]);
	SceSysconForDriver_2D471528(powerLowBatterySuspendCallback,
				    &s_powerCallbackControl.sysconStatus[2]);
	SceSysconForDriver_129EA022(powerUsbPowerCallback, NULL);
	ksceSysconSetThermalAlertCallback(powerThermalCallback, NULL);
	sceKernelRegisterSysEventHandler("ScePower", powerSysEventHandler, NULL);
	sceKernelRegisterProcEventHandler("ScePower", &s_powerProcessEventHandlers, 0);
	ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "ScePowerInit", powerStartupWorker, NULL);
	s_powerDebugNotifier0 = SceSysrootForDriver_10788257();
	s_powerDebugNotifier1 = SceSysrootForDriver_0A52A59C();
	return 0;
}

int module_start(SceSize args, void *argument)
{
	powerInitializeModule();
	return 0;
}

int kscePowerSetCallbackState(uint32_t state)
{
	s_powerCallbackControl.notificationMask = state;
	return 0;
}

int kscePowerRegisterCallback(SceUID callback)
{
	struct PowerCallbackRecord *record, *current;
	SceKernelIntrStatus intrState;
	uint32_t permittedMask;
	int result;

	if (ksceKernelGetThreadmgrUIDClass(callback) != 8)
		return (int)0xc00280a1;
	permittedMask = 0xfcf71f80;
	if (sceKernelGetProcessId() != SCE_KERNEL_PROCESS_ID && !sceSblACMgrIsSystemProgram(0))
		permittedMask = 0x00361180;
	record = SceProcessmgrForDriver_5468892A(sizeof(*record), 0);
	if (!record)
		return -1;
	record->pid = sceKernelGetProcessId();
	record->callback = callback;
	record->pendingEvents = 0;
	record->lastNotification = s_powerCallbackControl.stateFlags;
	record->permittedMask = permittedMask;
	record->notified = 0;
	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	for (current = s_powerCallbackControl.head; current; current = current->next) {
		if (current->callback == callback) {
			result = SCE_POWER_ERROR_ALREADY_REGISTERED;
			goto fail;
		}
	}
	result = ksceKernelNotifyCallback(callback,
			s_powerCallbackControl.notificationMask &
			s_powerCallbackControl.stateFlags & permittedMask);
	if (result < 0)
		goto fail;
	record->next = s_powerCallbackControl.head;
	s_powerCallbackControl.head = record;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
fail:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	SceProcessmgrForDriver_FC2A424E(record);
	return result;
}

int scePowerRegisterCallback(SceUID callback)
{
	uint32_t context = sceSyscallEnter();
	SceUID global = SceSysmemForDriver_45D22597(0, callback);
	int result = (int)0xc00280a1;

	if (global >= 0)
		result = kscePowerRegisterCallback(global);
	sceSyscallLeave(context);
	return result;
}

inline __attribute__((always_inline)) int kscePowerUnregisterCallback(SceUID callback)
{
	struct PowerCallbackRecord *record, *previous = NULL;
	SceKernelIntrStatus intrState;

	if (ksceKernelGetThreadmgrUIDClass(callback) != 8)
		return (int)0xc00280a1;
	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	for (record = s_powerCallbackControl.head; record; record = record->next) {
		if (record->callback == callback)
			break;
		previous = record;
	}
	if (!record) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
		return SCE_POWER_ERROR_CALLBACK_NOT_REGISTERED;
	}
	if (previous)
		previous->next = record->next;
	else
		s_powerCallbackControl.head = record->next;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	SceProcessmgrForDriver_FC2A424E(record);
	return 0;
}

int scePowerUnregisterCallback(SceUID callback)
{
	uint32_t context = sceSyscallEnter();
	SceUID global = SceSysmemForDriver_45D22597(0, callback);
	int result = (int)0xc00280a1;

	if (global >= 0)
		result = kscePowerUnregisterCallback(global);
	sceSyscallLeave(context);
	return result;
}

/*
 * Update the state flags and notify the callbacks of process `pid`
 * (SCE_KERNEL_ALL_PROCESS_ID: every callback); a callback that cannot be
 * notified any more is unlinked and freed.
 */
static void _scePowerNotifyCallbackProc(SceUID pid, uint32_t clearMask,
					uint32_t setMask, uint32_t events)
{
	struct PowerCallbackRecord *record;
	struct PowerCallbackRecord *previous = NULL;
	struct PowerCallbackRecord *next;
	SceKernelIntrStatus intrState;
	SceUID callback;
	uint32_t state, pending, notification;
	int count, result;
#if FW_DEBUG
	const void *caller = __builtin_return_address(0);
#endif

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	state = (s_powerCallbackControl.stateFlags & ~clearMask) | setMask;
	s_powerCallbackControl.stateFlags = state;
	for (record = s_powerCallbackControl.head; record; record = next) {
		next = record->next;
		if (pid != SCE_KERNEL_ALL_PROCESS_ID && record->pid != pid) {
			previous = record;
			continue;
		}
		callback = record->callback;
		if (callback >= 0) {
			count = ksceKernelGetCallbackCount(callback);
			if (count >= 0) {
				pending = count ? record->pendingEvents & ~clearMask : 0;
				pending |= events;
				notification = state | pending;
				record->pendingEvents = pending;
				record->lastNotification = notification;
				result = ksceKernelNotifyCallback(callback,
						notification & record->permittedMask &
						s_powerCallbackControl.notificationMask);
				if (result >= 0) {
					previous = record;
					continue;
				}
			}
		}
		if (previous)
			previous->next = next;
		else
			s_powerCallbackControl.head = next;
		result = sceKernelFreeRemoteProcessHeap(record->pid, record);
#if FW_DEBUG
		if (SceDebugForDriver_374B7868() > 0)
			SceDebugForDriver_821A2D59(1, result == 0,
				SCE_DBGINFO_FUNC(993, "_scePowerNotifyCallbackProc"), caller,
				"sceKernelFreeRemoteProcessHeap(0x%08X, %p) error: 0x%08X\n",
				record->pid, record, result);
#endif
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
}

int powerProcessResumeNotification(SceUID pid, int phase, ProcEventParam *param, int args)
{
	if (phase == 0x20000)
		_scePowerNotifyCallbackProc(pid, 0, SCE_POWER_CALLBACKARG_PROCESS_RESUMING, 0);
	else if (phase == 0x40000)
		_scePowerNotifyCallbackProc(pid, SCE_POWER_CALLBACKARG_PROCESS_RESUMING, 0,
					    SCE_POWER_CALLBACKARG_PROCESS_RESUME_COMP);
	powerProcessResumeEvent(pid, phase);
	return 0;
}

int ScePowerForDriver_FE35B73D(PowerWirelessCallback callback, void *argument)
{
	s_powerCallbackControl.wirelessCallback = callback;
	s_powerCallbackControl.wirelessArgument = argument;
	return 0;
}

void powerNotifyWirelessLimit(int enabled)
{
	PowerWirelessCallback callback = s_powerCallbackControl.wirelessCallback;

	if (callback)
		callback(enabled, s_powerCallbackControl.wirelessArgument);
}

int ScePowerForDriver_B24263F0(unsigned int index,
		PowerProcessFlagsCallback callback, void *argument)
{
	if (index >= 4)
		return SCE_POWER_ERROR_INVALID_VALUE;
	s_powerCallbackControl.flagCallbacks[index] = callback;
	s_powerCallbackControl.flagArguments[index] = argument;
	return 0;
}

void powerNotifyProcessFlags(SceUID pid, uint32_t flags)
{
	unsigned int index;

	for (index = 0; index < 4; index++) {
		PowerProcessFlagsCallback callback;

		callback = s_powerCallbackControl.flagCallbacks[index];
		if (callback)
			callback(pid, flags, s_powerCallbackControl.flagArguments[index]);
	}
}

/* Native module-global WLAN byte; startup and broader state aliases unresolved. */

int kscePowerWlanActivate(void)
{
	s_powerCallbackControl.wlanActive = 1;
	return 0;
}

int kscePowerWlanDeactivate(void)
{
	s_powerCallbackControl.wlanActive = 0;
	return 0;
}

/* Preserve the raw byte; Sony's internal function name is unresolved. */
int ScePowerForDriver_2B51FE2F(void)
{
	return s_powerCallbackControl.wlanActive;
}

/* name unknown: the notification the switch, display and battery code send */
void powerNotifyCallback(uint32_t clearMask, uint32_t setMask, uint32_t events)
{
	_scePowerNotifyCallbackProc(SCE_KERNEL_ALL_PROCESS_ID, clearMask, setMask, events);
}

int powerHasBusyCallback(uint32_t mask, SceUID *callback)
{
	struct PowerCallbackRecord *record;
	uint32_t intrState;

	intrState = SceCpuForDriver_D32ACE9E(&s_powerIdleLock);
	for (record = s_powerCallbackControl.head; record;
	     record = record->next) {
		if (record->callback >= 0 &&
		    (record->lastNotification & mask) && record->notified &&
		    ksceKernelGetCallbackCount(record->callback) > 0) {
			if (callback)
				*callback = record->callback;
			sceKernelSpinlockLowUnlockCpuResumeIntr(
				&s_powerIdleLock, intrState);
			return 1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_powerIdleLock, intrState);
	return 0;
}

static inline __attribute__((always_inline)) int powerCreateProcessStorage(void)
{
	int key;

	key = ksceKernelCreateProcessLocalStorage("ScePower", 0x40);
	if (key >= 0)
		s_processStorageKey = key;
	return key;
}
