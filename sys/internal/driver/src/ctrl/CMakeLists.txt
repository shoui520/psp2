psp2_module(os0_kd_bootimage_ctrl KIND kernel COMPILER gcc48 SOURCES
	ctrl.c
)
target_link_options(os0_kd_bootimage_ctrl PRIVATE
	"-Wl,--no-fix-cortex-a8")
