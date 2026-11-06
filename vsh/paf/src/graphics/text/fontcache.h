/*
 * The glyph cache the PVF library keeps its rendered glyphs in: the SDK's libpvf sample font
 * cache (sample_code/graphics/api_libpvf/fontcache), built into the PVF rasterizer. Names
 * Sony's, from the sample.
 */
#ifndef PAF_GRAPHICS_TEXT_FONTCACHE_H
#define PAF_GRAPHICS_TEXT_FONTCACHE_H

#include <font/libpvf.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	void *(*allocFunc)(int size);
	void (*freeFunc)(void *ptr);
	unsigned char *(*memcpyFunc)(unsigned char *dst, unsigned char *src, int size);
} myPvfCache_t_initRec;

void *myPvfCache_new(myPvfCache_t_initRec *pInitRec, int numCacheSlots);
ScePvf_t_cacheSystemInterface *myPvfCache_getCacheInterfaceHandle(void *_this_);
int myPvfCache_expireAll(void *_this_, int bCompulsion);
void myPvfCache_delete(void *_this_);

#ifdef __cplusplus
}
#endif

#endif
