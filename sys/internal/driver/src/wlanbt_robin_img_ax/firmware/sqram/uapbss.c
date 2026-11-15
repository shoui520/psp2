/*
 * The micro access point's BSS (SQRAM 0xc000a6a8..0xc000ab98): the defaults
 * an access point role starts from before the host configures it and falls
 * back to when its BSS stops, and what a connection takes from the BSS. Then
 * the P2P helpers of the same object (0xc000ab98..0xc000adfe): host events
 * about peers, the discovery timers and a group owner's SSID. The filename
 * and the names are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/apps.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/hostps.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/mef.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/p2pgo.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/slot.h"
#include "robin/sys.h"
#include "robin/txpower.h"
#include "robin/txring.h"
#include "robin/uapbss.h"
#include "robin/wapiie.h"
#include "robin/wmm.h"

#define UAP_DEFAULT_SSID		"Marvell Micro AP"
#define UAP_DEFAULT_CHANNELS		11
#define UAP_DEFAULT_CHANNEL		6
#define UAP_SCAN_TIME_TOTAL		2000u	/* TU, shared by the channels */
#define UAP_SCAN_TIME_MAX		110	/* TU, per channel */

/* The extended capabilities element: only the 20/40 BSS coexistence management support bit. */
__attribute__((noinline)) void uapExtCapabilityInit(uint8_t *element)
{
	__aeabi_memclr(element, 6);
	element[0] = 127;
	element[1] = 4;
	element[2] |= 1;
}

/* The overlapping BSS scan parameters element, with 802.11n's default values (little endian). */
__attribute__((noinline)) void uapObssScanInit(uint8_t *element)
{
	element[0] = 74;
	element[1] = 14;
	element[2] = 20;		/* passive dwell, TU */
	element[3] = 0;
	element[4] = 10;		/* active dwell, TU */
	element[5] = 0;
	element[6] = 300 & 0xff;	/* trigger scan interval, s */
	element[7] = 300 >> 8;
	element[8] = 200;		/* passive total per channel, TU */
	element[9] = 0;
	element[10] = 20;		/* active total per channel, TU */
	element[11] = 0;
	element[12] = 5;		/* width transition delay factor */
	element[13] = 0;
	element[14] = 20;		/* activity threshold */
	element[15] = 0;
}

/* Sets the access point role of the connection back to the defaults: an open 2.4 GHz HT BSS on channel 6. */
void uapBssDefaults(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	unsigned int i;

	if (info == NULL || role == NULL)
		return;

	info->ssidLength = sizeof(UAP_DEFAULT_SSID) - 1;
	romStringCopy(info->ssid, UAP_DEFAULT_SSID, sizeof(info->ssid));
	info->beaconPeriod = 100;
	info->dtimPeriod = 1;
	info->rtsThreshold = 2347;
	info->fragThreshold = 2346;
	info->shortRetryLimit = 7;
	info->longRetryLimit = 7;
	__aeabi_memcpy(info->bssid, info->macAddress, sizeof(info->bssid));
	__aeabi_memcpy4(connection->interfaceAddress, info->macAddress, sizeof(connection->interfaceAddress));
	macTxNotifyAcquireBuffer(2, connection->interfaceAddress, connection->interfaceIndex,
				 connection->interfaceType != 2);

	/* the SSID is broadcast; protection is not forced */
	info->protectionFlags = ((info->protectionFlags & ~1) | 6) & 0xc7;
	info->pktFwdCtl = 1;
	info->pktFwdCtlConfigured = 1;
	info->protectionFlags &= ~0xc0;
	info->protectionFlagsHigh &= ~1;

	/* channels 1 to 11, scanned actively and passively, 2000 TU in all */
	info->channelCount = UAP_DEFAULT_CHANNELS;
	__aeabi_memclr4(info->channels, sizeof(info->channels));
	for (i = 0; i < info->channelCount; i++) {
		struct WlanApChannel *channel = &info->channels[i];
		unsigned int time;

		channel->channel = i + 1;
		channel->channelFlags &= ~(WLAN_CHANNEL_BAND_MASK | WLAN_CHANNEL_WIDTH_MASK |
					   WLAN_CHANNEL_OFFSET_MASK);
		channel->scanFlags |= 3;
		time = UAP_SCAN_TIME_TOTAL / info->channelCount;
		if (time > UAP_SCAN_TIME_MAX)
			time = UAP_SCAN_TIME_MAX;
		channel->scanTime[0] = time;
		channel->scanTime[1] = time >> 8;
		time = UAP_SCAN_TIME_TOTAL / info->channelCount;
		channel->measureTime[0] = time;
		channel->measureTime[1] = time >> 8;
	}
	__aeabi_memcpy4(wlanDeviceAddress->address, info->macAddress, sizeof(wlanDeviceAddress->address));

	p2pSqramResetEntry(info->rates);
	info->authAlgorithm = 0;
	info->unicastRate = 0;
	info->txRate = 0;
	info->maxStations = 10;
	info->securityMode = (SecurityMode_t){ .noRsn = 1 };
	info->staAgeout = 1800;
	info->psStaAgeout = 400;
	macMgmtProcessHeader(info);
	p2pSqramSyncEntry(info->capability);
	/* short preamble unless protection is forced */
	info->capability[0] = (info->capability[0] & ~0x20) | (!(info->protectionFlags & 1) << 5);
	macMgmtBuildPacket(connection->interfaceType, connection->bssNumber);

	if (info->bssKey != NULL) {
		cipher_key_t *key = &info->bssKey->key;

		__aeabi_memclr4(key, sizeof(info->bssKey->keyData));
		key->hdr.keyDirection = KEY_DIRECTION_RXTX;
		key->hdr.keyType = 0;
		key->hdr.keyState &= ~KEY_STATE_ENABLE;
	}
	fmCoexClearBuffer(info);
	if (info->securityMode.wapi && info->groupKey != NULL)
		keyApi_FreeKey(&info->groupKey);

	info->htEnabled = 1;
	info->ht2040Managed = 1;
	p2pSqramApplyEntry(info->htCapability);
	info->htCapability[2] &= ~0x10;
	uapObssScanInit(&info->obssScan.id);
	uapExtCapabilityInit(&info->extCapability.id);

	/* WMM, with the TXOP limits of 802.11e for video and voice (32 us units) */
	info->qosEnabled = 1;
	wlanWmmFillParameterElement(info->wmmElement);
	info->acParameters[2].txopLimit = 94;
	info->acParameters[3].txopLimit = 47;
	info->uapsdEnabled = 1;
	wlanWmmSetApsdSupport(info->wmmElement, 1);

	/* the RSN settings, and the ready flag after them */
	__aeabi_memclr4(&info->apRsn, offsetof(struct WlanConnectionInfo, uapsdEnabled) -
			offsetof(struct WlanConnectionInfo, apRsn));
	info->apRsn.AuthKey = 2;
	info->apRsn.AuthKeyCount = 1;
	info->apRsn.Akmp.keyExchange = 0;
	info->apRsn.Akmp.authenticate = 0;
	info->apRsn.MaxPwsHskRetries = 3;
	info->apRsn.MaxGrpHskRetries = 3;
	info->apRsn.PwsHskTimeOut = 100;
	info->apRsn.GrpHskTimeOut = 100;
	info->apRsn.GrpReKeyTime = 86400;
	info->ready = 0;
	wlanPsMachineReset(info);

	role->channel = UAP_DEFAULT_CHANNEL;
	role->channelFlags = WLAN_CHANNEL_BAND_2GHZ;
	info->mgmtForwardMask = 0;
	macMgmtComputePacket(info, info->htOperation, info->htCapability, &role->band,
			     connection->beaconPeriod);
	connection->flags2 &= ~WLAN_CONN2_DOMAIN;
}

/* The access point role's information block was just allocated: its address, its power save machine and the defaults. */
void uapBssInit(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	__aeabi_memcpy4(info->macAddress, wlanDeviceAddress->address, sizeof(info->macAddress));
	wlanPsMachineCreate(info);
	uapBssDefaults(connection);
	macMgmtResolveFrame(info);
	wlanApPsResetGroup(info);
}

/*
 * Applies the BSS to a connection: with a peer, the station at address is
 * given the access point connection's rates, templates and keys first; without
 * one, the access point's own connection takes the BSS's settings as it
 * starts.
 */
void uapBssApply(struct WlanConnection *connection, struct WlanConnection *peer, const uint8_t *address)
{
	struct WlanConnectionInfo *info;
	struct WlanConnectionRole *role;

	if (peer != NULL) {
		__aeabi_memcpy(peer->peerAddress, address, sizeof(peer->peerAddress));
		peer->rateSet = connection->rateSet;
		peer->ofdmHigh = connection->ofdmHigh;
		peer->cckHigh = connection->cckHigh;
		peer->ofdmLow = connection->ofdmLow;
		peer->cckLow = connection->cckLow;
		peer->state = connection->state;
		peer->hostFlags = (peer->hostFlags & ~0x08) | (connection->hostFlags & 0x08);
		peer->keyBlocks[0] = connection->keyBlocks[0];
		peer->flags = (peer->flags & ~0x04) | (connection->flags & 0x04);
		peer->flags = (peer->flags & ~0x10) | (connection->flags & 0x10);
		peer->beaconBuffer = connection->beaconBuffer;
		peer->probeResponseBuffer = connection->probeResponseBuffer;
		peer->beaconRequest = connection->beaconRequest;
		peer->probeResponseRequest = connection->probeResponseRequest;
		peer->flags2 = (peer->flags2 & ~WLAN_CONN2_DOMAIN) | (connection->flags2 & WLAN_CONN2_DOMAIN);
		if (peer->hostFlags & 0x08)
			peer->keyBlocks[1] = connection->keyBlocks[1];
		role = wlanConnectionInfoSlot(peer);
		role->band = wlanConnectionInfoSlot(connection)->band;
	} else {
		peer = connection;
		role = wlanConnectionInfoSlot(connection);
	}

	info = wlanConnectionInfo(peer);
	peer->flags &= ~0x40;
	peer->flags &= ~0x80;
	__aeabi_memcpy(peer->supportedRates, info->rates, sizeof(peer->supportedRates));
	peer->authAlgorithm = info->authAlgorithm;
	peer->authSequence = 1;
	peer->hostFlags |= 0x10;
	peer->lastSequence = 0;
	peer->lastFragment = 0;
	peer->associationId = 0;
	peer->channel = role->channel;
	__aeabi_memcpy(peer->localAddress, info->bssid, sizeof(peer->localAddress));
	romStringCopy(peer->networkSsid, info->ssid, sizeof(peer->networkSsid));
	peer->networkSsidLength = info->ssidLength;
	peer->beaconPeriod = info->beaconPeriod;
	peer->dtimPeriod = info->dtimPeriod;
	peer->rtsThreshold[0] = info->rtsThreshold;
	peer->rtsThreshold[1] = info->rtsThreshold >> 8;
	peer->fragThreshold[0] = info->fragThreshold;
	peer->fragThreshold[1] = info->fragThreshold >> 8;
	peer->shortRetryLimit = info->shortRetryLimit;
	peer->longRetryLimit = info->longRetryLimit;
	peer->flags2 &= ~WLAN_CONN2_ASSOCIATED;
	role->listenInterval = info->listenInterval;
	role->state = 0;
}

/* Leaves the group owner's channel: the P2P connection goes back to discovery. */
void p2pDiscoveryResume(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	ps->p2pBssActive = 0;
	connection->p2pState = 3;
	macMgmtValidateTable(0);
}

/* Sends the host an event about a peer's frame (p2pDirectNotifyContext()); 1: no buffer for it. */
int p2pEventSend(struct WlanConnection *connection, const uint8_t *frame, uint16_t event, unsigned int type,
		 uint16_t value)
{
	OsaBuffer *buffer;
	struct WlanHostEventFrame *eventFrame;

	if (wlanTxAllocFrame(&buffer, (WlanTxDescriptor **)&eventFrame, type == 14 ? 900 : 200, event) == 0)
		return 1;
	p2pDirectNotifyContext(connection, frame, (uint8_t *)&eventFrame->code, type, value);
	eventFrame->length = eventFrame->code;
	wlanHostEventSend(connection, buffer);
	return 0;
}

__attribute__((noinline)) int p2pEventSendPeer(struct WlanConnection *connection, const uint8_t *frame, unsigned int type, uint16_t value)
{
	return p2pEventSend(connection, frame, WLAN_HOST_EVENT_WIFIDIRECT_GENERIC, type, value);
}

/* The P2P frame p2pListenTimerExpired() reports: only its length and the peer's address are read. */
struct P2pEventFrame {
	uint16_t length;
	uint8_t reserved2[10];
	uint8_t address[6];
	uint8_t reserved18[14];
};

