#include "mlndl_internal.h"

#define MLNDL_SLAB_UNITS 256
#define MLNDL_UNIT_BYTES 16
#define MLNDL_SLAB_BYTES 4368
#define MLNDL_SMALL_LIMIT 64
#define MLNDL_SLAB_CONT 0xFF

typedef struct MlndlHeapBlock {
	struct MlndlHeapBlock *prevPhys;
	struct MlndlHeapBlock *nextPhys;
	struct MlndlHeapBlock *prevFree;
	struct MlndlHeapBlock *nextFree;
} MlndlHeapBlock;

typedef struct MlndlHeapSlab {
	struct MlndlHeapSlab *prev;
	struct MlndlHeapSlab *next;
	int32_t freeUnits;
	uint32_t reserved;
	uint8_t unitMap[MLNDL_SLAB_UNITS];
	uint8_t arena[MLNDL_SLAB_UNITS * MLNDL_UNIT_BYTES];
} MlndlHeapSlab;

typedef struct MlndlHeapPool {
	uintptr_t arenaStart;
	uintptr_t arenaEnd;
	MlndlHeapBlock *freeHead;
	MlndlHeapSlab *slabHead;
	uint8_t zeroRun[4];
} MlndlHeapPool;

static MlndlHeapPool *s_defaultPool;
static MlndlHeapPool *s_auxPool;

static void *mlndlPoolAlloc(MlndlHeapPool *pool, uint32_t size);
static void mlndlPoolFree(MlndlHeapPool *pool, void *address);

void *mlndlMemSet(void *dest, uint8_t value, uint32_t count)
{
	uint8_t *out = (uint8_t *)dest;

	while (count != 0) {
		*out++ = value;
		count--;
	}
	return dest;
}

int mlndlMemCmp(const void *left, const void *right, uint32_t count)
{
	const int8_t *lhs = (const int8_t *)left;
	const int8_t *rhs = (const int8_t *)right;

	for (uint32_t pos = 0; pos < count; pos++) {
		if (lhs[pos] != rhs[pos])
			return (lhs[pos] < rhs[pos]) ? -1 : 1;
	}
	return 0;
}

void *mlndlMemCpy(void *dest, const void *src, uint32_t count)
{
	uint8_t *out = (uint8_t *)dest;
	const uint8_t *in = (const uint8_t *)src;

	while (count != 0) {
		*out++ = *in++;
		count--;
	}
	return dest;
}

int mlndlHeapInit(int poolSelector, void *memory, uint32_t size)
{
	MlndlHeapPool **slot = (poolSelector == 0) ? &s_defaultPool : &s_auxPool;
	uintptr_t base = (uintptr_t)memory;
	MlndlHeapPool *pool;
	MlndlHeapBlock *firstBlock;
	uintptr_t endAddr;

	if (size < 0x400)
		return 2;

	pool = (MlndlHeapPool *)((base + 15) & ~(uintptr_t)15);
	mlndlMemSet(pool, 0, sizeof(MlndlHeapPool));

	firstBlock = (MlndlHeapBlock *)((base + 35) & ~(uintptr_t)15);
	endAddr = (base + size - 16) & ~(uintptr_t)15;

	pool->arenaStart = (uintptr_t)firstBlock;
	pool->arenaEnd = endAddr;
	pool->freeHead = firstBlock;
	pool->slabHead = NULL;

	firstBlock->prevPhys = NULL;
	firstBlock->nextPhys = (MlndlHeapBlock *)endAddr;
	firstBlock->prevFree = NULL;
	firstBlock->nextFree = (MlndlHeapBlock *)endAddr;

	if (*slot != NULL)
		return 1;
	*slot = pool;
	return 0;
}

