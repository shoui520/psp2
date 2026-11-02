/* SSLeay STACK with the measured SSL-C changes in stack_sce.patch. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crypto.h"
#include "../memory.h"

/* Adapter/helper names below are ours, not recovered Sony names. */
void sslSort(void *base, unsigned int count, unsigned int size,
	     int (*compare)(const void *, const void *));

static void *sslBinarySearch(const void *key, const void *base, int count,
			    int size, int (*compare)(const void *, const void *))
{
	int low = 0;
	int high = count - 1;
	int middle;
	int result;
	const char *element;

	while (high > low) {
		middle = (high + low) / 2;
		element = (const char *)base + middle * size;
		result = compare(key, element);
		if (result == 0)
			return (void *)element;
		if (result > 0)
			low = middle + 1;
		else
			high = middle - 1;
	}
	if (high != low)
		return NULL;
	element = (const char *)base + low * size;
	return compare(key, element) == 0 ? (void *)element : NULL;
}

#undef Malloc
#undef Realloc
#undef Free
#define Malloc(size) sslAllocate(size)
#define Realloc(ptr, size, old_size) sslReallocate(ptr, size, old_size)
#define Free(ptr) sslFree(ptr)
#define memcpy sslCopyMemory
#define memset sslFillMemory
#define qsort sslSort
#define bsearch sslBinarySearch

#include "stack_sce.inc"

/* SSL-C additions. All four descriptive names here are ours. */
int sslStackFindUnsorted(STACK *st, char *data)
{
	int (*compare)() = st->comp;
	int i;

	if (compare == NULL)
		return -1;
	for (i = 0; i < st->num; i++)
		if (compare(&st->data[i], &data) == 0)
			return i;
	return -1;
}

int sslStackMove(STACK *st, int loc, int toEnd)
{
	char *data;

	if (loc < 0 || loc >= st->num)
		return -1;
	data = st->data[loc];
	sk_delete(st, loc);
	return sk_insert(st, data, toEnd ? st->num : 0);
}

void sslStackClearElements(STACK *st, void (*func)())
{
	int i;

	if (st == NULL)
		return;
	for (i = st->num - 1; i >= 0; i--)
		if (st->data[i] != NULL)
			func(st->data[i]);
	sk_zero(st);
}

char *sslStackSetValue(STACK *st, int loc, char *data)
{
	if (st == NULL)
		return NULL;
	st->sorted = 0;
	st->data[loc] = data;
	return data;
}
