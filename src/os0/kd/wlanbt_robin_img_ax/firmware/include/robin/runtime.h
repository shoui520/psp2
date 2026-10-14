/* Standard ARM run-time ABI helpers implemented by this firmware. */
#ifndef ROBIN_RUNTIME_H
#define ROBIN_RUNTIME_H

#include <stddef.h>

int __aeabi_uwrite4(int value, void *address);
/* RVCT library routine in ITCM (0x00052360); not converted. */
char *strcpy(char *destination, const char *source);
/* RVCT library routine in ITCM (0x00052234); not converted. */
int memcmp(const void *left, const void *right, size_t size);

/* RVCT library routine in ITCM (0x00052420); not converted. */
int strcmp(const char *left, const char *right);

#endif /* ROBIN_RUNTIME_H */