static void *mlndlSlabAlloc(MlndlHeapPool *pool, MlndlHeapSlab *slab, int units)
{
	int pos;
	int limit = MLNDL_SLAB_UNITS - units;

	if (slab == NULL || slab->freeUnits < units || limit < 0)
		return NULL;

	for (pos = 0; pos <= limit; pos++) {
		uint8_t mark = slab->unitMap[pos];
		if (mark != 0) {
			if (mark != MLNDL_SLAB_CONT)
				pos += (int)mark - 1;
			continue;
		}
		if (units > 1
			&& mlndlMemCmp(&slab->unitMap[pos + 1], pool->zeroRun,
				(uint32_t)(units - 1)) != 0) {
			continue;
		}
		slab->unitMap[pos] = (uint8_t)units;
		for (int fill = 1; fill < units; fill++)
			slab->unitMap[pos + fill] = MLNDL_SLAB_CONT;
		slab->freeUnits -= units;
		return &slab->arena[pos * MLNDL_UNIT_BYTES];
	}
	return NULL;
}

static MlndlHeapSlab *mlndlSlabCreate(MlndlHeapPool *pool)
{
	MlndlHeapSlab *slab;

	slab = (MlndlHeapSlab *)mlndlPoolAlloc(pool, MLNDL_SLAB_BYTES);
	if (slab != NULL) {
		mlndlMemSet(slab, 0, sizeof(MlndlHeapSlab));
		slab->prev = NULL;
		slab->next = pool->slabHead;
		pool->slabHead = slab;
		slab->freeUnits = MLNDL_SLAB_UNITS;
		if (slab->next != NULL)
			slab->next->prev = slab;
	}
	return slab;
}

static int mlndlInArena(const MlndlHeapPool *pool, uintptr_t addr)
{
	return addr >= pool->arenaStart && addr < pool->arenaEnd;
}

static void *mlndlPoolAlloc(MlndlHeapPool *pool, uint32_t size)
{
	int units;

	if (size == 0)
		return NULL;

	units = (int)((size + 15) >> 4);
	if (size <= MLNDL_SMALL_LIMIT) {
		MlndlHeapSlab *slab = pool->slabHead;
		while (slab != NULL) {
			void *res = mlndlSlabAlloc(pool, slab, units);
			if (res != NULL)
				return res;
			slab = slab->next;
		}
		slab = mlndlSlabCreate(pool);
		return mlndlSlabAlloc(pool, slab, units);
	}

	MlndlHeapBlock *cursor = pool->freeHead;
	while ((uintptr_t)cursor < pool->arenaEnd) {
		MlndlHeapBlock *nextPhys = cursor->nextPhys;
		MlndlHeapBlock *nextFree = cursor->nextFree;
		int availUnits = (int)(((uintptr_t)nextPhys - (uintptr_t)cursor)
			/ MLNDL_UNIT_BYTES) - 1;

		if (units + 1 < availUnits) {
			uint32_t alignedBytes = (size + 15) & ~15U;
			MlndlHeapBlock *split = (MlndlHeapBlock *)((uintptr_t)(cursor + 1)
				+ alignedBytes);

			split->prevPhys = cursor;
			split->nextPhys = nextPhys;
			split->prevFree = cursor->prevFree;
			split->nextFree = nextFree;

			if (mlndlInArena(pool, (uintptr_t)nextPhys))
				nextPhys->prevPhys = split;
			if (mlndlInArena(pool, (uintptr_t)cursor->prevFree))
				cursor->prevFree->nextFree = split;
			if (mlndlInArena(pool, (uintptr_t)nextFree))
				nextFree->prevFree = split;
			if (pool->freeHead == cursor)
				pool->freeHead = split;

			cursor->nextPhys = split;
			cursor->prevFree = NULL;
			cursor->nextFree = NULL;
			return (void *)(cursor + 1);
		}
		if (units <= availUnits) {
			if (mlndlInArena(pool, (uintptr_t)cursor->prevFree))
				cursor->prevFree->nextFree = nextFree;
			if (mlndlInArena(pool, (uintptr_t)nextFree))
				nextFree->prevFree = cursor->prevFree;
			if (pool->freeHead == cursor)
				pool->freeHead = nextFree;
			cursor->prevFree = NULL;
			cursor->nextFree = NULL;
			return (void *)(cursor + 1);
		}
		cursor = nextFree;
	}
	return NULL;
}

