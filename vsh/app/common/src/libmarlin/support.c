/*
 * SceLibMarlin's support code linked after the SDK libraries: the heap's free list, time
 * comparison and conversion, and the decoding of a message's list of buffer pairs.
 */
#include "mln_internal.h"

static inline __attribute__((always_inline)) int MlnHeapHolds(const MlnHeap *heap,
								 const MlnHeapBlock *block)
{
	return heap->start <= block && block < heap->end;
}

/* puts a block into the heap's free list, which is kept in address order */
void MlnHeapAddFree(MlnHeap *heap, MlnHeapBlock *block)
{
	MlnHeapBlock *free = heap->rover;

	if (block < free) {
		block->freeNext = free;
		block->freePrev = NULL;
		free->freePrev = block;
		heap->rover = block;
		return;
	}
	while (MlnHeapHolds(heap, free)) {
		MlnHeapBlock *next = free->freeNext;

		if (free < block) {
			if (next > block) {
				block->freePrev = free;
				block->freeNext = next;
				if (MlnHeapHolds(heap, free->freeNext))
					free->freeNext->freePrev = block;
				free->freeNext = block;
				return;
			}
		}
		free = next;
	}
}

/* orders two times by their date and time of day: -1, 0 or 1 (4 for a missing one) */
int MlnImpl_81081400(const MlnDateTime *a, const MlnDateTime *b)
{
	if (a == NULL || b == NULL)
		return 4;
	if (a->year != b->year)
		return a->year > b->year ? 1 : -1;
	if (a->month != b->month)
		return a->month > b->month ? 1 : -1;
	if (a->day != b->day)
		return a->day > b->day ? 1 : -1;
	if (a->hour != b->hour)
		return a->hour > b->hour ? 1 : -1;
	if (a->minute != b->minute)
		return a->minute > b->minute ? 1 : -1;
	if (a->second != b->second)
		return a->second > b->second ? 1 : -1;
	return 0;
}

/* a time, taken as UTC, in minutes since 1970 */
int MlnImpl_81081498(const MlnDateTime *time, int *minutes)
{
	MlnDateTime utc;
	unsigned int value;

	utc.year = time->year;
	utc.month = time->month;
	utc.day = time->day;
	utc.hour = time->hour;
	utc.minute = time->minute;
	utc.second = time->second;
	utc.microsecond = time->microsecond;
	utc.utcOffset = 0;
	utc.daylightSaving = 0;
	if (MlnTimeToMinutes(&utc, &value) != 0)
		return 4;
	*minutes = value;
	return 0;
}

static inline __attribute__((always_inline)) unsigned int MlnMessageReadWord(const unsigned char *p)
{
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

/* a pair of buffers, each given by its size and its offset past the pair's header */
static inline __attribute__((always_inline)) int MlnMessageDecodePair(const unsigned char *p,
								       unsigned int left,
								       MlnBuffer ***out)
{
	MlnBuffer **buffers;
	unsigned int i;
	int error;

	*out = NULL;
	buffers = MlnAllocate(1, 2 * sizeof(*buffers));
	if (buffers == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(buffers, 0, 2 * sizeof(*buffers));
	for (i = 0; i < 2; i++) {
		unsigned int size = __builtin_bswap32(*(const unsigned int *)p), offset;

		if (left <= 3 || left - 4 <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		offset = MlnMessageReadWord(p + 4);
		p += 8;
		left -= 8;
		if (size > left) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto out;
		}
		if (size != 0) {
			buffers[i] = MlnBufferCreate(1, size, p + offset);
			if (buffers[i] == NULL) {
				error = MLN_ERROR_NO_MEMORY;
				goto out;
			}
		}
	}
	*out = buffers;
	buffers = NULL;
	error = MLN_OK;
out:
	MlnImpl_8102337C(2, buffers);
	return error;
}

/* a message's list of buffer pairs: their count, then each pair's size and offset */
int MlnImpl_8108150C(const unsigned char *p, unsigned int left, void **payload)
{
	MlnMessagePairs *pairs;
	MlnMessageList *item;
	MlnBuffer **buffers;
	unsigned int count, i, j;
	int error;

	count = MlnMessageReadWord(p);
	if (left <= 3)
		return MLN_MESSAGE_ERROR_FORMAT;
	pairs = MlnAllocate(1, sizeof(*pairs));
	if (pairs == NULL)
		return MLN_ERROR_NO_MEMORY;
	MlnMemset(pairs, 0, sizeof(*pairs));
	pairs->items = NULL;
	pairs->count = count;
	if (count == 0)
		goto done;
	if (0xffffffffU / count <= 3) {
		error = MLN_MESSAGE_ERROR_FORMAT;
		goto fail;
	}
	pairs->items = MlnAllocate(1, count * sizeof(*pairs->items));
	if (pairs->items == NULL) {
		error = MLN_ERROR_NO_MEMORY;
		goto fail;
	}
	p += 4;
	MlnMemset(pairs->items, 0, count * sizeof(*pairs->items));
	left -= 4;
	for (i = 0; i != count; i++) {
		unsigned int size = MlnMessageReadWord(p), offset;

		if (left <= 3 || left - 4 <= 3) {
			error = MLN_MESSAGE_ERROR_FORMAT;
			goto fail;
		}
		offset = MlnMessageReadWord(p + 4);
		p += 8;
		left -= 8;
		error = MlnMessageDecodePair(p + offset, size, &buffers);
		if (error != MLN_OK)
			goto failPair;
		item = MlnAllocate(1, sizeof(*item));
		if (item == NULL) {
			error = MLN_ERROR_NO_MEMORY;
			goto failPair;
		}
		MlnMemset(item, 0, sizeof(*item));
		item->count = 2;
		item->buffers = buffers;
		pairs->items[i] = item;
	}
done:
	*payload = pairs;
	return MLN_OK;
failPair:
	if (buffers != NULL) {
		MlnBufferDestroy(1, buffers[0]);
		MlnBufferDestroy(1, buffers[1]);
		MlnFree(1, buffers);
	}
fail:
	if (pairs->items != NULL) {
		MlnMessageList **items = pairs->items;

		for (i = 0; i != pairs->count; i++) {
			item = items[i];
			if (item == NULL)
				continue;
			if (item->buffers != NULL) {
				for (j = 0; j != item->count; j++)
					MlnBufferDestroy(1, item->buffers[j]);
				MlnFree(1, item->buffers);
			}
			MlnFree(1, items[i]);
		}
		MlnFree(1, items);
	}
	MlnFree(1, pairs);
	return error;
}
