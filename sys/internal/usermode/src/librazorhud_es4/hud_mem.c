/*
 * GXM memory mapping, embedded bitmap font texture, and 2-pool memory allocator.
 */
#include "razor_hud_internal.h"
#include "hud_font.h"

typedef struct HudFontSingleton {
	SceGxmTexture texture;
	void *pixels;
	uint8_t initialized;
	uint8_t pad[3];
} HudFontSingleton;

typedef struct HudPoolBlock {
	uint32_t offset;
	uint32_t size;
	uint32_t flags;
} HudPoolBlock;

typedef struct HudMemPool {
	HudPoolBlock blocks[128];
	uint32_t blockCount;
	uint32_t maxBlockCount;
	int32_t memBlockUid;
	uint8_t *base;
	uint32_t usedBytes;
	uint32_t peakBytes;
	uint32_t capacity;
	uint32_t allocCount;
	uint32_t freeCount;
	uint32_t minAlign;
} HudMemPool;

static HudFontSingleton g_fontObj;
static uint32_t g_fontGuard;

static uint8_t g_poolUninitialized = 1;
static HudMemPool g_pools[2];
static uint8_t g_staticPoolBuf[0x500000];

static int32_t g_vdmRingUid;
static void *g_vdmRingBase;
static int32_t g_vertexRingUid;
static void *g_vertexRingBase;
static int32_t g_fragmentRingUid;
static void *g_fragmentRingBase;
static int32_t g_fragmentUsseRingUid;
static void *g_fragmentUsseRingBase;
static uint32_t g_fragmentUsseRingOffset;
static int32_t g_patcherBufUid;
static void *g_patcherBufBase;
static int32_t g_patcherUsseUid;
static void *g_patcherUsseBase;
static uint32_t g_patcherVertexUsseOffset;
static uint32_t g_patcherFragmentUsseOffset;

static inline __attribute__((always_inline)) void hudPoolsInitStatic(void)
{
	sceClibMemset(g_pools, 0, sizeof(g_pools));
	if (g_pools[0].base == NULL) {
		g_pools[0].base = g_staticPoolBuf;
		sceClibMemset(g_staticPoolBuf, 0xCC, sizeof(g_staticPoolBuf));
	}
	g_pools[1].minAlign = 64;
	g_pools[0].blockCount = 1;
	g_pools[0].maxBlockCount = 1;
	g_pools[0].capacity = sizeof(g_staticPoolBuf);
	g_pools[0].blocks[0].size = sizeof(g_staticPoolBuf);
	if (g_pools[0].minAlign <= 3)
		g_pools[0].minAlign = 4;
	g_pools[1].capacity = 0x600000;
	g_pools[1].blocks[0].size = 0x600000;
	g_pools[0].blocks[0].flags |= 1U;
	g_pools[1].blocks[0].flags |= 1U;
	g_pools[1].blockCount = 1;
	g_pools[1].maxBlockCount = 1;
	if (g_pools[1].minAlign <= 3)
		g_pools[1].minAlign = 4;
	g_poolUninitialized = 0;
}

void *hudMemFontSingleton(void)
{
	if ((g_fontGuard & 1U) == 0 && hudGuardAcquire(&g_fontGuard) != 0) {
		g_fontObj.pixels = NULL;
		g_fontObj.initialized = 0;
		sceClibMemset(&g_fontObj.texture, 0, sizeof(g_fontObj.texture));
		hudGuardRelease(&g_fontGuard);
	}
	return &g_fontObj;
}

