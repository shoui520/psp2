/*
 * The platform's session id from the secure clock, and Octopus objects written as their canonical byte
 * sequences: an array as its count, big-endian, and then its items in their sorted order
 * (0x8105F2C4-). The file's name is ours.
 */
#include "mln_internal.h"

#define MLN_CANONICAL_ERROR_CLOCK	0x230E0001

/* 1970-01-01 00:00:00 in RTC ticks */
#define MLN_UNIX_EPOCH_TICK		62135596800000000ULL
#define MLN_SECOND_TICKS		1000000

/* an object sorted by its first or its second field */
typedef struct MlnCanonicalItem {
	const MlnBuffer *first;
	const MlnBuffer *second;
} MlnCanonicalItem;

/*
 * The platform's session: its id the secure clock in seconds since 1970-01-01, its flags
 * none.
 */
int MlnImpl_8105F2C4(int *id, unsigned int *flags)
{
	unsigned long long tick;

	if (sceDrmBridgeGetCurrentSecureTick(&tick) != 0)
		return MLN_CANONICAL_ERROR_CLOCK;
	tick = (tick - MLN_UNIX_EPOCH_TICK) / MLN_SECOND_TICKS;
	*flags = 0;
	*id = tick;
	return 0;
}

int MlnImpl_8105F334(void)
{
	return 0;
}

/* the orders of the arrays' items */
int MlnImpl_8105F33C(const MlnCanonicalItem *a, const MlnCanonicalItem *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, a->second, b->second);
}

int MlnImpl_8105F358(const MlnCanonicalItem *a, const MlnCanonicalItem *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, a->first, b->first);
}

int MlnImpl_8105F374(const MlnCanonicalItem *a, const MlnCanonicalItem *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, a->first, b->first);
}

int MlnImpl_8105F390(const MlnCanonicalItem *a, const MlnCanonicalItem *b)
{
	return MlnBufferCompare(MLN_HEAP_GENERAL, a->first, b->first);
}

/* write an array: its count, then each item in the order compare sorts them */
int MlnImpl_8105F3AC(const MlnOctopusArray *array, MlnCanonicalCompare compare,
		     MlnCanonicalWrite write, MlnTextBuffer *out)
{
	unsigned char count[4];
	int *order;
	int n = array->count;
	int min;
	int ret;
	int t;
	int i;
	int j;

	count[0] = n >> 24;
	count[1] = n >> 16;
	count[2] = n >> 8;
	count[3] = n;
	ret = MlnTextBufferAppend(MLN_HEAP_GENERAL, out, sizeof(count), count);
	if (ret != 0)
		return ret;
	if (n <= 1)
		return write(array->items[0], out);

	order = MlnAllocate(MLN_HEAP_GENERAL, n * sizeof(int));
	if (order == NULL)
		return 2;
	for (i = 0; i < n; i++)
		order[i] = i;
	for (i = 0; i < n - 1; i++) {
		min = i;
		for (j = i + 1; j < n; j++) {
			if (compare(array->items[order[min]], array->items[order[j]]) > 0)
				min = j;
		}
		if (min != i) {
			t = order[min];
			order[min] = order[i];
			order[i] = t;
		}
		ret = write(array->items[order[i]], out);
		if (ret != 0)
			goto out;
	}
	ret = write(array->items[order[n - 1]], out);
out:
	MlnFree(MLN_HEAP_GENERAL, order);
	return ret;
}
