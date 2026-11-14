/*
 * Readers of the MAC event engine state (the antenna and path selection
 * state), and the per-channel calibration and transmit power lookups
 * (0xe63e..0xed04). The filename is inferred.
 */
#include <stddef.h>
#include "robin/calibration.h"
#include "robin/event.h"
#include "robin/phy.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/soc.h"

#define SOC_EVENT_CONTEXT	REG32(0x800020e4)	/* bit 0: event pattern pending */

#define WLAN_TX_POWER_DEFAULT_MAX	20	/* dBm, without a record */
#define WLAN_CHANNEL_WRAP		200	/* channel numbers wrap past 200 */
#define WLAN_RF_CONTEXT_DEFAULT		0x40
#define WLAN_BANDWIDTH_40		2
#define WLAN_SECONDARY_ABOVE		2
#define WLAN_SECONDARY_BELOW		3

/* Not converted yet; provisional names. */
unsigned int uapSqramBuildEntry(unsigned int band, unsigned int channel);	/* 0xc0008f81 */
int uapSqramValidateEntry(int low, int high, int lowPoint, int highPoint, int point); /* 0xc0008ef3 */
unsigned int uapSqramComputeEntry(unsigned int secondary);			/* 0xc0008fbb */
void uapSqramCheckTable(const uint8_t *data, unsigned int offset, unsigned int length,
			unsigned int action);					/* 0xc0009cc5 */
int uapSqramNotifyTable(const uint8_t *data, unsigned int offset, unsigned int length,
			unsigned int action);					/* 0xc000a225 */
int uapSqramQueryTable(uint8_t *data, uint16_t *length);			/* 0xc0009fb9 */
int rfCalibSyncHeader(unsigned int index, uint8_t *value);			/* 0x29e7e */
int rfCalibScheduleHeader(unsigned int index, uint8_t *value);			/* 0x29e9a */
int rfCalibInspectHeader(unsigned int index, uint16_t *value);			/* 0x29eba */
int rfCalibAcquireHeader(unsigned int index, uint16_t *value);			/* 0x29edc */
int rfCalibClearHeader(unsigned int index, uint16_t *value);			/* 0x29efe */
int rfCalibSelectHeader(unsigned int index, int8_t *values);			/* 0x29f1e */
int rfCalibFilterHeader(unsigned int index, uint8_t *block);			/* 0x29f3c */
int rfCalibUpdateDescriptor(unsigned int index, struct WlanTxPowerRecord *record); /* 0x29f7e */
int rfCalibValidateDescriptor(unsigned int index, unsigned int secondary, int8_t *adjust); /* 0x29fee */
int rfCalibParseDescriptor(unsigned int index, unsigned int mode, unsigned int power,
			   struct WlanTxPowerLevels *levels);			/* 0x2a0ac */
int rfCalibQueryDescriptor(unsigned int index, unsigned int mode, unsigned int range,
			   unsigned int channelIndex, int8_t *adjust);		/* 0x2a182 */
int macTxNotifyScheduleFrame(unsigned int index, uint8_t *values);		/* 0x3362a */
int macTxNotifyFormatFrame(unsigned int index, struct WlanCalibrationChannelValues *values); /* 0x33662 */
void macMgmtSyncHeader(int unused);						/* 0x3b4d2 */
void macMgmtQueryHeader(int kind, int count, void *value);			/* 0x3b384 */
void wlanPsConvertHeader(void);							/* 0x3e32e */
void rfCalibComputeContext(unsigned int value);					/* 0x26984 */
extern uint8_t *wlanAntennaProfile;	/* 0x040088ca, unaligned; byte 3: bits 0-1 primary, 2-3 secondary */

struct MacEventState macEventState __attribute__((section(".dtcm.bss")));
uint8_t macEventProfileActive __attribute__((section(".dtcm.rw")));
/* Set when the calibration data carried a type 60 entry (uapSqramParseTable). */
uint8_t wlanCalibrationType60Present __attribute__((section(".dtcm.rw")));

/* State of the secondary path, or 3 while no profile is active. */
int macGetEventSecondaryPathState(void)
{
	if (macEventProfileActive != 0)
		return macEventState.pathStates >> 4;
	return MAC_EVENT_PATH_STATE_UNKNOWN;
}

/* State of the primary path, or 3 while no profile is active. */
int macGetEventPrimaryPathState(void)
{
	if (macEventProfileActive != 0)
		return macEventState.pathStates & 0xf;
	return MAC_EVENT_PATH_STATE_UNKNOWN;
}

