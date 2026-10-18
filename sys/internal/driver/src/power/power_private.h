/*
 * ScePower: the state and helpers its units share.
 */

#pragma once

#include "power_imports.h"
#include <power/power.h>
#include <power/led.h>
#include <idstorage/idstorage.h>
#include <sysmem/suspend.h>
#include <sysmem/sysevent.h>
#include <sysmem/barrier.h>
#include <sysmem/sysroot.h>
#include <intrmgr/syscall.h>

/*
 * Power callback argument bits. Sony's SDK (power.h) names the ones user code
 * sees; the kernel-only bits it lists as reserved are named by what this
 * module does with them.
 */
#define POWER_CALLBACKARG_THERMAL_SUSPEND		0x00000200
#define POWER_CALLBACKARG_LOW_BATTERY_SUSPEND		0x00000400
#define POWER_CALLBACKARG_USB_POWER			0x00000800
#define POWER_CALLBACKARG_SYSTEM_SUSPEND			0x00010000
#define POWER_CALLBACKARG_SYSTEM_RESUMING		0x00020000
#define POWER_CALLBACKARG_SYSTEM_RESUME			0x00040000
#define POWER_CALLBACKARG_DISPLAY_OFF			0x00400000	/* state */
#define POWER_CALLBACKARG_DISPLAY_ON			0x00800000	/* event */
/* the buttons: power_switch.c's events */
#define POWER_CALLBACKARG_PS_START_PRESS		0x04000000
#define POWER_CALLBACKARG_PS_POWER_PRESS		0x08000000
#define POWER_CALLBACKARG_PS_HOLD			0x10000000
#define POWER_CALLBACKARG_PS_PRESS			0x20000000
#define POWER_CALLBACKARG_POWER_HOLD			0x40000000
#define POWER_CALLBACKARG_POWER_PRESS			0x80000000
/* the state bits the syscon reports */
#define POWER_CALLBACKARG_SYSCON_STATE	(SCE_POWER_CALLBACKARG_BATTERYEXIST | \
					 SCE_POWER_CALLBACKARG_LOWBATTERY | \
					 POWER_CALLBACKARG_THERMAL_SUSPEND | \
					 POWER_CALLBACKARG_LOW_BATTERY_SUSPEND | \
					 POWER_CALLBACKARG_USB_POWER | \
					 SCE_POWER_CALLBACKARG_POWERONLINE)

#include <stdint.h>
#include <stdbool.h>
#include <lowio/pervasive_regs.h>
#include <ctrl/ctrl.h>
#include <power.h>
#include <kernel/error.h>
#include <stddef.h>
#include <string.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/debug.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <acmgr/acmgr.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <lowio/pervasive.h>
#include <lowio/gpio.h>
#include <psp2kern/kernel/threadmgr/workqueues.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/dipsw.h>
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>
#include <display/display.h>
#include <sysmem/procevent.h>
#include <touch/touch.h>

/* Native idle table has an eight-byte header followed by eight 32-byte rows. */
struct PowerIdleTimer {
	uint64_t lastTick;
	uint64_t interval;
	uint32_t propagationMask;
	PowerIdleCallback callback;
	void *callbackArgument;
	uint16_t tickFlags;
	uint8_t tickRecorded;
	uint8_t idleState;
};

struct PowerProcessIdleState {
	PowerProcessIdleCallback callback;
	void *argument;
};

struct PowerIdleState {
	uint64_t tickSnapshot;
	struct PowerIdleTimer timers[8];
	struct PowerProcessIdleState processCallback;
};

extern struct PowerIdleState s_powerIdleTimers;

/* imports */

/* The saved continuation returns through the original caller's LR and SP. */
extern int powerSaveCpuContext(void *context) __attribute__((returns_twice));

/* Voltage codes decrease as the required voltage increases. */

/* Actual callbacks required by Power's startup registration sequence. */

/* LED callback and script engine; appended after the existing LED state. */

extern int __aeabi_idiv(int numerator, int denominator);

extern uint32_t __aeabi_uidiv(uint32_t numerator, uint32_t denominator);
extern uint64_t __aeabi_uldivmod(uint64_t numerator, uint64_t denominator);
extern void powerCpuResumeEntry(void);
extern void powerRestoreCpuContext(void *context, int value)
	__attribute__((noreturn));
extern void powerSmcResumePrimary(void);
extern void powerSmcResumeSecondary(int value);
extern void powerSmcSuspendSecondary(void);

