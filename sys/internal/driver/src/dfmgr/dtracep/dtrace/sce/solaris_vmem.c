/*
 * Solaris resource maps (Sony's dtrace/sce/solaris_vmem.c), as Apple's DTrace
 * glue has them: an arena of IDs is a block list, doubled when it runs out.
 */
#include <dtrace/bsd/blist.h>
#include <dtrace/heap.h>
#include <dtrace/sce/solaris_vmem.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>

#define VMEM_INITIAL_SIZE	128	/* the block list is preallocated: start small */

vmem_t *vmem_create(const char *name, void *base, size_t size, size_t quantum,
    void *afunc, void *ffunc, vmem_t *source, size_t qcache_max, int vmflag)
{
	vmem_t *vmp;

	SCE_DBG_ASSERT(quantum == 1, 43);
	SCE_DBG_ASSERT(afunc == NULL, 44);
	SCE_DBG_ASSERT(ffunc == NULL, 45);
	SCE_DBG_ASSERT(source == NULL, 46);
	SCE_DBG_ASSERT(qcache_max == 0, 47);
	SCE_DBG_ASSERT(vmflag & VMC_IDENTIFIER, 48);

	if (size > VMEM_INITIAL_SIZE)
		size = VMEM_INITIAL_SIZE;

	vmp = dtrace_heap_alloc(sizeof(*vmp));
	memset(vmp, 0, sizeof(*vmp));

	vmp->blist = blist_create(size);
	blist_free(vmp->blist, 0, size);
	if (base != NULL)
		blist_alloc(vmp->blist, (daddr_t)(uintptr_t)base);	/* the initial IDs */

	snprintf(vmp->name, sizeof(vmp->name), "%s", name);
	return vmp;
}

void vmem_destroy(vmem_t *vmp)
{
	SCE_DBG_ASSERT(vmp != NULL, 70);

	blist_destroy(vmp->blist);
	dtrace_heap_free(vmp);
}

void *vmem_alloc(vmem_t *vmp, size_t size, int vmflag)
{
	blist_t bl;
	daddr_t p;

	SCE_DBG_ASSERT(vmp != NULL, 83);
	SCE_DBG_ASSERT(size != 0, 84);

	bl = vmp->blist;
	p = blist_alloc(bl, size);
	if (p == SWAPBLK_NONE) {
		blist_resize(&bl, bl->bl_blocks << 1, 1);
		vmp->blist = bl;
		p = blist_alloc(bl, size);
		if (p == SWAPBLK_NONE)
			SCE_PANIC_AT(96);
	}
	return (void *)(uintptr_t)p;
}

void vmem_free(vmem_t *vmp, void *vaddr, size_t size)
{
	SCE_DBG_ASSERT(vmp != NULL, 114);

	blist_free(vmp->blist, (daddr_t)(uintptr_t)vaddr, size);
}