/* The time on the listen channel is over: the host is told (event type 2), and discovery goes on. */
void p2pListenTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	struct WlanPsPeerState *ps;
	struct P2pEventFrame frame;
	uint8_t flags[4];

	wlanCurrentChannelFlags(flags);
	ps = wlanConnectionPsState(connection);
	connection->p2pState = 2;
	connection->mode = 1;
	ps->p2pFlags &= ~1;
	wlanSetChannel(ps->defaultChannel, flags, 1);
	frame.length = sizeof(frame);
	__aeabi_memcpy4(frame.address, ps->eventPeerAddress, sizeof(frame.address));
	p2pEventSendPeer(connection, (const uint8_t *)&frame, 2, 0);
	p2pDiscoveryResume(connection);
	ps->p2pBssActive = 0;
}

/*
 * Queues the pending frame. The original takes the frame only when the
 * timer's argument is NULL, from the state wlanConnectionPsState(NULL) gives
 * (none); this is kept as it is.
 */
void p2pPendingFrameTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	if (connection == NULL && ps->pendingFrame != NULL) {
		wlanMgmtSendFrame(ps->pendingFrame);
		ps->pendingFrame = NULL;
	}
}

/* Queues the pending frame; 1: there is none, or it could not be queued. */
int p2pPendingFrameSend(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	if (ps->pendingFrame == NULL || wlanMgmtSendFrame(ps->pendingFrame) != 0)
		return 1;
	return 0;
}

void p2pPendingFrameRetryExpired(uint32_t timer, uint32_t argument)
{
	p2pPendingFrameSend((struct WlanConnection *)argument);
}

/* The wait on the group owner's channel is over: back to discovery. */
void p2pBssTimerExpired(uint32_t timer, uint32_t argument)
{
	struct WlanConnection *connection = (struct WlanConnection *)argument;

	wlanConnectionPsState(connection)->p2pBssActive = 0;
	p2pDiscoveryResume(connection);
}

void p2pDiscoverabilityTimerExpired(uint32_t timer, uint32_t argument)
{
	wlanConnectionPsState((struct WlanConnection *)argument)->discoverabilityPending = 0;
}

/*
 * A group owner's frame names its SSID: a "DIRECT-" SSID completes the group
 * client entry of its device (one with only the device address, length 13)
 * and the group's own Group ID when the device is the group's owner.
 */
void p2pGroupSsidUpdate(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps;
	struct WlanDirectContext attributes;
	WlanIePointers elements;
	uint8_t deviceInfo[12];
	unsigned int first = 0;
	unsigned int changed = 0;
	struct WlanDirectContext *direct;
	const uint8_t *ssid;
	uint8_t *client;

	ps = wlanConnectionPsState(connection);
	if (frame == NULL)
		return;
	direct = p2pDirectScheduleContext(frame, 0, &first, &changed, &attributes);
	if (direct == NULL)
		return;
	wlanParseIes(frame + 44, ((frame[1] << 8) | frame[0]) - 12, &elements);
	ssid = elements.pSsid;
	if (ssid == NULL || direct->attributes[P2P_SLOT_DEVICE_INFO] == NULL)
		return;
	if (changed & 0x2000)
		p2pDirectSyncContext(direct->attributes[P2P_SLOT_DEVICE_INFO], deviceInfo, 9,
				     &direct->attributes[P2P_SLOT_DEVICE_INFO]);
	client = p2pSqramSelectContext(connection, direct->attributes[P2P_SLOT_DEVICE_INFO] + 3, NULL, 0);
	if (client != NULL && ((client[2] << 8) | client[1]) == 13 &&
	    memcmp(elements.pSsid + 2, "DIRECT-", 7) == 0) {
		unsigned int length;

		__aeabi_memcpy(client + 9, elements.pSsid + 2, elements.pSsid[1]);
		length = elements.pSsid[1] + 6;
		client[1] = length;
		client[2] = length >> 8;
	}
	if (ps->groupIdLength == 13 && memcmp(elements.pSsid + 2, "DIRECT-", 7) == 0 &&
	    memcmp(direct->attributes[P2P_SLOT_DEVICE_INFO] + 3, ps->groupDeviceAddress, 6) == 0) {
		__aeabi_memcpy(ps->groupSsid, elements.pSsid + 2, elements.pSsid[1]);
		ps->groupIdLength = elements.pSsid[1] + 6;
	}
}

/* Nonzero while the radio is on the group owner's channel. */
int p2pBssActiveGet(struct WlanConnection *connection)
{
	return wlanConnectionPsState(connection)->p2pBssActive;
}

/* Sets the P2P state of a connection that is in discovery (mode 1). */
__attribute__((noinline)) void p2pConnectionStateSet(struct WlanConnection *connection, unsigned int state)
{
	if (connection != NULL && connection->mode == 1)
		connection->p2pState = state;
}

/*
 * The group owner negotiation: the higher GO intent becomes the group owner,
 * the peer's tie-breaker bit decides equal ones, and two intents of 15 fail.
 */
unsigned int p2pGroupRoleDecide(const struct WlanDirectContext *peer, unsigned int intent)
{
	const uint8_t *attribute;
	unsigned int peerIntent;
	unsigned int tieBreaker;

	if (peer == NULL || (attribute = peer->attributes[P2P_SLOT_GO_INTENT]) == NULL)
		return P2P_ROLE_UNKNOWN;
	peerIntent = attribute[3] >> 1;
	tieBreaker = attribute[3] & 1;
	if (peerIntent < intent)
		return P2P_ROLE_GROUP_OWNER;
	if (peerIntent > intent)
		return P2P_ROLE_CLIENT;
	if (peerIntent == 15 && intent == 15)
		return P2P_ROLE_FAILED;
	if (tieBreaker)
		return P2P_ROLE_CLIENT;
	return P2P_ROLE_GROUP_OWNER;
}

/*
 * Takes the role the negotiation gave: a group owner makes its group's SSID,
 * a client takes the peer's group (its device address and BSSID). A persistent
 * group is recorded with its Group ID.
 */
void p2pGroupRoleApply(struct WlanConnection *connection, struct WlanDirectContext *peer, unsigned int role)
{
	struct WlanPsPeerState *ps;
	const uint8_t *deviceAddress = NULL;
	uint8_t groupBssid[9];
	uint8_t groupId[22];
	const uint8_t *groupIdAttribute;
	const uint8_t *bssid;

	ps = wlanConnectionPsState(connection);
	if (role == P2P_ROLE_GROUP_OWNER) {
		connection->mode = P2P_ROLE_GROUP_OWNER;
		ps->p2pOptions |= 0x01;			/* group capability: P2P group owner */
		ps->capabilityAttribute[3] |= 0x10;	/* device capability: P2P device limit */
		__aeabi_memcpy(ps->groupSsid, "DIRECT-", 7);
		p2pDirectReleaseState(ps->groupSsid + 7);
		ps->groupIdLength = 15;
		wlanConnectionInfo(connection)->updatePassPhrase = 1;
		ps->ssidPending = 0;
		if (!(ps->p2pOptions & 0x02))		/* not a persistent group */
			return;
		groupIdAttribute = &ps->groupIdType;
		deviceAddress = peer->attributes[P2P_SLOT_DEVICE_INFO] + 3;
	} else if (role == P2P_ROLE_CLIENT) {
		connection->mode = P2P_ROLE_CLIENT;
		__aeabi_memcpy(ps->groupDeviceAddress, peer->attributes[P2P_SLOT_DEVICE_INFO] + 3,
			       sizeof(ps->groupDeviceAddress));
		__aeabi_memcpy(ps->groupSsid, "DIRECT-", 7);
		ps->groupIdLength = 13;
		bssid = peer->attributes[P2P_SLOT_GROUP_BSSID];
		if (bssid == NULL) {
			groupBssid[0] = P2P_ATTR_GROUP_BSSID;
			groupBssid[1] = 6;
			groupBssid[2] = 0;
			__aeabi_memclr(groupBssid + 3, 6);
			bssid = groupBssid;
		}
		__aeabi_memcpy(ps->groupBssidAttribute, bssid, 9);
		if (!(peer->attributes[P2P_SLOT_CAPABILITY][4] & 0x02))	/* the peer's group is not persistent */
			return;
		groupId[0] = P2P_ATTR_GROUP_ID;
		groupId[1] = 13;
		groupId[2] = 13 >> 8;
		__aeabi_memcpy(groupId + 3, peer->attributes[P2P_SLOT_DEVICE_INFO] + 3, 6);
		__aeabi_memcpy(groupId + 9, "DIRECT-", 7);
		groupIdAttribute = groupId;
	} else {
		return;
	}
	p2pSqramConvertContext(connection, groupIdAttribute, ps->groupBssidAttribute, ps->groupSsid,
			       connection->mode, deviceAddress);
}

/* Adds a P2P element with the Device ID attribute of address to the frame. */
void p2pDeviceIdAppend(uint8_t *frame, const uint8_t *address)
{
	uint8_t *element = p2pDirectFilterState(frame);
	uint8_t deviceId[9];
	unsigned int added;

	deviceId[0] = P2P_ATTR_DEVICE_ID;
	deviceId[1] = 6;
	deviceId[2] = 0;
	__aeabi_memcpy(deviceId + 3, address, 6);
	added = p2pDirectConfigureContext(element, deviceId);
	added += (frame[1] << 8) | frame[0];
	frame[0] = added;
	frame[1] = added >> 8;
}

/*
 * Starts a P2P public action or action frame to destination, with the P2P
 * OUI and subtype. A group owner sends from its P2P interface address, unless
 * the peer is associated (state 3), in the group's BSS; other modes use the
 * wildcard BSSID. 0, the frame in *frame; -1: no buffer.
 */
int p2pActionFrameAlloc(struct WlanConnection *connection, OsaBuffer **frame, const uint8_t *destination,
			unsigned int kind, uint8_t subtype)
{
	static const uint8_t wildcard[8] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0 };
	uint8_t bssidAny[8];
	struct WlanPsPeerState *ps;
	unsigned int peerState = 0;
	OsaBuffer *buffer;
	uint8_t *data;
	uint8_t *body;
	const uint8_t *source;
	const uint8_t *bssid;
	unsigned int length;

	__builtin_memcpy(bssidAny, wildcard, sizeof(bssidAny));
	ps = wlanConnectionPsState(connection);
	buffer = wlanMgmtAllocFrame(connection, 400);
	if (buffer == NULL)
		return -1;
	if (connection->mode == P2P_ROLE_GROUP_OWNER) {
		struct WlanConnection *peer = wlanConnectionFindByPeer(destination);

		if (peer != NULL) {
			wlanConnectionInfo(peer);
			peerState = wlanConnectionInfoSlot(peer)->state;
		}
	}
	buffer->context = connection;
	data = (uint8_t *)buffer + buffer->dataOffset;
	if (data == NULL) {
		p2pFrameDataMissing++;
		return -1;
	}
	bssid = connection->mode == P2P_ROLE_GROUP_OWNER ? connection->localAddress : bssidAny;
	source = connection->interfaceAddress;
	if (connection->mode == P2P_ROLE_GROUP_OWNER && peerState != 3)
		source = ps->interfaceAddress;
	wlanMgmtBuildFrame(buffer, source, destination, 13, bssid);
	buffer->dataOffset = 176;
	if (kind == P2P_FRAME_PUBLIC_ACTION) {
		body = data + 32;
		body[0] = 4;			/* public action */
		body[1] = 9;			/* vendor specific */
		__aeabi_memcpy(body + 2, wlanP2pOui, 4);
		body[6] = subtype;
		length = ((data[1] << 8) | data[0]) + 8;
	} else if (kind == P2P_FRAME_ACTION) {
		body = data + 32;
		body[0] = P2P_FRAME_ACTION;	/* vendor specific category */
		__aeabi_memcpy(body + 1, wlanP2pOui, 3);
		body[4] = 9;			/* the P2P OUI type */
		body[5] = subtype;
		length = ((data[1] << 8) | data[0]) + 7;
	} else {
		goto done;
	}
	data[0] = length;
	data[1] = length >> 8;
done:
	*frame = buffer;
	return 0;
}

/* Sends a device discoverability request for the wanted client to its group owner, and waits 2 s for the answer. */
int p2pDiscoverabilityRequestSend(struct WlanConnection *connection, const uint8_t *destination)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	OsaBuffer *buffer;
	uint8_t *data;

	if (p2pActionFrameAlloc(connection, &buffer, destination, P2P_FRAME_PUBLIC_ACTION, 5) != 0)
		return -1;
	data = (uint8_t *)buffer + buffer->dataOffset;
	p2pDeviceIdAppend(data, ps->discoveryDeviceAddress);
	data[32 + 7] = ps->dialogToken;
	if (wlanMgmtSendFrame(buffer) != 0)
		return -1;
	osaCreateTimer(p2pDiscoverabilityTimerExpired, (uint32_t)connection, 2000, &ps->discoverabilityTimer, 0);
	return 0;
}

