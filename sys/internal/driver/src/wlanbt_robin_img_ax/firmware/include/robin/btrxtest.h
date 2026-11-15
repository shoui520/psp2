/*
 * The Bluetooth RX test (sqram/btrxtest.c, SQRAM 0xc000ebf4..0xc000f298).
 */
#ifndef ROBIN_BTRXTEST_H
#define ROBIN_BTRXTEST_H

#include <stdint.h>

/* Where the test stands, and its parameters as the host sent them (0x55760). */
struct BtRxTest {
	uint8_t patternType;		/* 0xff: stop; 4: pseudo-random */
	uint8_t txChannel;
	uint8_t rxChannel;
	uint8_t packetType;		/* bit 4: an EDR type */
	uint8_t reportCount;		/* packets with bit errors still reported; 0xff: all */
	uint8_t ltAddress;
	uint8_t edr;
	uint8_t slots;			/* the slots a packet of the type takes */
	uint8_t countdown;		/* events left until the test ends */
	uint8_t receiving;		/* an access code was found, the packet not ended */
	uint8_t active;
	uint8_t state;			/* 1 receiving, 2 at the limit, 3 ending */
	uint8_t syncPending;
	uint8_t reserved13;
	uint16_t payloadLength;
	uint8_t *pattern;		/* the payload the sender sends */
	uint32_t packetLimit;
	uint32_t packetsReceived;
	uint8_t address[6];		/* the sender's */
};
extern struct BtRxTest btRxTest;			/* 0x55760 */

struct BtRxTestMessage;

void btRxTestStart(void);					/* SQRAM 0xc000ebf4 */
void btRxTestReport(unsigned int status);			/* SQRAM 0xc000ecaa */
void btRxTestSessionsReset(void);				/* SQRAM 0xc000ed3a */
void btRxTestStop(void);					/* SQRAM 0xc000ed68 */
uint8_t btRxTestCommand(unsigned int length, const uint8_t *parameters);	/* SQRAM 0xc000edbc */
void btRxTestPacketsCollect(int mask);				/* SQRAM 0xc000eeb6 */
void btRxTestInterrupt(void);					/* SQRAM 0xc000ef44, veneer 0x53acc */
void btRxTestBitErrors(unsigned int length, const uint8_t *received, const uint8_t *expected,
		       uint32_t *errors, uint32_t *bits);	/* SQRAM 0xc000f0b6 */
void btRxTestPacketReceived(uint8_t *packet);			/* SQRAM 0xc000f0ea */
void btRxTestSessionsSet(void);					/* SQRAM 0xc000f118 */
void btRxTestLinkEnable(void);					/* SQRAM 0xc000f15e */
void btRxTestChannelsSet(void);					/* SQRAM 0xc000f1a8 */
void btRxTestPacketCheck(struct BtRxTestMessage *message);	/* SQRAM 0xc000f1f4 */

#endif /* ROBIN_BTRXTEST_H */
