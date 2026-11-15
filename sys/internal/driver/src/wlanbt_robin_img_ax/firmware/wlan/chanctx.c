/*
 * The channel the radio is on, as the access point and IBSS code keep it: the channel flags
 * (WlanChannelFlags: the band in bits 0-1, the secondary channel in bits 4-5) and number, the
 * ones to return to (what the radio had when the connection took the channel), and the transmit
 * power limit of the link. Marvell published no source for it; the names are the firmware's own
 * placeholder names (macMgmt*Context), kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/antdiv.h"
#include "robin/btpower.h"
#include "robin/p2pgo.h"
#include "robin/ratesel.h"
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/htba.h"
#include "robin/ibss.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/mac.h"
#include "robin/macrate.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/peerrate.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/signal.h"
#include "robin/stamachine.h"
#include "robin/psctl.h"
#include "robin/stapssm.h"
#include "robin/sync.h"
#include "robin/uapbss.h"
#include "robin/wlantask.h"
#include "robin/wmm.h"


/* The P2P attributes of a beacon or probe response the direct code picked out (76 bytes). */

/* Not converted yet; provisional names. */
void macTxNotifyApplyBuffer(struct WlanConnection *connection);			/* 0x32204 */
void macTxNotifyResetBuffer(struct WlanConnection *connection, unsigned int value);	/* 0x3218c */
void macTxNotifyResolveBuffer(unsigned int rate);					/* 0x32294 */
void wlanPsApplyFrame(struct WlanConnection *connection, int a, unsigned int b);	/* 0x3d6d0 */
extern int (*wlanChannelValidateHook)(void);						/* 0x5f1a0 */
int wlanPsHandleHeader(struct WlanConnection *connection, const uint8_t *body, unsigned int length,
		       const uint8_t *tim, unsigned int flags);				/* 0x3dc5c */
void macMgmtAdjustTable(const WlanWirelessFrame *frame, int kind);			/* 0x37c72 */
int wlanFrequencyTrackingStub(void);							/* 0x2e9c8 */
void hostCmd003c(int timing, int carrier);						/* 0x2e9cc */
void romLibSyncContext(unsigned int mask);						/* ROM */
void cbProcRecordPacket(struct WlanConnection *connection, int first, int second);	/* 0x1e868 */
int rfCalibSyncDescriptor(void);							/* 0x2a20c */
void p2pSqramDispatchState(struct WlanDirectContext *attributes, uint32_t flags,
			   struct WlanPsPeerState *ps);					/* SQRAM */

/* The automatic frequency control (hostcmd.c: host command 60), which hostCmd003c() is a stub of here. */
uint8_t wlanAfcEnabled __attribute__((section(".itcm.rw")));			/* 0x54f18 */
int16_t wlanAfcTimingOffset __attribute__((section(".itcm.rw")));		/* 0x54f1a: the sum, then the mean */
int16_t wlanAfcCarrierOffset __attribute__((section(".itcm.rw")));		/* 0x54f1c */
uint8_t wlanAfcAuto __attribute__((section(".itcm.rw")));			/* 0x54f1e: the carrier error that is reported */
uint8_t wlanAfcThreshold __attribute__((section(".itcm.rw")));			/* 0x54f1f: the beacons that are summed */
uint8_t wlanAfcSamples __attribute__((section(".itcm.rw")));			/* 0x54f20: the beacons summed so far */
/* The TSF of the last beacon the connection took (hostcmd.c reports it). */
uint8_t wlanBeaconTimestamp[8] __attribute__((section(".dtcm.bss")));		/* 0x040088d2 */
/* The WEP ICV errors the IBSS remembers. */
WepIcvErrEntry wlanIbssIcvErrEntries[WEP_ICV_ERR_ENTRIES] __attribute__((section(".itcm.rw")));	/* 0x5f130 */
/* A counter block the connection's conversion clears (0x4004aa4). */
uint8_t wlanBssCounters[16] __attribute__((section(".dtcm.bss")));

