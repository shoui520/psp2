/*
 * The management frames of joining a BSS and leaving it (0x336e0 - 0x341ca; and the fmCoex
 * tables that build their bodies, 0x4bf66 - 0x4c0cc): the authentication, association and
 * reassociation requests a station sends (wlan/join.c asks for them), what it does with the
 * association and authentication responses and with a deauthentication or disassociation it
 * receives, the association response an AMP link answers with, and the request to deauthenticate
 * a peer after a delay. Marvell published no source for it; the names are the firmware's own
 * placeholder names, kept ("macTxNotify*Frame", "*Header", "fmCoex*Table"), and the file name
 * and the fields' are inferred.
 */
#include <stddef.h>
#include "robin/btpower.h"
#include "robin/p2pgo.h"
#include "robin/ratesel.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/join.h"
#include "robin/joinframe.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/peerrate.h"
#include "robin/rom.h"
#include "robin/signal.h"
#include "robin/slot.h"
#include "robin/subscribe.h"
#include "robin/task.h"
#include "robin/txbuild.h"
#include "robin/txring.h"
#include "robin/wapi.h"
#include "robin/wlan.h"
#include "robin/wmm.h"
#include "robin/stamachine.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))
#define BUFFER_FRAME(buffer)	((WlanWirelessFrame *)((uint8_t *)(buffer) + (buffer)->dataOffset))

#define WLAN_SUBTYPE_ASSOC_REQUEST	0
#define WLAN_SUBTYPE_ASSOC_RESPONSE	1
#define WLAN_SUBTYPE_REASSOC_REQUEST	2
#define WLAN_SUBTYPE_AUTH		11

#define WLAN_MGMT_FRAME_SIZE_JOIN	500	/* what an association frame is allocated with */
#define WLAN_MGMT_FRAME_SIZE_AUTH	348

/* The kinds macTxNotifyRecordFrame() is told of (wlanJoinAuthResult()'s results come out of them). */
#define WLAN_JOIN_RECORD_OK		0
#define WLAN_JOIN_RECORD_NO_BUFFER	1
#define WLAN_JOIN_RECORD_REFUSED	5

/* The tag of a deauthentication the leave request sent: what the transmit-done task reports it by. */
#define WLAN_TX_TAG_LEAVE		19

extern int wlanPsQueryDescriptor(const uint8_t *ies, int length, WlanBeaconIePointers *out);	/* 0x3e94a */
extern uint8_t wlanAssocHtCapability[28];			/* SQRAM 0xc0032170: the copy of it for the association */
int p2pDirectPrepareState(const uint8_t *address);				/* 0x3f3e4 */
int rfCalibSyncDescriptor(void);						/* 0x2a20c */
int macMgmtQueryState(uint32_t flags, uint32_t extra);				/* 0x34b82 */
extern uint8_t ampScanChannel;					/* amp/wlanif.c */

/* The capability sent in the last (re)association request, and the rates it announced as a bitmap. */
static uint16_t wlanAssocCapability __attribute__((section(".itcm.rw"))) = 0;	/* 0x55128 */
static uint32_t wlanAssocRateBitmap __attribute__((section(".itcm.rw"))) = 0;	/* 0x5512c */

/* The deauthentication being sent (macTxNotifyBuildHeader()): at most one at a time (24 bytes). */
struct WlanLeave {
	uint8_t peer[6];
	uint16_t reason;
	uint16_t result;		/* what macTxNotifyPrepareHeader() was told */
	uint8_t reserved10[2];
	int (*callback)(int status, const uint8_t *address);
	uint32_t slot;			/* the channel time request's handle; 0: none */
	struct WlanConnection *connection;
};

_Static_assert(sizeof(struct WlanLeave) == 24, "deauthentication request");

static struct WlanLeave wlanLeave __attribute__((section(".itcm.bss")));	/* 0x5e898 */

