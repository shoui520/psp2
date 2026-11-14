/*
 * WLAN/Bluetooth time sharing (0x8004..0x8e8c). While a station link and the
 * Bluetooth side both want the antenna, the firmware cuts time into cycles:
 * each cycle opens a WLAN window, closes it with a sleep timer and leaves the
 * rest to Bluetooth. The window lengths follow the configured period and
 * reserve, the Bluetooth link state and a running estimate of how long the
 * WLAN side stays awake. Marvell published no source for it; the names say
 * what the code does.
 */
#ifndef ROBIN_COEX_H
#define ROBIN_COEX_H

#include <stdint.h>

#include "robin/threadx.h"

struct WlanConnection;

/* Time-sharing modes, chosen when the link changes (WLAN_EVENT_COEX_UPDATE). */
#define WLAN_COEX_MODE_STATION		0x01
#define WLAN_COEX_MODE_P2P		0x02
#define WLAN_COEX_MODE_DEFAULT		0x04
#define WLAN_COEX_MODE_P2P_SLEEP	0x08
#define WLAN_COEX_MODE_BT_QUARTER	0x10	/* btConfig 4: Bluetooth decides the windows */
#define WLAN_COEX_MODE_BT_HALF		0x20	/* btConfig 8 */
#define WLAN_COEX_MODE_BT		(WLAN_COEX_MODE_BT_QUARTER | WLAN_COEX_MODE_BT_HALF)

/* wlanCoexState */
#define WLAN_COEX_OFF			0
#define WLAN_COEX_IDLE			1
#define WLAN_COEX_WLAN_WINDOW		2
#define WLAN_COEX_BT_WINDOW		3

/* Why the control task is asked to act (WLAN_EVENT_COEX_REQUEST); bits 8-15 end a window. */
#define WLAN_COEX_REQUEST_HOST		0x0001
#define WLAN_COEX_REQUEST_NO_WINDOW	0x0100
#define WLAN_COEX_REQUEST_TX_DONE	0x0200
#define WLAN_COEX_REQUEST_TIMER		0x0400
#define WLAN_COEX_REQUEST_BT_SLEEP	0x0800

/* Control-task event bits (wlanRaiseEvent) raised and handled here. */
#define WLAN_EVENT_PS_AWAKE		3
#define WLAN_EVENT_LINK_DOWN		2
#define WLAN_EVENT_COEX_RESUME		10
#define WLAN_EVENT_COEX_TX_HOLD		11
#define WLAN_EVENT_DISCONNECTED		17
#define WLAN_EVENT_COEX_SUSPEND		22
#define WLAN_EVENT_SLEEP_REFUSED	24
#define WLAN_EVENT_COEX_REQUEST		34
#define WLAN_EVENT_COEX_GUARD		37
#define WLAN_EVENT_COEX_WINDOW		38
#define WLAN_EVENT_COEX_UPDATE		49
#define WLAN_EVENT_COEX_END_WINDOW	50
#define WLAN_EVENT_COEX_BT_WAKE		51

#define WLAN_COEX_PROFILES		2	/* 0: WLAN modes, 1: Bluetooth modes */
#define WLAN_COEX_BT_STATES		2	/* Bluetooth link idle, active */
#define WLAN_COEX_DEFAULT_GUARD		10000
#define WLAN_COEX_POLL_RETRY		3750
#define WLAN_COEX_DRIFT_LIMIT		(-100000)
#define WLAN_COEX_SLEEP_RETRY		500

/* Time kept for Bluetooth out of each period, by Bluetooth link state. */
typedef union WlanCoexReserve {
	uint16_t time[WLAN_COEX_BT_STATES];
	uint32_t word;
} WlanCoexReserve;

/* Configuration (0x56018): set by wlanCoexInit() and the host. */
typedef struct WlanCoexConfig {
	uint16_t btConfig;			/* 4 or 8 select the Bluetooth modes */
	uint16_t flags[WLAN_COEX_PROFILES];	/* bit 0: times in ms; bits 8-15: guard time */
	uint16_t padding;
	WlanCoexReserve reserve[WLAN_COEX_PROFILES];
	uint32_t period[WLAN_COEX_PROFILES];
	int32_t minimumWindow;
	int32_t minimumEstimate;
	uint32_t slack;				/* earliest cycle start before a period ends */
	uint8_t maxCredits;			/* frames released per window */
	uint8_t retryLimit;
	uint16_t macTiming;
} WlanCoexConfig;

