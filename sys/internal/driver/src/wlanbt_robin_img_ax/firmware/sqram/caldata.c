/*
 * The calibration data decoder (SQRAM 0xc0008ea4..0xc000974c): the data is a chain
 * of annexes, each with an 8-byte header (its type, its length and the
 * offset of the next), read a big-endian word at a time from the image
 * calDataImage points to. Each annex type has a loader that checks the
 * annex's checksum and puts what it carries into the tables of
 * sys/calibdata.c. Marvell's calibration files name the entries by these
 * annex numbers; no source of the decoder survives, and the filename and
 * the names are inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/calibration.h"
#include "robin/rom.h"
#include "robin/util.h"

#define CAL_DATA_BAND_INDEXES	2
#define CAL_DATA_NO_INDEX	0xff
#define CAL_DATA_CHANNEL_14	14
#define CAL_DATA_RANGES		4
#define CAL_DATA_CHANNELS	6
#define CAL_DATA_POWER_LEVELS	21

/* The header every annex starts with. */
struct CalDataHeader {
	uint8_t type;
	uint8_t reserved;
	uint16_t length;	/* the annex's bytes, the header's included */
	uint32_t next;		/* image offset of the next annex; 0xffffffff ends the chain */
};

_Static_assert(sizeof(struct CalDataHeader) == 8, "annex header");

/* Annexes 14 and 37: the 2.4 GHz channels' power ranges, for the first and the second path. */
struct CalDataAnnex14 {
	struct CalDataHeader header;
	uint8_t reserved8[2];
	uint8_t flags;			/* bit 6: the flag kept in bit 0 of wlanCalibFlags */
	uint8_t context;
	uint8_t reserved12;
	uint8_t channels;		/* channels in channelList, at most 6 */
	uint8_t channelList[CAL_DATA_CHANNELS];	/* channel 14 goes to band index 1 */
	uint8_t reserved20[8];
	struct WlanCalibAdjust rangeAdjust[CAL_DATA_RANGES][CAL_DATA_CHANNELS];
	uint16_t value1[2];		/* band indexes 0 and 1 */
	uint8_t reserved80[12];
};

_Static_assert(offsetof(struct CalDataAnnex14, rangeAdjust) == 28, "annex 14");
_Static_assert(sizeof(struct CalDataAnnex14) == 92, "annex 14");

/* One power range of annex 16. */
struct CalDataAnnex16Range {
	uint8_t reserved[2];
	uint8_t high;
	uint8_t low;
};

/* Annex 16: a band index's frequencies, power ranges and the levels of each transmit power. */
struct CalDataAnnex16 {
	struct CalDataHeader header;
	uint8_t reserved8[2];
	uint8_t mode;			/* 0: the levels are kept for both modes, 1: for mode 1 only */
	uint8_t index;			/* the band index; below 2 in mode 0, below 5 in mode 1 */
	uint16_t frequencyHigh;
	uint16_t frequencyLow;
	struct CalDataAnnex16Range ranges[CAL_DATA_RANGES];
	struct WlanTxPowerLevels levels[CAL_DATA_POWER_LEVELS];	/* first byte: the level's index */
};

_Static_assert(offsetof(struct CalDataAnnex16, levels) == 32, "annex 16");
_Static_assert(sizeof(struct CalDataAnnex16) == 116, "annex 16");

/* Annex 18: value 0 of both band indexes. */
struct CalDataAnnex18 {
	struct CalDataHeader header;
	uint16_t value0[2][4];		/* the first halfword of each is used */
};

_Static_assert(sizeof(struct CalDataAnnex18) == 24, "annex 18");

/* Annexes 31 and 47: the signal values of both band indexes. */
struct CalDataAnnex31 {
	struct CalDataHeader header;
	uint8_t reserved8[3];
	uint8_t flags;			/* bit 1: the signal values are given */
	struct WlanCalibSignal signal[2];
};

_Static_assert(sizeof(struct CalDataAnnex31) == 20, "annex 31");

/* One entry of annex 42; the bytes that go into a block (see calDataLoadAnnex42()). */
struct CalDataAnnex42Entry {
	uint8_t bytes[12];		/* byte 0, bits 0-3: 0 for band index 0, 2 for both */
};

/* Annex 42: up to five entries of the transmit limit curve (wlanCalibBlock). */
struct CalDataAnnex42 {
	struct CalDataHeader header;
	struct CalDataAnnex42Entry entries[5];
};

