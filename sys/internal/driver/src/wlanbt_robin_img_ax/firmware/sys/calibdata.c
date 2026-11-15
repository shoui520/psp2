/*
 * The calibration data the host loads (sqram/caldata.c decodes it into
 * these tables, one per band index): values by kind, each held in a slot
 * with a validity mask, so that a lookup can tell "not loaded" from zero.
 * The names are inferred from use; the filename is too.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/calibration.h"
#include "robin/mef.h"
#include "robin/runtime.h"

#define WLAN_CALIB_BANDS		2	/* the band index of calDataBandIndex() */
#define WLAN_CALIB_POWER_LEVELS		21	/* transmit powers 2 to 22 dBm */
#define WLAN_CALIB_POWER_FIRST		2
#define WLAN_CALIB_RANGES		4
#define WLAN_CALIB_CHANNELS		6
#define WLAN_CALIB_MODES		2	/* 0: 20 MHz, 1: 40 MHz */

/* One slot of each size, in the slots' own layout (see wlanParam8Set() and the others). */
uint8_t wlanCalibContext[2] __attribute__((section(".itcm.rw")));		/* 0x54e3a */
uint8_t wlanCalibFlags[2] __attribute__((section(".itcm.rw")));		/* 0x54e3c */
uint8_t wlanCalibValue1[5] __attribute__((section(".itcm.rw")));		/* 0x54e3e */
uint8_t wlanCalibValue0[5] __attribute__((section(".itcm.rw")));		/* 0x54e43 */
uint8_t wlanCalibValue2[5] __attribute__((section(".itcm.rw")));		/* 0x54e48 */

const uint8_t *calDataImage __attribute__((section(".itcm.rw")));		/* 0x54e50 */
const uint16_t *calDataImageOffset __attribute__((section(".itcm.rw")));	/* 0x54e54 */
unsigned int (*calDataFlagsHook)(int index) __attribute__((section(".itcm.rw")));	/* 0x54e58 */

uint8_t wlanCalibSignal[9] __attribute__((section(".itcm.bss")));		/* 0x5ee8c */
uint8_t wlanCalibBlock[17] __attribute__((section(".itcm.bss")));		/* 0x5ee95 */

/* Transmit power record of each band index, and a bit per record that was loaded. */
struct {
	uint8_t record[WLAN_CALIB_BANDS][sizeof(struct WlanTxPowerRecord)];
	uint8_t valid;
} wlanCalibPower __attribute__((section(".itcm.bss")));			/* 0x5eea6 */

/* The adjustment of a secondary channel (two signed bytes) for each band index. */
struct {
	uint8_t adjust[WLAN_CALIB_BANDS][WLAN_CALIB_BANDS][2];
	uint8_t valid[WLAN_CALIB_BANDS];
} wlanCalibSecondary __attribute__((section(".itcm.bss")));		/* 0x5eed3 */

/* The levels (unaligned words) of each transmit power, by mode; a bit per power. */
struct WlanCalibLevels {
	uint8_t level[WLAN_CALIB_BANDS][WLAN_CALIB_POWER_LEVELS][4];
	uint8_t valid[WLAN_CALIB_BANDS][4];
};

struct WlanCalibLevels wlanCalibLevels[WLAN_CALIB_MODES] __attribute__((section(".itcm.bss")));
									/* 0x5eedd, 0x5ef8d */

/* The adjustment (halfwords) of a power range on each of the measured channels, by mode. */
struct WlanCalibRangeAdjust {
	uint8_t value[WLAN_CALIB_BANDS][WLAN_CALIB_RANGES][WLAN_CALIB_CHANNELS][2];
	uint8_t valid[WLAN_CALIB_BANDS][WLAN_CALIB_RANGES];
};

struct WlanCalibRangeAdjust wlanCalibRangeAdjust[WLAN_CALIB_MODES]
	__attribute__((section(".itcm.bss")));				/* 0x5f03d, 0x5f0a5 */

/* The RF context value (a byte). */
int wlanCalibContextSet(unsigned int index, const uint8_t *value)
{
	return wlanParam8Set(wlanCalibContext, index, value);
}

int wlanCalibContextGet(unsigned int index, uint8_t *value)
{
	return wlanParam8Get(wlanCalibContext, index, value);
}

void wlanCalibContextClear(void)
{
	wlanCalibContext[0] = 0;
	wlanCalibContext[1] = 0;
}

/* The flags (a byte; bits 0-2). */
int wlanCalibFlagsSet(unsigned int index, const uint8_t *value)
{
	return wlanParam8Set(wlanCalibFlags, index, value);
}

