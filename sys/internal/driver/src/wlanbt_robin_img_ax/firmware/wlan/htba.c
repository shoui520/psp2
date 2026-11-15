/*
 * 802.11n in the connections' management (0x301b0..0x30f1c): the HT elements
 * a connection keeps, the host commands that start and end a Block Ack
 * session (HostCmd_CMD_11N_ADDBA_REQ 0xce, _ADDBA_RSP 0xcf, _DELBA 0xd0, and
 * 0xe9, a 20/40 BSS coexistence frame), the Block Ack and coexistence frames
 * the radio receives, and the transmit rings' moves to a window. Marvell
 * published no source for it; the names say what the code does and the file
 * name is inferred.
 */
#include <stddef.h>
#include "robin/apps.h"
#include "robin/baslot.h"
#include "robin/conn.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mef.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/softtimer.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"
#include "robin/stamachine.h"

/* Not converted yet; provisional names. */
void macMgmtManagePacket(struct WlanConnection *connection, int enable);			/* 0x3a100 */

/* The HT configuration the host set (HostCmd_CMD_11N_CFG), and the Block Ack state kept in SQRAM. */
extern uint8_t wlanHtTxCapability[2];				/* SQRAM 0xc0011bc1 */
extern uint8_t wlanHtTxInfo[2];					/* SQRAM 0xc0011bc3 */
extern struct WlanInitStage wlanHtInitStage;			/* SQRAM 0xc0011bcc */
extern struct WlanBaTimeout wlanBaTimeouts[12];			/* SQRAM 0xc0031f28 */
extern struct WlanSoftTimer wlanAddbaTimer;			/* SQRAM 0xc003218c: the ADDBA request's timeout */
extern struct WlanAddbaPending wlanAddbaPending;		/* SQRAM 0xc00321a4 */
extern struct WlanSoftTimer wlanDelbaTimer;			/* SQRAM 0xc00321b8: the DELBA's timeout */
extern struct WlanDelbaPending wlanDelbaPending;		/* SQRAM 0xc00321d0 */
extern uint8_t wlanHtCapabilityCache[28];			/* SQRAM 0xc0032170 */

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))

static void put16(uint8_t *bytes, unsigned int value)
{
	bytes[0] = value;
	bytes[1] = value >> 8;
}

#define WLAN_ADDBA_TIMEOUT_TICKS	100

/* The HT operation element in use on the connection. */
const uint8_t *wlanConnectionHtOperation(struct WlanConnection *connection)
{
	const uint8_t *operation = NULL;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanHtElements *elements = wlanConnectionTypeBlock(connection);

	switch (connection->interfaceType) {
	case 0:
	case 2:
	case 3:
		if (connection->mode == 2)
			operation = info->htOperation;
		else if (connection->mode != 0)
			operation = elements->htOperation;
		break;
	case 1:
		operation = ((struct WlanHtElements *)(void *)&connection->localName)->htOperation;
		break;
	case 4:
		operation = ((struct WlanHtElements *)(void *)&connection->secondTimerHandle)->htOperation;
		break;
	case 5:
	case 6:
		sysIrqDisable();
		for (;;)
			;
	}
	return operation;
}

/* The 20/40 BSS coexistence element in use on the connection. */
const uint8_t *wlanConnectionHtCoexistence(struct WlanConnection *connection)
{
	const uint8_t *coexistence = NULL;
	struct WlanHtElements *elements = wlanConnectionTypeBlock(connection);

	switch (connection->interfaceType) {
	case 0:
		return elements->htCoexistence;
	case 1:
		return ((struct WlanHtElements *)(void *)&connection->localName)->htCoexistence;
	case 3:
		if (connection->mode != 3 && connection->mode != 1)
			return coexistence;
		return elements->htCoexistence;
	case 4:
		return ((struct WlanHtElements *)(void *)&connection->secondTimerHandle)->htCoexistence;
	default:
		sysIrqDisable();
		for (;;)
			;
	}
}