/* The connection takes its peer's capability, association ID and address. */
__attribute__((noinline)) void macTxNotifyManageFrame(struct WlanConnection *connection, uint16_t capability,
			    uint16_t associationId, const uint8_t *peer)
{
	connection->bssCapability[0] = (uint8_t)capability;
	connection->bssCapability[1] = (uint8_t)(capability >> 8);
	connection->associationId = associationId;
	__aeabi_memcpy(connection->peerAddress, peer, 6);
	rfCalibSyncDescriptor();
}

/*
 * The elements of an association request after the fixed fields: the SSID, the rates, the WMM
 * (or, on an AMP link, QoS capability) element, our HT capability and the host's own elements;
 * then the frame is sent.
 */
int macTxNotifyAcquireFrame(OsaBuffer *buffer, const uint8_t *ssid, const uint8_t *rates,
			    const uint8_t *ies, int iesLength)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + 176);
	uint32_t qosCapability = 302;		/* element 46, length 1, QoS info 0 */
	unsigned int offset;

	buffer->dataOffset = 176;
	offset = U16(frame->bodyLength);
	fmCoexTrackTable(frame, ssid, rates, NULL);
	wlanAssocRateBitmap = wlanRateBitmapAll(rates, 14, NULL, 0, (const WlanHtCapabilityIe *)wlanJoinHtCapability);
	if (connection->interfaceType == WLAN_IF_AMP && ampScanChannel != 0)
		__aeabi_memcpy(wlanBssWmmIe, &qosCapability, 3);
	else if (wlanWmmHasElement() == 0)
		goto noWmm;
	wlanAppendIe(frame, wlanBssWmmIe[0], &wlanBssWmmIe[2], wlanBssWmmIe[1]);
noWmm:
	if (wlanJoinHtCapability[0] != 0)
		wlanAppendIe(frame, wlanJoinHtCapability[0], &wlanJoinHtCapability[2], wlanJoinHtCapability[1]);
	if (iesLength != 0 && ies != NULL)
		wlanAppendIeList(frame, (uint8_t)offset, ies, iesLength);
	wlanCustomIeApply(connection, 1, frame, offset);
	if (connection != NULL && connection->interfaceType == WLAN_IF_AP) {
		unsigned int include;

		wlanConnectionPsState(connection);
		include = connection->mode == 3 ? 0x2000 : 0;
		PUT_U16(frame->bodyLength,
			p2pDirectCheckContext(connection, frame, NULL, include | 4, 0) +
			U16(frame->bodyLength));
	}
	if (wlanMgmtSendFrame(buffer) != -1)
		return 0;
	macTxNotifyAdjustFrame(1, 0);
	return -1;
}

/* An association request. */
int macTxNotifyReleaseFrame(struct WlanConnection *connection,
			    const struct WlanJoinAssocRequest *request)
{
	OsaBuffer *buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_JOIN);
	WlanWirelessFrame *frame;

	if (buffer == NULL) {
		macTxNotifyAdjustFrame(1, 0);
		return -1;
	}
	frame = BUFFER_FRAME(buffer);
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, request->peer,
			   WLAN_SUBTYPE_ASSOC_REQUEST, request->peer);
	wlanAssocCapability = request->capability;
	fmCoexNotifyTable((uint8_t *)frame, wlanAssocCapability, request->listenInterval, NULL);
	return macTxNotifyAcquireFrame(buffer, request->ssid, request->rates, request->ies,
				       request->iesLength);
}

/* A reassociation request: it names the access point the station was associated with. */
int macTxNotifySetupFrame(struct WlanConnection *connection,
			  const struct WlanJoinAssocRequest *request,
			  const struct WlanJoinBss *previous)
{
	OsaBuffer *buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_JOIN);
	WlanWirelessFrame *frame;

	if (buffer == NULL) {
		macTxNotifyAdjustFrame(1, 0);
		return -1;
	}
	frame = BUFFER_FRAME(buffer);
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, request->peer,
			   WLAN_SUBTYPE_REASSOC_REQUEST, request->peer);
	wlanAssocCapability = request->capability;
	fmCoexNotifyTable((uint8_t *)frame, wlanAssocCapability, request->listenInterval,
			  previous->bssid);
	return macTxNotifyAcquireFrame(buffer, request->ssid, request->rates, request->ies,
				       request->iesLength);
}

