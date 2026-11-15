/*
 * Sleep votes, the three event paths (primary, secondary, tertiary) that
 * hand the radio between WLAN, Bluetooth and the FM receiver, and the
 * Bluetooth coexistence link state that goes with them (0x28cc..0x3540,
 * 0x238d4..0x245d4). The names are inferred from use; see sys/eventpower.c.
 */
#ifndef ROBIN_EVENTPOWER_H
#define ROBIN_EVENTPOWER_H

#include <stdint.h>
#include "robin/clock.h"
#include "robin/gpio.h"
#include "robin/rfreg.h"
#include "robin/socpower.h"

struct OsaEventFlags;

/*
 * Bits of sysSleepVotes. A set bit allows the chip to sleep (sysSleepAllow()),
 * a cleared bit keeps it awake (sysSleepForbid()). The bits come in pairs:
 * what the path itself needs, and what the device routed to it needs.
 */
#define SLEEP_VOTE_PRIMARY_PATH		0x01
#define SLEEP_VOTE_PRIMARY_DEVICE	0x02
#define SLEEP_VOTE_SECONDARY_PATH	0x04
#define SLEEP_VOTE_SECONDARY_DEVICE	0x08
#define SLEEP_VOTE_WLAN			0x10
#define SLEEP_VOTE_BT			0x20
#define SLEEP_VOTE_WLAN_ENGINE		0x40	/* set with SLEEP_VOTE_WLAN */
#define SLEEP_VOTE_TERTIARY_DEVICE	0x80

/* sysEventEngineFlags */
#define SYS_ENGINE_EVENT_PATHS		0x01	/* cleared on silicon revision 0x40 */

/* Handler of an event source, set with sysSetEventSourceHandler(). */
typedef void (*SysEventSourceHandler)(void);

struct SysEventSourceHandlerEntry {
	uint8_t source;		/* event source: 12, 11, 10 */
	SysEventSourceHandler handler;
};

extern uint8_t sysEventEngineFlags;
extern uint8_t sysSleepShortWake;
extern uint32_t sysSleepVotes;
extern uint64_t sysIdleTime;
extern uint64_t sysIdleStart;
extern struct SysEventSourceHandlerEntry sysEventSourceHandlers[3];

/* sys/eventpower.c */
void sysProcessEventQueues(uint32_t events);
void sysEventRoute2Hold(void);
void sysEventRoute2Release(void);
void sysEventRoute3Hold(void);
void sysEventRoute3Release(void);
void sysHandleMappedEventSource(uint32_t device, uint32_t dispatch);
void sysHandlePrimaryEvent(void);
void sysHandleSecondaryEvent(void);
void sysEventPathEnter(int route, uint32_t device, uint32_t source);
void sysEventPathLeft(void);
void sysEventRoutesHold(uint32_t device);
void sysEventRoutesRelease(uint32_t device);
int sysEventRouteReady(unsigned int route, uint32_t device);
void sysEventRouteStop(unsigned int route, uint32_t device, uint32_t source);
void sysEventRouteStart(int route, uint32_t device);
int sysEventRouteBind(unsigned int device, unsigned int channel, unsigned int flags);
void sysEventDeviceActivate(int route, uint32_t device);
void sysEventRouteConfigure(unsigned int route, int device, int source);
int sysEventRouteIdle(unsigned int route);
void sysEventRoute0Hold(void);
void sysEventRoute0Release(void);
void sysEnterPrimaryEvent(void);
void sysPrimaryRouteStart(void);
void sysPrimaryRouteResume(void);
int sysPrimaryRouteReady(void);
int sysPrimaryWindowOpen(void);
int sysPrimaryWindowCheck(void);
void sysPrimaryRouteStop(void);
int sysPrimaryRouteArmed(void);
void sysPrimaryRouteTryStop(void);
void sysLeavePrimaryEvent(void);
void sysPrimaryEventPoll(void);
void sysFmCoexPathSet(void);
void sysFmCoexPathRestore(void);
void sysSecondaryGateOpen(void);
void sysSecondaryWaitAltMode(void);
void sysEventRoute1Hold(void);
void sysEventRoute1Release(void);
void sysEnterSecondaryEvent(void);
int sysSecondaryRouteReady(void);
int sysSecondaryReady(void);
int sysSecondaryCheck(void);
void sysSecondaryRouteStop(void);
void sysSecondaryRouteTryStop(void);
void sysTertiaryPathRestore(void);
void sysLeaveSecondaryEvent(void);
void sysSecondaryEventPoll(void);
void sysTertiaryPathConfigure(void);
int sysEnableEventSources(uint32_t sources);

