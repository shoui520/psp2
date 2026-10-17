/*
 * SDK include compatibility: SceSysmem's interface is sysmem/sysmem.h, which wraps vitasdk's
 * psp2kern/kernel/sysmem.h with Sony's layouts of the allocation options. Every include of it
 * leads there, so the order of includes does not matter.
 */
#ifdef SYSMEM_INCLUDE_VITASDK
#include_next <psp2kern/kernel/sysmem.h>
#else
#include <sysmem/sysmem.h>
#endif