/*
 * A group owner's beacon (chanctx.c): when its Group Info attribute lists the
 * client the host wants discovered, the group owner is sent a device
 * discoverability request, once.
 */
void p2pDiscoverabilityCheck(struct WlanDirectContext *peer, uint32_t changed, struct WlanPsPeerState *ps)
{
	uint8_t *pending = &ps->discoverabilityPending;
	uint8_t groupInfo[716];
	const uint8_t *clients;
	unsigned int i;

	if (*pending) {
		p2pDiscoverabilityRequestSend(p2pDeviceConnection, peer->attributes[P2P_SLOT_DEVICE_INFO] + 3);
		ps->p2pFlags &= ~0x04;
	}
	if (peer == NULL || peer->attributes[P2P_SLOT_GROUP_INFO] == NULL ||
	    peer->attributes[P2P_SLOT_DEVICE_INFO] == NULL)
		return;
	if (changed & (1u << P2P_ATTR_GROUP_INFO))
		p2pDirectSyncContext(peer->attributes[P2P_SLOT_GROUP_INFO], groupInfo, -1,
				     &peer->attributes[P2P_SLOT_GROUP_INFO]);
	clients = peer->attributes[P2P_SLOT_GROUP_INFO] + 3;
	for (i = 0; i <= (unsigned int)((peer->attributes[P2P_SLOT_GROUP_INFO][2] << 8) |
					 peer->attributes[P2P_SLOT_GROUP_INFO][1]);
	     i = (uint16_t)(i + clients[i] + 1)) {
		/* a client info descriptor: its length, then the client's device address */
		if (memcmp(clients + i + 1, ps->discoveryDeviceAddress, 6) == 0) {
			p2pDiscoverabilityRequestSend(p2pDeviceConnection, peer->attributes[P2P_SLOT_DEVICE_INFO] + 3);
			ps->p2pFlags &= ~0x04;
			__aeabi_memcpy(ps->p2pGroupId, peer->attributes[P2P_SLOT_DEVICE_INFO] + 3, 6);
			*pending = 1;
			return;
		}
	}
}

/*
 * Adds the WPS element of attributes 0 and 11 the host gave, with the second
 * one's value (bytes 4 and 5, big endian) set from wpsDevicePasswordId for this
 * frame only, while groupSettingE is set.
 */
unsigned int p2pWpsConfigAppend(struct WlanConnection *connection, uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *attribute = ps->wpsAttributes[WPS_SLOT_DEVICE_PASSWORD_ID];
	uint8_t saved[2] = { 0, 0 };
	uint32_t value = 0;
	unsigned int result;

	if (attribute != NULL && ps->groupSettingE != 0) {
		saved[0] = attribute[4];
		saved[1] = attribute[5];
		value = (ps->wpsDevicePasswordId << 8) | ((ps->wpsDevicePasswordId >> 8) & 0xff);
		attribute[4] = ((uint8_t *)&value)[0];
		attribute[5] = ((uint8_t *)&value)[1];
	}
	result = p2pDirectConvertState(connection, (WlanWirelessFrame *)frame, 0x801);
	if (ps->wpsAttributes[WPS_SLOT_DEVICE_PASSWORD_ID] != NULL && ps->groupSettingE != 0) {
		attribute[4] = saved[0];
		attribute[5] = saved[1];
	}
	return result;
}

/*
 * Chooses the group's operating channel from the channels both sides
 * support: the peer's operating channel, else the one the host asked for,
 * else the first common one. p2pStatus.value says why it failed (4: invalid
 * parameters, 7: no common channels).
 */
void p2pOperatingChannelSelect(struct WlanConnection *connection, const struct WlanDirectContext *peer)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	const uint8_t *list = peer->attributes[P2P_SLOT_CHANNEL_LIST];
	const uint8_t *operating;
	unsigned int count;
	unsigned int i;
	int peerChannel;
	int requested;
	unsigned int channel;

	if (list == NULL) {
		ps->p2pStatus.value = 4;
		return;
	}
	if (peer->attributes[P2P_SLOT_OPERATING_CHANNEL] != NULL) {
		ps->p2pStatus.value = 4;
		count = list[7];
		for (i = 0; i < count; i = (uint8_t)(i + 1)) {
			if (list[8 + i] == peer->attributes[P2P_SLOT_OPERATING_CHANNEL][7]) {
				ps->p2pStatus.value = 0;
				goto common;
			}
		}
	}
	if (ps->p2pStatus.value != 0)
		return;
common:
	p2pDirectUpdateContext(connection, peer->attributes[P2P_SLOT_CHANNEL_LIST],
			       peer->attributes[P2P_SLOT_CHANNEL_LIST][7], ps->channelListAttribute,
			       ps->channelListAttribute[7]);
	count = ps->commonChannels.count;
	if (count == 0) {
		ps->p2pStatus.value = 7;
		return;
	}
	peerChannel = 0;
	requested = 0;
	for (i = 0; i < count; i = (uint8_t)(i + 1)) {
		operating = peer->attributes[P2P_SLOT_OPERATING_CHANNEL];
		if (operating != NULL && ps->commonChannels.channels[i] == operating[7])
			peerChannel = 1;
		if (ps->commonChannels.channels[i] == ps->requestedChannel)
			requested = 1;
	}
	if (peerChannel == 1) {
		channel = peer->attributes[P2P_SLOT_OPERATING_CHANNEL][7];
	} else if (requested == 1) {
		ps->groupChannel = ps->requestedChannel;
		return;
	} else {
		channel = ps->commonChannels.channels[0];
	}
	ps->groupChannel = channel;
	ps->requestedChannel = channel;
}

/*
 * Builds the P2P element of a GO negotiation request (kind 0), response (1)
 * or confirmation (2) after the peer's frame: the negotiation's result is in
 * p2pStatus.value (1: the host is not ready, 2: incompatible parameters,
 * 4: invalid parameters, 7: no common channels, 9: both want to be the group
 * owner). While a station is associated the group is offered its channel
 * only. -1: the peer's confirmation reports a failure.
 */
int p2pGoNegotiationBuild(struct WlanConnection *connection, const uint8_t *request, uint8_t *response,
			  unsigned int kind)
{
	struct WlanDirectContext *peer = NULL;
	unsigned int given = 0;
	unsigned int include = 0;
	unsigned int first = 0;
	unsigned int changed = 0;
	int concurrent = 0;
	struct WlanPsPeerState *ps;
	struct WlanConnection *station;
	struct WlanConnection *accessPoint;
	struct WlanDirectContext attributes;
	uint8_t statusCopy[4];
	uint8_t channelListCopy[44];
	uint8_t operatingCopy[8];
	uint8_t intentCopy[4];
	uint8_t channelList[44];
	uint8_t operating[8];
	uint8_t band;
	uint32_t channel;
	unsigned int role;
	unsigned int length;

	ps = wlanConnectionPsState(connection);
	if (request != NULL)
		peer = p2pDirectScheduleContext(request, 1, &first, &changed, &attributes);
	if (kind == P2P_GO_NEG_CONFIRM && peer->attributes[P2P_SLOT_STATUS][3] != 0)
		return -1;
	if (changed & (1u << P2P_ATTR_GO_INTENT))
		p2pDirectSyncContext(peer->attributes[P2P_SLOT_GO_INTENT], intentCopy, 4,
				     &peer->attributes[P2P_SLOT_GO_INTENT]);
	if (changed & (1u << P2P_ATTR_OPERATING_CHANNEL))
		p2pDirectSyncContext(peer->attributes[P2P_SLOT_OPERATING_CHANNEL], operatingCopy, 8,
				     &peer->attributes[P2P_SLOT_OPERATING_CHANNEL]);
	if (changed & (1u << P2P_ATTR_CHANNEL_LIST))
		p2pDirectSyncContext(peer->attributes[P2P_SLOT_CHANNEL_LIST], channelListCopy, 43,
				     &peer->attributes[P2P_SLOT_CHANNEL_LIST]);
	if (changed & (1u << P2P_ATTR_STATUS))
		p2pDirectSyncContext(peer->attributes[P2P_SLOT_STATUS], statusCopy, 4,
				     &peer->attributes[P2P_SLOT_STATUS]);

	ps->p2pStatus.value = 0;
	ps->p2pStatus.length[0] = 1;
	ps->p2pStatus.length[1] = 0;
	station = wlanConnectionFirst(0);
	accessPoint = wlanConnectionFirst(2);
	if (station != NULL && (station->flags2 & WLAN_CONN2_ASSOCIATED)) {
		unsigned int listLength;

		/* the station's channel is the only one offered */
		p2pDirectHandleContext(ps->channelListAttribute, channelList);
		macMgmtPrepareContext(&band, &channel);
		channelList[8] = channel;
		channelList[7] = 1;
		listLength = ((channelList[2] << 8) | channelList[1]) + 1;
		channelList[1] = listLength;
		channelList[2] = listLength >> 8;
		concurrent = 1;
	}
	if (wlanConnectionBssStarted(accessPoint) == 1 || concurrent) {
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

		p2pDirectProcessContext(P2P_ATTR_OPERATING_CHANNEL, ps->operatingChannelAttribute, operating);
		operating[7] = role->channel;
		given = 1u << P2P_ATTR_OPERATING_CHANNEL;
		peer->attributes[P2P_SLOT_OPERATING_CHANNEL] = operating;
	}

	if (kind == P2P_GO_NEG_REQUEST) {
		/* length 0: this clears nothing, as the original's arguments are */
		__aeabi_memset4(&ps->commonChannels, 0,
				((ps->commonChannels.length[1] << 8) | ps->commonChannels.length[0]) + 2);
		p2pDirectManageContext(connection);
		ps->groupOwnerIntentAttribute[3] = (ps->groupOwnerIntentAttribute[3] & ~1) | (ps->tieBreaker & 1);
		if (concurrent) {
			peer->attributes[P2P_SLOT_CHANNEL_LIST] = channelList;
			given |= 1u << P2P_ATTR_CHANNEL_LIST;
		}
		goto attributes;
	}

	if (ps->groupSettingH == 0) {
		ps->p2pStatus.value = 1;
	} else if (ps->p2pStatus.value == 0) {
		const uint8_t *firstList;
		const uint8_t *secondList;
		unsigned int firstCount;
		unsigned int secondCount;

		if (concurrent) {
			firstList = channelList;
			secondList = peer->attributes[P2P_SLOT_CHANNEL_LIST];
			secondCount = secondList[7];
			firstCount = channelList[7];
		} else {
			secondCount = ps->channelListAttribute[7];
			firstCount = peer->attributes[P2P_SLOT_CHANNEL_LIST][7];
			secondList = ps->channelListAttribute;
			firstList = peer->attributes[P2P_SLOT_CHANNEL_LIST];
		}
		p2pDirectUpdateContext(connection, firstList, firstCount, secondList, secondCount);
		if (ps->commonChannels.count == 0)
			ps->p2pStatus.value = 7;
	}

	/* the attributes the peer's frame must carry */
	if (kind == P2P_GO_NEG_RESPONSE) {
		if (peer == NULL)
			goto decide;
		if (peer->attributes[P2P_SLOT_CONFIGURATION_TIMEOUT] == NULL ||
		    peer->attributes[P2P_SLOT_CHANNEL_LIST] == NULL ||
		    peer->attributes[P2P_SLOT_CAPABILITY] == NULL ||
		    peer->attributes[P2P_SLOT_LISTEN_CHANNEL] == NULL ||
		    peer->attributes[P2P_SLOT_INTENDED_INTERFACE_ADDRESS] == NULL ||
		    peer->attributes[P2P_SLOT_DEVICE_INFO] == NULL ||
		    peer->attributes[P2P_SLOT_OPERATING_CHANNEL] == NULL ||
		    peer->attributes[P2P_SLOT_GO_INTENT] == NULL)
			goto invalid;
	} else if (kind == P2P_GO_NEG_CONFIRM && peer != NULL) {
		if (peer->attributes[P2P_SLOT_CONFIGURATION_TIMEOUT] == NULL ||
		    peer->attributes[P2P_SLOT_CHANNEL_LIST] == NULL ||
		    peer->attributes[P2P_SLOT_STATUS] == NULL ||
		    peer->attributes[P2P_SLOT_CAPABILITY] == NULL ||
		    peer->attributes[P2P_SLOT_INTENDED_INTERFACE_ADDRESS] == NULL ||
		    peer->attributes[P2P_SLOT_DEVICE_INFO] == NULL ||
		    peer->attributes[P2P_SLOT_GO_INTENT] == NULL)
			goto invalid;
	}
decide:
	if (ps->p2pStatus.value == 0) {
		role = p2pGroupRoleDecide(peer, ps->groupOwnerIntentAttribute[3] >> 1);
		if (role == P2P_ROLE_FAILED) {
			ps->p2pStatus.value = 9;
		} else if (role == P2P_ROLE_UNKNOWN) {
			goto invalid;
		} else if (role == P2P_ROLE_CLIENT && peer->attributes[P2P_SLOT_CAPABILITY] != NULL &&
			   (peer->attributes[P2P_SLOT_CAPABILITY][4] & 0x02) && !(ps->p2pOptions & 0x02)) {
			/* the peer's group is persistent, ours is not */
			ps->p2pStatus.value = 2;
		} else if (ps->p2pStatus.value == 0) {
			p2pGroupRoleApply(connection, peer, (uint8_t)role);
		}
	}
	goto reply;
invalid:
	ps->p2pStatus.value = 4;
reply:
	if (peer->attributes[P2P_SLOT_GO_INTENT] != NULL && kind == P2P_GO_NEG_RESPONSE)
		/* the tie-breaker of a response is the opposite of the request's */
		ps->groupOwnerIntentAttribute[3] = (ps->groupOwnerIntentAttribute[3] & ~1) |
			!(peer->attributes[P2P_SLOT_GO_INTENT][3] & 1);
	response[32 + 7] = request[32 + 7];	/* the dialog token */
	if (concurrent) {
		if (ps->commonChannels.count != 0) {
			uint8_t only = ps->commonChannels.channels[0];

			ps->groupChannel = only;
			ps->requestedChannel = only;
		}
		peer->attributes[P2P_SLOT_CHANNEL_LIST] = (uint8_t *)&ps->commonChannels;
		given |= 1u << P2P_ATTR_CHANNEL_LIST;
	} else if (connection->mode == P2P_ROLE_GROUP_OWNER) {
		p2pOperatingChannelSelect(connection, peer);
	}
	if (ps->p2pStatus.value != 0) {
		connection->mode = 1;
		connection->p2pState = 3;
		macMgmtValidateTable(0);
	}
	if (connection->mode == P2P_ROLE_GROUP_OWNER) {
		p2pDirectProcessContext(P2P_ATTR_OPERATING_CHANNEL, ps->operatingChannelAttribute, operating);
		operating[6] = ps->commonChannels.operatingClass;
		operating[7] = ps->groupChannel;
		peer->attributes[P2P_SLOT_OPERATING_CHANNEL] = operating;
		given |= (1u << P2P_ATTR_OPERATING_CHANNEL) | (1u << P2P_ATTR_CHANNEL_LIST);
	}
	if (kind == P2P_GO_NEG_CONFIRM) {
		given = (1u << P2P_ATTR_OPERATING_CHANNEL) | (1u << P2P_ATTR_CHANNEL_LIST);
		include = (connection->mode == P2P_ROLE_GROUP_OWNER ? 0x8000 : 0) | 0x0005;
		goto check;
	}
	if (kind == P2P_GO_NEG_RESPONSE) {
		include = 0x2235;
		if (connection->mode == P2P_ROLE_GROUP_OWNER) {
			include = 0xa235;
			goto check;
		}
		if (!(given & (1u << P2P_ATTR_OPERATING_CHANNEL)))
			include = 0x22235;
		if (given & (1u << P2P_ATTR_CHANNEL_LIST))
			goto check;
		include |= 1u << P2P_ATTR_CHANNEL_LIST;
		goto check;
	}
	if (kind != P2P_GO_NEG_REQUEST)
		goto check;
attributes:
	include = 0x2374;
	if (!(given & (1u << P2P_ATTR_CHANNEL_LIST)))
		include = 0x2b74;
	if (!(given & (1u << P2P_ATTR_OPERATING_CHANNEL)))
		include |= 1u << P2P_ATTR_OPERATING_CHANNEL;
check:
	length = (uint16_t)p2pDirectCheckContext(connection, (WlanWirelessFrame *)response, peer, include, given);
	length += (response[1] << 8) | response[0];
	response[0] = length;
	response[1] = length >> 8;
	if (kind != P2P_GO_NEG_CONFIRM)
		p2pWpsConfigAppend(connection, response);
	return 0;
}

