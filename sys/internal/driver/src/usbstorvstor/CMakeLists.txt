# one Sony build, shipped as os0:kd/usbstorvstor and inside bootimage (os0:kd/bootimage/usbstorvstor):
# the target is named from the rel
psp2_module(${PSP2_TARGET} KIND kernel SOURCES
	usbstorvstor.c
)
