/*
 * A P2P group owner's connection (0x3f8e4 - 0x400e0): the group is started, set up and torn down,
 * its SSID is made ("DIRECT-" and two random characters), and the WPS and P2P information elements
 * of its frames are built from the custom elements the host gave. The names of the firmware's
 * placeholders are kept; see robin/p2pgo.h.
 */
#include <stddef.h>
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/macentry.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/p2pgo.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/uapbss.h"

extern WlanWirelessFrame *wlanIbssProbeResponseFrame;	/* 0x54c28 */
extern WlanWirelessFrame *wlanIbssBeaconFrame;		/* 0x54c2c */
extern void __aeabi_memclr4(void *dest, size_t n);
extern void __aeabi_memcpy4(void *dest, const void *source, size_t n);


uint8_t p2pBroadcastAddress[6] __attribute__((section(".itcm.rw"), aligned(4))) = {	/* 0x5524c */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};
uint32_t p2pFrameDataMissing __attribute__((section(".itcm.rw")));	/* 0x55254: frames p2pActionFrameAlloc() got without data */
struct WlanConnection *p2pDeviceConnection __attribute__((section(".itcm.rw")));	/* 0x55258 */
uint32_t p2pModeElevenSet __attribute__((section(".itcm.rw")));	/* 0x5525c */
struct WlanConnection *p2pGroupConnection __attribute__((section(".itcm.rw")));
const char *p2pDirectSsidPrefix __attribute__((section(".itcm.rw"))) = "DIRECT-";

const uint8_t wlanWpsOui[4] = { 0x00, 0x50, 0xf2, 0x04 };	/* the WPS element's OUI and type */

/* Tears the group down: the connection goes back to no role. */
unsigned int p2pDirectNotifyState(struct WlanConnection *connection)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	wlanConnectionCommandCheck(15, 1);
	__aeabi_memclr4(p2pPeerStateStorage, sizeof(p2pPeerStateStorage));
	__aeabi_memclr4(ps->wpsAttributes, sizeof(ps->wpsAttributes));
	__aeabi_memclr4(connection->networkSsid, sizeof(connection->networkSsid));
	connection->networkSsidLength = 0;
	p2pPersistentGroupsInit(ps);
	if (connection->mode == 2)
		romLibDispatchContext(0);
	connection->mode = 0;
	connection->p2pState = 0;
	ps->p2pAuthCount = 0;
	ps->groupClientCount = 0;
	wlanIbssProbeResponseFrame = NULL;
	wlanIbssBeaconFrame = NULL;
	return 0;
}

/* The connection answers to the address: its local and interface address, and its index in the MAC. */
int p2pDirectFormatState(struct WlanConnection *connection, const uint8_t *address)
{
	__aeabi_memcpy(connection->localAddress, address, 6);
	__aeabi_memcpy(connection->interfaceAddress, address, 6);
	return macTxNotifyAcquireBuffer(2, connection->interfaceAddress, connection->interfaceIndex, 1);
}

/* 1 when the group shares the channel of a station connection or an access point that is up. */
int p2pDirectInspectState(void)
{
	struct WlanPsPeerState *ps = wlanConnectionPsState(wlanConnectionFirst(3));
	struct WlanConnection *station = wlanConnectionFirst(0);

	if (station == NULL || (station->flags2 & 0x10) == 0) {
		if (wlanConnectionBssStarted(wlanConnectionFirst(2)) != 1)
			return 0;
	}
	ps->channelShared = 1;
	return 1;
}

/* The rates and thresholds a group starts with: 6 to 54 Mbit/s, the first one basic. */
unsigned int p2pDirectTrackState(struct WlanConnection *connection, struct WlanConnectionInfo *info)
{
	__aeabi_memclr(info->rates, sizeof(info->rates));
	info->rates[0] = 0x8c;
	info->rates[1] = 0x12;
	info->rates[2] = 0x18;
	info->rates[3] = 0x24;
	info->rates[4] = 0x30;
	info->rates[5] = 0x48;
	info->rates[6] = 0x60;
	info->rates[7] = 0x6c;
	__aeabi_memcpy(wlanLocalState->rates, info->rates, sizeof(info->rates));
	info->rateCount = 13;
	info->rtsThreshold = 2347;
	info->fragThreshold = 2346;
	info->staAgeout = 1800;
	info->unicastRate = 0;
	info->txRate = 12;
	return 12;
}

