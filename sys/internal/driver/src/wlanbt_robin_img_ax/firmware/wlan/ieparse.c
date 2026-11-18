/*
 * Classifying and finding information elements in a frame body: the vendor
 * elements of WMM and WPA, the pointer records the parsers fill (Marvell's
 * IEPointers_t), a search by id and the traffic indication map check. The
 * filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/wmm.h"
#include "robin/beacontrack.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/rom.h"
#include "robin/runtime.h"

/* The ROM's parser of a frame body calls this one through the hook at 0x54e9c. */
int (*wlanIeParseHook)(const uint8_t *ies, int length, WlanIePointers *out) __attribute__((section(".itcm.bss")));	/* 0x54e9c */

/* The kind of a WMM vendor element from the OUI that starts its body (0 when it is none). */
int wlanWmmElementType(const uint8_t *oui)
{
	int type = WLAN_VENDOR_NONE;
	uint8_t information[8] = { 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01 };	/* WMM, information */
	uint8_t parameter[8] = { 0x00, 0x50, 0xf2, 0x02, 0x01, 0x01 };	/* WMM, parameter */
	uint8_t tspec[8] = { 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };	/* WMM, TSPEC */

	if (memcmp(oui, information, 6) == 0)
		type = WLAN_VENDOR_WMM_INFORMATION;
	else if (memcmp(oui, parameter, 6) == 0)
		type = WLAN_VENDOR_WMM_PARAMETER;
	else if (memcmp(oui, tspec, 6) == 0)
		type = WLAN_VENDOR_WMM_TSPEC;
	return type;
}

/* WLAN_VENDOR_WPA when the OUI is the WPA element's (00:50:f2, type 1), else 0. */
int wlanWpaElementType(const uint8_t *oui)
{
	int type = WLAN_VENDOR_NONE;
	uint8_t wpa[8] = { 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00 };

	if (memcmp(oui, wpa, 6) == 0)
		type = WLAN_VENDOR_WPA;
	return type;
}

/* Records an element of a beacon, probe or association response in the pointers by kind; 0 when it is of no kind kept. */
int wlanIePointersRecord(const uint8_t *ie, WlanIePointers *ies)
{
	int kind;

	switch (ie[0]) {
	case IE_SSID:
		ies->pSsid = ie;
		break;
	case IE_SUPPORTED_RATES:
		ies->pSupportedRates = ie;
		break;
	case IE_DS_PARAMETER:
		ies->pDsParam = ie;
		break;
	case IE_TIM:
		ies->pTim = ie;
		break;
	case IE_IBSS_PARAMETER:
		ies->pIbssParam = ie;
		break;
	case IE_COUNTRY:
		ies->pCountry = ie;
		break;
	case IE_ERP_INFORMATION:
		ies->pErpInfo = (uint8_t *)ie;
		break;
	case IE_HT_CAPABILITY:
		ies->pHtCap = (const WlanHtCapabilityIe *)ie;
		break;
	case IE_RSN:
		ies->pRsn = ie;
		break;
	case IE_EXTENDED_RATES:
		ies->pExtSupportedRates = ie;
		break;
	case IE_HT_INFORMATION:
		ies->pHtInfo = ie;
		break;
	case IE_20_40_BSS_COEXISTENCE:
		ies->p2040Coexist = ie;
		break;
	case IE_OBSS_SCAN_PARAMETERS:
		ies->pHtScanParam = ie;
		break;
	case IE_EXTENDED_CAPABILITIES:
		ies->pExtCap = ie;
		break;
	case IE_VENDOR:
		if (wlanWpaElementType(ie + 2) != 0) {
			ies->pWpa = ie;
			break;
		}
		kind = wlanWmmElementType(ie + 2);
		if (kind == WLAN_VENDOR_WMM_INFORMATION) {
			ies->pWmmInfo = ie;
			break;
		}
		if (kind == WLAN_VENDOR_WMM_PARAMETER) {
			ies->pWmmParam = ie;
			break;
		}
		return 0;
	default:
		return 0;
	}
	return 1;
}

/* The same for the elements a station keeps of its AP's beacon (the second record of 0x3e94a). */
int wlanBeaconIePointersRecord(const uint8_t *ie, WlanBeaconIePointers *ies)
{
	int kind;

	switch (ie[0]) {
	case IE_SSID:
		ies->pSsid = ie;
		break;
	case IE_SUPPORTED_RATES:
		ies->pSupportedRates = ie;
		break;
	case IE_DS_PARAMETER:
		ies->pDsParam = ie;
		break;
	case IE_COUNTRY:
		ies->pCountry = ie;
		break;
	case IE_HT_CAPABILITY:
		ies->pHtCap = (const WlanHtCapabilityIe *)ie;
		break;
	case IE_RSN:
		ies->pRsn = ie;
		break;
	case IE_EXTENDED_RATES:
		ies->pExtSupportedRates = ie;
		break;
	case IE_HT_INFORMATION:
		ies->pHtInfo = ie;
		break;
	case IE_20_40_BSS_COEXISTENCE:
		ies->p2040Coexist = ie;
		break;
	case IE_OBSS_SCAN_PARAMETERS:
		ies->pHtScanParam = ie;
		break;
	case IE_EXTENDED_CAPABILITIES:
		ies->pExtCap = ie;
		break;
	case IE_VENDOR:
		if (wlanWpaElementType(ie + 2) != 0) {
			ies->pWpa = ie;
			break;
		}
		kind = wlanWmmElementType(ie + 2);
		if (kind == WLAN_VENDOR_WMM_INFORMATION) {
			ies->pWmmInfo = ie;
			break;
		}
		if (kind == WLAN_VENDOR_WMM_PARAMETER) {
			ies->pWmmParam = ie;
			break;
		}
		return 0;
	default:
		return 0;
	}
	return 1;
}

