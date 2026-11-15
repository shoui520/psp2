/*
 * An access point's stations (the soft AP and the P2P group owner): the
 * authentication exchange (open system and shared key with its challenge text),
 * the leaving of a peer and what the host is told of it (host events 44, 67,
 * 68), the commands that start, stop and reset the BSS and drop a station
 * (HostCmd 0xaf, 0xb1, 0xb2, 0xb5; Marvell's UAP_BSS_RESET, _START, _STOP and
 * _STA_DEAUTH) and the association IDs. Marvell published no source for it; the
 * names are the firmware's own placeholder names, kept, with what the code does
 * said here, and the file name is inferred.
 *
 * A peer's connection keeps its 802.11 state in WlanConnectionRole.state:
 * 0 free, 1 leaving (the deauthentication was sent), 2 authenticated, 3 associated.
 */
#include <stddef.h>
#include "robin/p2pgo.h"
#include "robin/apconfig.h"
#include "robin/apps.h"
#include "robin/conn.h"
#include "robin/esatypes.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/ieee80211.h"
#include "robin/keyMgmtAp.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/softtimer.h"
#include "robin/stream.h"
#include "robin/sys.h"
#include "robin/uapbss.h"
#include "robin/wlan.h"
#include "robin/hostps.h"
#include "robin/apsta.h"
#include "robin/peerrate.h"

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))

/* A message to the host (a command's response or an event): what wlanHostMessageAlloc() hands out. */
struct WlanHostMessage {
	uint8_t command[2];		/* the command, bit 15 set: its response */
	uint8_t size[2];		/* of the whole message */
	uint8_t sequence[2];
	uint8_t result[2];
	uint8_t body[];
};

#define BUFFER_DATA(buffer)		((uint8_t *)(buffer) + (buffer)->dataOffset)
#define WLAN_HOST_MESSAGE(buffer)	((struct WlanHostMessage *)BUFFER_DATA(buffer))

/* The kinds macMgmtBuildBuffer() queues for the management task (byte 0 of its message). */
#define WLAN_SME_BSS_START		29
#define WLAN_SME_BSS_STOP		31
#define WLAN_SME_BSS_STOP_DONE		32
#define WLAN_SME_BSS_RESET		33
#define WLAN_SME_BSS_RESET_DONE		34
#define WLAN_SME_STA_DEAUTH		35
#define WLAN_SME_BSS_START_HT		36

#define WLAN_HOST_EVENT_STA_LEFT	44
#define WLAN_HOST_EVENT_BSS_EMPTY	67
#define WLAN_HOST_EVENT_BSS_FIRST	68

#define WLAN_CHALLENGE_SIZE		128
#define WLAN_CHALLENGE_TIMEOUT		500000	/* us */

/* What a peer's leaving is reported to the host as: the event code of a deauthentication reason. */
__attribute__((noinline)) int wlanEventMaskIndex(int reason)
{
	switch (reason) {
	case 2:
	case 3:
		return 1;
	case 6:
		return 4;
	case 15:
		return 8;
	case 16:
		return 9;
	default:
		return 0;
	}
}

/* Takes the first free bit of a bitmap of count bits; returns its number, 255: none. */
__attribute__((noinline)) unsigned int wlanBitmapAllocate(uint8_t *bitmap, unsigned int count)
{
	unsigned int number = 0;
	int bit = 0;

	while (number < count) {
		if (bit == 8) {
			++bitmap;
			bit = 0;
		}
		if ((*bitmap & (1 << bit)) == 0) {
			*bitmap |= 1 << bit;
			return number;
		}
		++number;
		++bit;
	}
	return 255;
}

void wlanBitmapFree(uint8_t *bitmap, unsigned int count, unsigned int number)
{
	if (number < count)
		bitmap[number >> 3] &= ~(1 << (number & 7));
}

/* The host is told of a peer's leaving (host event 44) unless the event code says none. */
__attribute__((noinline)) void macMgmtDispatchBuffer(struct WlanConnection *connection, const uint8_t *peer,
						     unsigned int eventCode)
{
	if (!(eventCode >> 15))
		wlanHostEventSendCodeAddress(connection, WLAN_HOST_EVENT_STA_LEFT, eventCode & 0x7fff, peer);
}

