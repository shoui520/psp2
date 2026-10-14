# os0:kd/deci4p_sdfctl.skprx - remote power control over the debug UART (SDCMP / STSMP)
psp2_module(os0_kd_deci4p_sdfctl KIND kernel SOURCES
	source/sdfctl.c
	source/sdcmp.c
	source/stsmp.c
	source/uart.c
IMPORTS
	SceSysrootForDriver_2816A813
	SceSysrootForDriver_9421B223
	ksceKernelDeleteEventFlag
	ksceKernelDeleteThread
	ksceKernelTriggerSGI
	ksceKernelReleaseIntrHandler
)
