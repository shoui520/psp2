/*
 * WLAN/Bluetooth time sharing (0x8004..0x8e8c).
 *
 * A cycle starts with WLAN_EVENT_COEX_WINDOW: the WLAN window opens, the wake
 * and sleep timers bound it, and the cycle timer starts the next one at the
 * next period boundary. In the Bluetooth modes Bluetooth decides when the
 * window ends; in the others the window length comes from the period, the
 * Bluetooth reserve and an estimate of how long the WLAN side stays awake
 * (wlanCoexUpdateEstimate()), corrected by the drift carried over from the
 * cycles before.
 */
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/dmaring.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/txring.h"

#define MAC_010_QUEUE_HEAD	MAC_REG(0x010)
#define MAC_014_QUEUE_TAIL	MAC_REG(0x014)
#define MAC_730_TIMING_HIGH	MAC_REG(0x730)
#define MAC_7F4_TIMING_LOW	MAC_REG(0x7f4)

WlanCoexConfig wlanCoexConfig __attribute__((section(".itcm.bss")));
WlanCoex wlanCoex __attribute__((section(".itcm.bss")));
WlanCoexLast wlanCoexLast __attribute__((section(".itcm.rw"))) = {
	.btActive = 0xff,
};

static inline int wlanCoexIsBtMode(unsigned int mode)
{
	return (mode & WLAN_COEX_MODE_BT) != 0;
}

void wlanCoexResetCounters(void)
{
	wlanCoex.credits = 0;
	wlanCoex.outstanding = 0;
	wlanCoex.txRequested = 0;
	wlanCoex.deferred = 0;
	wlanCoex.hold = 0;
}

/* Computes the windows of a mode from its profile, unless nothing changed. */
void wlanCoexSetMode(uint32_t mode)
{
	UINT posture = sysIrqDisable();
	int profile = mode == WLAN_COEX_MODE_BT_QUARTER || mode == WLAN_COEX_MODE_BT_HALF;

	if (wlanCoex.mode != mode ||
	    wlanCoexConfig.period[profile] != wlanCoexLast.period ||
	    wlanCoexConfig.reserve[profile].word != wlanCoexLast.reserve ||
	    wlanCoexConfig.flags[profile] != wlanCoexLast.flags) {
		unsigned int flags = wlanCoexConfig.flags[profile];
		int unit = (flags & 1) ? 1000 : 1;
		WlanCoexReserve reserve = wlanCoexConfig.reserve[profile];
		uint32_t period = wlanCoexConfig.period[profile];

		wlanCoex.window[0] = (period - reserve.time[0]) * unit;
		if (reserve.time[1] == 0) {
			if (mode == WLAN_COEX_MODE_BT_QUARTER)
				reserve.time[1] = 10000 * period / 40000;
			else if (mode == WLAN_COEX_MODE_BT_HALF)
				reserve.time[1] = 20000 * period / 40000;
			else
				reserve.time[1] = 4000 * period / 20000;
		}
		wlanCoex.period = period * unit;
		wlanCoex.window[1] = (period - reserve.time[1]) * unit;
		if (wlanCoexIsBtMode(mode))
			wlanCoex.guardTime = (flags >> 8) ? (flags >> 8) * unit : WLAN_COEX_DEFAULT_GUARD;

		wlanCoex.btLinkLimit = 4;
		wlanCoexResetCounters();
		wlanTxRingPublishLimit = 1;
		wlanCoex.mode = mode;
		wlanCoexLast.period = wlanCoexConfig.period[profile];
		wlanCoexLast.reserve = wlanCoexConfig.reserve[profile].word;
		wlanCoexLast.flags = wlanCoexConfig.flags[profile];
	}
	sysIrqRestore(posture);
}

void wlanCoexClearCredits(void)
{
	wlanCoex.credits = 0;
	wlanCoex.outstanding = 0;
}

/* Starts the awake-time estimate over when the mode or the window changed. */
void wlanCoexResetEstimate(uint32_t mode, uint32_t btActive)
{
	int32_t window = wlanCoex.window[btActive];

	if (btActive != wlanCoexLast.btActive || mode != wlanCoexLast.mode ||
	    window != wlanCoexLast.window || wlanCoex.maxSample == 0) {
		int32_t sample = (window - wlanCoexConfig.minimumWindow) / 2;
		int32_t deviation;

		if (sample <= wlanCoexConfig.minimumEstimate)
			sample = wlanCoexConfig.minimumEstimate;
		wlanCoex.maxSample = sample;
		deviation = sample / 3;
		wlanCoex.drift = 0;
		wlanCoex.maxDeviation = deviation;
		wlanCoex.shortfall = 0;
		wlanCoex.mean = sample;
		wlanCoex.deviation = deviation;
		wlanCoexLast.window = wlanCoex.window[btActive];
		wlanCoexLast.btActive = btActive;
		wlanCoexLast.mode = mode;
	}
}

