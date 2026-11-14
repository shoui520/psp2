/*
 * Lock pages, message queues and object arenas.
 *
 * A lock is one byte (vnzKernelSpinLock). Locks come from lock pages: 0x100 bytes
 * whose byte 0 is the index of the first free lock and each free lock holds
 * the index of the next one, 0xff ending the list. Pages are kept in blocks:
 * the first is the VNZ_KERNEL_MEMORY_LOCAL_RAM region's, the others 0x1000 bytes of
 * the system heap, given back when their last lock is freed.
 */
#include "api.h"

#define VNZ_KERNEL_LOCK_PAGE		0x100
#define VNZ_KERNEL_LOCK_PAGES_END	0xff	/* a page's list: no lock left */
#define VNZ_KERNEL_LOCK_BLOCK_SIZE	0x1000

/* a block of lock pages (0x20 bytes) */
typedef struct VnzKernelLockBlock {
	struct VnzKernelLockBlock *next;
	struct VnzKernelLockBlock *previous;
	uint32_t kind;
	uint32_t base;
	uint32_t size;
	uint32_t pages;
	uint8_t *freePage;		/* a page with a free lock, or NULL */
	uint32_t used;			/* locks given out */
} VnzKernelLockBlock;

#define VNZ_KERNEL_LOCK_BLOCK_REGION	1	/* the local RAM region's: never freed */
#define VNZ_KERNEL_LOCK_BLOCK_HEAP	2

VnzKernelLockPages g_vnzKernelLockPages VNZ_KERNEL_SHARED;

static volatile uint32_t *const pagesSemaphore = vnzKernelSemaphore(VNZ_KERNEL_SEMAPHORE_PAGES);

/* every lock of a page free: byte i holds i + 1 */
static void lockPageInitialize(uint32_t *page)
{
	uint32_t word = 0x04030201;
	int i;

	for (i = 0; i < VNZ_KERNEL_LOCK_PAGE / 4 - 1; i++) {
		page[i] = word;
		word += 0x04040404;
	}
	page[i] = 0x00fffefd;
}

static VnzKernelLockBlock *lockBlockCreate(uint32_t kind, uint32_t base, uint32_t size)
{
	VnzKernelLockBlock *block;
	uint32_t i;

	block = vnzKernelHeapAllocate(vnzKernelSystemHeap(), sizeof(VnzKernelLockBlock), 0x40);
	if (block == NULL)
		return NULL;
	block = (VnzKernelLockBlock *)vnzKernelPhysicalToPublic((uint32_t)block);
	block->next = NULL;
	block->previous = NULL;
	block->base = base;
	block->size = size;
	block->kind = kind;
	block->used = 0;
	block->pages = size >> 8;
	for (i = 0; i < block->pages; i++)
		lockPageInitialize((uint32_t *)(base + (i << 8)));
	block->freePage = (uint8_t *)base;
	return block;
}

static void lockBlockAppend(VnzKernelLockBlock *block)
{
	if (g_vnzKernelLockPages.head == NULL) {
		g_vnzKernelLockPages.head = block;
	} else {
		g_vnzKernelLockPages.tail->next = block;
		block->previous = g_vnzKernelLockPages.tail;
	}
	g_vnzKernelLockPages.tail = block;
}

static void lockBlockRemove(VnzKernelLockBlock *block)
{
	if (block->previous == NULL) {
		if (block->next == NULL) {
			g_vnzKernelLockPages.head = NULL;
			g_vnzKernelLockPages.tail = NULL;
			return;
		}
		block->next->previous = NULL;
		g_vnzKernelLockPages.head = block->next;
		return;
	}
	if (block->next == NULL) {
		block->previous->next = NULL;
		g_vnzKernelLockPages.tail = block->previous;
		return;
	}
	block->previous->next = block->next;
	block->next->previous = block->previous;
}

static void lockBlockFree(VnzKernelLockBlock *block)
{
	vnzKernelHeapFree(vnzKernelSystemHeap(), (void *)vnzKernelPublicToPhysical((uint32_t)block),
		sizeof(VnzKernelLockBlock), 0x40);
}

static int pageHasFree(const uint8_t *page)
{
	return page[0] != VNZ_KERNEL_LOCK_PAGES_END;
}

