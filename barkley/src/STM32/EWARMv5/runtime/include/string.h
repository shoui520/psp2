/*
 * string.h: what the program uses of IAR's C library (DLib).
 */
#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memchr(const void *s, int c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
void *memset(void *dst, int c, size_t n);
char *strchr(const char *s, int c);
size_t strlen(const char *s);

#ifdef __cplusplus
}
#endif

#endif
