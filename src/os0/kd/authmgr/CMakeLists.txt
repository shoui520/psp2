psp2_module(os0_kd_authmgr KIND kernel SOURCES source/authmgr.c
	IMPORTS ksceKernelFinalizeFastMutex ksceKernelDeleteSema ksceKernelDeleteEventFlag)
target_compile_options(os0_kd_authmgr PRIVATE
	-std=gnu11 -fno-strict-aliasing -fno-delete-null-pointer-checks)
