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
#include "robin/pssm.h"
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
#include "robin/runtime.h"
#include "robin/sys.h"
#include "robin/wapi.h"
#include "robin/wlan.h"

#define WLAN_STATUS_INVALID_PAIRWISE_CIPHER	45
#define WLAN_STATUS_ASSOC_DENIED		12
#define WLAN_HT_CAP_40MHZ_INTOLERANT		0x4000
#define WLAN_HT_CAP_GREENFIELD			0x0010

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	((bytes)[0] = (uint8_t)(value), (bytes)[1] = (uint8_t)((value) >> 8))

/* Not converted yet; provisional names. */
void wlanPsAcquirePacket(struct WlanConnection *connection);				/* 0x3d1f4 */
void macTxNotifyApplyFrame(struct WlanConnection *connection);				/* 0x3360a */
void macTxNotifyQueryFrame(struct WlanConnection *connection);				/* 0x33570 */
void macTxNotifyDispatchFrame(struct WlanConnection *connection);			/* 0x3351c */
void wlanPsReleasePacket(struct WlanConnection *connection, uint32_t rateSet);		/* 0x3d286 */
unsigned int p2pDirectCheckContext(struct WlanConnection *connection, WlanWirelessFrame *frame,
				   const void *attributes, unsigned int include,
				   unsigned int exclude);				/* 0x402ea */
uint8_t *p2pDirectReleaseContext(struct WlanConnection *connection,
				 const WlanWirelessFrame *request);			/* 0x40e20 */
void wlanSubmitControlFrame(struct WlanConnection *connection, const uint8_t *frame);	/* 0x3b9f0 */
void p2pSqramMatchDirectSsid(struct WlanConnection *connection, const uint8_t *frame,
			     uintptr_t argument);					/* SQRAM */
void rfCalibFilterQueue(unsigned int wpa, void *keyData);				/* 0x2b764 */
void rfCalibHandleTimer(const uint8_t *htCapability, uint8_t **cursor, uint8_t *length); /* 0x2b774 */
void rfCalibProcessTimer(unsigned int value, uint8_t **cursor, uint8_t *length);	/* 0x2b7b6 */
void rfCalibUpdateTimer(const uint8_t *wmm, uint8_t **cursor, uint8_t *length);	/* 0x2b7ea */
void cbProcCheckFrame(struct WlanConnection *connection);				/* 0x1ea08 */
int p2pSqramFilterState(struct WlanConnection *connection, const uint8_t *frame, unsigned int category);
int rfCalibRecordQueue(struct WlanConnection *connection, unsigned int a, unsigned int b,
		       unsigned int maxStations, uint8_t *table);			/* 0x2b4c0 */
int fmCoexRecordBuffer(struct WlanConnectionInfo *info, const uint8_t *element, uint16_t *status);	/* 0x4bf6a */
void rfCalibAdjustQueue(struct WlanConnection *connection, const WlanIePointers *elements,
			unsigned int channel, uint8_t *erpInfo, uint8_t *olbcActive,
			unsigned int beaconPeriod, int add);			/* 0x2b45c */
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
int macMgmtUpdatePacket(struct WlanConnection *connection, const uint8_t *wps, const void *rsn,
			const void *wpa, const uint8_t *wapi, uint16_t *status)
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

/*
 * An association (or reassociation) request, answered with the response frame built here; status
 * is a refusal the caller has decided on already. The request is refused (the response goes out
 * through macMgmtHandlePacket(), which lets the peer go) when it is not for this BSS, when
 * the peer's security elements do not fit the BSS's, when the TKIP countermeasures are on, when the
 * rates do not fit, when HT is used with TKIP, or when no association ID or P2P slot is left. Else the
 * peer is associated: the BSS's elements go into the response, the peer's HT, ERP, preamble and
 * slot time capabilities are counted in the BSS (and the 20/40 coexistence managed when the first
 * 40 MHz intolerant peer comes or the last one goes), the host is told in event 45 with the request,
 * the response's fixed part and, for WAPI, the WAPI context.
 */
