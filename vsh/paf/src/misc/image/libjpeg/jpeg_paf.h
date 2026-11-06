/*
 * What libpaf adds to libjpeg (names ours): the source managers the JPEG codec reads through
 * (jdatasrc_paf.c) and the allocations jmemnobs makes from the paf heap the client data names,
 * or from the global heap when it names none (jmem_paf.cpp).
 */
#ifndef PAF_IMAGE_JPEG_PAF_H
#define PAF_IMAGE_JPEG_PAF_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Sony's source managers: the codec's source, a paf File read at its position or a buffer
 * read from its count of bytes read.
 */
int jpeg_paf_file_src(j_decompress_ptr cinfo, void *source);
int jpeg_paf_mem_src(j_decompress_ptr cinfo, void *source);

/* heap is a paf::memory::HeapAllocator, or NULL for the global heap */
void *jpeg_paf_malloc(size_t size, void *heap);
void *jpeg_paf_memalign(size_t align, size_t size, void *heap);
void jpeg_paf_free(void *ptr, void *heap);

#ifdef __cplusplus
}
#endif

#endif
