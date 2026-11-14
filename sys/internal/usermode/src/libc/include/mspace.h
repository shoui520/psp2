/*
 * mspace.h: the SDK's independent heaps (SDK Release 03.570.011, target/include/mspace.h).
 */
#ifndef _MSPACE
#define _MSPACE

#include <stddef.h>
#include <stdlib.h>	/* struct malloc_managed_size */

#define SCE_LIBC_MSPACE_DETECT_OVERRUN	(0x00000008U)

#ifdef __cplusplus
extern "C" {
#endif

typedef void *mspace;

mspace mspace_create(void *base, size_t capacity);
mspace mspace_create_with_flag(void *base, size_t capacity, unsigned int flag);
int mspace_destroy(mspace msp);
void *mspace_malloc(mspace msp, size_t size);
void mspace_free(mspace msp, void *ptr);
void *mspace_calloc(mspace msp, size_t nelem, size_t size);
void *mspace_memalign(mspace msp, size_t boundary, size_t size);
void *mspace_realloc(mspace msp, void *ptr, size_t size);
void *mspace_reallocalign(mspace msp, void *ptr, size_t size, size_t boundary);
size_t mspace_malloc_usable_size(void *p);
int mspace_malloc_stats(mspace msp, struct malloc_managed_size *mmsize);
int mspace_malloc_stats_fast(mspace msp, struct malloc_managed_size *mmsize);
int mspace_is_heap_empty(mspace msp);

#ifdef __cplusplus
}
#endif

#endif /* _MSPACE */
