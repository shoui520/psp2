psp2_module(os0_kd_bootimage_gps KIND kernel COMPILER gcc48 SOURCES gps.c nmea.c
	IMPORTS ksceKernelWaitThreadEnd
)
