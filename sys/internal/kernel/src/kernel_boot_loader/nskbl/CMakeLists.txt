# NSKBL, the non-secure kernel boot loader: segment 3 of kernel_boot_loader.self
# (ARZL-compressed there), a flat ARM image that runs at 0x51000000.
# Sony built it from os0 sources (SYSMEM_NSKBL in SceSysmem's); the reset vector
# and exception entries are the original's; boot.c is SKBL's (BOOT_NSKBL): the same
# exception handlers and ASLR helpers, and NSKBL's own boot(); memset and memcpy are
# SceSysmem's memory_set.c and memory_copy.c with sysclib's assembly cores.
set(sysmem_source ../../sysmem)
set(sdif_source ../../../../driver/src/sdif)
set(intrmgr_source ../../intrmgr)
psp2_image(slb2_nskbl ADDRESS 0x51000000 PARTIAL SOURCES
	reset.S
	../skbl/boot.c
	${intrmgr_source}/intrmgr.c
	${sysmem_source}/led.c
	${sysmem_source}/serial.c
	${sysmem_source}/sysroot_time.c
	${sysmem_source}/memory_set.c
	${sysmem_source}/memory_copy.c
	../../../../../../common/sysclib/copy_forward.S
	../../../../../../common/sysclib/fill_core.S
	${sdif_source}/sdif.c
	${sdif_source}/cmd_queue.c
	${sdif_source}/mmc.c
	${sdif_source}/card_cmd.c
	${sdif_source}/sd.c
	${sdif_source}/adma.c
	../../../../../../common/kbl/pervasive_init_kbl.c
	../../../../../../common/kbl/pervasive_kbl.c
)
set_source_files_properties(${intrmgr_source}/intrmgr.c PROPERTIES COMPILE_DEFINITIONS INTRMGR_NSKBL)
set_source_files_properties(${sysmem_source}/led.c ${sysmem_source}/sysroot_time.c PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_NSKBL)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_NSKBL COMPILE_OPTIONS "-fsigned-char")
set_source_files_properties(${sdif_source}/sdif.c ${sdif_source}/cmd_queue.c
	${sdif_source}/mmc.c ${sdif_source}/card_cmd.c ${sdif_source}/sd.c
	${sdif_source}/adma.c PROPERTIES COMPILE_DEFINITIONS SDIF_NSKBL)
set_source_files_properties(../skbl/boot.c PROPERTIES COMPILE_DEFINITIONS BOOT_NSKBL)
