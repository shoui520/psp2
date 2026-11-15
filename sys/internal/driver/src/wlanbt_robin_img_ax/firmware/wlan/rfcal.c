/*
 * Baseband tables for a channel and the radio's calibration (0x325ec - 0x32d30): the register
 * tables written when the channel changes (bbpLoadBandwidthTables), the hold on the MAC while
 * the synthesizer settles, and the "macTxNotify*Packet" functions, which are not about packets:
 * they talk to a second client of the serial bus (0x90000800: registers 5, 7, 18, 25, 64, 65, 71,
 * 176, 178, 179, 196, microcode written to 0x8000..) and run the radio's start-up calibration
 * through the baseband and radio registers. Marvell published no source for it; the names are the
 * firmware's own placeholder names, kept, and the file name is inferred.
 */
#include <stddef.h>
#include "robin/clock.h"
#include "robin/mac.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rfbus.h"
#include "robin/rfcal.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/socpower.h"

extern const uint16_t romRfChannelTable[];		/* ROM 0x03f42130 */
int fmCoexSyncBuffer(void);				/* 0x4c93c */

/* The serial controller's own registers; the second user of the bus the radio sits on. */
#define SERIAL_ENABLE		REG16(0x90000000)
#define SERIAL_SELECT		REG16(0x90000004)
#define SERIAL_CONTROL		REG16(0x90000800)
#define SERIAL_CONFIGURATION	REG16(0x90000804)
#define SERIAL_BUSY		REG8(0x9000081c)	/* bit 0 */
#define SERIAL_DATA		REG16(0x90000824)
#define SERIAL_ADDRESS		REG16(0x90000828)
#define SERIAL_MODE		REG16(0x90000830)
#define SERIAL_BUSY_BIT		0x01
#define SERIAL_CONFIGURATION_KEEP	0x0400
#define SERIAL_CONFIGURATION_MICROCODE	0x4000
#define SERIAL_MODE_MICROCODE	3
#define SERIAL_MICROCODE_WINDOW	0x8000

#define SOC_REG_CLOCK_GATE	SOC_REG(0x0c)
#define SOC_REG_PENDING		SOC_REG(0x7c)
#define SOC_REG_TUNE		SOC_REG(0x60)

#define MAC_710_TUNE_HOLD	MAC_REG(0x710)
#define MAC_710_TUNE_MASK	0xfffc03ffu
#define MAC_710_TUNE_HOLD_BITS	0x0002e800u

/* The bytes of the radio's start-up calibration (one per register the loop below adjusts). */
#define BBP_CALIBRATION_BYTES	7

/*
 * Baseband register tables of the channel (DTCM 0x040002dc..0x04000568): one for the band, one for
 * the mode (only 2.4 GHz mode 2/3 and 5 GHz mode 2/3 have one), two for the radio path.
 */
