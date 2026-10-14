/*
 * Free list of the system timer slots (see sys.c), guarded by a semaphore.
 * The filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include "robin/osa.h"
#include "robin/sync.h"
#include "robin/sys.h"

#define SYS_TIMER_SLOT_NODES	5

/* A node of the doubly linked free list; the sentinels carry no slot. */
struct SysTimerSlotNode {
	uint32_t reserved;
	uint32_t index;
	struct SysTimerSlotNode *prev;
	struct SysTimerSlotNode *next;
};

/* Nodes 0..4 are slots, followed by the head and tail sentinels. */
static struct SysTimerSlotNode sysTimerSlotList[SYS_TIMER_SLOT_NODES + 2]
	__attribute__((section(".itcm.bss")));
static TX_SEMAPHORE sysTimerSlotLockStorage __attribute__((section(".itcm.bss")));
static struct OsaSemaphore *sysTimerSlotLock __attribute__((section(".itcm.rw")));

#define SYS_TIMER_SLOT_HEAD	(&sysTimerSlotList[SYS_TIMER_SLOT_NODES])
#define SYS_TIMER_SLOT_TAIL	(&sysTimerSlotList[SYS_TIMER_SLOT_NODES + 1])

/* Init table entry. */
int sysTimerSlotInit(void)
{
	unsigned int i;
	struct SysTimerSlotNode *node;

	sysTimerSlotList[0].prev = SYS_TIMER_SLOT_HEAD;
	sysTimerSlotList[0].next = &sysTimerSlotList[1];
	for (i = 1; i < 4; i++) {
		node = &sysTimerSlotList[i];
		node->prev = node - 1;
		node->index = i;
		node->next = node + 1;
	}
	sysTimerSlotList[4].index = 4;
	sysTimerSlotList[4].prev = &sysTimerSlotList[3];
	sysTimerSlotList[4].next = SYS_TIMER_SLOT_TAIL;
	SYS_TIMER_SLOT_HEAD->prev = NULL;
	SYS_TIMER_SLOT_HEAD->next = &sysTimerSlotList[0];
	SYS_TIMER_SLOT_TAIL->prev = &sysTimerSlotList[4];
	SYS_TIMER_SLOT_TAIL->next = NULL;
	sysTimerSlotLock = (struct OsaSemaphore *)&sysTimerSlotLockStorage;
	osaSemaphoreInit(&sysTimerSlotLock, 1, 12);
	return 0;
}

/* Takes the first free slot and registers its callback; NULL when none is free. */
struct SysTimerSlotNode *sysTimerSlotAcquire(SysTimerCallback callback, uint32_t arg)
{
	struct SysTimerSlotNode *node;

	osaSemaphoreWait(sysTimerSlotLock, 0xffffffff);
	node = SYS_TIMER_SLOT_HEAD->next;
	if (node == SYS_TIMER_SLOT_TAIL) {
		osaSemaphoreRelease(sysTimerSlotLock);
		return NULL;
	}
	SYS_TIMER_SLOT_HEAD->next = node->next;
	SYS_TIMER_SLOT_HEAD->next->prev = SYS_TIMER_SLOT_HEAD;
	node->prev = NULL;
	node->next = NULL;
	osaSemaphoreRelease(sysTimerSlotLock);
	sysTimerRegister(node->index, callback, arg);
	return node;
}
