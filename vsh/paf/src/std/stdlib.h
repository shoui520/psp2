#ifndef PAF_STD_STDLIB_H
#define PAF_STD_STDLIB_H

#include <stddef.h>
#include <stdint.h>

int sce_paf_abs(int value);
long sce_paf_strtol(const char *str, char **endptr, int base);
unsigned long sce_paf_strtoul(const char *str, char **endptr, int base);
long long sce_paf_strtoll(const char *str, char **endptr, int base);
unsigned long long sce_paf_strtoull(const char *str, char **endptr, int base);
double sce_paf_atof(const char *str);
double sce_paf_strtod(const char *str, char **endptr);
int sce_paf_atoi(const char *str);
long sce_paf_atol(const char *str);
char *sce_paf_atob(const char *str, int *value);
long long sce_paf_atoll(const char *str);
void *sce_paf_bsearch(const void *key, const void *base, size_t count,
		      size_t element_size,
		      int (*compare)(const void *element, const void *key));
void *ScePafStdc_E8BD0273(void *base, size_t element_size,
		int (*compare)(const void *, const void *),
		int32_t first, int32_t last);
void sce_paf_qsort(void *base, size_t count, size_t element_size,
		int (*compare)(const void *, const void *));
void sce_paf_srand(unsigned int seed);
int sce_paf_rand(void);


void *GetGlobalHeapAllocator(void);
int sce_paf_printf_dummy(void);
void abort(void);
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
