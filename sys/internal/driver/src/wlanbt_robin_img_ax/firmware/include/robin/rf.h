/*
 * The RF878X radio (0xbf20..0xc2bc): power, the band and channel it is
 * tuned to, its synthesizer lock and the register tables written to it
 * through the radio bus. See sys/rf.c.
 */
#ifndef ROBIN_RF_H
#define ROBIN_RF_H

#include <stdint.h>

#include "robin/rfbus.h"

/* rfSetChannel(): band */
#define RF_BAND_2GHZ		0
#define RF_BAND_5GHZ		1

/* rfLoadBandRegisters(): modes, by table column */
#define RF_MODES		4

extern uint8_t rfVersion;
extern uint8_t rfSubVersion;
extern struct PhyRegisterEntry rfRegisterPatches[];		/* 0x04000184: 31 entries */
extern struct PhyRegisterEntry rfBand2gRegisters[];		/* 0x04000200 */
extern struct PhyRegisterEntry rfBand5gRegisters[];		/* 0x040002c4 */

void rfPathClearBits(int path, uint16_t reg, uint8_t mask);
void rfPowerDown(void);
void rfPathSetBits(int path, uint16_t reg, uint8_t mask);
void rfPowerUp(int first, int second);
int rfSetChannel(unsigned int band, uint32_t channel, unsigned int bandwidth);
uint32_t rfGetChannel(void);
unsigned int rfGetBand(void);
unsigned int rfGetBandwidth(void);
int rfCheckLock(void);
void rfInit(int unused);
int rfInitStage(int unused);
uint32_t rfGetSubVersion(void);
uint32_t rfGetVersion(void);
void rfSleepHook(void);
void rfBusWriteIndexed(int index, uint8_t value);
int rfWaitLock(void);
void rfLoadBandRegisters(int band, int mode);
void rfChannelHookBefore(void);
void rfChannelHookAfter(void);
void rfCalibrateChannel(int unused, unsigned int channel);
int rfSynthesizerSet(uint32_t frequency, uint32_t reference, int doubled);	/* 0x28496 */
int rfBusEnable(void);
void rfLockInterrupt(void);

#endif /* ROBIN_RF_H */
