# slb2:kbl/SceKernelIntrMgr - secure-world interrupt manager, under conversion to C
psp2_module(slb2_kbl_SceKernelIntrMgr KIND kernel PARTIAL SOURCES
	source/SceKernelIntrMgr.c
	source/dmacmgr_tzs.c
	source/usleep.c
	source/access_mode.c
)
# Sony built dmacmgr_tzs.c for size: literal pools, unaligned strings
set_source_files_properties(source/dmacmgr_tzs.c PROPERTIES COMPILE_OPTIONS "-Os")