/* Gives the BSS its address, rates, SSID and channel. */
unsigned int p2pDirectManageState(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	__aeabi_memcpy(info->bssid, ps->interfaceAddress, 6);
	p2pDirectTrackState(connection, info);
	info->phyMode = 1;
	if (ps->ssidFixed == 0) {
		__aeabi_memclr(info->ssid, sizeof(info->ssid));
		__aeabi_memcpy(info->ssid, p2pDirectSsidPrefix, strlen(p2pDirectSsidPrefix));
		info->ssidLength = 7;
	}
	role->channel = ps->defaultChannel;
	__aeabi_memcpy(connection->networkSsid, info->ssid, info->ssidLength);
	connection->networkSsidLength = info->ssidLength;
	connection->dtimPeriod = info->dtimPeriod;
	ps->groupChannel = role->channel;
	info->htOperation[2] = role->channel;	/* the primary channel of the HT operation element */
	return role->channel;
}

/* A character of the random part of the SSID: a letter or a digit. */
void p2pDirectReleaseState(uint8_t *suffix)
{
	unsigned int i;

	for (i = 0; i < 2; i = (uint8_t)(i + 1)) {
		unsigned int value = rand() % 62;

		if (value < 26)
			suffix[i] = value + 'A';
		else if (value < 52)
			suffix[i] = value + 71;
		else if (value < 62)
			suffix[i] = value - 4;
	}
}

/* Makes the group's SSID: "DIRECT-" and two random characters. */
static void p2pDirectSsidMake(struct WlanPsPeerState *ps)
{
	__aeabi_memcpy(ps->groupSsid, "DIRECT-", 7);
	p2pDirectReleaseState(ps->groupSsid + 7);
	ps->groupIdLength = 15;
}

/* Starts the group owner: its state, a channel, the address and the MAC's events; 1 when the host has not set it up. */
unsigned int p2pDirectAcquireState(struct WlanConnection *connection)
{
	uint32_t address[2] = { 0, 0 };
	uint8_t flags[4];
	struct WlanConnectionInfo *info;
	struct WlanPsPeerState *ps;
	unsigned int i;
	uint32_t timer;

	wlanConnectionInfoSlot(connection);
	info = wlanConnectionInfo(connection);
	ps = wlanConnectionPsState(connection);
	if ((P2P_GROUP_REQUIRED & ~ps->configuredMask) != 0)
		return 1;
	for (i = 0; i < 17; i++) {
		if (i != 6 && ps->wpsAttributes[i] == NULL)
			return 1;
	}
	ps->tieBreaker = rand() & 1;
	ps->maxDiscoveryPeriod = 3;
	ps->minDiscoveryPeriod = 1;
	ps->p2pAuthCount = 0;
	ps->groupClientCount = 0;
	ps->groupSettingA = 1;
	ps->groupSettingB = 1;
	ps->p2pChannel = 255;
	ps->ssidPending = 1;
	ps->ssidFixed = 0;
	ps->discoverabilityPending = 0;
	ps->persistentGroupIndex = 255;
	ps->p2pBssActive = 0;
	ps->pendingFrame = NULL;
	ps->provisioningAction = 1;
	ps->wpsConfigMethod = 256;
	ps->wpsParamsAction = 1;
	ps->wpsDevicePasswordId = 5;
	if (p2pDirectInspectState() != 0)
		ps->channelShared = 1;
	__aeabi_memcpy(address, ps->interfaceAddress, 6);
	romLibManageState((const uint8_t *)address);
	wlanCurrentChannelFlags(flags);
	wlanSetChannel(ps->defaultChannel, flags, 1);
	connection->mode = 1;
	connection->p2pState = 2;
	connection->channel = ps->defaultChannel;
	wlanConnectionIndexUnassign(ps->interfaceAddress);
	p2pDirectFormatState(connection, ps->interfaceAddress);
	__aeabi_memclr4(connection->networkSsid, sizeof(connection->networkSsid));
	connection->networkSsidLength = 0;
	__aeabi_memcpy4(connection->networkSsid, "DIRECT-", 7);
	connection->networkSsidLength = 7;
	p2pDirectManageState(connection);
	macMgmtFormatPacket(connection);
	timer = info->grpRekeyTimer;
	if (timer != 0) {
		osaCancelTimer(timer);
		info->grpRekeyTimer = 0;
	}
	romLibResolveState();
	MAC_300_MODE |= 0x5e46;
	MAC_300_MODE &= 0xffff5e4e;
	SOC_MAC_EVENT_CONTROL |= 0x20;
	MAC_824_CONTROL &= ~4u;
	romLibConfigureContext(0, 3);
	romLibConfigureContext(1, 4);
	wlanMacOperationModeSet(0);
	wlanConnectionCommandCheck(14, 1);
	romLibApplyState();
	romLibInspectContext(romLibFormatState());
	wlanMacFilterMasksUpdate();
	MAC_300_MODE |= (uintptr_t)&MAC_300_MODE >> 18;
	sleepMacResume();
	return 0;
}

