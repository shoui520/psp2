psp2_module(os0_kd_authmgr KIND kernel COMPILER gcc48 SOURCES authmgr.c
	IMPORTS sceKernelFinalizeFastMutex sceKernelDeleteSema sceKernelDeleteEventFlag)
target_compile_options(os0_kd_authmgr PRIVATE
	-std=gnu11 -fno-strict-aliasing -fno-delete-null-pointer-checks)
