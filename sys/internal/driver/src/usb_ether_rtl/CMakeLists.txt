psp2_module(os0_kd_bootimage_usb_ether_rtl KIND kernel
	SOURCES source/usb_ether_rtl.c source/registers.c source/worker.c
	IMPORTS ksceKernelDeleteEventFlag ksceUsbdUnregisterDriver)
