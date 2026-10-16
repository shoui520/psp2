psp2_module(os0_kd_excpmgr KIND kernel
	SOURCES source/excpmgr_vector.S
		source/excpmgr_panic_dabt.S source/excpmgr_panic_dabt.c
		source/excpmgr_panic_pabt.S source/excpmgr_panic_pabt.c
		source/excpmgr_panic_undef.S source/excpmgr_panic_undef.c
		source/excpmgr_leave.c
		source/excpmgr_initial_dabt.S source/excpmgr_initial_dabt.c
		source/excpmgr_initial_pabt.S source/excpmgr_initial_pabt.c
		source/excpmgr_initial_undef.S source/excpmgr_initial_undef.c
		source/excpmgr_context.c source/excpmgr_entry.S
		source/excpmgr_readonly.c source/excpmgr.c
		source/excpmgr_readonly_strings.c)

# Sony's exception-context code uses ARM; ordinary C diagnostics use Thumb.
set_source_files_properties(source/excpmgr_context.c source/excpmgr_leave.c
	source/excpmgr_panic_dabt.c source/excpmgr_panic_pabt.c
	source/excpmgr_panic_undef.c source/excpmgr_initial_dabt.c
	source/excpmgr_initial_pabt.c source/excpmgr_initial_undef.c
	PROPERTIES COMPILE_OPTIONS "-marm")
