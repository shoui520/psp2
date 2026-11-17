/*
 * The connection's state machine of an IBSS, a station and a P2P link (0x34778 - 0x354cc), and
 * the messages it sends: the probe request the AMP link and the P2P group owner send, the events
 * to the host, the host command 0xc7 reply. The firmware's own placeholder names,
 * "macMgmt*State", are kept; they say nothing of what the functions do, and the file name is
 * inferred. The machine is the ROM's hierarchical state machine (romHsm*, as in wlan/apbss.c):
 * one root (macMgmtClearState()) and below it Convert, which has the six states that handle the
 * events. macMgmtFilterState() makes the machine of a connection; the connection's own handler
 * hands it events (macMgmtSetupContext(), wlan/chanctx.c).
 */
#include <stddef.h>
#include "robin/led.h"
#include "robin/p2pgo.h"
#include "robin/adhoc.h"
#include "robin/txhold.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/hostcmd.h"
#include "robin/htba.h"
#include "robin/ibss.h"
#include "robin/ie.h"
#include "robin/joinframe.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/task.h"
#include "robin/stamachine.h"
#include "robin/mgmtentry.h"
#include "robin/swcrypto.h"
#include "robin/sync.h"
#include "robin/sys.h"
#include "robin/uapbss.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"

#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))
#define GET_U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))

/* The subtype of an action frame is 13 (frameControl bits 4-7). */
#define WLAN_SUBTYPE_ACTION	13
/* The element and the action categories of the frames macMgmtDispatchState() routes. */
#define WLAN_ACTION_CATEGORY_P2P_PUBLIC	9
#define WLAN_ACTION_CATEGORY_VENDOR	127
#define WLAN_ACTION_CATEGORY_DIRECT	0x11

/* The type of a Marvell TLV in the host command 0xc7's reply: channel and band list. */
#define TLV_CHANNEL_BAND_LIST		42

/* The tag the frame is sent with (what the transmit-done task reports it by). */
#define WLAN_TX_TAG_PROBE_REQUEST	32
#define WLAN_TX_TAG_AMP_PROBE_REQUEST	17

/* The size of the buffer of a frame of the management layer, and of the host's event messages. */
#define WLAN_MGMT_FRAME_SIZE		0x3b8
#define WLAN_HOST_MESSAGE_SIZE		0x258
#define WLAN_HOST_MESSAGE_OFFSET	176
#define WLAN_HOST_MESSAGE_CLEAR		0x69

/* The host's event that tells the SME the management exchange ended (wlanMgmtReport). */
#define WLAN_EVENT_MGMT_REPORT		17

/* The events macMgmtTaskMain() is woken with, and the connection's event that tells it of the message. */
#define WLAN_CONN_EVENT_HOST_MESSAGE	128

/* The address the IBSS code compares with when it has none (wlan/chanctx.c). */
extern uint8_t wlanNullAddress[8];

/* The SQRAM's HT capability element of the association (wlan/joinframe.c). */
extern uint8_t wlanAssocHtCapability[28];

__attribute__((noinline)) const void *p2pDirectFormatTable(uint8_t *length);					/* 0x42374 */
int p2pDirectFormatEntry(struct WlanConnection *connection, const uint8_t *body,
			 const uint8_t *frame);						/* 0x41894 */
void macMgmtFilterTable(WlanWirelessFrame *frame, const uint8_t *address);		/* 0x37d26 */
void romLibSyncContext(unsigned int mask);						/* ROM */
void macTxNotifyQueryFrame(struct WlanConnection *connection);
void macTxNotifyDispatchFrame(struct WlanConnection *connection);
void macTxNotifyParseFrame(struct WlanConnection *connection);
extern struct OsaBufferPool wlanMgmtFramePool;			/* 0x04000de0 */
OsaBuffer *wlanIbssBeaconBuffer __attribute__((section(".itcm.bss")));			/* 0x54c20 */
OsaBuffer *wlanIbssProbeResponseBuffer __attribute__((section(".itcm.bss")));		/* 0x54c24 */
WlanWirelessFrame *wlanIbssProbeResponseFrame __attribute__((section(".itcm.bss")));	/* 0x54c28 */
WlanWirelessFrame *wlanIbssBeaconFrame __attribute__((section(".itcm.bss")));		/* 0x54c2c */

int wlanSlotCancelFlag0(void);							/* 0xb984 */

struct WlanMgmtReport wlanMgmtReport __attribute__((section(".itcm.bss")));		/* 0x57832 */
uint8_t wlanProbeRates[14] __attribute__((section(".itcm.bss")));			/* 0x57824 */
struct WlanIbssStates wlanIbssStates __attribute__((section(".itcm.bss")));		/* 0x5783c */

