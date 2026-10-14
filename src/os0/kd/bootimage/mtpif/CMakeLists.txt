psp2_module(os0_kd_bootimage_mtpif KIND kernel SOURCES
	source/trans.c
	IMPORTS ksceKernelFinalizeFastMutex ksceKernelUnregisterProcEventHandler
)
