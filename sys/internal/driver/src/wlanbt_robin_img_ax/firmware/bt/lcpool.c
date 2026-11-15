/*
 * The Bluetooth controller's buffer pools (0x43724 - 0x4381x): the SQRAM memory is cleared and the three
 * pools of the controller (tables, packets, events) are created over it, and the allocators of its packet
 * and event buffers. The file name and roles are inferred; the function names are the firmware's
 * placeholders.
 */
#include <stddef.h>
#include "robin/btpool.h"
#include "robin/btpower.h"
#include "robin/btrom.h"
#include "robin/clock.h"
#include "robin/rfreg.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/socpower.h"
#include "robin/sys.h"

/* Bit of SOC_EVENT_ENGINE_GATE the radio set-up holds open. */
#define EVENT_ENGINE_RADIO	(1u << 24)

/* Where a buffer's data begins: after the allocator's header and the controller's own. */
#define POOL_DATA_OFFSET	40

/* The pools and their memory in SQRAM0 (0xc0001100 - 0xc00071b4). */
uint32_t btEventPoolFailures __attribute__((section(".sqram0.rw")));			/* 0xc0001100 */
uint32_t btPacketPoolFailures __attribute__((section(".sqram0.rw")));			/* 0xc0001104 */
struct OsaPoolConfig btPacketPoolConfig __attribute__((section(".sqram0.rw"))) = { 0, 0x468, BT_PACKET_POOL_BLOCKS };	/* 0xc0001108 */
struct OsaPoolConfig btEventPoolConfig __attribute__((section(".sqram0.rw"))) = { 0, 0x464, BT_EVENT_POOL_BLOCKS };	/* 0xc0001110 */
struct OsaBufferPool btTablePool __attribute__((section(".sqram0.rw"))) = { { { BT_TABLE_POOL_CLASSES, 0, 0 } } };	/* 0xc0001118 */
struct OsaPoolConfig btTablePoolConfig[BT_TABLE_POOL_CLASSES] __attribute__((section(".sqram0.rw"))) = {	/* 0xc0001124 */
	{ 0, 0x4c, 12 }, { 0, 0x9c, 8 }, { 0, 0x158, 12 },
};
struct OsaBufferPool btPacketPool __attribute__((section(".sqram0.rw"))) = { { { 1, 0, 0 } } };	/* 0xc000113c */
struct OsaBufferPool btEventPool __attribute__((section(".sqram0.rw"))) = { { { 1, 0, 0 } } };	/* 0xc0001148 */

/* A signed 14-bit value of the radio's table. */
#define S14(value)	((uint16_t)((value) & 0x3fff))

union BtPoolFrameMemory btPoolFrameMemory __attribute__((section(".sqram0.rw"), aligned(4))) = {	/* 0xc0001154 */
	.radioTable = {
		.length = 2 * BT_RADIO_TABLE_WORDS,
		.words = {
			S14(26), S14(-52), S14(-112), S14(-157), S14(-191), S14(-215), S14(-232), S14(-244),
			S14(-251), S14(-254), S14(-255), S14(-255), S14(-253), S14(-250), S14(-247), S14(-244),
			S14(-241), S14(-238), S14(-235), S14(-232), S14(-230), S14(-227), S14(-225), S14(-222),
			S14(-220), S14(-218), S14(-216), S14(-213), S14(-211), S14(-208), S14(-205), S14(-202),
			S14(-198), S14(-195), S14(-191), S14(-187), S14(-182), S14(-178), S14(-173), S14(-168),
			S14(-163), S14(-158), S14(-153), S14(-148), S14(-143), S14(-138), S14(-132), S14(-127),
			S14(-122), S14(-117), S14(-112), S14(-107), S14(-102), S14(-97), S14(-92), S14(-87),
			S14(-83), S14(-78), S14(-73), S14(-68), S14(-64), S14(-59), S14(-54), S14(-49),
			S14(-44), S14(-39), S14(-34), S14(-29), S14(-24), S14(-19), S14(-14), S14(-8),
			S14(-3), S14(3), S14(8), S14(14), S14(19), S14(25), S14(31), S14(37),
			S14(42), S14(48), S14(54), S14(60), S14(65), S14(71), S14(77), S14(82),
			S14(88), S14(93), S14(99), S14(104), S14(109), S14(115), S14(120), S14(125),
			S14(131), S14(136), S14(141), S14(147), S14(152), S14(158), S14(164), S14(169),
			S14(175), S14(181), S14(187), S14(193), S14(199), S14(206), S14(212), S14(218),
			S14(224), S14(230), S14(236), S14(241), S14(247), S14(252), S14(257), S14(262),
			S14(267), S14(271), S14(276), S14(282), S14(288), S14(296), S14(305), S14(317),
		},
		.start = 0x4643,
	},
};