int hudMemFontInit(void *fontPtr)
{
	HudFontSingleton *f = (HudFontSingleton *)fontPtr;
	int ret = 0;

	if (f->initialized == 0) {
		void *buf = hudPoolAlloc(1U, sizeof(g_hudFontPixels), 4U);
		f->pixels = buf;
		sceClibMemcpy_safe(buf, g_hudFontPixels, sizeof(g_hudFontPixels));
		sceGxmTextureInitLinear(&f->texture, f->pixels, 0x0C001000U, 256U, 128U, 1U);
		sceGxmTextureSetMinFilter(&f->texture, 1U);
		sceGxmTextureSetMagFilter(&f->texture, 1U);
		ret = sceGxmTextureSetMipFilter(&f->texture, 0x200U);
		f->initialized = 1;
	}
	return ret;
}

int hudMemFontTerm(void *fontPtr)
{
	HudFontSingleton *f = (HudFontSingleton *)fontPtr;

	if (f->pixels != NULL)
		hudPoolFree(1U, f->pixels);
	f->initialized = 0;
	return 0;
}

void *hudMemFontGetTexture(void *fontPtr, int idx)
{
	if (idx != 0)
		return NULL;
	return fontPtr;
}

int hudMemInit(void)
{
	void *base;

	if (g_poolUninitialized != 0)
		hudPoolsInitStatic();

	if (g_pools[1].base == NULL) {
		g_pools[1].memBlockUid = sceKernelAllocMemBlock("common", 0x0C208060U, 0x600000U, NULL);
		base = NULL;
		sceKernelGetMemBlockBase(g_pools[1].memBlockUid, &base);
		sceGxmMapMemory(base, 0x600000U, 1U);
		g_pools[1].base = (uint8_t *)base;
	}

	g_vdmRingUid = sceKernelAllocMemBlock("common", 0x0C208060U, 0x8000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_vdmRingUid, &base);
	sceGxmMapMemory(base, 0x8000U, 1U);
	g_vdmRingBase = base;

	g_vertexRingUid = sceKernelAllocMemBlock("common", 0x0C208060U, 0x40000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_vertexRingUid, &base);
	sceGxmMapMemory(base, 0x40000U, 1U);
	g_vertexRingBase = base;

	g_fragmentRingUid = sceKernelAllocMemBlock("common", 0x0C208060U, 0x20000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_fragmentRingUid, &base);
	sceGxmMapMemory(base, 0x20000U, 1U);
	g_fragmentRingBase = base;

	g_fragmentUsseRingUid = sceKernelAllocMemBlock("RazorHud", 0x0C208060U, 0x1000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_fragmentUsseRingUid, &base);
	sceGxmMapFragmentUsseMemoryInternal(base, 0x1000U, &g_fragmentUsseRingOffset);
	g_fragmentUsseRingBase = base;

	g_patcherBufUid = sceKernelAllocMemBlock("common", 0x0C208060U, 0x4000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_patcherBufUid, &base);
	sceGxmMapMemory(base, 0x4000U, 3U);
	g_patcherBufBase = base;

	g_patcherUsseUid = sceKernelAllocMemBlock("RazorHud", 0x0C208060U, 0x4000U, NULL);
	base = NULL;
	sceKernelGetMemBlockBase(g_patcherUsseUid, &base);
	sceGxmMapVertexUsseMemoryInternal(base, 0x4000U, &g_patcherVertexUsseOffset);
	sceGxmMapFragmentUsseMemoryInternal(base, 0x4000U, &g_patcherFragmentUsseOffset);
	g_patcherUsseBase = base;
	return 0;
}

