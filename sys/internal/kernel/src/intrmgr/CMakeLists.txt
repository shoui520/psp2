psp2_module(os0_kd_intrmgr KIND kernel
	SOURCES source/intrmgr_data.c source/intrmgr_cpu.S source/intrmgr_stack.c
		source/intrmgr_syscall.S source/intrmgr_copy.c source/intrmgr.c
		source/intrmgr_context.c source/intrmgr_readonly.c)
