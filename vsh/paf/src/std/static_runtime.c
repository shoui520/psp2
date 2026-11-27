/*
 * Static PAF C/C++ runtime exit hooks for standalone executables linked against
 * static libpaf_stdc.a (cui_setupper, psp2swu).
 */
#include "stdc++/atexit.h"
#include "stdc++/paf_crt_mgr.h"
#include "global_heap.h"
#include <stddef.h>

extern int sceKernelCallModuleExit(int status);
extern int sceKernelExitProcess(int status);
extern void *__dso_handle;

static void *paf_private_exit_dso;

void sce_paf_private_atexit(void *dso)
{
	paf_private_exit_dso = dso;
}

void exit(int status)
{
	sceKernelCallModuleExit(1);
	__cxa_finalize(paf_private_exit_dso ? paf_private_exit_dso : __dso_handle);
	sceKernelExitProcess(status);
}

#ifdef PAF_BUILD_2014
extern void *sce_paf_memcpy(void *dst, const void *src, size_t count);
extern void *sce_paf_memset(void *dst, int ch, size_t count);

void *memcpy(void *dst, const void *src, size_t count)
{
	return sce_paf_memcpy(dst, src, count);
}

void *memset(void *dst, int ch, size_t count)
{
	return sce_paf_memset(dst, ch, count);
}

extern int CallListListenerInitialize(void);

int (*const g_pafStaticInit[])(void) __attribute__((section(".init_array"), used)) = {
	PafCrtManagerInitialize,
	GlobalHeapInitialize,
	CallListListenerInitialize,
};
#else
/*
 * Static initializers in .init_array order matching Sony's 0.996 link:
 * PafCrtManager must initialize its LwMutexes before GlobalHeapInitialize calls __aeabi_atexit.
 */
static int (*const s_pafStaticInit[])(void) __attribute__((section(".init_array"), used)) = {
	PafCrtManagerInitialize,
	GlobalHeapInitialize,
};
#endif
