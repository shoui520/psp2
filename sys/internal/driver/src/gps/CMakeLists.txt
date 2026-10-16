psp2_module(os0_kd_bootimage_gps KIND kernel SOURCES source/gps.c source/nmea.c
	IMPORTS ksceKernelWaitThreadEnd
)
