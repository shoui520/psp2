/*
 * IBSS (ad-hoc) beacons: the beacon body from the host's AD_HOC_START
 * parameters, the AD_HOC_JOIN parameters turned into start parameters, and
 * the beacon fields the IBSS code changes later (0x5c20..0x5fb6). The
 * filename is inferred; no source filename survives.
 */
#include "robin/adhoc.h"
#include "robin/ie.h"
#include "robin/mef.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/runtime.h"

static ROBIN_PATCH_SLOT(wlanSetSecondaryChannelOverride);

uint8_t wlanBroadcastAddress[6] __attribute__((section(".itcm.rw"))) = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/* Returns the WMM parameter element the access point and IBSS beacons carry (0x2d248). */
void wlanGetWmmParameterIe(WlanWmmParameterIe *ie);

/*
 * Builds the header, beacon period, capability, SSID and supported rates of
 * a beacon (subtype 8) or probe response in buffer; returns the end.
 */
uint8_t *wlanBuildIbssBeacon(const WlanAdHocStart *start, const uint8_t *source,
			     const uint8_t *bssid, unsigned int subtype,
			     unsigned int powerManagement, OsaBuffer *buffer)
{
	WlanWirelessFrame *frame;
	uint8_t *end;

	if (!start || !buffer)
		return NULL;
	frame = (WlanWirelessFrame *)((uint8_t *)buffer + buffer->dataOffset);
	wlanBuildMgmtHeader(frame, source, wlanBroadcastAddress, bssid, subtype,
			    powerManagement);
	/* the timestamp (body 0..7) is the MAC's */
	frame->body[8] = start->beaconPeriod;
	frame->body[9] = start->beaconPeriod >> 8;
	frame->body[10] = ((const uint8_t *)&start->capability)[0];
	frame->body[11] = ((const uint8_t *)&start->capability)[1];
	end = frame->body + 12;
	end += wlanPutSsidIe(start->ssid, end);
	return end + wlanPutRatesIe(start->dataRate, end, 0);
}

/* Appends the DS (3 bytes) or FH (7 bytes) parameter set element. */
uint8_t *wlanPutPhyParamSet(uint8_t *buffer, const WlanAdHocStart *start)
{
	if (start->phyParamSet[0] == IE_DS_PARAMETER) {
		__aeabi_memcpy(buffer, start->phyParamSet, 3);
		buffer += 3;
	} else if (start->phyParamSet[0] == IE_FH_PARAMETER) {
		__aeabi_memcpy(buffer, start->phyParamSet, 7);
		buffer += 7;
	}
	return buffer;
}

/* The rates of the network to join, with the default beacon period of 100 TU filled in. */
void wlanAdHocJoinRates(WlanAdHocBssDesc *join, uint8_t *rates, uint8_t *count)
{
	unsigned int length;

	if (!join)
		return;
	if (join->beaconPeriod == 0)
		join->beaconPeriod = 100;
	length = romStringLength(join->dataRates, sizeof(join->dataRates));
	*count = length;
	__aeabi_memcpy(rates, join->dataRates, length);
}

/* Copies an SSID of at most 32 bytes and clears the BSSID. */
void wlanAdHocSsid(const uint8_t *ssid, uint8_t *bssid, uint8_t *destination,
		   uint8_t *length)
{
	if (!ssid)
		return;
	__aeabi_memclr(bssid, 6);
	*length = romStringCopy(destination, ssid, 32);
}

/* Start parameters (bssMode 2) from the network the host asked to join. */
void wlanAdHocJoinToStart(WlanAdHocStart *start, const WlanAdHocBssDesc *join)
{
	unsigned int count;
	unsigned int i;

	if (!start || !join)
		return;
	__aeabi_memclr(start, sizeof(*start));
	start->beaconPeriod = join->beaconPeriod;
	start->bssMode = 2;
	start->capability = join->capability;
	start->dtimPeriod = join->dtimPeriod;
	start->extraIes = join->extraIes;
	start->extraIesLength = join->extraIesLength;
	count = romStringLength(join->dataRates, sizeof(join->dataRates));
	for (i = 0; i < count; i = (uint8_t)(i + 1))
		start->dataRate[i] = join->dataRates[i];
	__aeabi_memcpy(start->phyParamSet, join->phyParamSet, sizeof(start->phyParamSet));
	__aeabi_memcpy(start->ssid, join->ssid, sizeof(start->ssid));
	__aeabi_memcpy(start->ssParamSet, join->ssParamSet, sizeof(start->ssParamSet));
}

/* Sets the beacon period in the kept beacon; returns its body. */
uint8_t *wlanSetBeaconPeriod(WlanBeaconDescriptor *const *beacon, uint16_t period)
{
	uint8_t *body = (*beacon)->frame->body;

	*(uint16_t *)(body + 8) = period;
	return body;
}

