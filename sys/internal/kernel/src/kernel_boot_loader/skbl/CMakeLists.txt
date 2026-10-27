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
set(lowio_source ../../../../driver/src/lowio)
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
	../../sysmem/copy_forward.S
	../../sysmem/move.S
	../../sysmem/fill_core.S
	${lowio_source}/lowio_state.c
	${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c
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
# the boot loader's own files see sysmem's and lowio's interfaces as SKBL builds them
set_property(SOURCE boot.c kbl_main.c archive_tzs.c APPEND PROPERTY
	COMPILE_DEFINITIONS SYSMEM_NSKBL SYSMEM_SKBL LOWIO_KBL)
# SceLowio's Pervasive driver, its own files built for the boot loader (LOWIO_KBL: physical
# registers, no lock; LOWIO_SKBL: what this image keeps), at the boot loader's -O2: the images keep
# ResetSet and GateSet whole, where os0's -O3 clones them
set_source_files_properties(${lowio_source}/lowio_state.c ${lowio_source}/pervasive.c
	${lowio_source}/pervasive_clock.c PROPERTIES
	COMPILE_DEFINITIONS "LOWIO_KBL;LOWIO_SKBL")
