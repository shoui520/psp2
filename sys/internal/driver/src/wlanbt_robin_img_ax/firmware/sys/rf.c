/*
 * The RF878X radio (0xbf20..0xc2bc): power, the band and channel it is
 * tuned to, its synthesizer lock and the register tables written to it
 * through the radio bus. Marvell published no source for it; the filename
 * and the names say what the code does.
 */
#include <stddef.h>
#include "robin/sys.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/soc.h"
#include "robin/txpower.h"

#define RF_REG_VERSION		0x000
#define RF_REG_SUBVERSION	0x001
#define RF_REG_LOCK_CONTROL	0x007	/* bit 1: lock detection */
#define RF_REG_LOCK_STATUS	0x01a	/* bits 6-7: unlocked; bit 3: lock interrupt */
#define RF_REG_PATH_ENABLE	0x020	/* per path, bit 4 */
#define RF_REG_CALIBRATION0	0x032
#define RF_REG_CALIBRATION1	0x033
#define RF_REG_CALIBRATION2	0x034
#define RF_REG_LOCK_DETAIL	0x046
#define RF_REG_CONTROL4B	0x04b
#define RF_REG_INTERRUPT	0x0b2	/* bit 4: lock interrupt pending */
#define RF_REG_INTERRUPT_MASK	0x0b3
#define RF_REG_POWER0		0x105	/* per path, bit 6 */
#define RF_REG_POWER1		0x19e	/* per path: bits 0-2 */
#define RF_REG_POWER2		0x1be	/* per path, bit 7 */
#define RF_PATH_STRIDE		0x100
#define RF_PATHS		1

#define RF_PATH_REG(path, reg)	((uint16_t)(((path) << 8) + (reg)))

/* Not converted yet; provisional names. */
extern uint8_t wlanPrimaryEvents;				/* 0x04008828, bit 5: no rate below index 5 */
extern const uint16_t romRfChannelTable[];			/* ROM 0x03f42130 */
extern const uint16_t rfCalibrationTable[];			/* 0x53d56 */
void wlanPsSelectHeader(unsigned int band, uint32_t channel);	/* 0x3e3aa */
int sysRefClockProfile(void);				/* 0x26842 */
extern uint32_t rfBusEnabled;
extern volatile uint32_t macPendingParam;			/* 0x8000a138, bit 4: 40 MHz */

#define MAC_PENDING_PARAM_40MHZ	0x10

uint8_t rfVersion __attribute__((section(".itcm.rw")));
uint8_t rfSubVersion __attribute__((section(".itcm.rw")));
static uint8_t rfBand __attribute__((section(".itcm.rw")));
static uint8_t rfBandwidth __attribute__((section(".itcm.rw")));
static uint8_t rfLockStatus __attribute__((section(".itcm.rw")));	/* RF_REG_LOCK_STATUS when last unlocked */
static uint8_t rfLockDetail __attribute__((section(".itcm.rw")));
static uint32_t rfLockChecks __attribute__((section(".itcm.rw")));	/* bits 16-31: found unlocked */
static uint32_t rfLockWaits __attribute__((section(".itcm.rw")));	/* bits 16-31: timed out */
static uint32_t rfChannel __attribute__((section(".itcm.rw")));
static uint32_t rfLockInterrupts __attribute__((section(".itcm.rw")));

/* Register tables (DTCM, so the host can patch them); each ends with 0xffff, 0xff. */
#define RF_TABLE_END	{ 0xffff, 0xff, 0 }

struct PhyRegisterEntry rfRegisterPatches[31] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};

struct PhyRegisterEntry rfBand2gRegisters[] __attribute__((section(".dtcm.rw"))) = {
	{ 0x008, 0x51, 0 }, { 0x009, 0x1c, 0 }, { 0x02e, 0x0f, 0 }, { 0x02f, 0x45, 0 },
	{ 0x030, 0x6e, 0 }, { 0x032, 0x08, 0 }, { 0x033, 0x35, 0 }, { 0x034, 0xc7, 0 },
	{ 0x035, 0x04, 0 }, { 0x036, 0x10, 0 }, { 0x03b, 0x05, 0 }, { 0x04f, 0x26, 0 },
	{ 0x050, 0x4f, 0 }, { 0x051, 0x80, 0 }, { 0x06d, 0x00, 0 }, { 0x070, 0x50, 0 },
	{ 0x071, 0x00, 0 }, { 0x07a, 0x08, 0 }, { 0x102, 0x18, 0 }, { 0x146, 0x80, 0 },
	{ 0x14d, 0x08, 0 }, { 0x14e, 0x02, 0 }, { 0x14f, 0x00, 0 }, { 0x150, 0x31, 0 },
	{ 0x152, 0x40, 0 }, { 0x153, 0x00, 0 }, { 0x156, 0x3c, 0 }, { 0x158, 0x18, 0 },
	{ 0x15b, 0x54, 0 }, { 0x15c, 0x00, 0 }, { 0x15e, 0x1a, 0 }, { 0x160, 0x20, 0 },
	{ 0x162, 0x00, 0 }, { 0x1a0, 0x00, 0 }, { 0x1a2, 0x24, 0 }, { 0x1a6, 0x6d, 0 },
	{ 0x1a8, 0xfc, 0 }, { 0x1af, 0x00, 0 }, { 0x1b4, 0x01, 0 }, { 0x1c0, 0x7f, 0 },
	{ 0x1c2, 0x46, 0 }, { 0x1c3, 0x03, 0 }, { 0x1c8, 0xff, 0 },
	RF_TABLE_END,
};