int wlanCalibFlagsGet(unsigned int index, uint8_t *value)
{
	return wlanParam8Get(wlanCalibFlags, index, value);
}

void wlanCalibFlagsClear(void)
{
	wlanCalibFlags[0] = 0;
	wlanCalibFlags[1] = 0;
}

int wlanCalibValue1Set(unsigned int index, const uint16_t *value)
{
	return wlanParam16Set(wlanCalibValue1, index, value);
}

int wlanCalibValue1Get(unsigned int index, uint16_t *value)
{
	return wlanParam16Get(wlanCalibValue1, index, value);
}

void wlanCalibValue1Clear(void)
{
	__aeabi_memclr(wlanCalibValue1, sizeof(wlanCalibValue1));
}

int wlanCalibValue0Set(unsigned int index, const uint16_t *value)
{
	return wlanParam16Set(wlanCalibValue0, index, value);
}

int wlanCalibValue0Get(unsigned int index, uint16_t *value)
{
	return wlanParam16Get(wlanCalibValue0, index, value);
}

void wlanCalibValue0Clear(void)
{
	__aeabi_memclr(wlanCalibValue0, sizeof(wlanCalibValue0));
}

int wlanCalibValue2Set(unsigned int index, const uint16_t *value)
{
	return wlanParam16Set(wlanCalibValue2, index, value);
}

int wlanCalibValue2Get(unsigned int index, uint16_t *value)
{
	return wlanParam16Get(wlanCalibValue2, index, value);
}

void wlanCalibValue2Clear(void)
{
	__aeabi_memclr(wlanCalibValue2, sizeof(wlanCalibValue2));
}

/* The four signal values of a band index (a word). */
int wlanCalibSignalSet(unsigned int index, const struct WlanCalibSignal *values)
{
	return wlanParam32Set(wlanCalibSignal, index, (const uint32_t *)values);
}

int wlanCalibSignalGet(unsigned int index, struct WlanCalibSignal *values)
{
	return wlanParam32Get(wlanCalibSignal, index, (uint32_t *)values);
}

void wlanCalibSignalClear(void)
{
	__aeabi_memclr(wlanCalibSignal, sizeof(wlanCalibSignal));
}

/* Eight bytes for a band index. */
int wlanCalibBlockSet(unsigned int index, const void *block)
{
	return wlanParam64Set(wlanCalibBlock, index, block);
}

int wlanCalibBlockGet(unsigned int index, void *block)
{
	return wlanParam64Get(wlanCalibBlock, index, block);
}

void wlanCalibBlockClear(void)
{
	__aeabi_memclr(wlanCalibBlock, sizeof(wlanCalibBlock));
}

/* The transmit power record of a band index. */
int wlanCalibPowerSet(unsigned int index, const struct WlanTxPowerRecord *record)
{
	if (index >= WLAN_CALIB_BANDS)
		return 0;
	wlanCalibPower.valid |= 1 << index;
	__aeabi_memcpy(wlanCalibPower.record[index], record, sizeof(struct WlanTxPowerRecord));
	return 1;
}

int wlanCalibPowerGet(unsigned int index, struct WlanTxPowerRecord *record)
{
	if (index >= WLAN_CALIB_BANDS || !(wlanCalibPower.valid & (1 << index)))
		return 0;
	__aeabi_memcpy(record, wlanCalibPower.record[index], sizeof(struct WlanTxPowerRecord));
	return 1;
}

void wlanCalibPowerClear(void)
{
	__aeabi_memclr(&wlanCalibPower, sizeof(wlanCalibPower));
}

/* The adjustment of a secondary channel's levels, for a band index. */
int wlanCalibSecondarySet(unsigned int index, unsigned int secondary, const struct WlanCalibAdjust *adjust)
{
	if (index >= WLAN_CALIB_BANDS || secondary >= WLAN_CALIB_BANDS)
		return 0;
	wlanCalibSecondary.valid[index] |= 1 << secondary;
	wlanCalibSecondary.adjust[index][secondary][0] = adjust->value[0];
	wlanCalibSecondary.adjust[index][secondary][1] = adjust->value[1];
	return 1;
}

int wlanCalibSecondaryGet(unsigned int index, unsigned int secondary, struct WlanCalibAdjust *adjust)
{
	if (index >= WLAN_CALIB_BANDS || secondary >= WLAN_CALIB_BANDS ||
	    !(wlanCalibSecondary.valid[index] & (1 << secondary)))
		return 0;
	adjust->value[0] = wlanCalibSecondary.adjust[index][secondary][0];
	adjust->value[1] = wlanCalibSecondary.adjust[index][secondary][1];
	return 1;
}

