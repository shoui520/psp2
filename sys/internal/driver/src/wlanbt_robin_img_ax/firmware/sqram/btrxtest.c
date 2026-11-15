/*
 * The Bluetooth RX test (SQRAM 0xc000ebf4..0xc000f298): vendor command
 * 0xfc18 receives test packets of one type and length from a device on two
 * fixed channels, compares each payload with the pattern the sender uses,
 * and reports the packet and bit error counts when the set number of
 * packets has passed or the host stops it. No Marvell source names this
 * code: the filename and the names describe what it does.
 */
#include <stdint.h>
#include "robin/btbuffer.h"
#include "robin/btcontext.h"
#include "robin/btpool.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/bttable.h"
#include "robin/btrxtest.h"
#include "robin/eventpower.h"
#include "robin/mdev.h"
#include "robin/rom.h"
#include "robin/vendor.h"

/* A halfword of one of the link controller's register windows. */
#define LINK_REG(base, offset)	(*(volatile uint16_t *)((base) + (offset)))
#define LINK_REG8(base, offset)	(*(volatile uint8_t *)((base) + (offset)))

#define BT_RX_TEST_STOP		0xff	/* the pattern that stops the test */
#define BT_RX_TEST_PRBS		4	/* the pseudo-random pattern */
#define BT_RX_TEST_SESSIONS	5
#define BT_RX_TEST_REPORTS_ALL	0xff	/* report every packet with bit errors */
#define BT_STATUS_BUSY		12	/* HCI status: command disallowed */

/* A packet the interrupt hands to the link controller's task. */
struct BtRxTestMessage {
	uint32_t id;			/* BT_MESSAGE_P2P_TABLE */
	uint16_t length;
	uint8_t reserved6[2];
	uint8_t *data;
	uint8_t ignore;			/* received outside the test's packets */
};

/* What the test counted, reported to the host as vendor event 1 (0x5ad34). */
struct BtRxTestStats {
	uint32_t packetsExpected;
	uint32_t packetsMissed;
	uint32_t syncsFound;		/* access codes detected */
	uint32_t headerErrors;
	uint32_t payloadErrors;		/* the link controller's */
	uint32_t packetsShort;
	uint32_t crcErrors;
	uint32_t packetsReceived;
	uint32_t packetsGood;
	uint32_t packetsLost;		/* missed or short */
	uint32_t packetErrorRate;	/* percent */
	uint32_t bitsExpected;
	uint32_t bitErrorsAll;		/* the lost packets' bits count as errors */
	uint32_t bitErrorRate;		/* percent */
	uint32_t bytesCompared;
	uint32_t bitErrors;
	int32_t rssi;			/* -1: nothing received */
};

/* The receive buffers the link controller fills (DTCM). */
extern uint8_t btLinkRxBufferCount;			/* 0x400f70c */
extern uint8_t btLinkRxBufferIndex;			/* 0x400f70d: the buffer filled next */
extern uint16_t *btLinkRxBufferStates;			/* 0x400f928: 1 when filled */
extern uint16_t *btLinkRxBufferOffsets;			/* 0x400f938 */
extern uint8_t *btLinkRxBufferBase;			/* 0x400f988 */
extern void (*btLinkRxBufferHook)(unsigned int index);	/* 0x400f158: takes the buffer */

/* Not converted, in ROM. */
void btLcoRomApplyContext(unsigned int enable);		/* 0x3f1f9bc */
unsigned int btLcoRomInspectContext(void);		/* 0x3f2000e: seeds the pseudo-random pattern */
unsigned int btLcoRomFormatContext(unsigned int value);	/* 0x3f1ffe6: its next byte */

/* The byte each pattern repeats (SQRAM 0xc000fa90). */
static const uint8_t btRxTestPatterns[] = { 0xfe, 0x00, 0x00, 0xff, 0x55 };

struct BtRxTest btRxTest __attribute__((section(".itcm.rw")));				/* 0x55760 */
static struct BtRxTestStats btRxTestStats __attribute__((section(".itcm.bss")));	/* 0x5ad34 */