/*
 * An association response: a refusal is reported at once; otherwise the connection takes
 * the BSS's elements (HT, rates, WMM), the peer's capability, association ID and address, and
 * the join is done.
 */
void macTxNotifyClearFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	WlanBeaconIePointers ies;
	uint8_t rates[16];
	uint8_t length = 0;
	uint8_t *htOperation;
	uint8_t *htCoexistence;
	uint8_t *htOverlapScan;
	unsigned int status;
	uint32_t rateBitmap;
	uint8_t flags;
	uint32_t extra;
	int state;
	int rateCount;
	int result;

	if (connection == NULL) {
		result = 1;
		status = 6;
		goto done;
	}
	htOperation = (uint8_t *)wlanConnectionHtOperation(connection);
	htCoexistence = (uint8_t *)wlanConnectionHtCoexistence(connection);
	htOverlapScan = (uint8_t *)wlanConnectionHtOverlapScan(connection);
	status = U16(frame->body + 2);
	if (status != 0) {
		result = 3;
		goto done;
	}
	wlanMacReleaseHold();
	wlanPsQueryDescriptor(frame->body + 6, (uint16_t)(U16(frame->bodyLength) - 6), &ies);
	if (wlanJoinHtCapability[0] != 0) {
		__aeabi_memcpy(wlanAssocHtCapability, wlanJoinHtCapability, 28);
		if (fmCoexFormatTable(&ies, connection->htCapability, htOperation, htCoexistence,
				      htOverlapScan) != 0) {
			connection->flags |= WLAN_CONN_HT_ENABLED;
			if ((connection->htCapability[2] & 2) && (wlanAssocHtCapability[2] & 2))
				connection->flags |= WLAN_CONN_HT40;
		}
	}
	rateCount = fmCoexInspectTable(&ies, rates, &length,
				       (const WlanHtCapabilityIe *)connection->htCapability);
	rateBitmap = macMgmtComputeContext(connection, rateCount) & wlanAssocRateBitmap;
	wlanRateGreenfieldSet(connection, 0, wlanHtGreenfieldWanted(connection));
	wlanPsQueryPacket();
	if (wlanConnectionIsHt(connection) != 0)
		macMgmtConfigureContext(htOperation);
	macMgmtCheckContext();
	macMgmtTrackContext(frame->body, rateBitmap, NULL, romGetMicroseconds());
	connection->supportedRatesLength = length;
	__aeabi_memcpy4(connection->supportedRates, rates, length);
	macMgmtQueryContext(connection, connection->supportedRates, connection->supportedRatesLength,
			    rateBitmap);
	macTxNotifyApplyFrame(connection);
	wlanSignalReset(connection);
	wlanSignalJoinDone(connection);
	macTxNotifyManageFrame(connection, wlanAssocCapability, U16(frame->body + 4), frame->address2);
	macMgmtVerifyContext();
	wlanSubscribeResetFailures(connection);
	macMgmtPrepareContext(&flags, &extra);
	if (flags & 3)
		state = 0;
	else
		state = macMgmtQueryState(flags, extra);
	btLmpLcHandleState(state);
	result = 0;
	status = 0;
done:
	macTxNotifyAdjustFrame(result, status);
}

/* A disassociation: the host is told; an associated connection counts it as a refused association. */
int macTxNotifyConvertFrame(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	uint8_t event[8];

	if (connection->state == 3)
		macTxNotifyAdjustFrame(3, U16(frame->body));
	if (U16(frame->bodyLength) < 2)
		return -1;
	__aeabi_memcpy(event, frame->address2, 6);
	event[6] = frame->body[0];
	event[7] = frame->body[1];
	wlanReportEvent(connection, event, 9);
	return 0;
}