/* The overlapping BSS scan parameters in use on the connection. */
const uint8_t *wlanConnectionHtOverlapScan(struct WlanConnection *connection)
{
	const uint8_t *scan = NULL;
	struct WlanHtElements *elements = wlanConnectionTypeBlock(connection);

	switch (connection->interfaceType) {
	case 0:
		return elements->overlapScan;
	case 1:
		return ((struct WlanHtElements *)(void *)&connection->localName)->overlapScan;
	case 3:
		if (connection->mode != 3 && connection->mode != 1)
			return scan;
		return elements->overlapScan;
	case 4:
		return ((struct WlanHtElements *)(void *)&connection->secondTimerHandle)->overlapScan;
	default:
		sysIrqDisable();
		for (;;)
			;
	}
}

/* Whether greenfield frames are used: the host allows them (the access point's own setting for mode 2) and the BSS does. */
int wlanHtGreenfieldWanted(struct WlanConnection *connection)
{
	unsigned int allowed = (wlanHtTxCapability[0] >> 4) & 1;
	const uint8_t *operation = wlanConnectionHtOperation(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	if (connection->mode == 2)
		allowed = (info->htCapability[2] >> 4) & 1;
	return allowed != 0 && wlanHtGreenfieldAllowed(connection->htCapability, operation);
}

/* Whether the short guard interval is used at the rate. */
int wlanHtShortGiWanted(struct WlanConnection *connection, unsigned int rate)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int capability;

	if (connection->mode == 2)
		capability = info->htCapability[2];
	else
		capability = wlanHtTxCapability[0];
	return wlanHtShortGiAllowed(connection->htCapability, rate, (capability >> 5) & 1,
				    (capability >> 6) & 1);
}

/* The channel width in use: 1 for 20 MHz, 2 for 40 MHz. */
int wlanHtChannelWidth(struct WlanConnection *connection)
{
	unsigned int wide;

	if (connection->mode == 2)
		wide = (wlanConnectionHtOperation(connection)[3] >> 2) & 1;
	else
		wide = (wlanHtTxCapability[0] >> 1) & 1;
	return wide + 1;
}

/* The shift of the Block Ack timeout: the peer's maximum A-MPDU length exponent. */
uint32_t wlanTxBaTimeoutShift(uintptr_t owner)
{
	return ((struct WlanConnection *)owner)->htCapability[4] & 3;
}

int wlanConnectionIsHt(struct WlanConnection *connection)
{
	return (connection->flags >> 6) & 1;
}

int wlanConnectionHtTransmitsBa(struct WlanConnection *connection)
{
	unsigned int flags = connection->flags;

	if (flags & 0x40)
		return flags >> 7 != 0;
	return 0;
}

/* Copies the connection's HT element of the id (45, 61, 72 or 74) into the frame being built; returns its length. */
int wlanHtElementGet(struct WlanConnection *connection, int elementId, void *destination)
{
	int length = 0;
	uint8_t *operation = (uint8_t *)wlanConnectionHtOperation(connection);
	const uint8_t *coexistence = wlanConnectionHtCoexistence(connection);
	const uint8_t *scan = wlanConnectionHtOverlapScan(connection);

	if (connection->flags & 0x40)
		return wlanHtElementCopy(elementId, destination, connection->htCapability, operation,
					 coexistence, 0, scan);
	return length;
}

/* The timer of a Block Ack timeout expired: the table's entry is given up and the session looked at. */
void wlanBaTimeoutCallback(uint32_t entry)
{
	struct WlanBaTimeout *timeout = (struct WlanBaTimeout *)entry;

	if (timeout->active == 1) {
		timeout->active = 0;
		wlanBaTimeoutExpired(timeout->connection, timeout->peer, wlanTxBaSessions,
				     wlanRxBaSessions, wlanBaTimeoutCallback);
	}
}

