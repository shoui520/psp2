# NSKBL, the non-secure kernel boot loader: segment 3 of kernel_boot_loader.self
# (ARZL-compressed there), a flat ARM image that runs at 0x51000000.
# Skeleton (T2): the reset vector and exception entries are the original's, every C
# function is a stub.
psp2_image(slb2_nskbl ADDRESS 0x51000000 SKELETON SOURCES
	source/reset.S
	source/boot.c
)
