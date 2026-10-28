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

/* drm_cipher.c */
int SceSblGcAuthMgrDrmBBForDriver_48D7784E(GcAuthDrmBbMacContext *context,
		unsigned int mode);
int SceSblGcAuthMgrDrmBBForDriver_500F5157(GcAuthDrmBbMacContext *context,
		const unsigned char *data, size_t size);
int SceSblGcAuthMgrDrmBBForDriver_6EF3C9DB(GcAuthDrmBbMacContext *context,
		unsigned char *mac, const unsigned char *key);
int SceSblGcAuthMgrDrmBBForDriver_3C25F9FA(GcAuthDrmBbMacContext *context,
		const unsigned char *mac, const unsigned char *key);
int SceSblGcAuthMgrDrmBBForDriver_7F98E4E2(GcAuthDrmBbCipherContext *context,
		int mode, int generationMode, unsigned char *seed,
		const unsigned char *key, unsigned int counter);
int SceSblGcAuthMgrDrmBBForDriver_C0F37F18(GcAuthDrmBbCipherContext *context,
		unsigned char *data, size_t size);
int SceSblGcAuthMgrDrmBBForDriver_6E6D2B89(GcAuthDrmBbCipherContext *context);
