/*
 * Solaris object caches (Sony's dtrace/sce/solaris_kmemcache.c): a cache
 * allocates each object from DTrace's heap at its alignment and runs the
 * constructor and destructor; it keeps nothing.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/sce/solaris_vmem.h>

typedef struct kmem_cache {
	char name[32];
	size_t bufsize;
	size_t align;
	int (*constructor)(void *buf, void *arg, int kmflags);
	void (*destructor)(void *buf, void *arg);
	void (*reclaim)(void *arg);
	void *private;
	void *vmp;
	int cflags;
} kmem_cache_t;

kmem_cache_t *kmem_cache_create(const char *name, size_t bufsize, size_t align,
    int (*constructor)(void *, void *, int), void (*destructor)(void *, void *),
    void (*reclaim)(void *), void *private, void *vmp, int cflags);
void kmem_cache_destroy(kmem_cache_t *cp);
void *kmem_cache_alloc(kmem_cache_t *cp, int kmflags);
void kmem_cache_free(kmem_cache_t *cp, void *buf);