struct PhyRegisterEntry bbpBandTable24[] __attribute__((section(".dtcm.rw"))) = {	/* 0x040002dc */
	{ 0x008, 0x04, 0 }, { 0x047, 0x6d, 0 }, { 0x04a, 0x4a, 0 }, { 0x050, 0x26, 0 }, { 0x057, 0x00, 0 },
	{ 0x069, 0xe6, 0 }, { 0x06b, 0xd9, 0 }, { 0x06c, 0xd8, 0 }, { 0x0e0, 0x83, 0 }, { 0x0f8, 0x66, 0 },
	{ 0x0fa, 0x30, 0 }, { 0x0fb, 0x79, 0 }, { 0x14d, 0x33, 0 }, { 0x158, 0x2c, 0 }, { 0x159, 0x2c, 0 },
	{ 0x165, 0x24, 0 }, { 0x176, 0x05, 0 }, { 0x177, 0x0b, 0 }, { 0x178, 0x12, 0 }, { 0x179, 0xee, 0 },
	{ 0x17c, 0x4c, 0 }, { 0x206, 0x18, 0 }, { 0x208, 0xc6, 0 }, { 0x209, 0x4c, 0 }, { 0x20a, 0xd0, 0 },
	{ 0x20d, 0xa0, 0 }, { 0x20f, 0x24, 0 }, { 0x210, 0x1e, 0 }, { 0x219, 0xe5, 0 }, { 0x230, 0x00, 0 },
	{ 0x2c3, 0x03, 0 }, { 0x25a, 0x89, 0 }, { 0x25b, 0x05, 0 }, { 0x26e, 0x00, 0 }, { 0x28d, 0x66, 0 },
	{ 0x2ae, 0x29, 0 }, { 0x2af, 0x29, 0 }, { 0x2c2, 0x85, 0 }, { 0x2c8, 0x62, 0 }, { 0x2cf, 0x60, 0 },
	{ 0x2d5, 0x04, 0 }, { 0x2e0, 0x0c, 0 }, { 0x2fe, 0xb5, 0 }, { 0x2ff, 0xc4, 0 }, { 0x326, 0xc3, 0 },
	{ 0x343, 0x66, 0 }, { 0x37f, 0x39, 0 }, { 0x392, 0x3e, 0 }, { 0x393, 0x2e, 0 }, { 0x3fb, 0x80, 0 },
	{ 0x3fe, 0x84, 0 }, { 0x3ff, 0x84, 0 }, { 0x401, 0xc0, 0 }, { 0x420, 0xc0, 0 }, { 0x421, 0x3e, 0 },
	{ 0x422, 0x00, 0 }, { 0x423, 0x03, 0 }, { 0x424, 0x3f, 0 }, { 0x425, 0x00, 0 }, { 0x426, 0x3f, 0 },
	{ 0x427, 0x08, 0 }, { 0x428, 0xb8, 0 }, { 0x429, 0x38, 0 }, { 0x42a, 0x39, 0 }, { 0x42b, 0x01, 0 },
	{ 0xffff, 0xff, 0 },
};

struct PhyRegisterEntry bbpModeTable24[] __attribute__((section(".dtcm.rw"))) = {	/* 0x040003e4 */
	{ 0x008, 0x0c, 0 }, { 0x0f8, 0x64, 0 }, { 0x0fb, 0xf3, 0 }, { 0x2cf, 0x60, 0 }, { 0xffff, 0xff, 0 },
};

struct PhyRegisterEntry bbpPathTableEnabled[] __attribute__((section(".dtcm.rw"))) = {	/* 0x040003f8 */
	{ 0x050, 0x22, 0 }, { 0x2d9, 0x1a, 0 }, { 0x2dd, 0x0c, 0 }, { 0x2df, 0xfa, 0 }, { 0x2e3, 0xae, 0 },
	{ 0x2ee, 0x33, 0 }, { 0x2ef, 0x33, 0 }, { 0x2f0, 0xbb, 0 }, { 0x2f1, 0x56, 0 }, { 0x2f3, 0x14, 0 },
	{ 0xffff, 0xff, 0 },
};

struct PhyRegisterEntry bbpPathTableDisabled[] __attribute__((section(".dtcm.rw"))) = {	/* 0x04000424 */
	{ 0x050, 0x26, 0 }, { 0x2d9, 0x9a, 0 }, { 0x2dd, 0x00, 0 }, { 0x2df, 0x00, 0 }, { 0x2e3, 0xb8, 0 },
	{ 0x2ee, 0x44, 0 }, { 0x2ef, 0x44, 0 }, { 0x2f0, 0xcc, 0 }, { 0x2f1, 0x52, 0 }, { 0x2f3, 0x19, 0 },
	{ 0xffff, 0xff, 0 },
};