/* The SSID, the passphrase and the channel of the group once its owner has been negotiated (1 when it is not one). */
unsigned int p2pDirectCheckSsidPrefix(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
	const struct WlanP2pPersistentGroup *selected;
	uint8_t *ssid;
	unsigned int length;

	if (connection->mode == 0)
		return 1;
	connection->mode = 2;
	connection->p2pState = 2;
	ps->p2pOptions = (ps->p2pOptions & 0xfa) | 1;
	if (ps->p2pOptions & 2) {
		selected = p2pPersistentGroupLookup(connection, ps->groupDeviceAddress, ps->groupSsid,
						 (uint8_t)(ps->groupIdLength - 6));
		if (ps->p2pOptions & 2) {
			if (ps->ssidPending == 1) {
				if (selected == NULL) {
					if (ps->ssidFixed == 0)
						p2pDirectSsidMake(ps);
					goto convert;
				}
			} else if (selected == NULL) {
convert:
				p2pPersistentGroupStore(connection, &ps->groupIdType, ps->groupBssidAttribute, ps->groupSsid, 2, NULL);
				ps->ssidPending = 0;
				goto made;
			}
			if (info->updatePassPhrase == 0)
				__aeabi_memcpy(info->apRsn.PSKValue, selected + 51, sizeof(info->apRsn.PSKValue));
			ps->ssidPending = 0;
			goto made;
		}
	}
	if (ps->ssidPending != 0 && ps->ssidFixed == 0)
		p2pDirectSsidMake(ps);
	ps->ssidPending = 0;
made:
	ssid = info->ssid;
	info->updatePassPhrase = 1;
	if (ssid == NULL)
		return 1;
	if (ps->ssidFixed != 0)
		length = (uint8_t)(ps->groupIdLength - 6);
	else
		length = 9;
	if (memcmp(ps->interfaceAddress, ps->groupAddress, 4) == 0) {
		wlanConnectionIndexAssign(ps->interfaceAddress);
		p2pDirectFormatState(connection, ps->groupAddress);
	}
	__aeabi_memcpy(ssid, ps->groupSsid, length);
	info->ssidLength = length;
	__aeabi_memcpy(connection->networkSsid, ssid, length);
	connection->networkSsidLength = info->ssidLength;
	if (ps->requestedChannel != 0)
		ps->groupChannel = ps->requestedChannel;
	role->channel = ps->groupChannel;
	connection->channel = ps->groupChannel;
	if (p2pDirectInspectState() != 0)
		ps->channelShared = 1;
	macMgmtUpdateContext(role->channelFlags, role->channel);
	wlanMacFilterMasksUpdate();
	return 0;
}