uint8_t wlanNullAddress[8] __attribute__((section(".itcm.rw")));				/* 0x55150 */
struct WlanChannelContext wlanChannelContext __attribute__((section(".itcm.rw")));	/* 0x55158 */

/* 0x35558: the IBSS to join: the MAC takes its address, the connection its BSSID and SSID. */
void macMgmtHandleContext(const WlanAdHocBssDesc *join, uint8_t *bssid, uint8_t *ssid,
			  uint8_t *ssidLength)
{
	unsigned int length;

	if (join == NULL)
		return;
	wlanTxSetSharedRadio(join->capability & 1);
	wlanMacAddressSet(join->bssid);
	__aeabi_memcpy(bssid, join->bssid, 6);
	length = romStringLength(join->ssid, 32);
	romStringCopy(ssid, join->ssid, length);
	*ssidLength = length;
	wlanMacAddressApply();
	wlanIbssRecordJoin(join);
	wlanIbssTimersControl(1);
}

/* 0x355b0: the channel's secondary channel is set from the HT operation element of the connection. */
void macMgmtProcessContext(struct WlanConnection *connection, uint8_t *channel, unsigned int number)
{
	uint8_t *htOperation = (uint8_t *)wlanConnectionHtOperation(connection);
	uint8_t index;

	wlanSetSecondaryChannel(channel, number, htOperation, wlanDomainChannelIndex(12, &index));
}

/* 0x355d2: the radio goes to the channel, unless it is there. */
int macMgmtUpdateContext(uint8_t channelFlags, unsigned int channel)
{
	uint8_t flags = channelFlags;

	if (channel != wlanChannelContext.channel || flags != wlanChannelContext.flags) {
		wlanSetChannel(channel, &flags, 1);
		wlanChannelContext.flags = flags;
		wlanChannelContext.channel = channel;
		if (wlanChannelContext.savedChannel == 0) {
			wlanChannelContext.savedFlags = flags;
			wlanChannelContext.savedChannel = channel;
		}
	}
	return 0;
}

/* 0x3560c: the radio's secondary channel follows the HT operation element. */
void macMgmtConfigureContext(const uint8_t *htOperation)
{
	uint8_t index;

	if (((wlanChannelContext.flags >> 4) & 3) != (htOperation[3] & 3)) {
		wlanSetSecondaryChannel(&wlanChannelContext.flags, wlanChannelContext.channel & 0xff,
					(uint8_t *)htOperation, wlanDomainChannelIndex(12, &index));
		wlanSetChannelFlags(&wlanChannelContext.flags);
	}
}

/* 0x3564a: what the radio is on is what it is to return to. */
void macMgmtCheckContext(void)
{
	wlanChannelContext.savedFlags = wlanChannelContext.flags;
	wlanChannelContext.savedChannel = wlanChannelContext.channel;
	wlanChannelContext.linkPowerLimit = wlanTxPowerGetLinkLimit();
}

/* 0x3565e */
void macMgmtValidateContext(void)
{
	wlanChannelContext.linkPowerLimit = wlanTxPowerGetLinkLimit();
}

/* 0x3566a: the channel flags and number the radio will return to. */
void macMgmtPrepareContext(uint8_t *band, uint32_t *extra)
{
	if (band != NULL)
		*band = wlanChannelContext.savedFlags;
	if (extra != NULL)
		*extra = wlanChannelContext.savedChannel;
}

/* 0x3567e: the radio is to return to this channel and flags (band, bandwidth). */
int macMgmtDispatchContext(unsigned int channel, unsigned int band, unsigned int offset)
{
	return wlanConnectionsIdle() == 0 && wlanChannelContext.savedChannel == channel &&
	       (wlanChannelContext.savedFlags & 3) == band &&
	       ((wlanChannelContext.savedFlags >> 2) & 3) == offset;
}