int macMgmtConfigurePacket(struct WlanConnection *connection, const WlanWirelessFrame *request,
			   unsigned int status)
{
	struct WlanConnectionRole *role = wlanConnectionInfoSlot(connection);
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	const uint8_t *peerAddress = connection->peerAddress;
	OsaBuffer *response = wlanMgmtAllocFrame(connection, 0x190);
	WlanWirelessFrame *frame;
	uint8_t *body;
	OsaBuffer *event;
	struct WlanHostEventFrame *eventFrame;
	WlanIePointers ies;
	uint8_t rates[14];
	uint8_t ratesCount = 0;
	uint8_t extendedCount;
	uint16_t code;
	const uint8_t *elements;
	int length;
	int fresh = 0;
	uint8_t previous0;
	uint8_t previous1;

	if (response == NULL) {
		macMgmtDispatchBuffer(connection, peerAddress, 5);
		return -1;
	}
	frame = (WlanWirelessFrame *)((uint8_t *)response + response->dataOffset);
	wlanMgmtBuildFrame(response, connection->interfaceAddress, peerAddress, 1, connection->localAddress);
	body = frame->body;
	body[2] = 0;
	body[3] = 0;
	fmCoexComputeTable(frame, request, &elements, &length);
	if (status != 0) {
		PUT_U16(body + 2, status);
		return macMgmtHandlePacket(response, 0);
	}
	length = wlanParseIes(elements, length, &ies);
	if (fmCoexCheckTable(frame, &ies, length, connection->networkSsid, connection->networkSsidLength) != 0)
		return macMgmtHandlePacket(response, 0);
	if (macMgmtUpdatePacket(connection, ies.pWps, ies.pRsn, ies.pWpa, ies.pWapi, &code) == -1) {
		PUT_U16(body + 2, code);
		return macMgmtHandlePacket(response, 0);
	}
	if (info->apMicError.disableStaAsso != 0)
		goto denied;
	if (fmCoexValidateTable(frame, &ies, info->rates, rates, &ratesCount, &extendedCount) != 0 ||
	    (info->htEnabled != 0 &&
	     fmCoexConfigureTable(frame, request, &ies, &role->keyMgmt.staUcstCipher, info->htCapability) != 0))
		return macMgmtHandlePacket(response, 0);
	if (extendedCount != 0)
		wlanAppendIe(frame, 0x32, rates + 8, extendedCount);
	if (info->htEnabled == 1 && ies.pHtCap != NULL)
		wlanAppendIe(frame, 0x3d, info->htOperation + 2, 22);
	if (info->securityMode.wapi)
		wlanAppendIe(frame, 0x44, ies.pWapi + 2, ies.pWapi[1]);
	if (info->htEnabled == 1 && ies.pHtCap != NULL) {
		if (info->phyMode != 2 && (info->htCapability[2] & 0x02))
			wlanAppendIe(frame, 0x4a, &info->obssScan.parameters, 14);
		wlanAppendIe(frame, 0x7f, info->extCapability.capability, 4);
	}
	if (info->qosEnabled == 1 && ies.pWmmInfo != NULL)
		wlanAppendIe(frame, 0xdd, info->wmmElement + 2, 24);
	if (ies.pVendor != NULL && wlanP2pConnection(connection) != 0) {
		struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

		ps->p2pStatus.value = ps->p2pAuthCount >= 8 ? 3 : 0;
		ps->p2pStatus.length[0] = 1;
		ps->p2pStatus.length[1] = 0;
		length = (uint8_t)p2pDirectCheckContext(connection, frame, NULL,
							(((ps->p2pOptions >> 1) & 1) << 8) + 1, 0);
		PUT_U16(frame->bodyLength, U16(frame->bodyLength) + length);
	}
	if (connection->associationId == 0) {
		macMgmtRecordBuffer(connection);
		fresh = 1;
		if (connection->associationId == 0)
			goto denied;
	}
	if (U16(body + 2) == 0 && wlanP2pConnection(connection) != 0) {
		struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

		if (ies.pVendor != NULL) {
			if (ps->p2pAuthCount >= 8)
				goto denied;
			role->p2pContext = p2pDirectReleaseContext(connection, request);
		} else if (++ps->p2pAuthCount > 8) {
			ps->p2pAuthCount--;
			goto denied;
		}
	}
	wlanCustomIeApply(connection, 1, frame, 6);
	connection->flags &= ~(WLAN_CONN_ENCRYPTED | WLAN_CONN_JOIN_OPEN);
	if (info->securityMode.noRsn) {
		connection->flags |= WLAN_CONN_ENCRYPTED | WLAN_CONN_JOIN_OPEN;
	} else if (info->securityMode.wepStatic) {
		if (info->uapContext->params.control & 1)
			connection->flags |= WLAN_CONN_ENCRYPTED | WLAN_CONN_JOIN_OPEN;
	} else if ((info->securityMode.wpa || info->securityMode.wpa2) &&
		   wlanInterfaceNeedsSecurity(connection) != 0) {
		if (connection->keyBlocks[1] != NULL)
			__aeabi_memclr4(connection->keyBlocks[1] + 28, 84);
		else
			connection->keyBlocks[1] = (uint8_t *)uapSqramUpdateContext(0);
	}
	if (info->qosEnabled == 1 &&
	    (ies.pWmmInfo != NULL || (info->htEnabled == 1 && ies.pHtCap != NULL))) {
		wlanConnectionSetActiveFlag(connection, 1);
		if (ies.pWmmInfo != NULL) {
			role->uapsd.qosInfo = ies.pWmmInfo[8];
			role->uapsd.triggerEnabled = ies.pWmmInfo[8];
			role->uapsd.deliveryEnabled = ies.pWmmInfo[8];
			connection->qosPeer = info->uapsdEnabled == 1 && (ies.pWmmInfo[8] & 0x0f) != 0;
		}
	} else {
		wlanConnectionSetActiveFlag(connection, 0);
	}
	PUT_U16(body + 4, connection->associationId | 0xc000);
	role->state = 3;
	role->peerPowerSave = 0;
	wlanConnectionUpdateListenInterval(connection);
	body[2] = 0;
	body[3] = 0;

	/*
	 * The BSS counts its HT peers by kind; a peer's earlier capabilities (the connection's, still
	 * those of its last association) are taken out of the counts and the new ones put in.
	 */
	if (info->htEnabled == 1) {
		int htNow = ies.pHtCap != NULL;
		int htWas = (connection->flags & WLAN_CONN_HT) != 0;
		int greenfieldWas = (connection->htCapability[2] & 0x10) != 0;
		int widthWas = (connection->htCapability[2] & 0x02) != 0;
		int intolerantWas = (connection->htCapability[3] & 0x40) != 0;

		if (!htNow) {
			if (fresh == 1 || htWas)
				info->htMixedStations++;
		} else if (fresh == 0 && !htWas) {
			info->htMixedStations--;
		}
		if (fresh == 0 && !greenfieldWas && htWas && (!htNow || fmCoexQueryTable(&ies)))
			info->htNonGreenfieldStations--;
		if (htNow && !fmCoexQueryTable(&ies) && (fresh == 1 || greenfieldWas || !htWas))
			info->htNonGreenfieldStations++;
		if (!widthWas && htWas && fresh == 0 && (!htNow || fmCoexResetTable(&ies)))
			info->ht20MhzStations--;
		if (htNow) {
			if (!fmCoexResetTable(&ies) && (fresh == 1 || !htWas || widthWas))
				info->ht20MhzStations++;
			if ((ies.pHtCap->capabilities & WLAN_HT_CAP_40MHZ_INTOLERANT) &&
			    (fresh == 1 || (htWas && !intolerantWas))) {
				info->ht40IntolerantStations++;
				if (role->info->ht40IntolerantStations == 1)
					macMgmtManagePacket(connection, 0);
			}
		}
		if (fresh == 0 && htWas && intolerantWas && htNow &&
		    !(ies.pHtCap->capabilities & WLAN_HT_CAP_40MHZ_INTOLERANT)) {
			info->ht40IntolerantStations--;
			if (role->info->ht40IntolerantStations == 0)
				macMgmtManagePacket(connection, 1);
		}
		if (htNow) {
			fmCoexParseTable(&ies, info->htCapability);
			connection->flags = (connection->flags & 0x7f) | (fmCoexResetTable(&ies) << 7) |
					    WLAN_CONN_HT | WLAN_CONN_HT_ENABLED;
			__aeabi_memcpy(connection->htCapability, ies.pHtCap, 28);
		}
	}
	wlanSecurityRefresh(connection);
	{
		uint32_t rateSet = wlanRateBitmapAll(rates, ratesCount, NULL, 0,
						     (connection->flags & WLAN_CONN_HT) ?
						     (const WlanHtCapabilityIe *)connection->htCapability : NULL);

		connection->supportedRatesLength = ratesCount;
		__aeabi_memcpy4(connection->supportedRates, rates, 14);
		wlanPsReleasePacket(connection, rateSet);
	}
	macTxNotifyApplyFrame(connection);
	macTxNotifyQueryFrame(connection);
	macTxNotifyDispatchFrame(connection);
	previous0 = connection->bssCapability[0];
	previous1 = connection->bssCapability[1];
	if (info->phyMode == 2) {
		role->erpCapable = 2;
	} else {
		/* Peers without an OFDM rate, without the short preamble and without the short slot time are counted. */
		if (fmCoexBuildTable(&ies)) {
			if (fresh == 0 && role->erpCapable == 0 && --info->erpInfo == 0)
				wlanApErpUpdate(connection);
			role->erpCapable = 1;
		} else {
			if ((fresh == 1 || role->erpCapable != 0) && ++info->erpInfo == 1)
				wlanApErpUpdate(connection);
			role->erpCapable = 0;
		}
		if (fmCoexPrepareTable(request)) {
			if (fresh == 0 && !(previous0 & 0x20) && --info->nonErpStations == 0)
				wlanApUpdateProtection(connection);
		} else if ((fresh == 1 || (previous0 & 0x20)) && ++info->nonErpStations == 1) {
			wlanApUpdateProtection(connection);
		}
		if (!fmCoexDispatchTable(request)) {
			if ((fresh == 1 || (previous1 & 0x04)) && ++info->longSlotStations == 1)
				wlanApUpdateSlotTime(connection);
		} else if (fresh == 0 && !(previous1 & 0x04) && --info->longSlotStations == 0) {
			wlanApUpdateSlotTime(connection);
		}
	}
	connection->bssCapability[0] = request->body[0];
	connection->bssCapability[1] = request->body[1];
	body[0] = info->capability[0];
	body[1] = info->capability[1];
	if (wlanTxAllocFrame(&event, (WlanTxDescriptor **)&eventFrame, 0x190, 45) != 0) {
		uint8_t *start = (uint8_t *)eventFrame + 24;
		uint8_t *cursor = start;

		cursor += fmCoexAdjustBuffer(info, cursor);
		cursor += wlanTlvFrameDataAppend(cursor, (const uint8_t *)request, U16(request->bodyLength));
		cursor += wlanTlvFrameDataAppend(cursor, (const uint8_t *)frame, 6);
		eventFrame->length = (uint16_t)(cursor - start);
		wlanHostEventSendWithAddress(connection, peerAddress, event);
	}
	return wlanMgmtSendFrame(response);

denied:
	PUT_U16(body + 2, WLAN_STATUS_ASSOC_DENIED);
	return macMgmtHandlePacket(response, 0);
}