/* Native switch modes and controller timing state. */
/* s_powerSwitchState.eventFlag */
#define POWER_SWITCH_EVENT_BUTTONS	0x00000001	/* the controller buttons changed */
#define POWER_SWITCH_EVENT_DISPLAY_ON	0x00000004
#define POWER_SWITCH_EVENT_DISPLAY_OFF	0x00000008
#define POWER_SWITCH_EVENT_STANDBY	0x00000010
#define POWER_SWITCH_EVENT_SUSPEND	0x00000020
#define POWER_SWITCH_EVENT_SOFT_RESET	0x00000040
#define POWER_SWITCH_EVENT_COLD_RESET	0x00000080
#define POWER_SWITCH_EVENT_SHUTDOWN	0x00000200
#define POWER_SWITCH_EVENT_HIBERNATE	0x00000400
#define POWER_SWITCH_EVENT_HDMI_CEC	0x00000800	/* the TV's power key */
#define POWER_SWITCH_EVENT_UNLOCKED	0x00010000	/* no switch lock is held */
#define POWER_SWITCH_EVENT_BUSY		0x00020000	/* a transition is running */
#define POWER_SWITCH_EVENT_IDLE		0x00040000	/* the last transition finished */
#define POWER_SWITCH_EVENT_EXIT		0x80000000
#define POWER_SWITCH_EVENT_REQUESTS	(POWER_SWITCH_EVENT_DISPLAY_ON | POWER_SWITCH_EVENT_DISPLAY_OFF | \
					 POWER_SWITCH_EVENT_STANDBY | POWER_SWITCH_EVENT_SUSPEND | \
					 POWER_SWITCH_EVENT_SOFT_RESET | POWER_SWITCH_EVENT_COLD_RESET | \
					 POWER_SWITCH_EVENT_SHUTDOWN | POWER_SWITCH_EVENT_HIBERNATE)

/* the request a transition carries out (the transition takes request * 0x101) */
#define POWER_REQUEST_NONE		0
#define POWER_REQUEST_STANDBY		1
#define POWER_REQUEST_SUSPEND		2
#define POWER_REQUEST_SOFT_RESET	3
#define POWER_REQUEST_COLD_RESET	4
#define POWER_REQUEST_SHUTDOWN		5
#define POWER_REQUEST_HIBERNATE		6

/* ScePowerForDriver_C1853BA7: what the power button does */
#define POWER_SW_MODE_HOLD_STANDBY	0x1	/* held for the push time: standby */
#define POWER_SW_MODE_PRESS_SUSPEND	0x2
#define POWER_SW_MODE_PRESS_DISPLAY_OFF	0x4
#define POWER_SW_MODE_PRESS_STANDBY	0x8

struct PowerSwitchState {
	SceUID eventFlag;
	SceUID mainThread;
	uint32_t primaryMode;
	uint32_t secondaryMode;
	uint32_t powerOffDelay;
	uint32_t ledPolicy;
	uint32_t standbyButtonPushTime;
	uint32_t psButtonPushTime;
	uint32_t suspendInProgress;
	uint32_t processLockCount;
	uint32_t systemLockCount;
	uint32_t initHold;
	volatile uint32_t buttonRequest;/* POWER_REQUEST_*, from the power button */
	volatile uint32_t pendingRequest;/* POWER_REQUEST_*, from the API */
	uint32_t shutdownArgument;
	uint32_t resumeRequested;
	uint32_t powerModeArgument;
	uint32_t requestState;
	uint32_t wakeupFactor;
	uint32_t resumeCount;
	uint32_t interruptState;
	uint32_t interruptCpuMask;
	uint32_t controllerButtons;
	uint32_t controllerMask;
	SceUID controllerCallback;
};

_Static_assert(offsetof(struct PowerSwitchState, standbyButtonPushTime) == 0x18,
	       "Power standby push time offset");

_Static_assert(offsetof(struct PowerSwitchState, buttonRequest) == 0x30,
	       "Power request state offset");
_Static_assert(offsetof(struct PowerSwitchState, resumeRequested) == 0x3c,
	       "Power resume request offset");

struct PowerCallbackRecord {
	struct PowerCallbackRecord *next;
	SceUID pid;
	SceUID callback;
	uint32_t pendingEvents;
	uint32_t lastNotification;
	uint32_t permittedMask;
	uint32_t notified;
};

