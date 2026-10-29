# SceUsbd host stack and user/driver interfaces.
# Sony built it at -O3: ehci.c's loops are vectorized and prefetched, and its
# start and stop are inlined into resume, suspend and term.
set(MODULE_COMPILE_OPTIONS -O3)
set(MODULE_SCE_RESERVE ON)	# its text ends just short of the data segment
psp2_module(os0_kd_bootimage_usbd KIND kernel COMPILER gcc48 SOURCES
	usbd.c
	usbd_os.c
	ehci.c
	ohci.c
	hub.c
)
target_compile_options(os0_kd_bootimage_usbd PRIVATE -std=gnu11
	-fno-delete-null-pointer-checks)
