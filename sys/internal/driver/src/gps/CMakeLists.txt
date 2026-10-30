psp2_module(os0_kd_bootimage_gps KIND kernel COMPILER gcc48 SOURCES gps.c gps_device.c nmea.c gps_api.c gps_wrapper.c gps_syscall.c
	IMPORTS sceKernelWaitThreadEnd
)
# Sony's build of this module lets GCC drop a NULL check after a dereference
target_compile_options(os0_kd_bootimage_gps PRIVATE -fdelete-null-pointer-checks)
