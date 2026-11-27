/*
 * scejpeg_work.c: 512 KiB EDRAM work-memory allocator for ScePspemu ME JPEG
 * decoding (0x8101e5e0..0x8101e7bb).
 */
#include "host_common.h"
#include <string.h>

#define PSPEMU_JPEG_WORK_EDRAM_OFFSET  0x00180000U
#define PSPEMU_JPEG_WORK_HEAP_SIZE     0x00080000U
#define PSPEMU_JPEG_WORK_ERR_INVALID   ((int)0x80000103)

typedef struct PspemuJpegWorkBlock {
	struct PspemuJpegWorkBlock *next;
	struct PspemuJpegWorkBlock *prev;
	uintptr_t                   inUseTag;
	uint32_t                    blockSize;
} PspemuJpegWorkBlock;

_Static_assert(sizeof(PspemuJpegWorkBlock) == 16, "PspemuJpegWorkBlock size");

static PspemuJpegWorkBlock *g_jpegWorkTail;
static PspemuJpegWorkBlock *g_jpegWorkHead;

void *sceJpegWorkReset(void)
{
	PspemuJpegWorkBlock *head = g_jpegWorkHead;

	g_jpegWorkTail = head;
	head->blockSize = PSPEMU_JPEG_WORK_HEAP_SIZE;
	head->next = NULL;
	head->prev = NULL;
	head->inUseTag = 0U;
	return head;
}

void *sceJpegWorkAlloc(uint32_t size)
{
	uint32_t needSize = ((size + 3U) & ~3U) + sizeof(PspemuJpegWorkBlock);
	PspemuJpegWorkBlock *blk = g_jpegWorkHead;

	do {
		if (blk->inUseTag == 0U && needSize <= blk->blockSize)
			break;
		blk = blk->next;
	} while (blk != NULL);
	if (blk == NULL)
		return NULL;

	{
		uint32_t remSize = blk->blockSize - needSize;
		blk->inUseTag = (uintptr_t)g_jpegWorkHead + 1U;
		if (remSize > sizeof(PspemuJpegWorkBlock)) {
			PspemuJpegWorkBlock *oldNext = blk->next;
			PspemuJpegWorkBlock *split = (PspemuJpegWorkBlock *)((uintptr_t)blk + needSize);

			blk->blockSize = needSize;
			split->prev = blk;
			split->next = oldNext;
			if (oldNext != NULL)
				oldNext->prev = split;
			split->inUseTag = 0U;
			split->blockSize = remSize;
			blk->next = split;
		}
	}

	if (blk > g_jpegWorkTail)
		g_jpegWorkTail = blk;
	return blk + 1;
}

int sceJpegWorkFree(void *hostPtr)
{
	PspemuJpegWorkBlock *blk;
	PspemuJpegWorkBlock *cur;
	PspemuJpegWorkBlock *prevBlk;
	PspemuJpegWorkBlock *nextBlk;
	uintptr_t validTag;

	if (hostPtr == NULL)
		return 0;

	blk = ((PspemuJpegWorkBlock *)hostPtr) - 1;
	validTag = (uintptr_t)g_jpegWorkHead + 1U;
	cur = g_jpegWorkTail;
	while (cur != blk || blk->inUseTag != validTag) {
		cur = cur->prev;
		if (cur == NULL)
			return PSPEMU_JPEG_WORK_ERR_INVALID;
	}

	prevBlk = blk->prev;
	if (prevBlk != NULL && prevBlk->inUseTag == 0U) {
		blk = prevBlk;
		nextBlk = prevBlk->next->next;
		prevBlk->blockSize += prevBlk->next->blockSize;
		prevBlk->next = nextBlk;
	} else {
		nextBlk = blk->next;
	}

	if (nextBlk != NULL) {
		if (nextBlk->inUseTag == 0U) {
			PspemuJpegWorkBlock *afterNext = nextBlk->next;
			blk->blockSize += nextBlk->blockSize;
			blk->next = afterNext;
			nextBlk = afterNext;
		}
	}

	if (nextBlk != NULL)
		nextBlk->prev = blk;
	else
		g_jpegWorkTail = blk->prev;

	blk->inUseTag = 0U;
	return 0;
}

int sceJpegWorkGetFreeSize(void)
{
	PspemuJpegWorkBlock *cur = g_jpegWorkHead;
	uintptr_t validTag = (uintptr_t)g_jpegWorkHead + 1U;

	for (;;) {
		PspemuJpegWorkBlock *nextBlk;
		uintptr_t tag = cur->inUseTag;

		if (tag != validTag && tag != 0U)
			*(volatile uint32_t *)1U = 1U;
		nextBlk = cur->next;
		if (nextBlk == NULL)
			return 0;
		if (nextBlk->prev != cur)
			*(volatile uint32_t *)3U = 1U;
		if ((uintptr_t)nextBlk - (uintptr_t)cur != cur->blockSize)
			*(volatile uint32_t *)5U = 1U;
		cur = nextBlk;
	}
}

void *sceJpegWorkAllocZero(uint32_t count, uint32_t elemSize)
{
	uint64_t total = (uint64_t)count * (uint64_t)elemSize;
	uint32_t alignedSize;
	void *ptr;

	if ((total >> 32) != 0U)
		return NULL;
	alignedSize = ((uint32_t)total + 3U) & ~3U;
	ptr = sceJpegWorkAlloc(alignedSize);
	if (ptr == NULL)
		return NULL;
	return memset(ptr, 0, alignedSize);
}

void *sceJpegWorkInit(void *edramBase)
{
	PspemuJpegWorkBlock *head = (PspemuJpegWorkBlock *)((uintptr_t)edramBase + PSPEMU_JPEG_WORK_EDRAM_OFFSET);

	g_jpegWorkHead = head;
	g_jpegWorkTail = head;
	return head;
}

int sceJpegWorkHostToGuest(void *hostPtr, uint32_t *outGuestAddr)
{
	uintptr_t offset;

	if (hostPtr == NULL) {
		*outGuestAddr = 0U;
		return 0;
	}
	offset = (uintptr_t)hostPtr - (uintptr_t)g_jpegWorkHead;
	if (offset >= PSPEMU_JPEG_WORK_HEAP_SIZE)
		return -1;
	*outGuestAddr = (uint32_t)offset + PSPEMU_JPEG_WORK_EDRAM_OFFSET;
	return 0;
}

int sceJpegWorkGuestToHost(uint32_t guestAddr, void **outHostPtr)
{
	uint32_t offset;

	if (guestAddr == 0U) {
		*outHostPtr = NULL;
		return 0;
	}
	offset = guestAddr - PSPEMU_JPEG_WORK_EDRAM_OFFSET;
	if (offset >= PSPEMU_JPEG_WORK_HEAP_SIZE)
		return -1;
	*outHostPtr = (void *)((uintptr_t)g_jpegWorkHead + offset);
	return 0;
}
