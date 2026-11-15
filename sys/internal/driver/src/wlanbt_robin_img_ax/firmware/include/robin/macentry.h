/*
 * The MAC's start-up, its mode switches and the rate-set helpers of a
 * connection (wlan/macentry.c, wlan/txrate.c). Marvell published no source;
 * the names say what the code does.
 */
#ifndef ROBIN_MACENTRY_H
#define ROBIN_MACENTRY_H

#include <stdint.h>
#include "robin/conn.h"
#include "robin/mac.h"

uint32_t wlanCryptoEnter(void);					/* 0x3110c */
void wlanCryptoLeave(uint32_t token);				/* 0x31132 */
void wlanCryptoAfterReset(void);				/* 0x31142 */
void wlanCryptoStart(void);					/* 0x3114c */
void wlanCryptoOpsInit(void);					/* 0x31156 */
void gpioPinInterruptSet(unsigned int pin, int first, int second);	/* 0x311a0 */
void hostCmd0074Reset(void);					/* 0x311d0 */
void hostCmd0074Arm(void);					/* 0x311ee */
void pmicRegWrite(uint8_t reg, uint8_t value);			/* 0x3121c */
uint8_t pmicRegRead(uint8_t reg);				/* 0x3121e */
void hostCmd0096Track(void);					/* 0x31222 */
int hostCmd008fApply(void);					/* 0x31224 */
uint16_t wlanConnectionAckTime(struct WlanConnection *connection, unsigned int rate, uint32_t flags);	/* 0x3125c */
uint16_t wlanConnectionFrameDuration(struct WlanConnection *connection, unsigned int rate, uint32_t flags,
				     unsigned int length, unsigned int wmm);	/* 0x31268 */
uint16_t wlanFrameDurationMacCall(int a, int b, int c, int d, int e);	/* 0x3127c */
void wlanContentionWindowUpdate(unsigned int interfaceType, unsigned int lowRate);	/* 0x3128c */
void wlanMacFilterMasksUpdate(void);				/* 0x312da */
void wlanMacOperationModeSet(int on);				/* 0x31398 */
int wlanMacEntryTablesCheck(unsigned int unused, int upper);	/* 0x313c4 */
int wlanMacEntryTablesWrite(void);				/* 0x313f6 */
int wlanMacEntryTablesApply(void);				/* 0x31420 */
int wlanMacRestoreState(void);					/* 0x31436 */
int wlanMacBringUp(uint32_t descriptor, const struct MacRegionProvider *provider, uint32_t dmaContext); /* 0x31450 */
void wlanMacDmaRestart(void);					/* 0x31596 */

/* wlan/txrate.c */
void wlanConnectionRateSetUpdate(struct WlanConnection *connection, uint32_t rateSet);	/* 0x31d94 */
uint8_t wlanTxLowRate(struct WlanConnection *connection, unsigned int start);		/* 0x31db0 */
unsigned int wlanConnectionResponseRate(struct WlanConnection *connection, unsigned int rate); /* 0x31dc0 */
void wlanTxHookIdle(void);					/* 0x31dd0 */
int wlanMacOperationWait(uint32_t timeout, uint32_t *status);	/* 0x31dd2 */
void wlanMacOperationRestore(void);				/* 0x31de8 */
int wlanSlotTableSize(int table);				/* 0x31dfe */
uint32_t wlanMacConfigApply(uint32_t value);			/* 0x31e0a */
uint8_t wlanTxControlRate(struct WlanConnection *connection);	/* 0x31e1c */
unsigned int wlanTxApGroupRate(struct WlanConnection *connection); /* 0x31e42 */

#endif /* ROBIN_MACENTRY_H */
