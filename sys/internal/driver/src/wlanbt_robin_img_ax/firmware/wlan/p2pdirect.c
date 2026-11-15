/*
 * The direct link of a P2P group owner (0x41050 - 0x41430): when the group owner may send to its
 * clients (the CTWindow and the absence periods of the Notice of Absence), the timer that waits for
 * it, the tests of the group owner's connection, and the admission state of the TSPECs the group's
 * clients set up. The names of the firmware's placeholders are kept; see robin/p2pgo.h.
 */
#include <stddef.h>
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/p2pgo.h"
#include "robin/rom.h"
#include "robin/stapssm.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wmm.h"

extern void __aeabi_memclr(void *destination, size_t length);
extern void __aeabi_memcpy(void *destination, const void *source, size_t length);
extern const uint8_t wlanP2pOui[4];					/* SQRAM 0xc000faa3 */

/* How often the group owner was away and present when wlanDirectLinkPoll() looked. */
unsigned int wlanDirectLinkPresences __attribute__((section(".itcm.rw")));	/* 0x55238 */
unsigned int wlanDirectLinkAbsences __attribute__((section(".itcm.rw")));	/* 0x5523c */

/* The CTWindow of the Notice of Absence: bit 7 of its byte is the opportunistic power save bit. */
#define NOA_CTWINDOW(ps)	((ps)->noaAttribute[4])
/* The first absence descriptor: count, duration, interval and start. */
#define NOA_DESCRIPTOR(ps)	(&(ps)->noaAttribute[5])
#define NOA_MODE_CTWINDOW	1
#define NOA_MODE_ABSENCE	2

/* The TSF of the beacon, read as one word pair. */
struct __attribute__((may_alias)) WlanBeaconTimestamp {
	uint64_t value;
};

#define BEACON_TIMER(ps)	(((uint64_t)(ps)->beaconTimerHigh << 32) | (ps)->beaconTimerLow)

/*
 * Whether the group owner may send to a client now: 1 when it may, 0 when it has to wait the time
 * put in *delay (microseconds). With a CTWindow the owner may send inside the window that opens
 * with each beacon; with absence periods, outside them.
 */
int wlanDirectLinkPoll(struct WlanConnection *connection, uintptr_t *delay)
{
	struct WlanPsPeerState *ps;

	if (delay != NULL)
		*delay = 0;
	if (connection == NULL)
		return 1;
	ps = wlanConnectionPsState(connection);
	if (ps == NULL)
		return 1;

	if (ps->noaMode & NOA_MODE_CTWINDOW) {
		unsigned int window;
		unsigned int period;
		unsigned int rest;
		uint64_t now;

		if (connection->mode != 3 || (NOA_CTWINDOW(ps) >> 7) == 0)
			return 1;
		window = (NOA_CTWINDOW(ps) << 10) & 0x1ffff;
		now = romGetTimerTimestamp();
		period = connection->beaconPeriod << 10;
		if (now >= BEACON_TIMER(ps) && BEACON_TIMER(ps) != 0) {
			rest = (now - BEACON_TIMER(ps)) % period;
			if (rest >= window - 200)
				rest = now % period;
			if (rest < window) {
				if (delay != NULL)
					*delay = window - rest;
				return 1;
			}
			if (delay != NULL)
				*delay = period - rest;
		}
		return 0;
	}
	if (ps->noaMode & NOA_MODE_ABSENCE) {
		const uint8_t *descriptor = NULL;
		unsigned int interval;
		unsigned int duration;
		unsigned int start;
		unsigned int rest;
		uint64_t beacon;
		uint64_t time;

		if (connection->mode == 3)
			descriptor = NOA_DESCRIPTOR(ps);
		interval = __aeabi_uread4(descriptor + 5);
		duration = __aeabi_uread4(descriptor + 1);
		start = __aeabi_uread4(descriptor + 9);
		beacon = ((const struct WlanBeaconTimestamp *)(const void *)ps->beaconTimestamp)->value;
		time = romGetTimerTimestamp() - BEACON_TIMER(ps) + beacon;
		if ((unsigned int)time < start)
			return 1;
		rest = ((unsigned int)time - start) % interval;
		if (rest < duration || interval - rest <= 150) {
			wlanDirectLinkAbsences++;
			if (delay != NULL)
				*delay = duration - rest;
			ps->noaPresent = 0;
		} else {
			wlanDirectLinkPresences++;
			if (delay != NULL)
				*delay = interval - rest;
			ps->noaPresent = 1;
		}
		return ps->noaPresent;
	}
	return 1;
}

/* Hands every queue of the MAC its frames and waits until the hardware has taken them. */
void wlanDirectLinkTrigger(void)
{
	unsigned int queue;

	for (queue = 0; queue < 4; queue = (uint8_t)(queue + 1)) {
		romLibDispatchEntry(queue);
		while (!((1u << queue) & MAC_REG(0x258)))
			;
	}
}

__attribute__((noinline)) void p2pDirectVerifyContext(void)
{
	uint8_t queue = 0;

	do {
		romLibParseEntry(queue);
		queue++;
	} while (queue <= 3);
}

/* The timer of wlanDirectLinkSend() ran out: the group owner may send. */
void p2pDirectSelectContext(uint32_t timer, uint32_t argument)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState((struct WlanConnection *)argument);

	if (ps->directLinkTimer == timer) {
		wlanRaiseEvent(15);
		if (ps->triggerPending == 1) {
			ps->triggerPending = 0;
			p2pDirectVerifyContext();
			macTxPostEvent(1);
		}
	}
}

void p2pDirectAdjustContext(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	osaCancelTimer(ps->directLinkTimer);
	ps->directLinkTimer = 0;
}

