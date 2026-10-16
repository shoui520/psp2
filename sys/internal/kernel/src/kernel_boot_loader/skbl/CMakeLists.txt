# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (slb2/kbl/),
# then hands over to NSKBL (sys/internal/kernel/src/kernel_boot_loader/nskbl/) in Non-secure state.
# The reset vector, reset handler, secure monitor (monitor.S) and the code copied to
# Compat SRAM (compat_sram.S) are the original's; kbl_main.c starts the kernel and hands
# over; serial.c is os0 SceSysmem's (SYSMEM_SKBL) and sysroot.c its sysroot setup
# as NSKBL builds it; intrmgr.c, usleep.c and dmacmgr_tzs.c are the TrustZone
# SceKernelIntrMgr's (INTRMGR_SKBL); led.c and l2cc.c os0 SceSysmem's (SYSMEM_SKBL);
# memset and memcpy sysmem's memset_chk.c and memcpy_chk.c with sysclib's assembly
# cores. The other C functions are stubs and the TrustZone module archive is not
# packed yet.
set(sysmem_source ../../sysmem)
set(intrmgr_source ../../intrmgr/tzs)
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld PARTIAL SOURCES
	reset.S
	memory_test.c
	monitor.S
	boot.c
	kbl_main.c
	archive_tzs.c
	${intrmgr_source}/dmacmgr_tzs.c
	${intrmgr_source}/intrmgr.c
	${intrmgr_source}/usleep.c
	${sysmem_source}/led.c
	${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S
	${sysmem_source}/serial.c
	${sysmem_source}/sysroot.c
	${sysmem_source}/memset_chk.c
	${sysmem_source}/memcpy_chk.c
	../../../../../../common/sysclib/copy_forward.S
	../../../../../../common/sysclib/move.S
	../../../../../../common/sysclib/fill_core.S
	../../../../../../common/kbl/pervasive_init_kbl.c
	../../../../../../common/kbl/pervasive_kbl.c
	compat_sram.S
)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL" COMPILE_OPTIONS "-fsigned-char")
set_source_files_properties(${sysmem_source}/sysroot.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL")
set_source_files_properties(${sysmem_source}/led.c ${sysmem_source}/l2cc.c
	${sysmem_source}/l2cc_maintenance.S PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_SKBL)
set_source_files_properties(${intrmgr_source}/intrmgr.c ${intrmgr_source}/usleep.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL)
set_source_files_properties(${intrmgr_source}/dmacmgr_tzs.c PROPERTIES
	COMPILE_DEFINITIONS INTRMGR_SKBL COMPILE_OPTIONS "-Os")
set_source_files_properties(../../../../../../common/kbl/pervasive_kbl.c PROPERTIES
	COMPILE_DEFINITIONS PERVASIVE_SKBL)