void btRxTestStart(void)
{
	uint8_t record[12];
	uint8_t *context;
	uint8_t *event;

	btRxTest.countdown = 0;
	btRxTest.state = 1;
	btRxTest.syncPending = 0;
	btLmpLcFilterState();
	context = btLinkContext;
	LINK_REG(context, 490) = 0;
	LINK_REG(context, 34) &= 0xcd;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 36) = LINK_REG(btLinkScanSlot, 36) >> 12 << 12;
	osa_LockRelease();
	*btLinkCommand = 0x7100;
	btLscRomScheduleState(1, (uint32_t)btLinkCommand);
	*btLinkCommand = 0x60a0;
	btLscRomScheduleState(1, (uint32_t)btLinkCommand);
	btLdaRomBuildState(record, btRxTest.address);
	btLdaRomConfigureState(btVendorP2pRecords + 88, record, sizeof(record));
	event = btLinkEvent;
	LINK_REG(event, 32) = 1;
	LINK_REG(event, 34) = 0;
	LINK_REG(event, 46) = 0;
	LINK_REG(event, 36) = 0xffc8;
	LINK_REG(event, 60) = btRxTest.ltAddress;
	btRxTestChannelsSet();
	*btLinkSlotMask |= 2;
	if (btRxTest.edr == 0)
		*btLinkStatus &= ~2;
	else
		*btLinkStatus |= 2;
	btLcoRomApplyContext(1);
	btRxTest.active = 1;
	btRxTestLinkEnable();
	btRxTestSessionsSet();
}

/* Vendor event 1: what the test counted. */
void btRxTestReport(unsigned int status)
{
	uint32_t limit = btRxTest.packetLimit;
	uint32_t bits;
	uint32_t errors;
	unsigned int length;

	btRxTestStats.packetsExpected = limit;
	length = btRxTest.payloadLength;
	bits = limit * length * 8;
	btRxTestStats.bitsExpected = bits;
	btRxTestStats.packetsReceived = btRxTest.packetsReceived;
	btRxTestStats.packetsMissed = limit - btRxTest.packetsReceived;
	if (limit < btRxTest.packetsReceived)
		btRxTestStats.packetsMissed = 0;
	btRxTestStats.packetsLost = btRxTestStats.packetsMissed + btRxTestStats.packetsShort;
	errors = btRxTestStats.bitErrors + btRxTestStats.packetsLost * length * 8;
	btRxTestStats.bitErrorsAll = errors;
	btRxTestStats.packetErrorRate = (limit - btRxTestStats.packetsGood) * 100 / limit;
	btRxTestStats.bitErrorRate = errors * 100 / bits;
	if (btRxTest.packetsReceived != 0)
		/* the hook takes the signed level byte */
		btRxTestStats.rssi = ((int (*)(int))btPowerHooks.payload)((int8_t)(*(uint16_t *)(btPacketTypes + 12) >> 8));
	else
		btRxTestStats.rssi = -1;
	btBdhRomConfigureContext(0xff, 0, "bbm", 1, status, &btRxTestStats, sizeof(btRxTestStats));
}

/* Gives the link controller's sessions back. */
void btRxTestSessionsReset(void)
{
	int i;

	btLtsRomPrepareState();
	btLinkModeControl->header = 0;
	btLinkModeControl->sessions = 1;
	for (i = 0; i < BT_RX_TEST_SESSIONS; i++) {
		btLinkSessions[i].active = 1;
		btLinkSessions[i].link = 0;
	}
	btLtsRomDispatchState();
}

void btRxTestStop(void)
{
	btLmpLcHandleContext();
	btRxTestSessionsReset();
	*btLinkSlotMask &= ~2;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 58) &= ~0x3b;
	LINK_REG(btLinkScanSlot, 2) &= ~1;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 38) = 0;
	osa_LockRelease();
	osa_LockRelease();
	if (btRxTest.pattern != NULL) {
		btEntryHooks.acquire(btRxTest.pattern);
		btRxTest.pattern = NULL;
	}
	btRxTest.active = 0;
}

/*
 * Vendor command 0xfc18: starts the test, or stops it and reports. The
 * pattern buffer holds the payload the sender sends, to compare with.
 */