/* The links and the management layer wake the MAC management task through its event flags. */
static inline int macMgmtWake(uint32_t events)
{
	return osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, events, MGMT_EVENT_SET_OR);
}

/* Queues a connection for the peer check; without room in the queue, the task is not woken. */
int macMgmtConfigureState(struct WlanConnection *connection)
{
	struct WlanConnection *message = connection;

	if (tx_queue_send((TX_QUEUE *)&macMgmtPeerCheckQueue, &message, 0) != 0)
		return 1;
	macMgmtWake(MGMT_EVENT_PEER_CHECK);
	return 0;
}

/* Hands a timer to the task, which runs it. */
int macMgmtCheckState(struct WlanSoftTimer *timer)
{
	int result;

	timer->unstarted = 1;
	result = osaMsgQueueSend(*wlanTaskHandles.timerQueue, 4, (uintptr_t)timer, 0);
	if (result == 0)
		return macMgmtWake(MGMT_EVENT_TIMERS);
	return result;
}

int macMgmtPostReceived(void)
{
	return macMgmtWake(MGMT_EVENT_POST_RECEIVED);
}

int macMgmtPostRelease(void)
{
	return macMgmtWake(MGMT_EVENT_POST_RELEASE);
}

/* The broadcast address an action frame may be sent to (0x540f4). */
static const uint8_t macMgmtBroadcastAddress[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/*
 * The action frames of ring C's connection: Block Ack and 20/40 coexistence, the public action
 * frames of P2P and the vendor-specific ones. `matched` is set when the P2P context has already
 * taken the frame for this connection; otherwise it has to be an action frame for the connection's
 * address or a broadcast one.
 */
int macMgmtDispatchState(OsaBuffer *buffer, uintptr_t argument, int matched)
{
	struct WlanConnection *connection = buffer->context;
	const WlanWirelessFrame *frame = (const WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	int handled = 0;

	if (!matched &&
	    (frame->frameControl[0] >> 4 != WLAN_SUBTYPE_ACTION ||
	     (memcmp(connection->interfaceAddress, frame->address1, 6) != 0 &&
	      memcmp(macMgmtBroadcastAddress, frame->address1, 6) != 0)))
		return 0;
	switch (frame->body[0]) {
	case WLAN_ACTION_CATEGORY_BLOCK_ACK:
		return wlanBaActionReceived(connection, frame->body, (const uint8_t *)frame, argument);
	case WLAN_ACTION_CATEGORY_PUBLIC:
	case WLAN_ACTION_CATEGORY_P2P_PUBLIC:
		handled = 1;
		if (frame->body[1] == 9)
			p2pActionFrameReceive(connection, (const uint8_t *)frame, WLAN_ACTION_CATEGORY_P2P_PUBLIC);
		return handled;
	case WLAN_ACTION_CATEGORY_DIRECT:
		return p2pDirectFormatEntry(connection, frame->body, (const uint8_t *)frame);
	case WLAN_ACTION_CATEGORY_VENDOR:
		p2pActionFrameReceive(connection, (const uint8_t *)frame, WLAN_ACTION_CATEGORY_VENDOR);
		return 1;
	default:
		return handled;
	}
}

/* The supported rates of the probe requests: the rates of the host's rate set, without the basic-rate bit. */
void macMgmtParseState(const uint8_t *rates, unsigned int count)
{
	unsigned int limit = 14;
	unsigned int index;

	__aeabi_memclr(wlanProbeRates, sizeof(wlanProbeRates));
	if (count < limit)
		limit = count;
	for (index = 0; index < limit; index = (uint8_t)(index + 1))
		wlanProbeRates[index] = rates[index] & 0x7f;
}

/*
 * Sends a probe request. A P2P group owner (or a connection being torn down) adds the P2P element
 * with the attributes the host kept; `peer` is the destination, the broadcast address without it.
 * Only the first two arguments of the third's callers differ: the SSID the request asks for and the
 * elements the host gave to append. `release` is what frees the frame once it has been sent.
 */
int macMgmtBuildState(struct WlanConnection *connection, const uint8_t *peer, const uint8_t *ssid,
		      const uint8_t *elements, unsigned int length, uint8_t tag,
		      int (*release)(OsaBuffer *buffer))
{
	uint32_t channel = 0;
	uint32_t *channelPointer = NULL;
	OsaBuffer *buffer;
	WlanWirelessFrame *frame;
	const uint8_t *destination;
	const void *vendor;
	uint8_t vendorLength;

	if (wlanCurrentBand() == 0) {
		channelPointer = &channel;
		channel = wlanOperatingChannel();
	}
	buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE);
	if (buffer == NULL)
		return -1;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	destination = peer != NULL ? peer : wlanScanAnyBssid;
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, destination, 4, destination);
	frame->bodyLength[0] = 0;
	frame->bodyLength[1] = 0;
	fmCoexTrackTable(frame, ssid, wlanProbeRates, channelPointer);
	vendor = p2pDirectFormatTable(&vendorLength);
	if (vendorLength != 0)
		wlanAppendIe(frame, 0xdd, vendor, vendorLength);
	if (connection->interfaceType == 3 &&
	    (connection->p2pState == 1 || connection->p2pState == 3 || connection->mode == 3)) {
		struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
		/* The context the attributes are put into; the selection leaves it empty. */
		struct WlanDirectContext *volatile context = NULL;
		unsigned int exclude = 0;
		unsigned int include;
		uint8_t deviceId[9];
		unsigned int added;

		p2pDirectSelectState(connection, frame);
		if (ps->p2pFlags & 2) {
			deviceId[0] = 3;
			deviceId[1] = 6;
			deviceId[2] = 0;
			__aeabi_memcpy(&deviceId[3], ps->p2pDeviceAddress, 6);
			context->attributes[P2P_SLOT_DEVICE_ID] = deviceId;
			exclude = 8;
		} else if (GET_U16(ps->p2pAttribute.length) != 0) {
			context->attributes[P2P_SLOT_DEVICE_ID] = (uint8_t *)&ps->p2pAttribute;
			exclude = 8;
		}
		include = (connection->mode == 2 ? 0x20000 : 0) | 0x144;
		if (connection->mode == 1) {
			uint8_t options = ps->p2pOptions;

			ps->p2pOptions = 0;
			added = p2pDirectCheckContext(connection, frame, NULL, include, exclude);
			ps->p2pOptions = options;
		} else {
			added = p2pDirectCheckContext(connection, frame, context, include, exclude);
		}
		PUT_U16(frame->bodyLength, GET_U16(frame->bodyLength) + added);
	}
	macMgmtFilterTable(frame, frame->address2);
	if (length != 0 && elements != NULL)
		wlanAppendIeList(frame, 0, elements, length);
	if (release != NULL) {
		buffer->flags |= 1;
		buffer->release = release;
	}
	buffer->data[109] = tag;
	if (tag == WLAN_TX_TAG_PROBE_REQUEST) {
		buffer->data[2] = wlanPsProcessFrame(connection, 4, peer == NULL);
	} else if (connection->interfaceType != 3) {
		buffer->data[2] = wlanCurrentBand() == 1 ? 5 : 0;
	}
	wlanCustomIeApply(connection, 1, frame, 0);
	if (wlanMgmtSendFrame(buffer) == -1)
		return -1;
	return 0;
}

