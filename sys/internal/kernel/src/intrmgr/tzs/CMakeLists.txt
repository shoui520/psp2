# slb2:kbl/SceKernelIntrMgr - secure-world interrupt manager
psp2_module(slb2_kbl_SceKernelIntrMgr KIND kernel SOURCES
	intrmgr_entry.S
	dmacmgr_tzs.c
	intrmgr.c
	usleep.c
	access_mode.c
)
# Sony built dmacmgr_tzs.c for size: literal pools, unaligned strings
set_source_files_properties(dmacmgr_tzs.c PROPERTIES COMPILE_OPTIONS "-Os")
