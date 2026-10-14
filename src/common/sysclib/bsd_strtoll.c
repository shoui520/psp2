#include <sys/cdefs.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef __FBSDID
#define __FBSDID(text)
#endif

/* SCE: use LibKernel's character classification and thread-local errors. */
extern unsigned int sceClibLookCtypeTable(int character);
extern void *sceKernelGetTLSAddr(unsigned int offset);

#undef isspace
#define isspace(character) (sceClibLookCtypeTable(character) & 8)
#undef errno
#define errno (*(int *)sceKernelGetTLSAddr(0x20))
#undef ERANGE
#define ERANGE ((int)0x80010022)
#undef EINVAL
#define EINVAL ((int)0x80010016)

#include "../../vendor/bsd-stdlib-7.0.0/strtoll.c"