/* A request for the management task: byte 0 of its message is the kind. */
int macMgmtBuildBuffer(struct WlanConnection *connection, unsigned int kind)
{
	int result = 1;
	OsaBuffer *message = macMgmtHandleState(connection);

	if (message != NULL) {
		*BUFFER_DATA(message) = (uint8_t)kind;
		result = macMgmtProcessState(message);
		if (result == 1)
			osa_BufferFree(message);
	}
	return result;
}

/* HostCmd 0xaf (UAP_BSS_RESET) */
int hostCmd00af(struct WlanConnection *connection)
{
	return macMgmtBuildBuffer(connection, WLAN_SME_BSS_RESET) != 0;
}

/* HostCmd 0xb2 (UAP_BSS_STOP): 2 when the BSS is not up */
int hostCmd00b2(struct WlanConnection *connection)
{
	if (wlanConnectionInfo(connection)->bssStarted != 0)
		return macMgmtBuildBuffer(connection, WLAN_SME_BSS_STOP) != 0;
	return 2;
}

/* The BSS has stopped or has been reset (kind 32 or 34): the host is answered when it asked (reply). */
int macMgmtResetBuffer(struct WlanConnection *connection, int unused, int kind, int reply)
{
	uint16_t command;

	if (kind == WLAN_SME_BSS_STOP_DONE) {
		command = 0x80b2;
	} else if (kind == WLAN_SME_BSS_RESET_DONE) {
		command = 0x80af;
	} else {
		return -1;
	}
	if (wlanHasApRole()) {
		struct WlanConnection *group = p2pDirectProcessEntry(3);

		if (group != NULL || (group = wlanConnectionFirst(0)) != NULL) {
			wlanConnectionActivate(group);
			if (wlanSessionId != 0)
				wlanPsStartHostModeTimer(group);
		}
	}
	wlanPsPhaseResume(connection);
	if (reply) {
		OsaBuffer *message = wlanHostMessageAlloc(connection);

		if (message != NULL) {
			struct WlanHostMessage *response = WLAN_HOST_MESSAGE(message);

			PUT_U16(response->command, command);
			response->result[0] = 0;
			response->result[1] = 0;
			response->size[0] = 8;
			response->size[1] = 0;
			if (wlanHostMessagePost(message) == 1) {
				osa_BufferFree(message);
				return -1;
			}
		}
	}
	return 0;
}

/* The BSS has started: the host is answered (UAP_BSS_START's response) when it asked (reply == 1). */
int macMgmtSyncBuffer(struct WlanConnection *connection, const uint8_t *record, int reply)
{
	int result = 0;
	uint16_t status;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	if (record != NULL && record[12] != 0) {
		result = -1;
		status = 1;
	} else {
		connection->state = 7;
		status = 0;
		info->bssStarted = 1;
		wlanPsPhaseSuspend();
		if (wlanSessionId != 0) {
			wlanConnectionActivate(connection);
			wlanPsStartHostModeTimer(connection);
		}
	}
	if (reply == 1) {
		OsaBuffer *message = wlanHostMessageAlloc(connection);

		if (message != NULL) {
			struct WlanHostMessage *response = WLAN_HOST_MESSAGE(message);

			response->command[0] = 0xb1;
			response->command[1] = 0x80;
			PUT_U16(response->result, status);
			response->size[0] = 8;
			response->size[1] = 0;
			response->body[0] = record[12];
			if (wlanHostMessagePost(message) == 1)
				osa_BufferFree(message);
		}
	}
	return result;
}

/* HostCmd 0xb1 (UAP_BSS_START): 2 when the connection is not free, 4 for a P2P client, 3 when it could not be queued, 5 when the HT variant went out. */
int hostCmd00b1(struct WlanConnection *connection)
{
	int result = 0;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	unsigned int kind = WLAN_SME_BSS_START;

	if (connection->state != 0)
		return 2;
	if (connection->interfaceType == 3 && connection->mode != 2)
		return 4;
	uapBssApply(connection, NULL, NULL);
	if (info->securityMode.noRsn)
		info->capability[0] &= ~0x10;
	else
		info->capability[0] |= 0x10;
	info->pktFwdCtl = info->pktFwdCtlConfigured;
	if (role->channelFlags >> 6 == 1 && macMgmtSchedulePacket()) {
		kind = WLAN_SME_BSS_START_HT;
		result = 5;
	}
	if (macMgmtBuildBuffer(connection, kind) == 1)
		return 3;
	return result;
}

