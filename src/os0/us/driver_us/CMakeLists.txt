psp2_module(os0_us_driver_us KIND user SOURCES
	source/motion_math.c
	source/fios.c
	source/rtc.c
	source/rtc_parse.c
	source/rtc_clock.c
	source/display.c
	source/appmgr.c
	source/motion.c
	source/motion_calib.c
	source/motion_fusion.c
	source/motion_ds3.c
)

# The system call front ends were built with the plain stack protector,
# the SceRtc and SceMotion libraries with the strong one (the default).
set_source_files_properties(source/fios.c source/rtc_clock.c source/display.c source/appmgr.c
	PROPERTIES COMPILE_OPTIONS "-fstack-protector")
set_source_files_properties(source/motion.c source/motion_math.c source/motion_calib.c
	source/motion_fusion.c source/motion_ds3.c
	PROPERTIES COMPILE_OPTIONS "-fno-math-errno")
