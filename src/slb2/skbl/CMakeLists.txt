# SKBL, the secure kernel boot loader and secure monitor: segments 0-2 of
# kernel_boot_loader.self (the reset vector page at 0, the code at 0x40020000 with the
# TrustZone modules it loads, the data). It starts the TrustZone kernel (src/slb2/kbl/),
# then hands over to NSKBL (src/slb2/nskbl/) in Non-secure state.
# Skeleton (T2): the reset vector and reset handler are the original's, every C function
# is a stub, the TrustZone module archive is not packed yet.
psp2_image(slb2_skbl ADDRESS 0x40020000 LINKER_SCRIPT skbl.ld SKELETON SOURCES
	source/reset.S
	source/boot.c
)
