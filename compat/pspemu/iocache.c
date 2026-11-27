/*
 * iocache.c: two-tier read-ahead block cache (512 KiB blocks) and whole-file
 * cache (up to 32 small files) for remote_msfs.
 */
#include "host_common.h"
#include <sysmem/memblock.h>
#include <string.h>

#define IOCACHE_BLOCK_SHIFT      19U
#define IOCACHE_BLOCK_SIZE       (1U << IOCACHE_BLOCK_SHIFT) /* 512 KiB */
#define IOCACHE_MAX_BLOCKS       256U
#define IOCACHE_TABLE_SLOTS      8192U
#define IOCACHE_REQ_QUEUE_SIZE   128U
#define IOCACHE_MAX_FILE_SLOTS   32

enum {
	IOCACHE_BLK_FREE = 0,
	IOCACHE_BLK_READY = 1,
	IOCACHE_BLK_READING = 2,
	IOCACHE_BLK_LOADING = 3,
};

typedef struct IoCacheBlock {
	uint16_t            blockIndex;
	uint16_t            state;
	uint32_t            validBytes;
	uint8_t            *buffer;
	struct IoCacheBlock *next;
} IoCacheBlock;

typedef struct IoCacheFileSlot {
	uint8_t *buffer;
	SceUID   fd;
	SceOff   position;
	uint32_t fileSize;
	uint32_t pad;
} IoCacheFileSlot;

int g_iocacheReady;
int g_iocacheReqHead;
SceUID g_iocacheThreadId;
SceUID g_iocacheReqSema;

int64_t g_iocacheReqQueue[IOCACHE_REQ_QUEUE_SIZE];
int g_iocacheReqTail;
SceKernelLwMutexWork g_iocacheReqLock;
SceUID g_iocacheBackingFd;
IoCacheBlock *g_iocacheLruTail;
uint8_t *g_iocacheBufferBase;
SceKernelLwMutexWork g_iocacheTableLock;
IoCacheBlock g_iocacheBlocks[IOCACHE_MAX_BLOCKS];
int g_iocacheBlockCount;
IoCacheBlock *g_iocacheLruHead;
SceUID g_iocacheMemBlockId;
IoCacheBlock *g_iocacheBlockTable[IOCACHE_TABLE_SLOTS];

int g_iocacheFileSlotCount;
IoCacheFileSlot g_iocacheFileSlots[IOCACHE_MAX_FILE_SLOTS];
SceUID g_iocacheFileMemBlockId;
uint8_t *g_iocacheFileBufferBase;
SceSize g_iocacheFileSlotSize;

/* 0x8101eef0 */
static IoCacheBlock *iocacheLookupBlock(SceOff offset)
{
	uint32_t blockIdx;
	IoCacheBlock *blk;

	if ((uint64_t)offset >= (1ULL << 32))
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	blockIdx = (uint32_t)offset >> IOCACHE_BLOCK_SHIFT;
	blk = g_iocacheBlockTable[blockIdx];
	if (blk != NULL && ((unsigned int)(blk->state - 1) > 2U || blk->blockIndex != blockIdx))
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	return blk;
}

