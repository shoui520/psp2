#ifndef PAF_STD_STDLIB_H
#define PAF_STD_STDLIB_H

#include <stddef.h>

#define sce_paf_realloc ScePafStdc_0ADEA3AD
#define GetGlobalHeapAllocator ScePafStdc_261CAF72

void *GetGlobalHeapAllocator(void);
int ScePafStdc_D41FD265(void);

void *sce_paf_realloc(void *ptr, size_t size);
void *sce_paf_malloc(size_t size);
void sce_paf_free(void *ptr);
void *sce_paf_memalign(size_t align, size_t size);

#endif
