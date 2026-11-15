/*
 * IBSS (ad-hoc) networks (0x10d48..0x11a34): the beacon and probe response
 * templates the MAC sends, starting an IBSS from the host's AD_HOC_START,
 * joining one from AD_HOC_JOIN, the beacons of the IBSS's peers, and
 * adopting the BSSID and beacon timing of a network that merges with ours. The filename is inferred; no source
 * filename survives.
 */
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/hostps.h"
#include "robin/esa.h"
#include "robin/ibss.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/protection.h"
#include "robin/pspoll.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"
#include "robin/htba.h"
#include "robin/peerrate.h"

#define LE16(bytes)	((uint16_t)((bytes)[0] | ((bytes)[1] << 8)))

/* The templates' frames start this far into their buffers. */
#define WLAN_IBSS_TEMPLATE_OFFSET	176

/* The high byte of the capability information: Spectrum Management. */
#define WLAN_CAPABILITY_SPECTRUM_MANAGEMENT 0x0100

/* HT Information element, byte 4 (operating mode): Non-Greenfield STAs Present. */
#define WLAN_HT_INFO_NON_GREENFIELD	0x04

/* The timestamp, beacon interval and capability ahead of a beacon's elements. */
#define WLAN_BEACON_FIXED_LENGTH	12

struct WlanIbssInfo wlanIbssInfo __attribute__((section(".itcm.bss")));

static const uint8_t wlanIbssBroadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Not converted yet; provisional names. */
extern OsaBuffer *wlanIbssBeaconBuffer;				/* 0x54c20 */
extern OsaBuffer *wlanIbssProbeResponseBuffer;			/* 0x54c24 */
extern WlanWirelessFrame *wlanIbssProbeResponseFrame;		/* 0x54c28 */
extern WlanWirelessFrame *wlanIbssBeaconFrame;			/* 0x54c2c */
extern struct WlanMacTxRequest *wlanIbssBeaconRequest;		/* 0x54bc4 */
extern struct WlanMacTxRequest *wlanIbssProbeResponseRequest;	/* 0x54bc8 */
extern uint8_t *wlanBeaconRatesEnd;				/* 0x54a9c */
extern uint8_t *wlanProbeRatesEnd;				/* 0x54aa0 */
extern uint8_t wlanHtOperationIe[];				/* SQRAM 0xc0031f00 */
void cbProcTrackPacket(void);						/* 0x1e570 */
int macMgmtRecordTable(const uint8_t *address, uint8_t *buffer);	/* 0x37d02 */
uint32_t rfCalibInspectTimer(unsigned int rate);			/* 0x2bc6c */
void wlanPsFilterFrame(uint16_t beaconPeriod);				/* 0x3dc4e */
void macMgmtProcessContext(struct WlanConnection *connection, uint8_t *channel,
			   unsigned int number);			/* 0x355b0 */
void macMgmtResolveEntry(struct WlanConnection *connection, unsigned int channel,
			 int start);					/* 0x36a32 */
int cbProcAdjustEntry(struct WlanConnection *connection);		/* 0x1cbce */
int macMgmtQueryState(uint32_t flags, uint32_t extra);			/* 0x34b82 */
void btLmpLcHandleState(int state);					/* 0x42488 */
void macMgmtPrepareDescriptor(uint16_t atimWindow);			/* 0x3bc5a */
uint16_t macMgmtDispatchDescriptor(void);				/* 0x3bc6e */
void wlanPsClearTable(struct WlanConnection *connection, int value);	/* 0x3c03c */
void wlanPsAdjustTable(struct WlanConnection *connection);		/* 0x3c0aa */
void cbProcUpdatePacket(struct WlanConnection *connection, unsigned int rates); /* 0x1e220 */
void cbProcParsePacket(struct WlanConnection *peer);			/* 0x1e378 */
void wlanPsFilterTable(struct WlanConnection *connection);		/* 0x3c108 */
struct WlanConnection *cbProcResetBuffer(const uint8_t *ies);		/* 0x1ddd6 */
void macMgmtDispatchEntry(const uint8_t *rsn, const uint8_t *wpa, uint8_t *third,
			  uint8_t *fourth, uint32_t a, uint32_t b, uint32_t c);	/* 0x364be */
