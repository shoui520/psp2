/* Baseband and radio tuning tables, and the serial-controller register helpers of 0x329ae..0x32cfa. */
#ifndef ROBIN_RFCAL_H
#define ROBIN_RFCAL_H

#include <stdint.h>

/* Baseband register 658 as read under selectors 1 and 2 of register 320, written back to 912 and 913. */
void macTxNotifyQueryPacket(void);					/* 0x32926 */

/* Registers of the serial controller (0x90000800): write, read-modify-write and poll. */
void macTxNotifySyncPacket(uint16_t reg, uint16_t value);		/* 0x329ae */
void macTxNotifyApplyPacket(uint16_t reg, uint16_t value);		/* 0x329c0 */
uint8_t macTxNotifyResolvePacket(uint16_t reg);				/* 0x329d2 */
void macTxNotifySchedulePacket(unsigned int mode);			/* 0x329e4 */
void macTxNotifyNotifyPacket(uint16_t reg, uint16_t bits);		/* 0x32a00 */
void macTxNotifyFormatPacket(uint16_t reg, uint16_t bits);		/* 0x32a16 */
int macTxNotifyInspectPacket(uint16_t reg, unsigned int bits);		/* 0x32a2c */
int macTxNotifyTrackPacket(unsigned int bits);				/* 0x32a48 */
void macTxNotifyManagePacket(const uint8_t *image);			/* 0x32a6a */
int macTxNotifyReleasePacket(void *result);				/* 0x32b84 */
void macTxNotifySetupPacket(void);					/* 0x32c16 */
unsigned int macTxNotifyClearPacket(unsigned int level, int scale);	/* 0x32cac */
void macTxNotifyConvertPacket(void);					/* 0x32cc4 */
void macTxNotifyVerifyPacket(void);					/* 0x32cfa */

#endif /* ROBIN_RFCAL_H */
