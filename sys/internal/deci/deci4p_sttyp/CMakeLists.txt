# os0:kd/deci4p_sttyp.skprx - kernel printf output over DECI4P (per-CPU STTYP channels)
psp2_module(os0_kd_deci4p_sttyp KIND kernel COMPILER gcc48 SOURCES
	sttyp.c
IMPORTS
	sceSDfMgrClose
)