/* The window closed: carries the time the WLAN side stayed awake past it over. */
void wlanCoexAccountWake(void)
{
	int32_t elapsed;
	int32_t limit;

	if (wlanCoexLast.state == WLAN_COEX_IDLE)
		return;
	sysEnablePendingEvent();
	if (wlanCoexIsBtMode(wlanCoex.mode))
		return;

	wlanCoexLast.now = osaGetHardwareTime();
	elapsed = (int32_t)((uint32_t)wlanCoexLast.now - (uint32_t)wlanCoex.cycleStart) -
		  wlanCoex.wakeTime;
	limit = wlanCoex.sleepTime - wlanCoex.wakeTime;
	if (limit >= elapsed)
		elapsed = limit;
	limit = wlanCoex.period - wlanCoex.wakeTime;
	if (limit <= elapsed)
		elapsed = limit;
	wlanCoex.drift += elapsed;
	if (wlanCoex.drift <= WLAN_COEX_DRIFT_LIMIT)
		wlanCoex.drift = WLAN_COEX_DRIFT_LIMIT;
}

void wlanCoexEndBtWindow(void)
{
	if (wlanCoexFeatureEnabled(16)) {
		if (wlanCoex.request >> 8)
			wlanPsSetPowerState(3);
		return;
	}
	wlanPsSleepTimeout = 0;
	wlanPsSleepSequence = 0;
	wlanPsSetPowerState(3);
	wlanRaiseEvent(WLAN_EVENT_PS_AWAKE);
}

void wlanCoexWakeTimerExpired(uint32_t timer, uint32_t argument)
{
	(void)argument;

	if (wlanCoex.wakeTimer != timer)
		return;
	wlanCoex.wakeTimer = 0;
	if (wlanCoex.mode == WLAN_COEX_MODE_STATION || wlanCoex.mode == WLAN_COEX_MODE_P2P) {
		wlanCoex.request = WLAN_COEX_REQUEST_TIMER;
		wlanRaiseEvent(WLAN_EVENT_COEX_REQUEST);
	} else {
		wlanCoexAccountWake();
		wlanCoexLast.state = WLAN_COEX_IDLE;
	}
}

void wlanCoexSleepTimerExpired(uint32_t timer, uint32_t argument)
{
	(void)argument;

	if (wlanCoex.sleepTimer != timer)
		return;
	wlanCoex.sleepTimer = 0;
	wlanCoexLast.state = WLAN_COEX_BT_WINDOW;
	if (wlanCoexIsBtMode(wlanCoex.mode)) {
		wlanCoex.request = WLAN_COEX_REQUEST_TIMER;
		wlanRaiseEvent(WLAN_EVENT_COEX_REQUEST);
	}
}

/* Releases one shadowed frame to the hardware while the window has credits. */
void wlanCoexReleaseTx(void)
{
	UINT posture;

	if (wlanCoexIsBtMode(wlanCoex.mode) || wlanCoexLast.state != WLAN_COEX_WLAN_WINDOW ||
	    wlanCoex.credits >= wlanCoexConfig.maxCredits)
		return;

	posture = sysIrqDisable();
	if (wlanTxRingPublishNext()) {
		++wlanCoex.credits;
		if (wlanCoex.mode == WLAN_COEX_MODE_P2P || wlanCoex.mode == WLAN_COEX_MODE_P2P_SLEEP)
			wlanCoex.lastTxTime = osaGetHardwareTime();
	}
	sysIrqRestore(posture);
}

int wlanCoexActive(void)
{
	return wlanCoexLast.state != WLAN_COEX_OFF;
}

/* A period boundary: asks the power-save code whether the next window may open. */
void wlanCoexCycleTimerExpired(uint32_t timer, uint32_t connection)
{
	struct WlanConnection *link = (struct WlanConnection *)connection;

	if (wlanCoex.cycleTimer != timer)
		return;
	wlanCoex.cycleTimer = 0;
	if (wlanCoexIsBtMode(wlanCoex.mode)) {
		wlanCoexEndBtWindow();
		return;
	}
	if (!wlanPsSleepStateReached() || !wlanPsEvaluate(link, 2) || !wlanCoexActive())
		return;

	if (wlanCoex.mode == WLAN_COEX_MODE_DEFAULT) {
		wlanCoexReleaseTx();
		if (wlanCoex.txHold)
			wlanPsEnterSleep(link);
	} else if (wlanCoex.mode == WLAN_COEX_MODE_P2P_SLEEP) {
		if (wlanTxRingsPending())
			wlanCoexReleaseTx();
		else if (wlanCoex.txHold)
			wlanPsSendPowerSaveNull(link);
	}
}

