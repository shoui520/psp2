/*
 * Baseband register helpers, the table of legacy rate codes with the maps
 * from hardware rate codes to rate indexes, and the calibration of received
 * signal levels. The filename is inferred.
 */
#include <stddef.h>
#include "robin/calibration.h"
#include "robin/coex.h"
#include "robin/event.h"
#include "robin/mac.h"
#include "robin/mactx.h"
#include "robin/phy.h"
#include "robin/rf.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/wlanrx.h"
#include "robin/txpower.h"

/*
 * The receive diversity (wlanUpdateAntennaDiversity()): the RSSI above which
 * the antenna is switched to 1, the RSSI at or below which it goes back to 0
 * after a while, the antenna in use (0 or 1), and when the signal was last
 * strong. The thresholds are not set anywhere in this firmware (inferred names).
 */
int32_t wlanAntennaRssiHigh __attribute__((section(".dtcm.bss")));
int32_t wlanAntennaRssiLow __attribute__((section(".dtcm.bss")));
uint32_t wlanAntenna __attribute__((section(".dtcm.bss")));
uint32_t wlanAntennaStrongTime __attribute__((section(".dtcm.bss")));

/*
 * The data, in Sony's order (0x549dc..0x54a2f): the bytes, the halfwords, the
 * words, then the tables.
 */
/* Added to every calibrated RSSI. */
static int8_t wlanRssiBias __attribute__((section(".itcm.rw"))) = 0;
/* The baseband's version registers, as last read. */
static uint8_t bbpVersionMajorCache __attribute__((section(".itcm.rw"))) = 0;
static uint8_t bbpVersionMinorCache __attribute__((section(".itcm.rw"))) = 0;
/* bbpSetAdjustment(): 0 or 1 selects BBP_REG_ADJUST_SELECT; 0xff: none set. */
static uint8_t bbpAdjustMode __attribute__((section(".itcm.rw"))) = 0xff;
static uint8_t bbpAdjustOffset __attribute__((section(".itcm.rw"))) = 0;
/* The operating region revision bbpInit() sets (1.38). */
uint8_t opRegionMinor __attribute__((section(".itcm.rw"))) = 0;
uint8_t opRegionMajor __attribute__((section(".itcm.rw"))) = 0;

static struct WlanSignalCalibration wlanNoiseCalibration __attribute__((section(".itcm.rw"))) = {
	{ -15, -15 }, -1, -120
};

/* The register pairs (high byte, low byte) the calibration lookups go to. */
static uint16_t bbpCalibrationRegisters[3][1][2] __attribute__((section(".itcm.rw"))) = {
	{ { 0x3c0, 0x3c1 } }, { { 0x3be, 0x3bf } }, { { 0x3c4, 0x3c5 } },
};

/* rfTuneChannel(): tunes that ended with the synthesizer unlocked, in all and by lock state. */
static struct {
	uint32_t total;
	uint32_t byState[3];
} rfUnlockedTunes __attribute__((section(".itcm.rw"))) = { 0 };

/*
 * The hardware rate code of each rate index: DSSS in units of 100 kbit/s, the
 * OFDM codes, then the MCS of HT20, MCS 32 and the MCS of HT40.
 */
static uint8_t wlanRateCodes[31] __attribute__((section(".itcm.rw"))) = {
	10, 20, 55, 110, 220, 0x0b, 0x0f, 0x0a, 0x0e, 0x09, 0x0d, 0x08, 0x0c, 0x07,
	0, 1, 2, 3, 4, 5, 6, 7, 32, 0, 1, 2, 3, 4, 5, 6, 7,
};
static struct WlanSignalCalibration wlanRssiCalibration[3] __attribute__((section(".itcm.rw"))) = {
	{ { -15, -15 }, -1, -110 },
	{ { -15, -15 }, -1, -110 },
	{ { -15, -15 }, -1, -110 },
};

/* bbpNoiseHistogramRead(): the level of each bin in dBm, 9 bins then 11. */
static const int8_t bbpNoiseHistogramLevels[20] = {
	-92, -89, -84, -79, -74, -69, -64, -59, -57,
	-92, -90, -87, -84, -81, -77, -72, -67, -62, -58, -55,
};

/* Written at start-up (the host may patch it); ends with 0xffff, 0xff. */
struct PhyRegisterEntry bbpInitRegisters[] __attribute__((section(".dtcm.rw"))) = {
	{ 0xffff, 0xff, 0 },
};