void wlanPsQueryPacket(void);						/* 0x3d006 */
extern uint32_t wlanIbssTsfOffset;					/* 0x54a98 */

/* Sets the ERP element of a template to the protection the BSS state asks for. */
void wlanIbssTemplateSetProtection(WlanWirelessFrame *frame, int protection)
{
	WlanIePointers ies;

	wlanConnectionHtOperation(wlanConnectionFindByTypeNumber(WLAN_IF_IBSS, 0));
	wlanParseIes(frame->body + WLAN_BEACON_FIXED_LENGTH,
		     LE16(frame->bodyLength) - WLAN_BEACON_FIXED_LENGTH, &ies);
	wlanSetErpProtection(&ies, &wlanProtection, wlanHtOperationIe, protection);
}

/* Both templates protect while a DSSS-only station is around. */
void wlanIbssUpdateProtection(void)
{
	int protection;

	if (!wlanConnectionExists(WLAN_IF_IBSS) || (wlanProtection.flags & WLAN_PROTECTION_DSSS_BSS))
		return;
	protection = (int32_t)__aeabi_uread4(
		wlanProtection.timers[WLAN_PROTECTION_DSSS_ONLY].remaining) > 0;
	wlanIbssTemplateSetProtection(wlanIbssBeaconFrame, protection);
	wlanIbssTemplateSetProtection(wlanIbssProbeResponseFrame, protection);
}

/* Builds the beacon (subtype 8) or probe response template in buffer and its MAC request. */
void wlanIbssBuildTemplate(struct WlanConnection *connection, const WlanAdHocStart *start,
			   const uint8_t *bssid, unsigned int subtype, OsaBuffer *buffer,
			   struct WlanMacTxRequest *request)
{
	WlanWirelessFrame *frame;
	uint8_t *cfStart;
	uint8_t *copyStart;
	uint8_t *end;
	uint32_t flags;
	unsigned int rate;
	unsigned int bodyOffset;
	unsigned int length;
	uint16_t bodyLength;
	int nonErp;
	int useProtection;
	int barker;

