/*
 * The Bluetooth RF test (SQRAM 0xc000e628..0xc000ebf4): vendor command
 * 0xfc19 sends test packets of one type and length, on two fixed channels or
 * hopping over a channel map, at a set power level, until the host stops it
 * or a set number of packets is sent; HCI Reset stops it too. The link
 * controller's interrupt counts the packets and ends the test. No Marvell
 * source names this code: the filename and the names describe what it does.
 */
#include <stdint.h>
#include "robin/btbuffer.h"
#include "robin/btcontext.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/bttable.h"
#include "robin/eventpower.h"
#include "robin/mdev.h"
#include "robin/rom.h"
#include "robin/sbc.h"
#include "robin/bttest.h"
#include "robin/vendor.h"

/* A halfword of one of the link controller's register windows. */
#define LINK_REG(base, offset)	(*(volatile uint16_t *)((base) + (offset)))

#define BT_RF_TEST_STOP		0xff	/* the channel selection that stops the test */
#define BT_RF_TEST_MESSAGE	3220	/* the message that tells the link controller about it */
#define BT_RF_TEST_SESSIONS_MAX	14

/* The test's parameters, as the host sent them (0x5ad14). */
struct BtRfTestParams {
	uint8_t txSetting;		/* 0x80 and over: keep the link record's */
	uint8_t reserved1;
	uint8_t rxSetting;		/* 0x80 and over: keep the link record's */
	uint8_t reserved3;
	uint8_t channelSelect;		/* BT_RF_TEST_STOP: stop the test */
	uint8_t hopping;		/* 0: the two channels below, else the channel map */
	uint8_t txChannel;
	uint8_t rxChannel;
	uint8_t sessionCount;		/* at most 14 */
	uint8_t packetType;		/* bit 4: an EDR type */
	uint16_t payloadLength;
	uint8_t whitening;
	uint8_t reserved13[3];
	uint32_t packetLimit;		/* 0: no limit */
	int8_t powerLevel;		/* dBm, -20..12 */
	uint8_t channelMap[10];
};

/* The message that tells the link controller about the test. */
struct BtRfTestMessage {
	uint32_t id;			/* BT_RF_TEST_MESSAGE */
	uint16_t hostSlot;
	uint8_t reserved6[4];
	uint8_t channelSelect;
	uint8_t reserved11[3];
	uint8_t sessionCount;
	uint8_t reserved15;
	uint16_t packetSlots;
	uint16_t payloadLength;
};

/* Not converted, in ROM. */
uint16_t btLdaRomSetupState(uint8_t packetType);	/* 0x3f23c44: the slots a packet of the type takes */
void btLdaRomParseState(uint8_t *clock);		/* 0x3f21dfa */

static struct BtRfTestParams btRfTestParams __attribute__((section(".itcm.bss")));	/* 0x5ad14 */
struct BtRfTest btRfTest __attribute__((section(".itcm.rw")));				/* 0x55750 */

/* Tells the link controller the test's channel selection. */
void btRfTestMessageSend(uint8_t channelSelect)
{
	struct BtRfTestMessage *message;
	uint16_t slots;

	slots = btLdaRomSetupState(btRfTestParams.packetType);
	message = osa_MsgAlloc(sizeof(*message));
	message->id = BT_RF_TEST_MESSAGE;
	message->hostSlot = btRfTest.hostSlot;
	message->channelSelect = channelSelect;
	message->sessionCount = btRfTestParams.sessionCount;
	message->packetSlots = slots;
	message->payloadLength = btRfTestParams.payloadLength;
	osa_MsgSend((void **)&message, romQueueLc);
}

