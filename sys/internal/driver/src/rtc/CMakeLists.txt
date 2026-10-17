psp2_module(os0_kd_rtc KIND kernel COMPILER gcc48 SOURCES
	rtc.c
	calendar.c
	format.c
	libc_time.c
	syscall.c
)
