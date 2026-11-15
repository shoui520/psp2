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
#include "robin/hostps.h"
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
