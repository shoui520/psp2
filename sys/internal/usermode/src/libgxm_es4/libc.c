/*
 * SceGxm: the C library functions the compiler calls, on SceSysclib.
 */

#include "gxm_internal.h"

void *memmove(void *dest, const void *src, SceSize size)
{
	return sceClibMemmove(dest, src, size);
}

void *memset(void *dest, int value, SceSize size)
{
	return sceClibMemset(dest, value, size);
}