/* 0x8101ef24 */
static int iocacheEnqueueRequest(int64_t offset)
{
	int nextTail = (g_iocacheReqTail + 1) % (int)IOCACHE_REQ_QUEUE_SIZE;
	int head;
	int res;

	pspemuLockLwMutexOrFatal(&g_iocacheReqLock);
	head = g_iocacheReqHead;
	pspemuUnlockLwMutexOrFatal(&g_iocacheReqLock);

	if (nextTail == head)
		return -1;

	g_iocacheReqQueue[g_iocacheReqTail] = offset;
	res = sceKernelSignalSema(g_iocacheReqSema, 1);
	if (res < 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	g_iocacheReqTail = nextTail;
	return res;
}

/* 0x8101efb0 */
static void iocacheUnlinkBlock(IoCacheBlock *blk)
{
	IoCacheBlock *cursor;

	if ((unsigned int)(blk->state - 2) <= 1U)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	if (blk->state == IOCACHE_BLK_READY)
		g_iocacheBlockTable[blk->blockIndex & (IOCACHE_TABLE_SLOTS - 1U)] = NULL;

	if (blk == g_iocacheLruHead) {
		if (blk->next == NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		g_iocacheLruHead = blk->next;
		return;
	}

	cursor = g_iocacheLruHead;
	while (cursor->next != blk)
		cursor = cursor->next;

	if (blk == g_iocacheLruTail) {
		if (blk->next != NULL)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
		cursor->next = NULL;
		g_iocacheLruTail = cursor;
	} else {
		cursor->next = blk->next;
	}
}

/* 0x8101f018 */
static SceSSize iocacheReadFromBlock(SceOff offset, void *dst, SceSize size)
{
	IoCacheBlock *blk;
	uint32_t blockIdx;
	uint32_t blockStart;
	uint32_t inBlockOff;
	int32_t avail;
	SceSize copyLen;

	pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
	blk = iocacheLookupBlock(offset);
	if (blk == NULL || blk->state == IOCACHE_BLK_LOADING) {
		pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
		return 0;
	}

	iocacheUnlinkBlock(blk);
	blk->state = IOCACHE_BLK_READING;
	blk->next = g_iocacheLruHead;
	g_iocacheLruHead = blk;

	if ((uint64_t)offset >= (1ULL << 32))
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	blockIdx = (uint32_t)offset >> IOCACHE_BLOCK_SHIFT;
	g_iocacheBlockTable[blockIdx] = blk;
	blk->blockIndex = (uint16_t)blockIdx;
	pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);

	if (blk->state != IOCACHE_BLK_READING)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	blockStart = (uint32_t)blk->blockIndex << IOCACHE_BLOCK_SHIFT;
	if ((uint64_t)offset < (uint64_t)blockStart ||
	    (uint64_t)offset >= (uint64_t)(blockStart + blk->validBytes)) {
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
	}

	inBlockOff = (uint32_t)offset - blockStart;
	avail = (int32_t)(blk->validBytes - inBlockOff);
	if (avail <= 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	copyLen = ((SceSize)avail < size) ? (SceSize)avail : size;
	if (copyLen < 0x4000U ||
	    (pspemuIsPs1Mode() != 1 && (pspemuIsPs1Mode() != 0 || pspemuCompatHasFlag(0x1000000U) != 0))) {
		memcpy(dst, blk->buffer + inBlockOff, copyLen);
	} else {
		sceDmacMemcpy(dst, blk->buffer + inBlockOff, copyLen);
	}

	pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
	blk->state = IOCACHE_BLK_READY;
	pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
	return (SceSSize)copyLen;
}

/* 0x8101f13c: background prefetch worker thread */
static int iocacheWorkerThread(SceSize args, void *argp)
{
	MemBlockUserOpt opt;
	SceKernelMemBlockType blockType;
	SceUID memId;
	int index;
	int res;

	(void)args;
	(void)argp;

	if (pspemuIsPs1Mode() == 1) {
		blockType = PSPEMU_MEMBLOCK_PHYCONT_NC_RW;
	} else if (pspemuCompatHasFlag(0x100000U) != 0) {
		blockType = PSPEMU_MEMBLOCK_PHYCONT_RW;
	} else {
		blockType = PSPEMU_MEMBLOCK_PHYCONT_NC_RW;
	}

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.extAttr = 1;

	memId = sceKernelAllocMemBlock(
		"ScePspemuIoCacheBuffer", blockType, (SceSize)g_iocacheBlockCount << IOCACHE_BLOCK_SHIFT,
		(const SceKernelAllocMemBlockOpt *)&opt);
	g_iocacheMemBlockId = memId;
	if (memId < 0)
		pspemuTriggerFatalError(memId);

	res = sceKernelGetMemBlockBase(memId, (void **)&g_iocacheBufferBase);
	if (res != 0)
		pspemuTriggerFatalError(res);

	memset(g_iocacheBlocks, 0, sizeof(g_iocacheBlocks));
	memset(g_iocacheBlockTable, 0, sizeof(g_iocacheBlockTable));

	for (index = 0; index < g_iocacheBlockCount; index++) {
		g_iocacheBlocks[index].buffer = g_iocacheBufferBase + ((SceSize)index << IOCACHE_BLOCK_SHIFT);
		g_iocacheBlocks[index].state = IOCACHE_BLK_FREE;
		g_iocacheBlocks[index].next = (index + 1 == g_iocacheBlockCount)
			? NULL
			: &g_iocacheBlocks[index + 1];
	}

	g_iocacheLruTail = &g_iocacheBlocks[g_iocacheBlockCount - 1];
	g_iocacheLruHead = &g_iocacheBlocks[0];
	g_iocacheReady = 1;

	for (;;) {
		int64_t reqOff;

		if (sceKernelWaitSema(g_iocacheReqSema, 1, NULL) < 0)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		reqOff = g_iocacheReqQueue[g_iocacheReqHead];
		if (reqOff == -1LL) {
			sceIoClose(g_iocacheBackingFd);
			pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
			g_iocacheBackingFd = 0;
			pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
		} else if (reqOff == -2LL) {
			return 0;
		} else {
			IoCacheBlock *blk;

			pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
			blk = iocacheLookupBlock(reqOff);
			if (blk != NULL) {
				if (blk->state == IOCACHE_BLK_FREE)
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
			} else {
				uint32_t blockIdx;
				SceSSize bytesRead;

				blk = g_iocacheLruTail;
				if ((unsigned int)(blk->state - 2) <= 1U) {
					pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				}

				iocacheUnlinkBlock(blk);
				blk->state = IOCACHE_BLK_LOADING;
				blk->next = g_iocacheLruHead;
				g_iocacheLruHead = blk;

				if ((uint64_t)reqOff >= (1ULL << 32))
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

				blockIdx = (uint32_t)reqOff >> IOCACHE_BLOCK_SHIFT;
				g_iocacheBlockTable[blockIdx] = blk;
				blk->blockIndex = (uint16_t)blockIdx;
				pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);

				if (g_iocacheBackingFd <= 0 ||
				    (int64_t)blk->blockIndex != (reqOff >> IOCACHE_BLOCK_SHIFT)) {
					pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
				}

				bytesRead = sceIoPread(
					g_iocacheBackingFd,
					blk->buffer,
					IOCACHE_BLOCK_SIZE,
					(SceOff)blk->blockIndex << IOCACHE_BLOCK_SHIFT);
				if (bytesRead <= 0) {
					blk->validBytes = 0;
					pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
					blk->state = IOCACHE_BLK_FREE;
					g_iocacheBlockTable[blockIdx] = NULL;
					iocacheUnlinkBlock(blk);
					g_iocacheLruTail->next = blk;
					g_iocacheLruTail = blk;
					blk->next = NULL;
					pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
				} else {
					blk->validBytes = (uint32_t)bytesRead;
					pspemuLockLwMutexOrFatal(&g_iocacheTableLock);
					blk->state = IOCACHE_BLK_READY;
					pspemuUnlockLwMutexOrFatal(&g_iocacheTableLock);
				}
			}
		}

		pspemuLockLwMutexOrFatal(&g_iocacheReqLock);
		g_iocacheReqHead = (g_iocacheReqHead + 1) % (int)IOCACHE_REQ_QUEUE_SIZE;
		pspemuUnlockLwMutexOrFatal(&g_iocacheReqLock);
	}
}

/* 0x8101f454 */
SceSSize iocacheRead(SceUID fd, void *buf, SceSize size)
{
	uint8_t *dst = (uint8_t *)buf;
	int isCachedDst = pspemuIsCachedCdramAddress((uintptr_t)dst);
	SceSize remaining;
	SceSSize totalRead = 0;

	if (g_iocacheReady == 0 || g_iocacheBackingFd <= 0)
		return sceIoRead(fd, dst, size);
	if (size == 0)
		return 0;

	remaining = size;
	while (remaining != 0) {
		SceOff curPos;
		SceSSize chunk;

		curPos = sceIoLseek(fd, 0, SCE_SEEK_CUR);
		if (curPos < 0)
			return (SceSSize)curPos;

		if ((int)remaining <= 0 || remaining > size)
			pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

		chunk = iocacheReadFromBlock(curPos, dst, remaining);
		if (isCachedDst != 0 && chunk > 0) {
			if (pspemuIsCachedCdramAddress((uintptr_t)dst) != 0)
				sceCompatCache(3, dst, (SceSize)(((chunk - 1) & ~63) + 64));

			if (chunk <= 63 && remaining > 63U) {
				SceSSize extra = iocacheReadFromBlock(curPos + chunk, dst + chunk, remaining - (SceSize)chunk);
				if (extra > 0)
					chunk += extra;
			}
			chunk &= ~63;
		}

		if (chunk > 0) {
			remaining -= (SceSize)chunk;
			if (sceIoLseek(fd, (SceOff)chunk, SCE_SEEK_CUR) < 0)
				return totalRead;
			dst += chunk;
			totalRead += chunk;
		} else {
			SceSSize directRead = sceIoRead(fd, dst, remaining);
			if (directRead <= 0)
				return (totalRead != 0) ? totalRead : directRead;
			remaining -= (SceSize)directRead;
			dst += directRead;
			if ((int)remaining < 0)
				pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
			totalRead += directRead;
		}
	}

	return totalRead;
}

/* 0x8101f594 */
SceUID iocacheOpenBackingFile(const char *path, int flags, SceMode mode)
{
	SceUID fd;

	if (g_iocacheBackingFd > 0)
		return -1;
	fd = sceIoOpen(path, flags, mode);
	g_iocacheBackingFd = fd;
	return fd;
}

/* 0x8101f5b4 */
int iocacheRequestClose(void)
{
	return iocacheEnqueueRequest(-1LL);
}

/* 0x8101f5c4 */
int iocacheInit(uint32_t cacheSize)
{
	uint32_t blockCount = cacheSize >> IOCACHE_BLOCK_SHIFT;
	SceUID semaId;
	int res;

	g_iocacheBlockCount = (int)blockCount;
	g_iocacheBackingFd = 0;

	if (blockCount <= IOCACHE_MAX_BLOCKS) {
		memset(g_iocacheReqQueue, 0, sizeof(g_iocacheReqQueue));
		g_iocacheReqTail = 0;
		g_iocacheReqHead = 0;

		if (sceKernelCreateLwMutex(&g_iocacheReqLock, "ScePspemuIoCacheLockForCacheRequest", 0, 0, NULL) == 0) {
			semaId = sceKernelCreateSema("ScePspemuIoCacheRequest", 0, 0, IOCACHE_REQ_QUEUE_SIZE, NULL);
			g_iocacheReqSema = semaId;
			if (semaId > 0) {
				g_iocacheThreadId = sceKernelCreateThread(
					"ScePspemuIoCacheRequest",
					iocacheWorkerThread,
					99,
					0x1000,
					0,
					SCE_KERNEL_CPU_MASK_USER_0,
					NULL);
				if (g_iocacheThreadId >= 0 &&
				    sceKernelCreateLwMutex(&g_iocacheTableLock, "ScePspemuIoCacheLockForCacheTable", 0, 0, NULL) == 0) {
					res = sceKernelStartThread(g_iocacheThreadId, 0, NULL);
					if (res == 0)
						return 0;
				}
			}
		}
	}

	pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);
}