void *mlndlAlloc(int poolSelector, uint32_t size)
{
	MlndlHeapPool *pool = (poolSelector != 0) ? s_auxPool : s_defaultPool;

	if (pool == NULL)
		return NULL;
	return mlndlPoolAlloc(pool, size);
}

static __attribute__((noinline)) MlndlHeapSlab *mlndlSlabFind(MlndlHeapPool *pool, uintptr_t address)
{
	MlndlHeapSlab *slab = pool->slabHead;

	while (slab != NULL) {
		uintptr_t arenaStart = (uintptr_t)slab->arena;
		uintptr_t arenaEnd = arenaStart + sizeof(slab->arena);
		if (address >= arenaStart && address < arenaEnd) {
			uintptr_t rel = address - arenaStart;
			if ((rel & 15U) != 0)
				return NULL;
			uint8_t mark = slab->unitMap[rel / MLNDL_UNIT_BYTES];
			if (mark == 0 || mark == MLNDL_SLAB_CONT)
				return NULL;
			return slab;
		}
		slab = slab->next;
	}
	return NULL;
}

static void mlndlSlabReleaseIfEmpty(MlndlHeapPool *pool, MlndlHeapSlab *slab)
{
	if (slab == NULL || slab->freeUnits != MLNDL_SLAB_UNITS)
		return;
	if (slab->prev != NULL)
		slab->prev->next = slab->next;
	if (slab->next != NULL)
		slab->next->prev = slab->prev;
	if (pool->slabHead == slab)
		pool->slabHead = slab->next;
	mlndlPoolFree(pool, slab);
}

static MlndlHeapBlock *mlndlBlockCoalesce(MlndlHeapPool *pool, MlndlHeapBlock *first,
	MlndlHeapBlock *second)
{
	MlndlHeapBlock *prevFree = NULL;
	MlndlHeapBlock *nextFree = NULL;
	int firstFree = (first->prevFree != NULL || first->nextFree != NULL) ? 2 : 0;
	int secondFree = (second->prevFree != NULL || second->nextFree != NULL) ? 1 : 0;
	int mask = firstFree + secondFree;

	if (mask == 1) {
		prevFree = second->prevFree;
		nextFree = second->nextFree;
	} else if (mask == 2) {
		prevFree = first->prevFree;
		nextFree = first->nextFree;
	} else if (mask == 3) {
		prevFree = first->prevFree;
		nextFree = second->nextFree;
	}

	first->nextPhys = second->nextPhys;
	first->prevFree = prevFree;
	first->nextFree = nextFree;

	second->prevPhys = NULL;
	second->nextPhys = NULL;
	second->prevFree = NULL;
	second->nextFree = NULL;

	if (mlndlInArena(pool, (uintptr_t)first->nextPhys))
		first->nextPhys->prevPhys = first;
	if (mlndlInArena(pool, (uintptr_t)prevFree))
		prevFree->nextFree = first;
	if (mlndlInArena(pool, (uintptr_t)nextFree))
		nextFree->prevFree = first;
	if (pool->freeHead == second)
		pool->freeHead = first;
	return first;
}

static __attribute__((noinline)) void mlndlFreeListInsert(MlndlHeapPool *pool, MlndlHeapBlock *block)
{
	MlndlHeapBlock *cursor = pool->freeHead;

	if ((uintptr_t)block < (uintptr_t)cursor) {
		block->nextFree = cursor;
		block->prevFree = NULL;
		cursor->prevFree = block;
		pool->freeHead = block;
		return;
	}

	while (mlndlInArena(pool, (uintptr_t)cursor)) {
		MlndlHeapBlock *nextFree = cursor->nextFree;
		if ((uintptr_t)cursor < (uintptr_t)block && (uintptr_t)block < (uintptr_t)nextFree) {
			block->prevFree = cursor;
			block->nextFree = nextFree;
			if (mlndlInArena(pool, (uintptr_t)nextFree))
				nextFree->prevFree = block;
			cursor->nextFree = block;
			return;
		}
		cursor = nextFree;
	}
}

