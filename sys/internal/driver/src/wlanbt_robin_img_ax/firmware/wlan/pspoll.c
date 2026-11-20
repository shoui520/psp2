/*
 * Power-save polling of a station (0x8e90..0x994a). A sleeping station
 * fetches the frames its access point holds for it with PS-Poll frames, one
 * frame per poll. A poll is "pending" from the moment it is decided until
 * the answer (or the timeout) ends it. Either the MAC times the answer
 * itself (wlanPsPollUsesMacTimer(), MAC_7F4 bit 31) or the retry and event
 * timers here do. With Bluetooth active, polls go between its slots: the
 * interval and offset come from the Bluetooth schedule, and the answer time
 * is estimated (wlanCoexUpdateEstimate()) to keep the timeout short.
 * The filename is inferred; no source filename survives.
 */
#include "robin/btrom.h"
#include "robin/stapssm.h"
#include "robin/coex.h"
#include "robin/conn.h"
#include "robin/psctl.h"
#include "robin/dmaring.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlantask.h"

/* Zero-initialised at load; the addresses are the original's. */
uint8_t wlanTxDataRing __attribute__((section(".sqram1.bss")));				/* 0xc0011bd4 */

#define MAC_PS_POLL_TIMER_ENABLE (1u << 31)	/* MAC_7F4_TIMING_LOW */

#define PS_POLL_COUNTDOWN	30
#define PS_POLL_DEFAULT_INTERVAL 3750
#define PS_POLL_DEFAULT_OFFSET	1250

WlanPsPoll wlanPsPoll __attribute__((section(".itcm.rw"))) = {
	.retryLimit = 8,
	.retryDelay = 500,
	.eventDelay = 2400,
	.triggerDelay = 2400,
	.timeoutDelay = 18000,
};
WlanPsPollTiming wlanPsPollTiming __attribute__((section(".itcm.bss")));

/* 802.11 frame type (0 management, 1 control, 2 data) of the frame a transmit descriptor points to. */
static inline unsigned int wlanTxFrameType(const uint8_t *data)
{
	return (((const struct WlanReplyFrame *)data)->frame[2] >> 2) & 3;
}

static inline int btLinkBusy(unsigned int state)
{
	return state == BT_STATE_BUSY_A || state == BT_STATE_BUSY_B;
}

/* Waits while Bluetooth holds the air in one of its busy states. */
static inline void wlanPsPollWaitBluetooth(void)
{
	unsigned int state = BT_LINK_STATE & 0xf;

	while ((SOC_PENDING_EVENT_CONTROL & SOC_PENDING_EVENT_ARMED) != 0 &&
	       sysPendingEventPolicyIsEnabled() != 0 &&
	       (SOC_PENDING_EVENT_STATUS & SOC_PENDING_EVENT_BT) != 0 &&
	       (state == BT_STATE_BUSY_A || state == BT_STATE_BUSY_B))
		;
}

/* Lets the trigger timer poll once; 1 if it already has. */
int wlanPsPollTriggerOnce(void)
{
	UINT posture = sysIrqDisable();

	if (wlanPsPoll.triggered != 0) {
		sysIrqRestore(posture);
		return 1;
	}
	wlanPsPoll.triggered = 1;
	sysIrqRestore(posture);
	return 0;
}

/* Programs the interval and the shortest timeout, and restarts the estimate. */
void wlanPsPollProgramTimeout(void)
{
	MAC_7F4_TIMING_LOW = (MAC_7F4_TIMING_LOW & 0xffff0000) | wlanPsPollTiming.interval;
	MAC_730_TIMING_HIGH = wlanPsPollTiming.minTimeout | (wlanPsPollTiming.interval << 16);
	wlanPsPollTiming.mean = wlanPsPollTiming.span - 100;
	wlanPsPollTiming.deviation = 0;
	wlanPsPollTiming.start = 0;
	wlanPsPollTiming.countdown = PS_POLL_COUNTDOWN;
}

void wlanPsPollTimeoutExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsPoll.timeoutTimer != timer)
		return;
	wlanPsPoll.pending = 0;
	wlanPsPoll.timeoutTimer = 0;
	wlanPsPollProgramTimeout();
	if (wlanPsHostBusy != 0)
		wlanRaiseEvent(WLAN_EVENT_COEX_SUSPEND);
	else if (wlanPsLegacyPollMode() || wlanCoexUseP2pSleepMode())
		wlanRaiseEvent(WLAN_EVENT_PS_POLL);
}

void wlanPsPollStartTimeout(void)
{
	osaCancelTimer(wlanPsPoll.timeoutTimer);
	osaCreateTimer(wlanPsPollTimeoutExpired, 0, wlanPsPoll.timeoutDelay,
		       &wlanPsPoll.timeoutTimer, 8);
}

/*
 * Marks a poll pending; 1 if none may start now. With the PS-Poll frame
 * still in a ring, the transmit-done task is woken if the MAC is busy, or
 * the station sleeps once the frame is out.
 */
int wlanPsPollReserve(void)
{
	UINT posture = sysIrqDisable();
	int busy = MAC_010_QUEUE_HEAD != MAC_014_QUEUE_TAIL;

	if ((wlanPsPoll.pending | wlanPsPoll.sent) != 0) {
		sysIrqRestore(posture);
		if (wlanPsPoll.sent != 0) {
			if (busy) {
				++wlanPsPoll.busyPolls;
				osaEventFlagsSet(macTxNotifyEventFlags, 1, 7);
				wlanRaiseEvent(WLAN_EVENT_PS_POLL);
			} else {
				++wlanPsPoll.deferredSleeps;
				wlanPsPoll.sleepAfterTx = 1;
			}
		}
		return 1;
	}
	if (wlanDmaRingRxDataWindowOpen() == 0) {
		sysIrqRestore(posture);
		++wlanPsPoll.windowClosed;
		wlanPsPollStartTimeout();
		return 1;
	}
	++wlanPsPoll.pending;
	sysIrqRestore(posture);
	return 0;
}

int wlanPsPollBusy(void)
{
	UINT posture = sysIrqDisable();

	if ((wlanPsPoll.pending | wlanPsPoll.retryTimer | wlanPsPoll.timeoutTimer) != 0) {
		sysIrqRestore(posture);
		return 1;
	}
	sysIrqRestore(posture);
	return 0;
}

void wlanPsPollEventTimerExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsPoll.eventTimer != timer)
		return;
	SOC_PENDING_EVENT_CONTROL |= SOC_PENDING_EVENT_ARMED;
	wlanPsPoll.eventTimer = 0;
	if (wlanPsDisabled())
		wlanRaiseEvent(WLAN_EVENT_PS_POLL);
}

/*
 * Decides whether the poll is tried again: while Bluetooth keeps the air
 * (and, in its polling profiles, until the retry limit).
 */
void wlanPsPollRetryTimerExpired(uint32_t timer, uint32_t argument)
{
	unsigned int state = BT_LINK_STATE & 0xf;

	if (wlanPsPoll.retryTimer != timer)
		return;
	wlanPsPoll.retryWanted = 0;
	if (btCoexProfileBit1()) {
		if ((SOC_PENDING_EVENT_CONTROL & SOC_PENDING_EVENT_ARMED) &&
		    (!(SOC_PENDING_EVENT_STATUS & SOC_PENDING_EVENT_BT) || !btLinkBusy(state)))
			wlanPsPoll.retryWanted = 1;
	} else if (btCoexProfilePollable() || btCoexProfileBit3()) {
		if ((SOC_PENDING_EVENT_CONTROL & SOC_PENDING_EVENT_ARMED) &&
		    (SOC_PENDING_EVENT_STATUS & SOC_PENDING_EVENT_BT)) {
			if (wlanPsPoll.retries < wlanPsPoll.retryLimit)
				wlanPsPoll.retryWanted = 1;
			++wlanPsPoll.retries;
		}
	}
	wlanPsPoll.retryTimer = 0;
	wlanRaiseEvent(WLAN_EVENT_PS_POLL_RETRY);
}

