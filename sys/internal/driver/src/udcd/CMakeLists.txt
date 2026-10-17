# SceUdcd: Sony's udcd_udc2.c (the UDC2 controller), the driver/API code and the user syscalls.
psp2_module(os0_kd_bootimage_udcd KIND kernel COMPILER gcc48 SOURCES
	udcd.c
	udcd_udc2.c
	udcd_user.c
)