/* A probe request of the connection for its own SSID, to its own address. */
int macMgmtComputeState(struct WlanConnection *connection)
{
	return macMgmtBuildState(connection, connection->localAddress, connection->networkSsid, NULL, 0,
				 WLAN_TX_TAG_PROBE_REQUEST, NULL);
}

/*
 * The channels a 40 MHz channel covers: the channel's own bit, and the bit of the other 20 MHz channel
 * four channels above or below it (flags bits 4-5: 1 the secondary channel is above, 3 below).
 */
int macMgmtQueryState(uint32_t flags, uint32_t extra)
{
	int8_t channel = (int8_t)extra;
	int mask = 1 << channel;
	unsigned int offset = flags & WLAN_CHANNEL_OFFSET_MASK;

	if (offset == WLAN_CHANNEL_OFFSET_ABOVE)
		return mask | (1 << (int8_t)(channel + 4));
	if (offset == WLAN_CHANNEL_OFFSET_BELOW)
		return mask | (1 << (int8_t)(channel - 4));
	return mask;
}

/*
 * The AMP link's probe request (driver command 0x59): the link's name as the SSID, the probe
 * rates split into the supported rates and the extended ones (above eight), the channel. Only the
 * connection is used; the rest is what the driver hands every command handler.
 */
int macMgmtResetState(uintptr_t argument, unsigned int command, uintptr_t second, int third)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	uint32_t channel = wlanOperatingChannel();
	unsigned int extended = 0;
	unsigned int count;
	unsigned int nameLength;
	OsaBuffer *buffer;
	WlanWirelessFrame *frame;
	const uint8_t *name;

	buffer = wlanMgmtAllocFrame(connection, WLAN_MGMT_FRAME_SIZE);
	if (buffer == NULL)
		return -1;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, connection->peerAddress, 4,
			   connection->interfaceAddress);
	frame->bodyLength[0] = 0;
	frame->bodyLength[1] = 0;
	name = (const uint8_t *)connection->localName.name;
	nameLength = romStringLength(name, 32);
	wlanAppendIe(frame, 0, name, nameLength);
	count = romStringLength(wlanProbeRates, 14);
	if (count > 8) {
		extended = (uint8_t)(count - 8);
		count = 8;
	}
	wlanAppendIe(frame, 1, wlanProbeRates, count);
	wlanAppendIe(frame, 3, &channel, 1);
	if (extended != 0)
		wlanAppendIe(frame, 0x32, &wlanProbeRates[count], extended);
	buffer->data[109] = WLAN_TX_TAG_AMP_PROBE_REQUEST;
	buffer->data[2] = wlanCurrentBand() == 1 ? 5 : 0;
	if (wlanMgmtSendFrame(buffer) != -1)
		return 0;
	return -1;
}