/* 0x356b4: 2 when the radio is on the 5 GHz band, else 1. */
int macMgmtParseContext(void)
{
	if ((wlanChannelContext.flags & 3) != 0)
		return 2;
	return 1;
}

/* 0x356c6: the host's channel report. */
void hostCmd001eChannel(uint8_t *channel)
{
	channel[0] = wlanChannelContext.savedFlags;
	channel[1] = wlanChannelContext.savedChannel;
}

/* 0x356d2: the rates an HT-less (or Block Ack-less) connection leaves out of its rate bitmap. */
unsigned int macMgmtComputeContext(struct WlanConnection *connection, unsigned int bitmap)
{
	if (wlanConnectionIsHt(connection) == 0)
		return bitmap & 0x80003fff;
	if (!wlanConnectionHtTransmitsBa(connection))
		return bitmap & 0x803fffff;
	return bitmap;
}

/* 0x356f6: the connection's rates: the basic ones, the rate set, the group rate. */
void macMgmtQueryContext(struct WlanConnection *connection, const uint8_t *rates,
			 unsigned int count, unsigned int bitmap)
{
	macMgmtParseState(rates, count);
	wlanConnectionRateSetUpdate(connection, wlanRateBitmapBasic(rates, count));
	macTxNotifyApplyBuffer(connection);
	wlanMacConfigApply(wlanTxApGroupRate(connection));
	wlanPsReleasePacket(connection, bitmap);
}

/* 0x35734: the MAC takes the address; an IBSS shares the radio with the others, an infrastructure link does not. */
void macMgmtResetContext(int adhoc, const uint8_t *address)
{
	wlanMacAddressSet(address);
	if (adhoc) {
		wlanTxSetSharedRadio(0);
		wlanChannelValidateHook();
	} else {
		wlanTxSetSharedRadio(1);
	}
	wlanIbssTimersControl(1);
	romLibComputeContext(0x1000000);
	romLibAcquireContext();
}

/* 0x3576a: the connection's channel and rates are put back on the radio. */
int macMgmtSyncContext(struct WlanConnection *connection)
{
	int adhoc;
	unsigned int bitmap;

	wlanTxPowerSetLinkLimit(wlanChannelContext.linkPowerLimit, 0);
	if (connection->interfaceType != 3 || p2pBssActiveGet(connection) == 0)
		macMgmtUpdateContext(wlanChannelContext.savedFlags, wlanChannelContext.savedChannel);
	adhoc = 0;
	if (connection->interfaceType == 1)
		adhoc = (connection->flags2 & 0x10) != 0;
	macMgmtResetContext(adhoc, connection->localAddress);
	bitmap = wlanRateBitmapAll(connection->supportedRates, connection->supportedRatesLength, 0, 0,
				   (const WlanHtCapabilityIe *)connection->htCapability);
	if (!adhoc)
		bitmap = macMgmtComputeContext(connection, bitmap);
	macMgmtQueryContext(connection, connection->supportedRates, connection->supportedRatesLength,
			    bitmap);
	wlanMacClear46cBits();
	wlanChannelContext.beaconMisses = 0;
	return 0;
}