/* Waits delay microseconds for the group owner to be allowed to send. */
void wlanDirectLinkSend(struct WlanConnection *connection, uintptr_t delay)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	p2pDirectAdjustContext(connection);
	osaCreateTimer(p2pDirectSelectContext, (uint32_t)(uintptr_t)connection, delay, &ps->directLinkTimer, 0);
}

/* The connection is a P2P group owner's (an access point with the P2P role 2), or its client's. */
int wlanP2pConnection(struct WlanConnection *connection)
{
	return connection->interfaceType == 3 && connection->mode == 2;
}

int wlanPsUapsdActive(struct WlanConnection *connection)
{
	return connection->interfaceType == 3 && connection->mode == 3;
}

/* The first access point connection that has the mode. */
struct WlanConnection *p2pDirectProcessEntry(unsigned int mode)
{
	struct WlanConnection *connection = wlanConnectionFirst(3);

	if (connection == NULL || connection->mode != mode)
		return NULL;
	return connection;
}

/* Takes the P2P element out of the beacon template of the group owner and checks the frame again. */
unsigned int p2pDirectUpdateEntry(struct WlanConnection *connection, unsigned int unused)
{
	uint8_t *frame;
	uint8_t *element;
	unsigned int length;
	unsigned int elementLength;
	unsigned int rest;

	if (connection->interfaceType != 3)
		return connection->interfaceType;
	frame = (uint8_t *)(uintptr_t)__aeabi_uread4(connection->beaconRequest->frame);
	length = (frame[1] << 8) | frame[0];
	element = p2pDirectDispatchContext((WlanWirelessFrame *)frame, 1, wlanP2pOui, 4);
	elementLength = element[1] + 2;
	rest = length - ((element - (frame + 32)) + elementLength);
	frame[0] = length - elementLength;
	frame[1] = (uint16_t)(length - elementLength) >> 8;
	if (rest != 0)
		__aeabi_memcpy(element, element + elementLength, rest);
	return p2pDirectValidateContext(connection, (WlanWirelessFrame *)frame);
}

/* The medium time the active TSPECs hold. */
unsigned int p2pDirectConfigureEntry(void)
{
	unsigned int total = 0;
	unsigned int slot;

	for (slot = 0; slot < WLAN_WMM_SLOTS; slot = (uint8_t)(slot + 1)) {
		if (wlanWmmSlots[slot].state == 4)
			total += (wlanWmmSlots[slot].mediumTime[1] << 8) | wlanWmmSlots[slot].mediumTime[0];
	}
	return total;
}

/* Each queue is admitted the medium time of its two slots. */
unsigned int p2pDirectCheckEntry(void)
{
	unsigned int queue = 0;

	do {
		struct WlanWmmQueue *record = &wlanWmm.queues[queue];
		const struct WlanWmmSlot *first = record->slots[0];
		const struct WlanWmmSlot *second = record->slots[1];

		queue = (uint8_t)(queue + 1);
		record->admittedTime = ((first->mediumTime[1] << 8) | first->mediumTime[0]) +
				       ((second->mediumTime[1] << 8) | second->mediumTime[0]);
	} while (queue < WLAN_WMM_QUEUES);
	return queue;
}

/* The TSPECs in use: how many, and in *toRequest how many are still to be requested (state 1). */
int p2pDirectValidateEntry(uint8_t *toRequest)
{
	int count = 0;
	uint8_t requestCount = 0;
	unsigned int slot;

	for (slot = 0; slot < WLAN_WMM_SLOTS; slot = (uint8_t)(slot + 1)) {
		const struct WlanWmmSlot *session = wlanWmmSessions[slot];

		if (session != NULL && session->state != 0) {
			count = (uint8_t)(count + 1);
			if (session->state == 1)
				requestCount++;
		}
	}
	if (toRequest != NULL)
		*toRequest = requestCount;
	return count;
}

/* The TSPEC takes its U-APSD bit (action 1) or gives it back (action 0). */
void p2pDirectPrepareEntry(struct WlanWmmSlot *session, int action)
{
	wlanTspecApplyBits(action, &wlanWmmSessionBits, &wlanWmmFlowBits, &wlanWmmReferenceBits, session->tspec,
			   &session->directions);
	wlanPsSetRoleMasks(wlanWmmFlowBits, wlanWmmSessionBits);
}

/* Frees the TSPEC of a slot. */
int p2pDirectDispatchEntry(unsigned int slot)
{
	struct WlanWmmSlot **session = &wlanWmmSessions[slot];

	__aeabi_memclr(*session, 0x37);
	__aeabi_memclr((*session)->tail, 0x1b);
	(*session)->tailLength = 0;
	(*session)->directions = 0;
	(*session)->state = 0;
	*session = NULL;
	return 0;
}

/* Starts the queues' medium time over: it runs for a second, and again while a queue used any. */
void p2pDirectParseEntry(uint32_t timer, uint32_t argument)
{
	int used = 0;
	unsigned int queue = 0;

	do {
		struct WlanWmmQueue *record = &wlanWmm.queues[queue];

		if (record->admittedTime != 0) {
			used = 1;
			if (record->exhausted != 0) {
				record->exhausted = 0;
				wlanWmmApplyQueue(queue);
			}
		}
		queue = (uint8_t)(queue + 1);
		record->usedTime = 0;
	} while (queue < WLAN_WMM_QUEUES);
	if (used != 0)
		osaCreateTimer(p2pDirectParseEntry, 0, 1000000, &wlanWmmPolicingTimer, 0);
	else
		wlanWmmPolicingTimer = 0;
}