struct PhyRegisterEntry bbpBandTable5[] __attribute__((section(".dtcm.rw"))) = {	/* 0x04000450 */
	{ 0x008, 0x24, 0 }, { 0x047, 0x6d, 0 }, { 0x04a, 0x4a, 0 }, { 0x050, 0x22, 0 }, { 0x057, 0x00, 0 },
	{ 0x069, 0xe6, 0 }, { 0x06b, 0xd9, 0 }, { 0x06c, 0xd8, 0 }, { 0x0e0, 0x83, 0 }, { 0x0f8, 0x66, 0 },
	{ 0x0fa, 0x31, 0 }, { 0x0fb, 0xc2, 0 }, { 0x14d, 0x33, 0 }, { 0x158, 0x2c, 0 }, { 0x159, 0x2c, 0 },
	{ 0x165, 0x24, 0 }, { 0x176, 0x04, 0 }, { 0x177, 0x0a, 0 }, { 0x178, 0x10, 0 }, { 0x179, 0xce, 0 },
	{ 0x17c, 0x4e, 0 }, { 0x206, 0x15, 0 }, { 0x208, 0xc6, 0 }, { 0x209, 0x4c, 0 }, { 0x20a, 0xd0, 0 },
	{ 0x20d, 0xa0, 0 }, { 0x20f, 0x21, 0 }, { 0x210, 0x1b, 0 }, { 0x219, 0xc5, 0 }, { 0x230, 0x04, 0 },
	{ 0x2c3, 0x43, 0 }, { 0x25a, 0x89, 0 }, { 0x25b, 0x05, 0 }, { 0x26e, 0x00, 0 }, { 0x28d, 0x66, 0 },
	{ 0x2ae, 0x29, 0 }, { 0x2af, 0x29, 0 }, { 0x2c2, 0x85, 0 }, { 0x2c8, 0x62, 0 }, { 0x2cf, 0x60, 0 },
	{ 0x2d5, 0x04, 0 }, { 0x2e0, 0x0a, 0 }, { 0x2fe, 0xb5, 0 }, { 0x2ff, 0xc4, 0 }, { 0x326, 0xc3, 0 },
	{ 0x343, 0x66, 0 }, { 0x37f, 0x39, 0 }, { 0x392, 0x3e, 0 }, { 0x393, 0x2e, 0 }, { 0x3fb, 0x80, 0 },
	{ 0x3fe, 0x84, 0 }, { 0x3ff, 0x84, 0 }, { 0x401, 0xc0, 0 }, { 0x420, 0xc0, 0 }, { 0x421, 0x3e, 0 },
	{ 0x422, 0x00, 0 }, { 0x423, 0x03, 0 }, { 0x424, 0x3f, 0 }, { 0x425, 0x00, 0 }, { 0x426, 0x3f, 0 },
	{ 0x427, 0x08, 0 }, { 0x428, 0xb8, 0 }, { 0x429, 0x17, 0 }, { 0x42a, 0x39, 0 }, { 0x42b, 0x01, 0 },
	{ 0xffff, 0xff, 0 },
};

struct PhyRegisterEntry bbpModeTable5[] __attribute__((section(".dtcm.rw"))) = {	/* 0x04000558 */
	{ 0x008, 0x2c, 0 }, { 0x0fb, 0x83, 0 }, { 0x2cf, 0x60, 0 }, { 0xffff, 0xff, 0 },
};

/* Per band; per band and mode (index 0: modes 2 and 3, 1: mode 0, 2: mode 4; none for the rest). */
static const struct PhyRegisterEntry *bbpBandTables[2] __attribute__((section(".itcm.rw"))) = {	/* 0x54fa0 */
	bbpBandTable24, bbpBandTable5,
};

static const struct PhyRegisterEntry *bbpModeTables[2][3] __attribute__((section(".itcm.rw"))) = {	/* 0x54fa8 */
	{ bbpModeTable24, NULL, NULL },
	{ bbpModeTable5, NULL, NULL },
};

/* Baseband register 658 as measured under selectors 1 and 2, held for rfTunePrepare to write back. */
static uint8_t bbpSavedReading[2] __attribute__((section(".itcm.rw")));	/* 0x54fc0 */

/*
 * What the start-up calibration measured, the registers its save and restore walk (twelve of the
 * radio's, twelve of the baseband's) and the table that turns a level into a value.
 */
struct WlanRfCalibration {
	uint8_t result[BBP_CALIBRATION_BYTES];
	uint8_t reserved;
	uint16_t rfRegisters[12];
	uint16_t bbpRegisters[12];
	uint16_t levelTable[64];
};

