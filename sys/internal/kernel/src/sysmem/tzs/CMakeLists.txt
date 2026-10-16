# slb2:kbl/SceSysmem - the secure world's SceSysmem. Sony built it from os0
# SceSysmem's sources with the Non-secure parts left out (SYSMEM_TZS); the
# export stubs in SceSysmem.c remain until their C is written.
set(sysmem_tzs_source ..)
# os0 SceSysmem's flags: no -fno-delete-null-pointer-checks
set(MODULE_COMPILE_OPTIONS -fdelete-null-pointer-checks)
psp2_module(slb2_kbl_SceSysmem KIND kernel PARTIAL SOURCES
	SceSysmem.c
	${sysmem_tzs_source}/led.c
	${sysmem_tzs_source}/serial.c
	${sysmem_tzs_source}/l2cc.c
	${sysmem_tzs_source}/l2cc_maintenance.S
)
set_source_files_properties(
	${sysmem_tzs_source}/led.c
	${sysmem_tzs_source}/serial.c
	${sysmem_tzs_source}/l2cc.c
	${sysmem_tzs_source}/l2cc_maintenance.S
	PROPERTIES COMPILE_DEFINITIONS SYSMEM_TZS)
set_source_files_properties(${sysmem_tzs_source}/serial.c PROPERTIES COMPILE_OPTIONS "-fsigned-char")