_Static_assert(sizeof(WlanCoexConfig) == 0x28, "time-sharing configuration");

/* Running state (0x56040), all times in microseconds. */
typedef struct WlanCoex {
	int32_t period;
	int32_t window[WLAN_COEX_BT_STATES];
	int32_t wakeTime;		/* the wake timer's delay */
	int32_t sleepTime;		/* the sleep timer's delay */
	int32_t mean;			/* estimate of the time the WLAN side needs awake */
	int32_t maxSample;
	int32_t deviation;
	uint32_t maxDeviation;
	int32_t drift;			/* awake time beyond the windows, carried over */
	uint32_t pollInterval;
	uint32_t guardTime;
	uint32_t wakeTimer;
	uint32_t sleepTimer;
	uint32_t cycleTimer;
	uint32_t guardTimer;
	uint32_t pollTimer;
	uint8_t credits;		/* frames released in this window */
	int8_t outstanding;		/* frames queued and not completed */
	uint16_t padding;
	uint32_t deferred;		/* the pending event could not be armed */
	uint32_t txRequested;
	uint32_t hold;
	uint16_t request;		/* WLAN_COEX_REQUEST_* */
	uint8_t mode;			/* WLAN_COEX_MODE_* */
	uint8_t btActive;
	uint64_t lastTxTime;
	uint64_t cycleStart;
	uint32_t txHold;		/* WLAN_EVENT_COEX_TX_HOLD seen */
	uint32_t shortfall;		/* the window is shorter than the estimate */
	uint8_t btLinkLimit;
	uint8_t padding2[3];
	uint32_t wakeBits;		/* macTxAcquireWakeBit() */
} WlanCoex;

_Static_assert(sizeof(WlanCoex) == 0x78, "time-sharing state");

/* What the last mode and estimate were computed from (0x548e0), and the state. */
typedef struct WlanCoexLast {
	uint8_t mode;
	uint8_t btActive;
	uint8_t state;			/* WLAN_COEX_* */
	uint8_t padding;
	uint16_t flags;
	uint16_t padding2;
	uint32_t period;
	uint32_t reserve;
	int32_t window;
	uint32_t reserved;
	uint64_t now;
} WlanCoexLast;

_Static_assert(sizeof(WlanCoexLast) == 0x20, "time-sharing history");

extern WlanCoexConfig wlanCoexConfig;
extern WlanCoex wlanCoex;
extern WlanCoexLast wlanCoexLast;

void wlanCoexResetCounters(void);
void wlanCoexSetMode(uint32_t mode);
void wlanCoexClearCredits(void);
void wlanCoexResetEstimate(uint32_t mode, uint32_t btActive);
void wlanCoexAccountWake(void);
void wlanCoexEndBtWindow(void);
void wlanCoexWakeTimerExpired(uint32_t timer, uint32_t argument);
void wlanCoexSleepTimerExpired(uint32_t timer, uint32_t argument);
void wlanCoexReleaseTx(void);
int wlanCoexActive(void);
void wlanCoexCycleTimerExpired(uint32_t timer, uint32_t connection);
void wlanCoexGuardTimerExpired(uint32_t timer, uint32_t argument);
void wlanCoexPollTimerExpired(uint32_t timer, uint32_t argument);
void wlanCoexStartPoll(void);
void wlanCoexStartCycleTimer(struct WlanConnection *connection);
void wlanCoexCancelTimers(void);
void wlanCoexRequestSleep(void);
void wlanCoexSleepRetryExpired(uint32_t timer, uint32_t slot);
uint32_t wlanCoexComputeWindows(void);
void wlanCoexInit(void);
int wlanCoexWanted(void);
int wlanCoexHandleEvent(struct WlanConnection *connection, int event);
int wlanCoexMayTransmit(int urgent);
int wlanCoexTxAllowed(void);
int wlanCoexDeferHostSleep(void);
uint32_t wlanCoexDeferred(void);
void wlanCoexRequestTx(void);
uint32_t wlanCoexHold(void);
void wlanCoexSetHold(void);
void wlanCoexClearHold(void);
uint8_t wlanCoexRetryLimit(void);
void wlanCoexTxQueued(int urgent);
void wlanCoexTxCreditReturned(void);
int wlanCoexTxCompleted(struct WlanConnection *connection, int moreData, int result,
			int acknowledged);
