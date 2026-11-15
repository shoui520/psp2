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
	uint8_t reserved0[5];
	uint8_t rates[14];		/* the rates a started network offers, 0-terminated (macMgmtConvertContext()) */
	uint8_t reserved19[5];
	uint32_t regionPowerTable;	/* nonzero: the region's power table limits transmit power */
	uint8_t params[15];		/* a connection's SNMP values; cleared when one is configured */
	uint8_t reserved43[160 - 43];
	uint16_t wmmQueueTime[8];	/* HostCmd_CMD_WMM_QUEUE_CONFIG's medium times */
	uint8_t reserved176[0xda - 176];
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
} __attribute__((aligned(4)));

/* An adjustment of the levels, a signed offset for each of two kinds. */
struct WlanCalibAdjust {
	int8_t value[2];
} __attribute__((aligned(2)));

/* The four signal values of a band index. */
struct WlanCalibSignal {
	int8_t value[4];
} __attribute__((aligned(4)));

/* Per-channel values of a lookup: a header and one value per measured channel. */
struct WlanCalibrationChannelValues {
	uint8_t header[6];
	uint16_t values[6];
};

_Static_assert(sizeof(struct WlanCalibrationChannelValues) == 18, "per-channel values");

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

/*
 * The calibration data tables (sys/calibdata.c). Each kind has a Set for the code that
 * decodes the host's data, a Get for the lookups and a Clear for a new load; every Set and
 * Get returns 0 for an index (or entry) that is out of range or was not loaded.
 */
int wlanCalibContextSet(unsigned int index, const uint8_t *value);			/* 0x29e90 */
int wlanCalibContextGet(unsigned int index, uint8_t *value);				/* 0x29e9a */
void wlanCalibContextClear(void);							/* 0x29ea4 */
int wlanCalibFlagsSet(unsigned int index, const uint8_t *value);			/* 0x29e76 */
int wlanCalibFlagsGet(unsigned int index, uint8_t *value);				/* 0x29e7e */
void wlanCalibFlagsClear(void);								/* 0x29e86 */
int wlanCalibValue1Set(unsigned int index, const uint16_t *value);			/* 0x29eb0 */
int wlanCalibValue1Get(unsigned int index, uint16_t *value);				/* 0x29eba */
void wlanCalibValue1Clear(void);							/* 0x29ec4 */
int wlanCalibValue0Set(unsigned int index, const uint16_t *value);			/* 0x29ed2 */
int wlanCalibValue0Get(unsigned int index, uint16_t *value);				/* 0x29edc */
void wlanCalibValue0Clear(void);							/* 0x29ee6 */
int wlanCalibValue2Set(unsigned int index, const uint16_t *value);			/* 0x29ef4 */
int wlanCalibValue2Get(unsigned int index, uint16_t *value);				/* 0x29efe */
void wlanCalibValue2Clear(void);							/* 0x29f08 */
int wlanCalibSignalSet(unsigned int index, const struct WlanCalibSignal *values);			/* 0x29f16 */
int wlanCalibSignalGet(unsigned int index, struct WlanCalibSignal *values);				/* 0x29f1e */
void wlanCalibSignalClear(void);							/* 0x29f26 */
int wlanCalibBlockSet(unsigned int index, const void *block);				/* 0x29f32 */
int wlanCalibBlockGet(unsigned int index, void *block);					/* 0x29f3c */
void wlanCalibBlockClear(void);								/* 0x29f46 */
int wlanCalibPowerSet(unsigned int index, const struct WlanTxPowerRecord *record);	/* 0x29f54 */
int wlanCalibPowerGet(unsigned int index, struct WlanTxPowerRecord *record);		/* 0x29f7e */
void wlanCalibPowerClear(void);								/* 0x29fae */
int wlanCalibSecondarySet(unsigned int index, unsigned int secondary, const struct WlanCalibAdjust *adjust); /* 0x29fbc */
int wlanCalibSecondaryGet(unsigned int index, unsigned int secondary, struct WlanCalibAdjust *adjust); /* 0x29fee */
void wlanCalibSecondaryClear(void);							/* 0x2a022 */
int wlanCalibLevelsSet(unsigned int index, int mode, unsigned int power,
		       const struct WlanTxPowerLevels *levels); /* 0x2a030 */
int wlanCalibLevelsGet(unsigned int index, int mode, unsigned int power,
		       struct WlanTxPowerLevels *levels); /* 0x2a0ac */
void wlanCalibLevelsClear(void);							/* 0x2a10a */
int wlanCalibRangeAdjustSet(unsigned int index, int mode, unsigned int range, unsigned int channel,
			    const struct WlanCalibAdjust *adjust);		/* 0x2a120 */
int wlanCalibRangeAdjustGet(unsigned int index, int mode, unsigned int range, unsigned int channel,
			    struct WlanCalibAdjust *adjust);			/* 0x2a182 */
void wlanCalibRangeAdjustClear(void);							/* 0x2a1e6 */

#endif /* ROBIN_CALIBRATION_H */