	macMgmtPrepareContext((uint8_t *)&flags, NULL);
	rate = wlanTxControlRate(connection);
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	end = wlanBuildIbssBeacon(start, connection->interfaceAddress, bssid, subtype,
				  wlanLocalState->powerSave, buffer);
	bodyOffset = (uint16_t)(end - (uint8_t *)frame);
	if (subtype == WLAN_FRAME_BEACON)
		wlanBeaconRatesEnd = end;
	else
		wlanProbeRatesEnd = end;
	end = wlanPutPhyParamSet(end, start);
	cfStart = end;
	__aeabi_memcpy(end, start->ssParamSet, 4);
	end += 4;
	wlanSetSlotTime((start->capability & 0x20) != 0);
	if (wlanDomainApplies(connection) == 1)
		end += wlanDomainBuildIe(connection, end);
	if (!(flags & 3) && !(wlanProtection.flags & WLAN_PROTECTION_DSSS_BSS)) {
		barker = 0;
		nonErp = 0;
		useProtection = 0;
		if (__aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_DSSS_ONLY].remaining)) {
			nonErp = 1;
			useProtection = 1;
		}
		if ((connection->txOptions & 0x20) ||
		    __aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_USE_ERP].remaining))
			useProtection = 1;
		if (__aeabi_uread4(wlanProtection.timers[WLAN_PROTECTION_NO_SHORT_PREAMBLE].remaining))
			barker = 1;
		end += wlanPutErpIe(end, nonErp, useProtection, barker);
	}
	end += wlanHtElementGet(connection, 45, end);
	copyStart = end;
	if (connection->flags & 4)
		end += wlanCopyIes(IE_RSN, copyStart, subtype, (uint8_t *)frame, bodyOffset,
				   start->extraIes, start->extraIesLength);
	end += wlanPutRatesIe(start->dataRate, end, 1);
	end += wlanHtElementGet(connection, 61, end);
	end += wlanHtElementGet(connection, 72, end);
	end += wlanHtElementGet(connection, 74, end);
	if (wlanConnectionIsHt(connection)) {
		wlanPutWmmParameterIe(&end);
		cbProcTrackPacket();
	}
	end += macMgmtRecordTable(connection->interfaceAddress, end);
	end += wlanCopyIes(IE_VENDOR, end, subtype, (uint8_t *)frame, bodyOffset,
			   start->extraIes, start->extraIesLength);

	wlanTxRequestReset(request);
	__aeabi_uwrite4(0, request->flags);
	__aeabi_uwrite4(rfCalibInspectTimer(rate), request->rateInfo);
	__aeabi_uwrite4((int)frame, request->frame);
	length = (uint16_t)(end - (uint8_t *)frame - sizeof(*frame));
	frame->bodyLength[0] = length;
	frame->bodyLength[1] = length >> 8;
	wlanCustomIeApply(connection, 1, frame, WLAN_BEACON_FIXED_LENGTH);
	end += LE16(frame->bodyLength) - length;
	request->cfOffset = cfStart - (uint8_t *)frame - 8;
	request->timOffset = end - (uint8_t *)frame - 8;
	bodyLength = LE16(frame->bodyLength) + 28;
	request->templateLength[0] = bodyLength;
	request->templateLength[1] = bodyLength >> 8;
	if (subtype == WLAN_FRAME_BEACON)
		wlanBeaconTemplateEnd = end;
	else
		wlanProbeTemplateEnd = end;
	__aeabi_memcpy(frame->address1, wlanIbssBroadcast, 6);
}

void wlanIbssSetInfo(struct WlanConnection *connection, uint16_t beaconPeriod,
		     uint16_t atimWindow)
{
	__aeabi_memclr(&wlanIbssInfo, sizeof(wlanIbssInfo));
	if (connection->txOptions & 0x20)
		wlanIbssInfo.flags |= WLAN_IBSS_PROTECTION;
	else
		wlanIbssInfo.flags &= ~WLAN_IBSS_PROTECTION;
	__aeabi_memcpy(wlanIbssInfo.bssid, connection->localAddress, 6);
	wlanIbssInfo.beaconPeriod = beaconPeriod;
	wlanIbssInfo.atimWindow = atimWindow;
}

/* Builds both templates and hands them to the MAC. */
static inline __attribute__((always_inline)) void
wlanIbssSetTemplates(struct WlanConnection *connection, const WlanAdHocStart *start,
		     const uint8_t *bssid)
{
	OsaBuffer *buffer;

	buffer = connection->beaconBuffer;
	wlanIbssBeaconBuffer = buffer;
	buffer->dataOffset = WLAN_IBSS_TEMPLATE_OFFSET;
	wlanIbssBeaconFrame = (WlanWirelessFrame *)((uint8_t *)buffer + WLAN_IBSS_TEMPLATE_OFFSET);
	wlanIbssBeaconRequest = connection->beaconRequest;
	wlanIbssBuildTemplate(connection, start, bssid, WLAN_FRAME_BEACON, buffer,
			      wlanIbssBeaconRequest);
	buffer = connection->probeResponseBuffer;
	wlanIbssProbeResponseBuffer = buffer;
	buffer->dataOffset = WLAN_IBSS_TEMPLATE_OFFSET;
	wlanIbssProbeResponseFrame =
		(WlanWirelessFrame *)((uint8_t *)buffer + WLAN_IBSS_TEMPLATE_OFFSET);
	wlanIbssProbeResponseRequest = connection->probeResponseRequest;
	wlanIbssBuildTemplate(connection, start, bssid, WLAN_FRAME_PROBE_RESPONSE, buffer,
			      wlanIbssProbeResponseRequest);
}

