/*
 * The Block Ack action frames (802.11 7.4.1: ADDBA request and response,
 * DELBA), the host's replies and events about them, the Block Ack timeout
 * table, and the 20/40 BSS coexistence frame (0x4a9f8..0x4b3d8). Marvell
 * published no source for it; the names say what the code does and the file
 * name is inferred.
 */
#include <stddef.h>
#include "robin/baslot.h"
#include "robin/conn.h"
#include "robin/htba.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/mactx.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/softtimer.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/threadx.h"
#include "robin/wlan.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))

/* The Block Ack timeout table's cells (0x553e4..0x553f4); wlanBaTimeoutOpsInit() fills them. */
uint8_t wlanBaTimeoutCount __attribute__((section(".itcm.rw")));
struct WlanBaTimeout *wlanBaTimeoutTable __attribute__((section(".itcm.rw")));
int (*wlanTxBaDeleteHook)(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid)
	__attribute__((section(".itcm.rw")));
int (*wlanRxBaDeleteHook)(const uint8_t *peer, unsigned int tid) __attribute__((section(".itcm.rw")));
int (*wlanTxBaAddHook)(struct WlanConnection *connection, const struct WlanBaRequest *request)
	__attribute__((section(".itcm.rw")));

/* The elements the frame bodies of the HT management put out. */
static inline void put16(uint8_t *bytes, unsigned int value)
{
	bytes[0] = value;
	bytes[1] = value >> 8;
}