/* The WAPI information element of the BSS (ID 68): its version, the one suite of each kind and no capabilities. Returns its size, 0 when the BSS is not a WAPI one. */
unsigned int fmCoexSetupBuffer(const struct WlanConnectionInfo *info, uint8_t *ie, unsigned int size)
{
	uint32_t oui = 0x721400;
	uint8_t *cursor = ie;

	if (ie != NULL && info->wapiConfigured != 0 && size >= 24) {
		__aeabi_memclr(ie, size);
		ie[0] = 68;
		ie[1] = 0;
		ie[2] = info->wapiVersion;
		ie[4] = 1;
		cursor = ie + 6;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapiAkm;
		cursor++;
		*cursor = 1;
		cursor += 2;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapiUnicastCipher;
		cursor++;
		__aeabi_memcpy(cursor, &oui, 3);
		cursor += 3;
		*cursor = info->wapiMulticastCipher;
		cursor += 3;
		ie[1] = (uint8_t)(cursor - ie - 2);
	}
	return (uint8_t)(cursor - ie);
}

unsigned int fmCoexSelectBuffer(const struct WlanConnectionInfo *info, uint8_t *ie)
{
	if (info->securityMode.wapi)
		return fmCoexSetupBuffer(info, ie, 0xff);
	return 0;
}

