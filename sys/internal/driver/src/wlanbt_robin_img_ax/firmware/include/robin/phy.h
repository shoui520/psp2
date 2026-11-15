/* Baseband register access, rate codes and signal-level calibration. */
#ifndef ROBIN_PHY_H
#define ROBIN_PHY_H

#include <stdint.h>
#include "robin/rfbus.h"
#include "robin/socpower.h"

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

uint32_t bbpReadVersionRegister(uint32_t index);
extern uint8_t opRegionMinor;					/* 0x549e1 */
extern uint8_t opRegionMajor;					/* 0x549e2 */
extern struct PhyRegisterEntry bbpInitRegisters[];		/* 0x040002d8 */
extern struct PhyRegisterEntry bbpBandTable24[];		/* 0x040002dc */
extern struct PhyRegisterEntry bbpBandTable5[];		/* 0x04000450 */
void bbpWriteTable(const struct PhyRegisterEntry *table);
void bbpWriteRegister(unsigned int reg, unsigned int value);
void bbpSetRegisterBits(unsigned int reg, unsigned int mask);
void bbpClearRegisterBits(unsigned int reg, unsigned int mask);

int wlanDsssRateIndex(int rate);
int wlanOfdmRateIndex(int code);
int wlanHtRateIndex(int is40MHz, unsigned int mcs);
int wlanRateCode(int index);
int wlanHt40CenterChannel(int fiveGHz, int channel, int mode);
void bbpLoadBandwidthTables(unsigned int band, unsigned int mode, unsigned int channel);	/* 0x325ec */
void bbpFmCoexAdjust(void);								/* 0x326a8 */
void bbpLoadPathTable(unsigned int band, int enabled);					/* 0x326bc */
void macTuneHoldStart(void);								/* 0x326ec */
void macTuneHoldEnd(void);								/* 0x32710 */
void rfTunePrepare(unsigned int band, uint32_t channel);				/* 0x32b82 */

void wlanSetRssiBias(int8_t bias);
int8_t wlanCalibrateRssi(int rssi, unsigned int phyMode);
int8_t wlanCalibrateNoise(int noise);
void wlanSignalCalibrationLoad(const int8_t *values);
void wlanSignalCalibrationReset(void);
int bbpInit(void);
int bbpTxHook(void);
void bbpLoadCalibration0(unsigned int channel, unsigned int band);
void bbpLoadCalibration1(unsigned int channel, unsigned int band);
void bbpLoadCalibration2(unsigned int channel, unsigned int band);
void bbpLoadSignalCalibration(unsigned int channel, unsigned int band);
int bbpSetAdjustment(unsigned int mode, uint8_t offset);
void bbpApplyAdjustment(void);
int wlanClampRssi(int dbm);
int rfTuneChannel(unsigned int band, uint32_t channel, unsigned int bandwidth);
void bbpNoiseHistogramStart(uint32_t period, unsigned int resolution, int wideRange);
int bbpNoiseHistogramRead(uint8_t *shares, uint16_t *average);
void rfWaitBtIdle(uint32_t path);


#endif /* ROBIN_PHY_H */