struct PowerCallbackControl {
	struct PowerCallbackRecord *head;
	uint32_t stateFlags;
	uint32_t notificationMask;
	uint8_t sysconStatus[4];
	uint8_t thermalAlert;
	uint8_t wlanActive;
	uint8_t padding[2];
	PowerWirelessCallback wirelessCallback;
	void *wirelessArgument;
	PowerProcessFlagsCallback flagCallbacks[4];
	void *flagArguments[4];
};

_Static_assert(sizeof(struct PowerCallbackRecord) == 0x1c,
	       "Power callback record size");
_Static_assert(offsetof(struct PowerCallbackControl, wirelessCallback) == 0x14,
	       "Power wireless callback offset");

/* Native state initialization remains unresolved. Unknown gaps stay explicit. */
struct PowerClockState {
	uint32_t busClockFrequency;
	uint32_t dmac5ClockFrequency;
	uint32_t cameraBusClockFrequency;
	uint32_t sysClockFrequency;
	uint32_t busSelector;
	uint32_t dmac5Selector;
	uint32_t cameraBusSelector;
	uint32_t sysSelector;
	uint32_t armClockFrequency;
	uint32_t armSelector;
	uint32_t veneziaClockFrequency;
	uint32_t veneziaSelector;
	uint32_t vipClockFrequency;
	uint32_t vipSelector;
	uint32_t compatClockFrequency;
	uint32_t compatSelector;
	uint32_t gpuXbarClockFrequency;
	uint32_t gpuXbarSelector;
	uint32_t gpuClockFrequency[2];
};

_Static_assert(offsetof(struct PowerClockState, armClockFrequency) == 0x20,
	       "Power ARM clock state offset");
_Static_assert(offsetof(struct PowerClockState, gpuClockFrequency) == 0x48,
	       "Power GPU clock state offset");

struct PowerClockControl {
	SceUID mutex;
	uint32_t hardwareInfo;
	uint32_t busVoltageTable[3];
	uint32_t armVoltageTable[6];
	uint32_t mediaVoltageTable[4];
	uint32_t compatVoltageTable[2];
	uint32_t gpuVoltageTable[2];
	uint32_t voltageDemand[10];
	uint32_t railVoltage[4];
	struct PowerClockState frequencies;
	uint32_t gpuSelector[2];
	void (*beforeGpuClockChange)(uint32_t, uint32_t, int, void *);
	void *beforeGpuClockArgument;
	void (*afterGpuClockChange)(uint32_t, uint32_t, int, void *);
	void *afterGpuClockArgument;
	uint32_t lockBypass;
	uint32_t kernelClockReduced;
	uint32_t gpuLimitOverride;
};

_Static_assert(offsetof(struct PowerClockControl, frequencies) == 0x84,
	       "Power clock frequencies offset");
_Static_assert(offsetof(struct PowerClockControl, lockBypass) == 0xec,
	       "Power clock lock bypass offset");

struct PowerBatteryState {
	uint32_t status;
	uint32_t remainCapacity;
	uint32_t lifePercent;
	uint32_t calculatedLifePercent;
	uint32_t fullCapacity;
	uint32_t remainLevel;
	uint32_t remainMaxLevel;
	uint32_t stateOfHealth;
	uint32_t healthStatus;
	uint32_t cycleCount;
	int32_t lifeTime;
	uint32_t temperature;
	uint32_t electricCurrent;
	uint32_t voltage;
	uint32_t usbStopPending;
	SceSysconPacket usbStopPacket;
	uint32_t usbStatus;
	uint32_t usbStatusPending;
	SceSysconPacket usbStatusPacket;
	uint32_t sysconLedOff;
	void (*temperatureCallback)(uint32_t, void *);
	void *temperatureArgument;
	uint32_t temperatureThreshold;
	uint32_t previousTemperature;
	uint64_t fakeStartTime;
	uint32_t fakeEnabled;
	uint32_t reserved;
	uint32_t fakeStatusSize;
	uint32_t fakeRemainCapacity;
	uint32_t fakeFullCapacity;
	uint32_t fakeDuration;
};

_Static_assert(offsetof(struct PowerBatteryState, temperature) == 0x2c,
	       "Power battery temperature offset");

_Static_assert(offsetof(struct PowerBatteryState, fakeEnabled) == 0x160,
	       "Power fake battery flag offset");
_Static_assert(offsetof(struct PowerBatteryState, fakeFullCapacity) == 0x170,
	       "Power fake full capacity offset");

