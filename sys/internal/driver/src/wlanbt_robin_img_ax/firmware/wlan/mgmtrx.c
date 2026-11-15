/*
 * Received management and control frames (0xf96c..0xfbf2): each interface
 * kind hands them to its handlers by frame subtype. The frame starts at the
 * buffer's data offset, with the firmware's fixed header (WlanWirelessFrame).
 */
#include <stddef.h>
#include "robin/pssm.h"
#include "robin/txhold.h"
#include "robin/txpower.h"
#include "robin/apps.h"
#include "robin/ibss.h"
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/joinframe.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/wlan.h"
#include "robin/peerrate.h"
#include "robin/stamachine.h"

#define WLAN_FC0_TYPE(fc0)		(((fc0) >> 2) & 3)
#define WLAN_FC0_SUBTYPE(fc0)		((fc0) >> 4)
#define WLAN_TYPE_CONTROL		1

/* Management subtypes */
#define WLAN_SUBTYPE_ASSOC_REQUEST	0
#define WLAN_SUBTYPE_REASSOC_REQUEST	2
#define WLAN_SUBTYPE_PROBE_REQUEST	4
#define WLAN_SUBTYPE_PROBE_RESPONSE	5
#define WLAN_SUBTYPE_BEACON		8
#define WLAN_SUBTYPE_ATIM		9
#define WLAN_SUBTYPE_DISASSOC		10
#define WLAN_SUBTYPE_AUTH		11
#define WLAN_SUBTYPE_DEAUTH		12
#define WLAN_SUBTYPE_ACTION		13
/* Control subtypes */
#define WLAN_SUBTYPE_BLOCK_ACK_REQUEST	8
#define WLAN_SUBTYPE_PS_POLL		10

/* Events of the station state machine posted from here. */
#define WLAN_SM_BEACON			3
#define WLAN_SM_PROBE_RESPONSE		6
#define WLAN_SM_PROBE_REQUEST		42
#define WLAN_SM_STATE_NO_PROBES		0x48	/* byte offset of the state that ignores probe requests */

#define AMP_PAL_IOCTL_PEER_TIMESTAMP	0x54
#define WLAN_HOST_EVENT_BLOCK_ACK_REQUEST 5
#define WLAN_EVENT_AP_PS_WAKE		5	/* wlanRaiseEvent */

#define WLAN_FRAME(buffer) \
	((const WlanWirelessFrame *)((const uint8_t *)(buffer) + (buffer)->dataOffset))

/* A Block Ack Request forwarded to the host (at buffer offset 32). */
struct __attribute__((packed)) WlanBarHostEvent {
	uint8_t bssType;
	uint8_t bssNumber;
	uint16_t length;
	uint16_t kind;
	uint16_t event;
	uint16_t startSequence;
	uint8_t tid;
	uint8_t reserved[7];
	uint8_t addresses[12];		/* receiver and transmitter */
	uint8_t padding[2];
};

/* Not converted yet; provisional names. */
void macMgmtAdjustTable(const WlanWirelessFrame *frame, int kind);		/* 0x37c72 */
void macMgmtSetupPacket(OsaBuffer *buffer);					/* 0x3a2ba */
void macMgmtRecordPacket(struct WlanConnection *connection, OsaBuffer *buffer,
			 uintptr_t argument);					/* 0x3a4e2 */
struct WlanConnection *p2pSqramCheckContext(OsaBuffer *buffer, uint32_t *channel); /* SQRAM, veneer 0x52dc4 */
int p2pDirectPrepareState(const uint8_t *address);				/* 0x3f3e4 */
struct WlanConnection *cbProcParseBuffer(const uint8_t *address, struct WlanConnection *connection); /* 0x1dc8c */
void cbProcParsePacket(struct WlanConnection *peer);				/* 0x1e378 */
unsigned int cbProcComputePacket(unsigned int mode);				/* 0x1e3cc */
unsigned int cbProcUpdateBuffer(struct WlanConnection *connection);		/* 0x1d5ba */
void uapSqramConfigureBuffer(struct WlanConnection *connection, struct WlanConnection *peer,
			     const uint8_t *address);				/* SQRAM, veneer 0x52dcc */
