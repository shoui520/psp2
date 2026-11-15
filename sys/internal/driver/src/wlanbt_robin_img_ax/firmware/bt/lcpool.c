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

int btLmpLcRecordContext(void)
{
	__aeabi_memclr(btPoolHeaderMemory, sizeof(btPoolHeaderMemory));
	__aeabi_memclr(btPoolFrameMemory, sizeof(btPoolFrameMemory));
	osa_BlockPoolCreate(&btTablePool, &btTablePoolConfig, btTablePoolControl, btPoolHeaderMemory, osaPoolEntryCreate);
	osa_BlockPoolCreate(&btPacketPool, &btPacketPoolConfig, btPacketPoolControl, btPoolDescriptorMemory, osaPoolEntryCreate);
	return osa_BlockPoolCreate(&btEventPool, &btEventPoolConfig, btEventPoolControl, btPoolFrameMemory, osaPoolEntryCreate);
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
	    btLmpLcSyncEntry(0xc000, (const uint16_t *)btPoolFrameMemory, 64) == 0)
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
