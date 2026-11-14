/*
 * Baseband register helpers, the table of legacy rate codes with the maps
 * from hardware rate codes to rate indexes, and the calibration of received
 * signal levels. The filename is inferred.
 */
#include <stddef.h>
#include "robin/phy.h"
#include "robin/soc.h"
#include "robin/wlanrx.h"

/* Index of the antenna in use (0 or 1); the receive diversity code switches it. */
uint32_t wlanAntenna __attribute__((section(".dtcm.bss")));

/* Added to every calibrated RSSI. */
static int8_t wlanRssiBias __attribute__((section(".itcm.bss")));

/* Hardware rate codes: DSSS in units of 100 kbit/s, then the OFDM codes. */
static uint8_t wlanRateCodes[14] __attribute__((section(".itcm.rw"))) = {
	10, 20, 55, 110, 220, 0x0b, 0x0f, 0x0a, 0x0e, 0x09, 0x0d, 0x08, 0x0c, 0x07
};

static struct WlanSignalCalibration wlanNoiseCalibration __attribute__((section(".itcm.rw"))) = {
	{ -15, -15 }, -1, -120
};
static struct WlanSignalCalibration wlanRssiCalibration[3] __attribute__((section(".itcm.rw"))) = {
	{ { -15, -15 }, -1, -110 },
	{ { -15, -15 }, -1, -110 },
	{ { -15, -15 }, -1, -110 },
};

/* bbpSetAdjustment(): 0 or 1 selects BBP_REG_ADJUST_SELECT; 0xff: none set. */
static uint8_t bbpAdjustMode __attribute__((section(".itcm.rw"))) = 0xff;
static uint8_t bbpAdjustOffset __attribute__((section(".itcm.rw")));

/* The register pairs (high byte, low byte) the calibration lookups go to. */
static uint16_t bbpCalibrationRegisters[3][2] __attribute__((section(".itcm.rw"))) = {
	{ 0x3c0, 0x3c1 }, { 0x3be, 0x3bf }, { 0x3c4, 0x3c5 },
};

/* Written at start-up (the host may patch it); ends with 0xffff, 0xff. */
static struct PhyRegisterEntry bbpInitRegisters[] __attribute__((section(".dtcm.rw"))) = {
	{ 0xffff, 0xff, 0 },
};

#define BBP_REG_ADJUST_SELECT	0x70	/* bit 2: mode 1 */
#define BBP_REG_ADJUST0		0x5f
#define BBP_REG_ADJUST1		0x60

/* The key of a calibration lookup, passed by value. */
struct WlanCalibrationKey {
	uint8_t channel;
	uint8_t band;
	uint8_t reserved[2];
};

/* Not converted yet; provisional names. */
int wlanCalibrationLookup0(struct WlanCalibrationKey key, uint16_t *value);	/* 0xec38 */
int wlanCalibrationLookup1(struct WlanCalibrationKey key, uint16_t *value);	/* 0xec10 */
int wlanCalibrationLookup2(struct WlanCalibrationKey key, uint16_t *value);	/* 0xec60 */
int wlanCalibrationLookupSignal(struct WlanCalibrationKey key, int8_t *values);	/* 0xecb4 */
uint32_t bbpReadVersionRegister(uint32_t index);
extern uint8_t opRegionMinor;
extern uint8_t opRegionMajor;

void bbpWriteTable(const struct PhyRegisterEntry *table)
{
	if (table == NULL)
		return;
	while (!(table->reg == 0xffff && table->value == 0xff)) {
		BBP_REGISTER(table->reg) = table->value;
		table++;
	}
}

/* Turns the baseband on and writes its start-up table; operating region 1.38. */
int bbpInit(void)
{
	SOC_CONTROL |= SOC_CONTROL_BASEBAND;
	SOC_BASEBAND_ENABLE |= 1;
	bbpWriteTable(bbpInitRegisters);
	opRegionMajor = 1;
	opRegionMinor = 38;
	return 1;
}

void bbpWriteRegister(unsigned int reg, unsigned int value)
{
	BBP_REGISTER(reg) = value;
}

void bbpSetRegisterBits(unsigned int reg, unsigned int mask)
{
	BBP_REGISTER(reg) = (uint8_t)BBP_REGISTER(reg) | mask;
}

void bbpClearRegisterBits(unsigned int reg, unsigned int mask)
{
	BBP_REGISTER(reg) = (uint8_t)BBP_REGISTER(reg) & ~mask;
}

