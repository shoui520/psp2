# one Sony build, shipped as os0:kd/usbpspcm and inside bootimage (os0:kd/bootimage/usbpspcm):
# the target is named from the rel
psp2_module(${PSP2_TARGET} KIND kernel SOURCES
	usbpspcm.c
)