void macTxNotifyVerifyFrame(void)
{
	wlanAssocCapability = 0;
	wlanTxPowerSetLinkLimit(255, 0);
}

/*
 * An association request an AMP link received: answered with the success response (association
 * ID 1, the rates, the EDCA parameter set when the peer sent a QoS capability); the link then
 * waits for the key handshake.
 */
int macTxNotifySelectFrame(OsaBuffer *buffer)
{
	static const uint8_t edcaParameterSet[20] = {
		12, 18, 0x80, 0x00, 0x03, 0xa4, 0x00, 0x00, 0x27, 0xa4,
		0x00, 0x00, 0x42, 0x43, 0x5e, 0x00, 0x62, 0x32, 0x2f, 0x00,
	};
	uint8_t edca[20];
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	const uint8_t *peer;
	WlanIePointers ies;
	OsaBuffer *reply;
	WlanWirelessFrame *response;
	unsigned int length;
	unsigned int extended = 0;
	unsigned int count;

	__aeabi_memcpy4(edca, edcaParameterSet, sizeof(edca));
	length = U16(frame->bodyLength);
	if (frame->frameControl[1] & 1)
		peer = frame->address3;
	else
		peer = frame->address2;
	if (connection->linkFlags != 1)
		return -1;
	wlanParseIes(frame->body + 4, (uint16_t)(length - 98), &ies);
	if (ies.pQosCapability != NULL && ampScanChannel != 0)
		wlanWmmSet(connection, 1, 0);
	if (!(connection->flags & WLAN_CONN_PRIVACY))
		return -1;
	if (ies.pRsn == NULL)
		return -1;
	reply = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_JOIN);
	if (reply == NULL)
		return -1;
	response = BUFFER_FRAME(reply);
	wlanMgmtBuildFrame(reply, connection->interfaceAddress, peer, WLAN_SUBTYPE_ASSOC_RESPONSE,
			   connection->interfaceAddress);
	response->bodyLength[0] = 0;
	response->bodyLength[1] = 0;
	response->body[0] = connection->bssCapability[0];
	response->body[1] = connection->bssCapability[1];
	response->body[0] = (response->body[0] & 0xfc) | 1;
	response->body[2] = 0;
	response->body[3] = 0;
	response->body[4] = 1;
	response->body[5] = 0;
	response->bodyLength[0] = 6;
	response->bodyLength[1] = 0;
	count = connection->supportedRatesLength;
	if (count > 8) {
		extended = (uint8_t)(count - 8);
		count = 8;
	}
	wlanAppendIe(response, IE_SUPPORTED_RATES, connection->supportedRates, count);
	if (ies.pQosCapability != NULL && ampScanChannel != 0)
		wlanAppendIe(response, edca[0], &edca[2], edca[1]);
	if (extended != 0)
		wlanAppendIe(response, IE_EXTENDED_RATES, &connection->supportedRates[count], extended);
	if (wlanMgmtSendFrame(reply) == -1)
		return -1;
	if (connection->interfaceType == WLAN_IF_AMP)
		connection->linkFlags = 2;
	connection->flags2 |= WLAN_CONN2_ASSOCIATED;
	osaTaskSleep(100);
	if (connection->flags & WLAN_CONN_PRIVACY)
		btampGeneratePWKMsg1(connection);
	return 0;
}

/* The join's association result as wlanJoinAssocResult() takes it: 0 done, 1 failed, 2 refused. */
__attribute__((noinline)) void macTxNotifyAdjustFrame(int result, uint16_t statusCode)
{
	if (result != 0) {
		if (result == 3)
			result = 2;
		else
			result = 1;
	}
	wlanJoinAssocResult(result, statusCode);
}

