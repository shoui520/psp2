/*
 * Solaris kernel memory (Sony's dtrace/sce/solaris_kmem.c), on DTrace's heap.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>
#include <dtrace/heap.h>

#define KM_SLEEP	0x0000	/* can block for memory; success guaranteed */
#define KM_NOSLEEP	0x0001	/* cannot block for memory; may fail */

void *kmem_alloc(size_t size, int kmflags);
/* Sony's checked string copies know the size of what it returns */
void *kmem_zalloc(size_t size, int kmflags) __attribute__((alloc_size(1)));
void *kmem_zalloc_nocheck(size_t size, int kmflags);
void kmem_free(const void *buf, size_t size);
