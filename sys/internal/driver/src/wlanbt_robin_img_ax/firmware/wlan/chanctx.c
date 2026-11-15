/*
 * The channel the radio is on, as the access point and IBSS code keep it: the channel flags
 * (WlanChannelFlags: the band in bits 0-1, the secondary channel in bits 4-5) and number, the
 * ones to return to (what the radio had when the connection took the channel), and the transmit
 * power limit of the link. Marvell published no source for it; the names are the firmware's own
 * placeholder names (macMgmt*Context), kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/adhoc.h"
#include "robin/apsta.h"
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/htba.h"
#include "robin/ibss.h"
#include "robin/icverr.h"
#include "robin/ie.h"
#include "robin/macentry.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/mgmt.h"
#include "robin/protection.h"
#include "robin/rom.h"
#include "robin/stamachine.h"

struct WlanChannelContext {
	uint8_t reserved0;
	uint8_t savedFlags;		/* the flags macMgmtCheckContext() found the radio on */
	uint8_t linkPowerLimit;		/* wlanTxPowerGetLinkLimit() then */
	uint8_t flags;			/* the flags the radio is on */
	uint8_t reserved4[4];
	uint32_t beaconMisses;		/* beacons in a row on another channel than the access point's (0x55160) */
	uint32_t savedChannel;		/* 0: none yet */
	uint32_t channel;		/* the channel the radio is on */
};

/* Not converted yet; provisional names. */
void macTxNotifyApplyBuffer(struct WlanConnection *connection);			/* 0x32204 */
void macTxNotifyResetBuffer(struct WlanConnection *connection, unsigned int value);	/* 0x3218c */
void macTxNotifyResolveBuffer(unsigned int rate);					/* 0x32294 */
void wlanPsReleasePacket(struct WlanConnection *connection, uint32_t rateSet);		/* 0x3d286 */
void wlanPsApplyFrame(struct WlanConnection *connection, int a, unsigned int b);	/* 0x3d6d0 */
int p2pSqramHandleState(struct WlanConnection *connection);				/* SQRAM */
extern int (*wlanChannelValidateHook)(void);						/* 0x5f1a0 */

uint8_t wlanNullAddress[8] __attribute__((section(".itcm.rw")));				/* 0x55150 */
struct WlanChannelContext wlanChannelContext __attribute__((section(".itcm.rw")));	/* 0x55158 */

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
	if (connection->interfaceType != 3 || p2pSqramHandleState(connection) == 0)
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

/* 0x35ac2 */
int macMgmtAcquireContext(struct WlanConnection *connection, const uint8_t *body, unsigned int length)
{
	return 1;
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
	} else if (connection->smState == connection->smStates + 72 &&
		   wlanOperatingChannel() != wlanChannelContext.savedChannel) {
		return 0;
	}
	return 1;
}

/* 0x35faa: the station machine is in one of the linked states. */
int wlanStaSmLinked(const struct WlanConnection *connection)
{
	const uint8_t *state = connection->smState;
	const uint8_t *states = connection->smStates;

	return state == states + 48 || state == states + 36 || state == states + 60;
}
