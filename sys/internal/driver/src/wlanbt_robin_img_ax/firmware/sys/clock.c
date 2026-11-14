/*
 * Clock requests and the reference clock (0x26398..0x26550, 0x26ab4..0x26c40,
 * 0x274d0..0x27900). A client asks for the rate it needs, in kHz, in each of
 * the four clock domains; the fastest request of all clients is applied to the
 * hardware. Rates come from tables in units of 100 kHz, selected by the
 * reference clock profile (the crystal strapped on SOC_REG(0)). The
 * filename and the names are inferred; no Marvell source names this code.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/hct.h"
#include "robin/rom.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sys.h"

/* SOC_REG(0x108): bits 16-19 select the core clock, bits 24-27 the auxiliary one. */
#define CLOCK_SELECT_REG	SOC_REG(0x108)
/* SOC_REG(0x120): the state of the clock source; bits 30-31 are the mode. */
#define CLOCK_SOURCE_REG	SOC_REG(0x120)
#define CLOCK_RATE_REG		SOC_REG(0x188)
#define CLOCK_PLL_REG		SOC_REG(0x1a0)


/*
 * The rates, in units of 100 kHz, of the reference clock of each profile
 * (index 3..12) and a second per-profile table (13..22); the table the
 * firmware shares with its sleep code (rfCalibrationTable).
 */
const uint16_t rfCalibrationTable[23] = {
	0xaaaa, 0x0003, 0x5819,
	0x0078, 0x0082, 0x00c0, 0x00c8, 0x00f0, 0x0104, 0x0180, 0x0190, 0x01b8, 0x0208,
	0x0fa0, 0x0fa0, 0x0dac, 0x0e10, 0x0dac, 0x0dac, 0x0bb8, 0x0bb8, 0x0bb8, 0x0bb8,
};

/* Per profile: the first word of the pair has bit 31 set when the reference is halved. */
static const uint32_t clockPllWords[10][2] = {
	{ 0x05555556, 0x04aec33e }, { 0x053b13b0, 0x04a151c4 }, { 0x04d55555, 0x046d3a06 },
	{ 0x04cccccc, 0x0468db8c }, { 0x04aaaaaa, 0x0457619e }, { 0x049d89d8, 0x0450a8e2 },
	{ 0x84d55555, 0x04369d04 }, { 0x84cccccc, 0x04346dc6 }, { 0x00000000, 0x00000000 },
	{ 0x849d89d8, 0x04285472 },
};
/* Per profile, the value of the select register's low half. */
static const uint32_t clockSelectWords[10] = {
	0x5555c000, 0x6276c000, 0x6aab8000, 0x66668000, 0x55558000,
	0x4ec58000, 0x35558000, 0x33338000, 0x2e8c8000, 0x27628000,
};
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
static const uint16_t clockRatesC[15] = {
	0x0014, 0x0028, 0x0050, 0x006a, 0x00a0, 0x00d5, 0x0140, 0x01aa,
	0x0280, 0x0355, 0x0500, 0x0500, 0x0000, 0x0000, 0x0000,
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

/* The request of each client for each domain, and the rate applied. */
struct ClockRequests {
	uint32_t khz[CLOCK_DOMAINS];
};
static struct ClockRequests clockRequests[CLOCK_CLIENTS] __attribute__((section(".itcm.bss")));
static uint32_t clockApplied[CLOCK_DOMAINS] __attribute__((section(".itcm.bss")));
/* The last two rates looked up in the tables and their indices. */
static struct { uint32_t khz; uint32_t index; } clockIndexCache[2] __attribute__((section(".itcm.bss")));

static uint32_t clockReferenceCache __attribute__((section(".itcm.rw")));
static uint32_t clockBaseCache __attribute__((section(".itcm.rw")));
int rfPowerBlock[5] __attribute__((section(".itcm.rw")));
static uint8_t clockIndexCursor __attribute__((section(".itcm.rw")));
static const uint8_t *clockDividerRow __attribute__((section(".itcm.rw")));
static uint32_t clockBaseRateCache __attribute__((section(".itcm.rw")));

uint32_t sysClockFrequency __attribute__((section(".sqram1.rw"))) = 0x2000;
/* The reference clock profile: valid once read, then the index. */
uint8_t sysRefClockProfileValid __attribute__((section(".sqram1.rw")));
uint8_t sysRefClockProfileIndex __attribute__((section(".sqram1.rw")));
static uint8_t sysRefClockProfileMap[8] __attribute__((section(".sqram1.rw"))) = {
	0, 1, 2, 4, 5, 9, 6, 6,
};

/* The clock rates a host command can choose between, in kHz (DTCM 0x040089b0..0x040089bc). */
uint32_t hostClockPreset[4] __attribute__((section(".dtcm.bss")));
/* The rate set for the host (client 3), and the rate it is to be set to. */
static uint32_t clockHostApplied __attribute__((section(".dtcm.bss")));
static uint32_t clockHostTarget __attribute__((section(".dtcm.bss")));

/*
 * Records the request of a client and applies the fastest request of all
 * clients when it changed; the two flags select the domain.
 */
uint32_t clockRequest(uint32_t khz, unsigned int client, int high, int low)
{
	unsigned int domain = low ? (high ? CLOCK_DOMAIN_CORE : CLOCK_DOMAIN_AUX) :
			     (high ? CLOCK_DOMAIN_BUS : CLOCK_DOMAIN_VOTE);
	unsigned int posture = sysIrqDisable();
	uint32_t applied = clockApplied[domain];
	uint32_t previous = clockRequests[client].khz[domain];
	uint32_t fastest = applied;
	unsigned int i;

	clockRequests[client].khz[domain] = khz;
	if (khz > applied) {
		fastest = khz;
	} else {
		if (previous != applied || previous <= khz)
			goto done;
		fastest = 0;
		for (i = 0; i < CLOCK_CLIENTS; i++) {
			if (clockRequests[i].khz[domain] > fastest)
				fastest = clockRequests[i].khz[domain];
		}
		if (fastest == applied)
			goto done;
	}
	clockApplied[domain] = fastest;
	if (high) {
		if (low)
			clockApplyCore(fastest);
		else
			clockSetBus(fastest);
	} else if (low) {
		CLOCK_SELECT_REG = ((clockIndexFor(fastest) << 24) & 0x0fffffff) |
				   (CLOCK_SELECT_REG & 0xf0ffffff);
	}
done:
	sysIrqRestore(posture);
	return fastest;
}

uint32_t __attribute__((noinline)) clockRequestCore(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 1, 1);
}