/* 0x357ea: the access point's HT Information element changed the secondary channel: the radio follows. */
void macMgmtApplyContext(struct WlanConnection *connection, uint8_t *htInformation)
{
	uint8_t *htOperation = (uint8_t *)wlanConnectionHtOperation(connection);
	uint8_t flags = wlanChannelContext.flags;
	uint8_t index;
	int widened = 0;
	unsigned int offset;

	wlanSetSecondaryChannel(&flags, htInformation[2], htInformation,
				wlanDomainChannelIndex(12, &index));
	if ((htOperation[3] & 3) == (htInformation[3] & 3))
		return;
	htOperation[3] = (htOperation[3] & ~3) | (htInformation[3] & 3);
	if (htOperation[3] & 3) {
		wlanHtTxCapability[0] |= 2;
		widened = 1;
	} else {
		wlanHtTxCapability[0] &= ~2;
	}
	wlanRateChannelWidthSet(connection, 0, wlanHtChannelWidth(connection));
	offset = htOperation[3] & 3;
	if (offset != 3 && offset == 1)
		flags = (flags & 0xc0) | (wlanChannelContext.flags & 0x0f) | WLAN_CHANNEL_OFFSET_ABOVE;
	else if (offset == 3)
		flags = (flags & 0xf0) | (wlanChannelContext.flags & 0x0f) | WLAN_CHANNEL_OFFSET_BELOW;
	if (offset == 1 || offset == 3)
		wlanSetChannel(htInformation[2], &flags, 1);
	macMgmtCheckContext();
	wlanHostEventSendData(connection, 72, &widened, 1);
}

/* 0x358c8: the protection state is applied to the connection's transmit settings. */
void macMgmtResolveContext(struct WlanConnection *connection, unsigned int rate)
{
	if (connection->interfaceType == 1)
		wlanWepIcvErrAge(&wlanIbssIcvErrList);
	wlanPsApplyFrame(connection, 0, (wlanProtection.flags >> 5) & 1);
	if (wlanProtection.flags & WLAN_PROTECTION_USE_PROTECTION)
		macTxNotifyResetBuffer(connection, rate);
	else
		macTxNotifyResolveBuffer(wlanTxApGroupRate(connection));
}

/* 0x35906 */
void macMgmtScheduleContext(void)
{
	wlanProtectionUpdateFlags(&wlanProtection);
	wlanIbssUpdateProtection();
}

/* 0x35916: the protection timers age to now. */
void macMgmtNotifyContext(struct WlanConnection *connection, unsigned int now)
{
	wlanProtectionAge(&wlanProtection, now);
	if (connection->interfaceType == 1)
		wlanProtectionAgeMode1(&wlanProtection, now);
}

/* 0x35936: the IBSS's protection is brought up to date. */
void macMgmtFormatContext(void)
{
	struct WlanConnection *connection = wlanConnectionFindByTypeNumber(1, 0);

	if (connection != NULL) {
		macMgmtNotifyContext(connection, romGetMicroseconds());
		macMgmtScheduleContext();
		macMgmtResolveContext(connection, connection->probeRate);
	}
}

/* 0x35962: the elements of a beacon or probe response and the rates it offers. */
unsigned int macMgmtInspectContext(struct WlanConnection *connection, const uint8_t *frame,
				   WlanIePointers *elements)
{
	const uint8_t *extended = NULL;
	unsigned int extendedCount = 0;

	wlanParseIes(frame + 44, ((frame[1] << 8) | frame[0]) - 12, elements);
	if (elements->pExtSupportedRates != NULL) {
		extendedCount = elements->pExtSupportedRates[1];
		extended = elements->pExtSupportedRates + 2;
	}
	return macMgmtComputeContext(connection,
				     wlanRateBitmapAll(elements->pSupportedRates + 2,
						       elements->pSupportedRates[1], extended,
						       extendedCount, elements->pHtCap));
}

/* 0x359a6: what the frame says about protection counts, on the 2.4 GHz band; on 5 GHz it clears. */
void macMgmtTrackContext(const uint8_t *capability, unsigned int rates, const uint8_t *erp,
			 unsigned int now)
{
	if ((wlanChannelContext.savedFlags & 3) == 0) {
		wlanProtectionObserve(&wlanProtection, capability, erp, rates, now,
				      WLAN_PROTECTION_TIMEOUT);
		return;
	}
	wlanProtectionClear(&wlanProtection);
	wlanIbssUpdateProtection();
}