_Static_assert(sizeof(struct CalDataAnnex42) == 68, "annex 42");

/*
 * Annex 49: band index 0 as one table: its channels, the power range
 * adjustments of both paths, and up to four transmit power levels (highest
 * first) between which the others are interpolated.
 */
struct CalDataAnnex49 {
	struct CalDataHeader header;
	uint8_t reserved8[2];
	uint8_t levelCount;
	uint8_t channels;
	uint8_t flags;
	uint8_t context;
	int8_t levelDelta;		/* each level's spread: its value plus and minus this */
	uint8_t reserved15;
	struct WlanCalibAdjust secondary[2];
	uint8_t channelList[4];
	struct WlanCalibAdjust rangeAdjust[2][3][4];	/* by path, range and channel */
	struct WlanTxPowerLevels levels[4];
};

_Static_assert(offsetof(struct CalDataAnnex49, rangeAdjust) == 24, "annex 49");
_Static_assert(sizeof(struct CalDataAnnex49) == 88, "annex 49");

/* Annexes 52, 53 and 54: one value of band index 0 (value 2, 1 and 0). */
struct CalDataAnnexValue {
	struct CalDataHeader header;
	uint8_t reserved8;
	uint8_t flags;			/* bit 0: the value is given */
	uint16_t value;
	uint8_t reserved12[8];
};

_Static_assert(sizeof(struct CalDataAnnexValue) == 20, "annexes 52 to 54");

#define CAL_DATA_ANNEX42_BAND_INDEX0	0
#define CAL_DATA_ANNEX42_BOTH		2

/* An annex is good when its bytes add up to 1. */
__attribute__((noinline)) int calDataChecksumOk(const uint8_t *data, unsigned int length)
{
	const uint8_t *end = data + length;
	uint8_t sum = 0;

	while (data < end)
		sum += *data++;
	return sum == 1;
}

/* Reads `length` bytes of the image at image offset `offset`, a byte-swapped word at a time. */
__attribute__((noinline)) int calDataRead(int fromHost, void *buffer, unsigned int offset, unsigned int length)
{
	uint32_t *output = buffer;
	unsigned int position = offset - *calDataImageOffset;
	uint16_t i;

	for (i = 0; i < length; i += 4)
		*output++ = util_Swap32(*(const uint32_t *)(calDataImage + position + i));
	return 1;
}

/* The value at `point` on the line through (lowPoint, low) and (highPoint, high), rounded. */
int16_t calDataInterpolate(int low, int high, int lowPoint, int highPoint, int point)
{
	if (low == high || point == lowPoint)
		return low;
	if (point == highPoint)
		return high;
	return (int16_t)(low + ((int16_t)((int16_t)((high - low) * 8 * (point - lowPoint)) /
					  (int16_t)(highPoint - lowPoint) + 4) >> 3));
}

/* The frequency of channel 0 of the channel's band, in MHz. */
__attribute__((noinline)) void calDataChannelBase(unsigned int channel, unsigned int band, uint16_t *base)
{
	if (channel == CAL_DATA_CHANNEL_14)
		*base = 2414;
	else if (channel < CAL_DATA_CHANNEL_14 && band == 0)
		*base = 2407;
	else if ((channel <= 16 && band == 1) || channel < 166)
		*base = 5000;
	else
		*base = 4000;
}

/* A channel's frequency, in MHz. */
__attribute__((noinline)) uint16_t calDataChannelFrequency(unsigned int band, unsigned int channel)
{
	uint16_t base;

	calDataChannelBase(channel, band, &base);
	return channel * 5 + base;
}

unsigned int calDataChannelNumber(unsigned int band, unsigned int channel)
{
	return channel;
}

/* The band index whose power record covers the channel's frequency, or 0xff. */
unsigned int calDataBandIndex(unsigned int band, unsigned int channel)
{
	struct WlanTxPowerRecord record;
	uint16_t frequency = calDataChannelFrequency(band, channel);
	uint8_t i;

	for (i = 0; i < CAL_DATA_BAND_INDEXES; i++)
		if (wlanCalibPowerGet(i, &record) && record.frequencyLow <= frequency &&
		    record.frequencyHigh >= frequency)
			return i;
	return CAL_DATA_NO_INDEX;
}

/* The secondary channel's index in the adjustment tables: 0 for none, 1 above or below. */
unsigned int calDataSecondaryIndex(unsigned int secondary)
{
	if (secondary == 1)
		return 0;
	if (secondary == 2 || secondary == 3)
		return 1;
	return CAL_DATA_NO_INDEX;
}