/* Sets the link's power index from the power level; EDR types start 7 higher. */
void btRfTestPowerSet(void)
{
	unsigned int link = btRfTest.linkId;
	unsigned int bit = 1u << link;
	unsigned int index;
	int level;

	*btLinkSlotMask |= bit;
	if (btRfTest.edr) {
		*btLinkStatus |= bit;
		index = 7;
	} else {
		*btLinkStatus &= ~bit;
		index = 0;
	}
	LINK_REG(btLinkContext, 400) = *btLinkStatus;
	level = btRfTestParams.powerLevel < 12 ? btRfTestParams.powerLevel : 12;
	level = level > -20 ? level : -20;
	index = (uint8_t)(index + btPowerLevelHeader + 2 * level + 40);
	if (index >= 67)
		index = 66;
	btLmpLcSyncState(link, index);
}

/* Sets the channels the test sends on. */
void btRfTestChannelsSet(void)
{
	uint8_t *context = btLinkContext;
	uint8_t *record;
	unsigned int base;

	LINK_REG(context, 106) = 0xc0;
	if (btRfTestParams.whitening)
		LINK_REG(context, 28) &= ~0x800;
	else
		LINK_REG(context, 28) |= 0x800;
	record = btLinkRecord;
	if (btRfTestParams.txSetting >> 7 == 0)
		LINK_REG(record, 14) = btRfTestParams.txSetting;
	if (btRfTestParams.rxSetting >> 7 == 0)
		LINK_REG(record, 18) = btRfTestParams.rxSetting;
	base = btLinkChannelBase;
	if (btRfTestParams.hopping == 0) {
		LINK_REG(context, 28) |= 0x100;
		LINK_REG(context, 104) = (btRfTestParams.txChannel + base) << 8 |
					 (btRfTestParams.rxChannel + base);
		btRfTest.channelMode = 96;
	} else {
		LINK_REG(context, 28) &= ~0x100;
		LINK_REG(context, 30) = base;
		__aeabi_memcpy(btLinkChannelMap, btRfTestParams.channelMap, sizeof(btRfTestParams.channelMap));
		btRfTest.channelMode = 80;
	}
	LINK_REG(btLinkContext, 36) = 0;
}

/* Takes a host slot for the test's link and sets the link up. */
void btRfTestLinkSetup(void)
{
	uint8_t clock[8];
	uint8_t record[12];
	uint8_t *slot;
	uint8_t *event;
	unsigned int offset;

	btLinkHostSlotAllocHook(&btRfTest.hostSlot);
	slot = romHctHostSlot[btRfTest.hostSlot];
	btRfTest.linkId = slot[2];
	btRfTest.frameId = slot[1];
	btRfTest.group = slot[4];
	*btLinkCommand = btRfTest.group << 5 | 0x6080;
	btLscRomScheduleState(1, (uint32_t)btLinkCommand);
	btLdaRomParseState(clock);
	btLdaRomBuildState(record, clock);
	btLdaRomConfigureState(btVendorP2pRecords + 76 + 12 * btRfTest.group, record, sizeof(record));
	offset = 32 * btRfTest.group;
	LINK_REG(btLinkEvent, offset) = 0;
	LINK_REG(btLinkEvent, offset + 2) = 0;
	event = btLinkEvent + offset;
	LINK_REG(event, 14) = 0;
	LINK_REG(event, 4) = 10;
}

void btRfTestStart(void)
{
	btLmpLcFilterState();
	LINK_REG(btLinkContext, 34) &= 0xcd;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 36) = LINK_REG(btLinkScanSlot, 36) >> 12 << 12;
	osa_LockRelease();
	LINK_REG(btLinkContext, 490) = 0;
	btRfTestLinkSetup();
	btRfTestChannelsSet();
	btRfTestPowerSet();
	btRfTestMessageSend(btRfTestParams.channelSelect);
	btRfTest.active = 1;
	btRfTestLinkEnable();
	btRfTestSessionsSet();
	btRfTest.state = 1;
}

/* Vendor event 25: the test ended, with the packets sent. */
void btRfTestReport(unsigned int status)
{
	btBdhRomConfigureContext(0xff, 0, "bbw", 25, status, btRfTest.packetsSent);
}

