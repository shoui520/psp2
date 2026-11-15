/*
 * An access point's BSS: the checks an association request has to pass, the
 * aging of silent peers, the PHY mode and HT operation of the BSS and its stop
 * (every peer dropped, the group key and the SQRAM context released, the radio
 * told). Marvell published no source for it; the names are the firmware's own
 * placeholder names, kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/apps.h"
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/assocAp.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/hostps.h"
#include "robin/htba.h"
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/stream.h"
#include "robin/task.h"
#include "robin/keyMgmtAp.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/sys.h"
#include "robin/wlan.h"

#define WLAN_STATUS_INVALID_PAIRWISE_CIPHER	45

/* Not converted yet; provisional names. */
void macMgmtScheduleFrame(struct WlanConnection *connection, unsigned int subtype);	/* 0x3ac6c */
void rfCalibFilterQueue(unsigned int wpa, void *keyData);				/* 0x2b764 */
void rfCalibHandleTimer(const uint8_t *htCapability, uint8_t **cursor, uint8_t *length); /* 0x2b774 */
void rfCalibProcessTimer(unsigned int value, uint8_t **cursor, uint8_t *length);	/* 0x2b7b6 */
void rfCalibUpdateTimer(const uint8_t *wmm, uint8_t **cursor, uint8_t *length);	/* 0x2b7ea */
void cbProcCheckFrame(struct WlanConnection *connection);				/* 0x1ea08 */
void romLibNotifyContext(void);								/* ROM veneer 0x532e4 */
int p2pSqramFilterState(struct WlanConnection *connection, const uint8_t *frame, unsigned int category);
int rfCalibRecordQueue(struct WlanConnection *connection, unsigned int a, unsigned int b,
		       unsigned int maxStations, uint8_t *table);			/* 0x2b4c0 */
int fmCoexRecordBuffer(struct WlanConnectionInfo *info, const uint8_t *element, uint16_t *status);	/* 0x4bf6a */
void rfCalibAdjustQueue(struct WlanConnection *connection, const WlanIePointers *elements,
			unsigned int channel, uint8_t *erpInfo, uint8_t *olbcActive,
			unsigned int beaconPeriod, int add);			/* 0x2b45c */
void uapSqramUpdateState(int value);						/* SQRAM */
void uapSqramConfigureState(int value);						/* SQRAM */
int uapSqramProcessBuffer(void);						/* SQRAM */

/* The association request goes back with the BSS's own elements (custom ones too) as a response, after the station is let go of. */
int macMgmtHandlePacket(OsaBuffer *buffer, uintptr_t argument)
{
	struct WlanConnection *connection = buffer->context;
	WlanWirelessFrame *frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);

	macMgmtFilterBuffer(connection);
	macMgmtDispatchBuffer(connection, connection->peerAddress, 5);
	wlanCustomIeApply(connection, 1, frame, 6);
	return wlanMgmtSendFrame(buffer);
}

/*
 * The security elements of an association request against the BSS's: 0: acceptable, -1: not, with the
 * status code 45 (or 0 where the WAPI element is checked). wps: the request carries a WPS element.
 */
int macMgmtUpdatePacket(struct WlanConnection *connection, int wps, const void *rsn, const void *wpa,
			const uint8_t *wapi, uint16_t *status)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	int result = -1;

	*status = WLAN_STATUS_INVALID_PAIRWISE_CIPHER;
	if (wlanFrameHasWpsIe((const uint8_t *)connection->probeResponseRequest) != 0) {
		if (wps != 0 || (rsn == NULL && wpa == NULL))
			return 0;
	} else if (wps != 0) {
		return result;
	}
	if (info->capability[0] & 0x10) {
		if (info->securityMode.wpa || info->securityMode.wpa2) {
			if (assocSrvAp_checkRsnWpa(connection, &role->keyMgmt, info->apRsn.wpaUcstCipher,
						   info->apRsn.wpa2UcstCipher, info->apRsn.mcstCipher,
						   info->apRsn.AuthKey, &info->securityMode,
						   (IEEEtypes_RSNElement_t *)rsn, (IEEEtypes_WPAElement_t *)wpa, 0) != 0)
				return result;
		} else if (info->securityMode.wepStatic) {
			if (rsn != NULL && wpa != NULL)
				return result;
		} else {
			if (!info->securityMode.wapi || fmCoexRecordBuffer(info, wapi, status) == 0)
				return result;
			*status = 0;
		}
	}
	return 0;
}