void wlanCoexGuardTimerExpired(uint32_t timer, uint32_t argument)
{
	(void)argument;

	if (wlanCoex.guardTimer != timer)
		return;
	wlanCoex.guardTimer = 0;
	wlanRaiseEvent(WLAN_EVENT_COEX_GUARD);
}

/* Re-arms the pending event while Bluetooth holds the antenna. */
void wlanCoexPollTimerExpired(uint32_t timer, uint32_t argument)
{
	(void)argument;

	if (wlanCoex.pollTimer != timer)
		return;
	wlanCoex.pollTimer = 0;
	if (sysReadPendingEventEnable()) {
		if (sysDeferPendingEvent(1))
			osaCreateTimer(wlanCoexPollTimerExpired, 0, wlanCoex.pollInterval,
				       &wlanCoex.pollTimer, 8);
	} else {
		sysEnablePendingEvent();
		osaCreateTimer(wlanCoexPollTimerExpired, 0, WLAN_COEX_POLL_RETRY,
			       &wlanCoex.pollTimer, 8);
	}
}

void wlanCoexStartPoll(void)
{
	osaCreateTimer(wlanCoexPollTimerExpired, 0, wlanCoex.pollInterval, &wlanCoex.pollTimer, 8);
}

/* Arms the cycle timer for the next period boundary of the MAC timer (TU). */
void wlanCoexStartCycleTimer(struct WlanConnection *connection)
{
	uint32_t period = (wlanCoex.period << 10) / 1000;
	uint32_t phase = romGetTimerTimestamp() % period;
	uint32_t delay = period - phase;

	if (wlanCoexConfig.slack < phase)
		delay += period;
	osaCreateTimer(wlanCoexCycleTimerExpired, (uint32_t)connection, delay,
		       &wlanCoex.cycleTimer, 0);
}

/* Cancels every timer but the cycle timer. */
void wlanCoexCancelTimers(void)
{
	UINT posture = sysIrqDisable();

	if (wlanCoex.wakeTimer) {
		osaCancelTimer(wlanCoex.wakeTimer);
		wlanCoex.wakeTimer = 0;
	}
	if (wlanCoex.sleepTimer) {
		osaCancelTimer(wlanCoex.sleepTimer);
		wlanCoex.sleepTimer = 0;
	}
	if (wlanCoex.guardTimer) {
		osaCancelTimer(wlanCoex.guardTimer);
		wlanCoex.guardTimer = 0;
	}
	if (wlanCoex.pollTimer) {
		osaCancelTimer(wlanCoex.pollTimer);
		wlanCoex.pollTimer = 0;
	}
	sysIrqRestore(posture);
}

/*
 * Puts the WLAN side to sleep when nothing is left to send, or tells the
 * control task it cannot; retries every WLAN_COEX_SLEEP_RETRY ticks.
 */
void wlanCoexRequestSleep(void)
{
	UINT posture = sysIrqDisable();
	int outstanding = wlanTxInFlightFrames();
	int idle;
	int i;

	if (outstanding)
		for (i = 0; i < WLAN_TX_RING_SHADOWED; i++)
			outstanding -= wlanTxRingPending[i];
	sysIrqRestore(posture);

	idle = macTxIdle() && MAC_010_QUEUE_HEAD == MAC_014_QUEUE_TAIL && outstanding == 0 &&
	       wlanHostQueueDepth(0) == 0 && wlanHostQueueDepth(0) == 0 &&
	       wlanHostSleepPending == 0;
	if ((idle == 1 && wlanDmaRingsHaveRoom() != 1) || wlanPsHostBusy != 0 ||
	    wlanPsSleepAllowance == 0) {
		wlanRaiseEvent(WLAN_EVENT_SLEEP_REFUSED);
		return;
	}
	macTxPostEvent(1);
	--wlanPsSleepAllowance;
	wlanPsArmRetryTimer(WLAN_COEX_SLEEP_RETRY, wlanCoexSleepRetryExpired);
}

void wlanCoexSleepRetryExpired(uint32_t timer, uint32_t slot)
{
	uint32_t *armed = (uint32_t *)slot;

	if (*armed != timer)
		return;
	*armed = 0;
	wlanCoexRequestSleep();
}

/*
 * Lays out the next window; returns 1 (shortfall) when it cannot hold the
 * estimated awake time.
 */
