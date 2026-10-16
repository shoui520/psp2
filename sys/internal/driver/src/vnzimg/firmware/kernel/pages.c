/*
 * Lock pages, message queues and object arenas.
 *
 * A lock is one byte (vkSpinLock). Locks come from lock pages: 0x100 bytes
 * whose byte 0 is the index of the first free lock and each free lock holds
 * the index of the next one, 0xff ending the list. Pages are kept in blocks:
 * the first is the VK_MEMORY_LOCAL_RAM region's, the others 0x1000 bytes of
 * the system heap, given back when their last lock is freed.
 */
#include "kernel.h"

#define VK_LOCK_PAGE		0x100
#define VK_LOCK_PAGES_END	0xff	/* a page's list: no lock left */
#define VK_LOCK_BLOCK_SIZE	0x1000

/* a block of lock pages (0x20 bytes) */
typedef struct VkLockBlock {
	struct VkLockBlock *next;
	struct VkLockBlock *previous;
	uint32_t kind;
	uint32_t base;
	uint32_t size;
	uint32_t pages;
	uint8_t *freePage;		/* a page with a free lock, or NULL */
	uint32_t used;			/* locks given out */
} VkLockBlock;

#define VK_LOCK_BLOCK_REGION	1	/* the local RAM region's: never freed */
#define VK_LOCK_BLOCK_HEAP	2

VkLockPages g_vkLockPages VK_SHARED;

static volatile uint32_t *const pagesSemaphore = vkSemaphore(VK_SEMAPHORE_PAGES);

/* every lock of a page free: byte i holds i + 1 */
static void lockPageInitialize(uint32_t *page)
{
	uint32_t word = 0x04030201;
	int i;

	for (i = 0; i < VK_LOCK_PAGE / 4 - 1; i++) {
		page[i] = word;
		word += 0x04040404;
	}
	page[i] = 0x00fffefd;
}