/* The engine's level while it runs with flag bit 3 set and both paths unknown, else 0x51. */
int macGetEventLevel(void)
{
	if (macEventProfileActive != 0 && macEventState.state == MAC_EVENT_STATE_RUNNING &&
	    (macEventState.flags & 8) != 0 &&
	    macGetEventPrimaryPathState() == MAC_EVENT_PATH_STATE_UNKNOWN &&
	    macGetEventSecondaryPathState() == MAC_EVENT_PATH_STATE_UNKNOWN)
		return macEventState.level;
	return MAC_EVENT_LEVEL_DEFAULT;
}

/* The highest transmit power the band's record allows on a channel. */
unsigned int wlanTxPowerMax(unsigned int channel, unsigned int band)
{
	struct WlanTxPowerRecord record;

	if (rfCalibUpdateDescriptor(uapSqramBuildEntry(band, channel), &record))
		return record.maximum;
	return WLAN_TX_POWER_DEFAULT_MAX;
}

/* The lowest transmit power the band's record allows on a channel. */
unsigned int wlanTxPowerMin(unsigned int channel, unsigned int band)
{
	struct WlanTxPowerRecord record;

	if (rfCalibUpdateDescriptor(uapSqramBuildEntry(band, channel), &record))
		return record.minimum;
	return 0;
}

unsigned int macGetEventPayloadMode(void)
{
	return (macEventState.payload >> 4) & 3;
}

uint8_t wlanCalibrationType60Loaded(void)
{
	return wlanCalibrationType60Present;
}

