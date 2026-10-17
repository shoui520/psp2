psp2_module(os0_kd_bootimage_usb_ether_rtl KIND kernel COMPILER gcc48
	SOURCES usb_ether_rtl.c registers.c worker.c
	IMPORTS ksceKernelDeleteEventFlag ksceUsbdUnregisterDriver)
