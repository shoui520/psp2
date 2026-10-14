/*
 * OSA workspace set-up and the table of memory pools. The filename is
 * inferred; no source filename survives.
 */
#include <stddef.h>
#include "robin/osa.h"
#include "robin/rom.h"

/* Name of the system pool; the anonymous pools use the same empty string. */
extern struct OsaWorkspace osaWorkspace;

/* Memory the system byte pool is created over, filled with a guard pattern. */
static uint8_t osaSystemPoolArea[OSA_SYSTEM_POOL_SIZE] __attribute__((section(".dtcm.bss")));

/*
 * Creates the workspace: the system byte pool, the table lock and the free
 * list of pool entries. Returns 0 when the workspace already exists.
 */
int osaInit(void)
{
	struct OsaWorkspace *workspace = NULL;
	unsigned int i;

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
	if (osaCreateSemaphore((struct OsaSemaphore *)&workspace->lock, 1) != 0)
		for (;;)
			;
	workspace->usedList = NULL;
	workspace->freeList = &workspace->pools[0];
	for (i = 0; i < OSA_POOL_COUNT; i++)
		workspace->pools[i].next = &workspace->pools[i + 1];
	workspace->pools[OSA_POOL_COUNT - 1].next = NULL;
	workspace->poolCount = 0;
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
	struct OsaPoolEntry *entry;
	int status;

	osaGetWorkspace(&workspace);
	if (workspace->poolCount == OSA_POOL_COUNT)
		return OSA_POOL_TABLE_FULL;
	osaWaitSemaphore((struct OsaSemaphore *)&workspace->lock, 0xffffffff);
	entry = workspace->freeList;
	if (entry != NULL) {
		workspace->freeList = entry->next;
		entry->next = NULL;
	}
	entry->prev = NULL;
	if (workspace->usedList == NULL) {
		workspace->usedList = entry;
		entry->next = NULL;
	} else {
		workspace->usedList->prev = entry;
		entry->next = workspace->usedList;
		workspace->usedList = entry;
	}
	workspace->poolCount++;
	osaReleaseSemaphore((struct OsaSemaphore *)&workspace->lock);
	status = osaCreatePool(entry, type, start, size, blockSize);
	if (status != 0) {
		osaWaitSemaphore((struct OsaSemaphore *)&workspace->lock, 0xffffffff);
		if (entry->prev != NULL)
			entry->prev->next = entry->next;
		else
			workspace->usedList = entry->next;
		if (entry->next != NULL)
			entry->next->prev = entry->prev;
		entry->prev = NULL;
		entry->next = NULL;
		workspace->poolCount--;
		entry->next = workspace->freeList;
		workspace->freeList = entry;
		osaReleaseSemaphore((struct OsaSemaphore *)&workspace->lock);
		return status;
	}
	entry->type = type;
	entry->start = start;
	entry->size = size;
	entry->blockSize = blockSize;
	*handle = entry;
	return status;
}

/* Deletes a pool made by osaCreatePoolEntry() and returns its entry to the free list. */
int osaDeletePoolEntry(struct OsaPoolEntry *entry)
{
	struct OsaWorkspace *workspace;
	int status;

	osaGetWorkspace(&workspace);
	if (entry == NULL)
		for (;;)
			;
	status = osaDeletePool(entry, entry->type);
	if (status == 0) {
		osaWaitSemaphore((struct OsaSemaphore *)&workspace->lock, 0xffffffff);
		if (entry->prev != NULL)
			entry->prev->next = entry->next;
		else
			workspace->usedList = entry->next;
		if (entry->next != NULL)
			entry->next->prev = entry->prev;
		entry->prev = NULL;
		entry->next = NULL;
		workspace->poolCount--;
		entry->next = NULL;
		entry->next = workspace->freeList;
		workspace->freeList = entry;
		osaReleaseSemaphore((struct OsaSemaphore *)&workspace->lock);
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
