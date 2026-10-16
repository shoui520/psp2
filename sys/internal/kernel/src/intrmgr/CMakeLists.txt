psp2_module(os0_kd_intrmgr KIND kernel
	SOURCES intrmgr_data.c intrmgr_cpu.S intrmgr_stack.c
		intrmgr_syscall.S intrmgr_copy.c intrmgr.c
		intrmgr_context.c intrmgr_readonly.c)