int wlanCoexHoldRequested(void);
uint32_t wlanCoexBtMode(void);
uint32_t wlanCoexWindowEndRequested(void);
void wlanCoexBtActivityChanged(int busy, int scheduled);
void wlanCoexStartGuardTimer(void);
void wlanCoexProgramMacTiming(void);
void wlanCoexSetBtLinkLimit(uint8_t limit);
uint8_t wlanCoexBtLinkLimit(void);
void wlanCoexCancelCycleTimer(void);

/*
 * Not converted yet. The names say what this file uses them for and are
 * provisional until their own files are written.
 */
int wlanCoexFeatureEnabled(uint32_t features);		/* 0x23d54 */
int btCoexLinkIsActive(void);				/* 0x2453a */
int btCoexLinkIsStarting(void);				/* 0x2454e */
uint32_t btCoexProfileBit1(void);			/* 0x2450a */
uint32_t btCoexProfileBit3(void);			/* 0x2452e */
uint32_t btCoexActivityStart(void);			/* 0x24560 */
int btCoexRefreshActivityIdle(void);			/* 0x24566 */
int btCoexActivityIdle(void);				/* 0x2457a */
unsigned int btCoexActiveLinks(void);			/* 0x2459e */
unsigned int btCoexLinkCapacity(void);			/* 0x245a6 */
int wlanCoexUseStationMode(void);			/* 0x14a5e */
int wlanCoexUseP2pMode(uint32_t unused);		/* 0x14a8a */
int wlanCoexUseP2pSleepMode(uint32_t btConfig);		/* 0x14f50 */
void wlanPsEnterSleep(struct WlanConnection *connection);	/* 0x14ee0 */
void wlanPsSendPowerSaveNull(struct WlanConnection *connection); /* 0x14f2c */
void wlanPsArmRetryTimer(uint32_t delay, void (*callback)(uint32_t timer, uint32_t slot)); /* 0x143dc */
void wlanPsSetPowerState(uint8_t state);		/* 0x16fac */
int wlanPsSleepStateReached(void);			/* 0x16fc2 */
uint32_t wlanHostQueueDepth(unsigned int queue);	/* 0x1ebe8 */
int wlanCoexBtNeedsSharing(void);			/* 0x2e9ee */
void wlanPsSetBtHold(int hold);				/* 0x2eaa4 */
void wlanPsBeginBtHold(void);				/* 0x2ead0 */
int wlanPsEvaluate(struct WlanConnection *connection, int reason);	/* 0x2eb18 */
int wlanCoexUpdateEstimate(uint64_t start, uint64_t now, uint32_t maxSample, int32_t *mean,
			   uint32_t maxDeviation, int32_t *deviation);	/* 0x2ecc2 */
unsigned int wlanTxInFlightFrames(void);			/* 0x2f944 */
int wlanTxRingsPending(void);				/* 0x31fd6 */
int wlanTxRingPublishNext(void);			/* 0x320bc */
int macTxIdle(void);					/* 0xaf4e */
int macTxPoolsIdle(void);				/* 0xaf5e */
int macTxSequenceWindowOpen(uint32_t unused0, uint32_t unused1, int outstanding); /* 0xd9f8 */
extern uint32_t wlanPsSleepTimeout;			/* 0x54c4c */
extern uint16_t wlanPsSleepSequence;			/* 0x54b84 */
extern uint8_t wlanPsSleepAllowance;			/* 0x54aaf */
extern uint8_t wlanPsLastTxComplete;			/* 0x54aab */
extern uint32_t wlanPsHostBusy;				/* 0x54b08 */
extern uint8_t wlanHostSleepPending;			/* 0x04000d40 */
extern void *wlanPsCurrentState;			/* 0x04004d20 */
extern uint8_t wlanPsStateSleep[];			/* 0x04004d78 */

#endif /* ROBIN_COEX_H */
