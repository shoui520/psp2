/*
 * The WAPI information element (ID 68) of an access point's BSS
 * (wlan/wapiie.c, 0x4ccb0..0x4ced8): built from the settings the host gives,
 * parsed into them, and checked against a station's in its association
 * request. Marvell published no source; names are inferred from use.
 */
#ifndef ROBIN_WAPIIE_H
#define ROBIN_WAPIIE_H

#include <stdint.h>
#include "robin/conn.h"

#define IE_WAPI_ID		68

/* Association status codes fmCoexRecordBuffer() gives. */
#define WAPI_STATUS_INVALID_ELEMENT	40
#define WAPI_STATUS_INVALID_AKMP	43
#define WAPI_STATUS_MULTICAST_CIPHER	47
#define WAPI_STATUS_UNICAST_CIPHER	48
#define WAPI_STATUS_VERSION		49

unsigned int fmCoexSetupBuffer(const struct WlanConnectionInfo *info, uint8_t *ie, unsigned int size); /* 0x4ccb0 */
void fmCoexClearBuffer(struct WlanConnectionInfo *info);			/* 0x4cd2e */
int fmCoexConvertBuffer(struct WlanWapiSettings *settings, const uint8_t *element);	/* 0x4cd3c */
int fmCoexVerifyBuffer(struct WlanConnectionInfo *info, const uint8_t *element);	/* 0x4cdde */
unsigned int fmCoexSelectBuffer(const struct WlanConnectionInfo *info, uint8_t *ie);	/* 0x4ce06 */
unsigned int fmCoexAdjustBuffer(const struct WlanConnectionInfo *info, uint8_t *tlv);	/* 0x4ce1a */
int fmCoexRecordBuffer(const struct WlanConnectionInfo *info, const uint8_t *element, uint16_t *status); /* 0x4ce74 */

#endif /* ROBIN_WAPIIE_H */