static inline __attribute__((always_inline)) int mlndlIsValidAllocatedBlock(const MlndlHeapPool *pool, uintptr_t address,
	MlndlHeapBlock **outBlock)
{
	uintptr_t aligned = address & ~(uintptr_t)15;
	MlndlHeapBlock *block = (MlndlHeapBlock *)(aligned - sizeof(MlndlHeapBlock));

	if (aligned != address)
		return 0;
	if (!mlndlInArena(pool, (uintptr_t)block))
		return 0;
	if (block->prevPhys != NULL && !mlndlInArena(pool, (uintptr_t)block->prevPhys))
		return 0;
	if ((uintptr_t)block->nextPhys < pool->arenaStart
		|| (uintptr_t)block->nextPhys > pool->arenaEnd)
		return 0;
	if (block->prevFree != NULL && !mlndlInArena(pool, (uintptr_t)block->prevFree))
		return 0;
	if (block->nextFree != NULL && ((uintptr_t)block->nextFree < pool->arenaStart
		|| (uintptr_t)block->nextFree > pool->arenaEnd))
		return 0;
	if ((uintptr_t)block->prevPhys >= (uintptr_t)block->nextPhys)
		return 0;
	if ((uintptr_t)block->prevFree > (uintptr_t)block->nextFree)
		return 0;
	if (block->prevFree != NULL || block->nextFree != NULL)
		return 0;

	*outBlock = block;
	return 1;
}

static void mlndlPoolFree(MlndlHeapPool *pool, void *address)
{
	MlndlHeapSlab *slab;
	MlndlHeapBlock *block;
	MlndlHeapBlock *prevPhys;
	MlndlHeapBlock *nextPhys;
	int merged = 0;

	if (address == NULL)
		return;

	slab = mlndlSlabFind(pool, (uintptr_t)address);
	if (slab != NULL) {
		int unitPos = (int)(((uintptr_t)address - (uintptr_t)slab->arena)
			/ MLNDL_UNIT_BYTES);
		int count = slab->unitMap[unitPos];
		for (int clear = 0; clear < count; clear++)
			slab->unitMap[unitPos + clear] = 0;
		slab->freeUnits += count;
		mlndlSlabReleaseIfEmpty(pool, slab);
		return;
	}

	if (!mlndlIsValidAllocatedBlock(pool, (uintptr_t)address, &block))
		return;

	prevPhys = block->prevPhys;
	nextPhys = block->nextPhys;

	if (mlndlInArena(pool, (uintptr_t)prevPhys)
		&& (prevPhys->prevFree != NULL || prevPhys->nextFree != NULL)) {
		block = mlndlBlockCoalesce(pool, prevPhys, block);
		nextPhys = block->nextPhys;
		merged = 1;
	}
	if (mlndlInArena(pool, (uintptr_t)nextPhys)
		&& (nextPhys->prevFree != NULL || nextPhys->nextFree != NULL)) {
		mlndlBlockCoalesce(pool, block, nextPhys);
		return;
	}
	if (!merged)
		mlndlFreeListInsert(pool, block);
}