static VkLockBlock *lockBlockCreate(uint32_t kind, uint32_t base, uint32_t size)
{
	VkLockBlock *block;
	uint32_t i;

	block = vkHeapAllocate(vkSystemHeap(), sizeof(VkLockBlock), 0x40);
	if (block == NULL)
		return NULL;
	block = (VkLockBlock *)vkPhysicalToPublic((uint32_t)block);
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

static void lockBlockAppend(VkLockBlock *block)
{
	if (g_vkLockPages.head == NULL) {
		g_vkLockPages.head = block;
	} else {
		g_vkLockPages.tail->next = block;
		block->previous = g_vkLockPages.tail;
	}
	g_vkLockPages.tail = block;
}

static void lockBlockRemove(VkLockBlock *block)
{
	if (block->previous == NULL) {
		if (block->next == NULL) {
			g_vkLockPages.head = NULL;
			g_vkLockPages.tail = NULL;
			return;
		}
		block->next->previous = NULL;
		g_vkLockPages.head = block->next;
		return;
	}
	if (block->next == NULL) {
		block->previous->next = NULL;
		g_vkLockPages.tail = block->previous;
		return;
	}
	block->previous->next = block->next;
	block->next->previous = block->previous;
}

static void lockBlockFree(VkLockBlock *block)
{
	vkHeapFree(vkSystemHeap(), (void *)vkPublicToPhysical((uint32_t)block), sizeof(VkLockBlock), 0x40);
}

static int pageHasFree(const uint8_t *page)
{
	return page[0] != VK_LOCK_PAGES_END;
}

static int blockHasFree(const VkLockBlock *block)
{
	return block->freePage != NULL;
}

static int blockUnused(const VkLockBlock *block)
{
	return block->kind == VK_LOCK_BLOCK_HEAP && block->used == 0;
}

static volatile uint8_t *lockTake(VkLockBlock *block)
{
	uint8_t *page = block->freePage, *lock, *next;
	uint32_t i;

	lock = page + page[0];
	page[0] = *lock;
	*lock = 0;
	block->used++;
	if (page[0] == VK_LOCK_PAGES_END) {
		/* the page is full: look for another */
		block->freePage = NULL;
		next = page + VK_LOCK_PAGE;
		for (i = 0; i < block->pages - 1; i++, next += VK_LOCK_PAGE) {
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

static void lockGive(VkLockBlock *block, volatile uint8_t *lock)
{
	uint8_t *page = (uint8_t *)((uint32_t)lock & ~(VK_LOCK_PAGE - 1));

	block->used--;
	*lock = page[0];
	page[0] = (uint32_t)lock & (VK_LOCK_PAGE - 1);
	if (!blockHasFree(block))
		block->freePage = page;
}

void vkLockPagesInitialize(uint32_t primary)
{
	uint32_t base, size, start;
	VkLockBlock *block;

	if (!primary)
		return;
	vkGiveSemaphore(pagesSemaphore);
	g_vkLockPages.head = NULL;
	g_vkLockPages.tail = NULL;
	base = vkMemoryRegion(VK_MEMORY_LOCAL_RAM, &size);
	if (base == 0)
		return;
	start = (base + 0xff) & ~0xff;
	if (size < 0x100)
		return;
	size = (size - (start - base)) & ~0xff;
	if (size == 0)
		return;
	block = lockBlockCreate(VK_LOCK_BLOCK_REGION, start, size);
	if (block)
		lockBlockAppend(block);
}

/* A lock, free. Returns NULL when there is no memory for another page. */
volatile uint8_t *vkLockPageAllocate(void)
{
	VkLockBlock *block;
	volatile uint8_t *lock = NULL;
	void *pages;

	vkDisableDispatch();
	vkTakeSemaphore(pagesSemaphore);
	for (block = g_vkLockPages.head; block; block = block->next) {
		if (blockHasFree(block)) {
			lock = lockTake(block);
			break;
		}
	}
	if (lock == NULL) {
		pages = vkHeapAllocate(vkSystemHeap(), VK_LOCK_BLOCK_SIZE, 0x100);
		if (pages == NULL)
			goto out;
		block = lockBlockCreate(VK_LOCK_BLOCK_HEAP, vkPhysicalToPublic((uint32_t)pages),
			VK_LOCK_BLOCK_SIZE);
		if (block == NULL)
			goto out;
		lockBlockAppend(block);
		lock = lockTake(block);
	}
out:
	vkGiveSemaphore(pagesSemaphore);
	vkEnableDispatch();
	return lock;
}

void vkLockPageFree(volatile uint8_t *lock)
{
	VkLockBlock *block;
	uint32_t base;

	vkDisableDispatch();
	vkTakeSemaphore(pagesSemaphore);
	for (block = g_vkLockPages.head; block; block = block->next) {
		if ((uint32_t)lock < block->base || (uint32_t)lock >= block->base + block->size)
			continue;
		lockGive(block, lock);
		if (blockUnused(block)) {
			base = block->base;
			lockBlockRemove(block);
			lockBlockFree(block);
			vkHeapFree(vkSystemHeap(), (void *)vkPublicToPhysical(base),
				VK_LOCK_BLOCK_SIZE, 0x100);
		}
		break;
	}
	vkGiveSemaphore(pagesSemaphore);
	vkEnableDispatch();
}

uint32_t vkLockPageTry(volatile uint8_t *lock)
{
	return vkTestAndSet(lock);
}

void vkLockPageUnlock(volatile uint8_t *lock)
{
	vkSpinUnlock(lock);
}

void vkLockPageLock(volatile uint8_t *lock)
{
	vkSpinLock(lock);
}

/*
 * Message queues: a ring of (message, argument) pairs one thread receives,
 * told of each by its event flags.
 */
VkMessageQueue *vkMessageQueueCreate(VkThread *thread, uint32_t flags, uint32_t count)
{
	VkHeap *heap = vkThreadHeap(thread);
	VkMessageQueue *queue;
	void *entries;

	queue = (VkMessageQueue *)vkPhysicalToPublic((uint32_t)vkHeapAllocate(heap,
		sizeof(VkMessageQueue), 0x40));
	if (queue == NULL)
		return NULL;
	entries = vkHeapAllocate(vkThreadHeap(thread), count * sizeof(VkMessageEntry), 0x40);
	queue->entries = (VkMessageEntry *)vkPhysicalToPublic((uint32_t)entries);
	if (queue->entries == NULL)
		goto freeQueue;
	queue->lock = vkLockPageAllocate();
	if (queue->lock == NULL)
		goto freeEntries;
	queue->thread = thread;
	queue->read = 0;
	queue->write = 0;
	queue->mask = count - 1;
	queue->flags = flags;
	queue->magic = VK_MESSAGE_QUEUE_MAGIC;
	return queue;

freeEntries:
	vkHeapFree(vkThreadHeap(thread), (void *)vkPublicToPhysical((uint32_t)queue->entries),
		count * sizeof(VkMessageEntry), 0x40);
freeQueue:
	vkHeapFree(vkThreadHeap(thread), (void *)vkPublicToPhysical((uint32_t)queue),
		sizeof(VkMessageQueue), 0x40);
	return NULL;
}

void vkMessageQueueDelete(VkMessageQueue *queue)
{
	uint32_t size = (queue->mask + 1) * sizeof(VkMessageEntry);
	VkMessageEntry *entries = queue->entries;

	queue->magic = 0;
	vkLockPageFree(queue->lock);
	vkHeapFree(vkThreadHeap(queue->thread), (void *)vkPublicToPhysical((uint32_t)entries),
		size, 0x40);
	vkHeapFree(vkThreadHeap(queue->thread), (void *)vkPublicToPhysical((uint32_t)queue),
		sizeof(VkMessageQueue), 0x40);
}

/*
 * The queue's indexes are read through the cache and written back before the
 * lock is given.
 */
static void queueRelease(VkMessageQueue *cached)
{
	VkMessageQueue *queue;

	vkCacheWriteback((uint32_t)cached, 0x40);
	queue = (VkMessageQueue *)vkPhysicalToPublic((uint32_t)cached);
	vkLockPageUnlock(queue->lock);
	vkEnableDispatch();
}

/* Send a message. Returns 1, or 0 when the queue is full. */
uint32_t vkMessageSend(VkMessageQueue *queue, VkMessage *message, uint32_t argument)
{
	VkMessageQueue *cached;
	VkMessageEntry *entry;
	VkThread *thread;
	uint32_t write, flags;

	vkDisableDispatch();
	vkLockPageLock(queue->lock);
	cached = (VkMessageQueue *)vkPublicToPhysical((uint32_t)queue);
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
	vkCacheWriteback((uint32_t)cached, 0x40);
	queue = (VkMessageQueue *)vkPhysicalToPublic((uint32_t)cached);
	queue->write = write + 1;
	if (flags)
		vkThreadSetFlags(thread, flags);
	vkLockPageUnlock(queue->lock);
	vkEnableDispatch();
	return 1;
}

/* Receive a message: its function is called. Returns 1, or 0 when there was none. */
uint32_t vkMessageReceive(VkMessageQueue *queue, uint32_t argument)
{
	VkMessageQueue *cached;
	VkMessageEntry *entry;
	VkMessage *message;
	uint32_t read, messageArgument;

	vkDisableDispatch();
	vkLockPageLock(queue->lock);
	cached = (VkMessageQueue *)vkPublicToPhysical((uint32_t)queue);
	read = cached->read;
	if ((int32_t)(cached->write - read) < 1) {
		queueRelease(cached);
		return 0;
	}
	entry = &cached->entries[read & cached->mask];
	messageArgument = entry->argument;
	message = entry->message;
	vkCacheWriteback((uint32_t)cached, 0x40);
	queue = (VkMessageQueue *)vkPhysicalToPublic((uint32_t)cached);
	queue->read = read + 1;
	vkLockPageUnlock(queue->lock);
	vkEnableDispatch();
	message(argument, messageArgument);
	return 1;
}

/*
 * Object arenas: blocks of blockSize bytes in a ring. Objects are taken from
 * the current block in turn; each ends with a pointer to its block, whose
 * freed count grows as they are freed. A block whose objects are all freed is
 * used again.
 */
#define VK_ARENA_BLOCK_HEADER	0x40

static uint32_t objectSize(uint32_t size)
{
	return (((size + 3) >> 6) + 1) << 6;
}

static VkArenaBlock *arenaBlockAdd(VkArena *arena)
{
	uint32_t size = arena->blockSize;
	VkArenaBlock *block, *current;

	block = vkAllocatePublic(size);
	if (block == NULL)
		return NULL;
	block->lock = vkLockPageAllocate();
	if (block->lock == NULL) {
		vkFreePublic(block, size);
		return NULL;
	}
	block->used = VK_ARENA_BLOCK_HEADER;
	block->freed = VK_ARENA_BLOCK_HEADER;
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
static VkArenaBlock *arenaBlockRemove(VkArena *arena, VkArenaBlock *block)
{
	VkArenaBlock *next;
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
	vkLockPageFree(block->lock);
	vkFreePublic(block, size);
	return next;
}

void vkArenaDelete(VkArena *arena)
{
	VkArenaBlock *block = arena->current;

	if (block && block->next) {
		block = block->next;
		while ((block = arenaBlockRemove(arena, block)) != NULL)
			;
	}
	vkFreePublic(arena, sizeof(VkArena));
}

/* An arena of minimum to maximum blocks of blockSize bytes; the minimum are made now. */
VkArena *vkArenaCreate(uint32_t blockSize, uint32_t minimum, uint32_t maximum)
{
	VkArena *arena;

	if (minimum == 0)
		minimum = 1;
	if (maximum < minimum)
		maximum = minimum;
	arena = vkAllocatePublic(sizeof(VkArena));
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
		vkArenaDelete(arena);
		return NULL;
	}
	return arena;
}

/* Make blocks until there are minimum. Returns the arena, or NULL (and deletes it) when out of memory. */
VkArena *vkArenaSetMinimum(VkArena *arena, uint32_t minimum)
{
	if (minimum == 0)
		minimum = 1;
	if (arena->maximum < minimum)
		arena->maximum = minimum;
	while (arena->count < minimum && arenaBlockAdd(arena))
		;
	if (arena->count < minimum) {
		vkArenaDelete(arena);
		return NULL;
	}
	arena->minimum = minimum;
	return arena;
}

/* Free unused blocks while there are more than low, and while more than high. */
static uint32_t arenaTrim(VkArena *arena, uint32_t low, uint32_t high, uint32_t count)
{
	VkArenaBlock *current = arena->current, *block = current->next;

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

void vkArenaSetMaximum(VkArena *arena, uint32_t maximum)
{
	if (maximum < arena->minimum)
		maximum = arena->minimum;
	arena->maximum = maximum;
	if (maximum < arena->count)
		arena->count = arenaTrim(arena, maximum, maximum, arena->count);
}

void vkArenaTrim(VkArena *arena)
{
	arena->count = arenaTrim(arena, arena->minimum, arena->maximum, arena->count);
}

static void arenaBlockReset(VkArenaBlock *block)
{
	block->used = VK_ARENA_BLOCK_HEADER;
	block->freed = VK_ARENA_BLOCK_HEADER;
}

/* Take size bytes; NULL when no block has room. */
void *vkArenaAllocate(VkArena *arena, uint32_t size)
{
	VkArenaBlock *current = arena->current, *next, *block;
	uint8_t *object;

	if (arena->blockSize < current->used + size + 4) {
		if (arena->blockSize < size + VK_ARENA_BLOCK_HEADER + 4)
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
	*(VkArenaBlock **)(object + size) = current;
	current->used += objectSize(size);
	return object;
}

void vkArenaFree(void *object, uint32_t size)
{
	VkArenaBlock *block = *(VkArenaBlock **)((uint8_t *)object + size);
	volatile uint8_t *lock = block->lock;

	vkDisableDispatch();
	vkSpinLock(lock);
	block->freed += objectSize(size);
	vkSpinUnlock(lock);
	vkEnableDispatch();
}
