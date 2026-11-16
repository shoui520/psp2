/*
 * OSA workspace set-up and the table of memory pools. The filename is
 * inferred; no source filename survives.
 */
#include <stddef.h>
#include "robin/osa.h"
#include "robin/btrom.h"
#include "robin/rom.h"

/* Name of the system pool; the anonymous pools use the same empty string. */
extern struct OsaWorkspace osaWorkspace;

/* Set to 1 by osaInit() once the workspace exists. */
uint8_t osaWorkspaceReady __attribute__((section(".dtcm.rw")));	/* DTCM 0x04000168 */

/* Memory the system byte pool is created over, filled with a guard pattern. */
static uint8_t osaSystemPoolArea[OSA_SYSTEM_POOL_SIZE] __attribute__((section(".dtcm.bss")));

/*
 * Creates the workspace: the system byte pool, the table lock and the free
 * list of pool entries. Returns 0 when the workspace already exists.
 */
int osaInit(void)
{
	struct OsaWorkspace *workspace = NULL;
	struct OsaPoolTable *table;
	struct OsaPoolEntry *entry;
	int status;
	uint8_t i;

	if (osaGetWorkspace(&workspace) != 0)
		return 0;
	osaWorkspaceReady = 1;
	osaClearWorkspace(workspace);
	__aeabi_memset4(osaSystemPoolArea, OSA_SYSTEM_POOL_SIZE, 0xa5);
	if (osaCreatePool(workspace, OSA_POOL_BYTE, osaSystemPoolArea,
			  OSA_SYSTEM_POOL_SIZE, 4) != 0)
		return 1;
	if (workspace == NULL)
		for (;;)
			;
	table = &workspace->table;
	status = osaCreateSemaphore((struct OsaSemaphore *)&table->lock, 1);
	if (status != 0)
		for (;;)
			;
	table->usedList = NULL;
	entry = &table->pools[0];
	table->freeList = entry;
	for (i = 0; i < OSA_POOL_COUNT; i++) {
		if (status != 0)
			for (;;)
				;
		entry->next = entry + 1;
		entry++;
	}
	table->pools[OSA_POOL_COUNT - 1].next = NULL;
	table->poolCount = 0;
	return 0;
}

/*
 * Creates a byte or block pool and records it in the table. Returns 31 when
 * the table is full.
 */
int osaCreatePoolEntry(struct OsaPoolEntry **handle, uint32_t type, void *start,
		       uint32_t size, uint32_t blockSize)
{
	struct OsaWorkspace *workspace;
	struct OsaPoolTable *table;
	struct OsaPoolEntry *entry;
	int status;

	osaGetWorkspace(&workspace);
	table = &workspace->table;
	if (table->poolCount == OSA_POOL_COUNT)
		return OSA_POOL_TABLE_FULL;
	osaWaitSemaphore((struct OsaSemaphore *)&table->lock, 0xffffffff);
	entry = table->freeList;
	if (entry != NULL) {
		table->freeList = entry->next;
		entry->next = NULL;
	}
	entry->prev = NULL;
	if (table->usedList == NULL) {
		table->usedList = entry;
		entry->next = NULL;
	} else {
		table->usedList->prev = entry;
		entry->next = table->usedList;
		table->usedList = entry;
	}
	table->poolCount++;
	osaReleaseSemaphore((struct OsaSemaphore *)&table->lock);
	status = osaCreatePool(entry, type, start, size, blockSize);
	if (status != 0) {
		osaWaitSemaphore((struct OsaSemaphore *)&table->lock, 0xffffffff);
		if (entry->prev != NULL)
			entry->prev->next = entry->next;
		else
			table->usedList = entry->next;
		if (entry->next != NULL)
			entry->next->prev = entry->prev;
		entry->prev = NULL;
		entry->next = NULL;
		table->poolCount--;
		entry->next = NULL;
		entry->next = table->freeList;
		table->freeList = entry;
		osaReleaseSemaphore((struct OsaSemaphore *)&table->lock);
		return status;
	}
	entry->type = type;
	entry->start = start;
	entry->size = size;
	entry->blockSize = blockSize;
	*handle = entry;
	return status;
}

/* The ROM's pool creation calls this back; the image passes it to osa_BlockPoolCreate(). */
int (*osaPoolEntryCreate)(struct OsaPoolEntry **handle, uint32_t type, void *start, uint32_t size,
			  uint32_t blockSize) __attribute__((section(".itcm.rw"))) = osaCreatePoolEntry;

/* Deletes a pool made by osaCreatePoolEntry() and returns its entry to the free list. */
int osaDeletePoolEntry(struct OsaPoolEntry *entry)
{
	struct OsaWorkspace *workspace;
	struct OsaPoolTable *table;
	int status;

	osaGetWorkspace(&workspace);
	table = &workspace->table;
	if (entry == NULL)
		for (;;)
			;
	status = osaDeletePool(entry, entry->type);
	if (status == 0) {
		osaWaitSemaphore((struct OsaSemaphore *)&table->lock, 0xffffffff);
		if (entry->prev != NULL)
			entry->prev->next = entry->next;
		else
			table->usedList = entry->next;
		if (entry->next != NULL)
			entry->next->prev = entry->prev;
		entry->prev = NULL;
		entry->next = NULL;
		table->poolCount--;
		entry->next = NULL;
		entry->next = table->freeList;
		table->freeList = entry;
		osaReleaseSemaphore((struct OsaSemaphore *)&table->lock);
	}
	return status;
}

/* Creates a task over caller-supplied stack; its handle must already point to storage. */
int osaCreateTask(TX_THREAD **handle, void *stack, ULONG stackSize, UINT priority,
		  CHAR *name, void (*entry)(ULONG), ULONG input)
{
	if (handle == NULL)
		for (;;)
			;
	if (*handle == NULL)
		for (;;)
			;
	return osaCreateThread(*handle, stack, stackSize, priority, name, entry, input);
}