uint32_t wlanCoexComputeWindows(void)
{
	unsigned int mode = wlanCoex.mode;
	unsigned int btActive = wlanCoex.btActive;
	int32_t window;
	int32_t length;
	int32_t limit;

	if (wlanCoexIsBtMode(mode)) {
		if (btActive == 1) {
			if (mode == WLAN_COEX_MODE_BT_QUARTER) {
				unsigned int links = btCoexActiveLinks();
				unsigned int capacity = btCoexLinkCapacity();

				wlanCoex.btLinkLimit = capacity;
				if (links <= capacity && !btCoexRefreshActivityIdle()) {
					wlanCoex.shortfall = 1;
					return wlanCoex.shortfall;
				}
				window = wlanCoex.window[1];
			} else if (mode == WLAN_COEX_MODE_BT_HALF) {
				if (btCoexActivityStart() != 0 && !btCoexActivityIdle()) {
					wlanCoex.shortfall = 1;
					return wlanCoex.shortfall;
				}
				wlanCoex.pollInterval = wlanCoex.window[1];
				wlanCoex.shortfall = 0;
				return wlanCoex.shortfall;
			} else {
				return wlanCoex.shortfall;
			}
		} else if (btActive == 0) {
			if (btCoexLinkIsStarting()) {
				wlanCoex.shortfall = 1;
				return wlanCoex.shortfall;
			}
			window = wlanCoex.window[0];
		} else {
			return wlanCoex.shortfall;
		}
		length = window - (int32_t)wlanCoex.guardTime;
		wlanCoex.sleepTime = length;
		if (length <= wlanCoexConfig.minimumWindow)
			length = wlanCoexConfig.minimumWindow;
		wlanCoex.sleepTime = length;
		wlanCoex.shortfall = 0;
		return wlanCoex.shortfall;
	}

	if (btActive == 1) {
		unsigned int links = btCoexActiveLinks();
		unsigned int capacity = btCoexLinkCapacity();

		wlanCoex.btLinkLimit = capacity;
		if (links <= capacity && !btCoexRefreshActivityIdle()) {
			wlanCoex.shortfall = 1;
			return wlanCoex.shortfall;
		}
		wlanCoex.wakeTime = wlanCoex.window[1];
		wlanCoex.sleepTime = wlanCoex.window[1] - (wlanCoex.mean + 3 * wlanCoex.deviation);
		wlanCoex.shortfall = 0;
		return wlanCoex.shortfall;
	}

	if (btCoexLinkIsStarting()) {
		wlanCoex.drift = 0;
		wlanCoex.shortfall = 1;
		return wlanCoex.shortfall;
	}

	window = btCoexProfileBit3() ? wlanCoexConfig.minimumWindow : wlanCoex.window[0];
	length = window - wlanCoex.drift;
	wlanCoex.wakeTime = length;
	limit = wlanCoex.period - (int32_t)wlanCoexConfig.slack;
	if (limit <= length)
		length = limit;
	wlanCoex.wakeTime = length;
	wlanCoex.sleepTime = length - (wlanCoex.mean + 3 * wlanCoex.deviation);
	if (wlanCoex.sleepTime >= wlanCoexConfig.minimumWindow) {
		wlanCoex.drift = wlanCoex.drift + length - window;
		wlanCoex.shortfall = 0;
	} else {
		wlanCoex.drift = wlanCoex.drift - window;
		wlanCoex.shortfall = 1;
	}
	return wlanCoex.shortfall;
}

void wlanCoexInit(void)
{
	__aeabi_memclr4(&wlanCoexConfig, sizeof(wlanCoexConfig));
	__aeabi_memclr4(&wlanCoex, sizeof(wlanCoex));
	wlanCoexConfig.btConfig = 2;
	wlanCoexConfig.period[0] = 20000;
	wlanCoexConfig.reserve[0].time[0] = 10000;
	wlanCoexConfig.reserve[0].time[1] = 4000;
	wlanCoexConfig.flags[0] = 0;
	wlanCoexConfig.period[1] = 40000;
	wlanCoexConfig.minimumWindow = 1000;
	wlanCoexConfig.reserve[1].time[0] = 25000;
	wlanCoexConfig.reserve[1].time[1] = 10000;
	wlanCoexConfig.minimumEstimate = 4500;
	wlanCoexConfig.slack = 2000;
	wlanCoexConfig.maxCredits = 1;
	wlanCoexConfig.retryLimit = 3;
	wlanCoexConfig.macTiming = 1000;
	wlanCoexLast.state = WLAN_COEX_OFF;
}

