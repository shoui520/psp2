psp2_module(os0_kd_bootimage_touch_dummy KIND kernel COMPILER gcc48 SOURCES
	touch_dummy.c
	IMPORTS ksceKernelDeleteEventFlag
)
