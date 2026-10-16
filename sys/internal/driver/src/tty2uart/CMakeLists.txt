# os0:kd/bootimage/tty2uart - tty0:.. devices on the debug UART with per-process log buffers
psp2_module(os0_kd_bootimage_tty2uart KIND kernel SOURCES
	tty2uart.c
IMPORTS
	ksceKernelDeleteMutex
	ksceKernelUnregisterProcEventHandler
)
