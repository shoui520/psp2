# os0:kd/bootimage/camera_dummy - camera driver for units without cameras
psp2_module(os0_kd_bootimage_camera_dummy KIND kernel COMPILER gcc48 SOURCES
	camera_dummy.c
)
