/*
 * The access point's configuration commands (wlan/apconfig.c): HostCmd 0xb0
 * (Marvell's UAP_SYS_CONFIGURE) and the TLVs of it and of HostCmd 0xb3
 * (UAP_STA_LIST).
 */
#ifndef ROBIN_APCONFIG_H
#define ROBIN_APCONFIG_H

#include <stdint.h>
#include "robin/conn.h"

/* Marvell's TLV numbers the commands use. */
#define WLAN_TLV_STA_INFO	0x0137

void wlanTlvStaInfoSet(uint8_t *tlv, uint8_t *address, const uint8_t *peer);
const uint8_t *wlanApConfigRequestParse(unsigned int length, const uint8_t *tlv, uint16_t *types,
					 int *count, uint32_t *antennas, uint32_t *wepKeys,
					 int unused, uint16_t *authKeyLength);
int hostCmd00b0Set(struct WlanConnection *connection, uint8_t *command);
int hostCmd00b0Get(struct WlanConnection *connection, uint8_t *command);
int hostCmd00b3(struct WlanConnection *connection, uint8_t *command);

#endif /* ROBIN_APCONFIG_H */