static struct PhyRegisterEntry rfMode2g0Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode2g1Registers[] __attribute__((section(".dtcm.rw"))) = {
	{ 0x102, 0x48, 0 },
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode2g2Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode2g3Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
struct PhyRegisterEntry rfBand5gRegisters[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode5g1Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode5g0Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode5g2Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};
static struct PhyRegisterEntry rfMode5g3Registers[] __attribute__((section(".dtcm.rw"))) = {
	RF_TABLE_END,
};

static struct PhyRegisterEntry *rfBandRegisters[2] __attribute__((section(".itcm.rw"))) = {
	rfBand2gRegisters, rfBand5gRegisters,
};
static struct PhyRegisterEntry *rfModeRegisters[2][RF_MODES] __attribute__((section(".itcm.rw"))) = {
	{ rfMode2g0Registers, rfMode2g1Registers, rfMode2g2Registers, rfMode2g3Registers },
	{ rfMode5g0Registers, rfMode5g1Registers, rfMode5g2Registers, rfMode5g3Registers },
};

void rfPathClearBits(int path, uint16_t reg, uint8_t mask)
{
	rfBusWrite(reg, rfBusRead(reg) & ~mask);
}

void rfPowerDown(void)
{
	int path;

	for (path = 0; path < RF_PATHS; path++) {
		rfBusClearBits(RF_PATH_REG(path, RF_REG_POWER1), 0x07);
		rfBusClearBits(RF_PATH_REG(path, RF_REG_POWER0), 0x40);
		rfPathClearBits(path, RF_REG_PATH_ENABLE, 0x10);
		rfBusSetBits(RF_PATH_REG(path, RF_REG_POWER2), 0x80);
	}
}

void rfPathSetBits(int path, uint16_t reg, uint8_t mask)
{
	rfBusWrite(reg, rfBusRead(reg) | mask);
}

/* Powers the paths up; the arguments set bits 0 and 2 of RF_REG_POWER1. */
void rfPowerUp(int first, int second)
{
	int path;
	uint16_t base;
	uint8_t power;

	rfBusAcquire();
	for (path = 0; path < RF_PATHS; path++) {
		base = path << 8;
		rfBusSetBits(base + RF_REG_POWER2, 0x80);
		power = rfBusRead(base + RF_REG_POWER1) & 0xfa;
		if (first)
			power |= 0x01;
		if (second)
			power |= 0x04;
		rfBusWrite(base + RF_REG_POWER1, power | 0x02);
		rfBusSetBits(base + RF_REG_POWER0, 0x40);
		rfPathSetBits(path, RF_REG_PATH_ENABLE, 0x10);
	}
	rfBusRelease();
}

/*
 * Tunes to a channel. On 5 GHz the rates below OFDM are not used; a
 * bandwidth of 2 or 3 is 40 MHz.
 */
int rfSetChannel(unsigned int band, uint32_t channel, unsigned int bandwidth)
{
	wlanPsSelectHeader(band, channel);
	rfChannel = channel;
	rfBand = band;
	rfBandwidth = bandwidth;
	if (band == RF_BAND_2GHZ)
		wlanPrimaryEvents &= ~0x20;
	else if (band == RF_BAND_5GHZ)
		wlanPrimaryEvents |= 0x20;
	rfTuneChannel(band, channel, bandwidth);
	if (bandwidth == 2 || bandwidth == 3)
		macPendingParam |= MAC_PENDING_PARAM_40MHZ;
	else
		macPendingParam &= ~MAC_PENDING_PARAM_40MHZ;
	return 1;
}

uint32_t rfGetChannel(void)
{
	return rfChannel;
}

unsigned int rfGetBand(void)
{
	return rfBand;
}

unsigned int rfGetBandwidth(void)
{
	return rfBandwidth;
}

/* Loops of the lock wait: the clock scaled by 4000, over 1024. */
static inline int rfLockWaitLoops(void)
{
	return (int)(((uint64_t)sysClockFrequency * 4000) >> 10);
}

/*
 * Waits a while for the synthesizer to lock; when it does not, records
 * the lock status registers and counts it.
 */
int rfCheckLock(void)
{
	int loops = rfLockWaitLoops();
	unsigned int unlocked;

	while (!(SOC_STATUS & SOC_STATUS_RF_LOCKED) && loops > 0)
		--loops;
	if (loops != 0)
		return loops;
	rfBusAcquire();
	rfLockStatus = rfBusRead(RF_REG_LOCK_STATUS);
	unlocked = rfLockStatus >> 6;
	rfLockDetail = rfBusRead(RF_REG_LOCK_DETAIL);
	if (unlocked && rfLockDetail == 0)
		rfLockChecks += 0x10000;
	rfBusRelease();
	return ++rfLockChecks;
}

/* Reads the version, writes the patch table, powers down and arms the lock interrupt. */
void rfInit(int unused)
{
	rfBusAcquire();
	rfVersion = rfBusRead(RF_REG_VERSION);
	rfSubVersion = rfBusRead(RF_REG_SUBVERSION);
	rfBusWriteTable(rfRegisterPatches);
	rfPowerDown();
	if (rfBusRead(RF_REG_INTERRUPT) & 0x10)
		rfBusSetBits(RF_REG_INTERRUPT, 0x10);
	rfBusSetBits(RF_REG_INTERRUPT_MASK, 0x10);
	rfBusSetBits(RF_REG_CONTROL4B, 0x20);
	rfLockInterrupts = 0;
	rfBusRelease();
}

int rfInitStage(int unused)
{
	rfInit(unused);
	wlanTxPowerRfEnable(1);
	return 1;
}

uint32_t rfGetSubVersion(void)
{
	return rfSubVersion;
}

uint32_t rfGetVersion(void)
{
	return rfVersion;
}

void rfSleepHook(void)
{
}

/* Writes register 0x1bb + 0x100 * index under the bus lock. */
void rfBusWriteIndexed(int index, uint8_t value)
{
	uint16_t reg = (index << 8) + 443;

	rfBusAcquire();
	rfBusWrite(reg, value);
	rfBusRelease();
}

/* Waits for the synthesizer to lock; 0 when it timed out. */
int rfWaitLock(void)
{
	int locked = 1;
	int loops;

	sysDelayUs(100);
	loops = rfLockWaitLoops();
	while (!(SOC_STATUS & SOC_STATUS_RF_LOCKED) && loops > 0)
		--loops;
	if (loops == 0) {
		locked = 0;
		rfLockWaits += 0x10000;
	} else {
		rfLockWaits += 1;
	}
	return locked;
}

/* Writes the band's table and the mode's (1, 2 or 3, 0, 4 by column). */
void rfLoadBandRegisters(int band, int mode)
{
	int fiveGhz = band != 0;
	int column;
	struct PhyRegisterEntry *modeTable;

	if (mode == 1)
		column = 0;
	else if (mode == 2 || mode == 3)
		column = 1;
	else if (mode == 0)
		column = 2;
	else if (mode == 4)
		column = 3;
	else
		column = -1;
	modeTable = column == -1 ? NULL : rfModeRegisters[fiveGhz][column];
	rfBusWriteTable(rfBandRegisters[fiveGhz]);
	rfBusWriteTable(modeTable);
	if (sysRefClockProfile() != 6 && sysRefClockProfile() != 9) {
		rfBusWrite(RF_REG_CALIBRATION0, 0x00);
		rfBusWrite(RF_REG_CALIBRATION1, 0x30);
		rfBusWrite(RF_REG_CALIBRATION2, 0xc3);
	}
}

void rfChannelHookBefore(void)
{
}

void rfChannelHookAfter(void)
{
}

void rfCalibrateChannel(int unused, unsigned int channel)
{
	int index = sysRefClockProfile();

	rfSynthesizerSet(romRfChannelTable[channel], rfCalibrationTable[index], 0);
}

/* Lets the accessors touch the bus. */
int rfBusEnable(void)
{
	rfBusEnabled = 1;
	return 1;
}

/* Interrupt 28: the radio's lock interrupt. */
void rfLockInterrupt(void)
{
	uint8_t pending;

	rfBusAcquire();
	pending = rfBusRead(RF_REG_INTERRUPT);
	if (pending & 0x10) {
		if (rfBusRead(RF_REG_LOCK_CONTROL) & 0x02) {
			rfBusClearBits(RF_REG_LOCK_CONTROL, 0x02);
			rfCheckLock();
			rfBusSetBits(RF_REG_LOCK_CONTROL, 0x02);
		}
		rfBusClearBits(RF_REG_LOCK_STATUS, 0x08);
		++rfLockInterrupts;
	}
	rfBusWrite(RF_REG_INTERRUPT, pending);
	rfBusRelease();
}
