# Debugger conversion: unresolved policy functions remain to be written.
psp2_module(os0_kd_deci4p_dbgp KIND kernel COMPILER gcc48 PARTIAL SOURCES
	excp_entries.S
	dbgp.c
	check.c
	process.c
	swbkpt.c
	hwwtpt.c
	single_step.c
	appobj.c
	excp_handler.c
	excp_info.c
	intr_handler.c
	thread_handler.c
	thread_info.c
	process_handler.c
	process_info.c
	deci4p_dbgp.c
)
# Sony built this module at -O3 without inlining non-inline functions: hwwtpt.c's
# watchpoint scans are peeled and its byte-select mask vectorized, while
# dbgpGetProcCB stays a call from its neighbours in dbgp.c.
target_compile_options(os0_kd_deci4p_dbgp PRIVATE -O3 -fno-inline-functions)
