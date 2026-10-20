# Crash console/reporting and exception capture in C with CPU entry veneers.
psp2_module(os0_kd_bsod KIND kernel COMPILER gcc48 SOURCES
	exception_entries.S
	moncall.S
	bsod.c
	iftu.c
	syscon.c
	console.c
	formatter.c
	report.c
	log.c
	dummyhostfs.c
)
# Sony built this module with -Os (literal pools, push for the frame).
target_compile_options(os0_kd_bsod PRIVATE -Os)
# Sony's formatter divides by 10 with a multiply (no division call): -O2.
set_source_files_properties(formatter.c PROPERTIES COMPILE_OPTIONS -O2)