/* AD_HOC_JOIN: joins the IBSS the host chose; refused while templates exist. */
void wlanIbssJoin(struct WlanConnection *connection, const WlanAdHocBssDesc *join)
{
	WlanAdHocStart start;
	uint32_t flags;
	uint32_t extra;
	unsigned int channelFlags;
	unsigned int channel;
	int status;

	macMgmtPrepareContext((uint8_t *)&flags, &extra);
	if (wlanIbssBeaconFrame || wlanIbssProbeResponseFrame) {
		status = 1;
	} else {
		cbProcDispatchTable(9, 1);
		cbProcPrepareTable(9, 0);
		if (join->reserved[1]) {
			channelFlags = join->reserved[0];
			channel = join->reserved[1];
		} else {
			channelFlags = flags;
			channel = join->phyParamSet[2];
		}
		wlanPsVerifyFrame(3);
		macMgmtUpdateContext(channelFlags, channel);
		macMgmtCheckContext();
		wlanIbssJoinSetup(connection, join);
		wlanAdHocJoinToStart(&start, join);
		connection->beaconPeriod = start.beaconPeriod;
		if (start.capability & WLAN_CAPABILITY_SPECTRUM_MANAGEMENT)
			macMgmtResolveEntry(connection, channel, 0);
		if (cbProcAdjustEntry(connection))
			for (;;)
				;
		wlanIbssSetTemplates(connection, &start, join->bssid);
		wlanPsFilterFrame(start.beaconPeriod);
		cbProcDispatchTable(9, 2);
		SOC_MAC_EVENT_CONTROL |= 0x20;
		cbProcPrepareTable(9, 1);
		wlanIbssSetInfo(connection, join->beaconPeriod, LE16(&join->ssParamSet[2]));
		if (!(flags & 3))
			btLmpLcHandleState(macMgmtQueryState(flags, extra));
		status = 0;
	}
	wlanIbssJoinDone(connection, status);
}

/* AD_HOC_START: starts an IBSS with the host's parameters. */
void wlanIbssStart(struct WlanConnection *connection, const WlanAdHocStart *start)
{
	uint8_t channel[2];		/* channel flags, number */
	uint32_t flags;
	uint32_t extra;
	unsigned int count;
	unsigned int rates;
	int status;

	macMgmtPrepareContext((uint8_t *)&flags, &extra);
	if (wlanIbssBeaconFrame || wlanIbssProbeResponseFrame || start->bssMode != 2 ||
	    start->beaconPeriod == 0) {
		status = 1;
	} else {
		wlanPsVerifyFrame(3);
		connection->beaconPeriod = start->beaconPeriod;
		if (start->reserved2[1]) {
			channel[0] = start->reserved2[0];
			channel[1] = start->reserved2[1];
		} else {
			channel[0] = flags;
			channel[1] = start->phyParamSet[2];
		}
		macMgmtProcessContext(connection, channel, channel[1]);
		macMgmtUpdateContext(channel[0], channel[1]);
		macMgmtCheckContext();
		wlanAdHocSsid(start->ssid, connection->localAddress, connection->networkSsid,
			      &connection->networkSsidLength);
		count = romStringLength(start->dataRate, sizeof(start->dataRate));
		connection->supportedRatesLength = count;
		__aeabi_memcpy(connection->supportedRates, start->dataRate, count);
		rates = wlanRateBitmapAll(start->dataRate, count, NULL, 0,
					  (const WlanHtCapabilityIe *)connection->htCapability);
		macMgmtQueryContext(connection, start->dataRate, count, rates);
		macTxNotifyApplyFrame(connection);
		connection->interfaceType = WLAN_IF_IBSS;
		wlanIbssTimingStart(connection, start);
		if (supplicantIsEnabled() && (connection->flags & 4))
			supplicantInitSession(connection, connection->networkSsid,
					      connection->networkSsidLength,
					      (IEEEtypes_MacAddr_t *)connection->localAddress,
					      (IEEEtypes_MacAddr_t *)connection->interfaceAddress);
		if (start->capability & WLAN_CAPABILITY_SPECTRUM_MANAGEMENT)
			macMgmtResolveEntry(connection, channel[1], 1);
		wlanProtection.flags = (wlanProtection.flags & ~WLAN_PROTECTION_DSSS_BSS) |
				       ((wlanRatesDsssOnly(rates) & 1) << 6);
		if (cbProcAdjustEntry(connection))
			for (;;)
				;
		wlanIbssSetTemplates(connection, start, connection->localAddress);
		wlanPsFilterFrame(start->beaconPeriod);
		connection->bssCapability[0] = start->capability;
		connection->bssCapability[1] = start->capability >> 8;
		cbProcDispatchTable(8, 2);
		SOC_MAC_EVENT_CONTROL |= 0x20;
		cbProcPrepareTable(8, 1);
		wlanIbssSetInfo(connection, start->beaconPeriod, LE16(&start->ssParamSet[2]));
		if (!(flags & 3))
			btLmpLcHandleState(macMgmtQueryState(flags, extra));
		status = 0;
	}
	wlanIbssStartDone(connection, status);
}

