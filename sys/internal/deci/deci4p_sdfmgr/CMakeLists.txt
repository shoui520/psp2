# The exception and SDIO interrupt entries (assembly), then Sony's C files.
psp2_module(os0_kd_deci4p_sdfmgr KIND kernel COMPILER gcc48 SOURCES
	cpu_entries.S
	sdfmgr.c
	pd_dcmp.c
	pd_tsmp.c
	if_sdio.c
)