/*
 * Sends the GO negotiation request to the peer the host named, on the
 * peer's channel when it is known, and gives the answer 10 s.
 */
int p2pGoNegotiationRequestSend(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps;
	OsaBuffer *buffer;
	uint8_t *data;
	uint8_t flags[4];

	wlanCurrentChannelFlags(flags);
	ps = wlanConnectionPsState(connection);
	connection->p2pState = 2;
	macMgmtValidateTable(0);
	if (ps->p2pChannel != 255)
		wlanSetChannel(ps->p2pChannel, flags, 1);
	connection->p2pState = 4;
	if (p2pActionFrameAlloc(connection, &buffer, ps->eventPeerAddress, P2P_FRAME_PUBLIC_ACTION, 0) != 0)
		return -1;
	data = (uint8_t *)buffer + buffer->dataOffset;
	p2pGoNegotiationBuild(connection, NULL, data, P2P_GO_NEG_REQUEST);
	data[32 + 7] = ps->dialogToken;
	if (wlanMgmtSendFrame(buffer) != 0)
		return -1;
	if (ps->negotiationTimer != 0) {
		osaCancelTimer(ps->negotiationTimer);
		ps->negotiationTimer = 0;
	}
	osaCreateTimer(p2pListenTimerExpired, (uint32_t)connection, 10000000, &ps->negotiationTimer, 0);
	return 0;
}

/* Adds the WPS element of attribute 4 the host gave, with its value (bytes 4 and 5) set to configMethods for this frame only. */
void p2pWpsConfigMethodsAppend(struct WlanConnection *connection, uint8_t *frame, const uint8_t *configMethods)
{
	uint8_t *attribute = wlanConnectionPsState(connection)->wpsAttributes[WPS_SLOT_CONFIG_METHODS];
	uint8_t saved[2];

	if (attribute == NULL)
		return;
	saved[0] = attribute[4];
	saved[1] = attribute[5];
	attribute[4] = configMethods[0];
	attribute[5] = configMethods[1];
	p2pDirectConvertState(connection, (WlanWirelessFrame *)frame, 0x10);
	attribute[4] = saved[0];
	attribute[5] = saved[1];
}

/*
 * The P2P element of a provision discovery request (with the group's Group ID
 * when one is given) and its WPS element, whose config methods are
 * wpsConfigMethod.
 */
void p2pProvisionDiscoveryBuild(struct WlanConnection *connection, uint8_t *frame, uint8_t *groupId)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext attributes;
	struct WlanDirectContext *given;
	unsigned int exclude;
	unsigned int length;
	uint32_t configMethods;

	if (groupId != NULL) {
		attributes.attributes[P2P_SLOT_GROUP_ID] = groupId;
		exclude = 1u << P2P_ATTR_GROUP_ID;
		given = &attributes;
	} else {
		exclude = 0;
		given = NULL;
	}
	length = (uint16_t)p2pDirectCheckContext(connection, (WlanWirelessFrame *)frame, given, 0x2004, exclude);
	length += (frame[1] << 8) | frame[0];
	frame[0] = length;
	frame[1] = length >> 8;
	configMethods = (ps->wpsConfigMethod << 8) | ((ps->wpsConfigMethod >> 8) & 0xff);
	p2pWpsConfigMethodsAppend(connection, frame, (const uint8_t *)&configMethods);
}

/* Sends a provision discovery request; without an answer in 200 ms discovery goes on. */
int p2pProvisionDiscoveryRequestSend(struct WlanConnection *connection, const uint8_t *destination, uint8_t *groupId)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	OsaBuffer *buffer;
	uint8_t *data;

	if (p2pActionFrameAlloc(connection, &buffer, destination, P2P_FRAME_PUBLIC_ACTION, 7) == 0) {
		data = (uint8_t *)buffer + buffer->dataOffset;
		p2pProvisionDiscoveryBuild(connection, data, groupId);
		data[32 + 7] = ps->dialogToken;
		if (wlanMgmtSendFrame(buffer) == 0) {
			if (ps->provisionTimer != 0) {
				osaCancelTimer(ps->provisionTimer);
				ps->provisionTimer = 0;
			}
			osaCreateTimer(p2pBssTimerExpired, (uint32_t)connection, 200000, &ps->provisionTimer, 0);
			return 0;
		}
	}
	p2pDiscoveryResume(connection);
	return -1;
}

/*
 * The peer discovery waits for was heard (its frame names it in the Device
 * Info attribute): the radio stays on its channel, and the first of the
 * pending actions (p2pFlags) is done. Without an invitation pending, the
 * original copies the peer's address to address 0 and compares it there;
 * this is kept as it is.
 */
void p2pPeerFound(struct WlanConnection *connection, const uint8_t *frame, unsigned int pending)
{
	struct WlanPsPeerState *ps;
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;
	uint8_t groupIdAttribute[44];
	uint8_t deviceInfo[12];
	uint32_t invitation = (uint32_t)-1;
	unsigned int changed;
	uint8_t *address = NULL;

	ps = wlanConnectionPsState(p2pDeviceConnection);
	if (pending == 0)
		return;
	direct = p2pDirectScheduleContext(frame, 0, NULL, &changed, &attributes);
	if (changed & (1u << P2P_ATTR_DEVICE_INFO))
		p2pDirectSyncContext(direct->attributes[P2P_SLOT_DEVICE_INFO], deviceInfo, 9,
				     &direct->attributes[P2P_SLOT_DEVICE_INFO]);
	if (pending & 0x02)
		address = p2pSqramTrackContext(connection, &invitation);
	else
		__aeabi_memcpy(address, ps->eventPeerAddress, 6);
	if (direct->attributes[P2P_SLOT_DEVICE_INFO] == NULL ||
	    memcmp(direct->attributes[P2P_SLOT_DEVICE_INFO] + 3, address, 6) != 0)
		return;
	ps->p2pChannel = wlanOperatingChannel();
	ps->p2pBssActive = 1;
	if (pending & 0x08) {
		const uint8_t *ssid = frame + 44;
		uint8_t *groupId = NULL;

		/* a group owner is asked with its group's Group ID */
		if (direct != NULL && (direct->attributes[P2P_SLOT_CAPABILITY][4] & 0x01) &&
		    direct->attributes[P2P_SLOT_DEVICE_INFO] != NULL) {
			unsigned int length = ssid[1] + 6;

			groupIdAttribute[0] = P2P_ATTR_GROUP_ID;
			groupIdAttribute[1] = length;
			groupIdAttribute[2] = length >> 8;
			__aeabi_memcpy(groupIdAttribute + 3, direct->attributes[P2P_SLOT_DEVICE_INFO] + 3, 6);
			__aeabi_memcpy(groupIdAttribute + 9, ssid + 2, ssid[1]);
			groupId = groupIdAttribute;
		}
		p2pProvisionDiscoveryRequestSend(p2pDeviceConnection, ps->eventPeerAddress, groupId);
		ps->p2pFlags &= ~0x08;
	} else if (pending & 0x01) {
		macMgmtAdjustEntry(connection);
		p2pGoNegotiationRequestSend(connection);
		ps->p2pFlags &= ~0x01;
	} else if (pending & 0x10) {
		p2pPendingFrameSend(connection);
		ps->p2pFlags &= ~0x10;
	} else if (pending & 0x04) {
		p2pDiscoverabilityCheck(direct, changed, ps);
	} else if ((pending & 0x02) && address != NULL) {
		ps->groupSettingD = invitation;
		p2pSqramReleaseContext(connection, invitation, address, direct);
		ps->p2pFlags &= ~0x02;
	}
}

/*
 * A probe request to a P2P device in discovery or to a group owner: 802.11b
 * only probes are ignored; a device answers probes for its Device ID (or any)
 * sent to the wildcard BSSID, with a "DIRECT-" SSID (while a station or an
 * access point is up, the wildcard SSID too), and the probe of the peer a GO
 * negotiation or an invitation waits for starts it. The answer is the probe
 * response template.
 */
