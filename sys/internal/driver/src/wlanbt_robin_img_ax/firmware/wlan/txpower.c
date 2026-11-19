/*
 * The transmit power: what the channel allows and the host asks for, per rate class
 * (0x3b35a..0x3b784), and the connection's MAC filter (0x3b14c..0x3b1e2). The firmware's own
 * placeholder names are kept, see robin/txpower.h.
 */
#include <stddef.h>
#include "robin/calibration.h"
#include "robin/conn.h"
#include "robin/mgmt.h"
#include "robin/phy.h"
#include "robin/ratectl.h"
#include "robin/rf.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/txpower.h"

struct WlanTxPowerState wlanTxPower __attribute__((section(".dtcm.rw"))) = {	/* 0x40007ba */
	.channel = 0xff,
	.policy = { 3, 0, 3, 2, 1, 9, 8, 7, 0xff, 0xff, 6, 5, 4, 0xff, 0xff, 0xff },
	.rates = {
		{ 7, 7, 7 }, { 3, 0, 3 }, { 7, 6, 7 }, { 7, 4, 5 }, { 7, 0, 3 }, { 8, 5, 7 },
		{ 8, 3, 4 }, { 8, 0, 2 }, { 8, 5, 7 }, { 8, 3, 4 }, { 8, 0, 2 },
	},
	.limit = { [0 ... WLAN_TX_POWER_RATES - 1] = { .maximum = 18, .minimum = 18, .spread = 0 } },
	.request = { [0 ... WLAN_TX_POWER_RATES - 1] = { .maximum = 18, .minimum = 18, .spread = 0 } },
	.defaults = { [0 ... WLAN_TX_POWER_RATES - 1] = { .maximum = 18, .minimum = 18, .spread = 0 } },
	.table = {
		{ 1, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 2, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 3, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 4, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 5, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 6, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 7, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 8, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 9, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 10, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 11, 18, 18, 16, 14, 18, 16, 14, 17, 15, 13 },
		{ 12, 16, 16, 16, 14, 16, 16, 14, 17, 15, 13 },
		{ 13, 16, 16, 16, 14, 16, 16, 14, 17, 15, 13 },
		{ 14, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12 },
	},
};
uint8_t wlanTxPowerIncludeFirst __attribute__((section(".itcm.bss")));		/* 0x5541d */
void (*wlanHtOperationHook)(struct WlanConnection *connection) __attribute__((section(".itcm.bss")));	/* 0x54e64 */
void (*wlanApErpUpdateHook)(struct WlanConnection *connection) __attribute__((section(".itcm.bss")));	/* 0x54e80 */
void (*wlanApTemplateHook)(struct WlanConnection *connection) __attribute__((section(".itcm.bss")));	/* 0x54e84 */
void (*wlanSqramFrameHook)(struct WlanConnection *connection) __attribute__((section(".sqram1.bss")));	/* SQRAM 0xc0011bb0 */
void (*wlanSqramMacFilterGetHook)(char code, int count, const void *list, uint8_t *out) __attribute__((section(".sqram1.bss")));	/* SQRAM 0xc0011bb8 */
void (*wlanSqramMacFilterSetHook)(uint8_t *code, uint8_t *count, void *list, const uint8_t *in) __attribute__((section(".sqram1.bss")));	/* SQRAM 0xc0011bbc */

/* The MAC filter of a connection: whether a station's frames are refused (1) or not (0). */
int macMgmtCheckHeader(struct WlanConnection *connection, const uint8_t *address)
{
	struct WlanConnectionInfo *info = wlanConnectionInfo(connection);
	int listed = 0;
	unsigned int i;

	if (info->macFilterPolicy == 0)
		return 0;
	for (i = 0; info->macFilterCount > i; ++i) {
		if (memcmp(info->macFilterList[i], address, 6) == 0) {
			listed = 1;
			break;
		}
	}
	if (info->macFilterPolicy == 1) {
		if (listed == 1)
			return 0;
	} else if (info->macFilterPolicy == 2 && listed == 0) {
		return 0;
	}
	return 1;
}

void macMgmtProcessHeader(struct WlanConnectionInfo *info)
{
	info->macFilterPolicy = 0;
	info->macFilterCount = 0;
}

void macMgmtUpdateHeader(char code, int count, const void *list, uint8_t *out)
{
	out[0] = code;
	out[1] = count;
	__aeabi_memcpy(out + 2, list, 6 * count);
}