static int blockHasFree(const VnzKernelLockBlock *block)
{
	return block->freePage != NULL;
}

static int blockUnused(const VnzKernelLockBlock *block)
{
	return block->kind == VNZ_KERNEL_LOCK_BLOCK_HEAP && block->used == 0;
}

static volatile uint8_t *lockTake(VnzKernelLockBlock *block)
{
	uint8_t *page = block->freePage, *lock, *next;
	uint32_t i;

	lock = page + page[0];
	page[0] = *lock;
	*lock = 0;
	block->used++;
	if (page[0] == VNZ_KERNEL_LOCK_PAGES_END) {
		/* the page is full: look for another */
		block->freePage = NULL;
		next = page + VNZ_KERNEL_LOCK_PAGE;
		for (i = 0; i < block->pages - 1; i++, next += VNZ_KERNEL_LOCK_PAGE) {
			if ((uint32_t)next >= block->base + (block->pages << 8))
				next = (uint8_t *)block->base;
			if (pageHasFree(next)) {
				block->freePage = next;
				break;
			}
		}
	}
	return lock;
}

static void lockGive(VnzKernelLockBlock *block, volatile uint8_t *lock)
{
	uint8_t *page = (uint8_t *)((uint32_t)lock & ~(VNZ_KERNEL_LOCK_PAGE - 1));

	block->used--;
	*lock = page[0];
	page[0] = (uint32_t)lock & (VNZ_KERNEL_LOCK_PAGE - 1);
	if (!blockHasFree(block))
		block->freePage = page;
}

void vnzKernelLockPagesInitialize(uint32_t primary)
{
	uint32_t base, size, start;
	VnzKernelLockBlock *block;

	if (!primary)
		return;
	vnzKernelGiveSemaphore(pagesSemaphore);
	g_vnzKernelLockPages.head = NULL;
	g_vnzKernelLockPages.tail = NULL;
	base = vnzKernelMemoryRegion(VNZ_KERNEL_MEMORY_LOCAL_RAM, &size);
	if (base == 0)
		return;
	start = (base + 0xff) & ~0xff;
	if (size < 0x100)
		return;
	size = (size - (start - base)) & ~0xff;
	if (size == 0)
		return;
	block = lockBlockCreate(VNZ_KERNEL_LOCK_BLOCK_REGION, start, size);
	if (block)
		lockBlockAppend(block);
}

/* A lock, free. Returns NULL when there is no memory for another page. */
volatile uint8_t *vnzKernelLockPageAllocate(void)
{
	VnzKernelLockBlock *block;
	volatile uint8_t *lock = NULL;
	void *pages;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(pagesSemaphore);
	for (block = g_vnzKernelLockPages.head; block; block = block->next) {
		if (blockHasFree(block)) {
			lock = lockTake(block);
			break;
		}
	}
	if (lock == NULL) {
		pages = vnzKernelHeapAllocate(vnzKernelSystemHeap(), VNZ_KERNEL_LOCK_BLOCK_SIZE,
			0x100);
		if (pages == NULL)
			goto out;
		block = lockBlockCreate(VNZ_KERNEL_LOCK_BLOCK_HEAP,
			vnzKernelPhysicalToPublic((uint32_t)pages),
			VNZ_KERNEL_LOCK_BLOCK_SIZE);
		if (block == NULL)
			goto out;
		lockBlockAppend(block);
		lock = lockTake(block);
	}
out:
	vnzKernelGiveSemaphore(pagesSemaphore);
	vnzKernelEnableDispatch();
	return lock;
}

void vnzKernelLockPageFree(volatile uint8_t *lock)
{
	VnzKernelLockBlock *block;
	uint32_t base;

	vnzKernelDisableDispatch();
	vnzKernelTakeSemaphore(pagesSemaphore);
	for (block = g_vnzKernelLockPages.head; block; block = block->next) {
		if ((uint32_t)lock < block->base || (uint32_t)lock >= block->base + block->size)
			continue;
		lockGive(block, lock);
		if (blockUnused(block)) {
			base = block->base;
			lockBlockRemove(block);
			lockBlockFree(block);
			vnzKernelHeapFree(vnzKernelSystemHeap(),
				(void *)vnzKernelPublicToPhysical(base),
				VNZ_KERNEL_LOCK_BLOCK_SIZE, 0x100);
		}
		break;
	}
	vnzKernelGiveSemaphore(pagesSemaphore);
	vnzKernelEnableDispatch();
}