/* The join's authentication result: kind 0 done, 4 an unsupported algorithm, 5 refused, else failed. */
__attribute__((noinline)) void macTxNotifyRecordFrame(unsigned int algorithm, int kind, const uint8_t *peer,
			    unsigned int statusCode)
{
	int result;

	(void)algorithm;
	(void)peer;
	if (kind != 0) {
		if (kind == 4)
			result = 9;
		else if (kind == 5)
			result = 7;
		else
			result = 6;
	} else {
		result = 5;
	}
	wlanJoinAuthResult(result, statusCode);
}

/* An authentication request, in the transaction sequence number the request names (1 by default). */
int macTxNotifyFilterFrame(struct WlanConnection *connection,
			   const struct WlanJoinAuthRequest *request)
{
	unsigned int sequence = 1;
	OsaBuffer *buffer;
	WlanWirelessFrame *frame;

	if (U16(request->sequence) != 0)
		sequence = U16(request->sequence);
	buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_AUTH);
	if (buffer == NULL) {
		macTxNotifyRecordFrame(request->algorithm, WLAN_JOIN_RECORD_NO_BUFFER, request->peer, 0);
		return -1;
	}
	frame = BUFFER_FRAME(buffer);
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, request->peer, WLAN_SUBTYPE_AUTH,
			   request->peer);
	connection->authAlgorithm = request->algorithm;
	connection->authSequence = sequence + 1;
	fmCoexScheduleTable((uint8_t *)frame, request->algorithm, sequence, 0, NULL);
	if (wlanMgmtSendFrame(buffer) == -1)
		macTxNotifyRecordFrame(request->algorithm, WLAN_JOIN_RECORD_NO_BUFFER, request->peer, 0);
	return 0;
}

/* Deauthenticates the peer the request names; -1 when it could not be sent, and a leave is pending. */
int macTxNotifyHandleHeader(struct WlanConnection *connection,
			    const struct WlanLeaveRequest *request, int notify)
{
	int result = wlanMgmtSendDeauth(connection, request->peer, request->peer, U16(request->reason),
					notify);

	if (result == -1 && notify != 0)
		macTxNotifyPrepareHeader(3);
	return result;
}

/*
 * The second authentication frame of the shared key algorithm: the challenge text is answered
 * with the third frame, encrypted with the key of the connection (the group key for a group
 * address, else the pairwise key). 1 when it was sent.
 */
int macTxNotifyProcessHeader(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	unsigned int status = U16(frame->body + 4);
	OsaBuffer *reply;
	WlanWirelessFrame *out;
	const struct WlanKeyBuffer *key;
	int kind;

	kind = WLAN_JOIN_RECORD_REFUSED;
	if (status == 0) {
		reply = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_AUTH);
		if (reply != NULL)
			goto build;
		kind = WLAN_JOIN_RECORD_NO_BUFFER;
	}
	macTxNotifyRecordFrame(1, kind, frame->address2, status);
	return -1;
build:
	out = BUFFER_FRAME(reply);
	wlanMgmtBuildFrame(reply, connection->interfaceAddress, frame->address2, WLAN_SUBTYPE_AUTH,
			   frame->address2);
	connection->authSequence = 4;
	fmCoexScheduleTable((uint8_t *)out, 1, 3, 0, frame->body + 8);
	if (out->address1[0] & 1)
		key = connection->keyBlocks[0];
	else
		key = connection->keyBlocks[1];
	if (key == NULL) {
		osa_BufferFree(reply);
		return -1;
	}
	wlanTxEncryptWepFragment((uint8_t *)out, U16(out->bodyLength),
				 (const WlanWepHostConfig *)&key->key);
	if (wlanMgmtSendFrame(reply) == -1)
		return -1;
	return 1;
}

/*
 * An authentication response: the join's authentication result for a refusal or an unknown
 * algorithm, the next step for the algorithm and sequence that were asked for. 1: it was not
 * the answer to the request; 0 and 1: the join goes on; -1: it failed.
 */
