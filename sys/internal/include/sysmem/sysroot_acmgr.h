#pragma once

#include <psp2kern/types.h>

/* Access-manager callback registered through NID BE1EF51C. */
typedef struct SysrootAcmgrOps {
	int (*checkRemapCodeForUser)(SceUID pid);
} SysrootAcmgrOps;

#ifndef __cplusplus
_Static_assert(sizeof(SysrootAcmgrOps) == 4,
	       "Sysroot access-manager service table size");
#endif

int SceSysrootForKernel_BE1EF51C(const SysrootAcmgrOps *ops);