/*
 * Message queues: a ring of (message, argument) pairs one thread receives,
 * told of each by its event flags.
 */
VnzKernelMessageQueue *vnzKernelMessageQueueCreate(VnzKernelThread *thread, uint32_t flags,
	uint32_t count)
{
	VnzKernelHeap *heap = vnzKernelThreadHeap(thread);
	VnzKernelMessageQueue *queue;
	void *entries;

	queue = (VnzKernelMessageQueue *)vnzKernelPhysicalToPublic((uint32_t)vnzKernelHeapAllocate(heap,
		sizeof(VnzKernelMessageQueue), 0x40));
	if (queue == NULL)
		return NULL;
	entries = vnzKernelHeapAllocate(vnzKernelThreadHeap(thread),
		count * sizeof(VnzKernelMessageEntry), 0x40);
	queue->entries = (VnzKernelMessageEntry *)vnzKernelPhysicalToPublic((uint32_t)entries);
	if (queue->entries == NULL)
		goto freeQueue;
	queue->lock = vnzLockAllocate();
	if (queue->lock == NULL)
		goto freeEntries;
	queue->thread = thread;
	queue->read = 0;
	queue->write = 0;
	queue->mask = count - 1;
	queue->flags = flags;
	queue->magic = VNZ_KERNEL_MESSAGE_QUEUE_MAGIC;
	return queue;

freeEntries:
	vnzKernelHeapFree(vnzKernelThreadHeap(thread),
		(void *)vnzKernelPublicToPhysical((uint32_t)queue->entries),
		count * sizeof(VnzKernelMessageEntry), 0x40);
freeQueue:
	vnzKernelHeapFree(vnzKernelThreadHeap(thread),
		(void *)vnzKernelPublicToPhysical((uint32_t)queue),
		sizeof(VnzKernelMessageQueue), 0x40);
	return NULL;
}

void vnzKernelMessageQueueDelete(VnzKernelMessageQueue *queue)
{
	uint32_t size = (queue->mask + 1) * sizeof(VnzKernelMessageEntry);
	VnzKernelMessageEntry *entries = queue->entries;

	queue->magic = 0;
	vnzLockFree(queue->lock);
	vnzKernelHeapFree(vnzKernelThreadHeap(queue->thread),
		(void *)vnzKernelPublicToPhysical((uint32_t)entries),
		size, 0x40);
	vnzKernelHeapFree(vnzKernelThreadHeap(queue->thread),
		(void *)vnzKernelPublicToPhysical((uint32_t)queue),
		sizeof(VnzKernelMessageQueue), 0x40);
}

/*
 * The queue's indexes are read through the cache and written back before the
 * lock is given.
 */
static inline void queueRelease(VnzKernelMessageQueue *cached)
{
	VnzKernelMessageQueue *queue;

	vnzCacheWriteback((uint32_t)cached, 0x40);
	queue = (VnzKernelMessageQueue *)vnzKernelPhysicalToPublic((uint32_t)cached);
	vnzSpinUnlock(queue->lock);
	vnzEnableDispatch();
}

/* Send a message. Returns 1, or 0 when the queue is full. */
uint32_t vnzKernelMessageSend(VnzKernelMessageQueue *queue, VnzKernelMessage *message,
	uint32_t argument)
{
	VnzKernelMessageQueue *cached;
	VnzKernelMessageEntry *entry;
	VnzKernelThread *thread;
	uint32_t write, flags;

	vnzDisableDispatch();
	vnzSpinLock(queue->lock);
	cached = (VnzKernelMessageQueue *)vnzKernelPublicToPhysical((uint32_t)queue);
	write = cached->write;
	if (cached->mask < write - cached->read) {
		queueRelease(cached);
		return 0;
	}
	entry = &cached->entries[write & cached->mask];
	entry->argument = argument;
	entry->message = message;
	thread = cached->thread;
	flags = cached->flags;
	vnzCacheWriteback((uint32_t)cached, 0x40);
	queue = (VnzKernelMessageQueue *)vnzKernelPhysicalToPublic((uint32_t)cached);
	queue->write = write + 1;
	if (flags)
		vnzThreadSetFlags(thread, flags);
	vnzSpinUnlock(queue->lock);
	vnzEnableDispatch();
	return 1;
}