/* Worker, charging and cached battery state share the native control block. */
struct PowerBatteryBase {
	SceUID eventFlag;
	SceUID worker;
	SceKernelSpinlock chargingLock;
	uint32_t reserved;
	uint32_t workerPaused;
	uint32_t chargingInhibitCount;
	uint32_t usbChargingEnabled;
	SceUID chargingAlarm;
	uint32_t initialScan;
	uint32_t scanStep;
	int32_t powerSource;
	uint32_t hardwareStatus;
	struct PowerBatteryState state;
};

_Static_assert(offsetof(struct PowerBatteryBase, state) == 0x30,
	       "Power cached battery state offset");
_Static_assert(offsetof(struct PowerBatteryBase, state.fakeEnabled) == 0x190,
	       "Power fake battery flag base offset");

_Static_assert(offsetof(struct PowerBatteryBase, state.fakeStartTime) == 0x188,
	       "Power fake start time base offset");
_Static_assert(offsetof(struct PowerBatteryBase, state.fakeRemainCapacity) == 0x19c,
	       "Power fake remaining capacity base offset");
_Static_assert(offsetof(struct PowerBatteryBase, state.fakeDuration) == 0x1a4,
	       "Power fake duration base offset");
struct PowerDisplayState {
	uint32_t requestedBrightness;
	uint32_t maxBrightness[2];
	uint32_t brightness[3];
	uint32_t displayEnabled;
	uint32_t controllerIdleGate;
	uint32_t ledWorkEnabled;
	uint32_t ledReason;
};

_Static_assert(offsetof(struct PowerBatteryBase, state.usbStopPending) == 0x68,
	       "Power USB stop pending offset");
_Static_assert(offsetof(struct PowerBatteryBase, state.usbStatus) == 0xec,
	       "Power USB status offset");
_Static_assert(offsetof(struct PowerBatteryBase, state.usbStatusPacket) == 0xf4,
	       "Power USB status packet offset");

/* LED error codes (lowio's 0x803f01xx facility; names unknown) */
#define SCE_LED_ERROR_INVALID_LED	0x803f0140
#define SCE_LED_ERROR_INVALID_MODE	0x803f0141

/* SceLedForDriver_EA24BE03's modes */
#define SCE_LED_MODE_OFF	0
#define SCE_LED_MODE_ON		1	/* Syscon pattern index */
#define SCE_LED_MODE_BLINK	2
#define SCE_LED_MODE_SCRIPT	3	/* option is a byte script */
#define SCE_LED_MODE_LEVEL	4	/* Syscon level, LED 2 only */

/* an open loop of an LED script (ops 3 and 4) */
struct PowerLedScriptLoop {
	const uint8_t *cursor;
	int count;
};

/* one LED: its GPIO line and what it is doing, updated each vblank */
struct PowerLedState {
	uint8_t gpio;			/* port << 5 | pin; 0: no LED */
	uint8_t polarity;		/* the output level that turns the LED off */
	uint16_t padding;
	uint32_t mode;
	int phase;			/* ticks into the on/off period */
	int onTicks;
	int offTicks;
	int repeatCount;
	int endState;
	uint32_t sysconLevel;
	uint32_t sysconIndex;
	uint32_t scriptMode;
	int scriptDelayTicks;
	const uint8_t *scriptCursor;
	const uint8_t *scriptLoop;
	int scriptDepth;
	struct PowerLedScriptLoop scriptStack[4];
};

_Static_assert(sizeof(struct PowerLedState) == 0x58, "Power LED row size");

_Static_assert(offsetof(struct PowerIdleState, processCallback) == 0x108,
	       "Power process idle callback offset");
_Static_assert(sizeof(struct PowerIdleState) == 0x110,
	       "Power idle control size");

_Static_assert(sizeof(struct PowerBatteryBase) == 0x1a8,
	       "Power battery control size");

/* Native low-power state passed to Syscon and system event consumers. */
/*
 * What Syscon reports on resume: the same words as the boot's SceKblParam at
 * 0xc0 (vitasdk psp2kern/kernel/kbl/kbl.h).
 */
struct PowerResumeInformation {
	uint32_t powerOffFactor;
	uint32_t wakeupFactor;
	uint32_t requestState;
	uint32_t holdControl;
	uint32_t resumeContextAddress;
	uint32_t hardwareInfo;
	uint32_t powerInfo;		/* vitasdk: 0x8 AC connected */
	uint32_t reserved;
};

