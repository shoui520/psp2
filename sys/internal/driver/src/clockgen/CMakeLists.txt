# os0:kd/bootimage/clockgen - clock generator
psp2_module(os0_kd_bootimage_clockgen KIND kernel COMPILER gcc48 SOURCES
	clockgen.c
	IMPORTS sceKernelDeleteMutex
)
