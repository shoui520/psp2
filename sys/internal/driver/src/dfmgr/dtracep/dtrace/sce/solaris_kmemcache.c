/*
 * Solaris object caches (Sony's dtrace/sce/solaris_kmemcache.c): each object
 * comes from DTrace's heap at the cache's alignment.
 */
#include <dtrace/sce/solaris_kmemcache.h>
#include <dtrace/heap.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize, size_t align,
    int (*constructor)(void *, void *, int), void (*destructor)(void *, void *),
    void (*reclaim)(void *), void *private, void *vmp, int cflags)
{
	kmem_cache_t *cp;

	cp = dtrace_heap_zalloc(sizeof(*cp));
	SCE_DBG_ASSERT(cp != NULL, 20);

	strncpy(cp->name, name, sizeof(cp->name) - 1);
	cp->bufsize = bufsize;
	cp->align = align;
	cp->constructor = constructor;
	cp->destructor = destructor;
	cp->reclaim = reclaim;
	cp->private = private;
	cp->vmp = vmp;
	cp->cflags = cflags;
	return cp;
}

void kmem_cache_destroy(kmem_cache_t *cp)
{
	SCE_DBG_ASSERT(cp != NULL, 37);
	dtrace_heap_free(cp);
}

void *kmem_cache_alloc(kmem_cache_t *cp, int kmflags)
{
	void *buf;

	SCE_DBG_ASSERT(cp != NULL, 44);
	buf = dtrace_heap_alloc_aligned(cp->bufsize, cp->align);
	SCE_DBG_ASSERT(buf != NULL, 47);

	if (cp->constructor != NULL)
		cp->constructor(buf, cp->private, kmflags);
	return buf;
}

void kmem_cache_free(kmem_cache_t *cp, void *buf)
{
	SCE_DBG_ASSERT(cp != NULL, 57);

	if (cp->destructor != NULL)
		cp->destructor(buf, cp->private);
	dtrace_heap_free(buf);
}
