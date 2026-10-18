/*
 * kernel/libkernel.h: the user library's memory functions, under Sony's names (SDK Release
 * 03.570.011, target/include/kernel/libkernel.h). SceLibKernel exports them; its other C
 * library functions, which the SDK does not declare, are libkernel/libkernel.h's.
 */

#pragma once

#include <scetypes.h>
#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the _Chk forms take the destination's size and stop the process when the copy would overrun it */
void *sceClibMemcpyChk(void *dest, const void *src, SceSize size, SceSize destsize)
	__attribute__((__nonnull__));
void *sceClibMemcpy(void *dest, const void *src, SceSize size) __attribute__((__nonnull__));
void *sceClibMemmoveChk(void *dest, const void *src, SceSize size, SceSize destsize)
	__attribute__((__nonnull__));
void *sceClibMemmove(void *dest, const void *src, SceSize size) __attribute__((__nonnull__));
void *sceClibMemsetChk(void *dest, int c, SceSize n, SceSize destsize) __attribute__((__nonnull__));
void *sceClibMemset(void *dst, int c, SceSize size) __attribute__((__nonnull__));
int sceClibMemcmp(const void *s1, const void *s2, SceSize size)
	__attribute__((__nonnull__, __pure__));
int sceClibMemcmpConstTime(const void *s1, const void *s2, SceSize size)
	__attribute__((__nonnull__));
void *sceClibMemchr(const void *p, int c, SceSize n) __attribute__((__nonnull__, __pure__));

#ifdef __cplusplus
}
#endif