/* Gives the link controller's sessions back. */
void btRfTestSessionsReset(void)
{
	int i;

	btLtsRomPrepareState();
	btLinkModeControl->current = 0;
	btLinkModeControl->sessions = 1;
	for (i = 0; i < BT_RF_TEST_SESSIONS_MAX; i++) {
		btLinkSessions[i].active = 1;
		btLinkSessions[i].link = 0;
	}
	btLtsRomDispatchState();
}

/* Stops the test, when it runs. HCI Reset calls this too. */
void btRfTestStop(void)
{
	if (btRfTest.active) {
		btRfTest.active = 0;
		btRfTestSessionsReset();
		osa_LockAcquire();
		LINK_REG(btLinkScanSlot, 58) &= ~0x190;
		osa_LockAcquire();
		LINK_REG(btLinkScanSlot, 2 * btRfTest.linkId) &= ~0xc0;
		osa_LockRelease();
		osa_LockRelease();
		btRfTestLinkRelease();
		btRfTestMessageSend(BT_RF_TEST_STOP);
		btLmpLcHandleContext();
	}
	btRfTest.state = 0;
}

/*
 * Vendor command 0xfc19: starts the test with new parameters, or stops it.
 * The packet limit and the power level need 18 bytes, the channel map 28.
 */
uint8_t btRfTestCommand(unsigned int length, const uint8_t *parameters)
{
	unsigned int sessions;
	unsigned int type;
	unsigned int payload;

	if (btRfTest.active && parameters[4] != BT_RF_TEST_STOP)
		btRfTestStop();
	btRfTestParams.channelSelect = parameters[4];
	if (btRfTestParams.channelSelect == BT_RF_TEST_STOP) {
		if (btRfTest.active) {
			btRfTestStop();
			btRfTestReport(1);
		}
		return 0;
	}
	btRfTestParams.txSetting = parameters[0];
	btRfTestParams.reserved1 = parameters[1];
	btRfTestParams.rxSetting = parameters[2];
	btRfTestParams.reserved3 = parameters[3];
	btRfTestParams.channelSelect = parameters[4];
	btRfTestParams.hopping = parameters[5];
	btRfTestParams.txChannel = parameters[6];
	btRfTestParams.rxChannel = parameters[7];
	btRfTestParams.sessionCount = sessions = parameters[8];
	btRfTestParams.packetType = type = parameters[9];
	btRfTestParams.payloadLength = payload = parameters[10] | parameters[11] << 8;
	btRfTestParams.whitening = parameters[12];
	if (length >= 18) {
		btRfTestParams.packetLimit = parameters[13] | parameters[14] << 8 |
					     (parameters[15] << 16 | parameters[16] << 24);
		btRfTestParams.powerLevel = parameters[17];
	} else {
		btRfTestParams.packetLimit = 0;
		btRfTestParams.powerLevel = 4;
	}
	if (length >= 28)
		__aeabi_memcpy(btRfTestParams.channelMap, parameters + 18, sizeof(btRfTestParams.channelMap));
	else
		__aeabi_memset4(btRfTestParams.channelMap, sizeof(btRfTestParams.channelMap), 0xff);
	if (payload == 0 || payload > romStateActiveTable[type] || sessions > BT_RF_TEST_SESSIONS_MAX)
		return BT_STATUS_INVALID;
	btRfTest.edr = type >> 4 & 1;
	btRfTestStart();
	return 0;
}

/*
 * The link controller's interrupt while the test runs: counts the packets
 * sent, ends the test at its limit, and reads the status registers it
 * raised to clear them.
 */
