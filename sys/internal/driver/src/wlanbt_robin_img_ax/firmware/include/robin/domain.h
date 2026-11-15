/*
 * The regulatory domain the host sets (Marvell's HostCmd_CMD_802_11D_DOMAIN_INFO,
 * host command 0x5b): the Country information element, as the access list of
 * channels and the transmit power each may use.
 */
#ifndef ROBIN_DOMAIN_H
#define ROBIN_DOMAIN_H

#include <stdint.h>

struct WlanConnection;

#define WLAN_DOMAIN_MAX_CHANNELS	64
#define WLAN_DOMAIN_DEFAULT_POWER	20	/* dBm, until the host gives the domain */
#define WLAN_IE_COUNTRY			7

/* The domain table (0x574ec). */
struct WlanChannelPowerTable {
	uint8_t country[3];
	uint8_t count;				/* channels listed */
	uint8_t channels[WLAN_DOMAIN_MAX_CHANNELS];
	uint8_t power[WLAN_DOMAIN_MAX_CHANNELS];
};

_Static_assert(sizeof(struct WlanChannelPowerTable) == 132, "domain table");

extern struct WlanChannelPowerTable wlanChannelPowerTable;

void wlanDomainClear(void);						/* 0x2ff78 */
int wlanDomainChannelIndex(unsigned int channel, uint8_t *index);		/* 0x2ff3c */
int wlanDomainApplies(const struct WlanConnection *connection);		/* 0x2ff62 */
unsigned int wlanDomainBuildIe(struct WlanConnection *connection, uint8_t *ie);	/* 0x2ffa0 */
int wlanDomainParseIe(const uint8_t *ie);				/* 0x300c0 */
unsigned int hostCmdDomainInfoGet(struct WlanConnection *connection, uint8_t *data);	/* 0x3009a */
int hostCmdDomainInfoSet(struct WlanConnection *connection, uint8_t *data,
			 unsigned int length);				/* 0x30144 */

#endif /* ROBIN_DOMAIN_H */
