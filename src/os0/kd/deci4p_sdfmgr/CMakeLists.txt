# C policy with live-register exception and interrupt return veneers.
psp2_module(os0_kd_deci4p_sdfmgr KIND kernel SOURCES
	source/deci4p_sdfmgr.c
	source/control.c
	source/startup.c
	source/errors.c
	source/context.c
	source/debug.c
	source/transport.c
	source/dcmp.c
	source/tsmp.c
	source/sdio.c
	source/cpu_entries.S
)