static struct WlanRfCalibration wlanRfCalibration __attribute__((section(".itcm.rw"))) = {	/* 0x54fd4 */
	.rfRegisters = {
		0x1af, 0x155, 0x156, 0x157, 0x158, 0x1b0, 0x1b1, 0x1ab, 0x1ac, 0x1a7, 0x104, 0x1a2,
	},
	.bbpRegisters = {
		0x008, 0x05a, 0x204, 0x0dd, 0x286, 0x02d, 0x02e, 0x04f, 0x17b, 0x179, 0x3c0, 0x3c1,
	},
	.levelTable = {
		0x0605, 0x07ed, 0x08d1, 0x0966, 0x09d6, 0x0a2f, 0x0a79, 0x0ab9, 0x0af1, 0x0b22,
		0x0b4f, 0x0b77, 0x0b9c, 0x0bbe, 0x0bde, 0x0bfc, 0x0c18, 0x0c32, 0x0c4b, 0x0c62,
		0x0c78, 0x0c8d, 0x0ca2, 0x0cb5, 0x0cc8, 0x0cd9, 0x0cea, 0x0cfb, 0x0d0b, 0x0d1a,
		0x0d29, 0x0d37, 0x0d45, 0x0d53, 0x0d60, 0x0d6c, 0x0d79, 0x0d85, 0x0d91, 0x0d9c,
		0x0da7, 0x0db2, 0x0dbc, 0x0dc7, 0x0dd1, 0x0ddb, 0x0de5, 0x0dee, 0x0df7, 0x0e00,
		0x0e09, 0x0e12, 0x0e1a, 0x0e23, 0x0e2b, 0x0e33, 0x0e3b, 0x0e43, 0x0e4b, 0x0e52,
		0x0e5a, 0x0e61, 0x0e68, 0x0e6f,
	},
};

/* The twelve radio and twelve baseband registers, as saved by macTxNotifyConvertPacket. */
static uint8_t rfSavedRegisters[24] __attribute__((section(".itcm.bss")));	/* 0x57570 */

/* Writes the tables of a band and a mode, and the baseband's bandwidth and clock settings. */
void bbpLoadBandwidthTables(unsigned int band, unsigned int mode, unsigned int channel)
{
	unsigned int fiveGHz = band != 0;
	const struct PhyRegisterEntry *modeTable;
	unsigned int value;
	int center;
	int index;

	index = 0;
	if (mode != 2 && mode != 3) {
		index = 1;
		if (mode != 0) {
			index = 2;
			if (mode != 4)
				index = -1;
		}
	}
	modeTable = index == -1 ? NULL : bbpModeTables[fiveGHz][index];
	bbpWriteTable(bbpBandTables[fiveGHz]);
	bbpWriteTable(modeTable);
	if (mode == 3)
		value = bbpReadVersionRegister(44) | 0x80;
	else
		value = bbpReadVersionRegister(44) & 0x7f;
	bbpWriteRegister(44, value);
	center = wlanHt40CenterChannel(band, channel, mode);
	if (band == 0) {
		unsigned int frequency = romRfChannelTable[center];

		if (mode == 1 && frequency - 2457 <= 15)
			bbpWriteRegister(251, 123);
	}
	if (sysRefClockProfile() == 5 || sysRefClockProfile() == 1) {
		bbpWriteRegister(1022, 4);
		bbpWriteRegister(1023, 4);
	}
}

void bbpFmCoexAdjust(void)
{
	if (fmCoexSyncBuffer() == 1)
		bbpClearRegisterBits(689, 0x80);
}

/* The path table of the 2.4 GHz band: the one for a path that is enabled, or the other. */
void bbpLoadPathTable(unsigned int band, int enabled)
{
	if (band == 0)
		bbpWriteTable(enabled ? bbpPathTableEnabled : bbpPathTableDisabled);
}

/* Holds the MAC's transmit path while the synthesizer is retuned. */
void macTuneHoldStart(void)
{
	MAC_710_TUNE_HOLD = (MAC_710_TUNE_HOLD & MAC_710_TUNE_MASK) | MAC_710_TUNE_HOLD_BITS;
	SOC_MAC_EVENT_PATH_CONTROL = (SOC_MAC_EVENT_PATH_CONTROL >> 4 << 4) | 0xe;
}

