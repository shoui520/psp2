# SceVshBridge: Sony's vshbridge.c, its user-pointer wrappers, and SceDrmBridge.
psp2_module(os0_kd_bootimage_vshbridge KIND kernel COMPILER gcc48 SOURCES
	vshbridge.c
	vshbridge_user.c
	drmbridge.c
	drmbridge_user.c
)
