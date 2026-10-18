# os0:kd/deci4p_sdfctl.skprx - remote power control over the debug UART (SDCMP / STSMP)
psp2_module(os0_kd_deci4p_sdfctl KIND kernel COMPILER gcc48 SOURCES
	sdfctl.c
	sdcmp.c
	stsmp.c
	uart.c
IMPORTS
	SceSysrootForDriver_2816A813
	SceSysrootForDriver_9421B223
	sceKernelDeleteEventFlag
	sceKernelDeleteThread
	sceKernelGenerateSoftIntr
	sceKernelReleaseIntrHandler
)