/*
 * Takes over the BSSID and beacon timing of the IBSS a received beacon
 * belongs to (IBSS merge), with its ATIM window, ERP protection and HT
 * Non-Greenfield state.
 */
void wlanIbssAdopt(struct WlanConnection *connection, const WlanWirelessFrame *frame,
		   const uint8_t *ibssParameters, const uint8_t *erp, const uint8_t *htInformation)
{
	WlanWirelessFrame *beacon = wlanIbssBeaconFrame;
	uint8_t *ie;
	UINT state;

	__aeabi_memclr(&wlanIbssInfo, sizeof(wlanIbssInfo));
	__aeabi_memcpy(connection->localAddress, frame->address3, 6);
	state = sysIrqDisable();
	cbProcSelectBuffer(10, connection->localAddress);
	__aeabi_memcpy(beacon->address3, frame->address3, 6);
	__aeabi_memcpy(wlanIbssProbeResponseFrame->address3, frame->address3, 6);
	__aeabi_memcpy(wlanIbssInfo.bssid, frame->address3, 6);
	wlanIbssInfo.beaconPeriod = LE16(&frame->body[8]);
	wlanPsFilterFrame(wlanIbssInfo.beaconPeriod);
	if (ibssParameters) {
		wlanIbssInfo.atimWindow = LE16(&ibssParameters[2]);
		macMgmtPrepareDescriptor(LE16(&ibssParameters[2]));
	} else {
		wlanIbssInfo.atimWindow = macMgmtDispatchDescriptor();
	}
	if (!(connection->txOptions & 0x20) && erp && (erp[2] & WLAN_ERP_USE_PROTECTION)) {
		ie = wlanFindIe(wlanIbssBeaconFrame, IE_ERP_INFORMATION, WLAN_FRAME_BEACON, NULL, 0);
		if (ie) {
			connection->txOptions |= 0x20;
			ie[2] |= WLAN_ERP_USE_PROTECTION;
		}
	}
	if (htInformation && (htInformation[4] & WLAN_HT_INFO_NON_GREENFIELD))
		__aeabi_uwrite4(WLAN_PROTECTION_TIMEOUT,
				wlanProtection.timers[WLAN_PROTECTION_NON_GREENFIELD].remaining);
	MAC_300_MODE &= ~8u;
	SOC_MAC_TIMER = 0;
	MAC_604_TIMER_HIGH = 0;
	MAC_608_NEXT_BEACON = (uint32_t)wlanIbssInfo.beaconPeriod << 11;
	MAC_60C_NEXT_BEACON_HIGH = 0;
	MAC_618_BEACON_PERIOD = wlanIbssInfo.beaconPeriod;
	MAC_640_BEACON_WINDOW = 511;
	MAC_300_MODE |= 8;
	sysIrqRestore(state);
	connection->stationFlags |= 2;
	if (wlanIbssInfo.atimWindow) {
		wlanPsClearTable(connection, 0);
		if (wlanSessionId & 0x10)
			wlanPsStartHostModeTimer(connection);
	} else {
		wlanPsAdjustTable(connection);
		wlanPsHostModeExit();
	}
}