/* Receive a message: its function is called. Returns 1, or 0 when there was none. */
uint32_t vnzKernelMessageReceive(VnzKernelMessageQueue *queue, uint32_t argument)
{
	VnzKernelMessageQueue *cached;
	VnzKernelMessageEntry *entry;
	VnzKernelMessage *message;
	uint32_t read, messageArgument;

	vnzDisableDispatch();
	vnzSpinLock(queue->lock);
	cached = (VnzKernelMessageQueue *)vnzKernelPublicToPhysical((uint32_t)queue);
	read = cached->read;
	if ((int32_t)(cached->write - read) < 1) {
		queueRelease(cached);
		return 0;
	}
	entry = &cached->entries[read & cached->mask];
	messageArgument = entry->argument;
	message = entry->message;
	vnzCacheWriteback((uint32_t)cached, 0x40);
	queue = (VnzKernelMessageQueue *)vnzKernelPhysicalToPublic((uint32_t)cached);
	queue->read = read + 1;
	vnzSpinUnlock(queue->lock);
	vnzEnableDispatch();
	message(argument, messageArgument);
	return 1;
}

/*
 * Object arenas: blocks of blockSize bytes in a ring. Objects are taken from
 * the current block in turn; each ends with a pointer to its block, whose
 * freed count grows as they are freed. A block whose objects are all freed is
 * used again.
 */
#define VNZ_KERNEL_ARENA_BLOCK_HEADER	0x40

static uint32_t objectSize(uint32_t size)
{
	return (((size + 3) >> 6) + 1) << 6;
}

static VnzKernelArenaBlock *arenaBlockAdd(VnzKernelArena *arena)
{
	uint32_t size = arena->blockSize;
	VnzKernelArenaBlock *block, *current;

	block = vnzKernelAllocatePublic(size);
	if (block == NULL)
		return NULL;
	block->lock = vnzKernelLockPageAllocate();
	if (block->lock == NULL) {
		vnzKernelFreePublic(block, size);
		return NULL;
	}
	block->used = VNZ_KERNEL_ARENA_BLOCK_HEADER;
	block->freed = VNZ_KERNEL_ARENA_BLOCK_HEADER;
	current = arena->current;
	if (current == NULL) {
		block->next = block;
		block->previous = block;
	} else {
		block->previous = current;
		block->next = current->next;
		current->next = block;
		block->next->previous = block;
	}
	arena->current = block;
	arena->count++;
	return block;
}

/* Take a block out of the ring and free it; returns the block after it, or NULL. */
static VnzKernelArenaBlock *arenaBlockRemove(VnzKernelArena *arena, VnzKernelArenaBlock *block)
{
	VnzKernelArenaBlock *next;
	uint32_t size;

	arena->count--;
	if (block->next == block) {
		next = NULL;
		arena->current = NULL;
	} else {
		block->next->previous = block->previous;
		block->previous->next = block->next;
		next = block->next;
		if (arena->current == block)
			arena->current = next;
	}
	size = arena->blockSize;
	vnzKernelLockPageFree(block->lock);
	vnzKernelFreePublic(block, size);
	return next;
}

void vnzKernelArenaDelete(VnzKernelArena *arena)
{
	VnzKernelArenaBlock *block = arena->current;

	if (block && block->next) {
		block = block->next;
		while ((block = arenaBlockRemove(arena, block)) != NULL)
			;
	}
	vnzKernelFreePublic(arena, sizeof(VnzKernelArena));
}

