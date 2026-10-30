/*
 * DTrace's own kernel heap (Sony's heap.c): every allocation DTrace makes
 * comes from it, and a failed allocation panics.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

int dtrace_heap_init(void);
int dtrace_heap_fini(void);
void *dtrace_heap_alloc(size_t size);
void *dtrace_heap_alloc_aligned(size_t size, size_t alignment);
void *dtrace_heap_zalloc(size_t size);
void *dtrace_heap_zalloc_nopanic(size_t size);
void dtrace_heap_free(void *ptr);
int dtrace_heap_contains(const void *ptr, size_t size);
