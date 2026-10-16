/*
 * System core of the Robin firmware: interrupt lines, the five ThreadX timer
 * slots, and the init table run from tx_application_define().
 */
#ifndef ROBIN_SYS_H
#define ROBIN_SYS_H

#include <stdint.h>
#include "robin/threadx.h"
#include "robin/event.h"

#define SYS_TIMER_COUNT	5

typedef int (*SysIrqHandler)(int line, uint32_t pending);
typedef int (*SysTimerCallback)(uint32_t index, uint32_t arg);
typedef void (*OsaTimerCallback)(uint32_t timer, uint32_t argument);

/* sys/sys.c */
int sysInit(void);
void sysIrqRegister(uint32_t line, SysIrqHandler handler);
int sysOsTimerIrq(int line, uint32_t pending);
int sysIrqUnregister(uint32_t line);
int sysIrqDispatch(void);
int sysIrqEnableAll(void);
int sysFatalHandler(void);
int sysInIsr(void);
UINT sysIrqDisable(void);
UINT sysIrqRestore(UINT posture);
int sysTimerRegister(uint32_t index, SysTimerCallback callback, uint32_t arg);
UINT sysTimerStart(uint32_t index, ULONG initial_ticks, ULONG reschedule_ticks);
UINT sysTimerStop(uint32_t index);
ULONG sysGetTime(ULONG *now);

/* sys/rom_patch.c */
int romPatchInit(void);
void romPatchInstall(uint32_t romAddress, uint32_t replacement);

/* sys/main.c */
extern void *sysFirstUnusedMemory;

/* Not converted yet: defined with their subsystems. */
void sysRegisterRomHooks(void);			/* boot/reset.S */
void sysRunInitTable(void);			/* sqram/init.c */
int osaInit(void);				/* OSA memory pools (0x3da4) */
void osaTimer0Isr(void);			/* OS tick follow-up (0x40f0) */
void osaTimer1Isr(void);			/* timer 1 expiry (0x40e0) */
void osaTimer2Isr(void);			/* timer 2 expiry (0x4206) */
void osaInitializeHardwareClock(void);
extern uint32_t osaTimerClockReload;
uint32_t osaGetTimerElapsedTicks(void);
void osaRestartTimerClock(void);
void osaDisableTimerInterrupt(void);
typedef void (*OsaTimerArmedHook)(void);
extern OsaTimerArmedHook osaTimerArmedHook;
uint32_t osaProgramTimerInterrupt(uint32_t delay);
uint64_t osaGetHardwareTime(void);
int irq17Handler(int line, uint32_t pending);	/* 0x62c */
int sysServiceWlanPackets(void);			/* WLAN packet completion service */
int irq22Handler(int line, uint32_t pending);	/* 0x892, also cascaded from line 1 */
int irq28Handler(int line, uint32_t pending);	/* 0x8a4 */
int irq30Handler(int line, uint32_t pending);	/* 0x688 */
uint32_t sysReadInterruptStatus(void);
uint32_t sysEnableInterruptSources(uint32_t sources);
uint32_t sysDisableInterruptSources(uint32_t sources);
uint32_t sysAcknowledgeInterruptSources(uint32_t sources);
int sysProcessPendingEvent(void);
void sysPendingEventExpired(uint32_t timer, uint32_t unusedArgument);
int sysArmPendingEvent(uint32_t flags, uint32_t delay);
int sysDeferPendingEvent(int dispatchMode);
void sysEnablePendingEvent(void);
uint32_t sysReadPendingEventEnable(void);
uint32_t sysSetPendingEventMode(int enabled);
uint32_t sysSetEventPathEnabled(uint32_t unusedPath, int enabled);
int sysEventPathIsEnabled(uint32_t path);
uint8_t *sysResetEventPolicy(void);
uint32_t sysApplyEventPolicy(uint32_t path, struct MacEventPolicy policy);
void sysSetEventPathPattern(uint32_t path, struct MacEventPolicy policy);
void sysConfigureEventPath(const struct MacEventPathConfiguration *configuration,
			   int toggleEnabled, int toggleActive,
			   uint32_t pathStates, int mode);
int sysInitializeEventPath(uint32_t path);
int sysSelectEventPath(uint32_t path);
extern uint8_t sysSelectedEventPath;
extern uint32_t sysEventPolicyInitialized;
int osaCreateTimer(OsaTimerCallback callback, uint32_t argument, uint32_t delay,
		   uint32_t *timer, uint32_t flags);
