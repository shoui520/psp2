# C implementation in original address order.
psp2_module(os0_kd_mgvideo KIND kernel SOURCES
	source/mgvideo_errors.c
	source/mgvideo_lifecycle.c
	source/mgvideo_callbacks.c
	source/mgvideo_usercopy.c
	source/mgvideo_keyring.c
	source/mgvideo_file.c
	source/mgvideo_process.c
	source/mgvideo_cipher.c
	source/mgvideo_parser.c
	source/mgvideo_data.c
	source/mgvideo.c
)