/* A management frame of the connection towards the peer, headed for an action frame of Block Ack category. */
void wlanBaActionFrameAlloc(struct WlanConnection *connection, OsaBuffer **buffer,
			    const uint8_t *peer)
{
	OsaBuffer *frame = wlanMgmtAllocFrame(connection, 300);
	WlanWirelessFrame *wireless;

	*buffer = frame;
	if (frame == NULL)
		return;
	wireless = (WlanWirelessFrame *)((uint8_t *)frame + frame->dataOffset);
	wlanMgmtBuildFrame(frame, connection->interfaceAddress, peer, 13, connection->localAddress);
	wireless->body[0] = WLAN_ACTION_CATEGORY_BLOCK_ACK;
	(*buffer)->dataOffset = 176;
}

/* Sends a public action frame holding a 20/40 BSS coexistence element and, with length over 3, an intolerant channel report. */
int wlanBssCoexFrameSend(struct WlanConnection *connection, const uint8_t *body, unsigned int length)
{
	OsaBuffer *frame = wlanMgmtAllocFrame(connection, 300);
	WlanWirelessFrame *wireless;

	if (frame == NULL)
		return -1;
	wireless = (WlanWirelessFrame *)((uint8_t *)frame + frame->dataOffset);
	wlanMgmtBuildFrame(frame, connection->interfaceAddress, connection->peerAddress, 13,
			   connection->localAddress);
	wireless->body[0] = WLAN_ACTION_CATEGORY_PUBLIC;
	frame->dataOffset = 176;
	wlanBssCoexFrameBuild(frame, body, length);
	return wlanMgmtSendFrame(frame);
}

/*
 * The ADDBA response arrived, or the request timed out (the result 2): the
 * session is added if the peer accepted, and the host is answered with the
 * response of its command.
 */
void wlanBaAddbaResponseReceived(struct WlanConnection *connection, int result,
				 const uint8_t *peer, uint8_t *response)
{
	OsaBuffer *message;
	unsigned int sequence;

	if (wlanAddbaTimer.running != 0)
		wlanSoftTimerStop(&wlanAddbaTimer);
	if (result == 0 && U16(response + 3) == 0) {
		if (connection->interfaceType == WLAN_IF_IBSS || connection->mode == 2)
			sequence = wlanPeerNextSequence(connection, (response[5] >> 2) & 0xf, 0);
		else
			sequence = wlanWmmLastSequence((response[5] >> 2) & 0xf);
		wlanBaAddbaAccepted(connection, sequence, U16(wlanAddbaPending.request.timeout),
				    U16(response + 5), peer, wlanTxBaSessions, wlanBaTimeoutCallback);
	}
	message = wlanHostMessageAlloc(connection);
	response[5] = (response[5] & 0xc3) | (wlanAddbaPending.request.paramSet[0] & 0x3c);
	wlanHostAddbaReplyBuild(message, result, peer, response, 0);
	wlanHostMessagePost(message);
}

/* The DELBA could not be sent in time: the host is told it failed. */
void wlanBaDelbaTimeout(uint32_t record)
{
	const struct WlanDelbaPending *pending = (const struct WlanDelbaPending *)record;
	uint8_t body[6];
	UINT posture = sysIrqDisable();

	wlanDelbaCount++;
	body[2] = pending->delba.paramSet[0];
	body[3] = pending->delba.paramSet[1];
	body[4] = pending->delba.reason[0];
	body[5] = pending->delba.reason[1];
	wlanHostDelbaReply(pending->connection, body, 1, pending->delba.peer);
	sysIrqRestore(posture);
}

/* The ADDBA request got no response: the response is made up as a failure. */
void wlanBaAddbaTimeout(uint32_t record)
{
	const struct WlanAddbaPending *pending = (const struct WlanAddbaPending *)record;
	uint8_t response[10];
	UINT posture = sysIrqDisable();

	if (wlanBaAddbaTimeoutResponse(response, (const uint8_t *)&pending->request, &wlanAddbaDialogToken) != 0)
		wlanBaAddbaResponseReceived(pending->connection, 2, pending->request.peer, response);
	sysIrqRestore(posture);
}

