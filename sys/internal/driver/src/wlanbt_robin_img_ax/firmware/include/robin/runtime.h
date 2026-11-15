/* Standard ARM run-time ABI helpers implemented by this firmware. */
#ifndef ROBIN_RUNTIME_H
#define ROBIN_RUNTIME_H

#include <stddef.h>

int __aeabi_uwrite4(int value, void *address);
/* RVCT library routines in ITCM (runtime/string.c, runtime/stringarm.c, runtime/rand.c). */
char *strcpy(char *destination, const char *source);
int memcmp(const void *left, const void *right, size_t size);

char *strncpy(char *destination, const char *source, size_t size);

int strcmp(const char *left, const char *right);

void srand(unsigned int seed);
int rand(void);
void robinInitHeapPool(void);

#endif /* ROBIN_RUNTIME_H */