/* 0x8101f6b8 */
int iocachePrefetchRange(SceOff offset, uint32_t length)
{
	SceOff endOff;
	SceOff cursor;

	if (g_iocacheReady == 0)
		return 0;
	if (g_iocacheBackingFd <= 0)
		return -1;

	endOff = offset + (SceOff)length + 0x100000LL;
	for (cursor = offset; cursor <= endOff; cursor += IOCACHE_BLOCK_SIZE)
		iocacheEnqueueRequest(cursor);
	return 0;
}

/* 0x8101f710 */
int iocacheFileInit(SceSize totalSize, SceSize slotSize)
{
	SceUID memId;
	int index;

	g_iocacheFileSlotSize = slotSize;
	g_iocacheFileSlotCount = (int)totalSize / (int)slotSize;
	if (g_iocacheFileSlotCount > IOCACHE_MAX_FILE_SLOTS)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	memId = sceKernelAllocMemBlock("ScePspemuIoCacheFileBuffer", PSPEMU_MEMBLOCK_PHYCONT_RW, totalSize, NULL);
	g_iocacheFileMemBlockId = memId;
	if (memId < 0 || sceKernelGetMemBlockBase(memId, (void **)&g_iocacheFileBufferBase) != 0)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	memset(g_iocacheFileBufferBase, 0, totalSize);
	memset(g_iocacheFileSlots, 0, sizeof(g_iocacheFileSlots));
	for (index = 0; index < g_iocacheFileSlotCount; index++)
		g_iocacheFileSlots[index].buffer = g_iocacheFileBufferBase + (SceSize)index * slotSize;

	return 0;
}

