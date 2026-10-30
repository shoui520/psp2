/*
 * Solaris identifier arenas (Sony's dtrace/sce/solaris_vmem.c): DTrace only
 * allocates IDs from them, from a blist that doubles when it is full.
 */
#pragma once

#include <dtrace/sce/solaris_types.h>

#define VM_SLEEP	0x00000000
#define VM_NOSLEEP	0x00000001
#define VM_BESTFIT	0x00000100
#define VMC_IDENTIFIER	0x00040000	/* the only kind the arenas support */

struct blist;

typedef struct vmem {
	struct blist *blist;
	char name[30];
} vmem_t;

vmem_t *vmem_create(const char *name, void *base, size_t size, size_t quantum,
    void *afunc, void *ffunc, vmem_t *source, size_t qcache_max, int vmflag);
void vmem_destroy(vmem_t *vmp);
void *vmem_alloc(vmem_t *vmp, size_t size, int vmflag);
void vmem_free(vmem_t *vmp, void *vaddr, size_t size);
