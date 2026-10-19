/* The BBMC mailboxes: queues of messages between its threads. */

#pragma once

#include <stdint.h>
#include <scetypes.h>
#include "bbmc_queue.h"

/* name unknown: all five words are initialized and used by the queue owner. */
typedef struct BbmcMailbox {
	SceUID semaphore;
	SceKernelSpinlock lock;
	int count;
	BbmcQueueEntry *head;
	BbmcQueueEntry *tail;
} BbmcMailbox;

/* bbmc_mailbox.c */
int bbmcInitializeMailbox(void);
int bbmcFinalizeMailbox(void);
int bbmcCreateMailbox(BbmcMailbox **mailbox);
unsigned int bbmcDeleteMailbox(BbmcMailbox *mailbox);
