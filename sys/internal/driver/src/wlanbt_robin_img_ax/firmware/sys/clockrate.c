/*
 * The clock rates (0x27558..0x278c4): the index of a rate in the tables, the
 * divider row of the profile, the current and slowest rates, and the core
 * and bus clocks set from them. A separate object in Sony's code, with its
 * own data. The filename and the names are inferred; no Marvell source names
 * this code.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

/* SOC_REG(0x108): bits 16-19 select the core clock, bits 24-27 the auxiliary one. */
#define CLOCK_SELECT_REG	SOC_REG(0x108)
/* SOC_REG(0x120): the state of the clock source; bits 30-31 are the mode. */
#define CLOCK_SOURCE_REG	SOC_REG(0x120)
#define CLOCK_RATE_REG		SOC_REG(0x188)
#define CLOCK_PLL_REG		SOC_REG(0x1a0)

/* The dividers a field of the rate register stands for. */
static const uint8_t clockDividers[8] = { 32, 16, 8, 4, 2, 1, 1, 1 };

/* Rates in units of 100 kHz: the processor clock choices and two more tables. */
static const uint16_t clockRatesA[15] = {
	0x000a, 0x0014, 0x0028, 0x0050, 0x006a, 0x00a0, 0x00d5, 0x0140,
	0x01aa, 0x0280, 0x0320, 0x0355, 0x042a, 0x0500, 0x058e,
};
static const uint16_t clockRatesB[16] = {
	0x0028, 0x0050, 0x00a0, 0x00c8, 0x0140, 0x0014, 0x0190, 0x0190,
	0x0280, 0x0320, 0x0320, 0x042a, 0x0500, 0x058e, 0x0640, 0x0640,
};
static const uint16_t clockRatesC[16] = {
	0x0014, 0x0028, 0x0050, 0x006a, 0x00a0, 0x00d5, 0x0140, 0x01aa,
	0x0280, 0x0355, 0x0500, 0x0500, 0x0000, 0x0000, 0x0000, 0x0000,
};
/* The entry of table B for each rate of table A. */
static const uint8_t clockIndexB[16] = {
	0x05, 0x05, 0x00, 0x01, 0x02, 0x02, 0x03, 0x04, 0x06, 0x08, 0x09, 0x0b, 0x0b, 0x0c, 0x0d, 0x0e,
};
/*
 * Sixteen entries each: the divider (an index into clockDividers) a rate of
 * table A needs, in a row per profile. Row 0 is also the entry of table C for
 * each rate of table A.
 */
