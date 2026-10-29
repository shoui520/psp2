#ifndef PAF_STD_STDLIB_H
#define PAF_STD_STDLIB_H

#include <stddef.h>
#include <stdint.h>

int sce_paf_abs(int value);

#define sce_paf_realloc ScePafStdc_0ADEA3AD
#define GetGlobalHeapAllocator ScePafStdc_261CAF72
#define sce_paf_calloc ScePafStdc_C9532982
#define sce_paf_mtrim ScePafStdc_5FBF2BA6
#define sce_paf_musable_size ScePafStdc_F74EEC17
#define sce_paf_dump_heap_info ScePafStdc_FA862AE4
#define sce_paf_dump_heap_status ScePafStdc_5A8F4CA9
#define sce_paf_report_memoryblocks ScePafStdc_B8FFD92C
#define sce_paf_check_memorybounds ScePafStdc_BE6601A7

void *GetGlobalHeapAllocator(void);
int ScePafStdc_D41FD265(void);
int ScePafStdc_D608246A(void);

void *sce_paf_realloc(void *ptr, size_t size);
void *sce_paf_malloc(size_t size);
void sce_paf_free(void *ptr);
void *sce_paf_memalign(size_t align, size_t size);
void *sce_paf_calloc(size_t nitems, size_t size);
void sce_paf_mtrim(void);
size_t sce_paf_musable_size(const void *ptr);
void sce_paf_dump_heap_info(int32_t sort_policy);
void sce_paf_dump_heap_status(void);
int32_t sce_paf_report_memoryblocks(
		int (*callback)(void *, size_t, int, void *), void *userdata);
int32_t sce_paf_check_memorybounds(void);

#endif