int wlanP2pConnection(struct WlanConnection *connection);			/* 0x41228 */

/* Management frames of a station. */
int wlanStaRxManagement(OsaBuffer *buffer, uintptr_t argument)
{
	struct WlanConnection *connection = buffer->context;
	const WlanWirelessFrame *frame = WLAN_FRAME(buffer);
	struct WlanSmRecord record;
	uint32_t channel;
	int result = 1;
	void *machine = &connection->smState;

	record.argument = argument;
	record.frame = frame;
	record.command = NULL;
	record.message = NULL;
	switch (WLAN_FC0_SUBTYPE(frame->frameControl[0])) {
	case WLAN_SUBTYPE_DEAUTH:
		if (connection != NULL)
			macTxNotifyConfigureHeader(buffer);
		break;
	case WLAN_SUBTYPE_DISASSOC:
		if (connection != NULL)
			macTxNotifyConvertFrame(buffer);
		break;
	case WLAN_SUBTYPE_PROBE_RESPONSE:
		if (connection != NULL)
			mdevRomQueryState(machine, WLAN_SM_PROBE_RESPONSE, &record);
		break;
	case WLAN_SUBTYPE_PROBE_REQUEST:
		macMgmtAdjustTable(WLAN_FRAME(buffer), 4);
		if (connection != NULL && connection->interfaceType == WLAN_IF_AP &&
		    connection->smState != connection->smStates + WLAN_SM_STATE_NO_PROBES)
			mdevRomQueryState(machine, WLAN_SM_PROBE_REQUEST, &record);
		break;
	case WLAN_SUBTYPE_BEACON:
		if (connection != NULL)
			mdevRomQueryState(machine, WLAN_SM_BEACON, &record);
		break;
	case WLAN_SUBTYPE_ACTION:
		channel = 0;
		if (connection == NULL) {
			connection = p2pSqramCheckContext(buffer, &channel);
			buffer->context = connection;
			if (connection == NULL)
				break;
		}
		result = macMgmtDispatchState(buffer, argument, channel);
		break;
	case WLAN_SUBTYPE_ATIM:
		macMgmtConvertHeader();
		break;
	}
	return result;
}

/* Management frames of an AMP link. */
int wlanAmpRxManagement(OsaBuffer *buffer, uintptr_t argument)
{
	struct WlanConnection *connection = buffer->context;
	const WlanWirelessFrame *frame = WLAN_FRAME(buffer);

	switch (WLAN_FC0_SUBTYPE(frame->frameControl[0])) {
	case WLAN_SUBTYPE_AUTH:
		if (connection != NULL)
			macTxNotifyCheckHeader(buffer);
		break;
	case WLAN_SUBTYPE_ASSOC_REQUEST:
		if (connection != NULL)
			macTxNotifySelectFrame(buffer);
		break;
	case WLAN_SUBTYPE_PROBE_REQUEST:
		macMgmtRecordPacket(buffer->context, buffer, argument);
		break;
	case WLAN_SUBTYPE_BEACON:
		if (connection->scheduleValid) {
			connection->scheduleValid = 0;
			__aeabi_memcpy(connection->peerTimestamp, frame->body, 8);
			mdevIoctl(ampPalDevice, AMP_PAL_IOCTL_PEER_TIMESTAMP, (uintptr_t)connection);
		}
		break;
	}
	return 1;
}

/* Forward a Block Ack Request to the host. */
void wlanRxBlockAckRequest(const WlanWirelessFrame *frame, OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	struct WlanBarHostEvent *event = (struct WlanBarHostEvent *)&buffer->data[4];
	UINT state;

	buffer->length = 32;
	buffer->dataOffset = 32;
	event->bssType = connection->bssType;
	event->bssNumber = connection->bssNumber;
	event->kind = 18;
	event->event = 0xe7;
	event->length = 14;
	event->tid = frame->body[1] >> 4;
	event->startSequence = (frame->body[3] << 8 | frame->body[2]) >> 4;
	buffer->dataOffset = 50;
	__aeabi_memcpy(event->addresses, frame->address1, sizeof(event->addresses));
	event->padding[0] = 0;
	event->padding[1] = 0;
	if (wlanApPsEnabled())
		wlanRaiseEvent(WLAN_EVENT_AP_PS_WAKE);
	state = sysIrqDisable();
	wlanHostCallback(wlanDevice, buffer, 0, WLAN_HOST_EVENT_BLOCK_ACK_REQUEST);
	sysIrqRestore(state);
}