/* Time sharing is needed: Bluetooth wants it and its profile does not forbid it. */
int wlanCoexWanted(void)
{
	return wlanCoexBtNeedsSharing() && btCoexProfileBit1() == 0;
}

/* Stops the cycle and the pending event. */
static inline void wlanCoexStopCycle(void)
{
	wlanCoexCancelTimers();
	wlanCoexCancelCycleTimer();
	sysCompletePendingEvent();
	wlanCoex.deferred = 0;
}

static inline uint32_t wlanCoexSelectMode(void)
{
	if (wlanCoexUseStationMode())
		return WLAN_COEX_MODE_STATION;
	if (wlanCoexUseP2pMode(0))
		return WLAN_COEX_MODE_P2P;
	if (wlanCoexConfig.btConfig == 4)
		return WLAN_COEX_MODE_BT_QUARTER;
	if (wlanCoexConfig.btConfig == 8)
		return WLAN_COEX_MODE_BT_HALF;
	if (wlanCoexUseP2pSleepMode(wlanCoexConfig.btConfig))
		return WLAN_COEX_MODE_P2P_SLEEP;
	return WLAN_COEX_MODE_DEFAULT;
}

/* The Bluetooth half mode polls Bluetooth while its link is active and idle. */
static inline void wlanCoexStartBtPoll(void)
{
	if (wlanCoex.mode == WLAN_COEX_MODE_BT_HALF && wlanCoex.btActive == 1 &&
	    btCoexActivityIdle())
		osaCreateTimer(wlanCoexPollTimerExpired, 0, wlanCoex.pollInterval,
			       &wlanCoex.pollTimer, 8);
}

/* Bluetooth decides when the window ends, except in the quarter mode with an active link. */
static inline int wlanCoexBtEndsWindow(void)
{
	return wlanCoex.btActive == 0 ||
	       (wlanCoex.btActive == 1 && wlanCoex.mode == WLAN_COEX_MODE_BT_QUARTER);
}

/* After a window opened: the cycle timer runs in the WLAN-timed modes. */
static inline void wlanCoexFinishWindow(struct WlanConnection *connection)
{
	unsigned int mode = wlanCoex.mode;

	if (mode == WLAN_COEX_MODE_P2P_SLEEP || mode == WLAN_COEX_MODE_DEFAULT ||
	    (wlanCoexIsBtMode(mode) && wlanCoexBtEndsWindow()))
		wlanCoexStartCycleTimer(connection);
}

static inline void wlanCoexAcquireWake(void)
{
	if (wlanCoex.wakeBits == 0)
		wlanCoex.wakeBits = macTxAcquireWakeBit();
}

static inline void wlanCoexReleaseWake(void)
{
	if (wlanCoex.wakeBits != 0)
		macTxReleaseWakeBits(&wlanCoex.wakeBits);
}

static inline int wlanCoexUpdate(struct WlanConnection *connection)
{
	unsigned int oldMode;
	UINT posture;

	if (!wlanCoexWanted()) {
		if (wlanCoexLast.state != WLAN_COEX_OFF) {
			wlanCoexStopCycle();
			sysEnablePendingEvent();
			wlanCoexReleaseWake();
			wlanCoexLast.state = WLAN_COEX_OFF;
			if (wlanCoexIsBtMode(wlanCoex.mode))
				wlanCoexEndBtWindow();
		}
		return 1;
	}

	oldMode = wlanCoex.mode;
	wlanCoexSetMode(wlanCoexSelectMode());
	if (wlanCoexLast.state != WLAN_COEX_OFF) {
		if (wlanCoex.mode != oldMode) {
			if (wlanCoexIsBtMode(wlanCoex.mode)) {
				if (!wlanCoexIsBtMode(oldMode)) {
					wlanCoexAcquireWake();
					wlanPsBeginBtHold();
				}
			} else if (wlanCoexIsBtMode(oldMode)) {
				wlanPsSetBtHold(1);
				wlanCoexReleaseWake();
			}
		}
	} else {
		if (wlanCoexIsBtMode(wlanCoex.mode)) {
			wlanCoexAcquireWake();
			wlanPsBeginBtHold();
		} else {
			wlanPsSetBtHold(1);
		}
		wlanCoexLast.state = WLAN_COEX_IDLE;
	}

	wlanCoex.request = 0;
	wlanCoexStopCycle();
	wlanCoexResetCounters();
	wlanCoex.btActive = btCoexLinkIsActive();
	if (!wlanCoexIsBtMode(wlanCoex.mode))
		wlanCoexResetEstimate(wlanCoex.mode, wlanCoex.btActive);

	posture = sysIrqDisable();
	if (wlanCoexComputeWindows()) {
		wlanCoexLast.state = WLAN_COEX_IDLE;
		sysEnablePendingEvent();
		if (wlanCoex.mode == WLAN_COEX_MODE_STATION || wlanCoex.mode == WLAN_COEX_MODE_P2P ||
		    wlanCoexIsBtMode(wlanCoex.mode)) {
			wlanCoex.request = WLAN_COEX_REQUEST_NO_WINDOW;
			wlanRaiseEvent(WLAN_EVENT_COEX_REQUEST);
		}
	} else if (sysDeferPendingEvent(0)) {
		wlanCoexStartBtPoll();
		wlanCoex.cycleStart = osaGetHardwareTime();
		wlanCoexLast.state = WLAN_COEX_WLAN_WINDOW;
		if (wlanCoexIsBtMode(wlanCoex.mode)) {
			macTxSetPowerSave(0);
			wlanCoexReleaseWake();
			if (wlanCoexBtEndsWindow())
				osaCreateTimer(wlanCoexSleepTimerExpired, 0, wlanCoex.sleepTime,
					       &wlanCoex.sleepTimer, 8);
		} else {
			osaCreateTimer(wlanCoexWakeTimerExpired, 0, wlanCoex.wakeTime,
				       &wlanCoex.wakeTimer, 8);
			osaCreateTimer(wlanCoexSleepTimerExpired, 0, wlanCoex.sleepTime,
				       &wlanCoex.sleepTimer, 8);
		}
	} else {
		wlanCoex.deferred = 1;
		sysIrqRestore(posture);
		return 0;
	}
	wlanCoexFinishWindow(connection);
	sysIrqRestore(posture);
	return 1;
}

