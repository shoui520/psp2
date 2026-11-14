/*
 * The C library's heap: malloc and its family call the program's replacements
 * (user_malloc ...) when its process parameters give a complete set, else the default heap,
 * an mspace in the library's own data.
 */
#include <stddef.h>
#include <stdlib.h>
#include <mspace.h>
#include <libkernel/libkernel.h>
#include <processmgr/processmgr.h>

#include "libc_internal.h"

static unsigned int heapInitError;	/* what the heap's initializer reports */

/* the default heap's state (dlmalloc's, laid out by _HeapInit) */
static unsigned long long heap[0x238 / sizeof(unsigned long long)];

/* the TLS blocks' allocator (ldtls.c) and the replacement's initializer and finalizer */
void (*_FreeForTls)(void *ptr);
void *(*_MallocForTls)(size_t size);
static void (*mallocForTlsInit)(void);
static int (*mallocForTlsFinalize)(void);	/* SCE: _MallocTerm returns what it returns */

/* the allocator: the replacement's functions, or the default heap's */
static void (*mallocInit)(void);
static void (*mallocFinalize)(void);
static void *(*mallocFunc)(size_t size);
static void (*freeFunc)(void *ptr);
static void *(*callocFunc)(size_t nelem, size_t size);
static void *(*reallocFunc)(void *ptr, size_t size);
static void *(*memalignFunc)(size_t boundary, size_t size);
static void *(*reallocalignFunc)(void *ptr, size_t size, size_t boundary);
static int (*mallocStatsFunc)(struct malloc_managed_size *mmsize);
static int (*mallocStatsFastFunc)(struct malloc_managed_size *mmsize);
static size_t (*mallocUsableSizeFunc)(void *ptr);

static void defaultMallocInit(void);
static void defaultMallocFinalize(void);
static void *defaultMalloc(size_t size);
static void defaultFree(void *ptr);
static void *defaultCalloc(size_t nelem, size_t size);
static void *defaultRealloc(void *ptr, size_t size);
static void *defaultMemalign(size_t boundary, size_t size);
static void *defaultReallocalign(void *ptr, size_t size, size_t boundary);
static int defaultMallocStats(struct malloc_managed_size *mmsize);
static int defaultMallocStatsFast(struct malloc_managed_size *mmsize);
static size_t defaultMallocUsableSize(void *ptr);

void sceLibcSetHeapInitError(unsigned int err_number)
{
	heapInitError = err_number;
}

