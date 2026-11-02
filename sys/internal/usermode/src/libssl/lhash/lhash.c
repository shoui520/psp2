/* SSLeay's LHASH, with the measured SSL-C changes in lhash_sce.patch. */
#include <stdio.h>
#include "crypto.h"
#include "../memory.h"

/* Descriptive adapter name is ours; the original has no recovered name. */
int sslCompareStrings(const char *left, const char *right);

#undef Malloc
#undef Free
#define Malloc(size) sslAllocate(size)
#define Free(ptr) sslFree(ptr)
#define strcmp sslCompareStrings

#include "lhash_sce.inc"
