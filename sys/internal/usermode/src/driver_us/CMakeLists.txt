psp2_module(os0_us_driver_us KIND user COMPILER gcc48 SOURCES
	motion_math.c
	fios.c
	rtc.c
	rtc_parse.c
	rtc_clock.c
	display.c
	appmgr.c
	motion.c
	motion_calib.c
	motion_fusion.c
	motion_ds3.c
)

# The system call front ends were built with the plain stack protector,
# the SceRtc and SceMotion libraries with the strong one (the default).
set_source_files_properties(fios.c rtc_clock.c display.c appmgr.c
	PROPERTIES COMPILE_OPTIONS "-fstack-protector")
set_source_files_properties(motion.c motion_math.c motion_calib.c
	motion_fusion.c
	PROPERTIES COMPILE_OPTIONS "-fno-math-errno")
# The calendar arithmetic and the DS3 sample processing were built at -O3: Sony's
# code has the day-of-year loop unswitched and unrolled, and Ds3RingAdd's range
# loop vectorized with NEON.
set_source_files_properties(motion_ds3.c PROPERTIES COMPILE_OPTIONS "-fno-math-errno;-O3")
set_source_files_properties(rtc.c PROPERTIES COMPILE_OPTIONS "-O3")
# The parsers read the string as signed char (sign-extending loads).
set_source_files_properties(rtc_parse.c PROPERTIES COMPILE_OPTIONS "-fsigned-char")