int macTxNotifyUpdateHeader(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	unsigned int algorithm = U16(frame->body);
	unsigned int sequence;
	unsigned int status = 0;
	unsigned int recorded = frame->body[0];
	int kind;
	int result = -1;

	if (connection == NULL) {
		kind = WLAN_JOIN_RECORD_NO_BUFFER;
		status = 6;
	} else if (algorithm >= 2 && algorithm != 128) {
		status = U16(frame->body + 4);
		kind = status != 0 ? WLAN_JOIN_RECORD_REFUSED : 4;
		if (status == 0)
			status = 13;
	} else {
		if (algorithm != connection->authAlgorithm)
			return 1;
		sequence = U16(frame->body + 2);
		if (sequence != connection->authSequence)
			return 1;
		if (algorithm != 1)
			return macTxNotifyValidateHeader(frame);
		if (sequence == 2)
			return macTxNotifyProcessHeader(buffer);
		if (sequence != 4)
			return -1;
		recorded = 1;
		status = U16(frame->body + 4);
		kind = status != 0 ? WLAN_JOIN_RECORD_REFUSED : WLAN_JOIN_RECORD_OK;
		result = status != 0 ? -1 : 0;
	}
	macTxNotifyRecordFrame(recorded, kind, frame->address2, status);
	return result;
}

/* A deauthentication: the host is told. */
int macTxNotifyConfigureHeader(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	uint8_t event[8];

	if (U16(frame->bodyLength) < 2)
		return -1;
	__aeabi_memcpy(event, frame->address2, 6);
	event[6] = frame->body[0];
	event[7] = frame->body[1];
	wlanReportEvent(connection, event, 8);
	return 0;
}

/* The authentication frame an AMP link answers an authentication request with: sequence 2, success. */
int macTxNotifyCheckHeader(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = BUFFER_FRAME(buffer);
	const uint8_t *peer;
	OsaBuffer *reply;
	WlanWirelessFrame *response;

	if (frame->frameControl[1] & 1)
		peer = frame->address3;
	else
		peer = frame->address2;
	reply = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE_AUTH);
	if (reply == NULL)
		return -1;
	response = BUFFER_FRAME(reply);
	wlanMgmtBuildFrame(reply, connection->interfaceAddress, peer, WLAN_SUBTYPE_AUTH,
			   connection->interfaceAddress);
	response->body[0] = 0;
	response->body[1] = 0;
	response->body[2] = 2;
	response->body[3] = 0;
	response->body[4] = 0;
	response->body[5] = 0;
	response->bodyLength[0] = 6;
	response->bodyLength[1] = 0;
	if (wlanMgmtSendFrame(reply) == -1)
		return -1;
	if (connection->interfaceType == WLAN_IF_AMP)
		connection->linkFlags = 1;
	return 0;
}

/* The authentication frame's response: the join's authentication result; 0 when it was a success. */
int macTxNotifyValidateHeader(const WlanWirelessFrame *frame)
{
	unsigned int status;

	if (U16(frame->body + 2) == 2) {
		status = U16(frame->body + 4);
		macTxNotifyRecordFrame(frame->body[0], status != 0 ? 5 : 0, frame->address2, status);
		return 0;
	}
	macTxNotifyRecordFrame(frame->body[0], 4, frame->address2, 14);
	return -1;
}

/* The leave request is over: the channel time it asked for is given up (6: not ours; 8: our own address). */
__attribute__((noinline)) void macTxNotifyPrepareHeader(unsigned int result)
{
	wlanLeave.result = result;
	wlanSlotAbort(wlanLeave.slot, wlanConnectionHasLocalAddress(wlanLeave.connection, wlanLeave.peer) ? 8 : 6);
}