void sysIdleWait(void);
uint32_t sysIdleSleep(void);
void sysSleepAllow(uint32_t votes);
void sysSleepForbid(uint32_t votes);
int sysSleepVotesAllowed(uint32_t votes);
uint32_t sysIdlePercent(void);
void sysIdleStatsReset(void);
uint32_t sysEventClockNow(void);
uint32_t sysEventClockElapsedUs(uint32_t since);
int sysSetEventSourceHandler(uint32_t source, SysEventSourceHandler handler);
int sysEventSourceReset(uint32_t source);
void sysEventSourceCallHandler(uint32_t sources);
void sysEventRoutesInit(void);
uint32_t sysEventEngineSleepPeriod(void);
uint32_t sysGetBusClock(void);
uint32_t sysEventEnginePollTicks(void);
void sysEventEngineInit(void);
void sysPathPresetsInit(void);

int sysPathPresetSelect(void);
int sysPathPresetApply(void);
int sysPathPresetClear(void);
int sysPathPresetSave(void);
int sysPathPresetRestore(void);
void sysPathRegistersInit(void);
int sysChannelQuiesce(int channel);
int sysEventRouteChannelUpdate(int route, int device, int source, int hold);
void sysEventEngineInitHook(void);
int sysChannelReset(unsigned int channel);
int sysEventRouteAcquireChannel(int route, int device, int source);
int sysEventRouteReleaseChannel(int route, int device, int source);
void sysEventRouteAdjust(int route, int device, int source, int flag);
void sysEventRoutesAdjust(void);

void sysTertiaryRouteStart(void);
void sysEnterTertiaryEvent(void);
int sysTertiaryRouteReady(void);
void sysTertiaryRouteStop(void);
int sysTertiaryRouteArmed(void);
void sysTertiaryRouteTryStop(void);

/* sys/btcoex.c */
void btCoexUpdateLink(void);
void btCoexLinkTimerExpired(uint32_t timer, uint32_t argument);
void btCoexSetProfile(uint16_t profile);
void btCoexActivityStarted(void);
void btCoexActivityStopped(void);
void btCoexSetScheduled(uint32_t scheduled);
void btCoexSetLoad(uint32_t load);
void btCoexSetLinkCapacity(uint32_t first, uint32_t second);
void btCoexPollTiming(uint32_t *interval, uint32_t *window);
uint32_t btCoexProfileBit1(void);
int btCoexProfilePollable(void);
uint32_t btCoexProfileBit3(void);
int btCoexLinkIsActive(void);
int btCoexLinkIsStarting(void);
uint32_t btCoexActivityStart(void);
int btCoexRefreshActivityIdle(void);
int btCoexActivityIdle(void);
int btCoexWlanFlagged(void);
void hostCmd00a9Reset(void);
uint32_t btCoexParam(void);
unsigned int btCoexActiveLinks(void);
unsigned int btCoexLinkCapacity(void);
void btSdioSetEnabled(int enabled);

/*
 * Not converted yet; the names are provisional until their own files are
 * written. Addresses are those of the 4.00 image.
 */
extern uint8_t *btLinkContext;				/* 0x400f954: the link controller's state */
extern uint8_t btLmpLcActiveCount;			/* 0x55287 */
extern const uint8_t rfCalibTimingA[];			/* 0x53d7e, per clock index */
extern const uint8_t rfCalibTimingB[];			/* 0x53d92 */
extern const uint8_t rfCalibTimingC[];			/* 0x53d88 */
void *btLmpLcFormatContext(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);	/* 0x432bc */
int btLmpLcBuildContext(uint32_t *remaining);		/* 0x42f4e */
int btLmpLcManageContext(int link);			/* 0x433f0 */
void btLmpLcClearState(void);				/* 0x429fc */
int btLmpLcUpdateDescriptor(void);			/* 0x49376 */
void btLmpLcSyncContext(void);				/* 0x43030 */
unsigned int btLmpLcResetEntry(void);			/* 0x438b0 */
int btHccRomHandleState(void);				/* ROM, via 0x52c54 */
void btLtsRomParseState(int a1);			/* ROM, via 0x52c5c */
int fmCoexPrepareState(uint32_t *state);		/* 0x49bc2 */
int fmCoexDispatchState(void);				/* 0x49be2 */
int fmCoexValidateState(uint32_t *state);		/* 0x49bba */
void fmCoexAdjustState(void);				/* 0x4a570 */
void fmCoexRecordState(void);				/* 0x4a58a */
extern struct OsaEventFlags *wlanHostSleepFlags;		/* 0x54bdc */
extern const uint16_t rfCalibrationTable[];		/* 0x53d50 */
void wlanMacRestart(void);				/* 0x1c92e */
void wlanMacSleepExit(void);				/* 0x1c870 */
int ampHciIsIdle(void);					/* 0x20f00 */
void wpaSuppQueryParam(uint32_t previous, uint32_t profile);	/* 0x2ec2a */

#endif /* ROBIN_EVENTPOWER_H */