/*
 * A beacon of a peer in our IBSS (from the management task): the peer's
 * rates, WMM and DSSS-only state, and a merge with the peer's network when
 * its BSSID differs.
 */
void wlanIbssBeaconReceived(struct WlanConnection *connection, const WlanWirelessFrame *frame,
			    const void *unused, const WlanIePointers *ies, unsigned int peerRates)
{
	WlanProtectionTimer *link = &wlanProtection.timers[WLAN_PROTECTION_DSSS_PEER_LINK];
	WlanProtectionTimer *noWmm = &wlanProtection.timers[WLAN_PROTECTION_NON_WMM_PEER];
	const uint8_t *bssid = frame->address3;
	struct WlanConnection *peer;
	uint8_t previous[6];
	unsigned int rates;
	uint32_t flags;
	int changed = 0;

	rates = wlanRateBitmapAll(connection->supportedRates, connection->supportedRatesLength, NULL,
				  0, (const WlanHtCapabilityIe *)connection->htCapability);
	flags = connection->stationFlags;
	if (!(flags & 0x40) || ((flags & 0x80) && !(connection->flags2 & 0x10))) {
		connection->stationFlags = flags | 0x40;
		connection->flags2 |= 0x10;
		cbProcUpdatePacket(connection, peerRates & rates);
		macTxNotifyQueryFrame(connection);
		macTxNotifyDispatchFrame(connection);
		wlanConnectionSetActiveFlag(connection, ies->pWmmParam != NULL);
		connection->flags |= 3;
		if (!(connection->stationFlags & 0x80))
			wlanHostEventSendAddress(connection, 32, connection->peerAddress);
		if ((connection->stationFlags & 1) &&
		    !wlanConnectionHasLocalAddress(connection, bssid)) {
			__aeabi_memcpy(previous, connection->localAddress, sizeof(previous));
			wlanIbssAdopt(connection, frame, ies->pIbssParam, ies->pErpInfo, ies->pHtInfo);
			while ((peer = wlanConnectionFindByLocal(WLAN_IF_IBSS, previous)) != NULL)
				__aeabi_memcpy(peer->localAddress, bssid, 6);
			wlanPostConnectionEvent(connection, 0x80000000u);
		}
		cbProcParsePacket(connection);
	} else if (!(flags & 1) || wlanConnectionHasLocalAddress(connection, bssid)) {
		cbProcParsePacket(connection);
	}
	if (wlanPsEnabled())
		wlanPsFilterTable(connection);
	if (peerRates <= 0xf) {
		wlanProtection.flags = (wlanProtection.flags & ~WLAN_PROTECTION_PEER_MASK) |
				       WLAN_PROTECTION_PEER_DSSS;
		__aeabi_uwrite4(WLAN_PROTECTION_TIMEOUT,
				wlanProtection.timers[WLAN_PROTECTION_DSSS_PEER].remaining);
		if (connection->stationFlags & 0x80) {
			if ((int32_t)__aeabi_uread4(link->remaining) <= 0)
				changed = 1;
			__aeabi_uwrite4(WLAN_PROTECTION_TIMEOUT, link->remaining);
		}
	} else if ((connection->stationFlags & 0x80) && (int32_t)__aeabi_uread4(link->remaining) < 0) {
		changed = 1;
		__aeabi_uwrite4(0, link->remaining);
	}
	if (connection->stationFlags & 0x80) {
		if (!ies->pWmmParam && !ies->pWmmInfo)
			__aeabi_uwrite4(WLAN_PROTECTION_TIMEOUT, noWmm->remaining);
		wlanConnectionSetActiveFlag(connection, (int32_t)__aeabi_uread4(noWmm->remaining) <= 0);
		if (changed) {
			cbProcUpdatePacket(connection, peerRates & rates);
			macTxNotifyQueryFrame(connection);
			macTxNotifyDispatchFrame(connection);
		}
	}
	wlanWepIcvErrRefresh(frame->address2, &wlanIbssIcvErrList);
}