/* A message for the host: waits for a buffer of the management task's pool. */
OsaBuffer *wlanHostMessageAlloc(struct WlanConnection *connection)
{
	OsaBuffer *message;

	while ((message = osa_BufferAlloc(&wlanMgmtFramePool, WLAN_HOST_MESSAGE_SIZE, 0)) == NULL)
		osaTaskSleep(1);
	message->context = connection;
	message->dataOffset = WLAN_HOST_MESSAGE_OFFSET;
	__aeabi_memclr((uint8_t *)message + WLAN_HOST_MESSAGE_OFFSET, WLAN_HOST_MESSAGE_CLEAR);
	return message;
}

/*
 * The security of the connection changed: its keys are disabled; a connection without privacy
 * is marked open, and its pairwise key stops being held back for the EAPOL exchange.
 */
int macMgmtResolveState(struct WlanConnection *connection, unsigned int security)
{
	cipher_key_t *key = NULL;

	if (connection->keyBlocks[1] != NULL)
		key = &connection->keyBlocks[1]->key;
	if (security != 0 || !(connection->bssCapability[0] & 0x10)) {
		keyApi_DisableKeys(connection, 1, 1);
		if (!(connection->bssCapability[0] & 0x10)) {
			connection->flags |= 3;
			if (key != NULL)
				key->hdr.keyState &= ~KEY_STATE_FORCE_EAPOL_UNENCRYPTED;
		}
	}
	wlanBaTablesChangedHandler();
	return hostCmdConnectionChanged();
}

/* The connection is left: its peer, state and keys are dropped. */
void macMgmtScheduleState(struct WlanConnection *connection)
{
	connection->state = 0;
	connection->flags2 &= ~0x10;
	__aeabi_memclr(connection->peerAddress, 6);
	macMgmtConfigureEntry(NULL, NULL, NULL, NULL, NULL);
	wlanSlotCancelFlag0();
	ledFirmwareStateSet(0);
	if (!(connection->hostFlags & WLAN_CONN_HOST_FLAG_8)) {
		UINT posture = sysIrqDisable();

		wlanCryptoTxDrain(&wlanCryptoTxRing);
		wlanCryptoRxDrain(&wlanCryptoRxRing);
		sysIrqRestore(posture);
		keyApi_DisableKeys(connection, 1, 1);
	}
	supplicantSmeResetNotification();
}

/* Queues a message for the host and tells the connection's task of it; a full queue drops the message. */
int wlanHostMessagePost(OsaBuffer *message)
{
	if (osaMsgQueueSend(*wlanTaskHandles.cbSmeQueue, 4, (uintptr_t)message, 0) != 0) {
		osa_BufferFree(message);
		return 1;
	}
	wlanPostConnectionEvent(message->context, WLAN_CONN_EVENT_HOST_MESSAGE);
	return 0;
}

/* A host message for a disassociation or deauthentication of the connection (event 0x8024). */
int macMgmtFormatState(struct WlanConnection *connection, const uint8_t *event)
{
	OsaBuffer *message = wlanHostMessageAlloc(connection);
	uint8_t *data;

	if (message == NULL)
		return 0;
	data = (uint8_t *)message + message->dataOffset;
	data[0] = 0x24;
	data[1] = 0x80;
	data[6] = 0;
	data[7] = 0;
	data[2] = 15;
	data[3] = 0;
	__aeabi_memcpy(data + 8, event, 7);
	if (wlanConnectionHasLocalAddress(connection, wlanNullAddress) ||
	    wlanConnectionHasLocalAddress(connection, event))
		macMgmtScheduleState(connection);
	return wlanHostMessagePost(message);
}