/* The Bluetooth radio's 8051 program (radio/bt, loaded at 0x8000 by btLmpLcSyncEntry), then zeros: the pool takes the area over. */
uint8_t btPoolHeaderMemory[BT_POOL_AREA_SIZE] __attribute__((section(".sqram0.rw"), aligned(4))) = {
#include "bt_radio.inc"
};	/* 0xc00022f4 */

uint8_t btTablePoolControl[4 * BT_TABLE_POOL_BLOCKS] __attribute__((section(".sqram0.bss1"), aligned(4)));	/* 0xc0005f64 */
uint8_t btPacketPoolControl[4 * BT_PACKET_POOL_BLOCKS] __attribute__((section(".sqram0.bss1"), aligned(4)));	/* 0xc0005fe4 */
uint8_t btEventPoolControl[4 * BT_EVENT_POOL_BLOCKS] __attribute__((section(".sqram0.bss1"), aligned(4)));	/* 0xc0006004 */
uint8_t btRecordBuffers[BT_RECORD_BUFFERS][BT_RECORD_SIZE] __attribute__((section(".sqram0.bss1"), aligned(4)));	/* 0xc0006014 */
uint8_t btContextBuffers[BT_RECORD_BUFFERS][BT_RECORD_SIZE] __attribute__((section(".sqram0.bss1"), aligned(4)));	/* 0xc00068e4 */

int btLmpLcRecordContext(void)
{
	__aeabi_memclr(btPoolHeaderMemory, sizeof(btPoolHeaderMemory));
	__aeabi_memclr(&btPoolFrameMemory, sizeof(btPoolFrameMemory));
	osa_BlockPoolCreate(&btTablePool, btTablePoolConfig, btTablePoolControl, btPoolHeaderMemory, osaPoolEntryCreate);
	osa_BlockPoolCreate(&btPacketPool, &btPacketPoolConfig, btPacketPoolControl, btPoolDescriptorMemory, osaPoolEntryCreate);
	return osa_BlockPoolCreate(&btEventPool, &btEventPoolConfig, btEventPoolControl, &btPoolFrameMemory, osaPoolEntryCreate);
}

void btLmpLcFilterContext(void)
{
}

/* An event buffer; waits for one outside an interrupt. */
uint8_t *btLmpLcHandleEntry(void)
{
	OsaBuffer *buffer = osa_BufferAlloc(&btEventPool, 0, sysInIsr() != 0 ? 0 : 0xffffffff);

	if (buffer != NULL) {
		buffer->dataOffset = POOL_DATA_OFFSET;
		return (uint8_t *)buffer + POOL_DATA_OFFSET;
	}
	btEventPoolFailures++;
	return NULL;
}

int btLmpLcProcessEntry(uint8_t *packet)
{
	return btLimitParam(packet);
}

/* A packet buffer. */
uint8_t *btLmpLcUpdateEntry(void)
{
	OsaBuffer *buffer = osa_BufferAlloc(&btPacketPool, 0, 0);

	if (buffer != NULL) {
		buffer->dataOffset = POOL_DATA_OFFSET;
		return (uint8_t *)buffer + POOL_DATA_OFFSET;
	}
	btPacketPoolFailures++;
	return NULL;
}