void macMgmtConfigureHeader(uint8_t *code, uint8_t *count, void *list, const uint8_t *in)
{
	*code = in[0];
	*count = in[1];
	__aeabi_memcpy(list, in + 2, 6 * in[1]);
}

void macMgmtValidateHeader(void)
{
	wlanSqramMacFilterGetHook = macMgmtUpdateHeader;
	wlanSqramMacFilterSetHook = macMgmtConfigureHeader;
}

/* Keeps a power of the host's channel power table (hostCmd00fb) for a channel; 0 when it is not one. */
__attribute__((noinline)) int macMgmtPrepareHeader(uint8_t channel, int band, unsigned int index, uint8_t power)
{
	unsigned int row;

	if (index >= WLAN_TX_POWER_INDICES + 1)
		return 0;
	if (band != 0)
		return 0;
	row = (uint8_t)(channel - 1);
	if (row >= WLAN_TX_POWER_CHANNELS)
		return 0;
	wlanTxPower.table[row][index] = power;
	return 1;
}

/* The host's channel power table: a list of TLVs 0x189, each a channel and 10 (index, power) pairs. */
int macMgmtDispatchHeader(const uint8_t *tlvs, unsigned int length)
{
	int changed = 0;

	while (length >= 4) {
		int remaining = length - 4;
		int size = (tlvs[3] << 8) | tlvs[2];

		if (size > remaining)
			break;
		if (((tlvs[1] << 8) | tlvs[0]) == 0x189 && size > 4) {
			uint8_t channel = tlvs[7];
			unsigned int band = wlanStartFrequencyBand((tlvs[5] << 8) | tlvs[4], channel);
			int i;
			int count = (size - 4) >> 1;

			for (i = 0; i < count; ++i) {
				const uint8_t *pair = &tlvs[2 * i];

				if (pair[8] < WLAN_TX_POWER_INDICES &&
				    macMgmtPrepareHeader(channel, band, (uint8_t)(pair[8] + 1), pair[9]) != 0)
					changed = 1;
			}
		}
		size = (tlvs[3] << 8) | tlvs[2];
		length = remaining - size;
		tlvs += size + 4;
	}
	return changed;
}

int macMgmtParseHeader(uint8_t *tlvs, uint16_t *length)
{
	uint8_t *start = tlvs;
	int i;

	*length = 0;
	if (tlvs == NULL || length == NULL)
		return 0;
	for (i = 0; i < WLAN_TX_POWER_CHANNELS; ++i) {
		unsigned int index = 0;
		int pairs = 0;
		uint8_t channel = wlanTxPower.table[i][0];

		tlvs[0] = 0x89;
		tlvs[1] = 1;
		wlanChannelDescriptor(tlvs + 4, 0, channel, 0);
		do {
			tlvs[2 * pairs + 8] = index;
			tlvs[2 * pairs + 9] = wlanTxPower.table[i][1 + index];
			index = (uint8_t)(index + 1);
			++pairs;
		} while (index < WLAN_TX_POWER_INDICES);
		tlvs[2] = (uint16_t)(2 * pairs + 4);
		tlvs[3] = (uint16_t)(2 * pairs + 4) >> 8;
		tlvs += (uint16_t)(2 * pairs + 4) + 4;
	}
	*length = tlvs - start;
	return 1;
}

int hostCmd00fb(struct WlanConnection *connection, unsigned int action, uint8_t *data, uint16_t *length)
{
	if (action == 1)
		return macMgmtDispatchHeader(data, *length);
	if (action == 0)
		return macMgmtParseHeader(data, length);
	return 1;
}

/* The limits are sent to the transmit side. */
void macMgmtComputeHeader(void)
{
	unsigned int i;

	for (i = 0; i < WLAN_TX_POWER_RATES; i = (uint8_t)(i + 1)) {
		wlanTxPowerClassMaximumSet(i, __aeabi_uread4(&wlanTxPower.limit[i].maximum));
		wlanTxPowerClassApply(i);
	}
}