void btRfTestInterrupt(void)
{
	unsigned int status;
	unsigned int window;
	unsigned int events;
	unsigned int frame;
	int i;

	status = LINK_REG(btLinkScanWindow, 62);
	window = LINK_REG(btLinkScanWindow, 32);
	if (window != 0)
		btLdrRomCheckState(window);
	if (status & 0x10) {
		events = LINK_REG(btLinkScanWindow, 58);
		if (events & 0x100) {
			btRfTest.packetsSent++;
			if (btRfTestParams.packetLimit != 0 &&
			    btRfTestParams.packetLimit <= btRfTest.packetsSent)
				btRfTest.state = 2;
		}
		if ((events & 0x80) && btRfTest.state == 2) {
			btRfTestSessionsReset();
			LINK_REG(btLinkScanSlot, 58) &= ~0x180;
			btRfTest.state = 3;
		}
		if ((events & 0x10) && btRfTest.state == 3) {
			btRfTestStop();
			btRfTestReport(0);
		}
	}
	if ((status & 0x20) && (LINK_REG(btLinkScanWindow, 64) & 0x3ff) != 0) {
		frame = LINK_REG(btLinkScanWindow, 2 * btRfTest.linkId);
		if (frame != 0 && (frame >> 6 & 3) != 0)
			btLmpLcFormatFrameHook(btRfTest.linkId, (int8_t)frame);
	}
	if (status & 0x40)
		(void)LINK_REG(btLinkScanWindow, 38);
	if (status & 0x100) {
		for (i = 0; i < 3; i++)
			(void)LINK_REG(btLinkScanWindow, 48 + 2 * i);
		(void)LINK_REG(btLinkScanWindow, 54);
	}
	if (status & 0x80)
		for (i = 0; i < 4; i++)
			(void)LINK_REG(btLinkScanWindow, 40 + 2 * i);
	if (status & 0x200)
		(void)LINK_REG(btLinkScanWindow, 30);
	if (status & 0x400)
		(void)LINK_REG(btLinkScanWindow, 34);
	if (status & 0x2000)
		(void)LINK_REG(btLinkScanWindow, 36);
}

/* Sets up the sessions the test sends in, all on the test's link. */
void btRfTestSessionsSet(void)
{
	unsigned int first;
	unsigned int other;
	uint16_t link;
	int i;

	btLtsRomPrepareState();
	btLinkModeControl->current = 0;
	btLinkModeControl->sessions = btRfTestParams.sessionCount;
	first = (btRfTest.channelMode << 8) + 19;
	other = (btRfTest.channelMode << 8) + 17;
	link = btRfTest.group << 14 | btRfTest.linkId;
	for (i = 0; i < btRfTestParams.sessionCount; i++) {
		if (i == 0)
			btLinkSessions[0].active = first;
		else
			btLinkSessions[i].active = other;
		btLinkSessions[i].link = link;
		btLinkSessions[i].flags = 0x103;
	}
	btLtsRomDispatchState();
}

void btRfTestLinkEnable(void)
{
	LINK_REG(btLinkContext, 126) = 1u << btRfTest.linkId;
	osa_LockAcquire();
	btRfTest.packetsSent = 0;
	osa_LockAcquire();
	LINK_REG(btLinkScanSlot, 2 * btRfTest.linkId) |= 0xc0;
	osa_LockRelease();
	LINK_REG(btLinkScanSlot, 58) |= 0x190;
	osa_LockRelease();
}

/* Gives the test's frame, link and host slot back. */
void btRfTestLinkRelease(void)
{
	unsigned int bit;

	btLdaRomProcessState(btLinkHostState + 28, btRfTest.frameId);
	btLdaRomProcessState(btLinkHostState + 24, btRfTest.linkId);
	btLdaRomProcessState(btLinkHostState + 20, btRfTest.hostSlot);
	romHctHostSlot[btRfTest.hostSlot][0] = 0;
	bit = 1u << btRfTest.linkId;
	*btLinkSlotMask &= ~bit;
	*(volatile uint16_t *)btLinkStatus &= ~bit;
	*(volatile uint16_t *)btLinkStatus &= ~bit;
}