/* Whether a Block Ack may be set up for the TID: not where the TID's access category is delivery enabled (U-APSD). */
int wlanBaAllowedForTid(struct WlanConnection *connection, unsigned int tid)
{
	if (connection->mode == 2 && connection->qosPeer == 1) {
		struct WlanConnectionRole *slot = wlanConnectionInfoSlot(connection);

		return !wlanApPsAcDeliveryEnabled(slot->uapsd.deliveryEnabled, wlanTidToAccessCategory(tid));
	}
	return 1;
}

/* The response of HostCmd 0xce when the request is refused: the body takes the response's layout. */
static void hostCmdAddbaRefuse(struct HostCmdAddbaRequest *request)
{
	uint8_t *body = (uint8_t *)request;

	put16(body + 14, U16(body + 12));
	put16(body + 12, U16(body + 10));
	body[10] = body[8];
	body[11] = body[9];
	body[8] = 1;
	body[9] = 0;
	body[0] = 1;
	wlanAddbaDialogToken = 0;
}

/*
 * HostCmd_CMD_11N_ADDBA_REQ: sends the ADDBA request of the host to its
 * peer. Returns 1 when the response is to come later, else the body is the
 * refused response.
 */
int hostCmd00ce(uint8_t *body, unsigned int length)
{
	struct HostCmdAddbaRequest *request = (struct HostCmdAddbaRequest *)body;
	struct WlanConnection *connection = wlanConnectionFindByPeer(request->peer);
	unsigned int tid;
	unsigned int hostTid;
	OsaBuffer *frame;
	unsigned int sequence;
	int result = -1;

	if (connection != NULL && (connection->flags & 0x40) &&
	    wlanBaAllowedForTid(connection, (body[8] >> 2) & 0xf)) {
		hostTid = (body[8] >> 2) & 0xf;
		if (connection->mode == 3 || connection->mode == 1) {
			tid = wlanWmmTid(hostTid);
			body[8] = (body[8] & 0xc3) | ((tid << 2) & 0x3c);
		} else {
			tid = hostTid;
		}
		result = wlanBaAddbaRequestCheck(body, wlanTxBaSessions, tid);
		if (result != -1) {
			wlanBaActionFrameAlloc(connection, &frame, request->peer);
			if (frame != NULL) {
				if (connection->interfaceType == WLAN_IF_IBSS || connection->mode == 2)
					sequence = wlanPeerNextSequence(connection, tid, 0);
				else
					sequence = wlanWmmLastSequence(tid);
				wlanBaAddbaRequestBuild(frame, sequence, request);
				body[8] = (body[8] & 0xc3) | ((hostTid << 2) & 0x3c);
				wlanAddbaDialogToken = request->dialogToken;
				if (wlanMgmtSendFrame(frame) != -1) {
					__aeabi_memcpy(&wlanAddbaPending, body, 14);
					wlanAddbaPending.connection = connection;
					wlanSoftTimerInit(&wlanAddbaTimer);
					wlanSoftTimerStartOnce(&wlanAddbaTimer, 1, wlanBaAddbaTimeout,
							       (uint32_t)&wlanAddbaPending,
							       WLAN_ADDBA_TIMEOUT_TICKS);
					return 1;
				}
			}
			result = -1;
		}
	}
	hostCmdAddbaRefuse(request);
	return result;
}

/* HostCmd_CMD_11N_ADDBA_RSP: sends the host's response to the ADDBA request of the peer. */
int hostCmd00cf(uint8_t *body, unsigned int length)
{
	struct HostCmdAddbaResponse *response = (struct HostCmdAddbaResponse *)body;
	struct WlanConnection *connection = wlanConnectionFindByPeer(response->peer);
	int result = -1;
	OsaBuffer *frame;

	if (connection != NULL && (connection->flags & 0x40)) {
		wlanBaActionFrameAlloc(connection, &frame, response->peer);
		if (frame != NULL) {
			wlanBaAddbaResponseBuild(frame, response);
			result = wlanMgmtSendFrame(frame);
		}
	}
	if (result == -1)
		body[0] = 1;
	else if (U16(response->status) == 0)
		wlanBaRxResponseApply(body, 0, wlanRxBaSessions, connection, wlanBaTimeoutCallback);
	return result;
}