void wlanPsPollStartRetryTimer(void)
{
	osaCancelTimer(wlanPsPoll.retryTimer);
	osaCreateTimer(wlanPsPollRetryTimerExpired, 0, wlanPsPoll.retryDelay,
		       &wlanPsPoll.retryTimer, 8);
}

/* Starts a poll if frames wait in the rings and none is out; urgent skips the trigger check. */
void wlanPsPollKick(int urgent)
{
	if (wlanTxRingsPending() == 0)
		return;
	if (wlanPsPollBusy())
		return;
	if (!(wlanPsTriggerEnabled() && wlanCoexUseP2pSleepMode()) && urgent == 0)
		return;
	if (wlanPsPollUsesMacTimer()) {
		if (urgent) {
			wlanPsPoll.retries = 0;
			if (wlanPsPollTriggerOnce())
				return;
			wlanPsPollProgramTimeout();
		} else {
			if (wlanPsPollReserve())
				return;
			wlanPsPollStartTimeout();
		}
		wlanTxRingPublishNext();
	} else {
		if (urgent) {
			wlanPsPoll.retries = 0;
		} else {
			if (wlanPsPollReserve())
				return;
			wlanPsPollStartTimeout();
		}
		wlanPsPollStartRetryTimer();
	}
}

void wlanPsPollTriggerTimerExpired(uint32_t timer, uint32_t argument)
{
	if (wlanPsPoll.triggerTimer != timer)
		return;
	wlanPsPoll.triggerTimer = 0;
	wlanPsPoll.triggered = 0;
	wlanPsPollKick(1);
}

void wlanPsPollStartTriggerTimer(void)
{
	osaCancelTimer(wlanPsPoll.triggerTimer);
	osaCreateTimer(wlanPsPollTriggerTimerExpired, 0, wlanPsPoll.triggerDelay,
		       &wlanPsPoll.triggerTimer, 8);
}

void wlanPsPollCancelTriggerTimer(void)
{
	osaCancelTimer(wlanPsPoll.triggerTimer);
	wlanPsPoll.triggerTimer = 0;
}

void wlanPsPollCancelRetryTimer(void)
{
	osaCancelTimer(wlanPsPoll.retryTimer);
	wlanPsPoll.retryTimer = 0;
}

void wlanPsPollStartEventTimer(void)
{
	osaCancelTimer(wlanPsPoll.eventTimer);
	osaCreateTimer(wlanPsPollEventTimerExpired, 0, wlanPsPoll.eventDelay,
		       &wlanPsPoll.eventTimer, 8);
}

void wlanPsPollCancelEventTimer(void)
{
	osaCancelTimer(wlanPsPoll.eventTimer);
	wlanPsPoll.eventTimer = 0;
}

void wlanPsPollCancelTimeout(void)
{
	osaCancelTimer(wlanPsPoll.timeoutTimer);
	wlanPsPoll.timeoutTimer = 0;
}

void wlanPsPollWaitRingsDrained(void)
{
	while (wlanTxRingsDrained() == 0)
		++wlanPsPoll.ringWaits;
}

static inline void wlanPsPollAbort(void)
{
	UINT posture = sysIrqDisable();

	wlanPsPoll.pending = 0;
	sysIrqRestore(posture);
}