void macTuneHoldEnd(void)
{
	MAC_710_TUNE_HOLD &= MAC_710_TUNE_MASK;
	SOC_MAC_EVENT_PATH_CONTROL = SOC_MAC_EVENT_PATH_CONTROL >> 8 << 8;
}

/* The baseband's calibration signal path: the registers the loop in ComputePacket relies on. */
static void macTxNotifyBuildPacket(void)
{
	bbpWriteRegister(8, 8);
	bbpSetRegisterBits(90, 0x80);
	bbpClearRegisterBits(516, 0x80);
	bbpClearRegisterBits(646, 4);
	sysDelayUs(100);
	bbpSetRegisterBits(646, 4);
	sysDelayUs(2);
	bbpWriteRegister(45, 192);
	bbpWriteRegister(46, 16);
}

/*
 * Runs the calibration loop: the radio generates a tone, the baseband steps register 951 until the
 * level register (952) reports done, and each step is written to radio register 74. The last
 * value is returned in *result; the registers it changed are restored.
 */
static void macTxNotifyComputePacket(uint8_t *result)
{
	uint8_t rf258, rf341, rf342, rf344, rf433;
	unsigned int bbp971;
	uint8_t value;

	rf258 = rfBusRead(258);
	rf341 = rfBusRead(341);
	rfBusRead(342);
	rf344 = rfBusRead(344);
	rf433 = rfBusRead(433);
	rfBusWrite(258, 24);
	bbp971 = bbpReadVersionRegister(971);
	bbpWriteRegister(971, 91);
	bbpWriteRegister(79, 161);
	rfBusWrite(341, 255);
	rf342 = rfBusRead(342);
	rfBusWrite(342, 56);
	rfBusWrite(344, 8);
	rfBusWrite(433, 1);
	bbpWriteRegister(379, 1);
	bbpWriteRegister(377, 224);
	macTxNotifyBuildPacket();
	rfBusWrite(74, 65);
	bbpWriteRegister(949, 0);
	bbpWriteRegister(950, 26);
	bbpWriteRegister(952, 24);
	bbpClearRegisterBits(901, 0x80);
	bbpSetRegisterBits(267, 0x80);
	sysDelayUs(2);
	bbpWriteRegister(951, 128);
	while ((bbpReadVersionRegister(952) & 0x80) == 0) {
		while ((bbpReadVersionRegister(953) & 0x10) == 0 && (bbpReadVersionRegister(952) & 0x80) == 0)
			sysDelayUs(10);
		value = bbpReadVersionRegister(951);
		rfBusWrite(74, value);
		bbpWriteRegister(953, 8);
		sysDelayUs(1);
		bbpWriteRegister(953, 0);
	}
	value = bbpReadVersionRegister(951);
	*result = value;
	rfBusWrite(74, value);
	bbpWriteRegister(951, 0);
	bbpWriteRegister(949, 0);
	bbpWriteRegister(950, 0);
	bbpClearRegisterBits(267, 0x80);
	bbpSetRegisterBits(901, 0x80);
	bbpWriteRegister(971, bbp971);
	rfBusWrite(258, rf258);
	rfBusWrite(341, rf341);
	rfBusWrite(342, rf342);
	rfBusWrite(344, rf344);
	rfBusWrite(433, rf433);
}

/* Writes the two readings back to baseband registers 912 and 913. */
void macTxNotifyQueryPacket(void)
{
	bbpWriteRegister(912, bbpSavedReading[0]);
	bbpWriteRegister(913, bbpSavedReading[1]);
}

/* Reads register 658 under each of the selectors 1 and 2 of register 320 and keeps what it reads. */
void macTxNotifyResetPacket(void)
{
	unsigned int value;

	SOC_REG_PENDING |= 1;
	bbpWriteRegister(660, 1);
	bbpWriteRegister(320, 1);
	value = bbpReadVersionRegister(658);
	bbpSavedReading[0] = value;
	bbpWriteRegister(912, value);
	bbpWriteRegister(660, 1);
	bbpWriteRegister(320, 2);
	value = bbpReadVersionRegister(658);
	bbpSavedReading[1] = value;
	bbpWriteRegister(913, value);
	bbpWriteRegister(320, 0);
	bbpWriteRegister(660, 0);
}