/* Rate index of a DSSS rate given in units of 100 kbit/s. */
int wlanDsssRateIndex(int rate)
{
	switch (rate) {
	case 10:
		return 0;
	case 20:
		return 1;
	case 55:
		return 2;
	case 110:
		return 3;
	case 220:
		return 4;
	}
	return WLAN_RATE_INDEX_INVALID;
}

/* Rate index of an OFDM rate code. */
int wlanOfdmRateIndex(int code)
{
	switch ((int8_t)code) {
	case 7:
		return 13;
	case 8:
		return 11;
	case 9:
		return 9;
	case 10:
		return 7;
	case 11:
		return 5;
	case 12:
		return 12;
	case 13:
		return 10;
	case 14:
		return 8;
	case 15:
		return 6;
	}
	return WLAN_RATE_INDEX_INVALID;
}

/* Rate index of an HT MCS (0..7 or 32) on a 20 or 40 MHz channel. */
int wlanHtRateIndex(int is40MHz, unsigned int mcs)
{
	if (mcs > 7) {
		if (mcs == 32)
			return 22;
		return WLAN_RATE_INDEX_INVALID;
	}
	if (is40MHz)
		return (uint8_t)(mcs + 23);
	return (uint8_t)(mcs + 14);
}

/* Hardware rate code of a rate index below 14. */
int wlanRateCode(int index)
{
	return wlanRateCodes[index];
}

void wlanSetRssiBias(int8_t bias)
{
	wlanRssiBias = bias;
}

/* Calibrated RSSI of a raw level; the PHY mode picks the calibration (3 uses 2's). */
int8_t wlanCalibrateRssi(int rssi, unsigned int phyMode)
{
	const struct WlanSignalCalibration *calibration;
	int level;

	if (phyMode == 3)
		phyMode = 2;
	calibration = &wlanRssiCalibration[phyMode];
	level = calibration->offset[wlanAntenna == 1] - rssi + wlanRssiBias;
	if (calibration->maximum < level)
		level = calibration->maximum;
	else if (calibration->minimum > level)
		level = calibration->minimum;
	return level;
}

int8_t wlanCalibrateNoise(int noise)
{
	int level = wlanNoiseCalibration.offset[wlanAntenna == 1] - noise;

	if (wlanNoiseCalibration.maximum < level)
		level = wlanNoiseCalibration.maximum;
	else if (wlanNoiseCalibration.minimum > level)
		level = wlanNoiseCalibration.minimum;
	return level;
}

/*
 * Loads the calibration offsets read from the radio: eight bytes, the first
 * four for the first antenna and the rest for the second, each group in the
 * order noise, RSSI of DSSS, of OFDM and of HT.
 */
void wlanSignalCalibrationLoad(const int8_t *values)
{
	wlanNoiseCalibration.offset[0] = values[0];
	wlanNoiseCalibration.offset[1] = values[4];
	wlanRssiCalibration[0].offset[0] = values[2];
	wlanRssiCalibration[0].offset[1] = values[6];
	wlanRssiCalibration[1].offset[0] = values[1];
	wlanRssiCalibration[1].offset[1] = values[5];
	wlanRssiCalibration[2].offset[0] = values[3];
	wlanRssiCalibration[2].offset[1] = values[7];
}

/* Back to the default offset of -15 on both antennas. */
void wlanSignalCalibrationReset(void)
{
	wlanNoiseCalibration.offset[0] = -15;
	wlanNoiseCalibration.offset[1] = -15;
	wlanRssiCalibration[0].offset[0] = -15;
	wlanRssiCalibration[0].offset[1] = -15;
	wlanRssiCalibration[1].offset[0] = -15;
	wlanRssiCalibration[1].offset[1] = -15;
	wlanRssiCalibration[2].offset[0] = -15;
	wlanRssiCalibration[2].offset[1] = -15;
}

/* A level in dBm limited to -101..0, reported as a positive number. */
int wlanClampRssi(int dbm)
{
	if (dbm <= 0) {
		if (dbm < -101)
			dbm = -101;
	} else {
		dbm = 0;
	}
	return -(int8_t)dbm;
}

/*
 * Converts what the MAC stored in a receive status to what the host sees:
 * the rate code becomes a rate index, the signal levels are calibrated.
 */