/* 0x8101f7bc */
SceSSize iocacheFileLoad(const char *path, int flags, SceMode mode, SceUID fd)
{
	int slotIndex;
	SceSSize bytesRead;

	(void)path;
	(void)flags;
	(void)mode;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == 0)
			break;
	}
	if (slotIndex >= g_iocacheFileSlotCount)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	bytesRead = sceIoRead(fd, g_iocacheFileSlots[slotIndex].buffer, g_iocacheFileSlotSize);
	if (bytesRead > 0) {
		sceIoLseek(fd, 0, SCE_SEEK_SET);
		g_iocacheFileSlots[slotIndex].fileSize = (uint32_t)bytesRead;
		g_iocacheFileSlots[slotIndex].fd = fd;
		g_iocacheFileSlots[slotIndex].position = 0;
		return fd;
	}
	return bytesRead;
}

/* 0x8101f83c */
int iocacheFileIsCached(SceUID fd)
{
	int slotIndex;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == fd)
			return 1;
	}
	return 0;
}

/* 0x8101f878 */
int iocacheFileCanCache(const char *path, int flags, SceMode mode)
{
	SceIoStat stat;
	int slotIndex;
	int hasFree = 0;

	(void)mode;

	if ((flags & 3) == 2 || g_iocacheFileSlotCount <= 0)
		return 0;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == 0) {
			hasFree = 1;
			break;
		}
	}
	if (!hasFree)
		return 0;

	if (sceIoGetstat(path, &stat) != 0 || stat.st_size < 1)
		return 0;

	return (SceOff)g_iocacheFileSlotSize >= stat.st_size;
}

