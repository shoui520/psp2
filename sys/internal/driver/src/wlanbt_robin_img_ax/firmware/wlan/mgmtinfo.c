/*
 * What an access point or IBSS derives from the stations around it: the
 * flags of the ERP and HT protection modes, the rates two elements share,
 * and the TLV a management frame's event carries. The filename is
 * inferred; the names of the flags are inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/mgmt.h"
#include "robin/rom.h"
#include "robin/runtime.h"

#define WLAN_TLV_FRAME_DATA		360

/* The OUI and type of the WPS element (00:50:f2, type 4). */
const uint8_t wlanWpsOui[4] = { 0x00, 0x50, 0xf2, 0x04 };

/* The ERP and HT state of a connection is passed around as a pointer to its erpInfo byte. */
#define WLAN_ERP_INFO(erpInfo) \
	((struct WlanConnectionInfo *)((uint8_t *)(erpInfo) - offsetof(struct WlanConnectionInfo, erpInfo)))

extern void (*wlanPsRefreshHook)(struct WlanConnection *connection);	/* 0x54e60 */
extern void (*wlanHtOperationHook)(struct WlanConnection *connection);	/* 0x54e64: the HT operating mode changed */

/* The "null frames at a low rate" flag follows from an ERP element or legacy stations around. */
void wlanApRateFlagsUpdate(unsigned int phyMode, uint8_t *erpInfo)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);

	if (info->erpInfo != 0) {
		info->rateFlags |= WLAN_RATE_FLAG_ERP;
	} else {
		info->rateFlags &= ~WLAN_RATE_FLAG_ERP;
		if (!(info->rateFlags & WLAN_RATE_FLAG_OLBC) &&
		    !(info->rateFlags & WLAN_RATE_FLAG_OLBC_ERP)) {
			info->rateFlags &= ~WLAN_RATE_FLAG_LOW_NULL;
			return;
		}
	}
	info->rateFlags |= WLAN_RATE_FLAG_LOW_NULL;
}

/* Forgets the overlapping legacy BSS: no ERP element, no legacy flags. */
void wlanApOlbcForget(uint8_t *erpInfo)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);

	info->erpInfo = 0;
	info->rateFlags &= ~(WLAN_RATE_FLAG_OLBC | WLAN_RATE_FLAG_OLBC_ERP);
}

/* Is the rate (without its basic-rate bit) one of the rates in either element? */
static __attribute__((noinline)) int wlanRateInSets(uint8_t rate, const uint8_t *first, const uint8_t *second)
{
	unsigned int i;

	rate &= 0x7f;
	if (first != NULL) {
		for (i = 0; i < first[1]; i++) {
			if ((first[i + 2] & 0x7f) == rate)
				return 1;
		}
	}
	if (second != NULL) {
		for (i = 0; i < second[1]; i++) {
			if ((second[i + 2] & 0x7f) == rate)
				return 1;
		}
	}
	return 0;
}

/*
 * Copies the rates (at most 14) that either element has to out, and their
 * number to count. Returns 0 when a basic rate is not among them.
 */
int wlanRatesSelect(const uint8_t *rates, const uint8_t *first, const uint8_t *second,
		    uint8_t *out, uint8_t *count)
{
	int result = 1;
	unsigned int number = 0;
	unsigned int length = romStringLength(rates, 14);
	unsigned int i;

	for (i = 0; i < length; i++) {
		if (wlanRateInSets(rates[i], first, second) == 1) {
			out[number] = rates[i];
			number = (uint8_t)(number + 1);
		} else if (rates[i] & 0x80) {
			result = 0;
		}
	}
	*count = number;
	return result;
}