/* 0x359fa: a beacon of the P2P group's owner, or of the IBSS, the state machine was handed. */
void macMgmtManageContext(struct WlanConnection *connection, const uint8_t *frame)
{
	WlanIePointers elements;
	unsigned int rates;
	unsigned int now;

	if (!wlanConnectionHasLocalAddress(connection, frame + 18))
		return;
	rates = macMgmtInspectContext(connection, frame, &elements);
	if (connection->interfaceType == 3) {
		struct WlanPsPeerState *ps = wlanConnectionPsState(connection);

		if (ps->p2pFlags & 4) {
			struct WlanDirectContext attributes[1];
			uint8_t buffer[32];
			uint32_t first = 0;
			uint32_t flags = 0;
			struct WlanDirectContext *direct;

			direct = p2pDirectScheduleContext(frame, 0, &first, &flags, attributes);
			if (flags & 0x2000)
				p2pDirectSyncContext(direct->attributes[P2P_SLOT_DEVICE_INFO], buffer, 9, &direct->attributes[P2P_SLOT_DEVICE_INFO]);
			if (direct->attributes[P2P_SLOT_DEVICE_INFO] != NULL &&
			    memcmp(direct->attributes[P2P_SLOT_DEVICE_INFO] + 3, ps->p2pGroupId, 6) == 0) {
				ps->p2pChannel = wlanOperatingChannel();
				ps->p2pBssActive = 1;
				p2pSqramDispatchState(direct, flags, ps);
			}
		}
	}
	now = romGetMicroseconds();
	macMgmtNotifyContext(connection, now);
	macMgmtTrackContext(frame + 42, rates, elements.pErpInfo, now);
	macMgmtScheduleContext();
	macMgmtResolveContext(connection, connection->probeRate);
	wlanWmmUpdate(connection, elements.pWmmInfo, elements.pWmmParam);
}

/* 0x35ac2 */
int macMgmtAcquireContext(struct WlanConnection *connection, const uint8_t *body, unsigned int length)
{
	return 1;
}

/* 0x35ac6: a beacon (or probe response) on the connection's channel: protection, the automatic frequency
   control, the power save state and the P2P group's schedule follow what it says. */
