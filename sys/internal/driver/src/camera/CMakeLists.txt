# Original module/export/import tables; complete camera C sources.
psp2_module(os0_kd_bootimage_camera KIND kernel COMPILER gcc48 SCE_LAYOUT SOURCES
	camera_user.c
	camera_config.c
	camera.c
	ov7735.c
)
