# C policy with live-register exception and interrupt return veneers.
psp2_module(os0_kd_deci4p_sdfmgr KIND kernel SOURCES
	source/sdfmgr.c
	source/pd_dcmp.c
	source/pd_tsmp.c
	source/if_sdio.c
	source/cpu_entries.S
)