#define BBP_REG_ADJUST_SELECT	0x70	/* bit 2: mode 1 */
#define BBP_REG_ADJUST0		0x5f
#define BBP_REG_ADJUST1		0x60
#define BBP_REG_RESET		0x02	/* bit 7, pulsed before a channel change */
#define BBP_REG_HISTOGRAM_CONTROL 0xcc	/* bit 7: start; bits 4-5: resolution; bits 0-3: bin */
#define BBP_REG_HISTOGRAM_PERIOD_LOW 0xcd
#define BBP_REG_HISTOGRAM_PERIOD_HIGH 0xce
#define BBP_REG_HISTOGRAM_COUNT0 0xcf	/* the selected bin's count, low byte first */
#define BBP_REG_HISTOGRAM_COUNT1 0xd0
#define BBP_REG_HISTOGRAM_COUNT2 0xd1
#define BBP_REG_HISTOGRAM_CONFIG 0xd2	/* bits 0-4: noise offset; bit 6: 9 bins */

#define BBP_HISTOGRAM_START	0x80
#define BBP_HISTOGRAM_NARROW	0x40

#define RF_REG_LOCK_CONTROL	0x007	/* bit 1: lock detection; bit 4: 2.4 GHz path; bits 6-7: state */
#define RF_REG_CONTROL4B	0x04b

#define MAC_824_TUNING		0x800000c0u	/* MAC_824_CONTROL, held while the baseband is retuned */
#define MAC_828_TUNE_CLEAR	0x00000203u	/* MAC_828_CONTROL */
#define MAC_2D0_BAND_MASK	0xe000u		/* MAC_2D0_FLAGS */
#define MAC_2D0_ENABLE		0x0008u

/* Not converted yet; provisional names. */
int phyChannelPowerCheck(unsigned int band);				/* 0x31e88 */
void phyChannelPowerLoad(unsigned int band, uint32_t channel);		/* 0x31eec */
void phyChannelPowerApply(unsigned int band, uint32_t channel);		/* 0x31f0e */
void wlanPsFilterHeader(unsigned int band, uint32_t channel, unsigned int bandwidth); /* 0x3e4ce */
/* 0x04000668: payload mode 1 starts at the first entry, the end of the table, and writes nothing. */
struct PhyRegisterEntry bbpPayloadModeRegisters[41] __attribute__((section(".dtcm.rw"))) = {
	{ 0xffff, 0xff, 0 }, { 0x0af, 0x00, 0 }, { 0x0b0, 0x00, 0 }, { 0x0b1, 0x00, 0 },
	{ 0x0b2, 0x00, 0 }, { 0x0b3, 0xfd, 0 }, { 0x0b4, 0xf7, 0 }, { 0x0b5, 0xed, 0 },
	{ 0x0b6, 0xe1, 0 }, { 0x0b7, 0xd8, 0 }, { 0x0b8, 0xd5, 0 }, { 0x0b9, 0xf0, 0 },
	{ 0x0ba, 0x00, 0 }, { 0x0bb, 0x0d, 0 }, { 0x0bc, 0x20, 0 }, { 0x0bd, 0x37, 0 },
	{ 0x0be, 0x50, 0 }, { 0x0bf, 0x68, 0 }, { 0x0c0, 0x7c, 0 }, { 0x0c1, 0x89, 0 },
	{ 0x0c2, 0x8e, 0 }, { 0x0c3, 0x04, 0 }, { 0x0c4, 0xab, 0 }, { 0x0c5, 0x38, 0 },
	{ 0x0c6, 0x40, 0 }, { 0x0c7, 0x09, 0 }, { 0x39e, 0x00, 0 }, { 0x39f, 0x00, 0 },
	{ 0x3a0, 0x00, 0 }, { 0x3a1, 0x00, 0 }, { 0x3a2, 0x00, 0 }, { 0x3a3, 0x00, 0 },
	{ 0x3a4, 0x00, 0 }, { 0x3a5, 0x00, 0 }, { 0x3a6, 0x00, 0 }, { 0x3a7, 0x00, 0 },
	{ 0x3a8, 0x00, 0 }, { 0x3a9, 0x00, 0 }, { 0x3aa, 0x00, 0 }, { 0x3ab, 0x00, 0 },
	{ 0xffff, 0xff, 0 },
};
/* 0x0400070c: channel 14 outside 40 MHz. */
struct PhyRegisterEntry bbpChannel14Registers[40] __attribute__((section(".dtcm.rw"))) = {
	{ 0x0af, 0xfc, 0 }, { 0x0b0, 0xfb, 0 }, { 0x0b1, 0xfc, 0 }, { 0x0b2, 0xff, 0 },
	{ 0x0b3, 0x06, 0 }, { 0x0b4, 0x0f, 0 }, { 0x0b5, 0x15, 0 }, { 0x0b6, 0x14, 0 },
	{ 0x0b7, 0x07, 0 }, { 0x0b8, 0xf0, 0 }, { 0x0b9, 0xe9, 0 }, { 0x0ba, 0xde, 0 },
	{ 0x0bb, 0xee, 0 }, { 0x0bc, 0xf5, 0 }, { 0x0bd, 0x05, 0 }, { 0x0be, 0x1f, 0 },
	{ 0x0bf, 0x3c, 0 }, { 0x0c0, 0x58, 0 }, { 0x0c1, 0x6c, 0 }, { 0x0c2, 0x73, 0 },
	{ 0x0c3, 0x00, 0 }, { 0x0c4, 0x00, 0 }, { 0x0c5, 0x08, 0 }, { 0x0c6, 0x0c, 0 },
	{ 0x0c7, 0xab, 0 }, { 0x39e, 0x00, 0 }, { 0x39f, 0x00, 0 }, { 0x3a0, 0x00, 0 },
	{ 0x3a1, 0x00, 0 }, { 0x3a2, 0x00, 0 }, { 0x3a3, 0x00, 0 }, { 0x3a4, 0x00, 0 },
	{ 0x3a5, 0x00, 0 }, { 0x3a6, 0xff, 0 }, { 0x3a7, 0xfe, 0 }, { 0x3a8, 0x00, 0 },
	{ 0x3a9, 0x00, 0 }, { 0x3aa, 0x00, 0 }, { 0x3ab, 0x80, 0 }, { 0xffff, 0xff, 0 },
};

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