/* Records what ended the connection (the code and, with a peer, its reason and address) and tells the SME. */
int macMgmtInspectState(struct WlanConnection *connection, uint8_t code, uint16_t reason,
			const uint8_t *peer)
{
	if (peer != NULL) {
		wlanMgmtReport.reason = reason;
		__aeabi_memcpy(wlanMgmtReport.peer, peer, 6);
	} else {
		wlanMgmtReport.reason = 0;
		__aeabi_memclr(wlanMgmtReport.peer, 6);
	}
	wlanMgmtReport.code = code;
	macMgmtScheduleState(connection);
	return wlanRaiseEvent(WLAN_EVENT_MGMT_REPORT);
}

/* The IBSS's BSSID is the local address, and the channel is the one the radio is on. */
void macMgmtTrackState(struct WlanConnection *connection)
{
	hostCmd001eChannel(connection->bssChannel);
	__aeabi_memcpy(connection->bssid, connection->localAddress, 6);
}

/*
 * Host command 0xc7: the IBSS's BSSID, and, while the connection has joined (flags2 bit 4), its
 * address, capability, security mode, SSID and, as Marvell TLVs, the channel and band, the
 * supported rates, the WMM element and the HT capability. Returns the length of the reply.
 */
unsigned int hostCmd00c7(struct WlanConnection *connection, uint8_t *body)
{
	uint8_t *cursor = body + 16;
	uint32_t band;
	uint32_t channel;
	uint8_t *tlv;
	union {
		SecurityMode_t mode;
		uint8_t bytes[2];
	} security;

	__aeabi_memclr(body, 17);
	__aeabi_memcpy(body + 6, connection->bssid, 6);
	if (connection->flags2 & 0x10) {
		__aeabi_memcpy(body, connection->localAddress, 6);
		body[12] = connection->bssCapability[0];
		body[13] = connection->bssCapability[1];
		security.mode = supplicantCurrentSecurityMode();
		body[14] = security.bytes[0];
		body[15] = security.bytes[1];
		body[16] = 0;
		body[17] = 0;
		body[18] = connection->networkSsidLength;
		body[19] = 0;
		__aeabi_memcpy(body + 20, connection->networkSsid, connection->networkSsidLength);
		tlv = body + 20 + GET_U16(&body[18]);
		tlv[0] = TLV_CHANNEL_BAND_LIST;
		tlv[1] = 1;
		tlv[2] = 2;
		tlv[3] = 0;
		macMgmtPrepareContext((uint8_t *)&band, &channel);
		tlv[4] = band;
		tlv[5] = channel;
		tlv += 4 + GET_U16(&tlv[2]);
		tlv[0] = 1;
		tlv[1] = 0;
		tlv[2] = 14;
		tlv[3] = 0;
		__aeabi_memcpy(tlv + 4, connection->supportedRates, 14);
		cursor = tlv + 4 + GET_U16(&tlv[2]);
		if (wlanWmm.infoIe[0][0] != 0) {
			cursor[0] = wlanWmm.infoIe[0][0];
			cursor[1] = 0;
			cursor[2] = wlanWmm.infoIe[0][1];
			cursor[3] = 0;
			__aeabi_memcpy(cursor + 4, &wlanWmm.infoIe[0][2], 7);
			cursor += 4 + 7;
		}
		if (wlanAssocHtCapability[0] != 0) {
			cursor[0] = wlanAssocHtCapability[0];
			cursor[1] = 0;
			cursor[2] = wlanAssocHtCapability[1];
			cursor[3] = 0;
			__aeabi_memcpy(cursor + 4, wlanAssocHtCapability, 26);
			cursor += 4 + 26;
		}
	}
	return cursor - body;
}

/* Asks the management state to deauthenticate `peer`, on the channel the radio is on. */
int macMgmtAcquireState(struct WlanConnection *connection, const uint8_t *peer, unsigned int reason,
			uint8_t notify)
{
	uint8_t channel[8];
	struct WlanLeaveRequest request;

	hostCmd001eChannel(channel);
	__aeabi_memcpy(request.peer, peer, 6);
	PUT_U16(request.reason, reason);
	__aeabi_memcpy(request.channel, channel, 7);
	return macTxNotifyHandleHeader(connection, &request, notify);
}