/*
 * Annexes 14 and 37: the measured channels become band index 0's power
 * record (channel 14, when listed, band index 1's), then the context, the
 * flag, the range adjustments by channel and value 1 of each.
 */
int calDataLoadAnnex14(int fromHost, unsigned int offset)
{
	struct CalDataAnnex14 annex;
	struct WlanTxPowerRecord record;
	struct WlanTxPowerRecord record14;
	struct WlanCalibAdjust none;
	uint8_t channelList[8] = { 0 };
	int secondPath;
	int hasChannel14 = 0;
	unsigned int channel14Position = 5;
	unsigned int channels, kept, i, range, channel, target, position;
	unsigned int index = 0, index14 = 1;
	uint32_t flags;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	flags = 0;
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	secondPath = annex.header.type == 37;
	channels = annex.channels < CAL_DATA_CHANNELS ? annex.channels : CAL_DATA_CHANNELS;
	kept = 0;
	for (i = 0; i < channels; i = (uint8_t)(i + 1)) {
		if (annex.channelList[i] == CAL_DATA_CHANNEL_14) {
			hasChannel14 = 1;
			channel14Position = i;
			break;
		}
		channelList[kept] = annex.channelList[i];
		kept = (uint8_t)(kept + 1);
	}

	if (!wlanCalibPowerGet(index, &record))
		__aeabi_memclr4(&record, sizeof(record));
	record.ranges = CAL_DATA_RANGES;
	record.channels = channels - (hasChannel14 != 0);
	__aeabi_memcpy4(record.channelList, channelList, record.channels);
	wlanCalibPowerSet(index, &record);
	if (hasChannel14) {
		if (!wlanCalibPowerGet(index14, &record14))
			__aeabi_memclr4(&record14, sizeof(record14));
		record14.ranges = CAL_DATA_RANGES;
		record14.channels = 1;
		record14.channelList[0] = CAL_DATA_CHANNEL_14;
		wlanCalibPowerSet(1, &record14);
	}

	wlanCalibContextSet(0, &annex.context);
	flags = calDataFlagsHook(0);
	flags = ((uint8_t)flags & ~1u) | ((annex.flags >> 6) & 1);
	wlanCalibFlagsSet(0, (const uint8_t *)&flags);
	for (range = 0; range < CAL_DATA_RANGES; range = (uint8_t)(range + 1)) {
		for (channel = 0; channel < channels; channel = (uint8_t)(channel + 1)) {
			if (hasChannel14 && channel == channel14Position) {
				target = index14;
				position = 0;
			} else {
				target = index;
				position = (uint8_t)(channel > channel14Position ? channel - 1 : channel);
			}
			wlanCalibRangeAdjustSet(target, secondPath, range, position,
						&annex.rangeAdjust[range][channel]);
		}
	}

	wlanCalibValue1Set(index, &annex.value1[0]);
	none.value[0] = none.value[1] = 0;
	wlanCalibSecondarySet(index, 0, &none);
	wlanCalibSecondarySet(index, 1, &none);
	if (hasChannel14) {
		wlanCalibValue1Set(index14, &annex.value1[1]);
		none.value[0] = none.value[1] = 0;
		wlanCalibSecondarySet(index14, 0, &none);
		wlanCalibSecondarySet(index14, 1, &none);
	}
	return 1;
}

/*
 * Annex 16: a band index's frequency range, power ranges and transmit power
 * levels. The record's minimum and maximum become the first and last level
 * listed; an empty level (0xff, 0xff, 0x7f) before the first is skipped and
 * one after it ends the list.
 */