/* The peer is deauthenticated (reason 2: its authentication is no longer valid). */
int macMgmtCheckPacket(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;

	macMgmtFilterBuffer(connection);
	return macMgmtInspectBuffer(connection, connection->peerAddress, 2);
}

/*
 * The aging of the BSS's peers, once per tick: each peer's countdown (WlanConnectionRole.listenInterval)
 * runs down; one that reaches zero is dropped when it was leaving or unauthenticated, and deauthenticated
 * (reason 2) otherwise.
 */
void macMgmtPreparePacket(struct WlanConnection *connection)
{
	unsigned int posture = sysIrqDisable();

	if (connection != NULL && wlanConnectionBssStarted(connection) != 0) {
		wlanConnectionInfoSlot(connection);
		wlanConnectionInfo(connection);
		do {
			struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);

			if (role->listenInterval != 0 && --role->listenInterval == 0) {
				if (role->state <= 1) {
					if (role->state == 1)
						macMgmtDispatchBuffer(connection, connection->peerAddress, role->eventCode);
					macMgmtScheduleBuffer(connection);
					if (!(connection->flags2 & WLAN_CONN2_HOLD))
						wlanConnectionRelease(connection);
				} else {
					role->eventCode = 2;
					macMgmtInspectBuffer(connection, connection->peerAddress, 2);
				}
			}
			connection = wlanConnectionNextByBss(connection, connection->bssType, connection->bssNumber);
		} while (connection != NULL);
	}
	sysIrqRestore(posture);
}

/* A beacon or probe response of another BSS, with the BSS's PHY mode b/g: its elements are looked at for overlapping legacy BSSs. */
void macMgmtDispatchPacket(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	if (info->phyMode == 1) {
		WlanIePointers elements;

		wlanParseIes(frame + 44, (frame[1] << 8 | frame[0]) - 12, &elements);
		rfCalibAdjustQueue(connection, &elements, connection->channel, &info->erpInfo, &info->olbcActive,
				   connection->beaconPeriod, 1);
	}
}

void macMgmtParsePacket(struct WlanConnection *connection, int value, int code)
{
	int data = value;

	wlanReportEvent(connection, &data, code);
}

void macMgmtBuildPacket(unsigned int interfaceType, unsigned int bssNumber)
{
	wlanCustomIeRemoveBss(interfaceType, bssNumber);
}

/* The HT Information element of the BSS (ID 61, 22 bytes): its primary channel, secondary channel offset and 40 MHz permission. */
void macMgmtComputePacket(struct WlanConnectionInfo *info, uint8_t *element, const uint8_t *htCapability,
			  const struct WlanConnectionRole *role, unsigned int beaconPeriod)
{
	wlanApOlbcReset(info, beaconPeriod);
	__aeabi_memclr(element, 0x18);
	element[0] = 61;
	element[1] = 22;
	element[2] = role->channel;
	element[3] |= 0x08;
	element[3] = (element[3] & 0xfb) | ((htCapability[2] & 0x02) << 1);
	element[3] = (element[3] & 0xfc) | ((role->channelFlags >> 4) & 3);
}