static inline int wlanCoexOpenWindow(struct WlanConnection *connection)
{
	UINT posture = sysIrqDisable();
	int pending;

	wlanCoex.deferred = 0;
	if (!wlanCoexActive()) {
		sysIrqRestore(posture);
		return 0;
	}
	wlanCoexStartBtPoll();
	wlanCoex.cycleStart = osaGetHardwareTime();
	wlanCoexLast.state = WLAN_COEX_WLAN_WINDOW;
	if (wlanCoexIsBtMode(wlanCoex.mode)) {
		macTxSetPowerSave(0);
		wlanCoexReleaseWake();
		if (wlanCoexBtEndsWindow())
			osaCreateTimer(wlanCoexSleepTimerExpired, 0, wlanCoex.sleepTime,
				       &wlanCoex.sleepTimer, 8);
	} else {
		osaCreateTimer(wlanCoexWakeTimerExpired, 0, wlanCoex.wakeTime,
			       &wlanCoex.wakeTimer, 8);
		osaCreateTimer(wlanCoexSleepTimerExpired, 0, wlanCoex.sleepTime,
			       &wlanCoex.sleepTimer, 8);
		if (wlanCoex.mode == WLAN_COEX_MODE_DEFAULT) {
			wlanCoexReleaseTx();
			if (wlanCoex.txHold)
				wlanPsEnterSleep(connection);
		} else if (wlanCoex.mode == WLAN_COEX_MODE_P2P_SLEEP ||
			   wlanCoex.mode == WLAN_COEX_MODE_P2P) {
			if (wlanTxRingsPending()) {
				wlanCoexReleaseTx();
			} else {
				pending = wlanCoex.mode == WLAN_COEX_MODE_P2P_SLEEP ? (int)wlanCoex.txHold
										   : (int)wlanCoex.txRequested;
				if (pending)
					wlanPsSendPowerSaveNull(connection);
			}
			wlanCoex.txRequested = 0;
		}
	}
	wlanCoexFinishWindow(connection);
	sysIrqRestore(posture);
	return 1;
}

static inline void wlanCoexEndWindow(void)
{
	unsigned int mode;

	wlanCoexCancelTimers();
	mode = wlanCoex.mode;
	if (mode == WLAN_COEX_MODE_STATION || mode == WLAN_COEX_MODE_P2P) {
		wlanCoex.request = WLAN_COEX_REQUEST_HOST;
		return;
	}
	if (mode == WLAN_COEX_MODE_DEFAULT || mode == WLAN_COEX_MODE_P2P_SLEEP) {
		wlanCoexCancelCycleTimer();
		sysCompletePendingEvent();
		wlanCoex.deferred = 0;
		wlanCoexAccountWake();
		wlanCoexLast.state = WLAN_COEX_IDLE;
		wlanCoex.txHold = 0;
		if (wlanCoex.request >> 8)
			return;
	} else {
		if (!wlanCoexIsBtMode(mode))
			return;
		if (wlanCoex.request >> 8) {
			wlanCoexAcquireWake();
			if (wlanCoexLast.state == WLAN_COEX_WLAN_WINDOW)
				wlanCoexLast.state = WLAN_COEX_BT_WINDOW;
			return;
		}
	}
	wlanCoex.request = WLAN_COEX_REQUEST_HOST;
}