/* Does the association request carry the WPS element? (request holds an unaligned pointer at 4.) */
int wlanFrameHasWpsIe(const uint8_t *request)
{
	const uint8_t *frame = (const uint8_t *)__aeabi_uread4(request + 4);
	int length = ((frame[1] << 8) | frame[0]) - 12;
	const uint8_t *ie = frame + 44;
	const uint8_t *found;

	while (length > 0) {
		found = wlanIeFind(IE_VENDOR, ie, length);
		if (found == NULL)
			break;
		if (memcmp(found + 2, wlanWpsOui, 4) == 0)
			return 1;
		length = length - ie[1] - 2;
		ie += ie[1] + 2;
	}
	return 0;
}

/* Does the list (to a zero or count entries) have an OFDM rate? */
int wlanRatesHaveOfdm(const uint8_t *rates, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count && rates[i] != 0; i++) {
		if (romRateIndex(rates[i] & 0x7f) - 5 <= 7)
			return 1;
	}
	return 0;
}

/* Does the list have a rate of the 802.11b set? */
int wlanRatesHaveCck(const uint8_t *rates, unsigned int count)
{
	unsigned int i;

	for (i = 0; i < count && rates[i] != 0; i++) {
		if (romRateIndex(rates[i] & 0x7f) <= 3)
			return 1;
	}
	return 0;
}

/* Appends the frame's data to a host event as a TLV; returns its length (0 without both pointers). */
unsigned int wlanTlvFrameDataAppend(uint8_t *tlv, const uint8_t *frame, uint16_t length)
{
	uint8_t *cursor;
	uint16_t type = WLAN_TLV_FRAME_DATA;
	uint16_t size = length + 2;

	if (tlv == NULL || frame == NULL)
		return 0;
	tlv[0] = type;
	tlv[1] = type >> 8;
	cursor = tlv + 2;
	cursor[0] = size;
	cursor[1] = size >> 8;
	cursor += 2;
	cursor[0] = frame[2];
	cursor[1] = frame[3];
	cursor += 2;
	__aeabi_memcpy(cursor, frame + 32, length);
	return (uint16_t)(cursor + length - tlv);
}

/*
 * Derives the HT operating mode of the BSS (the byte at 4 of the security
 * state: bits 0-1 the protection mode, bit 2 non-greenfield stations, bit 4
 * the third flag) from the stations seen, and tells the hooks when it changed.
 */
void wlanHtOperationUpdate(struct WlanConnection *connection, uint8_t *erpInfo,
			   const uint8_t *params, uint8_t *securityState)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);
	uint8_t *state = &securityState[4];
	unsigned int value = *state;
	unsigned int protection = value & 3;
	unsigned int nonGreenfield = (value >> 2) & 1;
	unsigned int flag4 = (value >> 4) & 1;

	if (info->htNonGreenfieldStations != 0)
		value |= 4;
	else
		value &= ~4u;
	*state = value;
	if (info->htMixedStations != 0)
		value |= 0x10;
	else
		value &= ~0x10u;
	*state = value;
	if (!(info->rateFlags & WLAN_RATE_FLAG_HT_OBSS)) {
		if (info->htMixedStations == 0) {
			if ((params[2] & 2) && info->ht20MhzStations != 0)
				value = ((value >> 2) << 2) + 2;
			else
				value = (value >> 2) << 2;
		} else {
			value |= 3;
		}
	} else if (info->htMixedStations == 0) {
		value = ((value >> 2) << 2) + 1;
	} else {
		value |= 3;
	}
	*state = value;
	value = *state;
	if ((value & 3) != protection || ((value >> 2) & 1) != nonGreenfield ||
	    ((value >> 4) & 1) != flag4) {
		wlanPsRefreshHook(connection);
		wlanHtOperationHook(connection);
	}
}

/* Clears what the HT operating mode is derived from. */
void wlanApHtStationsForget(uint8_t *erpInfo)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);

	info->ht20MhzStations = 0;
	info->htNonGreenfieldStations = 0;
	info->htMixedStations = 0;
	info->rateFlags &= ~WLAN_RATE_FLAG_HT_OBSS;
}