/* What the channel time request of the leave tells: the deauthentication goes out when it starts. */
int macTxNotifyDispatchHeader(uint32_t handle, int event, uint32_t argument)
{
	(void)argument;
	if (wlanLeave.slot == handle) {
		switch (event) {
		case 3:
			if ((wlanLeave.connection->flags2 & WLAN_CONN2_ASSOCIATED) && wlanLeave.callback == NULL &&
			    wlanConnectionHasLocalAddress(wlanLeave.connection, wlanLeave.peer))
				wlanSlotAbort(wlanLeave.slot, 6);
			else
				wlanMgmtSendDeauth(wlanLeave.connection, wlanLeave.peer, wlanLeave.peer,
						   wlanLeave.reason, 19);
			break;
		case 6:
		case 8:
			macTxNotifyResetHeader(0, wlanLeave.peer);
			wlanLeave.slot = 0;
			break;
		case 11:
			macTxNotifyResetHeader(2, wlanLeave.peer);
			wlanLeave.slot = 0;
			break;
		case 9:
			macTxNotifyPrepareHeader(3);
			break;
		}
		return 3;
	}
	if (wlanLeave.slot != 0)
		macTxNotifyPrepareHeader(3);
	return 4;
}

/* The transmit status of the deauthentication or disassociation the leave sent. */
int macTxNotifyParseHeader(uint32_t handle, const struct WlanTxStatus *status)
{
	unsigned int control;

	(void)handle;
	if (status != NULL) {
		control = ((const uint8_t *)TX_DATA_FRAME(status->data))[2];
		if (((control >> 2) & 3) == 0 && ((control >> 4) == 12 || (control >> 4) == 10) &&
		    TX_DATA_TAG(status->data) == WLAN_TX_TAG_LEAVE) {
			macTxNotifyPrepareHeader(status->result != 0 ? 3 : 0);
			return 1;
		}
	}
	return 0;
}

/*
 * Deauthenticates the peer after a delay: the request is queued as a channel time request, and
 * the callback is told how it went (0 sent or given up, 1 the peer is a direct peer, 2 a leave
 * is pending already).
 */
void macTxNotifyBuildHeader(struct WlanConnection *connection, const uint8_t *peer, uint16_t reason,
			    int (*callback)(int status, const uint8_t *address), uint32_t timeout)
{
	struct WlanSlotParams params;
	uint8_t band;
	uint32_t channel;

	if (wlanLeave.slot != 0) {
		if (callback != NULL)
			callback(2, peer);
		return;
	}
	__aeabi_memclr4(&params, sizeof(params));
	macMgmtPrepareContext(&band, &channel);
	params.callback = (int (*)(uint32_t, int, uint32_t))macTxNotifyDispatchHeader;
	params.context = (uint32_t)(uintptr_t)macTxNotifyParseHeader;
	wlanLeave.connection = connection;
	if (peer != NULL && !p2pDirectPrepareState(peer)) {
		if (wlanAddressIsBroadcast(peer)) {
			wlanReportEvent(connection, NULL, 7);
			if (callback != NULL)
				callback(0, connection->localAddress);
			return;
		}
		__aeabi_memcpy(wlanLeave.peer, peer, 6);
	} else {
		__aeabi_memcpy(wlanLeave.peer, connection->localAddress, 6);
	}
	if (p2pDirectPrepareState(wlanLeave.peer)) {
		if (callback != NULL)
			callback(1, wlanLeave.peer);
		return;
	}
	params.mode = channel;
	params.bssTypeLow = band & 3;
	params.bssTypeMiddle = (band >> 2) & 3;
	params.bssTypeHigh = (band >> 4) & 3;
	params.flags = (channel == (uint8_t)channel ? 2 * (params.flags >> 1) : params.flags | 1) | 4;
	params.delay = timeout;
	params.tag = 20000;
	params.connection = connection;
	wlanLeave.reason = reason;
	wlanLeave.callback = callback;
	wlanLeave.slot = wlanSlotSubmit(&params);
	if (wlanLeave.slot == 0)
		macTxNotifyResetHeader(0, peer);
}

/* The leave's end: the management state is told, with the address and the result. */
void macTxNotifyComputeHeader(int status, const uint8_t *address)
{
	uint8_t event[8];

	__aeabi_memcpy(event, address, 6);
	event[6] = (uint8_t)status;
	macMgmtFormatState(wlanLeave.connection, event);
}