/* The control task's events; returns 0 when the pending event could not be armed. */
int wlanCoexHandleEvent(struct WlanConnection *connection, int event)
{
	if (!wlanCoexActive() && event != WLAN_EVENT_COEX_UPDATE)
		return 1;

	switch (event) {
	case WLAN_EVENT_SLEEP_REFUSED:
		wlanCoexAccountWake();
		wlanCoexLast.state = WLAN_COEX_IDLE;
		break;
	case WLAN_EVENT_COEX_TX_HOLD:
		wlanCoex.txHold = 1;
		break;
	case WLAN_EVENT_LINK_DOWN:
	case WLAN_EVENT_DISCONNECTED:
		wlanCoexReleaseWake();
		wlanCoexStopCycle();
		sysEnablePendingEvent();
		wlanCoexLast.state = WLAN_COEX_OFF;
		break;
	case WLAN_EVENT_COEX_RESUME:
		sysEnablePendingEvent();
		wlanCoexLast.state = WLAN_COEX_IDLE;
		break;
	case WLAN_EVENT_COEX_SUSPEND:
		wlanCoexStopCycle();
		break;
	case WLAN_EVENT_COEX_WINDOW:
		return wlanCoexOpenWindow(connection);
	case WLAN_EVENT_COEX_UPDATE:
		return wlanCoexUpdate(connection);
	case WLAN_EVENT_COEX_END_WINDOW:
		wlanCoexEndWindow();
		break;
	case WLAN_EVENT_COEX_BT_WAKE:
		if (wlanCoexIsBtMode(wlanCoex.mode)) {
			wlanCoexAccountWake();
			wlanCoexLast.state = WLAN_COEX_IDLE;
		}
		break;
	default:
		break;
	}
	return 1;
}

/* A frame may go out now (urgent frames ignore the outstanding count). */
int wlanCoexMayTransmit(int urgent)
{
	if (!wlanCoexActive())
		return 1;
	if (wlanCoexLast.state != WLAN_COEX_WLAN_WINDOW)
		return 0;
	if (wlanCoex.outstanding >= 1 && urgent == 0)
		return 0;
	return macTxSequenceWindowOpen(0, 0, wlanCoex.outstanding) != 0;
}

int wlanCoexTxAllowed(void)
{
	return !wlanCoexActive() || wlanCoexLast.state == WLAN_COEX_WLAN_WINDOW;
}

int wlanCoexDeferHostSleep(void)
{
	return wlanCoexActive() && wlanCoexIsBtMode(wlanCoex.mode) && !macTxPoolsIdle() &&
	       wlanCoex.request != WLAN_COEX_REQUEST_NO_WINDOW;
}

uint32_t wlanCoexDeferred(void)
{
	return wlanCoex.deferred;
}

void wlanCoexRequestTx(void)
{
	wlanCoex.txRequested = 1;
}

uint32_t wlanCoexHold(void)
{
	return wlanCoex.hold;
}

void wlanCoexSetHold(void)
{
	wlanCoex.hold = 1;
}

void wlanCoexClearHold(void)
{
	wlanCoex.hold = 0;
}

uint8_t wlanCoexRetryLimit(void)
{
	return wlanCoexConfig.retryLimit;
}

void wlanCoexTxQueued(int urgent)
{
	if (!wlanCoexActive() || urgent != 0)
		return;
	wlanCoex.lastTxTime = osaGetHardwareTime();
	++wlanCoex.outstanding;
}

void wlanCoexTxCreditReturned(void)
{
	if (!wlanCoexActive())
		return;
	if (wlanCoex.mode == WLAN_COEX_MODE_STATION || wlanCoex.mode == WLAN_COEX_MODE_DEFAULT) {
		--wlanCoex.credits;
		wlanCoexReleaseTx();
	}
}

/* Ends a WLAN-timed window early when the WLAN side went idle. */
static inline void wlanCoexCloseIdleWindow(unsigned int raisingMode)
{
	if (wlanCoexLast.state != WLAN_COEX_BT_WINDOW)
		return;
	if (wlanCoex.wakeTimer) {
		osaCancelTimer(wlanCoex.wakeTimer);
		wlanCoex.wakeTimer = 0;
	}
	if (wlanCoex.mode == raisingMode) {
		wlanCoex.request = WLAN_COEX_REQUEST_TX_DONE;
		wlanRaiseEvent(WLAN_EVENT_COEX_REQUEST);
	} else {
		wlanCoexAccountWake();
		wlanCoexLast.state = WLAN_COEX_IDLE;
	}
}