int _MallocInit(void)
{
	const SceLibcMallocReplace *replace = NULL;
	const void *newReplace = NULL;
	const SceLibcMallocReplaceForTls *tls = NULL;
	const SceProcessParam *param = sceKernelGetProcessParam();
	int res;

	if (param != NULL && 5 <= param->version) {
		const SceLibcParam *libc = param->sceLibcParam;

		if (libc != NULL && 0x28 <= libc->size) {
			replace = libc->_sceLibcMallocReplace;
			newReplace = libc->_sceLibcNewReplace;
			if (0x38 <= libc->size)
				tls = libc->_sceLibcMallocReplaceForTls;
		}
	}

	res = 0;
	if (tls != NULL && (tls->user_malloc_for_tls_init != NULL
			    || tls->user_malloc_for_tls_finalize != NULL
			    || tls->user_malloc_for_tls != NULL || tls->user_free_for_tls != NULL)) {
		if (tls->user_malloc_for_tls_init != NULL && tls->user_malloc_for_tls_finalize != NULL
		    && tls->user_malloc_for_tls != NULL && tls->user_free_for_tls != NULL) {
			mallocForTlsInit = tls->user_malloc_for_tls_init;
			mallocForTlsFinalize = (int (*)(void))tls->user_malloc_for_tls_finalize;
			_MallocForTls = tls->user_malloc_for_tls;
			_FreeForTls = tls->user_free_for_tls;
		} else {	/* an incomplete set */
			sceClibPrintf("SceLibc malloc_for_tls replace failed.\n");
			res = -1;
		}
	} else {
		mallocForTlsInit = NULL;
		mallocForTlsFinalize = NULL;
		_MallocForTls = malloc;
		_FreeForTls = free;
	}
	if (res < 0)
		return res;

	res = 0;
	if (replace != NULL && (replace->user_malloc_init != NULL
				|| replace->user_malloc_finalize != NULL
				|| replace->user_malloc != NULL || replace->user_free != NULL
				|| replace->user_calloc != NULL || replace->user_realloc != NULL
				|| replace->user_memalign != NULL
				|| replace->user_reallocalign != NULL
				|| replace->user_malloc_stats != NULL
				|| replace->user_malloc_stats_fast != NULL
				|| replace->user_malloc_usable_size != NULL)) {
		if (replace->user_malloc_init != NULL && replace->user_malloc_finalize != NULL
		    && replace->user_malloc != NULL && replace->user_free != NULL
		    && replace->user_calloc != NULL && replace->user_realloc != NULL
		    && replace->user_memalign != NULL && replace->user_reallocalign != NULL
		    && replace->user_malloc_stats != NULL && replace->user_malloc_stats_fast != NULL
		    && replace->user_malloc_usable_size != NULL) {
			mallocInit = replace->user_malloc_init;
			mallocFinalize = replace->user_malloc_finalize;
			mallocFunc = replace->user_malloc;
			freeFunc = replace->user_free;
			callocFunc = replace->user_calloc;
			reallocFunc = replace->user_realloc;
			memalignFunc = replace->user_memalign;
			reallocalignFunc = replace->user_reallocalign;
			mallocStatsFunc = replace->user_malloc_stats;
			mallocStatsFastFunc = replace->user_malloc_stats_fast;
			mallocUsableSizeFunc = replace->user_malloc_usable_size;
		} else {	/* an incomplete set */
			sceClibPrintf("SceLibc malloc replace failed.\n");
			res = -1;
		}
	} else {	/* SCE: the default heap also serves the TLS blocks, replaced or not */
		mallocForTlsInit = NULL;
		mallocForTlsFinalize = NULL;
		mallocInit = defaultMallocInit;
		mallocFinalize = defaultMallocFinalize;
		mallocFunc = defaultMalloc;
		_MallocForTls = defaultMalloc;
		freeFunc = defaultFree;
		_FreeForTls = defaultFree;
		callocFunc = defaultCalloc;
		reallocFunc = defaultRealloc;
		memalignFunc = defaultMemalign;
		reallocalignFunc = defaultReallocalign;
		mallocStatsFunc = defaultMallocStats;
		mallocStatsFastFunc = defaultMallocStatsFast;
		mallocUsableSizeFunc = defaultMallocUsableSize;
	}
	if (res < 0)
		return res;

	if (_InitNewReplace != NULL)
		_InitNewReplace(newReplace);
	if (mallocForTlsInit != NULL) {
		heapInitError = 0;
		mallocForTlsInit();
		if (heapInitError != 0)
			return heapInitError;
	}
	heapInitError = 0;
	mallocInit();
	if (heapInitError != 0)
		return heapInitError;
	return res;
}

int _MallocTerm(void)
{
	mallocFinalize();
	if (mallocForTlsFinalize != NULL)
		return mallocForTlsFinalize();
	return 0;
}

void *malloc(size_t size)
{
	return mallocFunc(size);
}

void free(void *ptr)
{
	freeFunc(ptr);
}

void *calloc(size_t nelem, size_t size)
{
	return callocFunc(nelem, size);
}

void *realloc(void *ptr, size_t size)
{
	return reallocFunc(ptr, size);
}

void *memalign(size_t boundary, size_t size)
{
	return memalignFunc(boundary, size);
}

int malloc_stats(struct malloc_managed_size *mmsize)
{
	return mallocStatsFunc(mmsize);
}

int malloc_stats_fast(struct malloc_managed_size *mmsize)
{
	return mallocStatsFastFunc(mmsize);
}

void *reallocalign(void *ptr, size_t size, size_t boundary)
{
	return reallocalignFunc(ptr, size, boundary);
}

size_t malloc_usable_size(void *ptr)
{
	return mallocUsableSizeFunc(ptr);
}

static void defaultMallocInit(void)
{
	heapInitError = _HeapInit(heap);
}

static void defaultMallocFinalize(void)
{
	_HeapTerm();
}

static void *defaultMalloc(size_t size)
{
	return mspace_malloc(heap, size);
}

static void defaultFree(void *ptr)
{
	mspace_free(heap, ptr);
}

static void *defaultCalloc(size_t nelem, size_t size)
{
	return mspace_calloc(heap, nelem, size);
}

static void *defaultRealloc(void *ptr, size_t size)
{
	return mspace_realloc(heap, ptr, size);
}

static void *defaultMemalign(size_t boundary, size_t size)
{
	return mspace_memalign(heap, boundary, size);
}

static void *defaultReallocalign(void *ptr, size_t size, size_t boundary)
{
	return mspace_reallocalign(heap, ptr, size, boundary);
}

static int defaultMallocStats(struct malloc_managed_size *mmsize)
{
	return mspace_malloc_stats(heap, mmsize);
}

static int defaultMallocStatsFast(struct malloc_managed_size *mmsize)
{
	return mspace_malloc_stats_fast(heap, mmsize);
}

static size_t defaultMallocUsableSize(void *ptr)
{
	return mspace_malloc_usable_size(ptr);
}