uint32_t __attribute__((noinline)) clockRequestBus(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 1, 0);
}

uint32_t __attribute__((noinline)) clockRequestAux(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 0, 1);
}

uint32_t __attribute__((noinline)) clockRequestVote(uint32_t khz, unsigned int client)
{
	return clockRequest(khz, client, 0, 0);
}

/* What a client asked for in a domain. */
uint32_t __attribute__((noinline)) clockRequestRead(unsigned int client, int high, int low)
{
	unsigned int posture = sysIrqDisable();
	unsigned int domain = low ? (high ? CLOCK_DOMAIN_CORE : CLOCK_DOMAIN_AUX) :
				    (high ? CLOCK_DOMAIN_BUS : CLOCK_DOMAIN_VOTE);
	uint32_t khz = clockRequests[client].khz[domain];

	sysIrqRestore(posture);
	return khz;
}

uint32_t clockRequestedCore(unsigned int client)
{
	return clockRequestRead(client, 1, 1);
}

uint32_t clockRequestedBus(unsigned int client)
{
	return clockRequestRead(client, 1, 0);
}

uint32_t clockRequestedVote(unsigned int client)
{
	return clockRequestRead(client, 0, 0);
}

/* The profile of the reference clock: a strap in bits 9-11 of SOC_REG(0), mapped by a table. */
int __attribute__((noinline)) sysRefClockProfile(void)
{
	if (sysRefClockProfileValid == 0) {
		sysRefClockProfileValid = 1;
		sysRefClockProfileIndex = sysRefClockProfileMap[(SOC_REG(0) << 20) >> 29];
	}
	return sysRefClockProfileIndex;
}

/* The base rate of the reference clock, in Hz (4 MHz), once looked up. */
uint32_t clockBaseRate(void)
{
	if (clockBaseCache == 0)
		clockBaseCache = 4000000;
	return clockBaseCache;
}

/* The reference clock, in kHz, divided by two when its table says so. */
uint32_t clockReferenceRate(void)
{
	int profile;
	int index;

	if (clockReferenceCache == 0) {
		profile = sysRefClockProfile();
		index = sysRefClockProfile();
		clockReferenceCache = (100 * rfCalibrationTable[3 + index]) /
				      ((clockPllWords[profile][0] >> 31) + 1);
	}
	return clockReferenceCache;
}

/* The slowest rate: the reference divided by the last divider of the row. */
uint32_t clockSlowestRateGet(void)
{
	return clockSlowestRate();
}

