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
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/p2pgo.h"
#include "robin/pssm.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/scan.h"
#include "robin/txpower.h"
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

void p2pGroupStartTimerExpired(uint32_t timer, uint32_t argument)
{
	wlanConnectionPsState((struct WlanConnection *)argument)->groupStarted = 0;
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
