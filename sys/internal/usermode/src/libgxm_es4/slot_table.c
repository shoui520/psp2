/*
 * SceGxm: the slot table of contexts, and waiting on a GPU-written word.
 *
 * Sony's debug build keeps this code in a file of its own between mte.c and
 * notification.c (its own string pool), whose name no record gives.
 */

#include "gxm_internal.h"
#include "memory_util_private.h"

#define GXM_SLOT_RECORD_SIZE	0x10

int gxmSlotTableCreate(GxmSlotTable *table)
{
	PVRSRV_CLIENT_MEM_INFO *block;
	int err;

	err = gxmAllocGpuBlock(gxmGlobals.top.slotHeap, 0x40000f, 0x400, 4, &block);
	if (err != 0)
		return err;
	table->used[0] = 0;
	table->used[1] = 0;
	table->block = block;
	return 0;
}

void *gxmSlotTableAdd(GxmSlotTable *table, void *item)
{
	SceUInt32 slot, bit;

	for (slot = 0; slot < 64; slot++) {
		bit = 1 << (slot & 31);
		if (!(table->used[slot >> 5] & bit)) {
			table->used[slot >> 5] |= bit;
			table->items[slot] = item;
			return (char *)table->block->pvLinAddr + slot * GXM_SLOT_RECORD_SIZE;
		}
	}
	return NULL;
}

void gxmSlotTableRemove(GxmSlotTable *table, void *record)
{
	SceUInt32 position = (SceUInt32)record - (SceUInt32)table->block->pvLinAddr;
	SceUInt32 slot = position >> 4;

	if (slot > 63)
		return;
	table->used[position >> 9] &= ~(1 << (slot & 31));
}

void gxmSlotTableForEach(GxmSlotTable *table, void (*callback)(void *item))
{
	SceUInt32 slot;

	for (slot = 0; slot < 64; slot++) {
		if (table->used[slot >> 5] & (1 << (slot & 31)))
			callback(table->items[slot]);
	}
}

int gxmSlotTableDestroy(GxmSlotTable *table)
{
	return gxmFreeGpuBlock(table->block);
}

/* sleep on the GPU signal until the word holds the value */
void gxmWaitForValue(const PVRSRV_CLIENT_MEM_INFO *block, const volatile SceUInt32 *address,
		     SceUInt32 value)
{
	while (*address != value) {
		sceKernelGetTLSAddr(0x44);
		sceGpuSignalWait(sceKernelGetTLSAddr(0x44), 100000);
	}
}
