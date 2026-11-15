/*
 * The hooks the host command code calls through (0x54ea8..0x54ebc): the
 * storage the EEPROM access command reads, the host event send, the channel
 * preparation and the supplicant's PMK removal. wlanSessionOpsInit() points
 * them at the SQRAM, power save and management routines.
 */
#ifndef ROBIN_SESSION_H
#define ROBIN_SESSION_H

#include <stdint.h>

struct WlanConnection;

struct WlanSessionOps {
	int (*eepromOpen)(void);						/* 0x54ea8 */
	int (*eepromRead)(unsigned int offset, uint32_t *value);		/* 0x54eac */
	int (*eepromClose)(void);						/* 0x54eb0 */
	int (*hostEventSend)(struct WlanConnection *connection, uint16_t event); /* 0x54eb4 */
	void (*mgmtPrepare)(uint8_t *band, uint32_t *extra);			/* 0x54eb8 */
	int (*supplicantClearPMK)(const uint8_t *buffer, int length);		/* 0x54ebc */
};

extern struct WlanSessionOps wlanSessionOps;

/* The SQRAM code that reaches the host's EEPROM. */
int uapSqramConvertContext(void);					/* SQRAM 0xc00089b6: opens the storage */
int uapSqramSelectContext(unsigned int offset, uint32_t *value);	/* SQRAM 0xc00089ee: reads a word */
int uapSqramVerifyContext(void);					/* SQRAM 0xc00089e6: closes it */
int uapSqramRecordContext(unsigned int enable, unsigned int argument);	/* SQRAM 0xc0008a1c: write enable */
int uapSqramAdjustContext(unsigned int offset, uint32_t value);		/* SQRAM 0xc0008a00: writes a word */

#endif /* ROBIN_SESSION_H */
