# SceCompat implementation.
psp2_module(os0_kd_bootimage_compat KIND kernel SOURCES
	source/compat.c
	source/compat_framebuffer.c
	source/compat_syscalls.c
	source/compat_pocketstation.c
)