/* HostCmd 0xb5 (UAP_STA_DEAUTH): the station is named by its address; the reason is the command's (2 when it has none). */
int hostCmd00b5(struct WlanConnection *connection, uint8_t *command)
{
	const uint8_t *address = command + 8;
	struct WlanConnection *peer = wlanConnectionFindByPeer(address);
	OsaBuffer *message;

	if (peer == NULL || peer->bssType != connection->bssType || peer->bssNumber != connection->bssNumber)
		return 1;
	message = macMgmtHandleState(peer);
	if (message != NULL) {
		uint8_t *data = BUFFER_DATA(message);
		uint16_t reason;

		data[0] = WLAN_SME_STA_DEAUTH;
		__aeabi_memcpy(data + 9, address, 6);
		if (U16(command + 2) == 14)
			reason = 2;
		else
			reason = U16(command + 14);
		PUT_U16(data + 15, reason);
		if (macMgmtProcessState(message) != 1)
			return 0;
		osa_BufferFree(message);
	}
	return 1;
}

/*
 * HostCmd 0xb3 (UAP_STA_LIST): the stations associated with the access
 * point, a STA_INFO TLV each (address, power save, signal strength).
 */
int hostCmd00b3(struct WlanConnection *connection, uint8_t *command)
{
	uint8_t *count = command + 8;
	uint8_t *tlv;
	struct WlanConnectionRole *role;

	if (connection == NULL)
		return 1;
	PUT_U16(count, 0);
	PUT_U16(command + 2, U16(command + 2) + 2);
	tlv = command + 10;
	do {
		role = wlanConnectionInfoSlot(connection);
		if (connection->flags2 & 0x10) {
			PUT_U16(count, U16(count) + 1);
			wlanTlvStaInfoSet(tlv, tlv + 4, connection->peerAddress);
			tlv[10] = role->peerPowerSave;
			tlv[11] = wlanBeaconRssi(connection);
			PUT_U16(command + 2, U16(command + 2) + 12);
			tlv += 12;
		}
		connection = wlanConnectionNextByBss(connection, connection->bssType, connection->bssNumber);
	} while (connection != NULL);
	return 0;
}

/* The station's authentication is over, whatever way: its state is dropped and the peer forgotten. */
void macMgmtScheduleBuffer(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info;

	wlanTrace(0xcccccccc, 0x11111111);
	macMgmtFilterBuffer(connection);
	info = wlanConnectionInfo(connection);
	if (info->challengePending != 0 && connection->authSequence == 3) {
		osaCancelTimer(info->challengeTimer);
		info->challengePending = 0;
	}
	connection->authSequence = 1;
	if (wlanP2pConnection(connection) != 0) {
		if (role->p2pContext != NULL) {
			p2pDirectSetupContext(connection, role->p2pContext);
			role->p2pContext = NULL;
		} else {
			struct WlanPsPeerState *state = wlanConnectionPsState(connection);

			if (state->p2pAuthCount != 0)
				--state->p2pAuthCount;
		}
	}
	__aeabi_memclr4(&role->listenInterval, 0x6c);
	role->state = 0;
	__aeabi_memclr(connection->peerAddress, 6);
	macTxNotifyConfigureFrame(connection);
}

/* The peer is leaving: the host is told (unless the event code says none), then it is dropped at once when asked (immediate == 0). */
void macMgmtNotifyBuffer(struct WlanConnection *connection, int keep)
{
	struct WlanConnectionRole *role;

	wlanConnectionInfo(connection);
	role = wlanConnectionInfoSlot(connection);
	role->state = 1;
	macMgmtDispatchBuffer(connection, connection->peerAddress, 4);
	if (keep == 0) {
		macMgmtScheduleBuffer(connection);
		if (!(connection->flags2 & WLAN_CONN2_HOLD))
			wlanConnectionRelease(connection);
	}
}

/* The shared key challenge was not answered in time. */
void macMgmtFormatBuffer(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;

	wlanConnectionInfo(connection)->challengeTimer = 0;
	macMgmtNotifyBuffer(connection, 0);
}

