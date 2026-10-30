/*
 * Solaris kernel memory (Sony's dtrace/sce/solaris_kmem.c), on DTrace's heap.
 */
#include <dtrace/logging.h>
#include <dtrace/sce/solaris_kmem.h>
#include <sysmem/debug.h>

void *kmem_alloc(size_t size, int kmflags)
{
	void *buf;

	if (size == 0)
		return NULL;

	buf = dtrace_heap_alloc(size);
	if (buf == NULL)
		DTRACE_LOG_ERROR_AT(41, "%s - allocation of %d bytes failed", __func__, size);
	return buf;
}

void *kmem_zalloc(size_t size, int kmflags)
{
	void *buf;

	if (size == 0)
		return NULL;

	buf = dtrace_heap_zalloc(size);
	if (buf == NULL)
		DTRACE_LOG_ERROR_AT(67, "%s - allocation of %d bytes failed", __func__, size);
	return buf;
}

void *kmem_zalloc_nocheck(size_t size, int kmflags)
{
	if (size == 0)
		return NULL;

	return dtrace_heap_zalloc_nopanic(size);
}

void kmem_free(const void *buf, size_t size)
{
	if (buf == NULL && size == 0)
		return;

	SCE_DBG_ASSERT(size > 0, 88);
	dtrace_heap_free((void *)buf);
}