/* Writes a register of the serial controller: wait for the bus, address, then 16 bits of data. */
void macTxNotifySyncPacket(uint16_t reg, uint16_t value)
{
	while (SERIAL_BUSY & SERIAL_BUSY_BIT)
		;
	SERIAL_ADDRESS = reg;
	SERIAL_DATA = value;
}

void macTxNotifyApplyPacket(uint16_t reg, uint16_t value)
{
	while (SERIAL_BUSY & SERIAL_BUSY_BIT)
		;
	SERIAL_ADDRESS = reg;
	SERIAL_DATA = value;
}

uint8_t macTxNotifyResolvePacket(uint16_t reg)
{
	while (SERIAL_BUSY & SERIAL_BUSY_BIT)
		;
	SERIAL_ADDRESS = reg;
	return SERIAL_DATA;
}

/* Puts a two-bit mode in bits 6 and 7 of register 7. */
void macTxNotifySchedulePacket(unsigned int mode)
{
	macTxNotifyApplyPacket(7, (macTxNotifyResolvePacket(7) & 0x3f) | (uint8_t)(mode << 6));
}

void macTxNotifyNotifyPacket(uint16_t reg, uint16_t bits)
{
	macTxNotifyApplyPacket(reg, macTxNotifyResolvePacket(reg) | bits);
}

void macTxNotifyFormatPacket(uint16_t reg, uint16_t bits)
{
	macTxNotifyApplyPacket(reg, macTxNotifyResolvePacket(reg) & ~bits);
}

/* Waits, 10 microseconds at a time, until one of the bits is set in the register; returns 0. */
int macTxNotifyInspectPacket(uint16_t reg, unsigned int bits)
{
	while ((macTxNotifyResolvePacket(reg) & bits) == 0)
		sysDelayUs(10);
	return 0;
}

/* Waits for the bits in register 178, then writes them back to clear them. */
int macTxNotifyTrackPacket(unsigned int bits)
{
	int result;

	result = macTxNotifyInspectPacket(178, bits);
	if (result == 0) {
		macTxNotifyApplyPacket(178, bits);
		macTxNotifyResolvePacket(178);
	}
	return result;
}

/*
 * Loads a microcode image into the controller. The image starts with its length (16 bits, bytes)
 * and is written through the window at 0x8000 two bytes at a time, with the bus in the
 * microcode mode; the load is complete when register 178 reports it and register 25 shows no error.
 */
void macTxNotifyManagePacket(const uint8_t *image)
{
	const uint8_t *data = image + 2;
	unsigned int length = (image[1] << 8) | image[0];
	unsigned int i;

	SOC_REG_CLOCK_GATE |= 0x2020;
	SERIAL_ENABLE = 1;
	SERIAL_SELECT = 0;
	SERIAL_CONFIGURATION = (SERIAL_CONFIGURATION & SERIAL_CONFIGURATION_KEEP) + 856;
	SERIAL_CONTROL = 1;
	SERIAL_MODE = 0;
	macTxNotifyNotifyPacket(176, 31);
	macTxNotifyApplyPacket(64, 0);
	macTxNotifyApplyPacket(65, 0);
	macTxNotifySchedulePacket(2);
	macTxNotifyApplyPacket(179, 0);
	while (SERIAL_BUSY & SERIAL_BUSY_BIT)
		;
	SERIAL_CONFIGURATION |= SERIAL_CONFIGURATION_MICROCODE;
	SERIAL_MODE = SERIAL_MODE_MICROCODE;
	macTxNotifySyncPacket(SERIAL_MICROCODE_WINDOW, length);
	for (i = 0; (int)i < (int)length; i += 2)
		macTxNotifySyncPacket(SERIAL_MICROCODE_WINDOW + i, (data[i + 1] << 8) | data[i]);
	macTxNotifySyncPacket(SERIAL_MICROCODE_WINDOW, (data[i + 1] << 8) | data[i]);
	while (SERIAL_BUSY & SERIAL_BUSY_BIT)
		;
	SERIAL_CONFIGURATION &= ~SERIAL_CONFIGURATION_MICROCODE;
	SERIAL_MODE = 0;
	while ((macTxNotifyResolvePacket(178) & 0x20) == 0)
		;
	if (macTxNotifyResolvePacket(25) & 2)
		for (;;)
			;
	macTxNotifyApplyPacket(64, 31);
	macTxNotifyApplyPacket(65, 248);
}