/* A deauthentication is sent to the peer; returns what wlanMgmtSendDeauth() says, 0: sent. */
int macMgmtInspectBuffer(struct WlanConnection *connection, const uint8_t *peer, unsigned int reason)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	int own = 0;
	int result;
	unsigned int eventCode;

	if (memcmp(connection->peerAddress, peer, 6) == 0) {
		own = 1;
		if (role->state == 1)
			return 0;
		eventCode = role->eventCode;
		if (eventCode != 3 && eventCode != 2)
			eventCode = wlanEventMaskIndex(reason);
	} else {
		eventCode = wlanEventMaskIndex(reason);
	}
	result = wlanMgmtSendDeauth(connection, peer, connection->localAddress, reason, 0);
	if (!(peer[0] & 1)) {
		if (result != 0) {
			eventCode |= 0x8000;
		} else if (own == 1) {
			role->state = 1;
			wlanConnectionUpdateListenInterval(connection);
		}
		role->eventCode = eventCode;
	}
	return result;
}

/* The host's HostCmd 0xb5 or the access point's own decision: the peer is deauthenticated and the host is told. */
int macMgmtTrackBuffer(struct WlanConnection *connection, unsigned int reason)
{
	int result;
	OsaBuffer *message;
	struct WlanHostMessage *event;

	wlanConnectionInfoSlot(connection)->eventCode = 3;
	result = macMgmtInspectBuffer(connection, connection->peerAddress, reason);
	message = wlanHostMessageAlloc(connection);
	if (message == NULL)
		return -1;
	event = WLAN_HOST_MESSAGE(message);
	event->command[0] = 0xb5;
	event->command[1] = 0x80;
	PUT_U16(event->result, result != 0);
	event->size[0] = 16;
	event->size[1] = 0;
	__aeabi_memcpy(event->body, connection->peerAddress, 6);
	PUT_U16(event->body + 6, reason);
	wlanHostMessagePost(message);
	return result;
}

/* A frame from the peer says it leaves (deauthentication or disassociation): the access point lets it go. */
int macMgmtManageBuffer(struct WlanConnection *connection, const uint8_t *peer)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

	wlanTrace(0xcccccccc, 0x22222222);
	if (!(peer[0] & 1))
		macMgmtDispatchBuffer(connection, peer, role->eventCode);
	if (memcmp(connection->peerAddress, peer, 6) == 0 && role->state == 1) {
		macMgmtScheduleBuffer(connection);
		if (!(connection->flags2 & WLAN_CONN2_HOLD))
			wlanConnectionRelease(connection);
	}
	return 0;
}

/* The deauthentication was sent: the connection is let go if the peer was leaving. */
void macMgmtAcquireBuffer(struct WlanConnection *connection)
{
	if (wlanConnectionInfoSlot(connection)->state == 1) {
		macMgmtScheduleBuffer(connection);
		if (!(connection->flags2 & WLAN_CONN2_HOLD))
			wlanConnectionRelease(connection);
	}
}

/* A deauthentication or disassociation is received: its reason becomes the event code. */
int macMgmtReleaseBuffer(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = (WlanWirelessFrame *)BUFFER_DATA(buffer);
	unsigned int eventCode = wlanEventMaskIndex(U16(frame->body));

	macMgmtDispatchBuffer(connection, connection->peerAddress, eventCode);
	macMgmtScheduleBuffer(connection);
	if (!(connection->flags2 & WLAN_CONN2_HOLD))
		wlanConnectionRelease(connection);
	return 0;
}

/* An authentication frame goes to the peer: algorithm, sequence number, status and, with the challenge, its text. */
__attribute__((noinline)) int macMgmtSetupBuffer(uint16_t algorithm, uint16_t sequence, uint16_t status, const void *challenge,
		       struct WlanConnection *connection)
{
	OsaBuffer *buffer = wlanMgmtAllocFrame(connection, 0x15c);

	if (buffer == NULL)
		return 0;
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, connection->peerAddress, 11, connection->localAddress);
	fmCoexScheduleTable((uint8_t *)BUFFER_DATA(buffer),
			    algorithm, sequence, status, challenge);
	return wlanMgmtSendFrame(buffer);
}

