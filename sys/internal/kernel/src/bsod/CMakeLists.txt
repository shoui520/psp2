# Crash console/reporting and exception capture in C with CPU entry veneers.
psp2_module(os0_kd_bsod KIND kernel SOURCES
	bsod.c
	console.c
	dummyhostfs.c
	exception_entries.S
	formatter.c
	iftu.c
	log.c
	report.c
	syscon.c
)
