# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (src/slb2/kbl/),
# then hands over to NSKBL (src/slb2/nskbl/) in Non-secure state.
# The reset vector and reset handler are the original's, serial.c is os0 SceSysmem's
# (SYSMEM_SKBL), the other C functions are stubs and the TrustZone module archive is
# not packed yet.
set(sysmem_source ../../os0/kd/sysmem/source)
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld PARTIAL SOURCES
	source/reset.S
	source/boot.c
	${sysmem_source}/serial.c
)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS "SYSMEM_NSKBL;SYSMEM_SKBL" COMPILE_OPTIONS "-fsigned-char")
