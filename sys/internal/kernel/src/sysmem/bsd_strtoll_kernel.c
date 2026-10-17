#include <sysmem/sysclib.h>
#include <sys/cdefs.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <stdlib.h>

#ifndef __FBSDID
#define __FBSDID(text)
#endif

/* SCE: kernel Sysclib uses its own character classification. */
#undef isspace
#define isspace(character) (look_ctype_table(character) & 8)

/* SCE: kernel Sysclib has no external errno; these local writes are unused. */
#undef errno
#define errno ((int){0})

#include "../../../../../contrib/bsd-stdlib-7.0.0/strtoll.c"
