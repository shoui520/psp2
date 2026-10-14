#pragma once

/* name unknown: intrusive linkage owned by the mailbox. */
typedef struct BbmcQueueEntry {
	struct BbmcQueueEntry *next;
} BbmcQueueEntry;
