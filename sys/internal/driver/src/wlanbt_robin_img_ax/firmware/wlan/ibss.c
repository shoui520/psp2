/*
 * IBSS (ad-hoc) networks (0x10d48..0x11550): the beacon and probe response
 * templates the MAC sends, starting an IBSS from the host's AD_HOC_START,
 * joining one from AD_HOC_JOIN, and adopting the BSSID and beacon timing of
 * a network that merges with ours. The filename is inferred; no source
 * filename survives.
 */
#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/ibss.h"
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
extern uint8_t wlanIbssErpPending[];				/* SQRAM 0xc0031f00 */
void wlanParseIes(const uint8_t *ies, int length, WlanIePointers *out);	/* 0x3e8cc */
void macTxNotifyReleaseState(struct WlanConnection *connection);	/* 0x301b0 */
int macTxNotifyNotifyState(struct WlanConnection *connection);		/* 0x2ff62 */
int macTxNotifyInspectState(struct WlanConnection *connection, uint8_t *buffer); /* 0x2ffa0 */
int macTxNotifyHandleContext(struct WlanConnection *connection, int ieId,
			     uint8_t *buffer);				/* 0x3033e */
int macTxNotifyRecordState(struct WlanConnection *connection);		/* 0x30324 */
void cbProcTrackPacket(void);						/* 0x1e570 */
int macMgmtRecordTable(const uint8_t *address, uint8_t *buffer);	/* 0x37d02 */
uint32_t rfCalibInspectTimer(unsigned int rate);			/* 0x2bc6c */
void wlanPsVerifyFrame(int reason);					/* 0x3dac6 */
void wlanPsFilterFrame(uint16_t beaconPeriod);				/* 0x3dc4e */
void macTxSyncChannel(struct WlanConnection *connection, const WlanAdHocBssDesc *join); /* 0x11960 */
void macMgmtProcessContext(struct WlanConnection *connection, uint8_t *channel,
			   unsigned int number);			/* 0x355b0 */
void macMgmtQueryContext(struct WlanConnection *connection, const uint8_t *rates,
			 unsigned int count, unsigned int bitmap);	/* 0x356f6 */
void macTxNotifyApplyFrame(struct WlanConnection *connection);		/* 0x3360a */
void macMgmtResolveEntry(struct WlanConnection *connection, unsigned int channel,
			 int start);					/* 0x36a32 */
int cbProcAdjustEntry(struct WlanConnection *connection);		/* 0x1cbce */
int macMgmtQueryState(uint32_t flags, uint32_t extra);			/* 0x34b82 */
void btLmpLcHandleState(int state);					/* 0x42488 */
void wlanIbssJoinDone(struct WlanConnection *connection, int status);	/* 0x11944 */
void wlanIbssStartDone(struct WlanConnection *connection, int status);	/* 0x11952 */
void cbProcSelectBuffer(int kind, const uint8_t *bssid);		/* 0x1e064 */
void macMgmtPrepareDescriptor(uint16_t atimWindow);			/* 0x3bc5a */
uint16_t macMgmtDispatchDescriptor(void);				/* 0x3bc6e */
void wlanPsClearTable(struct WlanConnection *connection, int value);	/* 0x3c03c */
void wlanPsUpdateTable(struct WlanConnection *connection);		/* 0x17206 */
void wlanPsAdjustTable(struct WlanConnection *connection);		/* 0x3c0aa */
void wpaSuppNotifyDescriptor(void);					/* 0x18c16 */

/* Sets the ERP element of a template to the protection the BSS state asks for. */
void wlanIbssTemplateSetProtection(WlanWirelessFrame *frame, int protection)
{
	WlanIePointers ies;

	macTxNotifyReleaseState(wlanConnectionFindByTypeNumber(WLAN_IF_IBSS, 0));
	wlanParseIes(frame->body + WLAN_BEACON_FIXED_LENGTH,
		     LE16(frame->bodyLength) - WLAN_BEACON_FIXED_LENGTH, &ies);
	wlanSetErpProtection(&ies, &wlanProtection, wlanIbssErpPending, protection);
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

	macMgmtPrepareContext(&flags, NULL);
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
	if (macTxNotifyNotifyState(connection) == 1)
		end += macTxNotifyInspectState(connection, end);
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
	end += macTxNotifyHandleContext(connection, 45, end);
	copyStart = end;
	if (connection->flags & 4)
		end += wlanCopyIes(IE_RSN, copyStart, subtype, (uint8_t *)frame, bodyOffset,
				   start->extraIes, start->extraIesLength);
	end += wlanPutRatesIe(start->dataRate, end, 1);
	end += macTxNotifyHandleContext(connection, 61, end);
	end += macTxNotifyHandleContext(connection, 72, end);
	end += macTxNotifyHandleContext(connection, 74, end);
	if (macTxNotifyRecordState(connection)) {
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

	macMgmtPrepareContext(&flags, &extra);
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
		macTxSyncChannel(connection, join);
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

	macMgmtPrepareContext(&flags, &extra);
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
		wlanCalibrationStart(connection, start);
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
			wlanPsUpdateTable(connection);
	} else {
		wlanPsAdjustTable(connection);
		wpaSuppNotifyDescriptor();
	}
}