int hudMemTerm(void)
{
	void *base;
	int ret;

	base = NULL;
	sceKernelGetMemBlockBase(g_patcherUsseUid, &base);
	sceGxmUnmapFragmentUsseMemoryInternal(base);
	sceGxmUnmapVertexUsseMemoryInternal(base);
	sceKernelFreeMemBlock(g_patcherUsseUid);

	base = NULL;
	sceKernelGetMemBlockBase(g_patcherBufUid, &base);
	sceGxmUnmapMemory(base);
	sceKernelFreeMemBlock(g_patcherBufUid);

	base = NULL;
	sceKernelGetMemBlockBase(g_fragmentUsseRingUid, &base);
	sceGxmUnmapFragmentUsseMemoryInternal(base);
	sceKernelFreeMemBlock(g_fragmentUsseRingUid);

	base = NULL;
	sceKernelGetMemBlockBase(g_fragmentRingUid, &base);
	sceGxmUnmapMemory(base);
	sceKernelFreeMemBlock(g_fragmentRingUid);

	base = NULL;
	sceKernelGetMemBlockBase(g_vertexRingUid, &base);
	sceGxmUnmapMemory(base);
	sceKernelFreeMemBlock(g_vertexRingUid);

	base = NULL;
	sceKernelGetMemBlockBase(g_vdmRingUid, &base);
	sceGxmUnmapMemory(base);
	ret = sceKernelFreeMemBlock(g_vdmRingUid);

	if (g_pools[1].base != NULL) {
		base = NULL;
		sceKernelGetMemBlockBase(g_pools[1].memBlockUid, &base);
		sceGxmUnmapMemory(base);
		ret = sceKernelFreeMemBlock(g_pools[1].memBlockUid);
		g_pools[1].base = NULL;
	}
	return ret;
}

void *hudPoolAlloc(uint32_t poolIdx, uint32_t size, uint32_t align)
{
	HudMemPool *pool;
	uint32_t alignedSize, i;
	uintptr_t baseAddr, blkStart, blkEnd, allocStart, allocEnd;
	void *ptr;

	if (g_poolUninitialized != 0)
		hudPoolsInitStatic();
	if (poolIdx > 1U)
		return NULL;

	pool = &g_pools[poolIdx];
	if (pool->base == NULL)
		return NULL;

	baseAddr = (uintptr_t)pool->base;
	alignedSize = (size + pool->minAlign - 1U) & ~(pool->minAlign - 1U);

	for (i = 0; i < 128U; i++) {
		HudPoolBlock *blk = &pool->blocks[i];
		blkStart = baseAddr + blk->offset;
		if ((blk->flags & 1U) == 0)
			continue;
		blkEnd = blkStart + blk->size;
		allocStart = (blkStart + align - 1U) & ~(uintptr_t)(align - 1U);
		allocEnd = allocStart + alignedSize;
		if (blkEnd >= allocEnd) {
			uint32_t origOff = blk->offset;
			if (allocStart > blkStart && pool->blockCount < 128U) {
				HudPoolBlock *nb = &pool->blocks[pool->blockCount++];
				if (pool->blockCount > pool->maxBlockCount)
					pool->maxBlockCount = pool->blockCount;
				nb->offset = origOff;
				nb->size = (uint32_t)(allocStart - blkStart);
				nb->flags |= 1U;
			}
			if (allocEnd < blkEnd && pool->blockCount < 128U) {
				HudPoolBlock *nb = &pool->blocks[pool->blockCount++];
				if (pool->blockCount > pool->maxBlockCount)
					pool->maxBlockCount = pool->blockCount;
				nb->offset = (uint32_t)(allocEnd - baseAddr);
				nb->size = (uint32_t)(blkEnd - allocEnd);
				nb->flags |= 1U;
			}
			blk->offset = (uint32_t)(allocStart - baseAddr);
			blk->size = alignedSize;
			blk->flags &= ~1U;
			pool->usedBytes += alignedSize;
			if (pool->usedBytes > pool->peakBytes)
				pool->peakBytes = pool->usedBytes;
			pool->allocCount++;
			ptr = (void *)allocStart;
			sceClibMemset(ptr, 0xAA, alignedSize);
			return ptr;
		}
	}

	sceClibPrintf("[SceRazorHud]:Pool #%d is out of memory. Could not allocate %s bytes.\n",
		poolIdx, hudFormatInt64Commas((int64_t)alignedSize));
	return NULL;
}

