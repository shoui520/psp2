# SceCompat implementation.
psp2_module(os0_kd_bootimage_compat KIND kernel SOURCES
	compat.c
	compat_syscalls.c
	compat_pocketstation.c
)