void p2pProbeRequestAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps;
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;
	const uint8_t *rates;
	const uint8_t *extendedRates;
	const uint8_t *source;
	const uint8_t *ssid = frame + 32;
	uint8_t deviceId[12];
	unsigned int first = 0;
	unsigned int changed = 0;
	unsigned int length = (frame[1] << 8) | frame[0];
	OsaBuffer *buffer;
	uint8_t *data;
	uint8_t *body;
	const uint8_t *template;

	ps = wlanConnectionPsState(connection);
	if (connection->p2pState != 2)
		return;
	rates = osaFindTlv(frame + 32, 1, length);
	extendedRates = osaFindTlv(frame + 32, 50, (frame[1] << 8) | frame[0]);
	if (rates == NULL || wlanRatesHaveOfdm(rates + 2, rates[1]) == 0) {
		if (extendedRates == NULL || wlanRatesHaveOfdm(extendedRates + 2, extendedRates[1]) == 0)
			return;
	}
	direct = p2pDirectScheduleContext(frame, 0, &first, &changed, &attributes);
	source = frame + 12;
	if (connection->mode == 1) {
		struct WlanConnection *station;
		struct WlanConnection *accessPoint;

		if (direct == NULL)
			return;
		if (changed & (1u << P2P_ATTR_DEVICE_ID))
			p2pDirectSyncContext(direct->attributes[P2P_SLOT_DEVICE_ID], deviceId, 9,
					     &direct->attributes[P2P_SLOT_DEVICE_ID]);
		if (direct->attributes[P2P_SLOT_DEVICE_ID] != NULL &&
		    memcmp(direct->attributes[P2P_SLOT_DEVICE_ID] + 3, ps->interfaceAddress, 6) != 0)
			return;
		if (memcmp(frame + 18, p2pBroadcastAddress, 6) != 0)
			return;
		if (memcmp(frame + 6, p2pBroadcastAddress, 6) != 0 && memcmp(frame + 6, ps->interfaceAddress, 6) != 0)
			return;
		station = wlanConnectionFirst(0);
		accessPoint = wlanConnectionFirst(2);
		if ((station != NULL && (station->flags2 & WLAN_CONN2_ASSOCIATED)) ||
		    wlanConnectionBssStarted(accessPoint) != 0) {
			if (memcmp(ssid + 2, "DIRECT-", 7) != 0 && ssid[1] != 0)
				return;
		} else {
			if (ssid[1] == 0 || memcmp(ssid + 2, "DIRECT-", 7) != 0)
				return;
		}
		if (ps->p2pFlags & 0x01) {
			if (memcmp(source, ps->eventPeerAddress, 6) == 0) {
				p2pGoNegotiationRequestSend(connection);
				ps->p2pFlags &= ~0x01;
			}
			if (ps->p2pFlags & 0x01)
				goto negotiating;
		}
		if (ps->p2pFlags & 0x02)
			p2pPeerFound(connection, frame, 0x02);
negotiating:
		if (connection->p2pState == 4)
			return;
	} else if (connection->mode == 2) {
		struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

		if (ssid[1] != 0 && memcmp(ssid + 2, "DIRECT-", 7) != 0 &&
		    memcmp(ssid + 2, info->ssid, info->ssidLength) != 0)
			return;
	}

	buffer = wlanMgmtAllocFrame(connection, 700);
	if (buffer == NULL || (data = (uint8_t *)buffer + buffer->dataOffset) == NULL) {
		p2pFrameDataMissing++;
		return;
	}
	buffer->context = connection;
	wlanMgmtBuildFrame(buffer, connection->interfaceAddress, source, 5, connection->localAddress);
	buffer->dataOffset = 176;
	template = (const uint8_t *)connection->probeResponseBuffer + 176;
	body = data + 32;
	__aeabi_memcpy(body, template + 32, (template[1] << 8) | template[0]);
	data[0] = template[0];
	data[1] = ((template[1] << 8) | template[0]) >> 8;
	if ((connection->p2pState == 2 || connection->p2pState == 3) && connection->mode == 1)
		body[10] &= ~0x03;	/* a device is neither ESS nor IBSS */
	if (direct != NULL)
		p2pDirectPrepareContext(connection, (WlanWirelessFrame *)data);
	macMgmtSelectPacket(connection, buffer);
	if (wlanMgmtSendFrame(buffer) == 0 && ps->groupSettingA != 0 && connection->mode == 1)
		p2pEventSend(connection, frame, WLAN_HOST_EVENT_WIFIDIRECT_GENERIC, 14, 0);
}

/*
 * The scan slot of a P2P search starts its listen phase: the device listens on
 * its listen channel for the extended listen window (or the default listen
 * time), unless it was listening already or the channel is the one a station
 * or an access point is on, when the slot ends at once.
 */
int p2pListenSlotStart(struct WlanConnection *connection, uint32_t slot)
{
	struct WlanConnection *device = wlanConnectionFirst(3);
	struct WlanPsPeerState *ps = wlanConnectionPsState(device);
	struct WlanConnection *station = wlanConnectionFirst(0);
	struct WlanConnection *accessPoint = wlanConnectionFirst(2);
	uint32_t channel = 0;
	uint32_t flags;
	unsigned int duration;

	if (station != NULL && (station->flags2 & WLAN_CONN2_ASSOCIATED))
		macMgmtPrepareContext((uint8_t *)&flags, &channel);
	if (wlanConnectionBssStarted(accessPoint) == 1)
		channel = wlanConnectionInfoSlot(accessPoint)->channel;
	if (device->p2pState == 2 || ps->defaultChannel == channel) {
		p2pConnectionStateSet(connection, 2);
		return wlanSlotAbort(slot, 6);
	}
	p2pConnectionStateSet(connection, 2);
	if (ps->p2pWindowValid)
		duration = 1000 * (ps->p2pWindowEnd - ps->p2pWindowStart);
	else
		duration = 1000 * p2pDirectFormatContext(device);
	wlanCurrentChannelFlags((uint8_t *)&flags);
	wlanSetChannel(ps->defaultChannel, (uint8_t *)&flags, 1);
	wlanSlotSetChannel(flags, ps->defaultChannel);
	return wlanSlotSetDuration(slot, duration);
}

/* The group shares the channel of an infrastructure connection or an access point. */
int p2pChannelShared(struct WlanConnection *connection)
{
	return wlanConnectionPsState(connection)->channelShared;
}

/* The device discoverability response: the request's dialog token and the status. */
void p2pDiscoverabilityResponseBuild(struct WlanConnection *connection, uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	unsigned int length;

	frame[32 + 7] = ps->discoverabilityDialogToken;
	length = p2pDirectConfigureContext(p2pDirectFilterState(frame), (const uint8_t *)&ps->p2pStatus) +
		 ((frame[1] << 8) | frame[0]);
	frame[0] = length;
	frame[1] = length >> 8;
}

/*
 * A device discoverability request to the group owner: a client of the group
 * that can be discovered gets a GO discoverability request, and the response
 * waits for it; for any other the response says the device is unknown. A
 * client that sees its host power save end leaves host mode.
 */
int p2pDiscoverabilityRequestAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;
	const uint8_t *body;
	const uint8_t *clients;
	uint8_t deviceId[12];
	uint8_t groupInfo[716];
	unsigned int first = 0;
	unsigned int changed = 0;
	unsigned int offset = 0;
	OsaBuffer *request;
	OsaBuffer *response;

	if (connection->mode != 2) {
		if (connection->mode == 3 && wlanSessionId != 0) {
			wlanPsCancelHostModeTimer();
			wlanPsHostModeExit();
		}
		return 0;
	}
	body = frame + 32;
	if (body[0] == 4) {
		direct = p2pDirectScheduleContext(frame, 1, &first, &changed, &attributes);
		if (direct->attributes[P2P_SLOT_DEVICE_ID] == NULL)
			return -1;
		if (changed & (1u << P2P_ATTR_DEVICE_ID))
			p2pDirectSyncContext(direct->attributes[P2P_SLOT_DEVICE_ID], deviceId, 9,
					     &direct->attributes[P2P_SLOT_DEVICE_ID]);
		if (changed & (1u << P2P_ATTR_GROUP_INFO))
			p2pDirectSyncContext(direct->attributes[P2P_SLOT_GROUP_INFO], groupInfo, -1,
					     &direct->attributes[P2P_SLOT_GROUP_INFO]);
		__aeabi_memcpy(ps->discoverabilityRequester, frame + 12, 6);
		ps->discoverabilityDialogToken = body[7];
		ps->p2pStatus.value = 8;	/* unknown P2P group */
		clients = ps->groupInfoAttribute.clients;
		while ((unsigned int)((ps->groupInfoAttribute.length[1] << 8) | ps->groupInfoAttribute.length[0]) > offset + 6) {
			const struct WlanP2pClient *client = (const struct WlanP2pClient *)(clients + offset);

			if (memcmp(client->deviceAddress, direct->attributes[P2P_SLOT_DEVICE_ID] + 3, 6) == 0) {
				if (!(client->capability & 0x02)) {
					ps->p2pStatus.value = 8;
					return 0;
				}
				ps->clientDiscoverabilityPending = 1;
				ps->p2pStatus.value = 0;
				if (p2pActionFrameAlloc(connection, &request, client->interfaceAddress, P2P_FRAME_ACTION, 3) != 0)
					return -1;
				((uint8_t *)request + request->dataOffset)[32 + 7] = 0;
				if (wlanMgmtSendFrame(request) != 0) {
					ps->p2pStatus.value = 6;
					return -1;
				}
				return 0;
			}
			offset = (uint16_t)(clients[offset] + offset + 1);
		}
	}
	if (p2pActionFrameAlloc(connection, &response, ps->discoverabilityRequester, P2P_FRAME_PUBLIC_ACTION, 6) != 0)
		return -1;
	p2pDiscoverabilityResponseBuild(connection, (uint8_t *)response + response->dataOffset);
	if (wlanMgmtSendFrame(response) != 0)
		return -1;
	return 0;
}

/* A device discoverability response: 0 when its P2P Status attribute says success. */
int p2pDiscoverabilityResponseCheck(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;

	wlanConnectionPsState(connection);
	direct = p2pDirectScheduleContext(frame, 1, NULL, NULL, &attributes);
	if (direct == NULL || direct->attributes[P2P_SLOT_STATUS] == NULL ||
	    direct->attributes[P2P_SLOT_STATUS][3] != 0)
		return -1;
	return 0;
}

/*
 * A GO negotiation request to a device: the host hears of it, the response
 * goes back, and on success the device waits for the confirmation (ten
 * seconds); a failure status is reported to the host with the peer's address.
 */
int p2pGoNegotiationRequestAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint16_t peer[16];
	OsaBuffer *response;
	const uint8_t *source;
	uint8_t *data;

	if (connection->mode != 1)
		return 0;
	source = frame + 12;
	p2pEventSendPeer(connection, frame, 0, 3);
	if (p2pActionFrameAlloc(connection, &response, source, P2P_FRAME_PUBLIC_ACTION, P2P_GO_NEG_RESPONSE) != 0)
		return -1;
	data = (uint8_t *)response + response->dataOffset;
	connection->p2pState = 2;
	macMgmtValidateTable(0);
	if (p2pGoNegotiationBuild(connection, frame, data, P2P_GO_NEG_RESPONSE) == 0) {
		if (ps->p2pStatus.value == 0)
			connection->p2pState = 4;
		if (wlanMgmtSendFrame(response) != 0)
			return -1;
		if (connection->p2pState == 4) {
			if (ps->negotiationTimer != 0) {
				osaCancelTimer(ps->negotiationTimer);
				ps->negotiationTimer = 0;
			}
			osaCreateTimer(p2pListenTimerExpired, (uint32_t)connection, 10000000, &ps->negotiationTimer, 0);
			return 0;
		}
		peer[0] = 32;
		__aeabi_memcpy((uint8_t *)peer + 12, source, 6);
		p2pEventSendPeer(connection, (const uint8_t *)peer, 9, ps->p2pStatus.value);
	}
	connection->p2pState = 3;
	macMgmtValidateTable(0);
	return -1;
}

/*
 * The GO negotiation confirmation: a failure status ends the negotiation;
 * otherwise the group takes the peer's channel and the host hears that the
 * group formed, as its owner or as a client.
 */
int p2pGoNegotiationConfirmed(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;
	uint8_t status[4];
	unsigned int first = 0;
	unsigned int changed = 0;
	unsigned int role;

	if (ps->negotiationTimer != 0) {
		osaCancelTimer(ps->negotiationTimer);
		ps->negotiationTimer = 0;
	}
	direct = p2pDirectScheduleContext(frame, 1, &first, &changed, &attributes);
	if (direct != NULL && direct->attributes[P2P_SLOT_STATUS] != NULL) {
		if (changed & (1u << P2P_ATTR_STATUS))
			p2pDirectSyncContext(direct->attributes[P2P_SLOT_STATUS], status, 4,
					     &direct->attributes[P2P_SLOT_STATUS]);
		if (direct->attributes[P2P_SLOT_STATUS][3] != 0) {
			connection->p2pState = 3;
			macMgmtValidateTable(0);
			return -1;
		}
	}
	if (direct->attributes[P2P_SLOT_LISTEN_CHANNEL] != NULL)
		ps->groupChannel = direct->attributes[P2P_SLOT_LISTEN_CHANNEL][7];
	connection->p2pState = 2;
	if (connection->mode == 2)
		role = 1;
	else if (connection->mode == 3)
		role = 2;
	else
		role = 0;
	p2pEventSendPeer(connection, frame, 2, role);
	return 0;
}