/* The BSS is stopped: keys, the SQRAM context, the radio and the connection's slot are given back. */
void macMgmtQueryPacket(struct WlanConnection *connection)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	int count;

	KeyMgmtStopHskTimer(connection, 1);
	osaCancelTimer(info->grpRekeyTimer);
	if (info->securityMode.wpa || info->securityMode.wpa2) {
		if (connection->keyBlocks[0] != NULL)
			uapSqramConfigureContext(connection->keyBlocks[0]);
	}
	connection->keyBlocks[0] = NULL;
	if (info->securityMode.wapi && connection->keyBlocks[1] != NULL)
		uapSqramResolveContext(&connection->keyBlocks[1]);
	else
		connection->keyBlocks[1] = NULL;
	connection->flags &= ~WLAN_CONN_PRIVACY;
	connection->hostFlags &= ~WLAN_CONN_HOST_FLAG_8;
	connection->probeResponseBuffer->allocatorTag = 0;
	wlanBaTablesChangedHandler();
	romLibResolveState();
	uapSqramUpdateState(wlanConnectionCommandPrepare(2, 0));
	wlanConnectionDecrement();
	macMgmtComputePacket(info, info->htOperation, info->htCapability, role, connection->beaconPeriod);
	if (wlanConnectionSlotDetach(connection) != 0) {
		sysIrqDisable();
		for (;;)
			;
	}
	wlanTxPowerSetConnectionLimit(&connection->txPowerLimit, 255, 1);
	count = wlanConnectionGetCount();
	if (count != 0) {
		uapSqramConfigureState(count);
		wlanConnectionCommandPrepare(2, 1);
	} else {
		wlanMacOperationModeSet(0);
		wlanConnectionCommandCheck(3, 1);
		MAC_REG(0x0e0) = 31;
	}
	romLibApplyState();
}

/* The BSS is down: its state is cleared and, with all (flushAll), every peer is dropped and the group told (deauthentication, reason 3). */
int macMgmtResetPacket(struct WlanConnection *connection, int flushAll)
{
	unsigned int posture = sysIrqDisable();
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	connection->state = 0;
	info->bssStarted = 0;
	if (wlanSessionId != 0) {
		wlanPsCancelHostModeTimer();
		wlanPsHostModeExit();
	}
	wlanApPsFlushGroup(connection);
	if (flushAll) {
		macMgmtScheduleBuffer(connection);
		for (;;) {
			struct WlanConnection *peer = wlanConnectionNextByBss(connection, connection->bssType,
									      connection->bssNumber);

			if (peer == NULL)
				break;
			macMgmtScheduleBuffer(peer);
			wlanConnectionRelease(peer);
		}
		wlanMgmtSendDeauth(connection, wlanBroadcastAddress, connection->localAddress, 3, 0);
	}
	sysIrqRestore(posture);
	while (info->uapBusy != 0)
		osaTaskSleep(1);
	return 0;
}

/* The BSS is stopped for good: 2 when it could not be reset. */
int macMgmtSyncPacket(struct WlanConnection *connection)
{
	int result = 0;

	if (wlanConnectionBssStarted(connection) == 1) {
		int reset = macMgmtResetPacket(connection, 1);

		if (reset != 0) {
			if (reset != 3)
				for (;;)
					;
			return 2;
		}
		macMgmtQueryPacket(connection);
	}
	return result;
}

int macMgmtApplyPacket(void)
{
	return uapSqramProcessBuffer();
}

/*
 * The PHY mode of the BSS from its rates and band (0: 802.11b, 1: b/g, 2: g only), and
 * whether HT may be used (it may not with static WEP, TKIP-only security, without QoS or in 802.11b).
 */
