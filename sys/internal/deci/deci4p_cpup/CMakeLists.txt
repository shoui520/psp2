# os0:kd/deci4p_cpup.skprx - DECI4P protocol 0x3000 (CPUP) requests to the host
psp2_module(os0_kd_deci4p_cpup KIND kernel SOURCES
	cpup.c
IMPORTS
	ksceKernelFreeMemBlock
	sceDfMgrClose
)
