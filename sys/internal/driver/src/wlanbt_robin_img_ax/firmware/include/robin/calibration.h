/*
 * Calibration and transmit power data kept per channel (0xe63e..0xed04):
 * lookups by band and channel, interpolated between the channels the data
 * was measured on. See sys/macevent.c.
 */
#ifndef ROBIN_CALIBRATION_H
#define ROBIN_CALIBRATION_H

#include <stddef.h>
#include <stdint.h>

/*
 * The radio configuration (0x040089c8 points to it). Only the fields
 * converted code reads are named; the rest is used by files not converted yet.
 */
struct WlanRadioConfig {
	uint8_t reserved0[24];
	uint32_t regionPowerTable;	/* nonzero: the region's power table limits transmit power */
	uint8_t params[15];		/* a connection's SNMP values; cleared when one is configured */
	uint8_t reserved43[0xda - 43];
	uint8_t secondaryAntenna;
	uint8_t primaryAntenna;
	uint8_t secondaryDiversity;
	uint8_t primaryDiversity;
};

_Static_assert(offsetof(struct WlanRadioConfig, primaryDiversity) == 0xdd, "radio configuration");

extern struct WlanRadioConfig *wlanRadioConfig;			/* 0x040089c8 */

/* The key of a calibration lookup, passed by value. */
struct WlanCalibrationKey {
	uint8_t band;
	uint8_t channel;
	uint8_t reserved[2];
};

/* A transmit power request, passed by value. */
struct WlanTxPowerRequest {
	uint8_t power;			/* dBm, clamped to the record's limits */
	uint8_t bandwidth;		/* 2: 40 MHz */
	uint8_t secondary;		/* 2, 3: the 40 MHz secondary channel above, below */
	uint8_t reserved;
};

/* The transmit power record of a band (22 bytes). */
struct WlanTxPowerRecord {
	uint8_t reserved0[4];
	uint8_t ranges;			/* power ranges in rangeHigh/rangeLow */
	uint8_t channels;		/* channels in channelList */
	uint8_t minimum;		/* dBm */
	uint8_t maximum;		/* dBm */
	uint8_t channelList[6];		/* the channels the data was measured on */
	uint8_t rangeHigh[4];
	uint8_t rangeLow[4];
};

_Static_assert(sizeof(struct WlanTxPowerRecord) == 22, "transmit power record");

/* The power levels of a transmit power lookup. */
struct WlanTxPowerLevels {
	uint8_t reserved;
	uint8_t first;
	uint8_t second;
	uint8_t third;			/* bits 0-6; bit 7 kept */
};

/* Per-channel values of a lookup: a header and one value per measured channel. */
struct WlanCalibrationChannelValues {
	uint8_t header[6];
	uint16_t values[5];
};

_Static_assert(sizeof(struct WlanCalibrationChannelValues) == 16, "per-channel values");

unsigned int wlanTxPowerMax(unsigned int channel, unsigned int band);
unsigned int wlanTxPowerMin(unsigned int channel, unsigned int band);
uint8_t wlanCalibrationType60Loaded(void);
void wlanAntennaSelectPrimary(void);
void wlanAntennaSelectSecondary(void);
void wlanAntennaSelect(void);
unsigned int wlanRegionTableIndex(unsigned int code);
int wlanCalibrationLookupByte(unsigned int index, uint8_t *value);
void wlanRfApplyEventContext(unsigned int index);
int wlanRfEventRefresh(void);
int wlanCalibrationTableUpdate(const uint8_t *data, unsigned int offset, unsigned int length,
			       unsigned int action);			/* 0xe85a */
int wlanCalibrationTableQuery(uint8_t *data, uint16_t *length);	/* 0xe880 */
int wlanCalibrationChannelBracket(const uint8_t *channels, unsigned int count, unsigned int channel,
				  uint8_t *low, uint8_t *high, uint8_t *lowIndex,
				  uint8_t *highIndex);
int wlanTxPowerRange(const struct WlanTxPowerRecord *record, unsigned int power, uint8_t *range);
int wlanTxPowerLookup(struct WlanCalibrationKey key, struct WlanTxPowerRequest request,
		      uint8_t *levels);
int wlanCalibrationLookupBlock(struct WlanCalibrationKey key, void *block);
int wlanCalibrationLookupQuad(struct WlanCalibrationKey key, void *values);
int wlanInterpolate(int low, int high, int lowPoint, int highPoint, int point);
int wlanCalibrationLookupInterpolated(struct WlanCalibrationKey key, uint8_t *values);
int wlanCalibrationLookup1(struct WlanCalibrationKey key, uint16_t *value);
int wlanCalibrationLookup0(struct WlanCalibrationKey key, uint16_t *value);
int wlanCalibrationLookup2(struct WlanCalibrationKey key, uint16_t *value);
int wlanCalibrationLookupFlags(unsigned int index, uint8_t *flags);
int wlanCalibrationLookupSignal(struct WlanCalibrationKey key, int8_t *values);

#endif /* ROBIN_CALIBRATION_H */
