/* Shared DRM BB contexts; original internal type names are unknown. */
#pragma once

#include <stddef.h>

typedef struct GcAuthDrmBbMacContext {
	unsigned int mode;
	unsigned char chainingValue[16];
	unsigned char tail[16];
	unsigned int tailSize;
} GcAuthDrmBbMacContext;

_Static_assert(sizeof(GcAuthDrmBbMacContext) == 40, "DRM BB MAC context ABI");
_Static_assert(offsetof(GcAuthDrmBbMacContext, chainingValue) == 4,
	       "DRM BB MAC chaining value offset");
_Static_assert(offsetof(GcAuthDrmBbMacContext, tail) == 20,
	       "DRM BB MAC tail offset");
_Static_assert(offsetof(GcAuthDrmBbMacContext, tailSize) == 36,
	       "DRM BB MAC tail size offset");

typedef struct GcAuthDrmBbCipherContext {
	unsigned int mode;
	unsigned int counter;
	unsigned char seed[16];
} GcAuthDrmBbCipherContext;

_Static_assert(sizeof(GcAuthDrmBbCipherContext) == 24, "DRM BB cipher context ABI");
_Static_assert(offsetof(GcAuthDrmBbCipherContext, counter) == 4,
	       "DRM BB cipher counter offset");
_Static_assert(offsetof(GcAuthDrmBbCipherContext, seed) == 8,
	       "DRM BB cipher seed offset");
