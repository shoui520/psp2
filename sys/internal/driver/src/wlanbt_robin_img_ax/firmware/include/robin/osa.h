/*
 * OSA, the firmware's operating-system abstraction over the ThreadX kernel in
 * the Mask ROM: a workspace that owns a byte pool plus a table of the memory
 * pools made through it, message queues, event flags and semaphores, the idle
 * thread, and small formatting helpers.
 *
 * Return codes: OSA_SUCCESS is 0, OSA_ERROR a generic failure; the others are
 * the translations osaTranslateStatus() makes of ThreadX status codes.
 */
#ifndef ROBIN_OSA_H
#define ROBIN_OSA_H

#include <stddef.h>
#include <stdint.h>
#include "robin/threadx.h"
#include "robin/sync.h"

#define OSA_SUCCESS		0
#define OSA_ERROR		1
#define OSA_NO_MEMORY		3
#define OSA_QUEUE_EMPTY		15
#define OSA_POOL_TABLE_FULL	31

/* Pool kind recorded in the workspace; any other value is a block pool. */
#define OSA_POOL_BYTE		10

#define OSA_POOL_COUNT		34
#define OSA_SYSTEM_POOL_SIZE	500

struct OsaPoolEntry {
	/* The ThreadX pool control block, created over the caller's memory. */
	union {
		TX_BYTE_POOL byte;
		TX_BLOCK_POOL block;
	} pool;
	uint32_t type;
	void *start;
	uint32_t size;
	uint32_t blockSize;
	struct OsaPoolEntry *next;
	struct OsaPoolEntry *prev;
};

struct OsaWorkspace {
	TX_BYTE_POOL systemPool;
	uint32_t poolCount;
	TX_SEMAPHORE lock;
	struct OsaPoolEntry pools[OSA_POOL_COUNT];
	struct OsaPoolEntry *usedList;
	struct OsaPoolEntry *freeList;
};

_Static_assert(sizeof(struct OsaPoolEntry) == 76, "OSA pool entry");
_Static_assert(sizeof(struct OsaWorkspace) == 2676, "OSA workspace");
_Static_assert(offsetof(struct OsaWorkspace, pools) == 84, "OSA pool table");

/* Entries on the idle hook list, run in turn by the idle thread. */
struct OsaIdleHook {
	struct OsaIdleHook *next;
	int (*function)(void);
};

extern uint8_t osaWorkspaceReady;
extern struct OsaWorkspace *osaWorkspaceHandle;
extern struct OsaWorkspace osaWorkspace;
extern struct OsaIdleHook *osaIdleHooks;

/* osa/pool.c */
int osaInit(void);
int osaCreatePoolEntry(struct OsaPoolEntry **handle, uint32_t type, void *start,
		       uint32_t size, uint32_t blockSize);
int osaDeletePoolEntry(struct OsaPoolEntry *entry);
int osaCreateTask(TX_THREAD **handle, void *stack, ULONG stackSize, UINT priority,
		  CHAR *name, void (*entry)(ULONG), ULONG input);

/* osa/core.c */
int osaTranslateStatus(int status);
int osaGetWorkspace(struct OsaWorkspace **workspace);
void *osaClearWorkspace(void *workspace);
int osaCreateThread(TX_THREAD *thread, void *stack, ULONG stackSize,
		    UINT priority, CHAR *name, void (*entry)(ULONG), ULONG input);
int osaFreeMemory(struct OsaWorkspace *workspace, uint32_t type, void *memory);
int osaAllocateMemory(struct OsaWorkspace *workspace, uint32_t type, uint32_t size,
		      void **memory, uint32_t wait);
int osaGetEventFlagsState(TX_EVENT_FLAGS_GROUP *group, uint32_t *flags);
int osaCreatePool(void *pool, uint32_t type, void *start, uint32_t size,
		  uint32_t blockSize);
int osaDeletePool(void *pool, uint32_t type);
int osaCreateIdleThread(void);
int osaStartIdleThread(void);
int osaAddIdleHook(struct OsaIdleHook *hook);
/* Registers the ROM mdev drivers; the callees keep provisional names. */
void osaInitSubsystems(void);
void osaFormatHexByte(uint32_t value, char *output);
char osaHexDigit(uint32_t nibble);
const uint8_t *osaFindTlv(const uint8_t *data, int type, int length);
char *osaFormatVersionBanner(int selector, char *buffer);
void osaEnableArmedTimerInterrupt(void);
void osaTimerListInit(void);
unsigned int osaTimerPoolInit(void);

#endif /* ROBIN_OSA_H */