/* An arena of minimum to maximum blocks of blockSize bytes; the minimum are made now. */
VnzKernelArena *vnzKernelArenaCreate(uint32_t blockSize, uint32_t minimum, uint32_t maximum)
{
	VnzKernelArena *arena;

	if (minimum == 0)
		minimum = 1;
	if (maximum < minimum)
		maximum = minimum;
	arena = vnzKernelAllocatePublic(sizeof(VnzKernelArena));
	if (arena == NULL)
		return NULL;
	arena->blockSize = blockSize;
	arena->minimum = minimum;
	arena->maximum = maximum;
	arena->current = NULL;
	arena->count = 0;
	while (arena->count < minimum && arenaBlockAdd(arena))
		;
	if (arena->count < minimum) {
		vnzKernelArenaDelete(arena);
		return NULL;
	}
	return arena;
}

/* Make blocks until there are minimum. Returns the arena, or NULL (and deletes it) when out of memory. */
VnzKernelArena *vnzKernelArenaSetMinimum(VnzKernelArena *arena, uint32_t minimum)
{
	if (minimum == 0)
		minimum = 1;
	if (arena->maximum < minimum)
		arena->maximum = minimum;
	while (arena->count < minimum && arenaBlockAdd(arena))
		;
	if (arena->count < minimum) {
		vnzKernelArenaDelete(arena);
		return NULL;
	}
	arena->minimum = minimum;
	return arena;
}

/* Free unused blocks while there are more than low, and while more than high. */
static uint32_t arenaTrim(VnzKernelArena *arena, uint32_t low, uint32_t high, uint32_t count)
{
	VnzKernelArenaBlock *current = arena->current, *block = current->next;

	while (block != current && low < count) {
		if (block->freed == block->used) {
			block = arenaBlockRemove(arena, block);
			count--;
			continue;
		}
		if (!(high < count))
			break;
		block = block->next;
	}
	return count;
}

void vnzKernelArenaSetMaximum(VnzKernelArena *arena, uint32_t maximum)
{
	if (maximum < arena->minimum)
		maximum = arena->minimum;
	arena->maximum = maximum;
	if (maximum < arena->count)
		arena->count = arenaTrim(arena, maximum, maximum, arena->count);
}

void vnzKernelArenaTrim(VnzKernelArena *arena)
{
	arena->count = arenaTrim(arena, arena->minimum, arena->maximum, arena->count);
}

static void arenaBlockReset(VnzKernelArenaBlock *block)
{
	block->used = VNZ_KERNEL_ARENA_BLOCK_HEADER;
	block->freed = VNZ_KERNEL_ARENA_BLOCK_HEADER;
}

/* Take size bytes; NULL when no block has room. */
void *vnzKernelArenaAllocate(VnzKernelArena *arena, uint32_t size)
{
	VnzKernelArenaBlock *current = arena->current, *next, *block;
	uint8_t *object;

	if (arena->blockSize < current->used + size + 4) {
		if (arena->blockSize < size + VNZ_KERNEL_ARENA_BLOCK_HEADER + 4)
			return NULL;
		next = current->next;
		if (next->freed == next->used) {
			arenaBlockReset(next);
			current = next;
			arena->current = current;
		} else if (arena->count < arena->maximum) {
			current = arenaBlockAdd(arena);
			if (current == NULL)
				return NULL;
		} else {
			/* a free block further on goes after the current one */
			for (block = next->next; block != current; block = block->next)
				if (block->freed == block->used)
					break;
			if (block == current)
				return NULL;
			if (block->next == block) {
				arena->current = NULL;
			} else {
				block->next->previous = block->previous;
				block->previous->next = block->next;
				if (arena->current == block)
					arena->current = block->next;
			}
			arenaBlockReset(block);
			if (arena->current == NULL) {
				block->next = block;
				block->previous = block;
			} else {
				block->previous = arena->current;
				block->next = arena->current->next;
				arena->current->next = block;
				block->next->previous = block;
			}
			current = block;
			arena->current = current;
		}
	}
	object = (uint8_t *)current + current->used;
	*(VnzKernelArenaBlock **)(object + size) = current;
	current->used += objectSize(size);
	return object;
}

void vnzKernelArenaFree(void *object, uint32_t size)
{
	VnzKernelArenaBlock *block = *(VnzKernelArenaBlock **)((uint8_t *)object + size);
	volatile uint8_t *lock = block->lock;

	vnzKernelDisableDispatch();
	vnzKernelSpinLock(lock);
	block->freed += objectSize(size);
	vnzKernelSpinUnlock(lock);
	vnzKernelEnableDispatch();
}
