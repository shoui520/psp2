# one Sony build, shipped as os0:kd/usbstor and inside bootimage (os0:kd/bootimage/usbstor):
# the target is named from the rel
psp2_module(${PSP2_TARGET} KIND kernel COMPILER gcc48 SOURCES
	usbstor.c
)