int btLmpLcConfigureEntry(uint8_t *packet)
{
	return osa_BufferFree((OsaBuffer *)(packet - POOL_DATA_OFFSET));
}

/* The record's buffer of the link: counted, and its data offset set; returns the data. */
uint8_t *btLmpLcCheckEntry(int index)
{
	uint8_t *record = btRecordBuffers[index];

	record[14]++;
	*(uint16_t *)(record + 8) = 32;
	return record + 32;
}

/* The first record's buffer of the context table at index: its data. */
uint8_t *btLmpLcValidateEntry(int index)
{
	uint8_t *record = btRecordBuffers[index];

	record[14]--;
	return record;
}

uint8_t *btLmpLcPrepareEntry(int index)
{
	return btRecordBuffers[index] + 32;
}

uint8_t *btLmpLcDispatchEntry(int index)
{
	return btContextBuffers[index] + 32;
}

uint8_t *btLmpLcParseEntry(int index)
{
	uint8_t *record = btContextBuffers[index];

	record[14]++;
	*(uint16_t *)(record + 8) = 32;
	return record + 32;
}

uint8_t *btLmpLcBuildEntry(int index)
{
	uint8_t *record = btContextBuffers[index];

	record[14]--;
	return record;
}

uint8_t *btLmpLcComputeEntry(int index)
{
	return btContextBuffers[index];
}

/* Installs the buffer hooks in the ROM. */
uint8_t *(*btLmpLcQueryEntry(void))(int index)
{
	btEntryHooks.node = btLmpLcFilterContext;
	btEntryHooks.config = btLmpLcHandleEntry;
	btEntryHooks.release = btLmpLcProcessEntry;
	btEntryHooks.control = btLmpLcUpdateEntry;
	btEntryHooks.acquire = btLmpLcConfigureEntry;
	btEntryHooks.bitmap = btLmpLcCheckEntry;
	btEntryHooks.rate = btLmpLcValidateEntry;
	btEntryHooks.slot = btLmpLcParseEntry;
	btEntryHooks.window = btLmpLcBuildEntry;
	btEntryHooks.key = btLmpLcPrepareEntry;
	btEntryHooks.counter = btLmpLcDispatchEntry;
	return btLmpLcDispatchEntry;
}

/* The packet pool's config names the owner whose state byte (offset 8) the coexistence code reads. */
unsigned int btLmpLcResetEntry(void)
{
	const uint8_t *owner = (const uint8_t *)(uintptr_t)btPacketPoolConfig.reserved;

	return owner != NULL ? owner[8] : 0;
}

/*
 * Writes the words of the table at data (a count in the first word, then the values) to the radio
 * registers from reg on, and waits until the radio shows the mask in its status register 70;
 * 0 when it also shows the error bit 0x80.
 */
int btLmpLcSyncEntry(uint16_t reg, const uint16_t *data, uint32_t mask)
{
	int result = 1;
	int count = data[0] >> 1;
	int status;
	int index;

	rfBusEnter5350();
	rfBusWriteReg16(reg, 2 * count);
	for (index = 1; index <= count; index++)
		rfBusWriteReg16(2 * index + reg - 2, data[index]);
	rfBusWriteReg16(reg, data[count + 1]);
	rfBusLeave5350();
	rfBusEnter1350();
	do
		status = rfRegRead(70);
	while ((status & mask) == 0);
	if ((status & 0x80) != 0)
		result = 0;
	rfBusLeave1350();
	return result;
}

/*
 * Brings the radio up from the tables: the registers of the level state, the list of further words,
 * and waits for the radio to report its status (0 when it does, 1 when the wait ran out).
 */