/* Builds the PS-Poll frame for the access point of a station and sends it; 1 if it went out. */
int wlanPsPollSend(struct WlanConnection *connection)
{
	OsaBuffer *buffer = osa_BufferAlloc(&wlanBlockPool7, 300, 0);
	struct WlanReplyFrame *request;
	WlanPsPollFrame *frame;
	uint16_t aid;
	UINT posture;

	if (buffer == NULL) {
		wlanPsPollAbort();
		return 0;
	}
	buffer->packetClass = 2;
	buffer->context = connection;
	frame = (WlanPsPollFrame *)((uint8_t *)buffer + 176);
	buffer->dataOffset = 176;
	__aeabi_memclr(frame, sizeof(*frame));
	/* Protocol version 0, type 1 (control), subtype 10 (PS-Poll). */
	frame->frameControl[0] = (((((frame->frameControl[0] >> 2) << 2) & 0xf3) + 4) & 0xf) + 0xa0;
	frame->frameControl[1] = (frame->frameControl[1] & 0xef) | ((macTxGetPowerSave() & 1) << 4);
	aid = connection->associationId | 0xc000;
	frame->associationId[0] = aid;
	frame->associationId[1] = aid >> 8;
	__aeabi_memcpy(frame->transmitter, connection->interfaceAddress, 6);
	__aeabi_memcpy(frame->bssid, connection->localAddress, 6);

	request = (struct WlanReplyFrame *)((uint8_t *)buffer + 28);
	buffer->dataOffset = 28;
	__aeabi_memclr4(request, offsetof(struct WlanReplyFrame, frameLength));
	request->frame = frame->frameLength;
	request->descriptor.header.wire.packetLength[0] = wlanTxControlRate(connection);
	request->descriptor.control = 0x402;
	if ((wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE) == 0 && request->descriptor.header.wire.packetLength[0] != 0)
		request->descriptor.control = 0x40000402;
	request->queue = 3;

	if (wlanPsPollUsesMacTimer()) {
		wlanPsPollWaitRingsDrained();
	} else {
		wlanPsPollWaitBluetooth();
		if (wlanTxFramesOutsideRings()) {
			osa_BufferFree(buffer);
			posture = sysIrqDisable();
			wlanPsPoll.pending = 0;
			wlanPsPollTiming.start = 0;
			sysIrqRestore(posture);
			return 0;
		}
		SOC_PENDING_EVENT_CONTROL &= ~SOC_PENDING_EVENT_ARMED;
		wlanPsPollStartEventTimer();
	}
	wlanPsPollStartTimeout();
	wlanPsPoll.sent = 1;
	if (wlanTxSubmit(buffer) == 0) {
		osa_BufferFree(buffer);
		posture = sysIrqDisable();
		wlanPsPoll.pending = 0;
		wlanPsPoll.sent = 0;
		sysIrqRestore(posture);
		return 0;
	}
	return 1;
}

/* Hands the air back to WLAN once Bluetooth has finished its slot. */
int wlanPsPollResume(void)
{
	wlanPsPollWaitBluetooth();
	SOC_PENDING_EVENT_CONTROL &= ~SOC_PENDING_EVENT_ARMED;
	wlanPsPollStartEventTimer();
	return wlanTxRingPublishNext();
}

/*
 * The host has queued frames (or finished) while a P2P link sleeps between
 * Bluetooth slots: poll for the rings, or tell the peer with a null frame.
 * Returns result, or 0 when it acted.
 */
int wlanPsPollHostQueued(struct WlanConnection *connection, int moreData, int result, int active)
{
	if (wlanPsDisabled() || !wlanCoexUseP2pSleepMode() || wlanDmaRingsHaveRoom() == 1 ||
	    active == 0)
		return result;
	wlanPsLastTxComplete = moreData == 0;
	if (wlanPsPollEnabled() == 1) {
		UINT posture;

		wlanPsPollCancelTimeout();
		posture = sysIrqDisable();
		wlanPsPoll.pending = 0;
		sysIrqRestore(posture);
	}
	if (wlanPsSleepStateReached() == 0 || wlanPsHostBusy != 0)
		return result;
	if (wlanTxRingsPending()) {
		wlanPsPollKick(0);
	} else {
		if (moreData == 0 || wlanPsNullSuppressed != 0)
			return result;
		wlanPsSendPowerSaveNull(connection);
	}
	return 0;
}

