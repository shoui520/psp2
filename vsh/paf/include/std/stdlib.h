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
void *sce_paf_realloc(void *ptr, size_t size);
/* vitasdk: sce_paf_realloc */
void *ScePafStdc_0ADEA3AD(void *ptr, size_t size);
/* vitasdk: sce_paf_calloc */
void *ScePafStdc_C9532982(size_t nitems, size_t size);
int sce_paf_abs(int value);
int sce_paf_atoi(const char *str);
long long sce_paf_atoll(const char *str);
int sce_paf_printf_dummy(void);
int sce_paf_wprintf_dummy(void);
long sce_paf_strtol(const char *str, char **endptr, int base);
unsigned long sce_paf_strtoul(const char *str, char **endptr, int base);
long long sce_paf_strtoll(const char *str, char **endptr, int base);
double sce_paf_strtod(const char *str, char **endptr);
double sce_paf_atof(const char *str);
int sce_paf_rand(void);
void sce_paf_srand(unsigned int seed);
/* exit(): the module's exit, the atexit handlers, then the process's */
void sce_paf_private_exit(int status);
/* name unknown: reports a failed allocation in the paf templates and breaks */
int ScePafStdc_D608246A(void);

#ifdef __cplusplus
}
#endif

#endif
