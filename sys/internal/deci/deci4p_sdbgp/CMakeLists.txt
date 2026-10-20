# Debugger conversion: the files after sdbgp.c remain to be written.
psp2_module(os0_kd_deci4p_sdbgp KIND kernel COMPILER gcc48 PARTIAL SOURCES
	cpu_regs.S
	sdbgp.c
	check.c
	swbkpt.c
	deci4p_sdbgp.c
)