uint8_t btRxTestCommand(unsigned int length, const uint8_t *parameters)
{
	unsigned int type;
	unsigned int payload;
	unsigned int slots;
	unsigned int i;
	unsigned int value;
	uint8_t *pattern;

	if (btRxTest.active && parameters[0] != BT_RX_TEST_STOP)
		return BT_STATUS_BUSY;
	btRxTest.patternType = parameters[0];
	if (parameters[0] == BT_RX_TEST_STOP) {
		if (btRxTest.active)
			btRxTestStop();
		btRxTestReport(0);
		return 0;
	}
	btRxTest.txChannel = parameters[1];
	btRxTest.rxChannel = parameters[2];
	btRxTest.packetType = type = parameters[3];
	btRxTest.packetLimit = parameters[4] | parameters[5] << 8 | (parameters[6] << 16 | parameters[7] << 24);
	btRxTest.payloadLength = payload = parameters[8] | parameters[9] << 8;
	btRxTest.ltAddress = parameters[10];
	__aeabi_memcpy(btRxTest.address, parameters + 11, sizeof(btRxTest.address));
	btRxTest.reportCount = parameters[17];
	slots = type & 0xf;
	if (payload == 0 || romStateActiveTable[type] < payload)
		return BT_STATUS_INVALID;
	btRxTest.edr = type >> 4 & 1;
	if (slots == 14 || slots == 15)
		btRxTest.slots = 5;
	else if (slots == 10 || slots == 11)
		btRxTest.slots = 3;
	else
		btRxTest.slots = 1;
	if (btRxTest.pattern == NULL)
		btRxTest.pattern = btEntryHooks.control();
	if (btRxTest.patternType != BT_RX_TEST_PRBS) {
		__aeabi_memset4(btRxTest.pattern, btRxTest.payloadLength, btRxTestPatterns[btRxTest.patternType]);
	} else {
		__aeabi_memclr(btRxTest.pattern, btRxTest.payloadLength);
		payload = btRxTest.payloadLength;
		pattern = btRxTest.pattern;
		value = btLcoRomInspectContext();
		for (i = 0; (int)i < (int)payload; i++) {
			value = btLcoRomFormatContext(value);
			pattern[i] = value;
		}
	}
	__aeabi_memclr4(&btRxTestStats, sizeof(btRxTestStats));
	btRxTestStart();
	return 0;
}

/* Hands the filled receive buffers the mask names to the link controller's task. */
void btRxTestPacketsCollect(int mask)
{
	struct BtRxTestMessage *message;
	unsigned int index;
	unsigned int i;
	uint8_t *buffer;

	for (i = 0; i < btLinkRxBufferCount; i = (uint8_t)(i + 1)) {
		index = btLinkRxBufferIndex;
		if ((1 << index & mask) == 0) {
			if ((1 << (index + 8) & mask) == 0)
				return;
			continue;
		}
		if (btLinkRxBufferStates[index] != 1)
			continue;
		buffer = btLinkRxBufferBase + btLinkRxBufferOffsets[index];
		btLinkRxBufferHook(index);
		message = osa_MsgAlloc(sizeof(*message));
		message->id = BT_MESSAGE_P2P_TABLE;
		message->length = *(uint16_t *)(buffer + 2) >> 3 & 0x3ff;
		message->data = buffer + 4;
		if (btRxTest.state == 1 || btRxTest.state == 2) {
			message->ignore = 0;
			btRxTestStats.packetsReceived++;
		} else {
			message->ignore = 1;
		}
		osa_MsgSend((void **)&message, btQueueLcCommand);
	}
}

/*
 * The link controller's interrupt while the test runs: counts what the
 * receiver found, collects the packets, and ends the test five events
 * after the last packet.
 */
void btRxTestInterrupt(void)
{
	unsigned int status;
	unsigned int window;
	unsigned int events;
	unsigned int buffers;

	status = LINK_REG(btLinkScanWindow, 62);
	window = LINK_REG(btLinkScanWindow, 32);
	if (window != 0)
		btLdrRomCheckState(window);
	if ((status & 0x20) && (LINK_REG(btLinkScanWindow, 64) & 0x3ff) != 0 &&
	    (LINK_REG(btLinkScanWindow, 2) & 1) != 0) {
		if (btRxTest.state != 3 || btRxTest.syncPending != 0) {
			btRxTestStats.headerErrors++;
			if (btRxTest.syncPending != 0)
				btRxTest.syncPending = 0;
		}
	}
	if (status & 0x40) {
		buffers = LINK_REG(btLinkScanWindow, 38);
		if (buffers != 0)
			btRxTestPacketsCollect((int16_t)buffers);
	}
	if ((status & 0x10) == 0)
		return;
	events = LINK_REG(btLinkScanWindow, 58);
	if (events & 0x08) {
		LINK_REG(btLinkEvent, 36) = 32;
		if (++btRxTestStats.syncsFound >= btRxTest.packetLimit)
			btRxTest.state = 2;
		btRxTest.receiving = 1;
	}
	if ((events & 0x01) && btRxTest.receiving)
		btRxTestStats.crcErrors++;
	if ((events & 0x02) && btRxTest.receiving)
		btRxTestStats.payloadErrors++;
	if (events & 0x20) {
		if (btRxTest.receiving) {
			btRxTest.receiving = 0;
			btRxTest.packetsReceived++;
		}
		if (btRxTest.state == 2) {
			btRxTestSessionsReset();
			LINK_REG(btLinkScanSlot, 58) &= ~0x2b;
			btRxTest.state = 3;
			btRxTest.syncPending = 1;
			btRxTest.countdown = 5;
			btRxTest.receiving = 0;
		}
	}
	if ((events & 0x10) && btRxTest.countdown != 0) {
		btRxTest.countdown = (uint8_t)(btRxTest.countdown - 1);
		if (btRxTest.countdown == 0) {
			btRxTestStop();
			btRxTestReport(0);
		}
	}
}