/* What the frame's authentication algorithm and sequence number must be; the status code for an unacceptable one, 0: acceptable. */
int macMgmtClearBuffer(const WlanWirelessFrame *frame, unsigned int sequence, unsigned int algorithm,
		       const uint8_t *request)
{
	const uint8_t *body = frame->body;
	unsigned int received = U16(body);

	if (U16(body + 2) != sequence)
		return 14;
	if (U16(body + 2) == 3 && !(frame->frameControl[1] & 0x40))
		return 1;
	if (wlanFrameHasWpsIe(request) == 0 || received != 0) {
		if (algorithm == 255) {
			if (received < 2)
				return 0;
		} else if (received == algorithm) {
			return 0;
		}
		return 13;
	}
	return 0;
}

int macMgmtConvertBuffer(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	return macMgmtClearBuffer((const WlanWirelessFrame *)BUFFER_DATA(buffer),
				  connection->authSequence, connection->authAlgorithm,
				  (const uint8_t *)connection->probeResponseRequest);
}

/* The challenge came back (third frame of a shared key authentication): it must be the one sent. */
int macMgmtVerifyBuffer(OsaBuffer *buffer, int status)
{
	struct WlanConnection *connection = buffer->context;
	const uint8_t *frame = BUFFER_DATA(buffer);
	struct WlanConnectionInfo *info;

	wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	connection->authSequence = 1;
	osaCancelTimer(info->challengeTimer);
	info->challengePending = 0;
	if (status == 0 && memcmp(info->challengeText, frame + 40, WLAN_CHALLENGE_SIZE) != 0)
		return 15;
	return status;
}

/* A challenge text is made for a shared key authentication and its timer started; -1: one is outstanding. */
int macMgmtSelectBuffer(struct WlanConnection *connection, uint8_t **text)
{
	struct WlanConnectionInfo *info;

	wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	if (info->challengePending != 0)
		return -1;
	connection->authSequence = 3;
	*text = info->challengeText;
	info->challengePending = 1;
	osaCancelTimer(info->challengeTimer);
	osaCreateTimer(macMgmtFormatBuffer, (uint32_t)(uintptr_t)connection, WLAN_CHALLENGE_TIMEOUT,
		       &info->challengeTimer, 0);
	fmCoexUpdateTable(*text);
	return 0;
}

/* The challenge text: 128 bytes from the C library's generator, seeded with the MAC's timer. */
void fmCoexUpdateTable(uint8_t *text)
{
	uint32_t timer[2];
	uint32_t value;
	unsigned int offset;

	romLibVerifyState(&timer[1], &timer[0]);
	srand(timer[0]);
	for (offset = 0; offset < WLAN_CHALLENGE_SIZE; offset += 4) {
		value = rand();
		__aeabi_memcpy(text + offset, &value, 4);
	}
}

/*
 * What an association response carries, and what the request is checked against (the helpers of
 * macMgmtConfigurePacket()). The response frame is built as the request's reply: its status is the
 * second halfword of its body.
 */
#define WLAN_STATUS_ASSOC_DENIED	12	/* the association is refused for a reason of the BSS's */
#define WLAN_STATUS_BASIC_RATES		18	/* the peer does not support the BSS's basic rates */
#define WLAN_STATUS_HT_WITH_TKIP	42	/* HT capabilities with a TKIP pairwise cipher */

/* The HT capabilities of the BSS go into the response, unless the peer is HT and uses TKIP (refused). */
int fmCoexConfigureTable(WlanWirelessFrame *frame, const WlanWirelessFrame *request,
			 const WlanIePointers *ies, const Cipher_t *cipher,
			 const uint8_t *htCapability)
{
	if (ies->pHtCap != NULL) {
		if ((ies->pWpa != NULL || ies->pRsn != NULL) && cipher != NULL && cipher->tkip) {
			PUT_U16(frame->body + 2, WLAN_STATUS_HT_WITH_TKIP);
			return 1;
		}
		wlanAppendIe(frame, 0x2d, htCapability + 2, 0x1a);
	}
	return 0;
}

/* The request is for this BSS: its elements could be read, and its SSID is the BSS's. */
int fmCoexCheckTable(WlanWirelessFrame *frame, const WlanIePointers *ies, int unparsed,
		     const void *ssid, unsigned int length)
{
	int result = 0;

	if (unparsed != 0 || (result = memcmp(ies->pSsid + 2, ssid, length)) != 0) {
		PUT_U16(frame->body + 2, WLAN_STATUS_ASSOC_DENIED);
		return 1;
	}
	return result;
}

