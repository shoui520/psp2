/*
 * Copies of fixed-size records (Sony's util.h).
 */
#pragma once

#include <stdint.h>
#include "ulobjmgr_internal.h"

#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "util.h"

/* a 32-byte name; the last byte always becomes NUL */
static inline __attribute__((always_inline))
void copy32bWithNullTermination(char *dst, const char *src)
{
	SceUInt32 *d = (SceUInt32 *)dst;
	const SceUInt32 *s = (const SceUInt32 *)src;

	SCE_DBG_ASSERT(dst != NULL, 31);
	SCE_DBG_ASSERT(src != NULL, 32);
	SCE_DBG_ASSERT(((uintptr_t)dst & 3) == 0, 33);
	SCE_DBG_ASSERT(((uintptr_t)src & 3) == 0, 34);
	d[0] = s[0];
	d[1] = s[1];
	d[2] = s[2];
	d[3] = s[3];
	d[4] = s[4];
	d[5] = s[5];
	d[6] = s[6];
	d[7] = s[7] & 0x00ffffff;
}

static inline __attribute__((always_inline))
void copy64b(SceUInt64 *dst, const SceUInt64 *src)
{
	SCE_DBG_ASSERT(dst != NULL, 48);
	SCE_DBG_ASSERT(src != NULL, 49);
	SCE_DBG_ASSERT(((uintptr_t)dst & 7) == 0, 50);
	SCE_DBG_ASSERT(((uintptr_t)src & 7) == 0, 51);
	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
}

#pragma pop_macro("__FILE_NAME__")
