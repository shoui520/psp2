# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (src/slb2/kbl/),
# then hands over to NSKBL (src/slb2/nskbl/) in Non-secure state.
# The reset vector, reset handler, secure monitor (monitor.S) and the code copied to
# Compat SRAM (compat_sram.S) are the original's; serial.c is os0 SceSysmem's
# (SYSMEM_SKBL) and sysroot_time.c its sysroot setup as NSKBL builds it, intrmgr.c, usleep.c and dmacmgr_tzs.c the TrustZone SceKernelIntrMgr's
# (INTRMGR_SKBL), led.c and l2cc.c os0 SceSysmem's (SYSMEM_SKBL), memset and memcpy sysmem's
# memory_set.c and memory_copy.c with sysclib's assembly cores; the other C functions are
# stubs and the TrustZone module archive is not packed yet.
set(sysmem_source ../../os0/kd/sysmem/source)
set(intrmgr_source ../kbl/SceKernelIntrMgr/source)
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld PARTIAL SOURCES
	source/reset.S
	source/memory_test.c
	source/monitor.S
	source/boot.c
	${intrmgr_source}/dmacmgr_tzs.c
	${intrmgr_source}/intrmgr.c
	${intrmgr_source}/usleep.c
	${sysmem_source}/led.c
	${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S
	${sysmem_source}/serial.c
	${sysmem_source}/sysroot_time.c
	${sysmem_source}/memory_set.c
	${sysmem_source}/memory_copy.c
	../../common/sysclib/copy_forward.S
	../../common/sysclib/move.S
	../../common/sysclib/fill_core.S
	../../common/kbl/pervasive_init_kbl.c
	../../common/kbl/pervasive_kbl.c
	source/compat_sram.S
)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL" COMPILE_OPTIONS "-fsigned-char")
set_source_files_properties(${sysmem_source}/sysroot_time.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL")
set_source_files_properties(${sysmem_source}/led.c ${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_SKBL)
set_source_files_properties(${intrmgr_source}/intrmgr.c ${intrmgr_source}/usleep.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL)
set_source_files_properties(${intrmgr_source}/dmacmgr_tzs.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL COMPILE_OPTIONS "-Os")
set_source_files_properties(../../common/kbl/pervasive_kbl.c PROPERTIES
	COMPILE_DEFINITIONS PERVASIVE_SKBL)
