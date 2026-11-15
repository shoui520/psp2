/*
 * The Bluetooth controller's buffer pools (0x43724 - 0x4381x): the SQRAM memory is cleared and the three
 * pools of the controller (tables, packets, events) are created over it, and the allocators of its packet
 * and event buffers. The file name and roles are inferred; the function names are the firmware's
 * placeholders.
 */
#include <stddef.h>
#include "robin/btpool.h"
#include "robin/btrom.h"
#include "robin/rom.h"
#include "robin/sys.h"

/* Where a buffer's data begins: after the allocator's header and the controller's own. */
#define POOL_DATA_OFFSET	40

int btLmpLcRecordContext(void)
{
	__aeabi_memclr(btPoolHeaderMemory, sizeof(btPoolHeaderMemory));
	__aeabi_memclr(btPoolFrameMemory, sizeof(btPoolFrameMemory));
	osa_BlockPoolCreate(&btTablePool, btTablePoolConfig, btTablePoolControl, btPoolHeaderMemory);
	osa_BlockPoolCreate(&btPacketPool, btPacketPoolConfig, btPacketPoolControl, btPoolDescriptorMemory);
	return osa_BlockPoolCreate(&btEventPool, btEventPoolConfig, btEventPoolControl, btPoolFrameMemory);
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

int btLmpLcProcessEntry(void)
{
	return btLimitParam();
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