uint32_t bbpReadVersionRegister(uint32_t index)
{
	return (uint8_t)BBP_VERSION_REGISTERS[index];
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

uint32_t bbpGetVersionMajor(void)
{
	uint32_t version = bbpReadVersionRegister(0);

	bbpVersionMajorCache = version;
	return version;
}

uint32_t bbpGetVersionMinor(void)
{
	uint32_t version = bbpReadVersionRegister(1);

	bbpVersionMinorCache = version;
	return version;
}

uint32_t bbpGetVersionPatch(void)
{
	return 0;
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
int wlanOfdmRateIndex(uint8_t code)
{
	switch (code) {
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
	return (int8_t)-dbm;
}

/* Noise floor in dBm limited to -102..0, reported as a positive number. */
int wlanClampNoise(int value)
{
	if (value > 0)
		value = 0;
	else if (value < -102)
		value = -102;
	return (int8_t)-value;
}

/* Signal-to-noise value limited to 0..102. */
int8_t wlanClampSnr(int value)
{
	if (value > 102)
		return 102;
	if (value < 0)
		return 0;
	return value;
}

/*
 * Converts what the MAC stored in a receive status to what the host sees:
 * the rate code becomes a rate index, the signal levels are calibrated.
 */
void wlanRxConvertStatus(struct WlanRxStatus *status)
{
	unsigned int phyMode;
	unsigned int ht40;
	uint8_t legacyRate;
	int rate;

	if (status == NULL)
		return;
	phyMode = (status->phyInfo >> 3) & 3;
	ht40 = status->mcs >> 7;
	legacyRate = status->legacyRate;
	if (phyMode == 0)
		rate = wlanOfdmRateIndex(legacyRate);
	else if (phyMode == 1)
		rate = wlanDsssRateIndex(legacyRate);
	else if (phyMode == 2 || phyMode == 3)
		rate = wlanHtRateIndex(ht40, status->mcs & 0x7f);
	else
		rate = WLAN_RATE_INDEX_INVALID;
	status->legacyRate = rate;
	status->rssi = wlanCalibrateRssi((int8_t)status->rssi, (status->phyInfo >> 3) & 3);
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
static inline void bbpWriteCalibration(int table, int path, const uint16_t *value)
{
	bbpWriteRegister(bbpCalibrationRegisters[table][path][0], *value >> 8);
	bbpWriteRegister(bbpCalibrationRegisters[table][path][1], *value & 0xff);
}

void bbpLoadCalibration0(unsigned int channel, unsigned int band)
{
	uint16_t value;
	struct WlanCalibrationKey key;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup0(key, &value))
			bbpWriteCalibration(0, path, &value);
}

void bbpLoadCalibration1(unsigned int channel, unsigned int band)
{
	uint16_t value;
	struct WlanCalibrationKey key;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup1(key, &value))
			bbpWriteCalibration(1, path, &value);
}

void bbpLoadCalibration2(unsigned int channel, unsigned int band)
{
	uint16_t value;
	struct WlanCalibrationKey key;
	int path;

	key.channel = channel;
	key.band = band;
	for (path = 0; path < 1; path++)
		if (wlanCalibrationLookup2(key, &value))
			bbpWriteCalibration(2, path, &value);
}

/* The signal-level offsets of the channel, or the defaults when there are none. */
void bbpLoadSignalCalibration(unsigned int channel, unsigned int band)
{
	int8_t values[8];
	struct WlanCalibrationKey key;

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
	case 36:
	case 40:
		return 38;
	case 44:
	case 48:
		return 46;
	case 60:
	case 64:
		return 62;
	case 100:
	case 104:
		return 102;
	case 108:
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

/* Retunes the baseband and the radio to a channel; always 1. */
int rfTuneChannel(unsigned int band, uint32_t channel, unsigned int bandwidth)
{
	uint8_t channel8;
	int center;

	rfWaitBtIdle(band);
	MAC_828_CONTROL &= ~MAC_828_TUNE_CLEAR;
	MAC_824_CONTROL |= MAC_824_TUNING;
	bbpSetRegisterBits(BBP_REG_RESET, 0x80);
	sysDelayUs(1);
	bbpClearRegisterBits(BBP_REG_RESET, 0x80);
	sysDelayUs(1);
	rfTunePrepare(band, channel);
	bbpWriteTable(bbpInitRegisters);
	channel8 = (uint8_t)channel;
	bbpLoadBandwidthTables(band, bandwidth, channel8);
	if (band == RF_BAND_2GHZ) {
		const struct PhyRegisterEntry *table;

		if (bandwidth != 2 && bandwidth != 3 && channel == 14)
			table = bbpChannel14Registers;
		else if (macGetEventPayloadMode() == 1)
			table = &bbpPayloadModeRegisters[0];
		else
			table = &bbpPayloadModeRegisters[1];
		bbpWriteTable(table);
	}
	bbpFmCoexAdjust();
	bbpApplyAdjustment();
	bbpLoadCalibration2(channel, band);
	bbpLoadCalibration1(channel, band);
	bbpLoadCalibration0(channel, band);
	bbpLoadSignalCalibration(channel, band);
	MAC_824_CONTROL &= ~MAC_824_TUNING;

	rfBusAcquire();
	rfLoadBandRegisters(band, bandwidth);
	wlanTxPowerClassesApply();
	center = wlanHt40CenterChannel(band, channel8, bandwidth);
	rfChannelHookAfter(band, center);
	bbpLoadPathTable(band, sysEventPathIsEnabled(band));
	rfChannelHookBefore(band, sysEventPathIsEnabled(band));
	rfBusClearBits(RF_REG_LOCK_CONTROL, 0x02);
	macTuneHoldStart();
	if (sysEventPathIsEnabled(band))
		rfBusClearBits(RF_REG_LOCK_CONTROL, 0x10);
	else
		rfBusSetBits(RF_REG_LOCK_CONTROL, 0x10);
	rfBusClearBits(RF_REG_CONTROL4B, 0x20);
	rfCalibrateChannel(band, center);
	rfWaitLock();
	macTuneHoldEnd();
	rfBusSetBits(RF_REG_CONTROL4B, 0x20);
	if (sysEventPathIsEnabled(band)) {
		if (!(SOC_STATUS & SOC_STATUS_RF_LOCKED)) {
			switch (rfBusRead(RF_REG_LOCK_CONTROL) >> 6) {
			case 1:
				rfUnlockedTunes.byState[0]++;
				break;
			case 2:
				rfUnlockedTunes.byState[1]++;
				break;
			case 3:
				rfUnlockedTunes.byState[2]++;
				break;
			}
			rfUnlockedTunes.total++;
		}
		rfBusSetBits(RF_REG_LOCK_CONTROL, 0x02);
	}
	rfBusRelease();

	MAC_2D0_FLAGS &= ~MAC_2D0_BAND_MASK;
	if (band == RF_BAND_2GHZ) {
		MAC_2D0_FLAGS |= 0xa000;
		MAC_2D0_FLAGS = 0xa808;
	} else {
		MAC_2D0_FLAGS |= 0x6000;
		MAC_2D0_FLAGS = 0x6808;
	}
	sysSelectEventPath(band);
	if (phyChannelPowerCheck(band) == 1)
		phyChannelPowerLoad(band, channel);
	phyChannelPowerApply(band, channel);
	MAC_2D0_FLAGS |= MAC_2D0_ENABLE;
	wlanPsFilterHeader(band, channel, bandwidth);
	return 1;
}

/* Starts a noise histogram over a period, at a resolution of 32, 64 or 128. */
void bbpNoiseHistogramStart(uint32_t period, unsigned int resolution, int wideRange)
{
	int8_t offset = wlanNoiseCalibration.offset[0];
	uint32_t ticks = (period << 6) >> 16;
	uint8_t magnitude = offset > 0 ? 0 : -offset;
	unsigned int select;
	uint32_t config;

	if (magnitude >= 0x1f)
		magnitude = 0x1f;
	switch (resolution) {
	case 0x20:
		select = 0x10;
		break;
	case 0x40:
		select = 0x20;
		break;
	case 0x80:
		select = 0x30;
		break;
	default:
		select = 0;
		break;
	}
	BBP_REGISTER(BBP_REG_HISTOGRAM_PERIOD_LOW) = (uint8_t)ticks;
	BBP_REGISTER(BBP_REG_HISTOGRAM_PERIOD_HIGH) = ticks >> 8;
	config = (bbpReadVersionRegister(BBP_REG_HISTOGRAM_CONFIG) & ~0x1fu) | magnitude;
	if (!wideRange)
		config |= BBP_HISTOGRAM_NARROW;
	else
		config &= 0xbf;
	BBP_REGISTER(BBP_REG_HISTOGRAM_CONFIG) = config;
	BBP_REGISTER(BBP_REG_HISTOGRAM_CONTROL) = select | BBP_HISTOGRAM_START;
}

/*
 * Reads the histogram: each bin's share of 255 into shares, the weighted
 * level in dBm into *average (when given); returns the number of bins.
 */
int bbpNoiseHistogramRead(uint8_t *shares, uint16_t *average)
{
	uint32_t counts[11];
	const int8_t *levels;
	uint32_t total;
	uint8_t sum;
	int weighted;
	int bins;
	int i;

	if (bbpReadVersionRegister(BBP_REG_HISTOGRAM_CONFIG) & BBP_HISTOGRAM_NARROW) {
		levels = bbpNoiseHistogramLevels;
		bins = 9;
	} else {
		levels = bbpNoiseHistogramLevels + 9;
		bins = 11;
	}
	__aeabi_memclr(shares, bins);
	sum = 0;
	weighted = 0;
	total = 0;
	for (i = 0; i < bins; i++) {
		BBP_REGISTER(BBP_REG_HISTOGRAM_CONTROL) =
			(uint8_t)((bbpReadVersionRegister(BBP_REG_HISTOGRAM_CONTROL) & 0x70) | i);
		counts[i] = bbpReadVersionRegister(BBP_REG_HISTOGRAM_COUNT2) << 16;
		counts[i] |= bbpReadVersionRegister(BBP_REG_HISTOGRAM_COUNT1) << 8;
		counts[i] |= bbpReadVersionRegister(BBP_REG_HISTOGRAM_COUNT0);
		total += counts[i];
	}
	if (total != 0) {
		for (i = 0; i < bins; i++) {
			uint8_t share = (counts[i] * 255) / total;

			shares[i] = share;
			sum += share;
			weighted += levels[i] * share;
		}
	} else {
		shares[bins - 1] = 0xff;
		sum = 0xff;
		weighted = levels[bins - 1] * 255;
	}
	shares[0] += 255 - sum;
	weighted += levels[0] * (255 - sum);
	if (average)
		*average = weighted >> 8;
	return bins;
}

/* Waits (up to 300 steps each) while Bluetooth holds the air on a shared path. */
void rfWaitBtIdle(uint32_t path)
{
	unsigned int state;
	unsigned int count;

	if (!wlanCoexBtNeedsSharing() || !btCoexProfileBit1() || !sysEventPathIsEnabled(path))
		return;
	state = BT_LINK_STATE & 0xf;
	count = 300;
	while ((SOC_PENDING_EVENT_CONTROL & SOC_PENDING_EVENT_ARMED) &&
	       (SOC_PENDING_EVENT_STATUS & SOC_PENDING_EVENT_BT) &&
	       (state == BT_STATE_BUSY_A || state == BT_STATE_BUSY_B) && count-- != 0) {
		sysDelayUs(10);
		state = BT_LINK_STATE & 0xf;
	}
	/* The link state is not read again here. */
	count = 300;
	while ((SOC_PENDING_EVENT_CONTROL & SOC_PENDING_EVENT_ARMED) &&
	       sysPendingEventPolicyIsEnabled() &&
	       !(SOC_PENDING_EVENT_STATUS & SOC_PENDING_EVENT_BT) &&
	       (state == BT_STATE_BUSY_A || state == BT_STATE_BUSY_B) && count-- != 0)
		sysDelayUs(10);
}