const uint8_t *wlanIeFind(int id, const uint8_t *ies, unsigned int length)
{
	const uint8_t *ie = ies;

	if (ie == NULL)
		return NULL;
	while (length != 0) {
		if (ie[1] + 2 > (int)length)
			break;
		if (ie[0] == id)
			return ie;
		length = length - ie[1] - 2;
		ie += ie[1] + 2;
	}
	return NULL;
}

/*
 * Whether the TIM element says the station with the association ID has
 * traffic: *groupTraffic when the DTIM count is zero and the multicast bit is set,
 * *unicastTraffic when the station's bit is set. Returns 0 for no element or an
 * ID out of range.
 */
int wlanTimCheck(const uint8_t *tim, uint16_t aid, int *groupTraffic, int *unicastTraffic)
{
	unsigned int id = aid & 0x3fff;
	unsigned int octet;
	unsigned int start;
	unsigned int index;

	if (tim == NULL || id == 0 || id > WLAN_AID_MAX)
		return 0;
	if (tim[2] == 0)
		*groupTraffic = tim[4] & 1;
	octet = id >> 3;
	start = 2 * (tim[4] >> 1);
	if (octet >= start && octet <= (uint8_t)(tim[1] + start - 4)) {
		index = octet - start;
		if (index < 251 && (tim[5 + index] >> (id & 7)) & 1)
			*unicastTraffic = 1;
	}
	return 1;
}

/* A vendor element of the Wi-Fi Simple Configuration (OUI 00:50:f2, type 4): 4, else 0. */
__attribute__((noinline)) int wlanPsBuildDescriptor(const uint8_t *oui)
{
	uint32_t wps = 0x04f25000;
	int type = 0;

	if (memcmp(oui, &wps, 4) == 0)
		type = 4;
	return type;
}

/* A vendor element of Wi-Fi Direct (OUI 50:6f:9a, type 9): 7, else 0. */
__attribute__((noinline)) int wlanPsParseDescriptor(const uint8_t *oui)
{
	uint32_t p2p = 0x099a6f50;
	int type = 0;

	if (memcmp(oui, &p2p, 4) == 0)
		type = 7;
	return type;
}

/*
 * Sorts the elements of a frame body into the pointers by kind; returns the bytes left over
 * (nonzero: an element was cut short). Elements the pointer record keeps by id come first, the
 * vendor ones last.
 */
int wlanParseIes(const uint8_t *ies, int length, WlanIePointers *out)
{
	__aeabi_memclr4(out, sizeof(*out));
	while (length != 0 && ies[1] + 2 <= length) {
		switch (ies[0]) {
		case IE_EDCA_PARAMETER_SET:
			out->pEdcaParam = ies;
			break;
		case IE_QOS_CAPABILITY:
			out->pQosCapability = ies;
			break;
		case IE_RSN:
			out->pRsn = ies;
			break;
		case IE_WAPI:
			out->pWapi = ies;
			break;
		default:
			if (wlanIePointersRecord(ies, out) == 0 && ies[0] == IE_VENDOR) {
				if (wlanPsBuildDescriptor(ies + 2) != 0)
					out->pWps = ies;
				if (ies[0] == IE_VENDOR && wlanPsParseDescriptor(ies + 2) != 0)
					out->pVendor = ies;
			}
			break;
		}
		length = length - ies[1] - 2;
		ies += ies[1] + 2;
	}
	return length;
}

/* The elements of an access point's beacon the station keeps (the second pointer record). */
void wlanPsQueryDescriptor(const uint8_t *ies, int length, WlanBeaconIePointers *out)
{
	int size = 0;

	__aeabi_memclr4(out, sizeof(*out));
	while (length != 0) {
		size = ies[1] + 2;
		if (size > length)
			break;
		if (ies[0] == IE_AP_CHANNEL_REPORT)
			out->pApChannelReport = ies;
		else if (ies[0] != IE_MOBILITY_DOMAIN)
			wlanBeaconIePointersRecord(ies, out);
		size = ies[1];
		length = length - size - 2;
		ies += size + 2;
	}
}

/*
 * What the beacon's TIM says to the station: bit 0 and 3 when the DTIM says group traffic (with
 * bit 15), bit 1 when it has unicast traffic (with bit 15). 0 when there is no TIM.
 */
unsigned int wlanPsResetDescriptor(struct WlanConnection *connection, const uint8_t *tim)
{
	int groupTraffic = 0;
	int unicastTraffic = 0;
	unsigned int bits;

	if (tim == NULL)
		return 0;
	wlanPsSelectFrame(tim);
	if (wlanTimCheck(tim, connection->associationId, &groupTraffic, &unicastTraffic) == 0)
		return 0;
	bits = 0;
	if (groupTraffic != 0)
		bits = 0x8009;
	if (unicastTraffic != 0)
		bits |= 0x8002;
	return bits;
}

/* The ROM parses a frame body with wlanParseIes() from now on. */
void wlanPsSyncDescriptor(void)
{
	wlanIeParseHook = wlanParseIes;
}