int calDataLoadAnnex16(int fromHost, unsigned int offset)
{
	struct CalDataAnnex16 annex;
	struct WlanTxPowerRecord record;
	unsigned int length, mode, index, i;
	int found;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	mode = annex.mode;
	if (mode >= 2)
		return 0;
	if (mode == 0) {
		if (annex.index >= 2)
			return 0;
	} else if (mode == 1 && annex.index >= 5) {
		return 0;
	}
	index = annex.index;
	if (!wlanCalibPowerGet(index, &record))
		__aeabi_memclr4(&record, sizeof(record));
	record.frequencyLow = annex.frequencyLow;
	record.frequencyHigh = annex.frequencyHigh;
	record.ranges = CAL_DATA_RANGES;
	for (i = 0; i < CAL_DATA_RANGES; i = (uint8_t)(i + 1)) {
		record.rangeHigh[i] = annex.ranges[i].high;
		record.rangeLow[i] = annex.ranges[i].low;
	}

	found = 0;
	for (i = 0; i < CAL_DATA_POWER_LEVELS; i = (uint8_t)(i + 1)) {
		struct WlanTxPowerLevels *levels = &annex.levels[i];

		if ((levels->third & 0x7f) == 0x7f && levels->first == 0xff && levels->second == 0xff) {
			if (found)
				break;
			record.minimum = i + 1;
		} else if (levels->index == i) {
			if (!found) {
				found = 1;
				record.minimum = i;
			}
			record.maximum = i;
			if (mode == 0)
				wlanCalibLevelsSet(index, 0, i, levels);
			wlanCalibLevelsSet(index, 1, i, levels);
		}
	}
	wlanCalibPowerSet(index, &record);
	return 1;
}

/* Annex 18: value 0 of band indexes 0 and 1. */
int calDataLoadAnnex18(int fromHost, unsigned int offset)
{
	struct CalDataAnnex18 annex;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	wlanCalibValue0Set(0, annex.value0[0]);
	wlanCalibValue0Set(1, annex.value0[1]);
	return 1;
}

/* Annexes 31 and 47: the signal values of band indexes 0 and 1, when the annex gives them. */
int calDataLoadAnnex31(int fromHost, unsigned int offset)
{
	struct CalDataAnnex31 annex;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	if (annex.flags & 2) {
		wlanCalibSignalSet(0, &annex.signal[0]);
		wlanCalibSignalSet(1, &annex.signal[1]);
	}
	return 1;
}

/*
 * Annex 42: each entry gives the transmit limit curve of band index 0, or
 * of both (which ends the annex); seven of its bytes make the block.
 */
int calDataLoadAnnex42(int fromHost, unsigned int offset)
{
	struct CalDataAnnex42 annex;
	uint8_t block[8] = { 0 };
	unsigned int length, remaining, i, index;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	remaining = (uint16_t)(length - sizeof(annex.header));
	for (i = 0; remaining >= sizeof(struct CalDataAnnex42Entry) && i < 5; ) {
		const uint8_t *entry = annex.entries[i].bytes;

		block[0] = entry[0];
		block[2] = entry[2];
		block[3] = entry[3];
		block[4] = entry[5];
		block[5] = entry[11];
		block[6] = entry[6];
		block[7] = entry[10];
		if ((entry[0] & 0xf) == CAL_DATA_ANNEX42_BAND_INDEX0) {
			wlanCalibBlockSet(0, block);
		} else if ((entry[0] & 0xf) == CAL_DATA_ANNEX42_BOTH) {
			for (index = 0; index < CAL_DATA_BAND_INDEXES; index = (uint8_t)(index + 1))
				wlanCalibBlockSet(index, block);
			break;
		}
		remaining = (uint16_t)(remaining - sizeof(struct CalDataAnnex42Entry));
		i = (uint8_t)(i + 1);
	}
	return 1;
}

/* Sets the levels of a power for both modes: mode 0 from `second`, mode 1 from `first`, each spread by the delta. */
static inline __attribute__((always_inline)) void calDataSetLevelPair(struct WlanTxPowerLevels *levels,
								      unsigned int index, unsigned int power,
								      int value0, int value1, int delta)
{
	levels->first = value0 + delta;
	levels->second = value0 - delta;
	wlanCalibLevelsSet(index, 0, power, levels);
	levels->first = value1 + delta;
	levels->second = value1 - delta;
	wlanCalibLevelsSet(index, 1, power, levels);
}

/*
 * Annex 49: band index 0's frequencies (channels 1 to 14), channels, power
 * ranges (each from one listed level down to the next), context, flags,
 * secondary and range adjustments, and the levels of every power from the
 * highest listed to the lowest, the ones between two listed levels
 * interpolated.
 */