/* The connection's P2P state, mode and channel, for when it goes back. */
void p2pDeviceStateSave(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	ps->savedP2pState = connection->p2pState;
	ps->savedMode = connection->mode;
	if (connection->mode == 2 || connection->mode == 3)
		ps->savedChannel = ps->groupChannel;
	else
		ps->savedChannel = ps->defaultChannel;
}

/* Back to what p2pDeviceStateSave() kept: the group owner or client starts again, a device stops. */
void p2pDeviceStateRestore(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	connection->p2pState = ps->savedP2pState;
	connection->mode = ps->savedMode;
	ps->groupChannel = ps->savedChannel;
	if (connection->mode == 2) {
		p2pDirectCheckSsidPrefix(connection);
		if (ps->groupSettingB == 0)
			macMgmtFormatPacket(connection);
		return;
	}
	if (connection->mode == 3) {
		p2pDirectSetupState(connection);
	} else {
		ps->p2pBssActive = 0;
		connection->mode = 1;
		connection->p2pState = 3;
	}
	macMgmtValidateTable(0);
}

/*
 * The GO negotiation response to our request: the confirmation goes back. A
 * peer whose information is not available yet makes the device listen on its
 * channel for two minutes; a failure status is reported to the host.
 */
int p2pGoNegotiationResponseAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct WlanDirectContext attributes;
	struct WlanDirectContext *direct;
	uint32_t flags;
	unsigned int first = 0;
	unsigned int changed = 0;
	unsigned int type;
	OsaBuffer *confirm;

	if (ps->negotiationTimer != 0) {
		osaCancelTimer(ps->negotiationTimer);
		ps->negotiationTimer = 0;
	}
	direct = p2pDirectScheduleContext(frame, 1, &first, &changed, &attributes);
	if (direct == NULL || direct->attributes[P2P_SLOT_STATUS] == NULL)
		return -1;
	if (direct->attributes[P2P_SLOT_STATUS][3] == 1) {	/* information currently unavailable */
		connection->mode = 1;
		connection->p2pState = 2;
		wlanCurrentChannelFlags((uint8_t *)&flags);
		wlanSetChannel(ps->defaultChannel, (uint8_t *)&flags, 1);
		ps->p2pChannel = 255;
		osaCreateTimer(p2pListenTimerExpired, (uint32_t)connection, 120000000, &ps->negotiationTimer, 0);
	}
	if (direct->attributes[P2P_SLOT_STATUS][3] != 0) {
		type = 1;
		goto report;
	}
	if (p2pActionFrameAlloc(connection, &confirm, frame + 12, P2P_FRAME_PUBLIC_ACTION, P2P_GO_NEG_CONFIRM) != 0)
		return -1;
	if (p2pGoNegotiationBuild(connection, frame, (uint8_t *)confirm + confirm->dataOffset, P2P_GO_NEG_CONFIRM) != 0) {
		type = 1;
		goto report;
	}
	if (wlanMgmtSendFrame(confirm) != 0) {
		ps->p2pStatus.value = 6;
		return -1;
	}
	if (ps->p2pStatus.value == 0)
		connection->p2pState = 4;
	p2pEventSendPeer(connection, frame, 1, 3);
	if (ps->p2pStatus.value == 0)
		return 0;
	type = 2;
report:
	p2pEventSendPeer(connection, frame, type, 0);
	return -1;
}

/*
 * A provision discovery request: the response offers the config method both
 * sides support (none when there is none), which with its device password ID
 * provisioning then uses, and the host hears of it.
 */
int p2pProvisionDiscoveryRequestAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *wps[WPS_SLOT_COUNT];
	const uint8_t *element;
	uint8_t *data;
	OsaBuffer *response;
	union {
		uint32_t word;
		uint16_t methods;
	} selected;
	uint16_t ours = 0;
	uint16_t peer = 0;
	int left;
	unsigned int size;

	if (p2pActionFrameAlloc(connection, &response, frame + 12, P2P_FRAME_PUBLIC_ACTION, 8) != 0)
		goto failed;
	element = frame + 40;
	data = (uint8_t *)response + response->dataOffset;
	data[32 + 7] = frame[39];
	left = ((frame[1] << 8) | frame[0]) - 8;
	__aeabi_memclr4(wps, sizeof(wps));
	do {
		size = (uint8_t)(element[1] + 2);
		if (memcmp(element + 2, wlanWpsOui, 4) == 0)
			wpsAttributesParse(element + 6, (uint16_t)(size - 6), wps);
		left -= size;
		element += size;
	} while (left > 0 && element != NULL);
	if (wps[WPS_SLOT_CONFIG_METHODS] != NULL) {
		((uint8_t *)&peer)[0] = wps[WPS_SLOT_CONFIG_METHODS][4];
		((uint8_t *)&peer)[1] = wps[WPS_SLOT_CONFIG_METHODS][5];
		if (ps->wpsAttributes[WPS_SLOT_CONFIG_METHODS] != NULL) {
			((uint8_t *)&ours)[0] = ps->wpsAttributes[WPS_SLOT_CONFIG_METHODS][4];
			((uint8_t *)&ours)[1] = ps->wpsAttributes[WPS_SLOT_CONFIG_METHODS][5];
		}
		if (peer & ours) {
			unsigned int method;

			selected.word = peer;
			method = (uint16_t)((peer << 8) | (peer >> 8));
			ps->wpsConfigMethod = method;
			if (method == 0x0004)
				ps->wpsDevicePasswordId = 0;
			else if (method == 0x0008)		/* display */
				ps->wpsDevicePasswordId = 5;
			else if (method == 0x0080)		/* push button */
				ps->wpsDevicePasswordId = 4;
			else if (method == 0x0100)		/* keypad */
				ps->wpsDevicePasswordId = 1;
			else
				ps->wpsDevicePasswordId = 0;
			goto respond;
		}
	}
	selected.word = 0;
respond:
	p2pWpsConfigMethodsAppend(connection, data, (const uint8_t *)&selected.word);
	if (wlanMgmtSendFrame(response) != 0)
		goto failed;
	if (connection->mode != 2)
		p2pDiscoveryResume(connection);
	p2pEventSendPeer(connection, frame, 7, (uint16_t)((selected.methods << 8) | (selected.methods >> 8)));
	return 0;

failed:
	if (connection->mode != 2)
		p2pDiscoveryResume(connection);
	return -1;
}

/*
 * The provision discovery response to our request: provisioning ends, the
 * device searches again, and the host hears the config method the peer took.
 */
int p2pProvisionDiscoveryResponseAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *wps[WPS_SLOT_COUNT];
	const uint8_t *element = frame + 40;
	union {
		uint32_t word;
		uint16_t methods;
		uint8_t bytes[4];
	} selected;
	int left;
	unsigned int size;

	selected.word = 0;
	left = ((frame[1] << 8) | frame[0]) - 8;
	__aeabi_memclr4(wps, sizeof(wps));
	do {
		size = (uint8_t)(element[1] + 2);
		if (memcmp(element + 2, wlanWpsOui, 4) == 0)
			wpsAttributesParse(element + 6, (uint16_t)(size - 6), wps);
		left -= size;
		element += size;
	} while (left > 0 && element != NULL);
	if (wps[WPS_SLOT_CONFIG_METHODS] != NULL) {
		selected.bytes[0] = wps[WPS_SLOT_CONFIG_METHODS][4];
		selected.bytes[1] = wps[WPS_SLOT_CONFIG_METHODS][5];
	}
	if (ps->provisionTimer != 0) {
		osaCancelTimer(ps->provisionTimer);
		ps->provisionTimer = 0;
	}
	ps->p2pBssActive = 0;
	p2pDiscoveryResume(connection);
	p2pEventSendPeer(connection, frame, 8, (uint16_t)((selected.methods << 8) | (selected.methods >> 8)));
	return 0;
}

/* The presence response: success, and the Notice of Absence (a bare one without absence periods). */
unsigned int p2pPresenceResponseBuild(struct WlanConnection *connection, uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	struct {
		uint8_t id;
		uint8_t length[2];
		uint8_t index;
		uint8_t ctWindow : 7;
		uint8_t oppPs : 1;
		uint8_t descriptors[27];
	} noa;
	uint8_t *element = p2pDirectFilterState(frame);
	unsigned int length;

	ps->p2pStatus.value = 0;
	length = (uint16_t)p2pDirectConfigureContext(element, (const uint8_t *)&ps->p2pStatus);
	if (!(ps->noaMode & NOA_MODE_ABSENCE)) {
		noa.id = P2P_ATTR_NOTICE_OF_ABSENCE;
		noa.length[0] = 2;
		noa.length[1] = 0;
		noa.index = 0;
		noa.ctWindow = 0;
		noa.oppPs = 0;
	}
	return (uint16_t)(p2pDirectConfigureContext(element, (const uint8_t *)&noa) + length);
}

/*
 * A P2P action frame (presence request or response) to the peer: sent now, or
 * kept for a timer while the Notice of Absence has the device absent.
 */
int p2pVendorActionSend(struct WlanConnection *connection, const uint8_t *destination, unsigned int kind,
			unsigned int dialogToken, unsigned int unused, unsigned int subtype)
{
	struct WlanPsPeerState *ps;
	OsaBuffer *frame;
	uint8_t *data;
	uintptr_t delay = 0;
	uint32_t timer;
	unsigned int length = 0;

	(void)unused;
	ps = wlanConnectionPsState(connection);
	if (p2pActionFrameAlloc(connection, &frame, destination, (uint8_t)kind, (uint8_t)subtype) != 0)
		return -1;
	data = (uint8_t *)frame + frame->dataOffset;
	if (subtype == 0)
		;
	else if (subtype == 1)
		length = (uint16_t)p2pDirectConfigureContext(p2pDirectFilterState(data), ps->presenceNoaAttribute);
	else if (subtype == 2)
		length = p2pPresenceResponseBuild(connection, data);
	data[32 + 6] = dialogToken;
	length += (data[1] << 8) | data[0];
	data[0] = length;
	data[1] = length >> 8;
	if (wlanDirectLinkPoll(connection, &delay) == 0) {
		osaCreateTimer(p2pPendingFrameTimerExpired, (uint32_t)connection, delay, &timer, 0);
		ps->pendingFrame = frame;
		return 0;
	}
	if (wlanMgmtSendFrame(frame) != 0)
		return -1;
	return 0;
}

/* A presence request: it is answered, and the opportunistic power save stops. */
int p2pPresenceRequestAnswer(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	unsigned int dialogToken = frame[32 + 6];

	if (frame[32 + 5] == 1) {
		wlanConnectionInfo(connection);
		if (frame[40] > 2) {
			if (ps->noaMode & NOA_MODE_CTWINDOW)
				ps->noaMode &= ~NOA_MODE_CTWINDOW;
			p2pVendorActionSend(connection, frame + 12, P2P_FRAME_ACTION, dialogToken, 9, 2);
			ps->p2pEntryPending = 1;
		}
	}
	return 0;
}

/*
 * A P2P public action (kind 9) or P2P action frame (kind 127) for the P2P
 * interface. A group owner takes only frames to its interface (or to the
 * group, from its clients). While the radio is away from the group's channel
 * the frame is kept, and comes back here once the channel is back (no frame);
 * otherwise it goes to the handler of its subtype.
 */
