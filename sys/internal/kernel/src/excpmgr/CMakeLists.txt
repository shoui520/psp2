psp2_module(os0_kd_excpmgr KIND kernel COMPILER gcc48
	SOURCES excpmgr_vector.S
		excpmgr_panic_dabt.S excpmgr_panic_dabt.c
		excpmgr_panic_pabt.S excpmgr_panic_pabt.c
		excpmgr_panic_undef.S excpmgr_panic_undef.c
		excpmgr_leave.c
		excpmgr_initial_dabt.S excpmgr_initial_dabt.c
		excpmgr_initial_pabt.S excpmgr_initial_pabt.c
		excpmgr_initial_undef.S excpmgr_initial_undef.c
		excpmgr_context.c excpmgr_entry.S
		excpmgr_readonly.c excpmgr.c
		excpmgr_readonly_strings.c)

# Sony's exception-context code uses ARM; ordinary C diagnostics use Thumb.
set_source_files_properties(excpmgr_context.c excpmgr_leave.c
	excpmgr_panic_dabt.c excpmgr_panic_pabt.c
	excpmgr_panic_undef.c excpmgr_initial_dabt.c
	excpmgr_initial_pabt.c excpmgr_initial_undef.c
	PROPERTIES COMPILE_OPTIONS "-marm")
