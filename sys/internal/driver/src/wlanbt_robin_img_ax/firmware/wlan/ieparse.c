/*
 * Classifying and finding information elements in a frame body: the vendor
 * elements of WMM and WPA, the pointer records the parsers fill (Marvell's
 * IEPointers_t), a search by id and the traffic indication map check. The
 * filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/ie.h"
#include "robin/runtime.h"

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
