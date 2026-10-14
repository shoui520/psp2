/* Baseband register access, rate codes and signal-level calibration. */
#ifndef ROBIN_PHY_H
#define ROBIN_PHY_H

#include <stdint.h>
#include "robin/rfbus.h"

/*
 * Calibration of a signal level: the offset added for the antenna in use
 * and the range the result is held to.
 */
struct WlanSignalCalibration {
	int8_t offset[2];	/* per antenna */
	int8_t maximum;
	int8_t minimum;
};

/* Rate index space: 0..4 DSSS, 5..12 OFDM, 14..21 HT20, 22 MCS 32, 23..30 HT40. */
#define WLAN_RATE_INDEX_INVALID	0xff

void bbpWriteTable(const struct PhyRegisterEntry *table);
void bbpWriteRegister(unsigned int reg, unsigned int value);
void bbpSetRegisterBits(unsigned int reg, unsigned int mask);
void bbpClearRegisterBits(unsigned int reg, unsigned int mask);

int wlanDsssRateIndex(int rate);
int wlanOfdmRateIndex(int code);
int wlanHtRateIndex(int is40MHz, unsigned int mcs);
int wlanRateCode(int index);
int wlanHt40CenterChannel(int fiveGHz, int channel, int mode);

void wlanSetRssiBias(int8_t bias);
int8_t wlanCalibrateRssi(int rssi, unsigned int phyMode);
int8_t wlanCalibrateNoise(int noise);
void wlanSignalCalibrationLoad(const int8_t *values);
void wlanSignalCalibrationReset(void);
int wlanClampRssi(int dbm);

#endif /* ROBIN_PHY_H */