/*
 * The rates the peer shares with the BSS go into the response (the first eight in the supported
 * rates element, the others in *extended's count of the extended supported rates one); a peer
 * that shares none, or lacks a basic rate, is refused.
 */
int fmCoexValidateTable(WlanWirelessFrame *frame, const WlanIePointers *ies, const uint8_t *bssRates,
			uint8_t *rates, uint8_t *count, uint8_t *extended)
{
	uint8_t extra = 0;
	unsigned int first;
	int shared;

	__aeabi_memclr(rates, 14);
	shared = wlanRatesSelect(bssRates, ies->pSupportedRates, ies->pExtSupportedRates, rates, count);
	if (*count <= 8) {
		first = *count;
	} else {
		extra = *count - 8;
		first = 8;
	}
	if (shared != 0 && *count != 0) {
		wlanAppendIe(frame, 1, rates, first);
		*extended = extra;
		return 0;
	}
	PUT_U16(frame->body + 2, WLAN_STATUS_BASIC_RATES);
	return 1;
}

/* The request supports the short preamble. */
int fmCoexPrepareTable(const WlanWirelessFrame *request)
{
	return (request->body[0] & 0x20) != 0;
}

/* The request supports the short slot time. */
int fmCoexDispatchTable(const WlanWirelessFrame *request)
{
	return (request->body[1] & 0x04) != 0;
}

/* The peer's MCS set and channel width are cut down to what the BSS has (in the request's element). */
void fmCoexParseTable(const WlanIePointers *ies, const uint8_t *htCapability)
{
	WlanHtCapabilityIe *peer = (WlanHtCapabilityIe *)ies->pHtCap;
	uint8_t *capabilities = (uint8_t *)&peer->capabilities;
	int i;

	for (i = 0; i < 16; i++)
		peer->mcsSet[i] &= htCapability[5 + i];
	capabilities[0] &= (uint8_t)(((htCapability[2] & WLAN_HT_CAP_40MHZ) != 0) << 1) | 0xfd;
}

/* The peer has an OFDM rate (in either rates element). */
int fmCoexBuildTable(const WlanIePointers *ies)
{
	const uint8_t *rates = ies->pSupportedRates;

	if (rates == NULL || wlanRatesHaveOfdm(rates + 2, rates[1]) == 0) {
		rates = ies->pExtSupportedRates;
		if (rates == NULL || wlanRatesHaveOfdm(rates + 2, rates[1]) == 0)
			return 0;
	}
	return 1;
}

/*
 * The response is made the reply of the request: a reassociation request (subtype 2) is answered with a
 * reassociation response (subtype 3); the association ID starts as 0xc000 and the body as its three
 * halfwords. The request's elements (after its capability, listen interval and, for a reassociation,
 * the old access point's address) are given back with their length.
 */
const uint8_t *fmCoexComputeTable(WlanWirelessFrame *frame, const WlanWirelessFrame *request,
				  const uint8_t **elements, int *length)
{
	int remaining;
	const uint8_t *first;

	if (request->frameControl[0] >> 4 == 2)
		frame->frameControl[0] = (frame->frameControl[0] & 0x0f) + 0x30;
	frame->body[4] = 0;
	frame->body[5] = 0xc0;
	frame->bodyLength[0] = 6;
	frame->bodyLength[1] = 0;
	remaining = U16(request->bodyLength) - 4;
	first = request->body + 4;
	if (request->frameControl[0] >> 4 == 2) {
		first = request->body + 10;
		remaining = U16(request->bodyLength) - 10;
	}
	*elements = first;
	*length = remaining;
	return first;
}

/* The peer's HT capabilities say it can send greenfield frames. */
int fmCoexQueryTable(const WlanIePointers *ies)
{
	return (ies->pHtCap->capabilities & 0x0010) != 0;
}

/* The peer's HT capabilities say it supports 40 MHz. */
int fmCoexResetTable(const WlanIePointers *ies)
{
	return (ies->pHtCap->capabilities & WLAN_HT_CAP_40MHZ) != 0;
}

