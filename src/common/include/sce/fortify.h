/*
 * _FORTIFY_SOURCE as Sony's fortified builds have it: where the compiler
 * knows the size of the destination, a copy calls the checked routine
 * (__memcpy_chk) with that size; elsewhere it stays a plain call. newlib's
 * <ssp/string.h> does the same, but its prototypes clash with the kernel
 * headers'. A module includes this after its other headers, under the
 * condition that selects the fortified builds.
 *
 * Only memcpy so far: wrapping memset/memmove as well moved syscon's
 * inlining and frames away from the original's.
 */
#pragma once

#undef memcpy
#define memcpy(dst, src, n) \
	__builtin___memcpy_chk(dst, src, n, __builtin_object_size(dst, 0))