struct PowerMonitorState {
	uint32_t size;
	uint32_t powerModeArgument;
	struct PowerResumeInformation *resumeInformation;
	void (*resumeEntry)(void);
	uint32_t cp15[14];
	uint32_t reserved[8];
};

_Static_assert(sizeof(struct PowerMonitorState) == 0x68,
	       "Power monitor state size");

/* Per-process requested clock frequencies, idle timers and switch locks. */

/* the clocks a process requests a frequency of */
enum PowerProcessClock {
	POWER_PROCESS_CLOCK_BUS,
	POWER_PROCESS_CLOCK_ARM,
	POWER_PROCESS_CLOCK_GPU_XBAR,
	POWER_PROCESS_CLOCK_GPU,
	POWER_PROCESS_CLOCKS
};

struct PowerProcessState {
	struct PowerProcessState *next;
	SceUID pid;
	uint32_t clockExcluded;
	uint32_t powerLockCount;
	uint64_t idleInterval;
	uint64_t lastIdleTick;
	uint32_t tickRecorded;
	uint32_t idleState;
	int clockFrequency[POWER_PROCESS_CLOCKS];	/* requested, by enum PowerProcessClock */
	uint32_t wirelessFlags;
};

_Static_assert(offsetof(struct PowerProcessState, clockFrequency) == 0x28,
	       "Power process bus clock offset");

/* Only bits zero and one have proved wireless meanings; other bits are raw. */
_Static_assert(offsetof(struct PowerProcessState, wirelessFlags) == 0x38,
	       "Power process wireless flags offset");

_Static_assert(offsetof(struct PowerProcessState, idleInterval) == 0x10,
	       "Power process idle interval offset");
_Static_assert(offsetof(struct PowerProcessState, lastIdleTick) == 0x18,
	       "Power process idle tick offset");

/* shared state */
extern struct PowerCallbackControl s_powerCallbackControl;
extern struct PowerClockControl s_powerClockControl;
extern struct PowerBatteryBase s_powerBatteryBase;
extern struct PowerDisplayState s_powerDisplayState;
extern int s_processStorageKey;
extern SceKernelSpinlock s_powerIdleLock;
extern struct PowerProcessState *volatile s_powerProcessList;

/* across units */

uint32_t powerIsIdleTimer(uint32_t timerId);

int powerTickInternalPredicate(void);

int powerPermitChargeTimer(void *argp);

int powerChargingCommandComplete(SceSysconPacket *packet, void *argp);

int powerLockClockControl(void);
int powerSetBusRailClockRaw(unsigned int lane, int frequency,
			    unsigned int selector, uint32_t demand);

void powerUnlockClockControl(void);

void powerNotifyWirelessLimit(int enabled);

void powerNotifyProcessFlags(SceUID pid, uint32_t flags);

void powerNotifyCallback(uint32_t clearMask,
							    uint32_t setMask,
							    uint32_t events);

int powerInitializeClockControl(void);

int powerInitializeIdleControl(void);

void powerDefaultIdleCallback(uint32_t timerId, uint32_t elapsed,
			      void *argument, uint32_t elapsedHigh);

void powerRefreshDisplayBrightness(void);

uint32_t powerSwitchDisplay(uint32_t enabled, uint32_t reason);

int powerInitializeDisplayControl(void);

int powerInitializeBatteryControl(void);

int powerInitializeLedControl(void);
int powerStartLedControl(void);

int powerInitializeClockRuntime(void);

void powerRestoreClockRuntime(void);

int powerSuspendBatteryControl(void);

int powerResumeBatteryControl(const struct PowerResumeInformation *information);

int powerInitializeSwitchHardware(void);

int powerInitializeSwitchControl(void);

int powerInitializeKernelProcessState(void);

void powerCreateProcessState(SceUID pid);

void powerUnlinkKilledProcess(SceUID pid);

void powerUnlinkExitedProcess(SceUID pid);

void powerProcessSuspendEvent(SceUID pid, uint32_t event);

void powerProcessResumeEvent(SceUID pid, uint32_t event);

int powerReleaseProcessSwitchLocks(SceUID pid);

int powerTickProcessForPid(SceUID pid, uint32_t mask);
int powerBatteryPausePending(void);
void powerInitializeProcessIdleState(struct PowerProcessState *state);
void powerRecordProcessIdleTick(struct PowerProcessState *state);
int powerHasBusyCallback(uint32_t mask, SceUID *callback);
int powerClockSysEventHook(void);