/* Whether the station state machine is in one of its IBSS states. */
int wlanIbssSmActive(const struct WlanConnection *connection)
{
	return connection->smState == connection->smStates + 36 ||
	       connection->smState == connection->smStates + 60;
}

/* A peer was heard: its DSSS rates and state go to the rate and power-save code. */
void wlanConnectionTouch(struct WlanConnection *connection)
{
	unsigned int rates;

	connection->flags2 |= 0x10;
	rates = wlanRateBitmapAll(connection->supportedRates, connection->supportedRatesLength, NULL,
				  0, (const WlanHtCapabilityIe *)connection->htCapability);
	cbProcUpdatePacket(connection, rates & 0xf);
	macTxNotifyQueryFrame(connection);
	macTxNotifyDispatchFrame(connection);
	connection->flags |= 3;
	cbProcParsePacket(connection);
}

struct WlanIbssInfo *wlanIbssGetInfo(void)
{
	return &wlanIbssInfo;
}

/* After a merge, the MAC's kept beacons take the new ATIM window and beacon period. */
void wlanIbssRefreshBeacons(struct WlanConnection *connection)
{
	if (connection->interfaceType != WLAN_IF_IBSS)
		return;
	if (connection->stationFlags & 2) {
		wlanIbssRefreshBeacon((WlanBeaconDescriptor *const *)&MAC_004_REGION_B);
		wlanIbssRefreshBeacon((WlanBeaconDescriptor *const *)&MAC_008_REGION_C);
	}
	connection->stationFlags &= ~2u;
}

int wlanChannelValidate(void)
{
	cbProcDispatchTable(7, 2);
	SOC_MAC_EVENT_CONTROL |= 0x20;
	return cbProcPrepareTable(7, 1);
}

void wlanIbssReset(void)
{
	wlanBeaconRatesEnd = NULL;
	wlanProbeRatesEnd = NULL;
	__aeabi_memclr(&wlanIbssInfo, sizeof(wlanIbssInfo));
}

/* The IBSS of ours a received beacon names, on our channel. */
struct WlanConnection *wlanIbssMatchBeacon(const WlanWirelessFrame *frame, const uint8_t *body)
{
	struct WlanConnection *connection;
	const uint8_t *ds;

	if (!(body[10] & 2))		/* capability: IBSS */
		return NULL;
	connection = cbProcResetBuffer(body + WLAN_BEACON_FIXED_LENGTH);
	if (connection == NULL || !(connection->stationFlags & 1))
		return NULL;
	ds = wlanIeFind(IE_DS_PARAMETER, body + WLAN_BEACON_FIXED_LENGTH,
				LE16(frame->bodyLength) - WLAN_BEACON_FIXED_LENGTH);
	if (ds == NULL || wlanOperatingChannel() != ds[2])
		return NULL;
	return connection;
}

void wlanIbssBeaconMatched(const WlanWirelessFrame *frame, const uint8_t *body)
{
	struct WlanConnection *connection = wlanIbssMatchBeacon(frame, body);
	uint64_t now;

	if (connection == NULL)
		return;
	now = romGetTimerTimestamp();
	connection->lastBeaconTime[0] = (uint32_t)now;
	connection->lastBeaconTime[1] = (uint32_t)(now >> 32);
}

/*
 * The IBSS a beacon from another network asks us to merge with: one of ours
 * by SSID and channel, with a timestamp not behind ours, the same privacy
 * and, with privacy, the same security suites.
 */