/* Elements of the BSS go into a frame as they are kept: the HT capabilities, the HT operation and the WMM parameter ones; each returns its size. */
int fmCoexSyncTable(uint8_t *destination, const uint8_t *htCapability)
{
	__aeabi_memcpy(destination, htCapability, 28);
	return 28;
}

int fmCoexApplyTable(uint8_t *destination, const uint8_t *htOperation)
{
	__aeabi_memcpy(destination, htOperation, 24);
	return 24;
}

int fmCoexResolveTable(uint8_t *destination, const uint8_t *wmmElement)
{
	__aeabi_memcpy(destination, wmmElement, 26);
	return 26;
}

/*
 * The authentication frame's body: algorithm, sequence, status and, when the
 * challenge is sent, the challenge text element (ID 16, 128 bytes), the frame's
 * body length (its first halfword) counted up as each part is added. A shared
 * key frame is marked protected and its body starts four bytes later (the IV).
 */
void fmCoexScheduleTable(uint8_t *frame, uint16_t algorithm, uint16_t sequence, uint16_t status,
			 const void *challenge)
{
	uint8_t *body = frame + 32;
	uint16_t length;

	frame[0] = 0;
	frame[1] = 0;
	if (challenge != NULL && sequence == 3) {
		frame[3] |= 0x40;
		body += 4;
		frame[0] = 8;
		frame[1] = 0;
	}
	PUT_U16(body, algorithm);
	length = U16(frame) + 2;
	PUT_U16(frame, length);
	PUT_U16(body + 2, sequence);
	length = U16(frame) + 2;
	PUT_U16(frame, length);
	PUT_U16(body + 4, status);
	length = U16(frame) + 2;
	PUT_U16(frame, length);
	if (challenge != NULL) {
		body[6] = 16;
		body[7] = WLAN_CHALLENGE_SIZE;
		length = U16(frame) + 2;
		PUT_U16(frame, length);
		__aeabi_memcpy(body + 8, challenge, WLAN_CHALLENGE_SIZE);
		length = U16(frame) + WLAN_CHALLENGE_SIZE;
		PUT_U16(frame, length);
	}
}

/*
 * An authentication frame from a peer: checked, answered (open system: accepted at
 * once, shared key: the challenge is sent) or refused, and the host told when the
 * peer is let in. A shared key frame with the challenge's answer is decrypted first
 * with the group key.
 */
void macMgmtAdjustBuffer(OsaBuffer *buffer, uintptr_t argument)
{
	struct WlanRxStatus *status = (struct WlanRxStatus *)argument;
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = (WlanWirelessFrame *)BUFFER_DATA(buffer);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	unsigned int algorithm, sequence;
	int result = 0;
	int selected = 0;
	uint8_t *text = NULL;
	int notifyKeep;

	macMgmtFilterBuffer(connection);
	if (frame->frameControl[1] & 0x40) {
		const struct WlanKeyBuffer *key = connection->keyBlocks[0];
		uint8_t *length;

		if (key == NULL)
			return;
		if (!(key->key.hdr.keyState & 1))
			return;
		length = (uint8_t *)__aeabi_uread4(status->frame);
		PUT_U16(length, U16(length) + 96);
		if (wlanRxDecryptWep(&buffer, status, (cipher_key_t *)&key->key, 0) == 0)
			result = 15;
		frame = (WlanWirelessFrame *)BUFFER_DATA(buffer);
	}
	algorithm = U16(frame->body);
	sequence = U16(frame->body + 2);
	if (result == 0)
		result = macMgmtConvertBuffer(buffer);
	if (connection->authSequence == 3) {
		result = macMgmtVerifyBuffer(buffer, result);
		algorithm = 1;
		sequence = 3;
		if (result == 0)
			role->state = 2;
	} else if (result == 0) {
		if (algorithm == 0)
			role->state = 2;
		else
			selected = macMgmtSelectBuffer(connection, &text);
	}
	if (selected != -1) {
		if (macMgmtSetupBuffer(algorithm, sequence + 1, result, text, connection) != -1) {
			if (result == 0)
				return;
			notifyKeep = 1;
		} else {
			notifyKeep = 0;
		}
	} else {
		notifyKeep = 0;
	}
	macMgmtNotifyBuffer(connection, notifyKeep);
}