uint32_t sysPendingEventPolicyIsEnabled(void);
int sysCompletePendingEvent(void);
int sysEventGateIsSet(void);
int sysPendingEventIsArmed(void);
int sysDispatchPendingEventMode(uint32_t mode);
int osaCancelTimer(uint32_t timer);
uint32_t sysServiceEventInterrupts(void);
int sysDisableEventSources(uint32_t sources);
uint32_t sysUpdateEventDeadline(void);
int sysHandlePrimaryEvent(void);
int sysHandleSecondaryEvent(void);
uint32_t sysSetEventFlags(uint32_t flags);
uint32_t sysClearEventFlags(uint32_t flags);
uint32_t sysSetSecondaryEventFlags(uint32_t flags);
uint32_t sysClearSecondaryEventFlags(uint32_t flags);
uint32_t sysEventPrimaryHandshakeComplete(void);
uint32_t sysEventSecondaryHandshakeComplete(void);
void sysConfigureSecondaryEventEngine(void);
void sysResetSecondaryEventConfiguration(void);
void sysDisableSecondaryEventMode(void);
void sysEnableSecondaryEventMode(void);
void sysStartSecondaryEventEngine(void);
void sysStopSecondaryEventEngine(void);
void sysConfigureTertiaryEventPath(void);
void sysRestoreTertiaryEventPath(void);
void sysDisableTertiaryEventMode(void);
void sysEnableTertiaryEventMode(void);
void sysStartTertiaryEventEngine(void);
void sysStopTertiaryEventEngine(void);
void sysDisableEventEngineMode(void);
void sysEnableEventEngineMode(void);
void sysStartEventEngine(void);
void sysStopEventEngineAndWait(void);
void sysClearEventEngineTrigger(void);
void sysResetEventEngineConfiguration(void);
uint32_t sysConfigureEventEngineTiming(uint32_t primary, uint32_t secondary);
uint32_t sysReadEventEngineCountdown(void);
void sysProgramEventCountdown(uint32_t count, uint32_t unusedPrimary,
			      uint32_t unusedSecondary, uint32_t limit);
uint32_t sysReadEventCountdownReload(void);
void sysSetEventCountdownMode(void);
void sysClearEventCountdownMode(void);
uint32_t sysSelectEventEngineSource(uint32_t source);
uint32_t sysEventTransitionModeIsSet(void);
void sysSetEventTransitionMode(void);
void sysClearEventTransitionMode(void);
void sysTriggerEventEngineSource(void);
void sysInitializeEventRegisters(void);
uint32_t sysReadEventConfigurationStatus(void);
void sysSetEventModeFlag(void);
void sysClearEventModeFlag(void);
uint32_t sysEventAlternateModeIsSet(void);
void sysClearEventSleepRequest(void);
void sysSetEventSleepRequest(void);
void sysWaitForEventSleepHandshake(void);
void sysWaitForEventTransactions(void);
void sysSetEventPrimaryTiming(uint32_t lower, uint32_t upper);
void sysSetEventPrimaryTimingLower(uint32_t lower);
void sysSetEventTimingControl(uint32_t control);
void sysSetEventClockPeriod(uint32_t period);
void sysSetEventSecondaryTiming(uint32_t lower, uint32_t upper);
void sysSetEventControlByte(uint32_t control);
void sysWaitForEventMeasurement(void);
uint32_t sysStartEventMeasurement(uint32_t extent);
uint32_t sysReadEventMeasurementSample(void);
uint32_t sysReadEventMeasurementUnits(void);
uint32_t sysReadEventClock(void);
void sysDispatchMiddleEventBits(uint32_t events);
void sysDispatchHighEventBits(uint32_t events);
void sysDispatchEventSource(uint32_t index, uint32_t sources, uint32_t flags);
uint32_t sysGetEventSourceMapping(uint32_t index);
int sysHandleMappedEventSource(uint32_t index, uint32_t dispatch);
int sysEventEngineIsActive(void);
int sysPrepareEventDeadline(void);
int sysProcessEventQueues(uint32_t events);
uint32_t sysStopEventEngine(void);
extern uint32_t sysForceEventDispatch;
int sysServiceIndirectStatus(void);
uint32_t sysSubmitBbpCommand(uint32_t target, uint32_t value, uint32_t command);
int sysIssueBbpCommand(uint32_t target, uint32_t value, uint32_t command);
int sysAcknowledgeEvents(uint32_t events);
typedef volatile uint32_t *(*SysBbpCompletionHook)(void);
volatile uint32_t *sysWaitForBbpCommand(void);
SysBbpCompletionHook socEventHookInit(void);
extern SysBbpCompletionHook sysBbpCompletionHook;
int bbpIssueSecondaryCommand(uint32_t target, uint32_t value, uint32_t command);
int bbpIssuePrimaryCommand(uint32_t target, uint32_t value, uint32_t command);
int sysIssuePrimaryBbpCommand(uint32_t target, uint32_t value, uint32_t command);
extern void *wlanSdioDev;			/* mdev "sdio" handle (0x547ac) */
extern void *btSdioDev;				/* mdev "bt_sdio" handle (0x54c90) */

#endif /* ROBIN_SYS_H */