/* HostCmd_CMD_11N_DELBA: sends a DELBA. Returns 1 when its transmission is to be waited for. */
int hostCmd00d0(uint8_t *body, unsigned int length)
{
	struct HostCmdDelba *delba = (struct HostCmdDelba *)body;
	struct WlanConnection *connection = wlanConnectionFindByPeer(delba->peer);
	OsaBuffer *frame;
	unsigned int paramSet;
	unsigned int reason;
	int result;

	if (connection == NULL || !(connection->flags & 0x40))
		return -1;
	if (connection->mode == 3 || connection->mode == 1)
		body[8] = (body[8] & 0xf) | (wlanWmmTid(body[8] >> 4) << 4);
	paramSet = U16(delba->paramSet);
	reason = U16(delba->reason);
	wlanBaActionFrameAlloc(connection, &frame, delba->peer);
	if (frame != NULL) {
		wlanBaDelbaBuild(frame, paramSet, reason);
		result = wlanMgmtSendFrame(frame);
	} else {
		result = -1;
	}
	if (result == -1) {
		body[0] = 1;
	} else if (result == 0) {
		__aeabi_memcpy(&wlanDelbaPending, body, 11);
		wlanDelbaPending.connection = connection;
		wlanSoftTimerInit(&wlanDelbaTimer);
		wlanSoftTimerStartOnce(&wlanDelbaTimer, 1, wlanBaDelbaTimeout,
				       (uint32_t)&wlanDelbaPending, WLAN_ADDBA_TIMEOUT_TICKS);
		return 1;
	}
	return result;
}

/* HostCmd 0xe9: sends a 20/40 BSS coexistence frame; 1 is success. */
int hostCmd00e9(struct WlanConnection *connection, uint8_t *body, unsigned int length)
{
	int result = wlanBssCoexFrameSend(connection, body, length);

	if (result == 0)
		return 1;
	return result;
}

/* A beacon of the BSS told its HT operation: the connection takes its protection and its guard-interval state over. */
void wlanHtBeaconApply(struct WlanConnection *connection, const WlanIePointers *ies,
		       const uint8_t *source)
{
	const uint8_t *htCapability = (const uint8_t *)ies->pHtCap;
	const uint8_t *htInfo = ies->pHtInfo;
	uint8_t *operation = (uint8_t *)wlanConnectionHtOperation(connection);

	if (!(connection->flags & 0x40))
		return;
	if (connection->interfaceType == WLAN_IF_IBSS) {
		wlanHtProtectionObserve(&wlanProtection, ies, source, connection->localAddress, 5000000);
		return;
	}
	if ((connection->mode == 3 || connection->mode == 1) && htInfo != NULL && htCapability != NULL) {
		if (((operation[4] >> 2) & 1) != ((htInfo[4] >> 2) & 1))
			operation[4] = (operation[4] & 0xfb) | (htInfo[4] & 4);
		if (((operation[4] >> 4) & 1) != ((htInfo[4] >> 4) & 1))
			operation[4] = (operation[4] & 0xef) | (htInfo[4] & 0x10);
		if (((operation[3] >> 3) & 1) != ((htInfo[3] >> 3) & 1))
			operation[3] = (operation[3] & 0xf7) | (htInfo[3] & 8);
		if ((operation[4] & 3) != (htInfo[4] & 3))
			operation[4] = (operation[4] & 0xfc) | (htInfo[4] & 3);
		wlanRateGreenfieldSet(connection, 0, wlanHtGreenfieldWanted(connection));
	}
}

/* HostCmd_CMD_11N_CFG, set: the capability and the transmit information the host gave; the rates follow. */
void hostCmd00cdSet(struct WlanConnection *connection, uint8_t *body)
{
	wlanHtTxCapability[0] = body[2];
	wlanHtTxCapability[1] = body[3];
	wlanHtTxInfo[0] = body[4];
	wlanHtTxInfo[1] = body[5];
	wlanRateChannelWidthSet(connection, 0, wlanHtChannelWidth(connection));
	wlanRateGreenfieldSet(connection, 0, wlanHtGreenfieldWanted(connection));
}