void hudPoolFree(uint32_t poolIdx, void *ptr)
{
	HudMemPool *pool;
	uint32_t off, i, j;
	int merged;

	if (ptr == NULL)
		return;
	pool = &g_pools[poolIdx];
	off = (uint32_t)((uint8_t *)ptr - pool->base);

	for (i = 0; i < 128U; i++) {
		if ((pool->blocks[i].flags & 1U) == 0 && pool->blocks[i].offset == off) {
			pool->blocks[i].flags |= 1U;
			pool->freeCount++;
			pool->usedBytes -= pool->blocks[i].size;
			break;
		}
	}
	if (i == 128U)
		return;

	do {
		merged = 0;
		for (i = 0; i < pool->blockCount && !merged; i++) {
			if ((pool->blocks[i].flags & 1U) == 0)
				continue;
			for (j = i + 1U; j < pool->blockCount; j++) {
				if ((pool->blocks[j].flags & 1U) == 0)
					continue;
				if (pool->blocks[i].offset + pool->blocks[i].size == pool->blocks[j].offset) {
					pool->blocks[i].size += pool->blocks[j].size;
					if (j + 1U < pool->blockCount) {
						hudMemmoveWrap(&pool->blocks[j], &pool->blocks[j + 1U],
							(pool->blockCount - j - 1U) * sizeof(HudPoolBlock));
					}
					pool->blockCount--;
					merged = 1;
					break;
				}
				if (pool->blocks[j].offset + pool->blocks[j].size == pool->blocks[i].offset) {
					pool->blocks[j].size += pool->blocks[i].size;
					if (i + 1U < pool->blockCount) {
						hudMemmoveWrap(&pool->blocks[i], &pool->blocks[i + 1U],
							(pool->blockCount - i - 1U) * sizeof(HudPoolBlock));
					}
					pool->blockCount--;
					merged = 1;
					break;
				}
			}
		}
	} while (merged);
}

void *hudGetVdmRingMem(void) { return g_vdmRingBase; }
void *hudGetVertexRingMem(void) { return g_vertexRingBase; }
void *hudGetFragmentRingMem(void) { return g_fragmentRingBase; }
void *hudGetFragmentUsseRingMem(void) { return g_fragmentUsseRingBase; }
uint32_t hudGetVdmRingSize(void) { return 0x8000U; }
uint32_t hudGetVertexRingSize(void) { return 0x40000U; }
uint32_t hudGetFragmentRingSize(void) { return 0x20000U; }
uint32_t hudGetFragmentUsseRingSize(void) { return 0x1000U; }
uint32_t hudGetFragmentUsseRingOffset(void) { return g_fragmentUsseRingOffset; }
void *hudGetPatcherBufMem(void) { return g_patcherBufBase; }
void *hudGetPatcherVertexUsseMem(void) { return g_patcherUsseBase; }
void *hudGetPatcherFragmentUsseMem(void) { return g_patcherUsseBase; }
uint32_t hudGetPatcherBufSize(void) { return 0x4000U; }
uint32_t hudGetPatcherVertexUsseSize(void) { return 0x4000U; }
uint32_t hudGetPatcherFragmentUsseSize(void) { return 0x4000U; }
uint32_t hudGetPatcherVertexUsseOffset(void) { return g_patcherVertexUsseOffset; }
uint32_t hudGetPatcherFragmentUsseOffset(void) { return g_patcherFragmentUsseOffset; }

void *hudNew(uint32_t size)
{
	return hudPoolAlloc(0U, size != 0 ? size : 1U, 4U);
}

void *hudNewAligned(uint32_t size, uint32_t align)
{
	return hudPoolAlloc(0U, size != 0 ? size : 1U, align);
}

void *hudNewArray(uint32_t size)
{
	return hudPoolAlloc(0U, size != 0 ? size : 1U, 4U);
}

void hudDelete(void *ptr)
{
	hudPoolFree(0U, ptr);
}

void hudDeleteArray(void *ptr)
{
	hudPoolFree(0U, ptr);
}