__attribute__((noinline)) void macTxNotifyResetHeader(int status, const uint8_t *address)
{
	if (wlanLeave.callback != NULL)
		wlanLeave.callback(status, address);
}

/* The body of a (re)association request begins with the capability and the listen interval. */
__attribute__((noinline)) void fmCoexNotifyTable(uint8_t *frame, uint16_t capability, uint16_t listenInterval,
		       const uint8_t *currentAp)
{
	WlanWirelessFrame *request = (WlanWirelessFrame *)frame;
	unsigned int length = 4;

	PUT_U16(request->body, capability);
	PUT_U16(request->body + 2, listenInterval);
	if (currentAp != NULL) {
		__aeabi_memcpy(request->body + 4, currentAp, 6);
		length = 10;
	}
	PUT_U16(request->bodyLength, length);
}

/* The elements of an association response, as the connection keeps them: HT, 20/40 and scan. */
int fmCoexFormatTable(const WlanBeaconIePointers *ies, uint8_t *htCapability, uint8_t *htOperation,
		      uint8_t *coexistence, uint8_t *overlapScan)
{
	int result = 0;

	if (ies->p2040Coexist != NULL)
		__aeabi_memcpy(coexistence, ies->p2040Coexist, 3);
	if (ies->pHtScanParam != NULL)
		__aeabi_memcpy(overlapScan, ies->pHtScanParam, 16);
	if (ies->pHtInfo != NULL)
		__aeabi_memcpy(htOperation, ies->pHtInfo, 24);
	if (ies->pHtCap != NULL) {
		__aeabi_memcpy(htCapability, ies->pHtCap, 28);
		result = 1;
	}
	return result;
}

/* The rates an association response announces (at most 14: the supported and the extended ones). */
unsigned int fmCoexInspectTable(const WlanBeaconIePointers *ies, uint8_t *rates, uint8_t *count,
				const WlanHtCapabilityIe *htCapability)
{
	unsigned int length = 0;
	unsigned int extra;
	unsigned int bitmap;

	__aeabi_memclr(rates, 14);
	if (ies->pSupportedRates != NULL) {
		length = ies->pSupportedRates[1];
		if (length > 14)
			length = 14;
		__aeabi_memcpy(rates, ies->pSupportedRates + 2, length);
	}
	if (ies->pExtSupportedRates != NULL) {
		extra = ies->pExtSupportedRates[1];
		if (extra > 14 - length)
			extra = 14 - length;
		__aeabi_memcpy(rates + length, ies->pExtSupportedRates + 2, extra);
		length += extra;
	}
	bitmap = wlanRateBitmapAll(rates, length, NULL, 0, htCapability);
	*count = (uint8_t)length;
	return bitmap;
}

/* The SSID, the rates (the first 8 in the supported rates element), the channel and the extended rates. */
void fmCoexTrackTable(WlanWirelessFrame *frame, const void *ssid, const void *rates,
		      const void *dsChannel)
{
	unsigned int length = ssid != NULL ? romStringLength(ssid, 32) : 0;
	unsigned int count;
	unsigned int extra = 0;

	wlanAppendIe(frame, IE_SSID, ssid, length);
	count = romStringLength(rates, 14);
	if (count > 8) {
		extra = count - 8;
		count = 8;
	}
	wlanAppendIe(frame, IE_SUPPORTED_RATES, rates, count);
	if (dsChannel != NULL)
		wlanAppendIe(frame, IE_DS_PARAMETER, dsChannel, 1);
	if (extra != 0)
		wlanAppendIe(frame, IE_EXTENDED_RATES, (const uint8_t *)rates + count, extra);
}

/* A table whose first entry is 0xff, "none": the second and third are cleared. */
void fmCoexManageTable(uint8_t *table)
{
	table[0] = 0xff;
	table[1] = 0;
	table[2] = 0;
}