/* Clamps what the host asked for to what the channel allows; *changed says a limit moved. */
int macMgmtQueryHeader(unsigned int channel, int force, int *changed)
{
	int result = 1;
	uint8_t flags[4];
	uint8_t band;
	uint8_t bandwidth;
	unsigned int i;

	*changed = wlanTxPower.channel == channel && force != 1 ? 0 : 1;
	if (*changed)
		wlanTxPower.channel = channel;
	wlanCurrentChannelFlags(flags);
	wlanChannelFlagsDecode(flags, &band, &bandwidth);
	for (i = 0;;) {
		int allowed = wlanTxPowerAllowed(channel, flags[0] & 3, i);
		int minimum = wlanTxPowerMinimum(channel, flags[0] & 3);
		struct WlanTxPowerLimit *request = &wlanTxPower.request[i];
		struct WlanTxPowerLimit *limit = &wlanTxPower.limit[i];
		int value;
		int spread;

		if (allowed < minimum)
			allowed = minimum;
		value = __aeabi_uread4(&request->maximum);
		if (value < minimum)
			value = minimum;
		else if (value > allowed)
			value = allowed;
		if ((int)__aeabi_uread4(&limit->maximum) != value) {
			__aeabi_uwrite4(value, &limit->maximum);
			*changed = 1;
		}
		value = __aeabi_uread4(&request->minimum);
		if (value < minimum)
			value = minimum;
		else if (value > allowed)
			value = allowed;
		if ((int)__aeabi_uread4(&limit->minimum) != value) {
			__aeabi_uwrite4(value, &limit->minimum);
			*changed = 1;
		}
		spread = __aeabi_uread4(&limit->maximum) - __aeabi_uread4(&limit->minimum);
		value = __aeabi_uread4(&request->spread);
		if (spread < value)
			value = spread;
		__aeabi_uwrite4(value, &limit->spread);
		i = (uint8_t)(i + 1);
		if (i >= WLAN_TX_POWER_RATES)
			break;
		if (i == 5 && (flags[0] & 3) == 0)
			channel = (uint8_t)(wlanHt40CenterChannel(band, channel, bandwidth) + 1);
	}
	return result;
}

int macMgmtResetHeader(int unused, unsigned int channel, int force)
{
	int changed;
	int result = macMgmtQueryHeader(channel, force, &changed);

	if (changed != 0)
		macMgmtComputeHeader();
	return result;
}

/* Back to the defaults. */
int macMgmtSyncHeader(struct WlanConnection *connection)
{
	wlanTxPowerIncludeFirst = 0;
	__aeabi_memcpy(wlanTxPower.request, wlanTxPower.defaults, sizeof(wlanTxPower.request));
	return macMgmtResetHeader((int)connection, 6, 0);
}

/* The highest of the transmit side's values over the rate classes. */
int macMgmtApplyHeader(void)
{
	int highest = 0;
	unsigned int i;

	for (i = 0; i < WLAN_TX_POWER_RATES; i = (uint8_t)(i + 1)) {
		int value = wlanTxPowerClassMaximumGet(i);

		if (highest >= value)
			value = highest;
		highest = value;
	}
	return highest;
}

/* Every rate class gets the same request, then the limits are made again. */
int macMgmtResolveHeader(struct WlanConnection *connection, int power)
{
	unsigned int channel = (uint8_t)rfGetChannel();
	unsigned int i;

	for (i = 0; i < WLAN_TX_POWER_RATES; i = (uint8_t)(i + 1)) {
		struct WlanTxPowerLimit *request = &wlanTxPower.request[i];
		int value = __aeabi_uwrite4(power, &request->minimum);

		__aeabi_uwrite4(value, &request->maximum);
		__aeabi_uwrite4(0, &request->spread);
	}
	return macMgmtResetHeader((int)connection, channel, 0);
}

/* The 802.11b/g limit by channel (SNMP object 17): channel 14 is limited to 12 dBm, 11 to 14, 1 to 15. */
void hostCmd0016Oid17(void)
{
	unsigned int channel = (uint8_t)rfGetChannel();
	int i;

	for (i = 0; i < WLAN_TX_POWER_RATES; ++i) {
		int power;
		struct WlanTxPowerLimit *limit;

		if (i <= 1 && i > 0) {	/* class 1, tested as Sony does: i > 1, then i <= 0 */
			power = channel == 14 ? 12 : 16;
		} else if (channel == 1) {
			power = 15;
		} else if (channel == 11) {
			power = 14;
		} else if (channel == 14) {
			power = 12;
		} else if (i == 2 || i == 5) {
			power = 15;
		} else {
			power = 16;
		}
		limit = &wlanTxPower.limit[i];
		__aeabi_uwrite4(power, &limit->maximum);
		if ((int)__aeabi_uread4(&limit->minimum) > power)
			__aeabi_uwrite4(power, &limit->minimum);
		wlanTxPowerClassMaximumSet((uint8_t)i, power);
		wlanTxPowerClassApply((uint8_t)i);
	}
}

