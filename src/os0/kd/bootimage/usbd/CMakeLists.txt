# SceUsbd host stack and user/driver interfaces.
# Sony built it at -O3: ehci.c's loops are vectorized and prefetched, and its
# start and stop are inlined into resume, suspend and term.
set(MODULE_COMPILE_OPTIONS -O3)
set(MODULE_SCE_RESERVE ON)	# its text ends just short of the data segment
psp2_module(os0_kd_bootimage_usbd KIND kernel SOURCES
	source/usbd.c
	source/usbd_os.c
	source/ehci.c
	source/ohci.c
	source/hub.c
)
target_compile_options(os0_kd_bootimage_usbd PRIVATE -std=gnu11
	-fno-delete-null-pointer-checks
	-fsection-anchors)	# each file's data through one base, as in the original