/* Control frames of an access point: Block Ack Requests and PS-Polls; the rest is dropped. */
int wlanApRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	unsigned int subtype;

	if (WLAN_FC0_TYPE(frame->frameControl[0]) != WLAN_TYPE_CONTROL ||
	    wlanConnectionBssStarted(buffer->context) != 1)
		return osa_BufferFree(buffer);
	if (wlanApPsEnabled())
		wlanPsActivity(wlanConnectionByRole(2));
	subtype = WLAN_FC0_SUBTYPE(frame->frameControl[0]);
	if (subtype == WLAN_SUBTYPE_BLOCK_ACK_REQUEST) {
		wlanRxBlockAckRequest(frame, buffer);
		return 0;
	}
	if (subtype == WLAN_SUBTYPE_PS_POLL)
		wlanApPsPsPoll(connection);
	return osa_BufferFree(buffer);
}

/* Management frames of an access point. */
int wlanApRxManagement(OsaBuffer *buffer, uintptr_t argument)
{
	struct WlanConnection *connection = buffer->context;
	const WlanWirelessFrame *frame = WLAN_FRAME(buffer);

	if (wlanConnectionBssStarted(connection) == 0)
		return 1;
	switch (WLAN_FC0_SUBTYPE(frame->frameControl[0])) {
	case WLAN_SUBTYPE_ASSOC_REQUEST:
	case WLAN_SUBTYPE_REASSOC_REQUEST:
		wlanApPsResetStation(connection);
		macMgmtConfigurePacket(connection, frame, 0);
		break;
	case WLAN_SUBTYPE_DISASSOC:
		macMgmtCheckPacket(buffer);
		break;
	case WLAN_SUBTYPE_AUTH:
		macMgmtAdjustBuffer(buffer, argument);
		break;
	case WLAN_SUBTYPE_DEAUTH:
		macMgmtReleaseBuffer(buffer);
		break;
	case WLAN_SUBTYPE_BEACON:
		macMgmtDispatchPacket(buffer);
		macMgmtSetupPacket(buffer);
		break;
	case WLAN_SUBTYPE_ACTION:
		macMgmtTrackPacket(buffer, argument);
		break;
	case WLAN_SUBTYPE_PROBE_REQUEST:
		macMgmtRecordPacket(connection, buffer, argument);
		break;
	}
	return 1;
}

/*
 * The connection of a peer: a connection without a peer yet takes this one
 * (and starts its link state afresh); otherwise the peer is looked up.
 */
struct WlanConnection *wlanConnectionFindDirectPeer(struct WlanConnection *connection,
						    const uint8_t *address)
{
	if (!p2pDirectPrepareState(connection->peerAddress))
		return cbProcParseBuffer(address, connection);
	__aeabi_memcpy(connection->peerAddress, address, 6);
	macTxNotifyConfigureFrame(connection);
	__aeabi_memclr4(&connection->peerLink, sizeof(connection->peerLink));
	return connection;
}

/* Hand a frame to every connection of an access point or station interface type. */
void wlanRxManagementToAll(OsaBuffer *buffer, uintptr_t argument, unsigned int interfaceType)
{
	void *context = buffer->context;
	struct WlanConnection *connection;

	for (connection = wlanConnectionFirst(interfaceType); connection != NULL;
	     connection = wlanConnectionNextFlagged(connection, interfaceType)) {
		if (connection->rxManagement != NULL &&
		    (interfaceType == WLAN_IF_STATION || interfaceType == WLAN_IF_AP)) {
			buffer->context = connection;
			connection->rxManagement(buffer, argument);
		}
	}
	buffer->context = context;
}