static const uint8_t clockDividerRows[8][16] = {
	{ 0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x09, 0x0a, 0x0a, 0x0a, 0x0a },
	{ 0x00, 0x01, 0x02, 0x03, 0x03, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x01, 0x02, 0x03, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x01, 0x02, 0x03, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x01, 0x02, 0x03, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x02, 0x03, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
	{ 0x02, 0x03, 0x04, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05, 0x05 },
};

/* The last two rates looked up in the tables and their indices. */
static struct { uint32_t khz; uint32_t index; } clockIndexCache[2] __attribute__((section(".itcm.bss")));
static uint8_t clockIndexCursor __attribute__((section(".itcm.rw")));
static const uint8_t *clockDividerRow __attribute__((section(".itcm.rw")));
static uint32_t clockBaseRateCache __attribute__((section(".itcm.rw")));

/* The index of the first rate of table A that is not below khz, remembering the last two. */
unsigned int clockIndexFor(uint32_t khz)
{
	unsigned int slot = clockIndexCursor & 1;
	unsigned int hundreds;
	unsigned int index;

	if (clockIndexCache[slot].khz == khz)
		return clockIndexCache[slot].index;
	slot = ++clockIndexCursor & 1;
	if (clockIndexCache[slot].khz == khz)
		return clockIndexCache[slot].index;
	hundreds = khz / 100;
	for (index = 0; index < 15; index++) {
		if (clockRatesA[index] >= hundreds)
			break;
	}
	clockIndexCache[slot].khz = khz;
	clockIndexCache[slot].index = index;
	return index;
}

/* The row of dividers of the reference clock profile. */
const uint8_t *clockDividerRowSelect(void)
{
	if (clockDividerRow == NULL) {
		switch (sysRefClockProfile()) {
		case 0:
			clockDividerRow = clockDividerRows[7];
			break;
		case 1:
			clockDividerRow = clockDividerRows[6];
			break;
		case 2:
			clockDividerRow = clockDividerRows[5];
			break;
		case 3:
		case 6:
		case 7:
		case 8:
		case 10:
			clockDividerRow = clockDividerRows[2];
			return clockDividerRow;
		case 4:
			clockDividerRow = clockDividerRows[4];
			break;
		case 5:
			clockDividerRow = clockDividerRows[3];
			break;
		case 9:
			clockDividerRow = clockDividerRows[1];
			break;
		default:
			for (;;)
				;
		}
	}
	return clockDividerRow;
}

/* The mode of the clock source: 0 divides the reference, 1 and 3 use tables B and C. */
unsigned int clockSourceMode(void)
{
	unsigned int posture = sysIrqDisable();
	unsigned int mode = CLOCK_SOURCE_REG >> 30;

	if (mode < 2) {
		if (mode != 0)
			mode = 3;
	} else {
		mode = 1;
	}
	sysIrqRestore(posture);
	return mode;
}

/* The rate the clock runs at, in kHz. */
uint32_t clockCurrentRate(void)
{
	unsigned int divider = 1;
	unsigned int field;
	unsigned int posture;
	unsigned int mode;
	uint32_t rate;

	if (sysEventAlternateModeIsSet()) {
		field = (CLOCK_RATE_REG << 28) >> 29;
		if (field != 0)
			divider = field;
		if (!(CLOCK_RATE_REG & 0x10))
			divider *= 4;
		return 0x5833 / divider;
	}
	posture = sysIrqDisable();
	mode = clockSourceMode();
	if (mode == 1) {
		rate = 100 * clockRatesB[CLOCK_SOURCE_REG & 0xf];
	} else if (mode == 3) {
		rate = 100 * clockRatesC[(CLOCK_SOURCE_REG >> 4) & 0xf];
	} else {
		field = clockDividers[(CLOCK_SOURCE_REG >> 8) & 7];
		if (field != 0)
			divider = field;
		rate = (100 * rfCalibrationTable[sysRefClockProfile()]) / divider;
	}
	sysIrqRestore(posture);
	return rate;
}

/* Selects the clock for khz and keeps sysClockFrequency, in loops per microsecond, in step. */
uint32_t clockSetCore(uint32_t khz)
{
	unsigned int posture = sysIrqDisable();
	unsigned int index = clockIndexFor(khz);
	unsigned int mode = clockSourceMode();
	unsigned int divider;
	uint32_t rate;

	CLOCK_SELECT_REG = (index << 16) | (CLOCK_SELECT_REG & 0xfff0ffff);
	if (mode == 1) {
		rate = 100 * clockRatesB[clockIndexB[index]];
	} else if (mode != 0) {
		rate = 100 * clockRatesC[clockDividerRows[0][index]];
	} else {
		divider = clockDividers[clockDividerRowSelect()[index]];
		if (divider == 0)
			divider = 1;
		rate = (100 * rfCalibrationTable[sysRefClockProfile()]) / divider;
	}
	sysClockFrequency = ((rate << 10) / 1000 - 1) / 5 + 1;
	sysIrqRestore(posture);
	return rate;
}

/* Programs the clock tree for the rates the tables give khz. */
unsigned int clockRegistersInit(void)
{
	const uint8_t *row;
	uint32_t low;
	uint32_t high;
	uint32_t select;
	unsigned int rate;
	int i;

	SOC_REG(0x10c) = 0x43221055;
	SOC_REG(0x110) = 0xedcbb986;
	SOC_REG(0x17c) = 0x65432100;
	SOC_REG(0x180) = 0xaaaa9987;
	row = clockDividerRowSelect();
	low = 0;
	for (i = 0; i < 8; i++)
		low |= row[i] << (4 * i);
	SOC_REG(0x184) = low;
	high = 0;
	for (i = 8; i < 16; i++)
		high |= row[i] << (4 * (i - 8));
	CLOCK_PLL_REG = high;
	clockSetCore(clockCurrentRate());
	select = CLOCK_SELECT_REG;
	CLOCK_SELECT_REG = ((select & 0xf0000) << 8) | (select & 0xf0ffffff);
	rate = (CLOCK_RATE_REG & 0xffffffe1) + 2;
	CLOCK_RATE_REG = rate;
	return rate;
}

/* Programs the divider fields of the bus clocks for khz. */
int clockSetBus(int khz)
{
	unsigned int posture = sysIrqDisable();
	unsigned int index = clockIndexFor(khz);
	unsigned int mode = clockSourceMode();
	unsigned int tableB = clockIndexB[index];
	unsigned int divider;
	unsigned int row;
	unsigned int tableC;
	uint32_t field;

	if (mode == 1)
		khz = 100 * clockRatesB[tableB];
	SOC_REG(0x30) = (tableB << 5) | (SOC_REG(0x30) & 0xfffffe1f);
	field = SOC_REG(0x9c) & 0xfc7fff0f;
	row = clockDividerRowSelect()[index];
	field |= row << 23;
	if (mode == 0) {
		divider = clockDividers[row];
		if (divider == 0)
			divider = 1;
		khz = (100 * rfCalibrationTable[sysRefClockProfile()]) / divider;
	}
	tableC = clockDividerRows[0][index];
	if (mode == 3)
		khz = 100 * clockRatesC[tableC];
	SOC_REG(0x9c) = (tableC << 4) | field;
	sysIrqRestore(posture);
	return khz;
}

/* The slowest rate: the reference clock divided by the last divider of the row. */
uint32_t clockSlowestRate(void)
{
	unsigned int divider;

	if (clockBaseRateCache == 0) {
		divider = clockDividers[clockDividerRowSelect()[15]];
		clockBaseRateCache = (100 * rfCalibrationTable[sysRefClockProfile()]) / divider;
	}
	return clockBaseRateCache;
}
