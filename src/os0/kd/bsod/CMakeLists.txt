# Crash console/reporting and exception capture in C with CPU entry veneers.
psp2_module(os0_kd_bsod KIND kernel SOURCES
	source/bsod.c
	source/console.c
	source/dummyfs.c
	source/exception.c
	source/exception_capture.c
	source/exception_entries.S
	source/formatter.c
	source/iftu.c
	source/log.c
	source/report.c
	source/syscon.c
	source/ui.c
)