/* The host's reply to a Block Ack teardown. */
int wlanHostDelbaReply(struct WlanConnection *connection, const uint8_t *body, int success, const uint8_t *peer)
{
	OsaBuffer *message = wlanHostMessageAlloc(connection);

	if (message == NULL)
		return 0;
	wlanHostDelbaReplyBuild(message, connection, body, success, peer);
	return wlanHostMessagePost(message);
}

/* ... and to a 20/40 BSS coexistence frame. */
int wlanHostCoexistenceReply(struct WlanConnection *connection, const uint8_t *body)
{
	OsaBuffer *message = wlanHostMessageAlloc(connection);

	if (message == NULL)
		return 0;
	wlanHostCoexistenceReplyBuild(message, connection, body);
	return wlanHostMessagePost(message);
}

/*
 * The states. A handler returns NULL when it handled the event and the event when it did not
 * (the parent then sees it). Entering and leaving a state are the ROM's events -1 and -2.
 */
#define states(machine)		((struct WlanIbssStates *)(machine)->root)

/* The root: the machine starts in the Verify state. */
static const struct WlanStaEvent *macMgmtClearState(struct WlanIbssMachine *machine,
						    const struct WlanStaEvent *event)
{
	if (event->id == ROM_HSM_ENTRY)
		romHsmTransition(machine, &states(machine)->verify);
	return NULL;
}

static const struct WlanStaEvent *macMgmtConvertState(struct WlanIbssMachine *machine,
						      const struct WlanStaEvent *event)
{
	if (event->id == ROM_HSM_ENTRY) {
		romHsmTransition(machine, &states(machine)->verify);
		return NULL;
	}
	return event;
}

/* The channel and the P2P state a P2P group owner returns to when it comes back up. */
static inline __attribute__((always_inline)) void macMgmtDirectResume(struct WlanConnection *connection,
								      int started)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t flags[4];

	if (p2pBssActiveGet(connection) == 0 || ps->deferredFrame == NULL)
		return;
	if (ps->p2pChannel != 255) {
		wlanCurrentChannelFlags(flags);
		wlanSetChannel(ps->p2pChannel, flags, 1);
	}
	if (started)
		connection->p2pState = 4;
	p2pActionFrameReceive(connection, NULL, 0);
}

