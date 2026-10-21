/*
 * Block storage: a caller's buffer cut into blocks of one size, a bitmap of
 * the blocks in use at its end. Allocation searches round-robin from the
 * block after the last one handed out.
 */

#include <sysmem/sysclib.h>
#include "fios2_internal.h"

#define FIOS_STORAGE_BIT(i)	(1 << (7 - ((i) & 7)))

int sceFiosKernelStorageInit(SceFiosKernelStorage *storage, void *buffer, SceSize size,
			     SceSize maxBlocks, SceSize elementSize, SceSize alignment)
{
	SceSize blockSize = (elementSize - 1 + alignment) & -alignment;
	SceSize count;

	storage->blockSize = blockSize;
	storage->alignment = alignment;
	if (((uintptr_t)buffer & (alignment - 1)) != 0)
		return SCE_FIOS_ERROR_BAD_ALIGNMENT;
	if (buffer == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (size < blockSize + 1 || blockSize * maxBlocks < size)
		return SCE_FIOS_ERROR_BAD_SIZE;

	/* as many blocks as fit next to their bitmap */
	count = (size - (size / blockSize + 7) / 8) / blockSize;
	storage->count = count;
	storage->used = 0;
	storage->blocks = buffer;
	storage->generation = 1;
	storage->last = -1;
	storage->bitmap = (uint8_t *)buffer + blockSize * count;
	memset(buffer, 0, size);
	storage->initialized = 1;
	return 0;
}

/* count blocks in a row: the first one's index, or -1 */
int sceFiosKernelStorageAllocBlocks(SceFiosKernelStorage *storage, SceSize count)
{
	SceSize start, end, i, tried;
	SceSize last;

	if (!storage->initialized || storage->used + count > storage->count || storage->count == 0)
		return -1;

	last = storage->last;
	tried = 0;
	do {
		start = last + 1;
		end = start + count;
		if (end > storage->count) {
			/* wrap around */
			start = 0;
			end = count;
			if (++storage->generation == 0x7fff)
				storage->generation = 1;
		}
		for (i = start; i < end; i++)
			if (storage->bitmap[i >> 3] & FIOS_STORAGE_BIT(i))
				break;
		if (i >= end) {
			char *blocks = (char *)storage->blocks + storage->blockSize * start;

			for (i = start; i < end; i++)
				storage->bitmap[i >> 3] |= FIOS_STORAGE_BIT(i);
			storage->last = end - 1;
			storage->used += count;
			memset(blocks, 0, storage->blockSize * count);
			return start;
		}
		tried += i + 1 - start;
		last = i;
	} while (tried < storage->count);
	return -1;
}

void sceFiosKernelStorageFreeBlocks(SceFiosKernelStorage *storage, SceSize index, SceSize count)
{
	SceSize i;

	for (i = index; i < index + count; i++)
		storage->bitmap[i >> 3] &= ~FIOS_STORAGE_BIT(i);
	storage->used -= count;
}
