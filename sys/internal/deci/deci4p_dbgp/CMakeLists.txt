# Debugger conversion: unresolved policy functions remain to be written.
psp2_module(os0_kd_deci4p_dbgp KIND kernel PARTIAL SOURCES
	source/excp_entries.S
	source/dbgp.c
	source/check.c
	source/process.c
	source/deci4p_dbgp.c
)