int p2pActionFrameReceive(struct WlanConnection *connection, const uint8_t *frame, unsigned int kind)
{
	struct WlanPsPeerState *ps;
	uint32_t flags;
	int result;

	if (connection == NULL) {
		connection = wlanConnectionFirst(3);
		if (connection == NULL)
			return -1;
	} else if (connection->interfaceType != 3) {
		return -1;
	}
	ps = wlanConnectionPsState(connection);
	if (connection->mode == 2) {
		struct WlanConnection *peer = wlanConnectionFindByPeer(frame + 12);
		unsigned int state = 0;

		if (peer != NULL) {
			wlanConnectionInfo(peer);
			state = wlanConnectionInfoSlot(peer)->state;
		}
		if (state == 3) {
			if (memcmp(ps->groupAddress, frame + 6, 6) != 0)
				return -1;
		} else if (memcmp(ps->interfaceAddress, frame + 6, 6) != 0) {
			return -1;
		}
	}
	wlanCurrentChannelFlags((uint8_t *)&flags);
	if (connection->smState == connection->smStates + WLAN_SM_STATE_NO_PROBES) {
		ps->p2pBssActive = 1;
		macMgmtAdjustEntry(connection);
		connection->p2pState = 2;
		macMgmtValidateTable((connection->flags2 & WLAN_CONN2_ASSOCIATED) ? 1 : 0);
		ps->p2pChannel = wlanOperatingChannel();
		ps->deferredFrame = wlanTxBufferAlloc(&wlanBlockPool28, 2000, 0xffffffff, 3);
		if (ps->deferredFrame == NULL) {
			ps->p2pBssActive = 0;
			ps->p2pChannel = 255;
			connection->p2pState = 3;
			macMgmtValidateTable(0);
		} else {
			__aeabi_memcpy((uint8_t *)ps->deferredFrame + ps->deferredFrame->dataOffset, frame, 1000);
			macMgmtNotifyTable();
		}
		return 0;
	}
	if (ps->p2pBssActive != 0 && ps->deferredFrame != NULL) {
		frame = (const uint8_t *)ps->deferredFrame + ps->deferredFrame->dataOffset;
		ps->deferredFrame = NULL;
		if (frame[32] == WLAN_ACTION_CATEGORY_PUBLIC)
			goto public_action;
		if (frame[32] == P2P_FRAME_ACTION)
			goto action;
	}
	if (kind != P2P_FRAME_PUBLIC_ACTION) {
		if (kind != P2P_FRAME_ACTION || frame[32] != P2P_FRAME_ACTION)
			return 0;
		goto action;
	}
public_action:
	switch (frame[32 + 6]) {
	case 0:		/* GO negotiation request */
		if (ps->p2pChannel != 255)
			wlanSetChannel(ps->p2pChannel, (uint8_t *)&flags, 1);
		p2pGoNegotiationRequestAnswer(connection, frame);
		ps->p2pBssActive = 0;
		return 0;
	case 1:		/* GO negotiation response */
		if (connection->p2pState != 4)
			return 0;
		p2pGoNegotiationResponseAnswer(connection, frame);
		ps->p2pBssActive = 0;
		return 0;
	case 2:		/* GO negotiation confirmation */
		if (connection->p2pState != 4)
			return 0;
		result = p2pGoNegotiationConfirmed(connection, frame);
		ps->p2pBssActive = 0;
		return result;
	case 3:		/* invitation request */
		result = p2pSqramRecordContext(connection, frame);
		ps->p2pBssActive = 0;
		return result;
	case 4:		/* invitation response */
		p2pSqramFilterContext(connection, frame, ps->groupSettingD);
		ps->p2pBssActive = 0;
		return 0;
	case 5:		/* device discoverability request */
		return p2pDiscoverabilityRequestAnswer(connection, frame);
	case 6:		/* device discoverability response */
		result = p2pDiscoverabilityResponseCheck(connection, frame);
		ps->p2pBssActive = 0;
		return result;
	case 7:		/* provision discovery request */
		return p2pProvisionDiscoveryRequestAnswer(connection, frame);
	case 8:		/* provision discovery response */
		return p2pProvisionDiscoveryResponseAnswer(connection, frame);
	default:
		return 0;
	}
action:
	switch (frame[32 + 5]) {
	case 1:		/* presence request */
	case 2:		/* presence response */
		p2pPresenceRequestAnswer(connection, frame);
		break;
	case 3:		/* GO discoverability request */
		return p2pDiscoverabilityRequestAnswer(connection, frame);
	}
	return 0;
}

/* A received action frame: public actions and P2P actions go to p2pActionFrameReceive(). */
int p2pActionFrameRoute(struct WlanConnection *connection, const uint8_t *frame)
{
	unsigned int category = frame[32];

	if (category == WLAN_ACTION_CATEGORY_PUBLIC || category == P2P_FRAME_PUBLIC_ACTION)
		category = P2P_FRAME_PUBLIC_ACTION;
	else if (category != P2P_FRAME_ACTION)
		return 1;
	p2pActionFrameReceive(connection, frame, category);
	return 1;
}

/*
 * A P2P public action frame went out: a GO negotiation frame waits ten seconds
 * for the next one, an invitation 120 ms, and after a device discoverability
 * request the device searches again.
 */
void p2pPublicActionSent(struct WlanConnection *connection, unsigned int subtype)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	OsaTimerCallback callback;
	uint32_t *timer;
	uint32_t delay;

	switch (subtype) {
	case 0:
	case 1:
	case 2:
		if (ps->negotiationTimer != 0) {
			osaCancelTimer(ps->negotiationTimer);
			ps->negotiationTimer = 0;
		}
		callback = p2pListenTimerExpired;
		delay = 10000000;
		timer = &ps->negotiationTimer;
		break;
	case 3:
	case 4:
		if (ps->invitationTimer != 0) {
			osaCancelTimer(ps->invitationTimer);
			ps->invitationTimer = 0;
		}
		callback = p2pSqramScheduleContext;
		delay = 120000;
		timer = &ps->invitationTimer;
		break;
	case 5:
		connection->mode = 1;
		p2pDiscoveryResume(connection);
		return;
	default:
		return;
	}
	osaCreateTimer(callback, (uint32_t)connection, delay, timer, 0);
}

/*
 * A P2P action frame went unanswered: a GO discoverability request to a client
 * makes the device discoverability response report the failure.
 */
void p2pActionSent(struct WlanConnection *connection, unsigned int subtype)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	switch (subtype) {
	case 0:
	case 1:
	case 2:
		break;
	case 3:
		ps->p2pStatus.value = 6;
		p2pDiscoverabilityRequestAnswer(connection, NULL);
		break;
	}
}

/* The peer acknowledged our GO negotiation confirmation: the group formed. */
void p2pConfirmationAcked(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint16_t peer[16];
	unsigned int role;

	if (frame[32 + 6] != P2P_GO_NEG_CONFIRM)
		return;
	if (ps->negotiationTimer != 0) {
		osaCancelTimer(ps->negotiationTimer);
		ps->negotiationTimer = 0;
	}
	peer[0] = 32;
	__aeabi_memcpy((uint8_t *)peer + 12, frame + 6, 6);
	if (connection->mode == 2)
		role = 1;
	else if (connection->mode == 3)
		role = 2;
	else
		role = 0;
	p2pEventSendPeer(connection, (const uint8_t *)peer, 2, role);
}

/*
 * The P2P interface a P2P public action or P2P action frame belongs to, if it
 * is up; forUs is set when the frame's receiver is the interface, the
 * broadcast address or the device.
 */
struct WlanConnection *p2pActionFrameConnection(const OsaBuffer *buffer, uint32_t *forUs)
{
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	const uint8_t *oui;
	struct WlanConnection *connection;
	struct WlanPsPeerState *ps;

	if (frame[32] == WLAN_ACTION_CATEGORY_PUBLIC) {
		if (frame[33] != 9)	/* vendor specific */
			return NULL;
		oui = frame + 34;
	} else if (frame[32] == P2P_FRAME_ACTION) {
		oui = frame + 33;
	} else {
		return NULL;
	}
	if (memcmp(oui, wlanP2pOui, 4) != 0)
		return NULL;
	connection = wlanConnectionFirst(3);
	if (connection == NULL || connection->mode == 0)
		return NULL;
	ps = wlanConnectionPsState(connection);
	if ((frame[2] >> 4) == 13) {	/* an action frame */
		if (memcmp(connection->interfaceAddress, frame + 6, 6) == 0 ||
		    memcmp(wlanScanAnyBssid, frame + 6, 6) == 0 ||
		    (ps != NULL && memcmp(ps->interfaceAddress, frame + 6, 6) == 0))
			*forUs = 1;
	}
	return connection;
}

/* An address in two words (the BSSID p2pServiceDiscoverySend() puts in its frame). */
union WlanBssidWords {
	uint8_t bytes[8];
	uint32_t words[2];
};

static const union WlanBssidWords p2pWildcardBssid = {
	.bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0, 0 },
};

/*
 * A service discovery (GAS) frame the host gives: the peer's address and the
 * public action body. A device sends an initial request after the peer's next
 * probe, and its initial response at once; 1: no frame, 2: not GAS or not sent.
 */
int p2pServiceDiscoverySend(struct WlanConnection *connection, const uint8_t *request, unsigned int length)
{
	struct WlanPsPeerState *ps;
	const uint8_t *body = request + 6;
	union WlanBssidWords bssid = p2pWildcardBssid;
	const uint8_t *address = NULL;
	unsigned int now = 0;
	OsaBuffer *frame;
	uint8_t *data;

	ps = wlanConnectionPsState(connection);
	__aeabi_memcpy(ps->eventPeerAddress, request, 6);
	frame = wlanMgmtAllocFrame(connection, 400);
	if (frame == NULL)
		return 1;
	frame->context = connection;
	data = (uint8_t *)frame + frame->dataOffset;
	if (data == NULL)
		return 1;
	__aeabi_memclr(data, 400);
	if (connection->mode == 2) {
		const union WlanBssidWords *local = (const void *)connection->localAddress;

		bssid = *local;	/* the 6 bytes and the 2 after them */
	} else if (connection->mode == 3) {
		address = request;
	} else if (connection->mode == 1) {
		if (body[1] == 10) {		/* GAS initial request */
			address = request;
		} else if (body[1] == 11) {	/* GAS initial response */
			now = 1;
			address = ps->interfaceAddress;
		}
	}
	if (address != NULL)
		__aeabi_memcpy(bssid.bytes, address, 6);
	wlanMgmtBuildFrame(frame, connection->interfaceAddress, request, 13, bssid.bytes);
	if (body[1] != 10 && body[1] != 11)
		return 2;
	__aeabi_memcpy(data + 32, body, length - 6);
	length = ((data[1] << 8) | data[0]) + length - 6;
	data[0] = length;
	data[1] = length >> 8;
	if (now == 0) {
		ps->pendingFrame = frame;
		ps->p2pFlags |= 0x01;
		return 0;
	}
	if (wlanMgmtSendFrame(frame) != 0)
		return 2;
	return 0;
}

/*
 * A management frame of the P2P interface went out. Acknowledged: a GO
 * discoverability request makes the device discoverability response report
 * success, an empty GAS initial response lets the device search again, and a
 * GO negotiation confirmation forms the group; else the frame's sent handlers.
 */
void p2pFrameSent(OsaBuffer *buffer, int acknowledged)
{
	struct WlanConnection *connection = buffer->context;
	const struct WlanReplyFrame *reply = (const struct WlanReplyFrame *)((uint8_t *)buffer + buffer->dataOffset);
	const uint8_t *frame = (const uint8_t *)reply->frame;
	const uint8_t *body;
	struct WlanPsPeerState *ps;

	if (connection == NULL || connection->interfaceType != 3)
		return;
	ps = wlanConnectionPsState(connection);
	if (acknowledged == 0) {
		body = frame + 32;
		if (body[0] == WLAN_ACTION_CATEGORY_PUBLIC)
			p2pPublicActionSent(connection, body[6]);
		if (body[0] == P2P_FRAME_ACTION)
			p2pActionSent(connection, body[5]);
		return;
	}
	body = frame + 32;
	if (body[0] == P2P_FRAME_ACTION) {
		if (body[5] == 3) {
			ps->p2pStatus.value = 0;
			p2pDiscoverabilityRequestAnswer(connection, frame);
		}
		return;
	}
	if (body[0] != WLAN_ACTION_CATEGORY_PUBLIC || connection->mode == 0)
		return;
	if (body[1] == 11 && ((body[5] << 8) | body[4]) == 0)
		p2pDiscoveryResume(connection);
	if (memcmp(frame + 34, wlanP2pOui, 3) == 0 && body[5] == 9)
		p2pConfirmationAcked(connection, frame);
}

/* A beacon or probe response with a P2P element while the device searches: the host hears of the peer. */
void p2pScanResultFound(struct WlanConnection *connection, const uint8_t *frame)
{
	struct WlanPsPeerState *ps;

	if (connection != NULL && connection->interfaceType != 3)
		return;
	p2pDeviceConnection = connection;
	if (connection == NULL)
		p2pDeviceConnection = wlanConnectionFirst(3);
	ps = wlanConnectionPsState(p2pDeviceConnection);
	if (p2pDirectInspectContext((WlanWirelessFrame *)(uintptr_t)frame) == 0)
		return;
	p2pGroupSsidUpdate(p2pDeviceConnection, frame);
	p2pEventSend(p2pDeviceConnection, frame, WLAN_HOST_EVENT_WIFIDIRECT_GENERIC, 14, 0);
	p2pPeerFound(p2pDeviceConnection, frame, ps->p2pFlags);
}