/* An association ID is given to the peer (1 + the bit taken) unless the access point is full; 0: none. */
unsigned int macMgmtRecordBuffer(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int number;

	if (info->stationCount >= info->maxStations)
		return 0;
	number = wlanBitmapAllocate(info->aidBitmap, 32);
	connection->associationId = 0;
	if (number != 255) {
		connection->associationId = number + 1;
		connection->flags2 |= WLAN_CONN2_ASSOCIATED;
		if (++info->stationCount == 1)
			wlanHostEventSendPlain(connection, WLAN_HOST_EVENT_BSS_FIRST);
	}
	return connection->associationId;
}

/* The associated peer is removed: its BA sessions, counters, key and ID are given back, and the host is told when the BSS is empty. */
__attribute__((noinline)) void macMgmtFilterBuffer(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role;
	struct WlanConnectionInfo *info;

	if (!(connection->flags2 & WLAN_CONN2_ASSOCIATED))
		return;
	role = wlanConnectionInfoSlot(connection);
	info = role->info;
	if (connection->flags & WLAN_CONN_HT) {
		const uint8_t *peer = connection->peerAddress;
		unsigned int tid;

		for (tid = 0; tid < 8; tid++) {
			uint8_t hwSlot[4];

			if (wlanRxBaHwSlot(tid, peer, hwSlot, wlanRxBaSessions) == 1) {
				macRxBaDelete(peer, tid);
				wlanBaTimeoutCancel(peer, tid, 0);
			}
			if (wlanTxBaProbe(tid, peer, wlanTxBaSessions) == 1) {
				macTxBaAbort(connection, peer, tid);
				wlanBaTimeoutCancel(peer, tid, 1);
			}
		}
		if (!(connection->htCapability[2] & 0x02))
			--info->ht20MhzStations;
		if (!(connection->htCapability[2] & 0x10))
			--info->htNonGreenfieldStations;
		if (connection->htCapability[3] & 0x40) {
			if (--info->ht40IntolerantStations == 0)
				macMgmtManagePacket(connection, 1);
		}
	} else {
		--info->htMixedStations;
	}
	wlanSecurityRefresh(connection);
	connection->flags &= ~WLAN_CONN_HT;
	__aeabi_memclr(connection->htCapability, sizeof(connection->htCapability));
	wlanApPsStationGone(connection);
	connection->flags2 &= ~WLAN_CONN2_ASSOCIATED;
	if (connection->associationId != 0)
		wlanBitmapFree(info->aidBitmap, 32, connection->associationId - 1);
	connection->associationId = 0;
	--info->stationCount;
	role->state = 2;
	if (role->erpCapable == 0) {
		if (--info->erpInfo == 0)
			wlanApErpUpdate(connection);
	}
	if (!(connection->bssCapability[0] & 0x20)) {
		if (--info->nonErpStations == 0)
			wlanApUpdateProtection(connection);
	}
	if (!(connection->bssCapability[1] & 0x04)) {
		if (--info->longSlotStations == 0)
			wlanApUpdateSlotTime(connection);
	}
	if (info->securityMode.wpa || info->securityMode.wpa2) {
		if (connection->keyBlocks[1] != NULL) {
			wlanKeyBufferFree(connection->keyBlocks[1]);
			connection->keyBlocks[1] = NULL;
		}
	}
	if (info->securityMode.wapi && connection->keyBlocks[1] != NULL) {
		keyApi_FreeKey(&connection->keyBlocks[1]);
		connection->flags &= ~WLAN_CONN_WAPI;
		connection->keyBlocks[0] = NULL;
	}
	connection->flags &= ~(WLAN_CONN_ENCRYPTED | WLAN_CONN_JOIN_OPEN);
	wlanSoftTimerStop(&connection->softTimer);
	macTxNotifyParseFrame(connection);
	if (wlanInterfaceNeedsSecurity(connection) != 0) {
		KeyMgmtStopHskTimer(connection, 0);
		__aeabi_memclr4(&role->keyMgmt, 0x34);
	}
	if (info->stationCount == 0)
		wlanHostEventSendPlain(connection, WLAN_HOST_EVENT_BSS_EMPTY);
	macTxFlushConnection(connection);
}