/* The group's BSSID is known: the MAC answers to it, and the connection is active. */
unsigned int p2pDirectSetupState(struct WlanConnection *connection)
{
	uint32_t address[2] = { 0, 0 };
	uint8_t flags[4];
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

	if (connection->mode == 0)
		return 1;
	MAC_824_CONTROL |= 4;
	wlanCurrentChannelFlags(flags);
	wlanSetChannel(ps->groupChannel, flags, 1);
	connection->channel = ps->groupChannel;
	connection->p2pState = 2;
	connection->mode = 3;
	__aeabi_memcpy(info->bssid, ps->groupAddress, 6);
	__aeabi_memcpy4(address, ps->groupAddress, 6);
	romLibManageState((const uint8_t *)address);
	if (memcmp(ps->interfaceAddress, ps->groupAddress, 4) == 0) {
		wlanConnectionIndexAssign(ps->interfaceAddress);
		p2pDirectFormatState(connection, ps->groupAddress);
	}
	p2pGroupConnection = connection;
	wlanConnectionActivate(connection);
	wlanMacFilterMasksUpdate();
	return 0;
}

/* Copies an attribute (two bytes of id, two of length, big endian, and the body); its body length. */
unsigned int p2pDirectClearState(uint8_t *out, const uint8_t *attribute)
{
	unsigned int length;

	if (attribute == NULL)
		return 0;
	out[0] = attribute[0];
	out[1] = attribute[1];
	out[2] = attribute[2];
	out[3] = attribute[3];
	length = (uint16_t)((attribute[2] << 8) | attribute[3]);
	__aeabi_memcpy(out + 4, attribute + 4, length);
	return length;
}

/*
 * Appends a WPS element to the frame body in buffer (its length, 16 bits little endian, is the
 * first two bytes): the attributes of the custom elements the mask picks.
 */
unsigned int p2pDirectConvertState(struct WlanConnection *connection, WlanWirelessFrame *frame, unsigned int mask)
{
	uint8_t *buffer = (uint8_t *)frame;
	uint8_t *const *attributes = wlanConnectionPsState(connection)->wpsAttributes;
	uint8_t *element = &buffer[((buffer[1] << 8) | buffer[0]) + 32];
	uint8_t *length = element + 1;
	uint8_t *end = element + 6;
	unsigned int i;
	unsigned int total;

	*element = 0xdd;
	__aeabi_memcpy(element + 2, wlanWpsOui, 4);
	for (i = 0; i < 19; i = (uint8_t)(i + 1)) {
		if ((1u << i) > mask)
			break;
		if (((1u << i) & mask) != 0) {
			const uint8_t *attribute = attributes[i];

			if (attribute != NULL)
				end += p2pDirectClearState(end, attribute) + 4;
		}
	}
	*length = (uint8_t)(end - length - 1);
	total = ((buffer[1] << 8) | buffer[0]) + *length + 2;
	buffer[0] = total;
	buffer[1] = total >> 8;
	return buffer[1];
}

unsigned int p2pDirectVerifyState(struct WlanConnection *connection, WlanWirelessFrame *frame)
{
	return p2pDirectConvertState(connection, frame, 0x1083);
}

unsigned int p2pDirectSelectState(struct WlanConnection *connection, WlanWirelessFrame *frame)
{
	return p2pDirectConvertState(connection, frame, 0x1fb5);
}

unsigned int p2pDirectAdjustState(struct WlanConnection *connection, WlanWirelessFrame *frame)
{
	return p2pDirectConvertState(connection, frame, 0x5f9bb);
}

/* The header of a P2P element: id, length 4 and the OUI and type of Wi-Fi Direct. */
__attribute__((noinline)) uint8_t *p2pDirectRecordState(uint8_t *element)
{
	element[1] = 4;
	element[0] = 0xdd;
	element[2] = 0x50;
	element[3] = 0x6f;
	element[4] = 0x9a;
	element[5] = 9;
	return element;
}

/* Appends the header of a P2P element to the frame body in buffer. */
uint8_t *p2pDirectFilterState(uint8_t *buffer)
{
	uint8_t *element = &buffer[((buffer[1] << 8) | buffer[0]) + 32];

	p2pDirectRecordState(element);
	*(uint16_t *)buffer = ((buffer[1] << 8) | buffer[0]) + element[1] + 2;
	return element;
}
