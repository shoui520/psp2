#ifndef VSH_APP_SHELL_FAT_FORMAT_USER_COMPAT_H
#define VSH_APP_SHELL_FAT_FORMAT_USER_COMPAT_H

#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>

/* SNC C mode does not implement C11 _Static_assert */
#ifndef _Static_assert
#define _Static_assert(cond, msg)
#endif

/* Include sysmem/debug.h before redirecting sceKernelPrintfLevel to user sceClibPrintf */
#include <sysmem/debug.h>
#define sceKernelPrintfLevel(level, ...) ((void)(level), sceClibPrintf(__VA_ARGS__))

/* SNC C mode emits calls to __builtin_memcpy and strncpy; route to ScePafStdc / SceLibKernel */
extern void *sce_paf_memcpy(void *dst, const void *src, SceSize len);
#define __builtin_memcpy sce_paf_memcpy
#define strncpy sceClibStrncpy

/* Sony's user-mode SceShell build keeps fat_format.c's internal helpers out of line */
#define inline

#endif /* VSH_APP_SHELL_FAT_FORMAT_USER_COMPAT_H */
