# C policy with live-register exception and interrupt return veneers.
psp2_module(os0_kd_deci4p_sdfmgr KIND kernel COMPILER gcc48 SOURCES
	sdfmgr.c
	pd_dcmp.c
	pd_tsmp.c
	if_sdio.c
	cpu_entries.S
)
