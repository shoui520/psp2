psp2_module(os0_kd_excpmgr KIND kernel COMPILER gcc48
	SOURCES excpmgr_vector.S excpmgr_initial.S excpmgr_context.S
		excpmgr_readonly.c excpmgr.c
		excpmgr_readonly_strings.c)