/* What the thunk at 0x32b82 does for rfTuneChannel: restore the saved readings. */
void rfTunePrepare(unsigned int band, uint32_t channel)
{
	macTxNotifyQueryPacket();
}

/* Runs the calibration in the controller's registers and the baseband; the result is 7 bytes. */
int macTxNotifyReleasePacket(void *result)
{
	__aeabi_memclr(result, BBP_CALIBRATION_BYTES);
	macTxNotifyInspectPacket(196, 1);
	macTxNotifyNotifyPacket(71, 16);
	macTxNotifyComputePacket(result);
	macTxNotifyFormatPacket(71, 16);
	macTxNotifyInspectPacket(196, 2);
	macTxNotifyNotifyPacket(71, 8);
	macTxNotifyNotifyPacket(5, 64);
	macTxNotifyFormatPacket(71, 8);
	macTxNotifyInspectPacket(178, 2);
	macTxNotifyNotifyPacket(71, 8);
	macTxNotifyNotifyPacket(71, 4);
	macTxNotifyFormatPacket(71, 8);
	macTxNotifyNotifyPacket(71, 2);
	macTxNotifyFormatPacket(71, 4);
	macTxNotifyFormatPacket(71, 2);
	macTxNotifyFormatPacket(5, 64);
	macTxNotifyTrackPacket(1);
	return 0;
}

/* Start-up: tunes the radio to channel 6 of 2.4 GHz, runs the calibration and restores the bus. */
void macTxNotifySetupPacket(void)
{
	bbpLoadBandwidthTables(0, 1, 6);
	rfBusAcquire();
	rfLoadBandRegisters(0, 1);
	macTxNotifyFormatPacket(7, 18);
	rfCalibrateChannel(0, 5);
	macTuneHoldStart();
	macTxNotifySchedulePacket(3);
	macTxNotifyNotifyPacket(18, 128);
	macTxNotifyNotifyPacket(2, 16);
	macTxNotifyTrackPacket(64);
	macTxNotifyNotifyPacket(2, 2);
	SOC_REG_TUNE &= ~0x10000000u;
	SOC_REG_TUNE |= 0x20003000;
	macTxNotifyReleasePacket(wlanRfCalibration.result);
	bbpSetRegisterBits(2, 0x80);
	sysDelayUs(1);
	bbpClearRegisterBits(2, 0x80);
	sysDelayUs(1);
	macTxNotifySchedulePacket(1);
	macTuneHoldEnd();
	rfBusRelease();
}

/* The value for a level (its top bits select the entry) and a scale. */
unsigned int macTxNotifyClearPacket(unsigned int level, int scale)
{
	return (20 * wlanRfCalibration.levelTable[level >> 6] + 6165 * scale) >> 7;
}

/* Saves the twelve radio and twelve baseband registers of the calibration. */
void macTxNotifyConvertPacket(void)
{
	int i;

	for (i = 0; i < 12; i++)
		rfSavedRegisters[i] = rfBusRead(wlanRfCalibration.rfRegisters[i]);
	for (i = 0; i < 12; i++)
		rfSavedRegisters[i + 12] = bbpReadVersionRegister(wlanRfCalibration.bbpRegisters[i]);
}

/* Writes them back. */
void macTxNotifyVerifyPacket(void)
{
	int i;

	for (i = 0; i < 12; i++)
		rfBusWrite(wlanRfCalibration.rfRegisters[i], rfSavedRegisters[i]);
	for (i = 0; i < 12; i++)
		bbpWriteRegister(wlanRfCalibration.bbpRegisters[i], rfSavedRegisters[i + 12]);
}