void wlanPsPollEnableMac(void)
{
	MAC_REG(0x2fc) |= 0x80000010;
}

/* Takes the poll interval from the Bluetooth schedule and reprograms the timeout when it changed. */
void wlanPsPollUpdateTiming(void)
{
	uint32_t interval;
	uint32_t offset;
	uint32_t minimum;

	if (btCoexProfileBit1()) {
		btCoexPollTiming(&interval, &offset);
	} else {
		interval = PS_POLL_DEFAULT_INTERVAL;
		offset = PS_POLL_DEFAULT_OFFSET;
	}
	if (wlanPsPollTiming.interval == interval && wlanPsPollTiming.offset == offset)
		return;
	wlanPsPollTiming.interval = interval;
	wlanPsPollTiming.span = interval - offset;
	wlanPsPollTiming.offset = offset;
	minimum = offset + 600;
	if (minimum <= interval - 3000)
		minimum = interval - 3000;
	wlanPsPollTiming.minTimeout = minimum;
	wlanPsPollTiming.maxTimeout = interval - 1000;
	wlanPsPollProgramTimeout();
}

/* Forgets any poll in progress and rearms the answer timing. */
void wlanPsPollReset(void)
{
	UINT posture = sysIrqDisable();

	wlanPsPollCancelTimeout();
	wlanPsPoll.pending = 0;
	wlanPsPoll.sent = 0;
	wlanPsPollUpdateTiming();
	wlanTxRingClearCounters();
	if (wlanPsPollUsesMacTimer()) {
		wlanPsPollCancelTriggerTimer();
		wlanPsPoll.triggered = 0;
		MAC_7F4_TIMING_LOW |= MAC_PS_POLL_TIMER_ENABLE;
	} else {
		wlanPsPollCancelRetryTimer();
		wlanPsPollCancelEventTimer();
		wlanPsPoll.retryWanted = 0;
		SOC_PENDING_EVENT_CONTROL |= SOC_PENDING_EVENT_ARMED;
	}
	sysIrqRestore(posture);
}

/* Ends polling when time sharing with Bluetooth is not running. */
void wlanPsPollStop(void)
{
	UINT posture;

	if (wlanCoexActive() != 0)
		return;
	if (wlanPsPollEnabled() != 1)
		return;
	if (wlanPsPollUsesMacTimer()) {
		wlanPsPollCancelTimeout();
		posture = sysIrqDisable();
		MAC_7F4_TIMING_LOW |= MAC_PS_POLL_TIMER_ENABLE;
	} else {
		wlanPsPollCancelRetryTimer();
		wlanPsPollCancelEventTimer();
		wlanPsPollCancelTimeout();
		posture = sysIrqDisable();
		SOC_PENDING_EVENT_CONTROL |= SOC_PENDING_EVENT_ARMED;
	}
	wlanPsPoll.pending = 0;
	wlanPsPollTiming.start = 0;
	sysIrqRestore(posture);
}

/* Sets the timeout to what is left of the interval after the expected answer time, within bounds. */
void wlanPsPollSetTimeout(int32_t mean, int32_t deviation)
{
	uint16_t timeout = wlanPsPollTiming.interval - mean - deviation;

	if (wlanPsPollTiming.minTimeout > timeout)
		timeout = wlanPsPollTiming.minTimeout;
	else if (wlanPsPollTiming.maxTimeout < timeout)
		timeout = wlanPsPollTiming.maxTimeout;
	MAC_730_TIMING_HIGH = (MAC_730_TIMING_HIGH >> 16 << 16) | timeout;
}

