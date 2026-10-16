psp2_module(os0_kd_bootimage_gps KIND kernel SOURCES gps.c nmea.c
	IMPORTS ksceKernelWaitThreadEnd
)