/* 0x8101f900 */
SceSSize iocacheFileRead(SceUID fd, void *buf, SceSize size)
{
	IoCacheFileSlot *slot;
	int slotIndex;
	SceOff curPos;
	SceOff endPos;
	SceOff copyLen;
	uint32_t actual;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == fd)
			break;
	}
	if (slotIndex >= g_iocacheFileSlotCount)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	slot = &g_iocacheFileSlots[slotIndex];
	curPos = slot->position;
	endPos = curPos + (SceOff)size;
	if (endPos > (SceOff)slot->fileSize)
		endPos = (SceOff)slot->fileSize;

	copyLen = endPos - curPos;
	if (copyLen <= 0) {
		actual = 0;
	} else {
		actual = (uint32_t)copyLen;
		memcpy(buf, slot->buffer + curPos, actual);
	}

	slot->position += (SceOff)actual;
	return (SceSSize)actual;
}

/* 0x8101f9d0 */
SceOff iocacheFileLseek(SceUID fd, SceOff offset, int whence)
{
	IoCacheFileSlot *slot;
	int slotIndex;
	SceOff nextPos;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == fd)
			break;
	}
	if (slotIndex >= g_iocacheFileSlotCount)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	slot = &g_iocacheFileSlots[slotIndex];
	switch (whence) {
	case SCE_SEEK_SET:
		nextPos = offset;
		break;
	case SCE_SEEK_CUR:
		nextPos = slot->position + offset;
		break;
	case SCE_SEEK_END:
		nextPos = (SceOff)slot->fileSize + offset;
		break;
	default:
		return -1;
	}

	if (nextPos < 0)
		nextPos = 0;
	slot->position = nextPos;
	return nextPos;
}

/* 0x8101fa58 */
int iocacheFileClose(SceUID fd)
{
	IoCacheFileSlot *slot;
	int slotIndex;

	for (slotIndex = 0; slotIndex < g_iocacheFileSlotCount; slotIndex++) {
		if (g_iocacheFileSlots[slotIndex].fd == fd)
			break;
	}
	if (slotIndex >= g_iocacheFileSlotCount)
		pspemuTriggerFatalError(PSPEMU_ERROR_FATAL);

	slot = &g_iocacheFileSlots[slotIndex];
	slot->position = 0;
	slot->fd = 0;
	slot->fileSize = 0;
	memset(slot->buffer, 0, g_iocacheFileSlotSize);
	return 0;
}
