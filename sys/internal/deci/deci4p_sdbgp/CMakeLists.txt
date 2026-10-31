# Debugger conversion: the files after sdbgp.c remain to be written.
psp2_module(os0_kd_deci4p_sdbgp KIND kernel COMPILER gcc48 PARTIAL SOURCES
	cpu_regs.S
	sdbgp.c
	check.c
	swbkpt.c
	hwbkpt.c
	leakcheck.c
	phymem.c
	bg_check.c
	sysmem_address_space.c
	sysmem_partition.c
	cpu_mmu.c
	excp_handler.c
	probe.S
	group_module.c
	group_file.c
	excp_info.c
	testapi.c
)