/* The host's rate power table: records of 8 bytes (class, first rate, last rate, spread, minimum, maximum, 40 MHz). */
int hostCmd00d1Set(struct WlanConnection *connection, uint8_t *body)
{
	unsigned int channel = (uint8_t)rfGetChannel();
	unsigned int count;
	unsigned int i;
	unsigned int j;
	uint32_t action;

	if (body == NULL)
		return 0;
	count = ((body[11] << 8) | body[10]) >> 3;
	if (count > 8)
		return 0;
	action = __aeabi_uread4(body + 4);
	wlanTxPowerIncludeFirst = 0;
	if (action == 1) {
		for (i = 0; i < count; ++i) {
			const uint8_t *record = body + 12 + 8 * i;

			for (j = record[1]; record[2] >= j; j = (uint8_t)(j + 1)) {
				unsigned int index;
				struct WlanTxPowerLimit *request;

				if (record[6] != 0)
					index = wlanRateIndexHt40(record[0], j);
				else
					index = wlanRateIndexOf(record[0], j);
				request = &wlanTxPower.request[wlanTxPowerRateClass(index)];
				__aeabi_uwrite4((int8_t)record[5], &request->maximum);
				__aeabi_uwrite4((int8_t)record[4], &request->minimum);
				__aeabi_uwrite4((int8_t)record[3], &request->spread);
			}
		}
	} else {
		__aeabi_memcpy(wlanTxPower.request, wlanTxPower.defaults, sizeof(wlanTxPower.request));
	}
	return macMgmtResetHeader((int)connection, channel, 0);
}

unsigned int hostCmd00d1Get(uint8_t *body)
{
	uint8_t *record = body + 12;
	unsigned int count = 0;
	int i;

	__aeabi_uwrite4(1, body + 4);
	i = wlanTxPowerIncludeFirst == 0;
	do {
		const uint8_t *rate = wlanTxPower.rates[i];
		struct WlanTxPowerLimit *limit;
		unsigned int index;
		uint8_t first;
		uint8_t second;

		record[0] = first = rate[0];	/* the rate as read, before the record's stores */
		record[1] = second = rate[1];
		record[2] = rate[2];
		if (i < 8) {
			record[6] = 0;
			index = wlanRateIndexOf(first, second);
		} else {
			record[6] = 1;
			index = wlanRateIndexHt40(first, second);
		}
		limit = &wlanTxPower.limit[wlanTxPowerRateClass(index)];
		record[5] = __aeabi_uread4(&limit->maximum);
		record[4] = __aeabi_uread4(&limit->minimum);
		record[3] = __aeabi_uread4(&limit->spread);
		record += 8;
		count = (uint8_t)(count + 1);
	} while (++i < 11);
	body[8] = 0x54;
	body[9] = 1;
	body[10] = 8 * count;
	body[11] = (8 * count) >> 8;
	return 8 * count + 4;
}

/* The power the region's table gives a channel and a rate (index 0 of the policy bytes: the rate; 8..: its fallback). */
int macMgmtInspectHeader(unsigned int channel, unsigned int band, unsigned int rate)
{
	unsigned int first;
	unsigned int second;
	unsigned int row;
	const uint8_t *powers;
	int result;

	if (rate >= 8)
		rate = 2;
	first = (uint8_t)(wlanTxPower.policy[rate] + 1);
	row = (uint8_t)(channel - 1);
	second = (uint8_t)(wlanTxPower.policy[rate + 8] + 1);
	if (row >= WLAN_TX_POWER_CHANNELS)
		row = 6;
	powers = wlanTxPower.table[row];
	result = powers[first];
	if (second != 0) {
		int other = powers[second];

		if (result >= other)
			return other;
	}
	return result;
}

/* The limit of a channel without the region's table. */
int macMgmtTrackHeader(unsigned int channel, unsigned int band, unsigned int rate)
{
	if (band == 0) {
		if (rate == 1) {
			if (channel == 14)
				return 12;
		} else {
			switch (channel) {
			case 1:
				return 15;
			case 11:
				return 14;
			case 14:
				return 12;
			}
			if (rate == 2 || rate == 5)
				return 15;
		}
		return 16;
	}
	return 100;
}
