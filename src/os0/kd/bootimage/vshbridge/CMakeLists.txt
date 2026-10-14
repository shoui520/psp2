# SceVshBridge: Sony's vshbridge.c, its user-pointer wrappers, and SceDrmBridge.
psp2_module(os0_kd_bootimage_vshbridge KIND kernel SOURCES
	source/vshbridge.c
	source/vshbridge_user.c
	source/drmbridge.c
	source/drmbridge_user.c
)
