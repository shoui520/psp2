# NSKBL, the non-secure kernel boot loader: segment 3 of kernel_boot_loader.self
# (ARZL-compressed there), a flat ARM image that runs at 0x51000000.
# Sony built it from os0 sources (SYSMEM_NSKBL in SceSysmem's); the reset vector
# and exception entries are the original's, the rest of boot.c is stubs.
set(sysmem_source ../../os0/kd/sysmem/source)
psp2_image(slb2_nskbl ADDRESS 0x51000000 PARTIAL SOURCES
	source/reset.S
	source/boot.c
	${sysmem_source}/serial.c
)
set_source_files_properties(${sysmem_source}/serial.c PROPERTIES
	COMPILE_DEFINITIONS SYSMEM_NSKBL COMPILE_OPTIONS "-fsigned-char")
