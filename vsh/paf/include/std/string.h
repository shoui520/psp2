/* paf's C string functions (ScePafStdc). */
#ifndef PAF_STD_STRING_H
#define PAF_STD_STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *sce_paf_memchr(const void *ptr, int ch, size_t count);
int sce_paf_memcmp(const void *left, const void *right, size_t count);
void *sce_paf_memcpy(void *dst, const void *src, size_t count);
void *sce_paf_memmove(void *dst, const void *src, size_t count);
void *sce_paf_memset(void *dst, int ch, size_t count);
size_t sce_paf_strlen(const char *str);
int sce_paf_strcmp(const char *left, const char *right);
int sce_paf_strncmp(const char *left, const char *right, size_t num);
int sce_paf_strcasecmp(const char *left, const char *right);
int sce_paf_strncasecmp(const char *left, const char *right, size_t num);
char *sce_paf_strchr(const char *str, int ch);
char *sce_paf_strrchr(const char *str, int ch);
char *sce_paf_strcpy(char *dst, const char *src);
char *sce_paf_strncpy(char *dst, const char *src, size_t num);
char *sce_paf_strcat(char *dst, const char *src);
char *sce_paf_strncat(char *dst, const char *src, size_t num);
size_t sce_paf_strlcpy(char *dst, const char *src, size_t size);
size_t sce_paf_strlcat(char *dst, const char *src, size_t size);
char *sce_paf_strstr(const char *haystack, const char *needle);
char *sce_paf_strtok_r(char *str, const char *delimiters, char **save);
char *sce_paf_private_strtok(const char *haystack, const char *needle);
size_t sce_paf_wcslen(const wchar_t *str);
int sce_paf_wcscmp(const wchar_t *left, const wchar_t *right);
int sce_paf_wcsncmp(const wchar_t *left, const wchar_t *right, size_t num);
wchar_t *sce_paf_wcschr(const wchar_t *str, int ch);
wchar_t *sce_paf_wcsrchr(const wchar_t *str, int ch);
wchar_t *sce_paf_wcscpy(wchar_t *dst, const wchar_t *src);
wchar_t *sce_paf_wcsncpy(wchar_t *dst, const wchar_t *src, size_t num);
wchar_t *sce_paf_wmemcpy(wchar_t *dst, const wchar_t *src, size_t count);
wchar_t *sce_paf_wmemmove(wchar_t *dst, const wchar_t *src, size_t count);
wchar_t *sce_paf_wmemset(wchar_t *dst, int ch, size_t count);
int sce_paf_wmemcmp(const wchar_t *left, const wchar_t *right, size_t count);
wchar_t *sce_paf_wmemchr(const wchar_t *ptr, wchar_t ch, size_t count);

#ifdef __cplusplus
}
#endif

#endif