static void *mlndlPoolRealloc(MlndlHeapPool *pool, void *address, uint32_t newSize)
{
	MlndlHeapSlab *slab;
	MlndlHeapBlock *block;
	int newUnits;

	if (address == NULL)
		return (newSize != 0) ? mlndlPoolAlloc(pool, newSize) : NULL;
	if (newSize == 0) {
		mlndlPoolFree(pool, address);
		return NULL;
	}

	newUnits = (int)((newSize + 15) >> 4);
	slab = mlndlSlabFind(pool, (uintptr_t)address);
	if (slab != NULL) {
		int unitPos = (int)(((uintptr_t)address - (uintptr_t)slab->arena)
			/ MLNDL_UNIT_BYTES);
		int oldUnits = slab->unitMap[unitPos];
		if (oldUnits == newUnits)
			return address;

		void *replacement = mlndlPoolAlloc(pool, newSize);
		if (replacement == NULL)
			return NULL;

		uint32_t copyLen = (newSize > (uint32_t)(oldUnits * MLNDL_UNIT_BYTES))
			? (uint32_t)(oldUnits * MLNDL_UNIT_BYTES)
			: newSize;
		mlndlMemCpy(replacement, address, copyLen);
		for (int clear = 0; clear < oldUnits; clear++)
			slab->unitMap[unitPos + clear] = 0;
		slab->freeUnits += oldUnits;
		mlndlSlabReleaseIfEmpty(pool, slab);
		return replacement;
	}

	if (!mlndlIsValidAllocatedBlock(pool, (uintptr_t)address, &block))
		return NULL;

	int oldUnits = (int)(((uintptr_t)block->nextPhys - (uintptr_t)block)
		/ MLNDL_UNIT_BYTES) - 1;
	if (newUnits + 1 < oldUnits) {
		uint32_t alignedBytes = (newSize + 15) & ~15U;
		MlndlHeapBlock *split = (MlndlHeapBlock *)((uintptr_t)address + alignedBytes);
		MlndlHeapBlock *nextPhys = block->nextPhys;

		split->prevPhys = block;
		split->nextPhys = nextPhys;
		split->prevFree = NULL;
		split->nextFree = NULL;
		block->nextPhys = split;

		if (mlndlInArena(pool, (uintptr_t)nextPhys)) {
			nextPhys->prevPhys = split;
			if (nextPhys->prevFree != NULL || nextPhys->nextFree != NULL)
				mlndlBlockCoalesce(pool, split, nextPhys);
			else
				mlndlFreeListInsert(pool, split);
		}
		return address;
	}

	if (newUnits > oldUnits) {
		void *replacement = mlndlPoolAlloc(pool, newSize);
		if (replacement != NULL) {
			uint32_t oldBytes = (uint32_t)(oldUnits * MLNDL_UNIT_BYTES);
			uint32_t copyLen = (newSize > oldBytes) ? oldBytes : newSize;
			mlndlMemCpy(replacement, address, copyLen);
			mlndlPoolFree(pool, address);
		}
		return replacement;
	}
	return address;
}

void *mlndlRealloc(int poolSelector, void *block, uint32_t size)
{
	MlndlHeapPool *pool = (poolSelector != 0) ? s_auxPool : s_defaultPool;

	if (pool == NULL)
		return NULL;
	return mlndlPoolRealloc(pool, block, size);
}

void mlndlFree(int poolSelector, void *block)
{
	MlndlHeapPool *pool = (poolSelector != 0) ? s_auxPool : s_defaultPool;

	if (pool != NULL)
		mlndlPoolFree(pool, block);
}

void mlndlHeapTerm(int poolSelector)
{
	MlndlHeapPool **slot = (poolSelector == 0) ? &s_defaultPool : &s_auxPool;

	if (*slot != NULL)
		*slot = NULL;
}

void *mlndlCopyBytes(void *dest, const void *src, uint32_t count)
{
	uint8_t *out = (uint8_t *)dest;
	const uint8_t *in = (const uint8_t *)src;

	while (count != 0) {
		*out++ = *in++;
		count--;
	}
	return dest;
}

int mlndlFormatString(char *dest, int maxLen, const char *format, ...)
{
	va_list args;
	int written = 0;

	(void)maxLen;
	va_start(args, format);
	while (*format != '\0') {
		if (*format == '%') {
			format++;
			if (*format == 's') {
				const char *str = va_arg(args, const char *);
				format++;
				while (*str != '\0') {
					*dest++ = *str++;
					written++;
				}
			}
		} else {
			*dest++ = *format++;
			written++;
		}
	}
	*dest = '\0';
	va_end(args);
	return written;
}

char *mlndlStrNCpy(char *dest, const char *src, uint32_t count)
{
	char *start = dest;

	while (count != 0) {
		char ch = *src++;
		*dest++ = ch;
		if (ch == '\0')
			break;
		count--;
	}
	return start;
}

int mlndlStrNCmp(const char *left, const char *right, uint32_t count)
{
	uint32_t pos = 0;

	while (pos < count && left[pos] != '\0') {
		if (left[pos] != right[pos])
			return (left[pos] > right[pos]) ? 1 : -1;
		pos++;
	}
	if (pos != count && right[pos] != '\0')
		return -1;
	return 0;
}
