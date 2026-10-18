/*
 * LibKernel's sceClibStrlcpy: FreeBSD's strlcpy over newlib's headers until the user-mode SDK
 * headers come (sysmem builds the adapted copy, strlcpy_sce.patch). FreeBSD identification
 * records have no runtime role on PSP2.
 */
#ifndef __FBSDID
#define __FBSDID(record)
#endif

#include "../../../../../contrib/bsd-string-1998/strlcpy.c"