void wlanCalibSecondaryClear(void)
{
	__aeabi_memclr(&wlanCalibSecondary, sizeof(wlanCalibSecondary));
}

/* The levels of a transmit power (2 to 22 dBm) in a mode, for a band index. */
int wlanCalibLevelsSet(unsigned int index, int mode, unsigned int power,
		       const struct WlanTxPowerLevels *levels)
{
	struct WlanCalibLevels *table;
	unsigned int step = (uint8_t)(power - WLAN_CALIB_POWER_FIRST);

	if (mode == 0) {
		if (index >= WLAN_CALIB_BANDS || step >= WLAN_CALIB_POWER_LEVELS)
			return 0;
		table = &wlanCalibLevels[0];
	} else if (mode == 1 && index < WLAN_CALIB_BANDS && step < WLAN_CALIB_POWER_LEVELS) {
		table = &wlanCalibLevels[1];
	} else {
		return 0;
	}
	__aeabi_uwrite4(__aeabi_uread4(table->valid[index]) | (1 << step), table->valid[index]);
	__aeabi_uwrite4(*(const uint32_t *)levels, table->level[index][step]);
	return 1;
}

int wlanCalibLevelsGet(unsigned int index, int mode, unsigned int power,
		       struct WlanTxPowerLevels *levels)
{
	struct WlanCalibLevels *table;
	unsigned int step = (uint8_t)(power - WLAN_CALIB_POWER_FIRST);

	if (mode == 0) {
		if (index >= WLAN_CALIB_BANDS || step >= WLAN_CALIB_POWER_LEVELS)
			return 0;
		table = &wlanCalibLevels[0];
	} else {
		if (mode != 1 || index >= WLAN_CALIB_BANDS || step >= WLAN_CALIB_POWER_LEVELS)
			return 0;
		table = &wlanCalibLevels[1];
	}
	if (!(__aeabi_uread4(table->valid[index]) & (1 << step)))
		return 0;
	*(uint32_t *)levels = __aeabi_uread4(table->level[index][step]);
	return 1;
}

void wlanCalibLevelsClear(void)
{
	__aeabi_memclr(&wlanCalibLevels[0], sizeof(wlanCalibLevels[0]));
	__aeabi_memclr(&wlanCalibLevels[1], sizeof(wlanCalibLevels[1]));
}

/* The adjustment of a power range at one of the measured channels in a mode, for a band index. */
int wlanCalibRangeAdjustSet(unsigned int index, int mode, unsigned int range, unsigned int channel,
			    const struct WlanCalibAdjust *adjust)
{
	struct WlanCalibRangeAdjust *table;

	if (mode == 0) {
		if (index >= WLAN_CALIB_BANDS || range >= WLAN_CALIB_RANGES ||
		    channel >= WLAN_CALIB_CHANNELS)
			return 0;
		table = &wlanCalibRangeAdjust[0];
	} else if (mode == 1 && index < WLAN_CALIB_BANDS && range < WLAN_CALIB_RANGES &&
		   channel < WLAN_CALIB_CHANNELS) {
		table = &wlanCalibRangeAdjust[1];
	} else {
		return 0;
	}
	table->valid[index][range] |= 1 << channel;
	table->value[index][range][channel][0] = adjust->value[0];
	table->value[index][range][channel][1] = adjust->value[1];
	return 1;
}

int wlanCalibRangeAdjustGet(unsigned int index, int mode, unsigned int range, unsigned int channel,
			    struct WlanCalibAdjust *adjust)
{
	struct WlanCalibRangeAdjust *table;

	if (mode == 0) {
		if (index >= WLAN_CALIB_BANDS || range >= WLAN_CALIB_RANGES ||
		    channel >= WLAN_CALIB_CHANNELS)
			return 0;
		table = &wlanCalibRangeAdjust[0];
	} else {
		if (mode != 1 || index >= WLAN_CALIB_BANDS || range >= WLAN_CALIB_RANGES ||
		    channel >= WLAN_CALIB_CHANNELS)
			return 0;
		table = &wlanCalibRangeAdjust[1];
	}
	if (!(table->valid[index][range] & (1 << channel)))
		return 0;
	adjust->value[0] = table->value[index][range][channel][0];
	adjust->value[1] = table->value[index][range][channel][1];
	return 1;
}

void wlanCalibRangeAdjustClear(void)
{
	__aeabi_memclr(&wlanCalibRangeAdjust[0], sizeof(wlanCalibRangeAdjust[0]));
	__aeabi_memclr(&wlanCalibRangeAdjust[1], sizeof(wlanCalibRangeAdjust[1]));
}
