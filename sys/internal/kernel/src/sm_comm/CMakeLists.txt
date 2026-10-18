psp2_module(os0_kd_sm_comm KIND kernel COMPILER gcc48 SOURCES sm_comm.c
	IMPORTS sceKernelFinalizeFastMutex sceKernelDeleteSema)
target_compile_options(os0_kd_sm_comm PRIVATE
	-std=gnu11 -fno-strict-aliasing -fno-delete-null-pointer-checks)
