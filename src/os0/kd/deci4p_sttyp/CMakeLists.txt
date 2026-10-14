# os0:kd/deci4p_sttyp.skprx - kernel printf output over DECI4P (per-CPU STTYP channels)
psp2_module(os0_kd_deci4p_sttyp KIND kernel SOURCES
	source/sttyp.c
IMPORTS
	SceDeci4pSDfMgrForKernel_86CFC8C6
)
