# os0:kd/bootimage/touch_dummy - SceTouchDummy. Sony built it at -O3, like motion_dummy:
# the process searches are unrolled into compare chains and the priority copy is vectorised.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_touch_dummy KIND kernel COMPILER gcc48 SOURCES
	touch_dummy.c ring.c touch_user.c tracking.c virtual.c
	IMPORTS sceKernelDeleteEventFlag
)
