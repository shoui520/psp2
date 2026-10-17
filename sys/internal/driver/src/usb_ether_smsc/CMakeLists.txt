# one Sony build, shipped as os0:kd/usb_ether_smsc and inside bootimage (os0:kd/bootimage/usb_ether_smsc):
# the target is named from the rel
psp2_module(${PSP2_TARGET} KIND kernel SOURCES usb_ether_smsc.c)
