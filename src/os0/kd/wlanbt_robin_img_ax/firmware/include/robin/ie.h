/* 802.11 information element helpers working on WlanWirelessFrame bodies. */
#ifndef ROBIN_IE_H
#define ROBIN_IE_H

#include <stdint.h>
#include "robin/wlan.h"

enum {
	IE_SSID = 0,
	IE_SUPPORTED_RATES = 1,
	IE_DS_PARAMETER = 3,
	IE_ERP_INFORMATION = 42,
	IE_RSN = 48,
	IE_EXTENDED_RATES = 50,
	IE_VENDOR = 221,
};

enum {
	WLAN_FRAME_PROBE_REQUEST = 4,
	WLAN_FRAME_PROBE_RESPONSE = 5,
	WLAN_FRAME_BEACON = 8,
};

typedef void (*WlanMoveFn)(uint8_t *destination, const uint8_t *source, uint32_t length);
extern WlanMoveFn wlanIeMoveForward;
extern WlanMoveFn wlanIeMoveBackward;
extern uint16_t wlanIeLimitProbe;
extern uint16_t wlanIeLimitBeacon;
int wlanIeInit(void);
int wlanChannelHooksInit(void);

uint8_t *wlanFindIe(WlanWirelessFrame *frame, int ieId, int frameKind,
		    const void *match, unsigned int matchLength);
void wlanSetIe(WlanWirelessFrame *frame, const uint8_t *ie, uint8_t *end,
	       uint8_t *removed, uint8_t *added);
int wlanPutErpIe(uint8_t *buffer, int nonErpPresent, int useProtection, int barker);
int wlanPutDsParameterIe(uint8_t *buffer, int channel);
unsigned int wlanPutSsidIe(const void *ssid, uint8_t *buffer);
unsigned int wlanPutRatesIe(const uint8_t *rates, uint8_t *buffer, int extended);
unsigned int wlanInsertIeSorted(const uint8_t *ie, uint8_t *start, uint8_t *end);
unsigned int wlanCopyIes(int ieId, uint8_t *destination, int frameKind,
			 uint8_t *base, int ieOffset, const uint8_t *source,
			 unsigned int sourceLength);
void wlanAppendIe(WlanWirelessFrame *frame, uint8_t id, const void *data, unsigned int length);
unsigned int wlanAppendIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
unsigned int wlanRemoveIe(const uint8_t *ie, uint8_t *start, uint8_t *end);
unsigned int wlanRemoveIeList(WlanWirelessFrame *frame, int offset,
			      const uint8_t *ies, unsigned int length);
int wlanClearBodyLength(uint16_t *length);
unsigned int wlanRateBitmapBasic(const uint8_t *rates, unsigned int count);
unsigned int wlanRateBitmap(const uint8_t *rates, unsigned int count);
int wlanTimeDelta(unsigned int now, unsigned int last);

#endif /* ROBIN_IE_H */