/* Counts the bits in which the received payload differs from the pattern. */
void btRxTestBitErrors(unsigned int length, const uint8_t *received, const uint8_t *expected,
		       uint32_t *errors, uint32_t *bits)
{
	unsigned int count = 0;
	int i;
	int difference;

	for (i = 0; i < btRxTest.payloadLength; i++) {
		for (difference = received[i] ^ expected[i]; difference != 0; difference >>= 1)
			if (difference & 1)
				count++;
	}
	*errors = count;
	*bits = length * 8;
}

/* The link controller's task takes a received packet (message BT_MESSAGE_P2P_TABLE). */
void btRxTestPacketReceived(uint8_t *packet)
{
	struct BtRxTestMessage *message = (struct BtRxTestMessage *)packet;

	if (message->ignore == 0) {
		if (message->length >= btRxTest.payloadLength)
			btRxTestPacketCheck(message);
		else
			btRxTestStats.packetsShort++;
	}
	btEntryHooks.release(message->data);
}

void btRxTestSessionsSet(void)
{
	unsigned int active;
	int i;

	btLtsRomPrepareState();
	btLinkModeControl->header = 0;
	btLinkModeControl->sessions = BT_RX_TEST_SESSIONS;
	btRxTest.packetsReceived = 0;
	active = (btRxTest.ltAddress << 4) + 0x6003;
	for (i = 0; i < BT_RX_TEST_SESSIONS; i++) {
		btLinkSessions[i].active = active;
		btLinkSessions[i].link = 0x4001;
		btLinkSessions[i].flags = 0x103;
	}
	btLtsRomDispatchState();
}

void btRxTestLinkEnable(void)
{
	unsigned int mask;

	LINK_REG(btLinkContext, 126) = 2;
	osa_LockAcquire();
	btRxTest.packetsReceived = 0;
	LINK_REG(btLinkScanSlot, 2) |= 1;
	mask = (1 << btLinkRxBufferCount) - 1;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 38) = mask << 8 | mask;
	osa_LockRelease();
	LINK_REG(btLinkScanSlot, 58) |= 0x3b;
	osa_LockRelease();
}

/* Receives on the two fixed channels, with whitening off. */
void btRxTestChannelsSet(void)
{
	uint8_t *context = btLinkContext;
	uint8_t *record;

	LINK_REG(context, 106) = 0xc0;
	LINK_REG(context, 28) |= 0x800;
	LINK_REG(context, 28) |= 0x100;
	LINK_REG(context, 28) &= 0x7fff;
	LINK_REG(context, 104) = btRxTest.txChannel << 8 | btRxTest.rxChannel;
	LINK_REG(context, 36) = 0;
	record = btLinkRecord;
	LINK_REG8(record, 12) = LINK_REG8(record, 12) >> 5 << 5;
	LINK_REG8(record, 12) |= 0x1f;
}

/* Compares a received payload with the pattern, and reports it when it has bit errors. */
void btRxTestPacketCheck(struct BtRxTestMessage *message)
{
	uint8_t *data = message->data;
	uint32_t errors = 0;
	uint32_t bits = 0;

	if (data == NULL)
		return;
	btRxTestBitErrors(btRxTest.payloadLength, data, btRxTest.pattern, &errors, &bits);
	btRxTestStats.bitErrors += errors;
	btRxTestStats.bytesCompared += btRxTest.payloadLength;
	if (errors == 0) {
		btRxTestStats.packetsGood++;
		return;
	}
	if (btRxTest.reportCount == 0)
		return;
	if (btRxTest.reportCount != BT_RX_TEST_REPORTS_ALL)
		btRxTest.reportCount--;
	btBdhRomConfigureContext(0xff, 0, "bbhm", 2, 0, message->length, data, 64);
}