int macMgmtResolvePacket(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	int haveOfdm = wlanRatesHaveOfdm(info->rates, 14);
	int haveCck = wlanRatesHaveCck(info->rates, 14);
	unsigned int band = role->channelFlags & 3;

	if (connection->channel == 0)
		return 0;
	if (band == 1 || band == 2) {
		if (haveCck != 0)
			return 0;
		info->phyMode = 2;
	} else if (connection->interfaceType == 3) {
		if (connection->mode == 0 && haveCck == 0)
			return 0;
	} else {
		if (haveCck == 0)
			return 0;
		if (haveOfdm != 0)
			info->phyMode = 1;
		else
			info->phyMode = 0;
	}
	if (info->htEnabled == 1) {
		Cipher_t cipher = { 0 };

		if (info->securityMode.wpa2)
			cipher = info->apRsn.wpa2UcstCipher;
		else if (info->securityMode.wpa)
			cipher = info->apRsn.wpaUcstCipher;
		if (info->securityMode.wepStatic ||
		    ((info->securityMode.wpa || info->securityMode.wpa2) && !cipher.ccmp) ||
		    info->qosEnabled == 0 || info->phyMode == 0)
			return 0;
	}
	if (info->qosEnabled == 1)
		info->pktFwdCtl &= ~1;
	return 1;
}

/* The channel of the BSS and its secondary channel (the channel flags: bits 4-5 the offset). out[0]: flags, out[1]: channel. */
void macMgmtNotifyPacket(struct WlanConnection *connection, uint8_t *out)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	uint32_t band;
	uint8_t index;

	if (macMgmtSchedulePacket() == 1) {
		out[0] = role->channelFlags;
		out[1] = role->channel;
		return;
	}
	macMgmtPrepareContext(out, &band);
	connection->channel = (uint8_t)band;
	out[1] = (uint8_t)band;
	role->channel = (uint8_t)band;
	if ((role->channelFlags >> 4) & 3) {
		if (((out[0] >> 4) & 3) == 0) {
			int both = wlanDomainChannelIndex(12, &index);

			wlanSetSecondaryChannel(out, connection->channel, info->htOperation, both);
		}
		role->channelFlags = (role->channelFlags & 0xcf) | (((out[0] >> 4) & 3) << 4);
	}
}

/*
 * The BSS is started (an access point's connection, or a P2P group owner's): its channel, security,
 * rates, HT operation and beacon and probe response templates are set, the MAC is given the BSS's
 * address and beacon timing and the host is sent the BSS's configuration (event 46). 1: started.
 */
