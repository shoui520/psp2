# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (src/slb2/kbl/),
# then hands over to NSKBL (src/slb2/nskbl/) in Non-secure state.
# The reset vector and reset handler are the original's, serial.c is os0 SceSysmem's
# (SYSMEM_SKBL), intrmgr.c, usleep.c and dmacmgr_tzs.c the TrustZone SceKernelIntrMgr's
# (INTRMGR_SKBL), led.c and l2cc.c os0 SceSysmem's (SYSMEM_SKBL), memcpy
# sysmem's memory_copy.c and sysclib's copy_forward.S,
# the other C functions are stubs and the TrustZone module archive is
# not packed yet.
set(sysmem_source ../../os0/kd/sysmem/source)
set(intrmgr_source ../kbl/SceKernelIntrMgr/source)
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld PARTIAL SOURCES
	source/reset.S
	source/memory_test.c
	source/boot.c
	${intrmgr_source}/dmacmgr_tzs.c
	${intrmgr_source}/intrmgr.c
	${intrmgr_source}/usleep.c
	${sysmem_source}/led.c
	${sysmem_source}/l2cc.c
	${sysmem_source}/serial.c
	${sysmem_source}/memory_copy.c
	../../common/sysclib/copy_forward.S
	../../common/kbl/pervasive_init_kbl.c
	../../common/kbl/pervasive_kbl.c
)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL" COMPILE_OPTIONS "-fsigned-char")
set_source_files_properties(${sysmem_source}/led.c ${sysmem_source}/l2cc.c PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_SKBL)
set_source_files_properties(${intrmgr_source}/intrmgr.c ${intrmgr_source}/usleep.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL)
set_source_files_properties(${intrmgr_source}/dmacmgr_tzs.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL COMPILE_OPTIONS "-Os")
set_source_files_properties(../../common/kbl/pervasive_kbl.c PROPERTIES
	COMPILE_DEFINITIONS PERVASIVE_SKBL)