void macMgmtReleaseContext(struct WlanConnection *connection, const uint8_t *frame, uintptr_t argument)
{
	WlanIePointers elements;
	unsigned int rates;
	unsigned int now;
	int dtim;

	if ((connection->interfaceType == 3 || connection->interfaceType == 0) &&
	    !wlanConnectionHasLocalAddress(connection, frame + 18))
		return;
	rates = macMgmtInspectContext(connection, frame, &elements);
	now = romGetMicroseconds();
	macMgmtNotifyContext(connection, now);
	macMgmtTrackContext(frame + 42, rates, elements.pErpInfo, now);
	macMgmtScheduleContext();
	macMgmtResolveContext(connection, connection->probeRate);
	wlanHtBeaconApply(connection, &elements, frame + 18);
	macMgmtAdjustTable((const WlanWirelessFrame *)frame, 8);

	if (wlanChannelContext.trackBeacons >= wlanAfcThreshold) {
		int carrier;

		wlanChannelContext.trackBeacons = 0;
		if (wlanAfcSamples != 0) {
			wlanAfcTimingOffset = wlanAfcTimingOffset / wlanAfcSamples;
			wlanAfcCarrierOffset = wlanAfcCarrierOffset / wlanAfcSamples;
		}
		if (wlanAfcEnabled != 0) {
			carrier = (int8_t)(1102 * wlanAfcCarrierOffset / 1000);
			if (carrier >= wlanAfcAuto || carrier <= -wlanAfcAuto)
				hostCmd003c(wlanAfcTimingOffset, wlanAfcCarrierOffset);
		}
		wlanAfcSamples = 0;
		wlanAfcTimingOffset = 0;
		wlanAfcCarrierOffset = 0;
	}
	wlanFrequencyTrackingStub();
	wlanChannelContext.trackBeacons++;

	if (elements.pTim != NULL)
		connection->dtimPeriod = elements.pTim[3];
	connection->beaconPeriod = frame[40] | (frame[41] << 8);
	dtim = wlanPsHandleHeader(connection, frame + 32, frame[0] | (frame[1] << 8), elements.pTim,
				  wlanChannelContext.savedFlags & 3);
	__aeabi_memcpy(wlanBeaconTimestamp, frame + 32, 8);

	if (connection->interfaceType == 3) {
		struct WlanPsPeerState *ps = wlanConnectionPsState(connection);
		struct WlanDirectContext attributes[1];
		uint8_t buffer[32];
		uint32_t first = 0;
		uint32_t flags = 0;
		struct WlanDirectContext *direct;
		uint64_t timestamp;

		__aeabi_memcpy(ps->beaconTimestamp, frame + 32, 8);
		timestamp = romGetTimerTimestamp();
		ps->beaconTimerLow = (uint32_t)timestamp;
		ps->beaconTimerHigh = timestamp >> 32;
		direct = p2pDirectScheduleContext(frame, 0, &first, &flags, attributes);
		if (flags & 0x1000)
			p2pDirectSyncContext(direct->attributes[P2P_SLOT_NOTICE_OF_ABSENCE], buffer, -1, &direct->attributes[P2P_SLOT_NOTICE_OF_ABSENCE]);
		if (direct == NULL || direct->attributes[P2P_SLOT_NOTICE_OF_ABSENCE] == NULL) {
			__aeabi_memclr(ps->noaAttribute, sizeof(ps->noaAttribute));
			ps->noaMode = 0;
		} else {
			const uint8_t *attribute = direct->attributes[P2P_SLOT_NOTICE_OF_ABSENCE];
			unsigned int length = attribute[1] | (attribute[2] << 8);
			unsigned int window = attribute[4] & 0x7f;

			__aeabi_memcpy(ps->noaAttribute, attribute, length + 3);
			if ((attribute[4] >> 7) && window) {
				wlanPsWakeBeacons = window >> 1;
				ps->noaMode = 1;
			} else if (length > 2 && (ps->noaAttribute[6] | ps->noaAttribute[7] |
						  ps->noaAttribute[8] | ps->noaAttribute[9]) &&
				   (ps->noaAttribute[10] | ps->noaAttribute[11] |
				    ps->noaAttribute[12] | ps->noaAttribute[13])) {
				ps->noaMode = 2;
			}
		}
		if (ps->triggerPending) {
			wlanChannelContext.triggerCount++;
			osaEventFlagsSet(macTxEventFlags, 1, 7);
		}
	}

	if (connection->interfaceType == 1) {
		wlanIbssBeaconReceived(connection, (const WlanWirelessFrame *)frame, (const void *)argument,
				       &elements, rates);
		return;
	}
	if (wlanConnectionIsHt(connection) && elements.pHtCap != NULL)
		macMgmtApplyContext(connection, (uint8_t *)elements.pHtInfo);
	wlanStaPsBeaconSeen(connection, elements.pTim, dtim);
	wlanWmmUpdate(connection, elements.pWmmInfo, elements.pWmmParam);
	if (elements.pDsParam != NULL && wlanOperatingChannel() != elements.pDsParam[2]) {
		if (++wlanChannelContext.beaconMisses >= 3) {
			wlanReportEvent(connection, NULL, 7);
			wlanChannelContext.beaconMisses = 0;
		}
	} else {
		wlanChannelContext.beaconMisses = 0;
		if (rates < 0x20)
			wlanProtection.flags = (wlanProtection.flags | 8) & ~7;
		else
			wlanProtection.flags = (wlanProtection.flags & ~(8 | 7)) | 2;
	}
}

/* 0x35da6: the machine is set up and started in its first state. */
int macMgmtSetupContext(struct WlanConnection *connection)
{
	macMgmtFilterState(&connection->machine);
	romHsmStart(&connection->machine, &wlanIbssStates.verify);
	return 0;
}