void hostCmd00cdGet(uint8_t *body)
{
	body[2] = wlanHtTxCapability[0];
	body[3] = wlanHtTxCapability[1];
	body[4] = wlanHtTxInfo[0];
	body[5] = wlanHtTxInfo[1];
}

/* Deletes the Block Ack sessions of the peer, in both directions, and tells the host. */
void wlanBaSessionsDelete(struct WlanConnection *connection, const uint8_t *peer)
{
	unsigned int tid;
	uint8_t slot;

	for (tid = 0; tid < 8; tid = (uint8_t)(tid + 1)) {
		if (wlanRxBaHwSlot(tid, peer, &slot, wlanRxBaSessions) == 1) {
			macRxBaDelete(peer, tid);
			wlanBaTimeoutCancel(peer, tid, 0);
		}
		if (wlanTxBaProbe(tid, peer, wlanTxBaSessions) == 1) {
			macTxBaAbort(connection, peer, tid);
			wlanBaTimeoutCancel(peer, tid, 1);
		}
	}
}

/* Gives up the HT elements of the connection and the ones the BSS shares, and the Block Ack sessions of its peer. */
void wlanHtElementsReset(struct WlanConnection *connection)
{
	connection->flags &= 0x3f;
	wlanHtElementsClear(wlanHtCapabilityCache, NULL, NULL, NULL, NULL);
	wlanHtElementsClear(connection->htCapability, wlanHtOperationIe, wlanHtCoexIe, NULL,
			    wlanHtOverlapScanIe);
	wlanBaSessionsDelete(connection, connection->peerAddress);
	wlanRateGreenfieldSet(connection, 0, 0);
	MAC_REG(0x138) &= ~0x10u;
}

/* The host's HT configuration starts with the short guard interval and nothing else (0x20). */
int wlanHtElementsInit(void)
{
	wlanHtTxCapability[1] = 0;
	wlanHtTxInfo[0] = 0;
	wlanHtTxInfo[1] = 0;
	wlanHtTxCapability[0] = 32;
	return 0;
}

int wlanHtInitStageRegister(void)
{
	wlanHtElementsInit();
	wlanHtInitStage.run = wlanHtElementsInit;
	return wlanInitStageRegister(&wlanHtInitStage);
}


/* A Block Ack or coexistence frame of ours has gone out (or failed): the DELBA's timer and the host's replies. */
void wlanBaActionSent(OsaBuffer *frame, int success)
{
	struct WlanConnection *connection = frame->context;
	struct WlanReplyFrame *data = (struct WlanReplyFrame *)((uint8_t *)frame + frame->dataOffset);
	WlanWirelessFrame *wireless = (WlanWirelessFrame *)data->frame;
	const uint8_t *body = wireless->body;
	const uint8_t *peer = wireless->address1;

	if (wlanBaIsDelba(body)) {
		if (wlanBaDelbaCheck(success, body, connection, peer, &wlanDelbaTimer))
			wlanHostDelbaReply(connection, body, success, peer);
	} else if (wlanBssCoexIsFrame(body)) {
		if (connection->interfaceType != WLAN_IF_STATION)
			wlanHostCoexistenceReply(connection, body);
	}
}

/*
 * A 20/40 BSS coexistence management frame arrived (the body: category,
 * action, the coexistence element, then intolerant channel reports): a
 * 40 MHz BSS falls back to 20 MHz when a station says it is intolerant or
 * reports a channel inside the 40 MHz span, and an information request is
 * answered.
 */
