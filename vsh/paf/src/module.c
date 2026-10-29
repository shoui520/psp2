/* ScePaf module startup and C++ runtime finalization. */
#include "module.h"
#include "std/stdc++/atexit.h"
#include <stddef.h>

/* Only import prototypes are needed; processmgr.h pulls C11-only headers. */
extern int sceKernelCallModuleExit(int status);
extern int sceKernelExitProcess(int status);

SceUInt32 global_heap_size;
SceInt32 reserved_init_param[2];
SceInt32 cdlg_mode;
SceInt32 global_heap_alignment;
SceInt32 global_heap_disable_assert_on_alloc_failure;
void *__dso_handle;
static void *paf_private_exit_dso;

void sce_paf_private_atexit(void *dso)
{
	paf_private_exit_dso = dso;
}

void sce_paf_private_exit(int status)
{
	sceKernelCallModuleExit(1);
	ScePafStdc_F36BABE3(paf_private_exit_dso ?
		paf_private_exit_dso : __dso_handle);
	sceKernelExitProcess(status);
}

/* SNC's linker-provided constructor bounds. */
extern void (*__start__Zpreinit_array[])(void);
extern void (*__stop__Zpreinit_array[])(void);
extern void (*__start__Zinit_array[])(void);
extern void (*__stop__Zinit_array[])(void);

int module_start(SceSize argSize, const void *argp)
{
	const ScePafInit *param = argp;
	void (**init)(void);

	global_heap_size = param->global_heap_size;
	reserved_init_param[0] = param->reserved[0];
	reserved_init_param[1] = param->reserved[1];
	cdlg_mode = param->cdlg_mode;
	global_heap_alignment = param->global_heap_alignment;
	__dso_handle = &__dso_handle;
	global_heap_disable_assert_on_alloc_failure =
		param->global_heap_disable_assert_on_alloc_failure;

	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	return 0;
}

int module_stop(SceSize argSize, const void *argp)
{
	ScePafStdc_F36BABE3(&__dso_handle);
	return 0;
}