static inline __attribute__((always_inline)) void
wlanRxManagementTo(struct WlanConnection *connection, OsaBuffer *buffer, uintptr_t argument)
{
	if (connection->rxManagement != NULL) {
		buffer->context = connection;
		connection->rxManagement(buffer, argument);
	}
}

/* A management frame not addressed to a known connection: find who takes it. */
void wlanRxManagementRoute(OsaBuffer *buffer, uintptr_t argument)
{
	const WlanWirelessFrame *frame = WLAN_FRAME(buffer);
	struct WlanConnection *connection, *peer;
	unsigned int reason;

	if (WLAN_FC0_TYPE(frame->frameControl[0]) != 0)
		return;
	switch (WLAN_FC0_SUBTYPE(frame->frameControl[0])) {
	case WLAN_SUBTYPE_PROBE_REQUEST:
		wlanRxManagementToAll(buffer, argument, WLAN_IF_AP);
		wlanRxManagementToAll(buffer, argument, WLAN_IF_STATION);
		if (wlanConnectionExists(WLAN_IF_AMP)) {
			connection = wlanConnectionFirst(WLAN_IF_AMP);
			if (connection != NULL && connection->rxManagement != NULL)
				connection->rxManagement(buffer, argument);
		}
		break;
	case WLAN_SUBTYPE_BEACON:
		connection = wlanConnectionFindByLocal(WLAN_IF_IBSS, frame->address3);
		if (connection == NULL)
			connection = wlanIbssFindMerge(frame);
		if (connection == NULL)
			break;
		peer = wlanConnectionFindDirectPeer(connection, frame->address2);
		if (peer != NULL) {
			wlanRxManagementTo(peer, buffer, argument);
			cbProcParsePacket(peer);
		}
		break;
	case WLAN_SUBTYPE_AUTH:
		connection = wlanConnectionFindByLocal(WLAN_IF_STATION, frame->address3);
		if (connection == NULL || wlanConnectionBssStarted(connection) == 0) {
			connection = wlanConnectionFindByLocal(WLAN_IF_AP, frame->address3);
			if (connection != NULL && wlanP2pConnection(connection) == 0)
				connection = NULL;
		}
		if (wlanConnectionBssStarted(connection) == 0)
			break;
		if (macMgmtCheckHeader(connection, frame->address2)) {
			reason = 6;
		} else {
			struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
			unsigned int kind = cbProcComputePacket(connection->mode);

			if (info->maxStations > cbProcUpdateBuffer(connection) && kind < 10) {
				peer = wlanConnectionFindDirectPeer(connection, frame->address2);
				if (peer != NULL) {
					uapSqramConfigureBuffer(connection, peer, frame->address2);
					wlanRxManagementTo(peer, buffer, argument);
				}
				break;
			}
			reason = 7;
		}
		macMgmtDispatchBuffer(connection, frame->address2, reason);
		break;
	default:
		wlanStaRxManagement(buffer, argument);
		break;
	}
}

/* Control frames of a station or IBSS: Block Ack Requests go to the host. */
int wlanStaRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer)
{
	if (WLAN_FC0_TYPE(frame->frameControl[0]) == WLAN_TYPE_CONTROL &&
	    WLAN_FC0_SUBTYPE(frame->frameControl[0]) == WLAN_SUBTYPE_BLOCK_ACK_REQUEST) {
		wlanRxBlockAckRequest(frame, buffer);
		return 0;
	}
	return osa_BufferFree(buffer);
}

/* The same for frames of the management task. */
int wlanMgmtRxControl(const WlanWirelessFrame *frame, OsaBuffer *buffer)
{
	if (WLAN_FC0_TYPE(frame->frameControl[0]) == WLAN_TYPE_CONTROL &&
	    WLAN_FC0_SUBTYPE(frame->frameControl[0]) == WLAN_SUBTYPE_BLOCK_ACK_REQUEST) {
		wlanRxBlockAckRequest(frame, buffer);
		return 0;
	}
	return osa_BufferFree(buffer);
}
