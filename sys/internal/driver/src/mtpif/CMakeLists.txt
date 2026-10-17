psp2_module(os0_kd_bootimage_mtpif KIND kernel SOURCES
	trans.c
	IMPORTS ksceKernelFinalizeFastMutex SceProcEventForDriver_3DED57CC
)