/* Choose the primary antenna: by the path state with diversity, else as configured. */
void wlanAntennaSelectPrimary(void)
{
	if ((((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[3] & 2) != 0) {
		wlanRadioConfig->primaryDiversity = 1;
		switch (macGetEventPrimaryPathState()) {
		case 3:
		case 5:
		case 7:
			wlanRadioConfig->primaryAntenna = 0;
			break;
		case 6:
			wlanRadioConfig->primaryAntenna = 1;
			break;
		}
	} else {
		wlanRadioConfig->primaryDiversity = 0;
		wlanRadioConfig->primaryAntenna =
			((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[3] & 1;
	}
}

/* The same for the secondary antenna (profile bits 2-3). */
void wlanAntennaSelectSecondary(void)
{
	if ((((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[3] & 8) != 0) {
		wlanRadioConfig->secondaryDiversity = 1;
		switch (macGetEventSecondaryPathState()) {
		case 3:
		case 5:
		case 7:
			wlanRadioConfig->secondaryAntenna = 0;
			break;
		case 6:
			wlanRadioConfig->secondaryAntenna = 1;
			break;
		}
	} else {
		wlanRadioConfig->secondaryDiversity = 0;
		wlanRadioConfig->secondaryAntenna =
			(((const uint8_t *)__aeabi_uread4(&wlanAntennaProfile))[3] >> 2) & 1;
	}
}

void wlanAntennaSelect(void)
{
	wlanAntennaSelectPrimary();
	wlanAntennaSelectSecondary();
}

/* The index of a regulatory region code in the region tables. */
unsigned int wlanRegionTableIndex(unsigned int code)
{
	switch (code) {
	case 0x11:
		return 0;
	case 0x12:
		return 1;
	case 0x21:
		return 4;
	case 0x22:
		return 5;
	case 0x31:
		return 8;
	case 0x32:
		return 9;
	case 0x33:
		return 10;
	case 0x77:
		return 0x77;
	default:
		return 0;
	}
}

int wlanCalibrationLookupByte(unsigned int index, uint8_t *value)
{
	uint8_t byte;

	if (rfCalibScheduleHeader(index, &byte)) {
		*value = byte;
		return 1;
	}
	return 0;
}

/* Program the RF context for the event engine's current pattern. */
void wlanRfApplyEventContext(unsigned int index)
{
	uint8_t value;

	if (macEventProfileActive == 0)
		return;
	if (macEventState.payload & 1) {
		SOC_EVENT_CONTEXT |= 1;
		value = 0;
	} else {
		SOC_EVENT_CONTEXT &= ~1u;
		if (!wlanCalibrationLookupByte(index, &value))
			value = WLAN_RF_CONTEXT_DEFAULT;
	}
	rfCalibComputeContext(value);
}

int macGetEventPolicy(struct MacEventPolicy *policy)
{
	if (policy == NULL || macEventProfileActive == 0 ||
	    macEventState.state != MAC_EVENT_STATE_RUNNING)
		return 0;
	policy->mode = macEventState.mode;
	policy->flags = macEventState.flags;
	policy->pathStates = macEventState.policyPathStates;
	return 1;
}

int macGetEventPathConfiguration(uint32_t path, struct MacEventPathConfiguration *configuration)
{
	if (configuration == NULL || macEventProfileActive == 0 ||
	    macEventState.state != MAC_EVENT_STATE_RUNNING || path != 0)
		return 0;
	__aeabi_memcpy(configuration, &macEventState.configuration, sizeof(*configuration));
	return 1;
}

int wlanRfEventRefresh(void)
{
	uint32_t value;

	wlanRfApplyEventContext(0);
	macMgmtSyncHeader(0);
	macMgmtQueryHeader(6, 1, &value);
	wlanPsConvertHeader();
	return 0;
}

/* HostCmd_CMD_CFG_DATA, the calibration table: writes the host's data into it. */
int wlanCalibrationTableUpdate(const uint8_t *data, unsigned int offset, unsigned int length,
			       unsigned int action)
{
	uapSqramCheckTable(data, offset, length, action);
	if (!uapSqramNotifyTable(data, offset, length, action))
		wlanRfEventRefresh();
	return 0;
}

/* HostCmd_CMD_CFG_DATA, the calibration table: copies it out with its length. */
int wlanCalibrationTableQuery(uint8_t *data, uint16_t *length)
{
	return uapSqramQueryTable(data, length);
}

/*
 * The measured channels around a channel: the channel itself when it was
 * measured, else the two neighbours it lies between (channel numbers wrap
 * past 200).
 */
int wlanCalibrationChannelBracket(const uint8_t *channels, unsigned int count, unsigned int channel,
				  uint8_t *low, uint8_t *high, uint8_t *lowIndex,
				  uint8_t *highIndex)
{
	uint8_t i;

	for (i = 0; i < count; i++) {
		unsigned int current = channels[i];
		unsigned int next, target;

		if (current == channel) {
			*low = channels[i];
			*high = channels[i];
			*lowIndex = i;
			*highIndex = i;
			return 1;
		}
		if (i == count - 1)
			return 0;
		next = channels[i + 1];
		target = channel;
		if (next < current)
			next += WLAN_CHANNEL_WRAP;
		if (channel < current)
			target = channel + WLAN_CHANNEL_WRAP;
		if (target > current && target < next) {
			*low = channels[i];
			*high = channels[i + 1];
			*lowIndex = i;
			*highIndex = i + 1;
			return 1;
		}
	}
	return 0;
}

/* The power range of a record a power falls in. */
int wlanTxPowerRange(const struct WlanTxPowerRecord *record, unsigned int power, uint8_t *range)
{
	uint8_t i;

	for (i = 0; i < record->ranges; i++) {
		if (record->rangeLow[i] <= power && power <= record->rangeHigh[i]) {
			*range = i;
			return 1;
		}
	}
	return 0;
}

/*
 * The power levels for a transmit power request: the record's levels for
 * the power, adjusted by the channel's offsets (interpolated between the
 * measured channels) and, at 40 MHz, by the secondary channel's offsets.
 */
int wlanTxPowerLookup(struct WlanCalibrationKey key, struct WlanTxPowerRequest request,
		      uint8_t *levels)
{
	struct WlanTxPowerRecord record;
	struct WlanTxPowerLevels base;
	uint8_t low, high, lowIndex, highIndex, range;
	int8_t adjust[2], other[2];
	unsigned int index, mode, power, secondary, channel;

	index = uapSqramBuildEntry(key.band, key.channel);
	if (!rfCalibUpdateDescriptor(index, &record))
		return 0;
	if (request.power > record.maximum)
		request.power = record.maximum;
	else if (request.power < record.minimum)
		request.power = record.minimum;
	mode = request.bandwidth;
	if (request.bandwidth == WLAN_BANDWIDTH_40)
		mode = 1;
	power = request.power;
	if (!rfCalibParseDescriptor(index, mode, request.power, &base))
		return 0;

	secondary = request.secondary;
	if (request.bandwidth == WLAN_BANDWIDTH_40 &&
	    (secondary == WLAN_SECONDARY_ABOVE || secondary == WLAN_SECONDARY_BELOW)) {
		key.channel = wlanHt40CenterChannel(key.band, key.channel, secondary);
		if (key.band == 0)
			key.channel++;
	}
	channel = key.channel;
	if (!wlanCalibrationChannelBracket(record.channelList, record.channels, key.channel,
					   &low, &high, &lowIndex, &highIndex) ||
	    !wlanTxPowerRange(&record, power, &range) ||
	    !rfCalibQueryDescriptor(index, mode, range, lowIndex, adjust))
		return 0;

	if (lowIndex != highIndex) {
		unsigned int highPoint = high;

		if (!rfCalibQueryDescriptor(index, mode, range, highIndex, other))
			return 0;
		if (high < low)
			highPoint = high + WLAN_CHANNEL_WRAP;
		if (channel < low)
			channel += WLAN_CHANNEL_WRAP;
		adjust[0] = uapSqramValidateEntry(adjust[0], other[0], low, highPoint, channel);
		adjust[1] = uapSqramValidateEntry(adjust[1], other[1], low, highPoint, channel);
	}
	base.first += adjust[1];
	base.second += adjust[1];
	base.third = ((base.third + adjust[0]) & 0x7f) | (base.third & 0x80);

	if (request.bandwidth == WLAN_BANDWIDTH_40) {
		if (!rfCalibValidateDescriptor(index, uapSqramComputeEntry(secondary), other))
			return 0;
		base.first += other[0];
		base.second += other[0];
		base.third = ((base.third + other[1]) & 0x7f) | (base.third & 0x80);
	}
	levels[0] = base.first;
	levels[1] = base.second;
	levels[2] = base.third & 0x7f;
	return 1;
}

int wlanCalibrationLookupBlock(struct WlanCalibrationKey key, void *block)
{
	uint8_t buffer[8];

	if (!rfCalibFilterHeader(uapSqramBuildEntry(key.band, key.channel), buffer))
		return 0;
	__aeabi_memcpy(block, buffer, sizeof(buffer));
	return 1;
}

int wlanCalibrationLookupQuad(struct WlanCalibrationKey key, void *values)
{
	uint8_t buffer[4];

	if (!macTxNotifyScheduleFrame(uapSqramBuildEntry(key.band, key.channel), buffer))
		return 0;
	__aeabi_memcpy(values, buffer, sizeof(buffer));
	return 1;
}

/* Linear interpolation between (lowPoint, low) and (highPoint, high), rounded. */
int wlanInterpolate(int low, int high, int lowPoint, int highPoint, int point)
{
	if (low == high || point == lowPoint)
		return low;
	if (point == highPoint)
		return high;
	return low + ((((high - low) * 8) * (point - lowPoint) / (highPoint - lowPoint) + 4) >> 3);
}

/* Eight bytes of per-channel values, the last halfword interpolated for the channel. */
int wlanCalibrationLookupInterpolated(struct WlanCalibrationKey key, uint8_t *values)
{
	struct WlanCalibrationChannelValues buffer;
	struct WlanTxPowerRecord record;
	uint8_t low, high, lowIndex, highIndex;
	unsigned int index, channel = key.channel;
	uint16_t value;

	index = uapSqramBuildEntry(key.band, key.channel);
	if (!macTxNotifyFormatFrame(index, &buffer) ||
	    !rfCalibUpdateDescriptor(index, &record) ||
	    !wlanCalibrationChannelBracket(record.channelList, record.channels, channel,
					   &low, &high, &lowIndex, &highIndex))
		return 0;
	__aeabi_memcpy(values, &buffer, 8);
	if (lowIndex == highIndex) {
		value = buffer.values[lowIndex];
	} else {
		unsigned int highPoint = high;

		if (high < low)
			highPoint = high + WLAN_CHANNEL_WRAP;
		if (channel < low)
			channel += WLAN_CHANNEL_WRAP;
		value = wlanInterpolate(buffer.values[lowIndex], buffer.values[highIndex], low,
					highPoint, channel);
	}
	((uint16_t *)values)[3] = value;
	return 1;
}

int wlanCalibrationLookup1(struct WlanCalibrationKey key, uint16_t *value)
{
	uint16_t result;

	if (!rfCalibInspectHeader(uapSqramBuildEntry(key.band, key.channel), &result))
		return 0;
	*value = result;
	return 1;
}

int wlanCalibrationLookup0(struct WlanCalibrationKey key, uint16_t *value)
{
	uint16_t result;

	if (!rfCalibAcquireHeader(uapSqramBuildEntry(key.band, key.channel), &result))
		return 0;
	*value = result;
	return 1;
}

int wlanCalibrationLookup2(struct WlanCalibrationKey key, uint16_t *value)
{
	uint16_t result;

	if (!rfCalibClearHeader(uapSqramBuildEntry(key.band, key.channel), &result))
		return 0;
	*value = result;
	return 1;
}

int wlanCalibrationLookupFlags(unsigned int index, uint8_t *flags)
{
	uint8_t value;

	if (!rfCalibSyncHeader(index, &value))
		return 0;
	flags[0] = value & 1;
	flags[1] = (value & 2) != 0;
	flags[2] = (value & 4) != 0;
	return 1;
}

/* The signal calibration of a channel, spread over the eight values phy.c reads. */
int wlanCalibrationLookupSignal(struct WlanCalibrationKey key, int8_t *values)
{
	int8_t buffer[4];

	if (!rfCalibSelectHeader(uapSqramBuildEntry(key.band, key.channel), buffer))
		return 0;
	if (buffer[3] == buffer[2])
		buffer[0] = buffer[1];
	values[0] = buffer[3];
	values[1] = buffer[1];
	values[2] = buffer[3];
	values[3] = buffer[3];
	values[4] = buffer[2];
	values[5] = buffer[0];
	values[7] = buffer[2];
	values[6] = buffer[2];
	return 1;
}