struct WlanConnection *wlanIbssFindMerge(const WlanWirelessFrame *frame)
{
	const uint8_t *body = frame->body;
	struct WlanConnection *connection;
	const uint8_t *rsn;
	const uint8_t *wpa;
	uint64_t timestamp;
	uint64_t local;
	unsigned int length;
	uint8_t third[4];
	uint8_t fourth[4];

	connection = wlanIbssMatchBeacon(frame, body);
	if (connection == NULL)
		return NULL;
	__aeabi_memcpy(&timestamp, body, 8);
	local = (uint64_t)wlanIbssTsfOffset +
		((uint64_t)connection->lastBeaconTime[1] << 32 | connection->lastBeaconTime[0]);
	if (timestamp + 5000 < local)
		return NULL;
	if (((body[10] >> 4) & 1) != ((connection->bssCapability[0] >> 4) & 1))
		return NULL;
	if (!(connection->bssCapability[0] & 0x10))	/* capability: Privacy */
		return connection;
	length = (uint16_t)(LE16(frame->bodyLength) - WLAN_BEACON_FIXED_LENGTH);
	rsn = wlanIeFind(IE_RSN, body + WLAN_BEACON_FIXED_LENGTH, length);
	wpa = wlanIeFind(IE_VENDOR, body + WLAN_BEACON_FIXED_LENGTH, length);
	if (wlanWpaElementType(wpa + 4) != WLAN_VENDOR_WPA)
		wpa = NULL;
	macMgmtDispatchEntry(rsn, wpa, third, fourth, 0, 0, 0);
	if (memcmp(fourth, &connection->securitySuites[0], 1) ||
	    memcmp(third, &connection->securitySuites[1], 1))
		return NULL;
	return connection;
}

void wlanIbssJoinDone(struct WlanConnection *connection, int status)
{
	wlanReportEvent(connection, &status, 10);
}

void wlanIbssStartDone(struct WlanConnection *connection, int status)
{
	wlanReportEvent(connection, &status, 12);
}

/* Takes the rates, capability, BSSID and SSID of the IBSS to join. */
void wlanIbssJoinSetup(struct WlanConnection *connection, const WlanAdHocBssDesc *join)
{
	unsigned int rates;

	wlanAdHocJoinRates((WlanAdHocBssDesc *)join, connection->supportedRates,
			   &connection->supportedRatesLength);
	wlanPsQueryPacket();
	rates = wlanRateBitmapAll(connection->supportedRates, connection->supportedRatesLength, NULL,
				  0, (const WlanHtCapabilityIe *)connection->htCapability);
	macMgmtQueryContext(connection, connection->supportedRates,
			    connection->supportedRatesLength, rates);
	wlanProtection.flags = (wlanProtection.flags & ~WLAN_PROTECTION_DSSS_BSS) |
			       ((wlanRatesDsssOnly(rates) & 1) << 6);
	macTxNotifyApplyFrame(connection);
	connection->bssCapability[0] = join->capability;
	connection->bssCapability[1] = join->capability >> 8;
	macMgmtHandleContext(join, connection->localAddress, connection->networkSsid,
			     &connection->networkSsidLength);
	if (supplicantIsEnabled() && (connection->flags & 4))
		supplicantInitSession(connection, connection->networkSsid,
				      connection->networkSsidLength,
				      (IEEEtypes_MacAddr_t *)connection->localAddress,
				      (IEEEtypes_MacAddr_t *)connection->interfaceAddress);
	wlanPsHostMode = 0;
	connection->interfaceType = WLAN_IF_IBSS;
	wlanMacReleaseHold();
}

/* A kept beacon takes the ATIM window and beacon period of wlanIbssInfo. */
void wlanIbssRefreshBeacon(WlanBeaconDescriptor *const *beacon)
{
	wlanSetAtimWindow(beacon, wlanIbssInfo.atimWindow);
	wlanSetBeaconPeriod(beacon, wlanIbssInfo.beaconPeriod);
}