/* A frame of the window completed; returns result, or 0 once the window may end. */
int wlanCoexTxCompleted(struct WlanConnection *connection, int moreData, int result,
			int acknowledged)
{
	UINT posture;

	if (!wlanCoexActive())
		return result;

	if (wlanCoex.mode == WLAN_COEX_MODE_STATION || wlanCoex.mode == WLAN_COEX_MODE_DEFAULT) {
		uint64_t now = osaGetHardwareTime();

		wlanCoexUpdateEstimate(wlanCoex.lastTxTime, now, wlanCoex.maxSample, &wlanCoex.mean,
				       wlanCoex.maxDeviation, &wlanCoex.deviation);
		--wlanCoex.outstanding;
		posture = sysIrqDisable();
		wlanCoexCloseIdleWindow(WLAN_COEX_MODE_STATION);
		sysIrqRestore(posture);
		return result;
	}

	if ((wlanCoex.mode == WLAN_COEX_MODE_P2P || wlanCoex.mode == WLAN_COEX_MODE_P2P_SLEEP) &&
	    acknowledged != 0 && !wlanPsDisabled() && wlanDmaRingsHaveRoom() != 1) {
		uint64_t now = osaGetHardwareTime();

		wlanCoexUpdateEstimate(wlanCoex.lastTxTime, now, wlanCoex.maxSample, &wlanCoex.mean,
				       wlanCoex.maxDeviation, &wlanCoex.deviation);
		--wlanCoex.credits;
		wlanPsLastTxComplete = moreData == 0;
		if (result != 0 && moreData == 0)
			return result;

		result = 0;
		posture = sysIrqDisable();
		if (wlanCoexLast.state == WLAN_COEX_BT_WINDOW) {
			wlanCoexCloseIdleWindow(WLAN_COEX_MODE_P2P);
			sysIrqRestore(posture);
			return result;
		}
		sysIrqRestore(posture);
		if (wlanPsSleepStateReached() && wlanPsHostBusy == 0) {
			if (wlanTxRingsPending())
				wlanCoexReleaseTx();
			else if (moreData != 0)
				wlanPsSendPowerSaveNull(connection);
		}
	}
	return result;
}

int wlanCoexHoldRequested(void)
{
	return wlanCoexActive() && wlanCoex.request != WLAN_COEX_REQUEST_HOST;
}

uint32_t wlanCoexBtMode(void)
{
	return wlanCoex.mode & WLAN_COEX_MODE_BT;
}

uint32_t wlanCoexWindowEndRequested(void)
{
	return wlanCoex.request & 0xff00;
}

void wlanCoexBtActivityChanged(int busy, int scheduled)
{
	UINT posture = sysIrqDisable();

	if (wlanCoexActive() && wlanCoex.mode == WLAN_COEX_MODE_BT_HALF && wlanCoex.btActive == 1) {
		if (busy != 0 && scheduled != 0) {
			if (wlanPsCurrentState == wlanPsStateSleep) {
				wlanCoex.request = WLAN_COEX_REQUEST_BT_SLEEP;
				wlanRaiseEvent(WLAN_EVENT_COEX_REQUEST);
			}
		} else {
			wlanCoexEndBtWindow();
		}
	}
	sysIrqRestore(posture);
}

void wlanCoexStartGuardTimer(void)
{
	osaCreateTimer(wlanCoexGuardTimerExpired, 0, wlanCoex.guardTime, &wlanCoex.guardTimer, 8);
}

void wlanCoexProgramMacTiming(void)
{
	MAC_7F4_TIMING_LOW = (MAC_7F4_TIMING_LOW >> 16 << 16) | wlanCoexConfig.macTiming;
	MAC_730_TIMING_HIGH = (MAC_730_TIMING_HIGH & 0xffff) | ((uint32_t)wlanCoexConfig.macTiming << 16);
}

void wlanCoexSetBtLinkLimit(uint8_t limit)
{
	wlanCoex.btLinkLimit = limit;
}

uint8_t wlanCoexBtLinkLimit(void)
{
	return wlanCoex.btLinkLimit;
}

void wlanCoexCancelCycleTimer(void)
{
	if (wlanCoex.cycleTimer) {
		osaCancelTimer(wlanCoex.cycleTimer);
		wlanCoex.cycleTimer = 0;
	}
}
