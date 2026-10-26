/*
 * stdlib.h: what the program uses of IAR's C library (DLib).
 */
#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *malloc(size_t size);
void free(void *p);

#ifdef __cplusplus
}
#endif

#endif
