psp2_module(os0_kd_bootimage_mtpif KIND kernel COMPILER gcc48 SOURCES
	trans.c
	IMPORTS sceKernelFinalizeFastMutex sceKernelUnregisterProcEventHandler
)
