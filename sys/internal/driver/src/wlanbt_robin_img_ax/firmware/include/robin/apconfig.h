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
#define WLAN_TLV_PKT_FWD_CTL	0x0136
#define WLAN_TLV_STA_INFO	0x0137

void wlanTlvStaInfoSet(uint8_t *tlv, uint8_t *address, const uint8_t *peer);
const uint8_t *wlanApConfigRequestParse(unsigned int length, const uint8_t *tlv, uint16_t *types,
					 int *count, uint32_t *antennas, uint32_t *wepKeys,
					 uint32_t *unused, uint16_t *authKeyLength);
int wlanApConfigTlvSet(const uint8_t *tlv, struct WlanConnectionInfo *info, uint8_t *ssid,
		       uint8_t *channelFlags, int bssStarted, uint32_t *pmkChanged, void **copyTo,
		       const uint8_t *value, uint16_t *status, struct WlanConnection *connection);
int wlanApConfigTlvGet(unsigned int type, void **addresses, uint8_t *lengths,
		       struct WlanConnectionInfo *info, uint8_t *ssid, uint8_t *channelFlags,
		       uint16_t *length, uint8_t *tlv, int shortPreamble, int bssStarted,
		       uint32_t *antennas, uint32_t *wepKeys, uint16_t hookValue,
		       uint8_t *scratchFlag, uint8_t *scratchByte, uint16_t *scratchHalf,
		       uint32_t *scratchWord, uint8_t *htScratch, uint8_t *wmmScratch,
		       uint8_t *protocols);
uint16_t hostCmd00b0Set(struct WlanConnection *connection, uint8_t *command);
int hostCmd00b0Get(struct WlanConnection *connection, uint8_t *command);
int hostCmd00b3(struct WlanConnection *connection, uint8_t *command);

#endif /* ROBIN_APCONFIG_H */