/* Whether SOC_REG(0x30) bit 26 is set; refused while the first radio block is on. */
int clockSelectSource(int enable)
{
	unsigned int posture = sysIrqDisable();

	if (rfPowerBlock[0] == 0) {
		if (enable)
			SOC_REG(0x30) |= 0x4000000;
		else
			SOC_REG(0x30) &= ~0x4000000u;
	}
	sysIrqRestore(posture);
	return 0;
}

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
		rate = (100 * rfCalibrationTable[3 + sysRefClockProfile()]) / divider;
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
		rate = (100 * rfCalibrationTable[3 + sysRefClockProfile()]) / divider;
	}
	sysClockFrequency = ((rate << 10) / 1000 - 1) / 5 + 1;
	sysIrqRestore(posture);
	return rate;
}

/* Programs the clock tree for the rates the tables give khz. */
unsigned int clockRegistersInit(void)
{
	const uint8_t *row = clockDividerRowSelect();
	uint32_t low = 0;
	uint32_t high = 0;
	unsigned int i;

	SOC_REG(0x10c) = 0x43221055;
	SOC_REG(0x110) = 0xedcbb986;
	SOC_REG(0x17c) = 0x65432100;
	SOC_REG(0x180) = 0xaaaa9987;
	for (i = 0; i < 8; i++)
		low |= row[i] << (4 * i);
	SOC_REG(0x184) = low;
	for (i = 8; i < 16; i++)
		high |= row[i] << (4 * (i - 8));
	CLOCK_PLL_REG = high;
	clockSetCore(clockCurrentRate());
	CLOCK_SELECT_REG = ((CLOCK_SELECT_REG & 0xf0000) << 8) | (CLOCK_SELECT_REG & 0xf0ffffff);
	CLOCK_RATE_REG = (CLOCK_RATE_REG & 0xffffffe1) + 2;
	return CLOCK_RATE_REG;
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
		khz = (100 * rfCalibrationTable[3 + sysRefClockProfile()]) / divider;
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
		clockBaseRateCache = (100 * rfCalibrationTable[3 + sysRefClockProfile()]) / divider;
	}
	return clockBaseRateCache;
}

unsigned int clockApplyCore(unsigned int khz)
{
	if (sleepCalibrationActive() == 0)
		return clockSetCore(khz);
	return 0;
}

/* The rate (client 3) of the host commands that set the clock. */
static inline void clockHostRequest(uint32_t khz)
{
	clockHostApplied = khz;
	clockRequestCore(khz, 3);
	clockRequestAux(khz, 3);
	clockRequestBus(khz, 3);
	clockRequestVote(khz, 3);
}

void clockHostSetMax(void)
{
	if (clockHostApplied != 128000)
		clockHostRequest(128000);
}

void clockHostSetMin(void)
{
	if (clockHostApplied != 4000)
		clockHostRequest(4000);
}

void clockHostApplyTarget(void)
{
	if (clockHostTarget != clockHostApplied)
		clockHostRequest(clockHostTarget);
}

void clockHostSelectPreset0(void)
{
	clockHostTarget = hostClockPreset[0];
	clockHostApplyTarget();
}

void clockHostSelectPreset1(void)
{
	clockHostTarget = hostClockPreset[1];
	clockHostApplyTarget();
}

void clockHostSelectPreset2(void)
{
	clockHostTarget = hostClockPreset[2];
	clockHostApplyTarget();
}

void clockHostSelectPreset3(void)
{
	clockHostTarget = hostClockPreset[3];
	clockHostApplyTarget();
}

/* Programs the clock multiplier of the profile into SOC_REG(0x118) and SOC_REG(0x11c). */
void clockPllProgram(void)
{
	int profile = sysRefClockProfile();
	int first = clockPllWords[profile][0];

	if (first >= 0)
		SOC_REG(0x94) = (uint16_t)SOC_REG(0x94) | 0x100;
	SOC_REG(0x118) = (SOC_REG(0x118) & 0x7c000000) | (first & 0x83ffffff);
	SOC_REG(0x11c) = ((SOC_REG(0x11c) >> 26) << 26) | (clockPllWords[profile][1] & 0x3ffffff);
	SOC_REG(0x118) |= 0x4000000;
	SOC_REG(0x11c) |= 0x4000000;
	SOC_REG(0x16c) &= ~0x2000u;
	SOC_REG(0x16c) &= ~0x1000u;
}

/* Programs the clock selection of the profile into SOC_REG(0xac). */
unsigned int clockSelectProgram(void)
{
	unsigned int select = clockSelectWords[sysRefClockProfile()] & 0xffffc008;

	SOC_REG(0xac) = (SOC_REG(0xac) & 0x3ff7) | select;
	return select;
}
