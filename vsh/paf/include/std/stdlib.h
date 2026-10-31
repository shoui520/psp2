/* paf's C library allocation and runtime hooks (ScePafStdc). */
#ifndef PAF_STD_STDLIB_H
#define PAF_STD_STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *sce_paf_malloc(size_t size);
void *sce_paf_memalign(size_t align, size_t size);
void sce_paf_free(void *ptr);
/* vitasdk: sce_paf_realloc */
void *ScePafStdc_0ADEA3AD(void *ptr, size_t size);
/* vitasdk: sce_paf_calloc */
void *ScePafStdc_C9532982(size_t nitems, size_t size);
int sce_paf_printf_dummy(void);
unsigned long sce_paf_strtoul(const char *str, char **endptr, int base);
/* name unknown: reports a failed allocation in the paf templates and breaks */
int ScePafStdc_D608246A(void);

#ifdef __cplusplus
}
#endif

#endif