/* The WAPI context goes to the host as TLV 103 (byte-reversed); a BSS without WAPI adds nothing. Returns the TLV's size. */
unsigned int fmCoexAdjustBuffer(const struct WlanConnectionInfo *info, uint8_t *tlv)
{
	uint8_t data[16];

	if (!info->securityMode.wapi)
		return 0;
	tlv[0] = 103;
	tlv[1] = 1;
	tlv[2] = 16;
	tlv[3] = 0;
	if (info->wapiContext != NULL) {
		wlanReverseBytes(data, info->wapiContext->data, 16);
		__aeabi_memcpy(tlv + 4, data, 16);
	} else {
		__aeabi_memclr(tlv + 4, 16);
	}
	return U16(tlv + 2) + 4;
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

/* No peer is associated, no access point BSS is started and the P2P group owner's connection (if there is one) is free. */
int macMgmtSchedulePacket(void)
{
	int idle = wlanAnyConnectionFlags2Bit4() == 0;
	struct WlanConnection *connection;
	struct WlanConnection *owner;

	for (connection = wlanConnectionFirst(2); idle && connection != NULL;
	     connection = wlanConnectionNextFlagged(connection, connection->interfaceType)) {
		if (wlanConnectionBssStarted(connection) == 1) {
			idle = 0;
			break;
		}
	}
	owner = p2pDirectProcessEntry(2);
	if (idle && owner != NULL)
		return owner->state == 0;
	return idle;
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
	if (info->ht2040Managed != 0 || !(info->htCapability[2] & 0x02)) {
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

	rfCalibRecordQueue(connection, a, b, info->maxStations, info->tim);
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

/*
 * Whether the BSS may use its 40 MHz channel width (want == 1) or has to fall back to 20 MHz
 * (want == 0), when the access point manages 20/40 coexistence itself: the HT Information
 * element's STA channel width and secondary channel offset change, and the beacon and the
 * associated peers are told.
 */
void macMgmtManagePacket(struct WlanConnection *connection, int want)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int posture;
	uint8_t *operation;

	wlanConnectionInfoSlot(connection);
	if (info->ht2040Managed == 0 || !(info->htCapability[2] & 0x02))
		return;
	posture = sysIrqDisable();
	if (want == 1) {
		if (info->olbcTimeout != 0 || info->ht40IntolerantStations != 0 ||
		    ((info->rateFlags & WLAN_RATE_FLAG_HT_OBSS) && info->phyMode != 2)) {
			sysIrqRestore(posture);
			return;
		}
	} else if (want == 0) {
		struct WlanConnectionInfo *same = wlanConnectionInfo(connection);

		same->olbcTimeout = (1000000 * same->obssScan.parameters.triggerScanInterval * same->obssScan.parameters.widthTransitionDelayFactor) /
				    ((unsigned int)connection->beaconPeriod << 10) + 1;
	}
	sysIrqRestore(posture);
	operation = info->htOperation;
	if (((operation[3] >> 2) & 1) != want && (info->htCapability[2] & 0x02)) {
		struct WlanConnection *peer = wlanConnectionFind(connection->bssType, connection->bssNumber);
		struct WlanConnectionRole *role = wlanConnectionInfoSlot(peer);

		operation[3] = (operation[3] & 0xfb) | ((4 * want) & 7);
		if (want == 1)
			operation[3] = ((operation[3] >> 2) << 2) | ((role->channelFlags >> 4) & 3);
		else
			operation[3] = (operation[3] >> 2) << 2;
		macMgmtReleaseFrame(connection);
		while (peer != NULL) {
			if (peer->flags2 & WLAN_CONN2_ASSOCIATED) {
				wlanPsAcquirePacket(peer);
				macTxNotifyApplyFrame(peer);
			}
			peer = wlanConnectionNextByBss(peer, peer->bssType, peer->bssNumber);
		}
	}
}

/* The count of the HT overlapping BSS timer runs down; at zero the flag goes and the BSS may widen. */
void macMgmtAcquirePacket(struct WlanConnection *connection, uint8_t *counter, uint8_t *erpInfo)
{
	if (*counter != 0 && --*counter == 0) {
		erpInfo[3] &= ~WLAN_RATE_FLAG_HT_OBSS;
		wlanSecurityRefresh(connection);
		macMgmtManagePacket(connection, 1);
	}
}

/*
 * A beacon of another BSS (the body, length bytes): when it says legacy or non-HT stations are around, the
 * HT overlapping BSS flag is set (it goes after 1 s of beacons without) and the BSS narrows to 20 MHz.
 */
void macMgmtReleasePacket(struct WlanConnection *connection, const uint8_t *body, uint16_t length,
			  unsigned int beaconPeriod, uint8_t *erpInfo, uint8_t *counter, unsigned int seconds)
{
	WlanIePointers elements;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int was = (erpInfo[3] >> 5) & 1;

	wlanParseIes(body + 12, (uint16_t)(length - 12), &elements);
	if (elements.pErpInfo != NULL ? !(elements.pErpInfo[2] & 1) : info->phyMode == 2) {
		if (elements.pHtInfo != NULL && !(elements.pHtInfo[4] & 0x10))
			return;
	}
	erpInfo[3] |= WLAN_RATE_FLAG_HT_OBSS;
	if (((erpInfo[3] >> 5) & 1) != was)
		wlanSecurityRefresh(connection);
	*counter = (seconds * 1000000) / (beaconPeriod << 10) + 1;
	if (info->phyMode != 2)
		macMgmtManagePacket(connection, 0);
}

void macMgmtSetupPacket(OsaBuffer *buffer)
{
	struct WlanConnection *connection = buffer->context;
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

	if (info->htEnabled != 0)
		macMgmtReleasePacket(connection, frame + 32, frame[1] << 8 | frame[0], connection->beaconPeriod,
				     &info->erpInfo, &info->olbcCount, 1);
}

/* The 20/40 timer of the BSS ticks. */
void macMgmtClearPacket(struct WlanConnection *connection)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	unsigned int posture = sysIrqDisable();

	if (info->olbcTimeout != 0 && --info->olbcTimeout == 0)
		macMgmtManagePacket(connection, 1);
	sysIrqRestore(posture);
}

void macMgmtConvertPacket(struct WlanConnection *connection)
{
	wlanConnectionInfo(connection)->uapBusy = 0;
}

/*
 * A probe request: 1 when this BSS is to answer it (its SSID, or a broadcast SSID when allowed, and the BSS's
 * address or the broadcast address as the BSSID and the address it is sent to).
 */
int macMgmtVerifyPacket(struct WlanConnection *connection, const uint8_t *frame)
{
	int answer = 0;
	int local = 0;
	struct WlanConnection *peer = wlanConnectionFindByPeer(frame + 12);

	if (frame[33] != 0) {
		unsigned int length = connection->networkSsidLength;

		if (frame[33] != length || memcmp(frame + 34, connection->networkSsid, length) != 0)
			goto done;
		answer = 1;
		if (peer == NULL ||
		    (!(peer->flags2 & WLAN_CONN2_ASSOCIATED) && connection->bssType == peer->bssType &&
		     connection->bssNumber == peer->bssNumber))
			local = 1;
	} else {
		if (!(wlanConnectionInfo(connection)->protectionFlags & 2))
			goto done;
		answer = 1;
	}
	if (memcmp(frame + 18, connection->localAddress, 6) == 0 || memcmp(wlanBroadcastAddress, frame + 18, 6) == 0) {
		if (memcmp(connection->localAddress, frame + 6, 6) == 0)
			local = 1;
	} else {
		answer = 0;
	}
done:
	if (wlanApPsEnabled() && local == 1 && answer == 1)
		wlanPsActivity(wlanConnectionByRole(2));
	return answer;
}

/* The transmit request of a probe response is made from the BSS's template's. */
void macMgmtSelectPacket(struct WlanConnection *connection, OsaBuffer *buffer)
{
	struct WlanMacTxRequest *request = (struct WlanMacTxRequest *)((uint8_t *)buffer + 48);
	const struct WlanMacTxRequest *template = connection->probeResponseRequest;

	buffer->dataOffset = 28;
	__aeabi_memclr(request, 0x40);
	__aeabi_uwrite4(__aeabi_uread4(template->expiry), request->expiry);
	__aeabi_uwrite4(__aeabi_uread4(template->expiry + 4), request->expiry + 4);
	request->reserved1[1] = 0;
	request->reserved1[2] = template->reserved1[2];
}

/* A probe response for the peer (a probe request's address) goes out from the BSS's template; 0: sent. */
int macMgmtAdjustPacket(struct WlanConnection *connection, const uint8_t *peer)
{
	struct WlanConnectionInfo *info = NULL;
	OsaBuffer *buffer;
	uint8_t *data;
	int result;

	if (connection->interfaceType != 4)
		info = wlanConnectionInfo(connection);
	buffer = connection->probeResponseBuffer;
	buffer->context = connection;
	buffer->flags &= ~1;
	data = (uint8_t *)buffer + 176;
	__aeabi_memcpy(data + 6, peer, 6);
	data[3] &= 0x97;
	macMgmtSelectPacket(connection, buffer);
	if (connection->interfaceType == 4)
		connection->transmitBusy = 1;
	else
		info->uapBusy = 1;
	result = wlanMgmtSendFrame(buffer);
	if (result != 0) {
		if (connection->interfaceType == 4)
			connection->transmitBusy = 0;
		else
			info->uapBusy = 0;
	}
	return result;
}

/* A probe request is received: the access point or P2P group owner answers it, and the AMP link does when it is looking for its peer. */
void macMgmtRecordPacket(struct WlanConnection *connection, OsaBuffer *buffer, uintptr_t argument)
{
	const uint8_t *frame = (const uint8_t *)buffer + buffer->dataOffset;

	if (connection->interfaceType == 3 || connection->interfaceType == 2) {
		struct WlanConnectionInfo *info = wlanConnectionInfo(connection);

		if (connection->interfaceType == 3) {
			if (connection->mode == 1 || connection->mode == 2) {
				p2pSqramMatchDirectSsid(connection, frame, argument);
				return;
			}
		} else if (connection->interfaceType == 2 && macMgmtVerifyPacket(connection, frame) == 1) {
			if (info->uapBusy == 1)
				return;
			if (macMgmtAdjustPacket(connection, frame + 12) == 0 &&
			    (info->mgmtForwardMask & (1u << (frame[2] >> 4))) != 0)
				wlanSubmitControlFrame(connection, frame);
		}
	}
	if (frame[33] == 0)
		connection = wlanConnectionFirst(4);
	if (connection != NULL && connection->interfaceType == 4 &&
	    (connection->transmitBusy != 1 || connection->state != 0))
		macMgmtAdjustPacket(connection, frame + 12);
}

/* Every connection of an interface type that is in a BSS role of its own is sent the event. */
void macMgmtFilterPacket(int code, unsigned int interfaceType)
{
	struct WlanConnection *connection;

	for (connection = wlanConnectionFirst(interfaceType); connection != NULL;
	     connection = wlanConnectionNextFlagged(connection, interfaceType)) {
		if (connection->mode == 2)
			wlanReportEvent(connection, NULL, code);
	}
}
