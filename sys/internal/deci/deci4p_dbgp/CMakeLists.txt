# Debugger conversion: unresolved policy functions remain to be written.
psp2_module(os0_kd_deci4p_dbgp KIND kernel COMPILER gcc48 PARTIAL SOURCES
	excp_entries.S
	dbgp.c
	check.c
	process.c
	deci4p_dbgp.c
)
