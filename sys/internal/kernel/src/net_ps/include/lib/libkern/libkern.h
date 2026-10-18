/* NetBSD's kernel utility declarations, shared by the vendored sources. */
#include "contrib/netbsd-3.0/sys/lib/libkern/libkern.h"

/*
 * SCE: the original calls memcpy, memcmp, memset, strcpy, strcmp, strlen and ffs, which
 * libkern.h makes GCC's built-ins.
 */
#undef memcpy
#undef memcmp
#undef memset
#undef strcpy
#undef strcmp
#undef strlen
#undef ffs