/* The access point has answered: updates the estimate and ends the poll. */
void wlanPsPollAnswered(const struct WlanRxStatus *status)
{
	UINT posture;

	if (wlanCoexActive() != 0)
		return;
	if (wlanPsPollEnabled() != 1)
		return;
	if (wlanPsDisabled())
		return;
	if (wlanPsLegacyPollMode())
		wlanPsPollCancelTimeout();
	if (wlanPsPollUsesMacTimer()) {
		if (wlanPsLegacyPollMode() ||
		    (wlanCoexUseP2pSleepMode() && (status->qosControl[0] & 0x10))) {
			int32_t deviation = wlanPsPollTiming.deviation;
			int32_t mean = wlanPsPollTiming.mean;

			wlanCoexUpdateEstimate(wlanPsPollTiming.start, osaGetHardwareTime(),
					       wlanPsPollTiming.span, &wlanPsPollTiming.mean,
					       wlanPsPollTiming.span >> 1, &wlanPsPollTiming.deviation);
			if (wlanPsPollTiming.countdown != 0 &&
			    wlanPsPollTiming.mean + wlanPsPollTiming.deviation < mean + deviation) {
				--wlanPsPollTiming.countdown;
			} else {
				wlanPsPollSetTimeout(wlanPsPollTiming.mean, wlanPsPollTiming.deviation);
				wlanPsPollTiming.countdown = PS_POLL_COUNTDOWN;
			}
		}
		posture = sysIrqDisable();
		MAC_7F4_TIMING_LOW |= MAC_PS_POLL_TIMER_ENABLE;
	} else {
		wlanPsPollCancelEventTimer();
		posture = sysIrqDisable();
		SOC_PENDING_EVENT_CONTROL |= SOC_PENDING_EVENT_ARMED;
	}
	if (wlanPsLegacyPollMode()) {
		wlanPsPoll.pending = 0;
		wlanTxRingPublishNext();
	}
	sysIrqRestore(posture);
}

/* Counts the air time of the frames sent since into the answer time. */
void wlanPsPollDeductAirtime(uint32_t airtime)
{
	if (wlanPsPollTiming.start == 0)
		return;
	if (airtime >= wlanPsPollTiming.start)
		return;
	++wlanPsPoll.airtimeCorrections;
	wlanPsPollTiming.start -= airtime;
}

void wlanPsPollTxDone(struct WlanConnection *connection, struct WlanTxStatus *status)
{
	if (wlanPsPoll.pending == 0)
		return;
	wlanPsPollTiming.start = osaGetHardwareTime();
	wlanPsPollDeductAirtime(wlanTxAirtime(connection, status));
}

/*
 * Sees a transmitted frame while polling. A management frame forced through
 * by Bluetooth restarts the trigger timer; a data frame that failed goes back
 * to the head of its ring, marked as a retry, and a poll follows. Returns 1
 * if the frame was put back.
 */