/* 0x35dc0: the protection's preamble kind is applied. */
void macMgmtClearContext(struct WlanConnection *connection)
{
	wlanPsApplyFrame(connection, 0, (wlanProtection.flags >> 5) & 1);
	wlanMacSlotConfigure((wlanProtection.flags & WLAN_PROTECTION_LONG_PREAMBLE) != 0);
}

/* 0x35e14: the connection is put back to what it was before it joined or started a network. */
void macMgmtConvertContext(struct WlanConnection *connection)
{
	const uint8_t *rates = wlanRadioConfig->rates;
	unsigned int count;

	macTxSetPowerSave(0);
	romLibSyncContext(0xc00);
	wlanClearBodyLength((uint16_t *)connection->bssCapability);
	if (connection->interfaceType == 3 && connection->p2pState == 3) {
		__aeabi_memclr4(connection->localAddress, 6);
		__aeabi_memclr4(connection->networkSsid, 32);
		connection->networkSsidLength = 0;
		connection->dtimPeriod = 0;
		connection->beaconPeriod = 0;
	}
	macTxFlushConnection(connection);
	wlanHtElementsReset(connection);
	__aeabi_memclr(connection->peerAddress, 6);
	connection->associationId = 0;
	rfCalibSyncDescriptor();
	wlanBssSelectAddress(1, NULL);
	wlanSetSlotTime(1);
	wlanBssAllowStart(0);
	wlanBssAllowStart(3);
	wlanConnectionCommandCheck(1, 1);
	wlanConnectionCommandPrepare(1, 0);
	macTxNotifyParseFrame(connection);
	macTxNotifyConfigureFrame(connection);
	if (wlanRadioConfig->primaryDiversity)
		antDivStart();
	else
		antDivReset();
	wlanSignalReset(connection);
	count = romStringLength(rates, 14);
	wlanPsBuildPacket();
	macMgmtQueryContext(connection, rates, count, wlanRateBitmapAll(rates, count, 0, 0, NULL));
	macTxNotifyApplyFrame(connection);
	cbProcRecordPacket(connection, 0, 0);
	btLmpLcHandleState(0);
	connection->flags &= ~(0x10 | 0x04);
	connection->dtimPeriod = 0;
	connection->beaconPeriod = 100;
	connection->beaconRssiLast = -98;
	connection->noiseLast = -98;
	__aeabi_memclr(wlanBssCounters, sizeof(wlanBssCounters));
	__aeabi_memclr(&wlanProtection, 0x62);
	wlanProtection.flags |= WLAN_PROTECTION_LONG_PREAMBLE;
	macMgmtClearContext(connection);
	wlanWepIcvErrInit(wlanIbssIcvErrEntries, &wlanIbssIcvErrList);
	wlanTxPowerSetConnectionLimit(&connection->txPowerLimit, 255, 1);
}

/* 0x35f74 */
int macMgmtVerifyContext(void)
{
	wlanChannelContext.beaconMisses = 0;
	return 0;
}

/* 0x35f7c: the connection may be left on its channel. */
int macMgmtSelectContext(struct WlanConnection *connection)
{
	if (connection->mode == 2) {
		if ((connection->flags2 & 0x10) == 0)
			return 0;
	} else if (connection->machine.state == &wlanIbssStates.hold &&
		   wlanOperatingChannel() != wlanChannelContext.savedChannel) {
		return 0;
	}
	return 1;
}

/* 0x35faa: the station machine is in one of the linked states. */
int wlanStaSmLinked(const struct WlanConnection *connection)
{
	const struct RomHsmState *state = connection->machine.state;

	return state == &wlanIbssStates.select || state == &wlanIbssStates.adjust ||
	       state == &wlanIbssStates.record;
}

/* 0x35fca: the IBSS the machine is in is dropped; the beacons it missed are forgotten. */
void macMgmtRecordContext(void)
{
	wlanChannelContext.beaconMisses = 0;
	wlanIbssReset();
}