int wlanBssCoexFrameReceived(struct WlanConnection *connection, const uint8_t *body,
			     const uint8_t *frame, int unused)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	int conflict = 0;

	if (U16(frame) > 5) {
		unsigned int remaining = (uint8_t)(frame[0] - 5);
		const uint8_t *cursor = body + 5;
		struct WlanConnection *own = wlanConnectionFindByTypeNumber(connection->interfaceType,
									   connection->bssNumber);
		struct WlanConnectionRole *slot = wlanConnectionInfoSlot(own);
		int low = (int8_t)slot->channel;
		int high = low;
		unsigned int offset = (slot->channelFlags >> 4) & 3;

		/* The span of the 40 MHz channel: the primary channel and the one above or below it. */
		if (offset == 1) {
			low = low - 3 > 1 ? low - 3 : 1;
			high = low + 7 < 11 ? low + 7 : 11;
		} else if (offset == 3) {
			low = low - 7 > 1 ? low - 7 : 1;
			high = low + 3 < 11 ? low + 3 : 11;
		}
		while (remaining != 0 && !conflict) {
			const uint8_t *report = osaFindTlv(cursor, 73, remaining);
			unsigned int count;
			unsigned int index;
			unsigned int length;

			if (report == NULL)
				break;
			length = (uint8_t)(report[1] + 2);
			count = report[1] - 1;
			for (index = 0; count > index; index = (uint8_t)(index + 1)) {
				int channel = report[index + 3];

				if (channel >= low && channel <= high && slot->channel != channel) {
					conflict = 1;
					break;
				}
			}
			remaining = (uint8_t)(remaining - (uint8_t)(report - cursor) - length);
			cursor = report + length;
		}
	}
	if ((info->htCapability[2] & 2) && (((body[4] >> 1) & 3) != 0 || conflict == 1))
		macMgmtManagePacket(connection, 0);
	if (body[4] & 9) {
		uint8_t reply[3] = { 72, 1, 0 };

		wlanBssCoexFrameSend(connection, reply, 3);
	}
	return 1;
}

/* A Block Ack action frame arrived from the peer (the body: category, action code, then the action's fields). */
int wlanBaActionReceived(struct WlanConnection *connection, const uint8_t *body,
			 const uint8_t *frame, int unused)
{
	const uint8_t *peer = frame + 12;
	OsaBuffer *reply;

	switch (body[1]) {
	case WLAN_BA_ACTION_ADDBA_REQUEST:
		if (connection->flags & 0x40) {
			wlanBaAddbaEventPost(peer, body, connection);
		} else {
			wlanBaActionFrameAlloc(connection, &reply, peer);
			if (reply != NULL) {
				wlanBaAddbaDeclineBuild(reply, body);
				wlanMgmtSendFrame(reply);
			}
		}
		break;
	case WLAN_BA_ACTION_ADDBA_RESPONSE: {
		UINT posture = sysIrqDisable();

		if (body[2] == wlanAddbaDialogToken) {
			wlanAddbaDialogToken = 0;
			wlanBaAddbaResponseReceived(connection, 0, peer, (uint8_t *)body);
		}
		sysIrqRestore(posture);
		break;
	}
	case WLAN_BA_ACTION_DELBA:
		if (connection->flags2 & WLAN_CONN2_ASSOCIATED)
			wlanBaDelbaReceived(connection, peer, body);
		break;
	default:
		return 0;
	}
	return 1;
}

/* Sets up the Block Ack timeout table and the hooks that add and delete sessions. */
void wlanBaTimeoutOpsInit(void)
{
	wlanBaTimeoutCount = 12;
	wlanBaTimeoutTable = wlanBaTimeouts;
	wlanTxBaDeleteHook = macTxBaDelete;
	wlanRxBaDeleteHook = macRxBaDelete;
	wlanTxBaAddHook = macTxBaAdd;
}

void wlanTxRingMovePending(unsigned int ring, unsigned int target)
{
	wlanTxRingRetargetAll(ring, target, (uint32_t *)wlanTxRingReadIndex[ring], wlanTxRingWriteIndex[ring]);
}

void wlanTxRingMoveToWindow(unsigned int target, unsigned int ring, unsigned int type,
			    const uint8_t *address)
{
	wlanTxRingRetargetMatching(target, ring, type, address, (uint32_t *)wlanTxRingReadIndex[ring],
				   wlanTxRingWriteIndex[ring]);
}