int wlanPsPollTxStatus(struct WlanConnection *connection, uint8_t *data, struct WlanTxStatus *status)
{
	volatile uint32_t *ring;
	uint32_t ringBase;
	uint8_t *statusData;
	struct WlanMacTxRequest *request;
	uint32_t published;
	uint32_t last;
	uint8_t *header;
	UINT posture;
	unsigned int type;

	if (wlanCoexPsPollForced() && wlanPsPollUsesMacTimer() &&
	    wlanTxFrameType(data) == 0) {
		wlanPsPollStartTriggerTimer();
		return 0;
	}
	if (data == NULL || status->result == 0) {
		wlanPsPollTxDone(connection, status);
		return 0;
	}
	type = wlanTxFrameType(data);
	if (!(type == 2 && !wlanPsDisabled() && wlanCoexUseP2pSleepMode() && wlanPsTriggerEnabled())) {
		if (!wlanCoexPsPollForced())
			return 0;
		if (wlanTxFrameType(data) != 0)
			return 0;
	}

	ringBase = MAC_000_REGION_A | ((uint32_t)wlanTxDataRing << 8);
	posture = sysIrqDisable();
	statusData = status->data;
	request = TX_DATA_REQUEST(statusData);
	published = wlanTxRingPublished[wlanTxDataRing];
	if ((uint8_t)published != 0)
		last = *(volatile uint32_t *)(((uint8_t)published - 4) | ringBase);
	else
		last = *(volatile uint32_t *)(ringBase | 0xfc);
	if (last != (uint32_t)request || --TX_DATA_RETRIES(statusData) == 0 ||
	    status->retryBlocked != 0) {
		sysIrqRestore(posture);
		return 0;
	}
	ring = (volatile uint32_t *)ringBase;
	last = *(volatile uint32_t *)(ringBase | 0xfc);
	__aeabi_memmove((void *)(ringBase | 4), (void *)ringBase, 0xfc);
	ring[0] = last;
	++wlanTxRingPending[wlanTxDataRing];
	wlanTxRingFilled[wlanTxDataRing] += 4;
	header = (uint8_t *)__aeabi_uread4(request->frame);
	header[3] |= 0x08;	/* frame control: Retry */
	request->mode = 1;
	if (wlanPsPollUsesMacTimer()) {
		MAC_7F4_TIMING_LOW |= MAC_PS_POLL_TIMER_ENABLE;
	} else {
		wlanPsPollCancelEventTimer();
		SOC_PENDING_EVENT_CONTROL |= SOC_PENDING_EVENT_ARMED;
	}
	wlanPsPollCancelTimeout();
	wlanPsPoll.pending = 0;
	wlanPsPollKick(wlanTxFrameType(data) == 0);
	sysIrqRestore(posture);
	return 1;
}

void wlanPsPollHandleEvent(struct WlanConnection *connection, int event)
{
	if (event == WLAN_EVENT_PS_POLL_RETRY) {
		if (wlanPsPoll.retryWanted == 1) {
			wlanPsPollStartRetryTimer();
			return;
		}
		if (wlanPsDisabled() || wlanCoexUseP2pSleepMode()) {
			wlanPsPollResume();
			return;
		}
		if (!wlanPsLegacyPollMode())
			return;
		if (wlanPsPollReserve())
			return;
		if (wlanPsPollSend(connection) == 1) {
			wlanPsPollSentTime = romGetTimerTimestamp();
			return;
		}
		wlanPsEnterSleep(connection, 0);
		return;
	}
	if (event != WLAN_EVENT_PS_POLL)
		return;
	if (wlanPsDisabled()) {
		wlanPsPollKick(1);
	} else if (wlanPsLegacyPollMode()) {
		if (wlanPsPoll.sent != 0)
			wlanPsPollStartTimeout();
		else
			wlanPsEnterSleep(connection, 0);
	} else if (wlanCoexUseP2pSleepMode()) {
		if (wlanTxRingsPending())
			wlanPsPollKick(0);
		else
			wlanPsSendPowerSaveNull(connection);
	}
}

/* The PS-Poll frame has been sent. */
void wlanPsPollFrameDone(void)
{
	wlanPsPoll.sent = 0;
}

int wlanTxDiscardFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	if (connection->interfaceType == WLAN_IF_IBSS)
		return wlanIbssTxFrame(buffer, 0);
	return wlanTxReleaseFrame(buffer);
}

/* Sends a frame of the host: straight into its ring while triggers are used, else through the resubmit path. */
int wlanTxSendFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	if (connection->interfaceType == WLAN_IF_IBSS)
		return wlanIbssTxFrame(buffer, 1);
	if (wlanPsTriggerMode() == 0)
		return wlanTxResubmit(buffer);
	wlanTxInFlightAdd(buffer);
	if (wlanTxQueueFrame(buffer) == 0) {
		wlanTxInFlightRemove(buffer);
		return 0;
	}
	if (wlanCoexUseP2pSleepMode())
		wlanPsPollKick(0);
	wlanCoexReleaseTx();
	return 1;
}