/* The state the machine starts in: it takes the host's requests. */
static const struct WlanStaEvent *macMgmtVerifyState(struct WlanIbssMachine *machine,
						     const struct WlanStaEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	const struct WlanStaEventData *data = event->data;
	struct RomHsmState *target;

	switch (event->id) {
	case WLAN_STA_EVENT_START:
		wlanIbssStart(connection, (const WlanAdHocStart *)data->request);
		return NULL;
	case WLAN_STA_EVENT_JOIN:
		wlanIbssJoin(connection, (const WlanAdHocBssDesc *)data->request);
		return NULL;
	case WLAN_STA_EVENT_RESET_ENTRY:
		if (macMgmtResetEntry(connection, (const struct WlanHostCompletion *)data->message) != 0)
			return NULL;
		target = &states(machine)->adjust;
		break;
	case WLAN_STA_EVENT_QUERY_ENTRY:
		if (macMgmtQueryEntry(connection, (const struct WlanHostCompletion *)data->message) != 0)
			return NULL;
		target = &states(machine)->adjust;
		break;
	case ROM_HSM_EXIT:
		wlanConnectionCommandPrepare(1, 1);
		machine->previous = machine->state;
		return NULL;
	case ROM_HSM_ENTRY:
		machine->reportCode = 0;
		if (connection->interfaceType != 4) {
			macMgmtConvertContext(connection);
			wlanConnectionCommandPrepare(1, 0);
			if (connection->interfaceType == 3)
				macMgmtDirectResume(connection, 0);
		}
		macMgmtValidateTable(0);
		return NULL;
	case WLAN_STA_EVENT_FRAME:
		if (connection != NULL)
			p2pActionFrameRoute(connection, data->frame);
		return event;
	case WLAN_STA_EVENT_JOINED:
		machine->ibss = 1;
		target = &states(machine)->record;
		break;
	case WLAN_STA_EVENT_STARTED:
		machine->ibss = 0;
		target = &states(machine)->record;
		break;
	case WLAN_STA_EVENT_DIRECT_SSID:
		if (connection != NULL && connection->interfaceType == 3 && connection->mode == 1) {
			p2pProbeRequestAnswer(connection, data->frame);
			return NULL;
		}
		return event;
	default:
		return event;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* The machine of the IBSS that is up. */
static const struct WlanStaEvent *macMgmtSelectState(struct WlanIbssMachine *machine,
						     const struct WlanStaEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	const struct WlanStaEventData *data = event->data;
	const struct WlanStaEventMessage *message;
	const uint8_t *peer;
	uint16_t reason;
	uint8_t code;

	switch (event->id) {
	case ROM_HSM_EXIT:
		machine->previous = machine->state;
		return NULL;
	case ROM_HSM_ENTRY:
		romLibSyncContext(0xc00);
		macTxNotifyQueryFrame(connection);
		macTxNotifyDispatchFrame(connection);
		wlanMacEntryEnable(1);
		if (connection->interfaceType == 3)
			macMgmtDirectResume(connection, 1);
		macMgmtValidateTable(1);
		if (machine->reportCode == 0)
			return NULL;
		code = machine->reportCode;
		reason = machine->reportReason;
		peer = connection->localAddress;
		break;
	case WLAN_STA_EVENT_RELEASE:
		macMgmtReleaseContext(connection, data->frame, data->argument);
		return NULL;
	case WLAN_STA_EVENT_MANAGE:
		macMgmtManageContext(connection, data->frame);
		return NULL;
	case WLAN_STA_EVENT_LOST:
		code = 3;
		reason = 0;
		peer = NULL;
		break;
	case WLAN_STA_EVENT_DISASSOCIATION:
	case WLAN_STA_EVENT_DEAUTHENTICATION:
		message = (const struct WlanStaEventMessage *)data->message;
		if (!wlanConnectionHasLocalAddress(connection, message->address))
			return NULL;
		code = event->id;
		reason = GET_U16((const uint8_t *)&message->reason);
		peer = message->address;
		break;
	case WLAN_STA_EVENT_JOINED:
		machine->ibss = 1;
		romHsmTransition(machine, &states(machine)->record);
		return NULL;
	case WLAN_STA_EVENT_STARTED:
		machine->ibss = 0;
		romHsmTransition(machine, &states(machine)->record);
		return NULL;
	case WLAN_STA_EVENT_DIRECT_SSID:
		if (connection != NULL && connection->interfaceType == 3 && connection->mode == 1) {
			p2pProbeRequestAnswer(connection, data->frame);
			return NULL;
		}
		return event;
	default:
		return event;
	}
	macMgmtInspectState(connection, code, reason, peer);
	romHsmTransition(machine, &states(machine)->verify);
	return NULL;
}

/* The IBSS ended: the templates and the beacons are dropped, and the machine starts over. */
static const struct WlanStaEvent *macMgmtAdjustState(struct WlanIbssMachine *machine,
						     const struct WlanStaEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	const struct WlanStaEventData *data = event->data;
	struct RomHsmState *target;

	switch (event->id) {
	case WLAN_STA_EVENT_STOP:
		wlanConnectionSlotDetach(connection);
		wlanIbssBeaconBuffer = NULL;
		wlanIbssProbeResponseBuffer = NULL;
		wlanIbssProbeResponseFrame = NULL;
		wlanIbssBeaconFrame = NULL;
		wlanIbssLeaveAll(1);
		romHsmTransition(machine, &states(machine)->verify);
		macMgmtScheduleEntry(connection);
		return NULL;
	case ROM_HSM_EXIT:
		machine->previous = machine->state;
		return NULL;
	case ROM_HSM_ENTRY:
		macMgmtValidateTable(2);
		wlanMacEntryEnable(1);
		macTxNotifyInspectBuffer();
		macTxNotifyTrackBuffer();
		return NULL;
	case WLAN_STA_EVENT_RELEASE:
		macMgmtReleaseContext(connection, data->frame, data->argument);
		return NULL;
	case WLAN_STA_EVENT_MANAGE:
		macMgmtManageContext(connection, data->frame);
		return NULL;
	case WLAN_STA_EVENT_LOST:
		wlanConnectionCommandCheck(1, 1);
		wlanConnectionCommandPrepare(1, 0);
		macMgmtInspectState(connection, 3, 0, NULL);
		target = &states(machine)->verify;
		break;
	case WLAN_STA_EVENT_HOLD:
	case WLAN_STA_EVENT_RESUME:
		return NULL;
	case WLAN_STA_EVENT_JOINED:
		machine->ibss = 1;
		target = &states(machine)->record;
		break;
	case WLAN_STA_EVENT_STARTED:
		machine->ibss = 0;
		target = &states(machine)->record;
		break;
	default:
		return event;
	}
	romHsmTransition(machine, target);
	return NULL;
}

/* The machine is held: the timers and the MAC entry stop, and only what ended the connection is kept. */
static const struct WlanStaEvent *macMgmtRecordState(struct WlanIbssMachine *machine,
						     const struct WlanStaEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	const struct WlanStaEventMessage *message = (const struct WlanStaEventMessage *)event->data->message;

	switch (event->id) {
	case ROM_HSM_EXIT:
		return NULL;
	case ROM_HSM_ENTRY:
		macTxNotifyParseFrame(connection);
		wlanMacEntryEnable(0);
		wlanIbssTimersControl(0);
		return NULL;
	case WLAN_STA_EVENT_LOST:
		machine->reportCode = 3;
		return NULL;
	case WLAN_STA_EVENT_DISASSOCIATION:
	case WLAN_STA_EVENT_DEAUTHENTICATION:
		if (wlanConnectionHasLocalAddress(connection, message->address)) {
			machine->reportCode = event->id;
			machine->reportReason = GET_U16((const uint8_t *)&message->reason);
		}
		return NULL;
	case WLAN_STA_EVENT_HOLD:
		romHsmTransition(machine, &states(machine)->hold);
		return NULL;
	default:
		return event;
	}
}

/* 0x0003543c: reached from the Record state: it keeps the report, and returns to where the machine was. */
static inline const struct WlanStaEvent *macMgmtHoldState(struct WlanIbssMachine *machine,
						   const struct WlanStaEvent *event)
{
	struct WlanConnection *connection = machine->connection;
	const struct WlanStaEventMessage *message = (const struct WlanStaEventMessage *)event->data->message;

	switch (event->id) {
	case ROM_HSM_EXIT:
		machine->previous = machine->state;
		return NULL;
	case ROM_HSM_ENTRY:
	case 20:
		return NULL;
	case WLAN_STA_EVENT_LOST:
		machine->reportCode = 3;
		return NULL;
	case WLAN_STA_EVENT_DISASSOCIATION:
	case WLAN_STA_EVENT_DEAUTHENTICATION:
		if (wlanConnectionHasLocalAddress(connection, message->address)) {
			machine->reportCode = event->id;
			machine->reportReason = GET_U16((const uint8_t *)&message->reason);
		}
		return NULL;
	case WLAN_STA_EVENT_RESUME:
		romHsmTransition(machine, machine->previous);
		return NULL;
	case WLAN_STA_EVENT_RESELECT:
		machine->reportCode = 0;
		romHsmTransition(machine, &states(machine)->select);
		return NULL;
	case WLAN_STA_EVENT_REVERIFY:
		machine->reportCode = 0;
		romHsmTransition(machine, &states(machine)->verify);
		return NULL;
	default:
		return event;
	}
}

/* Makes the machine of a connection: the states, below the one root, and the handlers. */
void macMgmtFilterState(struct WlanIbssMachine *machine)
{
	machine->root = &wlanIbssStates.clear;
	romHsmStateInit(&wlanIbssStates.clear, NULL, (RomHsmHandler)macMgmtClearState);
	romHsmStateInit(&wlanIbssStates.convert, &wlanIbssStates.clear, (RomHsmHandler)macMgmtConvertState);
	romHsmStateInit(&wlanIbssStates.verify, &wlanIbssStates.convert, (RomHsmHandler)macMgmtVerifyState);
	romHsmStateInit(&wlanIbssStates.select, &wlanIbssStates.convert, (RomHsmHandler)macMgmtSelectState);
	romHsmStateInit(&wlanIbssStates.adjust, &wlanIbssStates.convert, (RomHsmHandler)macMgmtAdjustState);
	romHsmStateInit(&wlanIbssStates.record, &wlanIbssStates.convert, (RomHsmHandler)macMgmtRecordState);
	romHsmStateInit(&wlanIbssStates.hold, &wlanIbssStates.convert, (RomHsmHandler)macMgmtHoldState);
}

/* The last event of the target the host is told of: its bytes, and how many there are. */
uint8_t wlanTargetEvent[256] __attribute__((section(".itcm.bss")));		/* 0x59da4 */
uint8_t wlanTargetEventLength __attribute__((section(".itcm.bss")));		/* 0x59ea4 */

__attribute__((noinline)) const void *p2pDirectFormatTable(uint8_t *length)
{
	*length = wlanTargetEventLength;
	return wlanTargetEvent;
}

/* Nothing is reset (the hook the initialisation calls stays empty). */
__attribute__((noinline)) void p2pDirectResetTable(void)
{
}
