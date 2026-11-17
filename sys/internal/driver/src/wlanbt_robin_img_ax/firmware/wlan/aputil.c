/*
 * Access point helpers (0x0002b680..0x0002b82a): the overlapping legacy BSS
 * (OLBC) detection from other BSSs' beacons, the TIM element's bits, the
 * TKIP countermeasure state of a new BSS, and the TLVs the association event
 * to the host carries. The filename and the names are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/apconfig.h"
#include "robin/conn.h"
#include "robin/esatypes.h"
#include "robin/ie.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/txpower.h"

/* The WMM TLV's value: a parameter element's 24 bytes, whatever its length says. */
#define WLAN_TLV_WMM_VALUE	24

#define U16(bytes)		((uint16_t)((bytes)[0] | (bytes)[1] << 8))
#define PUT_U16(bytes, value)	(((struct __attribute__((packed)) { uint16_t word; } *)(bytes))->word = (uint16_t)(value))

/* erpInfo points to WlanConnectionInfo.erpInfo; the functions reach the rest of it. */
#define WLAN_ERP_INFO(erpInfo) \
	((struct WlanConnectionInfo *)((uint8_t *)(erpInfo) - offsetof(struct WlanConnectionInfo, erpInfo)))

/*
 * Once a beacon interval: when no legacy BSS was heard for the time
 * wlanApOlbcDetect() set, the protection it asked for is dropped.
 */
void wlanApOlbcExpire(struct WlanConnection *connection, uint8_t *olbcActive, uint8_t *erpInfo)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);

	if (*olbcActive == 0)
		return;
	if (--*olbcActive != 0)
		return;
	info->rateFlags &= ~(WLAN_RATE_FLAG_OLBC | WLAN_RATE_FLAG_OLBC_ERP);
	wlanApErpUpdateHook(connection);
}

/*
 * A beacon of another BSS on our channel: a BSS without OFDM rates, or one
 * whose ERP element says non-ERP stations are present, is an overlapping
 * legacy BSS. The protection is kept for the next `seconds` seconds.
 */
void wlanApOlbcDetect(struct WlanConnection *connection, const WlanIePointers *elements,
		      unsigned int channel, uint8_t *erpInfo, uint8_t *olbcActive,
		      unsigned int beaconPeriod, unsigned int seconds)
{
	struct WlanConnectionInfo *info = WLAN_ERP_INFO(erpInfo);
	int legacy;

	if (elements->pDsParam == NULL || elements->pDsParam[2] != channel)
		return;
	legacy = 0;
	if ((elements->pSupportedRates == NULL ||
	     !wlanRatesHaveOfdm(elements->pSupportedRates + 2, elements->pSupportedRates[1])) &&
	    (elements->pExtSupportedRates == NULL ||
	     !wlanRatesHaveOfdm(elements->pExtSupportedRates + 2, elements->pExtSupportedRates[1]))) {
		info->rateFlags |= WLAN_RATE_FLAG_OLBC;
		legacy = 1;
	}
	if (elements->pErpInfo != NULL && (elements->pErpInfo[2] & 1))
		info->rateFlags |= WLAN_RATE_FLAG_OLBC_ERP;
	else if (!legacy)
		return;
	wlanApErpUpdateHook(connection);
	*olbcActive = seconds * 1000000 / (beaconPeriod << 10) + 1;
}

/*
 * Sets or clears an association ID's bit of the TIM element (AID 0: the
 * group-addressed frames bit), then has the beacon template rebuilt.
 */
void wlanApTimSet(struct WlanConnection *connection, unsigned int aid, unsigned int set,
		  unsigned int maxStations, uint8_t *tim)
{
	uint8_t *bitmap;
	unsigned int bit;

	if (aid == 0) {
		if (set == 1)
			tim[4] |= 1;
		else
			tim[4] &= ~1;
	} else {
		bitmap = &tim[5 + (aid >> 3)];
		bit = 1u << (aid & 7);
		if (set == 1)
			*bitmap |= bit;
		else
			*bitmap &= ~bit;
	}
	wlanApTemplateHook(connection);
}

/* A WPA access point starts with the TKIP countermeasures armed and no failure seen. */
void wlanApMicErrorInit(unsigned int wpa, MIC_Error_t *micError)
{
	if (wpa) {
		micError->MICCounterMeasureEnabled = 1;
		micError->disableStaAsso = 0;
		micError->status = NO_MIC_FAILURE;
	}
}

/* Appends an element as a TLV of its element ID; length is the TLVs' total (LE). */
void wlanTlvAppendElement(const uint8_t *element, uint8_t **cursor, uint8_t *length)
{
	unsigned int size = element[1] + 4;
	unsigned int bodyLength;

	PUT_U16(*cursor, element[0]);
	bodyLength = element[1];
	PUT_U16(*cursor + 2, bodyLength);
	__aeabi_memcpy(*cursor + 4, element + 2, bodyLength);
	*cursor += size;
	PUT_U16(length, U16(length) + size);
}

/* Appends the packet forwarding control TLV. */
void wlanTlvAppendPktFwdCtl(unsigned int value, uint8_t **cursor, uint8_t *length)
{
	PUT_U16(*cursor, WLAN_TLV_PKT_FWD_CTL);
	PUT_U16(*cursor + 2, 1);
	(*cursor)[4] = value;
	*cursor += 5;
	PUT_U16(length, U16(length) + 5);
}

/* Appends the WMM parameter element as a TLV. */
void wlanTlvAppendWmm(const uint8_t *wmm, uint8_t **cursor, uint8_t *length)
{
	PUT_U16(*cursor, wmm[0]);
	PUT_U16(*cursor + 2, wmm[1]);
	__aeabi_memcpy(*cursor + 4, wmm + 2, wmm[1]);
	PUT_U16(length, U16(length) + WLAN_TLV_WMM_VALUE + 4);
	*cursor += WLAN_TLV_WMM_VALUE + 4;
}
