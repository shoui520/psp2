/*
 * Rates, frame durations and the MAC's timing registers (wlan/macrate.c). A
 * rate index is 0 to 3 for the four CCK rates and 5 to 12 for the OFDM rates;
 * a rate set has a bit per index. The names are inferred from use.
 */
#ifndef ROBIN_MACRATE_H
#define ROBIN_MACRATE_H

#include <stdint.h>
#include "robin/txring.h"

unsigned int wlanMacResponseRate(void);					/* 0x28608 */
unsigned int wlanRateResponseSelect(unsigned int rate, uint32_t rateSet);	/* 0x28618 */
uint16_t wlanAckTime(uint32_t rateSet, unsigned int rate, uint32_t flags);	/* 0x2864e */
unsigned int wlanMacAckTime(void);					/* 0x28670 */
uint16_t wlanFrameDuration(uint32_t rateSet, unsigned int rate, uint32_t flags,
			   unsigned int length, unsigned int wmm);	/* 0x2867c */
uint16_t wlanFrameDurationMac(int a, int b, int c, int d, int e);	/* 0x286ae */
void wlanMacTimersProgram(uint32_t low, uint32_t high, uint32_t limit);	/* 0x286d4 */
void wlanMacEventMasksSet(void);					/* 0x28706 */
void wlanMacE80Bit2Set(void);						/* 0x28716 */
void wlanMacE80Bit2Clear(void);						/* 0x28722 */
void wlanTxControlRateSet(uint32_t *control, unsigned int rate);	/* 0x287e2 */
int wlanTxRateInit(uint32_t *control, struct WlanMacTxRequest *request,
		   unsigned int dataRate, unsigned int rtsRate);	/* 0x28800 */
void wlanTxRequestInit(struct WlanMacTxRequest *request, const uint8_t *frame, int rts);	/* 0x28842 */
void wlanTxRequestRateSet(struct WlanMacTxRequest *request, unsigned int rate, int flag,
			  int extra, int fixed);					/* 0x28e10 */
void wlanTxLifetimeSet(struct WlanMacTxRequest *request, uint64_t now, int lifetime);	/* 0x285d4 */
void wlanRateSetLimits(uint32_t *rateSet, uint32_t value, uint8_t *ofdmHigh, uint8_t *ofdmLow,
		       uint8_t *cckHigh, uint8_t *cckLow);		/* 0x288c0 */
void wlanMacConfigBit15Update(int enable);				/* 0x2894a */
void wlanMacSelectConfig(int mode);					/* 0x2896c */
void wlanMacSlotConfigure(int longSlot);				/* 0x2897c */
void wlanRateResponseTableBuild(uint32_t rateSet, int count);		/* 0x28a9e */

/* The transmit control word: bits 8-11 the rate, bit 12 set when the rate field holds one. */
#define MAC_TX_CONTROL_RATE_FIXED	0x00001000u	/* the control word's rate field is set */
#define MAC_TX_CONTROL_RATE_MASK	0x00000f00u

#endif /* ROBIN_MACRATE_H */