/* The body of the action frame in a buffer that wlanBaActionFrameAlloc() made. */
static WlanWirelessFrame *wlanActionFrame(OsaBuffer *buffer)
{
	return (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
}

/* The response of a host command in a message buffer (wlanHostMessageAlloc()). */
static struct WlanHostResponse *wlanHostResponse(OsaBuffer *message)
{
	return (struct WlanHostResponse *)((uint8_t *)message + message->dataOffset);
}

/* Whether the peer may use greenfield frames: it can, and no protection mode asks for them to stop. */
int wlanHtGreenfieldAllowed(const uint8_t *htCapability, const uint8_t *htOperation)
{
	if (htCapability[2] & 0x10) {
		unsigned int information = htOperation[4];

		if ((information & 3) == 0 && !(information & 4) && !(information & 0x10))
			return 1;
	}
	return 0;
}

/* Whether the short guard interval may be used at the rate (14-21: HT 20 MHz, 22-30: HT 40 MHz). */
int wlanHtShortGiAllowed(const uint8_t *htCapability, unsigned int rate, int shortGi20, int shortGi40)
{
	if (rate - 14 > 7) {
		if (rate - 22 <= 8 && (htCapability[2] & 0x40) && shortGi40 != 0)
			return 1;
	} else if ((htCapability[2] & 0x20) && shortGi20 != 0) {
		return 1;
	}
	return 0;
}

/* Copies the HT element of the id (45 capabilities, 61 operation, 72 coexistence, 74 overlap scan) into a frame; returns its length. */
int wlanHtElementCopy(int elementId, void *destination, const uint8_t *htCapability,
		      uint8_t *htOperation, const uint8_t *coexistence, int unused,
		      const uint8_t *overlapScan)
{
	switch (elementId) {
	case 45:
		__aeabi_memcpy(destination, htCapability, 28);
		return 28;
	case 61:
		htOperation[4] = (htOperation[4] & 0xe8) | 3;
		__aeabi_memcpy(destination, htOperation, 24);
		return 24;
	case 72:
		if (coexistence[1] != 0) {
			__aeabi_memcpy(destination, coexistence, 3);
			return 3;
		}
		break;
	default:
		if (elementId == 74 && overlapScan[1] != 0) {
			__aeabi_memcpy(destination, overlapScan, 16);
			return 16;
		}
		break;
	}
	return 0;
}

/* The first free entry of the Block Ack timeout table. */
__attribute__((noinline)) struct WlanBaTimeout *wlanBaTimeoutFree(void)
{
	unsigned int index;

	for (index = 0; index < wlanBaTimeoutCount; index = (uint8_t)(index + 1)) {
		if (wlanBaTimeoutTable[index].active == 0)
			return &wlanBaTimeoutTable[index];
	}
	return NULL;
}

/* Starts the timeout of a Block Ack session; its unit is 1024 microseconds, the timer's tick 10 ms. */
void wlanBaTimeoutStart(struct WlanConnection *connection, unsigned int timeout, const uint8_t *peer,
			unsigned int tid, unsigned int initiator, void (*callback)(uint32_t))
{
	struct WlanBaTimeout *entry = wlanBaTimeoutFree();

	if (entry != NULL) {
		entry->active = 1;
		__aeabi_memcpy(entry->peer, peer, 6);
		entry->initiator = initiator;
		entry->tid = tid;
		entry->timeout = timeout;
		entry->connection = connection;
		wlanSoftTimerInit(&entry->timer);
		wlanSoftTimerStartOnce(&entry->timer, 1, callback, (uint32_t)entry,
				       (uint16_t)((timeout << 10) / 10000));
	}
}

/*
 * A session's timeout ran out: when the session saw traffic it goes on, else
 * the host is told (EVENT_BA_STREAM_TIMEOUT); the timeout starts again.
 */
void wlanBaTimeoutExpired(struct WlanConnection *connection, const uint8_t *record,
			  struct WlanTxBaSession *txSessions, struct WlanRxBaSession *rxSessions,
			  void (*callback)(uint32_t))
{
	unsigned int tid = record[6];
	unsigned int initiator = record[7];
	int idle;

	if (initiator == 0) {
		idle = wlanRxBaFrameCount(tid, record, rxSessions) == 0;
		if (!idle)
			wlanRxBaResetCount(tid, record, rxSessions);
	} else {
		idle = wlanTxBaIdleCount(tid, record, txSessions) == 0;
		if (!idle)
			wlanTxBaTouch(tid, record, txSessions);
	}
	if (idle) {
		uint8_t event[8];

		event[0] = tid;
		__aeabi_memcpy(&event[1], record, 6);
		event[7] = initiator;
		wlanRomServices.sendHostEvent(connection, WLAN_EVENT_BA_STREAM_TIMEOUT, event, 8);
	}
	wlanBaTimeoutStart(connection, U16(record + 8), record, record[6], record[7], callback);
}

/* Stops the timeout of the session; the table's entry is free again. */
int wlanBaTimeoutCancel(const uint8_t *peer, unsigned int tid, unsigned int initiator)
{
	unsigned int index;
	unsigned int count;

	for (index = 0;; index = (uint8_t)(index + 1)) {
		struct WlanBaTimeout *entry;

		count = wlanBaTimeoutCount;
		if (index >= count)
			return count;
		entry = &wlanBaTimeoutTable[index];
		if (entry->tid == tid && memcmp(peer, entry->peer, 6) == 0 &&
		    entry->initiator == initiator && entry->active != 0) {
			wlanSoftTimerStop(&entry->timer);
			entry->active = 0;
			return 0;
		}
	}
}

/* The DELBA action frame: the parameter set (initiator and TID) and the reason. */
void wlanBaDelbaBuild(OsaBuffer *frame, uint16_t paramSet, uint16_t reason)
{
	WlanWirelessFrame *wireless = wlanActionFrame(frame);

	wireless->body[1] = WLAN_BA_ACTION_DELBA;
	put16(&wireless->body[2], paramSet);
	put16(&wireless->body[4], reason);
	put16(wireless->bodyLength, 6);
}

/* The ADDBA response that declines the request of the peer (the request's body: category, action, token, parameter set, timeout). */
void wlanBaAddbaDeclineBuild(OsaBuffer *frame, const uint8_t *request)
{
	WlanWirelessFrame *wireless = wlanActionFrame(frame);

	wireless->body[1] = WLAN_BA_ACTION_ADDBA_RESPONSE;
	wireless->body[2] = request[2];
	put16(&wireless->body[3], WLAN_STATUS_REQUEST_DECLINED);
	wireless->body[5] = request[3];
	wireless->body[6] = request[4];
	put16(&wireless->body[7], U16(request + 5));
	put16(wireless->bodyLength, 9);
}

/* Tells the host of the ADDBA request of a peer (EVENT_ADDBA): its body is the layout of HostCmd 0xce's. */
void wlanBaAddbaEventPost(const uint8_t *peer, const uint8_t *request, struct WlanConnection *connection)
{
	struct HostCmdAddbaRequest event;

	__aeabi_memcpy(event.peer, peer, 6);
	event.result = 0;
	event.dialogToken = request[2];
	event.paramSet[0] = request[3];
	event.paramSet[1] = request[4];
	put16(event.timeout, U16(request + 5));
	put16(event.startSequence, U16(request + 7) >> 4);
	wlanRomServices.sendHostEvent(connection, WLAN_EVENT_ADDBA, &event, sizeof(event));
}

/* A DELBA arrived: the session it names is deleted, and the host is told (EVENT_DELBA). */
void wlanBaDelbaReceived(struct WlanConnection *connection, const uint8_t *peer, const uint8_t *body)
{
	struct HostCmdDelba event;
	UINT posture = tx_interrupt_control(TX_INT_DISABLE);

	if (body[3] & 8) {
		if (wlanRxBaDeleteHook(peer, body[3] >> 4) == 1)
			wlanBaTimeoutCancel(peer, body[3] >> 4, 0);
	} else {
		if (wlanTxBaDeleteHook(connection, peer, body[3] >> 4) == 1)
			wlanBaTimeoutCancel(peer, body[3] >> 4, 1);
	}
	tx_interrupt_control(posture);
	__aeabi_memcpy(event.peer, peer, 6);
	event.paramSet[0] = body[2];
	event.paramSet[1] = body[3];
	event.reason[0] = body[4];
	event.reason[1] = body[5];
	event.result = 0;
	wlanRomServices.sendHostEvent(connection, WLAN_EVENT_DELBA, &event, sizeof(event));
}

/* The response of HostCmd 0xce: the peer's ADDBA response (its body: category, action, token, status, parameter set, timeout). */
void wlanHostAddbaReplyBuild(OsaBuffer *message, int result, const uint8_t *peer, const uint8_t *body,
			     uint16_t status)
{
	struct WlanHostResponse *response = wlanHostResponse(message);

	response->command[0] = WLAN_HOST_RESPONSE_ADDBA_REQUEST & 0xff;
	response->command[1] = WLAN_HOST_RESPONSE_ADDBA_REQUEST >> 8;
	put16(response->result, status);
	response->sequence[0] = 0;
	response->size[0] = 24;
	response->size[1] = 0;
	__aeabi_memcpy(&response->body[1], peer, 6);
	response->body[0] = result;
	response->body[7] = body[2];
	response->body[8] = body[3];
	response->body[9] = body[4];
	response->body[10] = body[5];
	response->body[11] = body[6];
	response->body[12] = body[7];
	response->body[13] = body[8];
}

/*
 * The peer's ADDBA response was accepted: the transmit session is added
 * (unless one exists) and its timeout started.
 */
void wlanBaAddbaAccepted(struct WlanConnection *connection, uint16_t startSequence, unsigned int timeout,
			 uint16_t paramSet, const uint8_t *peer, struct WlanTxBaSession *txSessions,
			 void (*callback)(uint32_t))
{
	struct WlanBaRequest request;
	unsigned int tid = (paramSet >> 2) & 0xf;

	__aeabi_memcpy(request.peer, peer, 6);
	if (wlanTxBaProbe(tid, request.peer, txSessions) == 0) {
		request.flags = 0;
		__aeabi_uwrite4(tid, request.tid);
		__aeabi_uwrite4(paramSet >> 6, request.timeout);
		__aeabi_uwrite4(paramSet >> 6, request.windowSize);
		request.startSequence[0] = startSequence << 4;
		request.startSequence[1] = (startSequence << 4) >> 8;
		if (wlanTxBaAddHook(connection, &request) == 0)
			return;
	}
	if (timeout != 0)
		wlanBaTimeoutStart(connection, timeout, request.peer, tid, 1, callback);
}

/* The response of HostCmd 0xd0, the DELBA: the host's result, the peer, and the parameter set and reason it gave. */
void wlanHostDelbaReplyBuild(OsaBuffer *message, struct WlanConnection *connection, const uint8_t *body, int success,
			     const uint8_t *peer)
{
	struct WlanHostResponse *response = wlanHostResponse(message);
	unsigned int reason;

	response->command[0] = WLAN_HOST_RESPONSE_DELBA & 0xff;
	response->command[1] = WLAN_HOST_RESPONSE_DELBA >> 8;
	response->size[0] = 20;
	response->size[1] = 0;
	response->result[0] = 0;
	response->result[1] = 0;
	response->sequence[0] = 0;
	response->body[0] = success == 0;
	__aeabi_memcpy(&response->body[1], peer, 6);
	response->body[7] = body[2];
	response->body[8] = body[3];
	reason = body[4] | body[5] << 8;
	response->body[9] = reason;
	response->body[10] = reason >> 8;
	response->body[11] = 0;
}

/* The ADDBA request action frame of the host's HostCmd 0xce (the starting sequence number goes in bits 4-15). */
void wlanBaAddbaRequestBuild(OsaBuffer *frame, uint16_t startSequence, const struct HostCmdAddbaRequest *request)
{
	WlanWirelessFrame *wireless = wlanActionFrame(frame);

	wireless->body[1] = WLAN_BA_ACTION_ADDBA_REQUEST;
	wireless->body[2] = request->dialogToken;
	wireless->body[3] = request->paramSet[0];
	wireless->body[4] = request->paramSet[1];
	put16(&wireless->body[5], U16(request->timeout));
	put16(&wireless->body[7], startSequence << 4);
	put16(wireless->bodyLength, 9);
}

/* Whether the request can be sent: a session of the peer and TID is under way, or a hardware slot is free (else -1). */
int wlanBaAddbaRequestCheck(const uint8_t *request, struct WlanTxBaSession *txSessions, unsigned int tid)
{
	int result = 0;

	if (wlanTxBaProbe(tid, request + 1, txSessions) != 0)
		wlanBaTimeoutCancel(request + 1, tid, 1);
	else if (wlanTxBaFindFreeHwSlot() == wlanTxBaCount)
		return -1;
	return result;
}

/* The ADDBA response action frame of the host's HostCmd 0xcf. */
void wlanBaAddbaResponseBuild(OsaBuffer *frame, const struct HostCmdAddbaResponse *response)
{
	WlanWirelessFrame *wireless = wlanActionFrame(frame);

	wireless->body[1] = WLAN_BA_ACTION_ADDBA_RESPONSE;
	wireless->body[2] = response->dialogToken;
	wireless->body[5] = response->paramSet[0];
	wireless->body[6] = response->paramSet[1];
	wireless->body[7] = response->timeout[0];
	wireless->body[8] = response->timeout[1];
	put16(&wireless->body[3], U16(response->status));
	put16(wireless->bodyLength, 9);
}

/* The receive session's request from the host's ADDBA response. */
void wlanBaRxRequestFromResponse(struct WlanBaRequest *request, const uint8_t *response)
{
	request->flags = 2;
	__aeabi_uwrite4((response[10] >> 2) & 0xf, request->tid);
	__aeabi_uwrite4(U16(response + 10) >> 6, request->timeout);
	__aeabi_uwrite4(U16(response + 10) >> 6, request->windowSize);
	__aeabi_memcpy(request->peer, response + 1, 6);
	request->startSequence[0] = response[14];
	request->startSequence[1] = response[15];
}

/* Notes what the HT elements of a frame say in the protection timers: non-greenfield stations and other BSSs' non-HT stations. */
void wlanHtProtectionObserve(WlanProtection *protection, const WlanIePointers *ies, const uint8_t *source,
			     const uint8_t *bssid, int32_t timeout)
{
	const uint8_t *htInformation = ies->pHtInfo;
	const WlanHtCapabilityIe *htCapability = ies->pHtCap;
	WlanProtectionTimer *timer;
	int own = memcmp(bssid, source, 6) == 0;

	if (htInformation != NULL && htCapability != NULL) {
		if (htInformation[4] & 4)
			__aeabi_uwrite4(timeout, protection->timers[WLAN_PROTECTION_NON_GREENFIELD].remaining);
		if (htInformation[4] & 0x10)
			__aeabi_uwrite4(timeout, protection->timers[WLAN_PROTECTION_OBSS_NON_HT].remaining);
		if (own) {
			if (htCapability->capabilities & 0x10)
				return;
		} else if ((htCapability->capabilities & 0x10) && htCapability->mcsSet[0] != 0) {
			return;
		}
	}
	timer = own ? &protection->timers[WLAN_PROTECTION_NO_GREENFIELD_BSS]
		    : &protection->timers[WLAN_PROTECTION_NO_GREENFIELD_PEER];
	__aeabi_uwrite4(timeout, timer->remaining);
}

/* Clears the HT elements the arguments name (capability 28 bytes, operation 24, coexistence 3, an address 6, overlap scan 16). */
void wlanHtElementsClear(uint8_t *htCapability, uint8_t *htOperation, uint8_t *coexistence, uint8_t *address,
			 uint8_t *overlapScan)
{
	if (htCapability != NULL)
		__aeabi_memclr(htCapability, 28);
	if (htOperation != NULL)
		__aeabi_memclr(htOperation, 24);
	if (coexistence != NULL)
		__aeabi_memclr(coexistence, 3);
	if (address != NULL)
		__aeabi_memclr(address, 6);
	if (overlapScan != NULL)
		__aeabi_memclr(overlapScan, 16);
}

/*
 * A DELBA was sent (notify 1) or its transmission failed (0): the timer that waited for it stops and, when it
 * was running and the session is deleted, the session's timeout too. Returns 1 when the timer was running.
 */
uint8_t wlanBaDelbaCheck(int notify, const uint8_t *body, struct WlanConnection *connection, const uint8_t *peer,
			 struct WlanSoftTimer *timer)
{
	int running = 0;
	UINT posture = tx_interrupt_control(TX_INT_DISABLE);

	if (notify != 0) {
		if (timer->running != 0) {
			int deleted;

			running = 1;
			wlanSoftTimerStop(timer);
			if (body[3] & 8)
				deleted = wlanTxBaDeleteHook(connection, peer, body[3] >> 4);
			else
				deleted = wlanRxBaDeleteHook(peer, body[3] >> 4);
			if (deleted == 1)
				wlanBaTimeoutCancel(peer, body[3] >> 4, (body[3] >> 3) & 1);
		}
	} else if (timer->running != 0) {
		running = 1;
		wlanSoftTimerStop(timer);
	}
	tx_interrupt_control(posture);
	return running;
}

/* Whether the action frame body is a DELBA. */
int wlanBaIsDelba(const uint8_t *body)
{
	return body[0] == WLAN_ACTION_CATEGORY_BLOCK_ACK && body[1] == WLAN_BA_ACTION_DELBA;
}

/* Whether the action frame body is a 20/40 BSS coexistence management frame (public action 0). */
int wlanBssCoexIsFrame(const uint8_t *body)
{
	return body[0] == WLAN_ACTION_CATEGORY_PUBLIC && body[1] == 0;
}

/* The response of HostCmd 0xe9: the coexistence element and the intolerant channel report of the frame. */
void wlanHostCoexistenceReplyBuild(OsaBuffer *message, struct WlanConnection *connection, const uint8_t *body)
{
	struct WlanHostResponse *response = wlanHostResponse(message);

	response->command[0] = WLAN_HOST_RESPONSE_COEXISTENCE & 0xff;
	response->command[1] = WLAN_HOST_RESPONSE_COEXISTENCE >> 8;
	response->result[0] = 0;
	response->result[1] = 0;
	__aeabi_memcpy(&response->body[0], body + 2, 3);
	__aeabi_memcpy(&response->body[3], body + 5, body[6] + 2);
	put16(response->size, body[6] + 13);
}

/* Makes the response of an ADDBA request that got no answer, a failure (status 1); 0 when no request was waiting. */
int wlanBaAddbaTimeoutResponse(uint8_t *response, const uint8_t *record, uint8_t *token)
{
	if (*token == 0)
		return 0;
	*token = 0;
	response[2] = record[7];
	response[3] = WLAN_STATUS_UNSPECIFIED_FAILURE;
	response[4] = 0;
	response[5] = record[8];
	response[6] = record[9];
	put16(&response[7], U16(record + 10));
	return 1;
}

/* The public action frame of a 20/40 BSS coexistence management frame: the coexistence element and, with more than three bytes, the report. */
void wlanBssCoexFrameBuild(OsaBuffer *frame, const uint8_t *body, unsigned int length)
{
	WlanWirelessFrame *wireless = wlanActionFrame(frame);
	unsigned int size = 5;

	wireless->body[1] = 0;
	__aeabi_memcpy(&wireless->body[2], body, 3);
	if (length > 3) {
		__aeabi_memcpy(&wireless->body[5], body + 3, body[4] + 2);
		size = body[4] + 7;
	}
	put16(wireless->bodyLength, size);
}

/*
 * The host's ADDBA response was sent: a receive session of the peer and TID
 * is made or updated (update) or taken out of the hardware's tables, and its
 * timeout started when the response names one.
 */
void wlanBaRxResponseApply(const uint8_t *response, int update, struct WlanRxBaSession *sessions,
			   struct WlanConnection *connection, void (*callback)(uint32_t))
{
	struct WlanBaRequest request;
	uint8_t slot;
	uint8_t hwSlot = 0;
	unsigned int timeout;

	wlanBaRxRequestFromResponse(&request, response);
	if (wlanRxBaHwSlot(request.tid[0], request.peer, &slot, sessions) != 0) {
		if (update)
			wlanRxBaHwUpdate(&request, slot);
		wlanBaTimeoutCancel(request.peer, request.tid[0], 0);
	} else {
		if (update) {
			if (wlanRxBaHwSetup(&request, &hwSlot) != 1)
				return;
		} else {
			wlanRxBaHwRelease(request.peer, (uint16_t)request.tid[0]);
		}
		wlanRxBaAdd(&request, hwSlot, sessions);
		if (update)
			wlanMacE80Select();
	}
	timeout = U16(response + 12);
	if (timeout != 0)
		wlanBaTimeoutStart(connection, timeout, request.peer, request.tid[0], 0, callback);
}
