#include "atrac_internal.h"

/* ---------------------------------------------------------------------- */
/* helpers                                                                */

void copyBytes(void *dst, const void *src, SceUInt32 size)
{
	SceUChar8 *d = dst;
	const SceUChar8 *s = src;

	while (size != 0) {
		*d++ = *s++;
		size--;
	}
}

/* bytes left when a difference is found, 0 when the ranges are equal */
SceUInt32 compareBytes(const void *a, const void *b, SceUInt32 size)
{
	const SceUChar8 *p = a;
	const SceUChar8 *q = b;

	do {
		if (*p++ != *q++)
			break;
	} while (--size != 0);
	return size;
}

SceUInt32 alignSizeUp(SceUInt32 size, SceUInt32 align)
{
	return (size + align - 1) & ~(align - 1);
}

/* no caller */
SceUInt32 alignSizeDown(SceUInt32 size, SceUInt32 align)
{
	return size & ~(align - 1);
}