int btLmpLcApplyEntry(void)
{
	int failed = 0;
	unsigned int gated = SOC_EVENT_ENGINE_GATE & EVENT_ENGINE_RADIO;
	unsigned int state;
	unsigned int index;
	unsigned int value;
	uint16_t count;

	SOC_EVENT_ENGINE_GATE |= EVENT_ENGINE_RADIO;
	rfBusEnter1350();
	rfRegWrite(2, 4);
	rfRegWrite(2, 7);
	rfBusLeave1350();
	state = btPowerLevelState[sysRefClockProfile()];
	if (state == 7)
		for (;;)
			;
	rfBusEnter1350();
	for (index = 0; index < BT_POWER_MASKS; index = (uint8_t)(index + 1))
		rfRegWrite(btPowerMaskEntry[index], btPowerMaskContext[index][state]);
	rfBusLeave1350();
	rfRegBegin();
	value = 2 * (rfRegRead(33) >> 1);
	rfRegWrite(33, value | btPowerIndexCommand[btPowerLevelState[sysRefClockProfile()]]);
	rfRegEnd();
	rfBusEnter1350();
	for (index = 0; index < BT_POWER_WORDS; index++) {
		unsigned int reg = btPowerLevelPacket[index].reg;

		if (reg == 0xfff)
			break;
		if (reg != 0xffe)
			rfRegWrite(reg, btPowerLevelPacket[index].value);
	}
	rfBusLeave1350();
	sysDelayUs(400);
	rfBusEnter1350();
	if (btPowerValueRecord != 0) {
		rfRegWrite(72, 1);
		rfRegWrite(96, 54);
	}
	rfRegWrite(2, 16 * (rfRegRead(2) >> 4) | 6);
	count = 5000;
	while ((rfRegRead(86) & 0x3f) != 0xf) {
		count = (uint16_t)(count - 1);
		if (count == 0) {
			failed = 1;
			break;
		}
		sysDelayUs(100);
	}
	rfBusLeave1350();
	if (gated == 0)
		SOC_EVENT_ENGINE_GATE &= ~EVENT_ENGINE_RADIO;
	return failed;
}

/* Resets the radio's control registers, notes its identity words, loads the two tables and applies them. */
int btLmpLcResolveEntry(void)
{
	uint8_t saved;
	int count;

	rfBusEnter1350();
	rfRegWrite(9, rfRegRead(9) | 0xc);
	rfRegWrite(21, rfRegRead(21) & 0xbf);
	saved = rfRegRead(71);
	rfRegWrite(71, saved | 6);
	for (count = 0; count < 10; count++)
		rfRegRead(71);
	rfRegWrite(71, saved & 0xf9);
	rfRegWrite(70, rfRegRead(70) | 0x10);
	rfRegWrite(71, rfRegRead(71) & 0xf9);
	btPowerLevelContext = rfRegRead(0);
	btPowerLevelEntry = rfRegRead(1);
	rfBusLeave1350();
	if (btLmpLcSyncEntry(0x8000, (const uint16_t *)btPoolHeaderMemory, 32) == 0 ||
	    btLmpLcSyncEntry(0xc000, &btPoolFrameMemory.radioTable.length, 64) == 0)
		for (;;)
			;
	btSbcApplyContext();
	return btLmpLcApplyEntry();
}

void btLmpLcScheduleEntry(uint8_t value)
{
	btPowerValueRecord = value;
}

/* With the value record set the link record's words are given the values the radio's tables need. */
int btLmpLcNotifyEntry(void)
{
	int result = btPowerValueRecord;

	if (btPowerValueRecord != 0) {
		uint8_t *record = btLinkRecord;

		result = (int)(uintptr_t)record;
		*(uint16_t *)(record + 14) = 0x3033;
		*(uint16_t *)(record + 18) = 0x1c13;
		*(uint16_t *)(record + 24) = 0xe027;
		*(uint16_t *)(record + 42) = 0x17c;
		*(uint16_t *)(record + 44) = 0x7c;
	}
	return result;
}