void wlanRxConvertStatus(struct WlanRxStatus *status)
{
	unsigned int phyMode;
	int rate;

	if (status == NULL)
		return;
	phyMode = (status->phyInfo >> 3) & 3;
	if (phyMode == 0)
		rate = wlanOfdmRateIndex(status->legacyRate);
	else if (phyMode == 1)
		rate = wlanDsssRateIndex(status->legacyRate);
	else
		rate = wlanHtRateIndex(status->mcs >> 7, status->mcs & 0x7f);
	status->legacyRate = rate;
	status->rssi = wlanCalibrateRssi((int8_t)status->rssi, phyMode);
	status->noise = wlanCalibrateNoise((int8_t)status->noise);
}

/*
 * Centre channel of the 40 MHz pair that holds a channel. Mode 2 puts the
 * second channel above and mode 3 below; any other mode is a 20 MHz channel
 * and is only mapped on 2.4 GHz. Returns 54 for a channel that is not part
 * of a pair.
 */
/* Always 0: the transmit path asks it whether to adjust a frame. */
int bbpTxHook(void)
{
	return 0;
}

/* Writes a 16-bit calibration value, high byte first, to a register pair. */
static inline void bbpWriteCalibration(int pair, uint16_t value)
{
	bbpWriteRegister(bbpCalibrationRegisters[pair][0], value >> 8);
	bbpWriteRegister(bbpCalibrationRegisters[pair][1], value & 0xff);
}

void bbpLoadCalibration0(unsigned int band, unsigned int channel)
{
	struct WlanCalibrationKey key;
	uint16_t value;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup0(key, &value))
			bbpWriteCalibration(path * 2 + 0, value);
}

void bbpLoadCalibration1(unsigned int band, unsigned int channel)
{
	struct WlanCalibrationKey key;
	uint16_t value;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup1(key, &value))
			bbpWriteCalibration(path * 2 + 1, value);
}

void bbpLoadCalibration2(unsigned int band, unsigned int channel)
{
	struct WlanCalibrationKey key;
	uint16_t value;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup2(key, &value))
			bbpWriteCalibration(path * 2 + 2, value);
}

/* The signal-level offsets of the channel, or the defaults when there are none. */
void bbpLoadSignalCalibration(unsigned int band, unsigned int channel)
{
	struct WlanCalibrationKey key;
	int8_t values[8];

	key.channel = channel;
	key.band = band;
	if (wlanCalibrationLookupSignal(key, values))
		wlanSignalCalibrationLoad(values);
	else
		wlanSignalCalibrationReset();
}

int wlanHt40CenterChannel(int fiveGHz, int channel, int mode)
{
	if (!fiveGHz) {
		if (mode == 2)
			return (unsigned int)channel > 9 ? 8 : channel + 1;
		if (mode == 3)
			return (unsigned int)channel < 5 ? 2 : channel - 3;
		return channel - 1;
	}
	if (mode != 2 && mode != 3)
		return channel;
	switch (channel) {
	case 112:
		return 110;
	case 116:
	case 120:
		return 118;
	case 124:
	case 128:
		return 126;
	case 132:
	case 136:
		return 134;
	case 149:
	case 153:
		return 151;
	case 157:
	case 161:
		return 159;
	}
	if (channel > 136 || (channel & 3) != 0)
		return 54;
	switch ((uint8_t)(channel >> 2)) {
	case 9:
	case 10:
		return 38;
	case 11:
	case 12:
		return 46;
	case 15:
	case 16:
		return 62;
	case 25:
	case 26:
		return 102;
	case 27:
		return 110;
	}
	return 54;
}

/* Host command: mode 0, 1 or 0xffff (none) and an offset; 1 when refused. */
int bbpSetAdjustment(unsigned int mode, uint8_t offset)
{
	if (mode != 0 && mode != 1 && mode != 0xffff)
		return 1;
	bbpAdjustMode = mode;
	bbpAdjustOffset = offset;
	return 0;
}

/* Applies the adjustment after a channel change. */
void bbpApplyAdjustment(void)
{
	if (bbpAdjustMode == 0xff)
		return;
	if (bbpAdjustMode == 1)
		bbpSetRegisterBits(BBP_REG_ADJUST_SELECT, 4);
	else
		bbpClearRegisterBits(BBP_REG_ADJUST_SELECT, 4);
	BBP_REGISTER(BBP_REG_ADJUST0) = (uint8_t)(bbpReadVersionRegister(BBP_REG_ADJUST0) +
						  bbpAdjustOffset);
	BBP_REGISTER(BBP_REG_ADJUST1) = (uint8_t)(bbpReadVersionRegister(BBP_REG_ADJUST1) +
						  bbpAdjustOffset);
}
