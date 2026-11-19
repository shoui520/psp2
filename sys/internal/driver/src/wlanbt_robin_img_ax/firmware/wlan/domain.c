/*
 * The 802.11d domain table: the country and the transmit power of each listed
 * channel, set and read by host command 0x5b (Marvell's HostCmd_CMD_802_11D_DOMAIN_INFO,
 * with the Country information element as its TLV). The filename is inferred.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/domain.h"
#include "robin/mgmt.h"
#include "robin/rom.h"

struct WlanChannelPowerTable wlanChannelPowerTable __attribute__((section(".itcm.bss")));	/* 0x574ec */

/* The table is empty again. */
void wlanDomainClear(void)
{
	__aeabi_memclr(wlanChannelPowerTable.country, 3);
	wlanChannelPowerTable.count = 0;
	__aeabi_memclr(wlanChannelPowerTable.channels, WLAN_DOMAIN_MAX_CHANNELS);
	__aeabi_memset4(wlanChannelPowerTable.power, WLAN_DOMAIN_MAX_CHANNELS, WLAN_DOMAIN_DEFAULT_POWER);
}

/* Where a channel is in the table. */
int wlanDomainChannelIndex(unsigned int channel, uint8_t *index)
{
	unsigned int i;

	if (wlanChannelPowerTable.count == 0)
		return 0;
	for (i = 0; wlanChannelPowerTable.count > i; i++) {
		if (wlanChannelPowerTable.channels[i] == channel) {
			*index = i;
			return 1;
		}
	}
	return 0;
}

/* The connection takes the domain's power limits (the first flag of its second flag byte), and a domain is set. */
int wlanDomainApplies(const struct WlanConnection *connection)
{
	return (connection->flags2 & 1) != 0 && wlanChannelPowerTable.count != 0;
}

/* The distance between two channels of the same sub-band triplet. */
static __attribute__((noinline)) unsigned int wlanChannelSpacing(unsigned int channel, unsigned int band)
{
	if (channel >= 34 || (channel >= 8 && band != 0))
		return 4;
	return 1;
}

/*
 * Writes the Country information element: the country, then a (first channel, count,
 * power) triplet for each run of channels the same spacing apart with the same power,
 * padded to an even length. Returns the bytes written, 0 without a domain.
 */
unsigned int wlanDomainBuildIe(struct WlanConnection *connection, uint8_t *ie)
{
	uint8_t *triplet;
	uint8_t flags;
	unsigned int band;
	unsigned int i;
	unsigned int runs = 0;
	unsigned int open = 0;
	unsigned int last = 0;

	if (wlanChannelPowerTable.count == 0)
		return wlanChannelPowerTable.count;
	ie[0] = WLAN_IE_COUNTRY;
	ie[1] = 0;
	__aeabi_memcpy(ie + 2, wlanChannelPowerTable.country, 3);
	ie[1] += 3;
	triplet = ie + 5;
	if (connection->mode == 2) {
		band = wlanConnectionInfoSlot(connection)->channelFlags & WLAN_CHANNEL_BAND_MASK;
	} else if (connection->interfaceType == 1 || connection->interfaceType == 0) {
		macMgmtPrepareContext(&flags, NULL);
		band = flags & WLAN_CHANNEL_BAND_MASK;
	} else {
		band = 1;
	}
	for (i = 0; wlanChannelPowerTable.count > i; i = (uint8_t)(i + 1)) {
		unsigned int channel;

		if (wlanChannelPowerTable.power[i] == last) {
			triplet[1]++;
		} else {
			triplet += 3 * open;
			triplet[0] = wlanChannelPowerTable.channels[i];
			last = triplet[2] = wlanChannelPowerTable.power[i];
			triplet[1] = 1;
			open = 1;
			runs = (uint8_t)(runs + 1);
		}
		channel = wlanChannelPowerTable.channels[i];
		if (wlanChannelPowerTable.channels[i + 1] != (uint8_t)(wlanChannelSpacing(channel, band) + channel))
			last = 0;
	}
	ie[1] = (uint8_t)(ie[1] + 3 * runs);
	if ((ie[1] & 1) != 0) {
		ie[5 + 3 * runs] = 0;
		ie[1]++;
	}
	return (uint8_t)(ie[1] + 2);
}

/* Takes a Country information element as the domain; 0 if it is not one or lists too many channels. */
int wlanDomainParseIe(const uint8_t *ie)
{
	unsigned int index = 0;
	unsigned int length;
	unsigned int remaining;

	if (ie[0] != WLAN_IE_COUNTRY)
		return 0;
	wlanDomainClear();
	length = ie[1];
	if (length == 0)
		return 0;
	__aeabi_memcpy(wlanChannelPowerTable.country, ie + 2, 3);
	ie += 5;
	for (remaining = (uint8_t)(length - 3); remaining > 1; remaining = (uint8_t)(remaining - 3), ie += 3) {
		if (ie[0] < 201 && ie[0] != 0) {
			unsigned int i;

			for (i = 0; ie[1] > i; i = (uint8_t)(i + 1)) {
				if (index >= WLAN_DOMAIN_MAX_CHANNELS) {
					wlanDomainClear();
					return 0;
				}
				wlanChannelPowerTable.channels[index] = ie[0] + i;
				wlanChannelPowerTable.power[index] = ie[2];
				index = (uint8_t)(index + 1);
			}
			wlanChannelPowerTable.count += ie[1];
		}
	}
	return 1;
}

/* Host command 0x5b, get: the TLV (type 7, then the length of the element) and the element's content. */
unsigned int hostCmdDomainInfoGet(struct WlanConnection *connection, uint8_t *data)
{
	unsigned int result;

	data[0] = WLAN_IE_COUNTRY;
	data[1] = 0;
	result = wlanDomainBuildIe(connection, data + 2);
	if (result != 0) {
		data[2] = result - 2;
		data[3] = (result - 2) >> 8;
		return (uint8_t)(result + 2);
	}
	return result;
}

/* Host command 0x5b, set: without a TLV the domain is cleared, if no connection depends on it. */
int hostCmdDomainInfoSet(struct WlanConnection *connection, uint8_t *data, unsigned int length)
{
	if (length != 2) {
		if ((uint16_t)(data[0] | data[1] << 8) == WLAN_IE_COUNTRY &&
		    (uint16_t)(data[2] | data[3] << 8) <= 255) {
			uint16_t tlvLength = data[2];

			data[3] = data[2];
			data[2] = WLAN_IE_COUNTRY;
			if (wlanDomainParseIe(data + 2) != 0) {
				wlanTxPowerUpdateConnection(connection, wlanOperatingChannel(), 1);
				data[2] = tlvLength;
				data[3] = tlvLength >> 8;
				return 1;
			}
			data[2] = tlvLength;
			data[3] = tlvLength >> 8;
		}
		return 0;
	}
	if (wlanAnyActiveConnectionFlags2Bit0() != 0)
		return 0;
	wlanDomainClear();
	return 1;
}