/* Sets the ATIM window of the kept beacon's IBSS parameter set. */
uint8_t *wlanSetAtimWindow(WlanBeaconDescriptor *const *beacon, uint16_t window)
{
	uint8_t *ie;

	ie = wlanFindIe((*beacon)->frame, IE_IBSS_PARAMETER, WLAN_FRAME_BEACON, NULL, 0);
	if (ie)
		*(uint16_t *)(ie + 2) = window;
	return ie;
}

/*
 * Takes the secondary channel offset of the HT Information element into the
 * channel flags, keeping the 40 MHz pairs a channel allows (2.4 GHz: 1-4 only
 * above, 10-13 only below, 8 and 9 below unless allowed both ways; 5 GHz: by
 * the channel's pair), and writes the result back into the element.
 */
void wlanSetSecondaryChannel(uint8_t *channelFlags, unsigned int channel,
			     uint8_t *htInformation, int bothWays)
{
	unsigned int offset;

	if (!channelFlags || !htInformation ||
	    wlanSetSecondaryChannelOverride(channelFlags, channel, htInformation, bothWays))
		return;
	offset = htInformation[3] & 3;
	if (offset != 1)
		offset = (offset == 3) ? 3 : 0;
	*channelFlags = (*channelFlags & ~WLAN_CHANNEL_OFFSET_MASK) |
			(offset << WLAN_CHANNEL_OFFSET_SHIFT);
	if (!(*channelFlags & WLAN_CHANNEL_OFFSET_MASK))
		return;
	if ((*channelFlags & WLAN_CHANNEL_BAND_MASK) == WLAN_CHANNEL_BAND_2GHZ) {
		if (channel == 1 || channel == 2 || channel == 3 || channel == 4)
			*channelFlags = (*channelFlags & ~WLAN_CHANNEL_OFFSET_MASK) +
					WLAN_CHANNEL_OFFSET_ABOVE;
		if (channel == 10 || channel == 11 || channel == 12 || channel == 13)
			*channelFlags |= WLAN_CHANNEL_OFFSET_BELOW;
		if (!bothWays && (channel == 8 || channel == 9))
			*channelFlags |= WLAN_CHANNEL_OFFSET_BELOW;
	} else if ((*channelFlags & WLAN_CHANNEL_BAND_MASK) == WLAN_CHANNEL_BAND_5GHZ) {
		switch (channel) {
		case 36: case 44: case 52: case 60: case 100: case 108:
		case 116: case 124: case 132: case 149: case 157:
			*channelFlags = (*channelFlags & ~WLAN_CHANNEL_OFFSET_MASK) +
					WLAN_CHANNEL_OFFSET_ABOVE;
			break;
		case 40: case 48: case 56: case 64: case 104: case 112:
		case 120: case 128: case 136: case 153: case 161:
			*channelFlags |= WLAN_CHANNEL_OFFSET_BELOW;
			break;
		}
	}
	htInformation[3] = (htInformation[3] & ~3) |
			   ((*channelFlags & WLAN_CHANNEL_OFFSET_MASK) >> WLAN_CHANNEL_OFFSET_SHIFT);
}

/*
 * Appends the WMM parameter element at *cursor and programs the four
 * access categories' EDCA parameters from it.
 */
void wlanPutWmmParameterIe(uint8_t **cursor)
{
	WlanWmmParameterIe ie;
	const WlanWmmAcParameter *ac;
	unsigned int queue;
	int i;

	wlanGetWmmParameterIe(&ie);
	__aeabi_memcpy(*cursor, &ie, sizeof(ie));
	*cursor += sizeof(ie);
	for (i = 0; i < 4; i++) {
		ac = &ie.ac[i];
		queue = wlanAccessCategoryToQueue((ac->aciAifsn >> 5) & 3);
		romEdcaSetCwMin(queue, (1 << (ac->ecw & 0xf)) - 1);
		romEdcaSetCwMax(queue, (1 << (ac->ecw >> 4)) - 1);
		romEdcaSetAifsn(queue, ac->aciAifsn & 0xf);
		romEdcaSetTxop(queue, ac->txopLimit);
	}
}

/* Sets the ERP element's Non-ERP Present and Use Protection flags together. */
void wlanSetErpProtection(WlanIePointers *ies, const void *bssState,
			  const void *pending, int protection)
{
	uint8_t *erp = ies->pErpInfo;
	unsigned int flags;

	if (!erp || ((erp[2] >> 1) & 1) == protection)
		return;
	flags = (erp[2] & ~WLAN_ERP_NON_ERP_PRESENT) | (protection & 1);
	erp[2] = flags;
	ies->pErpInfo[2] = (flags & ~WLAN_ERP_USE_PROTECTION) | ((protection & 1) << 1);
}