int macMgmtFormatPacket(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanPsSleep *sleep = &info->sleep;
	uint8_t channelOut[2];
	OsaBuffer *buffer = NULL;
	WlanTxDescriptor *descriptor;
	uint8_t *cursor;
	unsigned int ratesLength;
	unsigned int bitmap;
	uint8_t contextFlags, contextChannel;

	if (connection->interfaceType != 2 && (connection->interfaceType != 3 || connection->mode == 0))
		return 0;
	macMgmtNotifyPacket(connection, channelOut);
	if (macMgmtResolvePacket(connection) != 1)
		return 0;
	if (wlanSessionId != 0) {
		wlanPsCancelHostModeTimer();
		wlanPsHostModeExit();
	}
	romLibResolveState();
	uapSqramUpdateState(wlanConnectionCommandPrepare(2, 0));
	MAC_REG(0x824) |= 4;
	wlanTxPowerUpdateConnection(connection, channelOut[1], 1);
	if (info->securityMode.wpa || info->securityMode.wpa2)
		KeyMgmtInit(connection);
	keyApi_ApUpdateKeyMaterial(connection, 1);
	rfCalibFilterQueue(info->securityMode.wpa, (uint8_t *)info + 796);
	wlanBaTablesChangedHandler();
	SOC_MAC_EVENT_CONTROL |= 0x20;
	if (wlanP2pConnection(connection) != 0) {
		contextFlags = role->channelFlags;
		contextChannel = role->channel;
	} else {
		contextFlags = channelOut[0];
		contextChannel = channelOut[1];
	}
	macMgmtUpdateContext(contextFlags, contextChannel);
	macMgmtCheckContext();
	ratesLength = (uint8_t)romStringLength(info->rates, 14);
	bitmap = wlanRateBitmapAll(info->rates, ratesLength, 0, 0, (const WlanHtCapabilityIe *)connection->htCapability);
	macMgmtQueryContext(connection, info->rates, ratesLength, bitmap);
	wlanApOlbcClear(connection);
	wlanApNonErpGone(connection);
	wlanApLongSlotGone(connection);
	macMgmtComputePacket(info, info->htOperation, info->htCapability, role, connection->beaconPeriod);
	if (info->ht20Only != 0 || !(info->htCapability[2] & 0x02)) {
		info->htOperation[3] &= 0xf8;
	} else {
		info->htOperation[3] |= 4;
		info->htOperation[3] = (info->htOperation[3] & 0xfc) | ((role->channelFlags >> 4) & 3);
	}
	wlanConnectionIncrement();
	connection->dtimHold = 0;
	if (wlanConnectionSlotAttach(connection) != 0)
		for (;;)
			;
	connection->probeResponseBuffer->allocatorTag = 1;
	connection->beaconBuffer->dataOffset = 176;
	macMgmtScheduleFrame(connection, 8);
	connection->probeResponseBuffer->dataOffset = 176;
	macMgmtScheduleFrame(connection, 5);
	info->uapBusy = 0;
	wlanBssSelectAddress(2, connection->localAddress);
	wlanMacAddressApply();
	sleep->beaconPeriod = connection->beaconPeriod;
	sleep->beaconInterval = connection->beaconPeriod << 10;
	wlanMacOperationModeSet(1);
	MAC_REG(0x824) |= 4;
	MAC_REG(0x300) |= 0x2000;
	if (info->psStaAgeout != 0) {
		unsigned int beacon = ((unsigned int)info->beaconPeriod << 7) / 125;

		info->listenIntervalPs = (beacon + 100 * info->psStaAgeout - 1) / beacon;
	} else {
		info->listenIntervalPs = 0;
	}
	if (info->staAgeout != 0) {
		unsigned int beacon = ((unsigned int)info->beaconPeriod << 7) / 125;

		info->listenInterval = (beacon + 100 * info->staAgeout - 1) / beacon;
	} else {
		info->listenInterval = 0;
	}
	cbProcCheckFrame(connection);
	romLibNotifyContext();
	uapSqramConfigureState(wlanConnectionCommandCheck(2, 0));
	wlanConnectionCommandPrepare(2, 1);
	romLibApplyState();
	if (wlanTxAllocFrame(&buffer, &descriptor, 0x190, 46) != 0) {
		uint8_t *length = (uint8_t *)descriptor + 10;

		cursor = (uint8_t *)descriptor + 24;
		if (info->htEnabled == 1)
			rfCalibHandleTimer(info->htCapability, &cursor, length);
		rfCalibProcessTimer(info->pktFwdCtl, &cursor, length);
		if (info->qosEnabled == 1)
			rfCalibUpdateTimer((const uint8_t *)info->wmmRecords, &cursor, length);
		wlanHostEventSendWithAddress(connection, connection->localAddress, buffer);
	}
	return 1;
}

int macMgmtInspectPacket(struct WlanConnection *connection, unsigned int a, unsigned int b)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int posture = sysIrqDisable();

	rfCalibRecordQueue(connection, a, b, info->maxStations, info->peerTable);
	return sysIrqRestore(posture);
}

/* An action frame of the peer: Block Ack (3), spectrum / 20/40 BSS coexistence (4) and vendor specific (127). */
int macMgmtTrackPacket(OsaBuffer *buffer, uintptr_t argument)
{
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	const uint8_t *body = frame + 32;
	struct WlanConnection *connection = buffer->context;

	unsigned int category;

	if (body[0] == 3)
		return wlanBaActionReceived(connection, body, frame, (int)argument);
	if (body[0] == 4) {
		if (body[1] == 0)
			return wlanBssCoexFrameReceived(connection, body, frame, (int)argument);
		if (body[1] != 9 || connection == NULL || connection->interfaceType != 3)
			return (int)(uintptr_t)connection;
		category = 9;
	} else {
		if (body[0] != 127 || connection == NULL || connection->interfaceType != 3)
			return (int)(uintptr_t)connection;
		category = 127;
	}
	return p2pSqramFilterState(connection, frame, category);
}
