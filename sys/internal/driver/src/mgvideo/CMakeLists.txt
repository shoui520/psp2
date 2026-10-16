# C implementation in original address order.
psp2_module(os0_kd_mgvideo KIND kernel SOURCES
	mgvideo_errors.c
	mgvideo_lifecycle.c
	mgvideo_callbacks.c
	mgvideo_usercopy.c
	mgvideo_keyring.c
	mgvideo_file.c
	mgvideo_process.c
	mgvideo_cipher.c
	mgvideo_parser.c
	mgvideo_data.c
	mgvideo.c
)