/* The P2P device searches (or listens), or is a client: scan results take P2P peers. */
int p2pDiscoveryActive(struct WlanConnection *connection)
{
	if (connection == NULL) {
		connection = wlanConnectionFirst(3);
		if (connection == NULL)
			return 0;
	}
	if (connection->interfaceType != 3)
		return 0;
	wlanConnectionPsState(connection);
	if (connection->p2pState == 1 || connection->p2pState == 3 || connection->mode == 3)
		return 1;
	return 0;
}

/*
 * The P2P attributes the host gives (its custom P2P element): each one the
 * device keeps is copied to its place and marked configured. The listen
 * channel is the operating channel too; the GO intent keeps its tie-breaker.
 */
void p2pHostAttributesSet(struct WlanConnection *connection, const uint8_t *attributes, unsigned int length)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	while (length != 0) {
		unsigned int size = (uint16_t)(((attributes[2] << 8) | attributes[1]) + 3);

		switch (attributes[0]) {
		case P2P_ATTR_CAPABILITY:
			__aeabi_memcpy(ps->capabilityAttribute, attributes, size);
			break;
		case P2P_ATTR_DEVICE_ID:
			__aeabi_memcpy(ps->deviceIdAttribute, attributes, size);
			break;
		case P2P_ATTR_GO_INTENT:
			__aeabi_memcpy(ps->groupOwnerIntentAttribute, attributes, size);
			ps->groupOwnerIntentAttribute[3] = (ps->groupOwnerIntentAttribute[3] & 1) | (attributes[3] << 1);
			break;
		case P2P_ATTR_CONFIGURATION_TIMEOUT:
			__aeabi_memcpy(ps->configTimeoutAttribute, attributes, size);
			break;
		case P2P_ATTR_LISTEN_CHANNEL:
			__aeabi_memcpy(ps->listenChannelAttribute, attributes, size);
			__aeabi_memcpy(ps->operatingChannelAttribute, attributes, size);
			ps->configuredMask |= 1u << P2P_ATTR_OPERATING_CHANNEL;
			break;
		case P2P_ATTR_GROUP_BSSID:
			__aeabi_memcpy(ps->groupBssidAttribute, attributes, size);
			break;
		case P2P_ATTR_EXTENDED_LISTEN_TIMING:
			__aeabi_memcpy(ps->extendedListenAttribute, attributes, size);
			break;
		case P2P_ATTR_INTENDED_INTERFACE_ADDRESS:
			__aeabi_memcpy(ps->intendedAddressAttribute, attributes, size);
			break;
		case P2P_ATTR_CHANNEL_LIST:
			__aeabi_memcpy(ps->channelListAttribute, attributes, size);
			ps->channelListAttribute[5] = 4;
			break;
		case P2P_ATTR_NOTICE_OF_ABSENCE:
			break;
		case P2P_ATTR_DEVICE_INFO:
			__aeabi_memcpy(ps->deviceInfoAttribute, attributes, size);
			break;
		case P2P_ATTR_GROUP_ID:
			__aeabi_memcpy(&ps->groupIdType, attributes, size);
			break;
		case P2P_ATTR_INTERFACE:
			__aeabi_memcpy(ps->interfaceAttribute, attributes, size);
			break;
		case P2P_ATTR_OPERATING_CHANNEL:
			__aeabi_memcpy(ps->operatingChannelAttribute, attributes, size);
			break;
		default:
			goto next;
		}
		ps->configuredMask |= 1u << attributes[0];
next:
		attributes += size;
		length = (uint16_t)(length - size);
	}
}

#define P2P_ATTRIBUTE_LENGTH(attribute)	(((attribute)[2] << 8) | (attribute)[1])

/*
 * The P2P attributes the host configured, for its custom IE list: after the
 * entry's 6-byte header, in attribute order. (For the Interface attribute the
 * firmware reports the Intended Interface Address one.)
 */
void p2pHostAttributesGet(struct WlanConnection *connection, uint8_t *entry, uint16_t *length)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	uint8_t *out = entry + 6;
	unsigned int total = 0;
	unsigned int id;

	for (id = 0; id < P2P_ATTR_COUNT; id = (uint8_t)(id + 1)) {
		unsigned int size;

		if (!(ps->configuredMask & (1u << id)))
			continue;
		switch (id) {
		case P2P_ATTR_DEVICE_ID:
			__aeabi_memcpy(out, ps->deviceIdAttribute, 9);
			out += 9;
			total = (uint16_t)(total + 9);
			continue;
		case P2P_ATTR_CAPABILITY:
			__aeabi_memcpy(out, ps->capabilityAttribute, 5);
			out += 5;
			total = (uint16_t)(total + 5);
			continue;
		case P2P_ATTR_LISTEN_CHANNEL:
			__aeabi_memcpy(out, ps->listenChannelAttribute, 8);
			out += 8;
			total = (uint16_t)(total + 8);
			continue;
		case P2P_ATTR_OPERATING_CHANNEL:
			__aeabi_memcpy(out, ps->operatingChannelAttribute, 8);
			out += 8;
			total = (uint16_t)(total + 8);
			continue;
		case P2P_ATTR_GO_INTENT: {
			unsigned int intent = ps->groupOwnerIntentAttribute[3] >> 1;

			__aeabi_memcpy(out, ps->groupOwnerIntentAttribute,
				       P2P_ATTRIBUTE_LENGTH(ps->groupOwnerIntentAttribute) + 3);
			out += 3;
			out[0] = intent;	/* without the tie-breaker */
			size = P2P_ATTRIBUTE_LENGTH(ps->groupOwnerIntentAttribute);
			break;
		}
		case P2P_ATTR_DEVICE_INFO:
			__aeabi_memcpy(out, ps->deviceInfoAttribute, P2P_ATTRIBUTE_LENGTH(ps->deviceInfoAttribute) + 3);
			size = P2P_ATTRIBUTE_LENGTH(ps->deviceInfoAttribute);
			out += 3;
			break;
		case P2P_ATTR_CHANNEL_LIST:
			__aeabi_memcpy(out, ps->channelListAttribute, P2P_ATTRIBUTE_LENGTH(ps->channelListAttribute) + 3);
			size = P2P_ATTRIBUTE_LENGTH(ps->channelListAttribute);
			out += 3;
			break;
		case P2P_ATTR_GROUP_ID:
			__aeabi_memcpy(out, &ps->groupIdType, P2P_ATTRIBUTE_LENGTH(&ps->groupIdType) + 3);
			size = P2P_ATTRIBUTE_LENGTH(&ps->groupIdType);
			out += 3;
			break;
		case P2P_ATTR_GROUP_BSSID:
			__aeabi_memcpy(out, ps->groupBssidAttribute, P2P_ATTRIBUTE_LENGTH(ps->groupBssidAttribute) + 3);
			size = P2P_ATTRIBUTE_LENGTH(ps->groupBssidAttribute);
			out += 3;
			break;
		case P2P_ATTR_INTERFACE:
			__aeabi_memcpy(out, ps->intendedAddressAttribute,
				       P2P_ATTRIBUTE_LENGTH(ps->intendedAddressAttribute) + 3);
			size = P2P_ATTRIBUTE_LENGTH(ps->intendedAddressAttribute);
			out += 3;
			break;
		default:
			continue;
		}
		out += size;
		total = (uint16_t)(total + 3 + size);
	}
	*length = total;
}

/* The attributes of a WPS element (big endian type and length), by WPS_SLOT_*. */
void wpsAttributesParse(const uint8_t *data, unsigned int length, uint8_t **attributes)
{
	unsigned int offset = 0;

	while (offset < length) {
		unsigned int type = (uint16_t)((((data[1] << 8) | data[0]) << 8) | (((data[1] << 8) | data[0]) >> 8));
		unsigned int size;

		switch (type) {
		case 0x104a:
			attributes[WPS_SLOT_VERSION] = (uint8_t *)data;
			break;
		case 0x1044:
			attributes[WPS_SLOT_WPS_STATE] = (uint8_t *)data;
			break;
		case 0x103a:
			attributes[WPS_SLOT_REQUEST_TYPE] = (uint8_t *)data;
			break;
		case 0x103b:
			attributes[WPS_SLOT_RESPONSE_TYPE] = (uint8_t *)data;
			break;
		case 0x1008:
			attributes[WPS_SLOT_CONFIG_METHODS] = (uint8_t *)data;
			break;
		case 0x1047:
			attributes[WPS_SLOT_UUID_E] = (uint8_t *)data;
			break;
		case 0x1054:
			attributes[WPS_SLOT_PRIMARY_DEVICE_TYPE] = (uint8_t *)data;
			break;
		case 0x103c:
			attributes[WPS_SLOT_RF_BANDS] = (uint8_t *)data;
			break;
		case 0x1002:
			attributes[WPS_SLOT_ASSOCIATION_STATE] = (uint8_t *)data;
			break;
		case 0x1009:
			attributes[WPS_SLOT_CONFIGURATION_ERROR] = (uint8_t *)data;
			break;
		case 0x1012:
			attributes[WPS_SLOT_DEVICE_PASSWORD_ID] = (uint8_t *)data;
			break;
		case 0x1011:
			attributes[WPS_SLOT_DEVICE_NAME] = (uint8_t *)data;
			break;
		case 0x1021:
			attributes[WPS_SLOT_MANUFACTURER] = (uint8_t *)data;
			break;
		case 0x1024:
			attributes[WPS_SLOT_MODEL_NUMBER] = (uint8_t *)data;
			break;
		case 0x1023:
			attributes[WPS_SLOT_MODEL_NAME] = (uint8_t *)data;
			break;
		case 0x1042:
			attributes[WPS_SLOT_SERIAL_NUMBER] = (uint8_t *)data;
			break;
		}
		size = (uint16_t)((((data[3] << 8) | data[2]) << 8) | (((data[3] << 8) | data[2]) >> 8));
		offset = (uint16_t)(offset + size + 4);
		data += size + 4;
	}
}

/*
 * Host command 0x00f4 sends a P2P frame: after the command header, the peer's
 * address, the frame's category, and its subtype and dialog token at 12 and 13.
 */

/* A P2P action frame the host asks for: a presence request. */
int p2pActionCommand(struct WlanConnection *connection, const uint8_t *command)
{
	const uint8_t *request = command + 8;

	switch (request[12]) {
	case 0:
		break;
	case 1:		/* presence request */
		p2pVendorActionSend(connection, request, request[6], request[13], request[11], 1);
		break;
	}
	return 0;
}

/*
 * A P2P public action the host asks for: the device waits for the peer's probe
 * to start a GO negotiation (ten seconds) or an invitation, or searches for it
 * to send a provision discovery or device discoverability request.
 */
int p2pPublicActionCommand(struct WlanConnection *connection, const uint8_t *command)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	const uint8_t *request = command + 8;

	ps->dialogToken = request[13];
	switch (request[12]) {
	case 0:		/* GO negotiation request */
		ps->p2pFlags |= 0x01;
		p2pDeviceConnection = connection;
		__aeabi_memcpy(ps->eventPeerAddress, request, 6);
		if (ps->negotiationTimer != 0) {
			osaCancelTimer(ps->negotiationTimer);
			ps->negotiationTimer = 0;
		}
		osaCreateTimer(p2pListenTimerExpired, (uint32_t)connection, 10000000, &ps->negotiationTimer, 0);
		break;
	case 3:		/* invitation request */
		__aeabi_memcpy(ps->p2pDeviceAddress, request, 6);
		ps->p2pFlags |= 0x02;
		p2pDeviceConnection = connection;
		p2pDeviceStateSave(connection);
		connection->mode = 1;
		macMgmtScheduleTable(1);
		p2pDiscoveryResume(connection);
		break;
	case 5:		/* device discoverability request */
		p2pDeviceConnection = connection;
		__aeabi_memcpy(ps->discoveryDeviceAddress, request, 6);
		ps->p2pFlags |= 0x04;
		break;
	case 7:		/* provision discovery request */
		ps->p2pFlags |= 0x08;
		p2pDeviceConnection = connection;
		p2pDeviceStateSave(connection);
		__aeabi_memcpy(ps->eventPeerAddress, request, 6);
		break;
	}
	return 0;
}

int hostCmd00f4(struct WlanConnection *connection, uint8_t *command)
{
	if (command[14] == WLAN_ACTION_CATEGORY_PUBLIC)
		p2pPublicActionCommand(connection, command);
	else if (command[14] == P2P_FRAME_ACTION)
		p2pActionCommand(connection, command);
	return 0;
}