int calDataLoadAnnex49(int fromHost, unsigned int offset)
{
	struct CalDataAnnex49 annex;
	struct WlanTxPowerRecord record;
	struct WlanTxPowerLevels levels;
	unsigned int index = 0;
	unsigned int length, count, i, k, power;
	int segment, segments;
	int highThird, lowThird;
	unsigned int highPower, lowPower;
	int value;

	__aeabi_memclr4(&annex, sizeof(annex));
	__aeabi_memclr4(&record, sizeof(record));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	record.frequencyLow = calDataChannelFrequency(0, 1);
	record.frequencyHigh = calDataChannelFrequency(0, CAL_DATA_CHANNEL_14);
	record.channels = annex.channels < 4 ? annex.channels : 4;
	__aeabi_memcpy4(record.channelList, annex.channelList,
			record.channels < CAL_DATA_CHANNELS ? record.channels : CAL_DATA_CHANNELS);
	count = annex.levelCount < 4 ? annex.levelCount : 4;
	if (count < 2)
		return 0;

	record.maximum = annex.levels[0].index;
	record.minimum = annex.levels[count - 1].index;
	record.ranges = (int)(annex.levelCount - 1) < 3 ? annex.levelCount - 1 : 3;
	for (i = 0; i < record.ranges; i = (uint8_t)(i + 1)) {
		record.rangeHigh[i] = annex.levels[i].index;
		record.rangeLow[i] = annex.levels[i + 1].index + 1;
	}
	record.rangeLow[record.ranges - 1]--;
	wlanCalibPowerSet(index, &record);
	wlanCalibContextSet(0, &annex.context);
	wlanCalibFlagsSet(0, &annex.flags);
	wlanCalibSecondarySet(index, 0, &annex.secondary[0]);
	wlanCalibSecondarySet(index, 1, &annex.secondary[1]);
	for (i = 0; i < record.ranges; i = (uint8_t)(i + 1)) {
		for (k = 0; k < record.channels; k = (uint8_t)(k + 1)) {
			wlanCalibRangeAdjustSet(index, 0, i, k, &annex.rangeAdjust[0][i][k]);
			wlanCalibRangeAdjustSet(index, 1, i, k, &annex.rangeAdjust[1][i][k]);
		}
	}

	for (i = 0; i < count; i = (uint8_t)(i + 1)) {
		levels = annex.levels[i];
		calDataSetLevelPair(&levels, index, annex.levels[i].index, annex.levels[i].second,
				    annex.levels[i].first, annex.levelDelta);
	}

	segments = count - 1;
	for (segment = 0; segment < segments; segment = (uint8_t)(segment + 1)) {
		const struct WlanTxPowerLevels *high = &annex.levels[segment];
		const struct WlanTxPowerLevels *low = &annex.levels[segment + 1];

		highThird = high->third & 0x7f;
		lowThird = low->third & 0x7f;
		highPower = high->index;
		lowPower = low->index;
		for (power = (uint8_t)(highPower - 1); power > lowPower; power = (uint8_t)(power - 1)) {
			levels.index = power;
			levels.third = calDataInterpolate(lowThird, highThird, lowPower, highPower, power) & 0x7f;
			value = calDataInterpolate(low->second, high->second, lowPower, highPower, power);
			levels.second = value - annex.levelDelta;
			levels.first = annex.levelDelta + value;
			wlanCalibLevelsSet(index, 0, power, &levels);
			value = calDataInterpolate(low->first, high->first, lowPower, highPower, power);
			levels.second = value - annex.levelDelta;
			levels.first = annex.levelDelta + value;
			wlanCalibLevelsSet(index, 1, power, &levels);
		}
	}
	return 1;
}

/* Annex 52: value 2 of band index 0, when the annex gives it. */
int calDataLoadAnnex52(int fromHost, unsigned int offset)
{
	struct CalDataAnnexValue annex;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	if (annex.flags & 1)
		wlanCalibValue2Set(0, &annex.value);
	return 1;
}

/* Annex 53: value 1 of band index 0, when the annex gives it. */
int calDataLoadAnnex53(int fromHost, unsigned int offset)
{
	struct CalDataAnnexValue annex;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	if (annex.flags & 1)
		wlanCalibValue1Set(0, &annex.value);
	return 1;
}

/* Annex 54: value 0 of band index 0, when the annex gives it. */
int calDataLoadAnnex54(int fromHost, unsigned int offset)
{
	struct CalDataAnnexValue annex;
	unsigned int length;

	__aeabi_memclr4(&annex, sizeof(annex));
	calDataRead(fromHost, &annex, offset, sizeof(annex));
	length = annex.header.length < sizeof(annex) ? annex.header.length : sizeof(annex);
	if (!calDataChecksumOk((const uint8_t *)&annex, length))
		return 0;
	if (annex.flags & 1)
		wlanCalibValue0Set(0, &annex.value);
	return 1;
}
