/*
 * The Bluetooth RF test (sqram/bttest.c, SQRAM 0xc000e628..0xc000ebf4).
 */
#ifndef ROBIN_BTTEST_H
#define ROBIN_BTTEST_H

#include <stdint.h>

/* Where the test stands (0x55750). */
struct BtRfTest {
	uint8_t state;			/* 0 idle, 1 sending, 2 at the limit, 3 ending */
	uint8_t active;
	uint8_t edr;
	uint8_t frameId;		/* the host slot's frame */
	uint8_t group;			/* the host slot's link group */
	uint8_t linkId;			/* the host slot's link */
	uint8_t channelMode;		/* 96: fixed channels, 80: hopping */
	uint8_t reserved7;
	uint16_t hostSlot;
	uint8_t reserved10[2];
	uint32_t packetsSent;
};
extern struct BtRfTest btRfTest;			/* 0x55750 */

void btRfTestMessageSend(uint8_t channelSelect);		/* SQRAM 0xc000e628 */
void btRfTestPowerSet(void);					/* SQRAM 0xc000e660 */
void btRfTestChannelsSet(void);					/* SQRAM 0xc000e6d0 */
void btRfTestLinkSetup(void);					/* SQRAM 0xc000e752 */
void btRfTestStart(void);					/* SQRAM 0xc000e7d0 */
void btRfTestReport(unsigned int status);			/* SQRAM 0xc000e826 */
void btRfTestSessionsReset(void);				/* SQRAM 0xc000e83e */
void btRfTestStop(void);					/* SQRAM 0xc000e86c, veneer 0x53b64 */
uint8_t btRfTestCommand(unsigned int length, const uint8_t *parameters);	/* SQRAM 0xc000e8bc, veneer 0x53b6c */
void btRfTestInterrupt(void);					/* SQRAM 0xc000e994, veneer 0x53ad4 */
void btRfTestSessionsSet(void);					/* SQRAM 0xc000ead2 */
void btRfTestLinkEnable(void);					/* SQRAM 0xc000eb36 */
void btRfTestLinkRelease(void);					/* SQRAM 0xc000eb7a */

#endif /* ROBIN_BTTEST_H */
