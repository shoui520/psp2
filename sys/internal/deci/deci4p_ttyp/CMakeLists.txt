# os0:kd/deci4p_ttyp.skprx - the host's consoles tty0:..tty7: over DECI4P (TTYP0..7)
psp2_module(os0_kd_deci4p_ttyp KIND kernel COMPILER gcc48 SOURCES
	ttyp.c
	ttyp_fs.c
IMPORTS
	sceDfMgrClose
)
