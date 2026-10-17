# os0:kd/deci4p_sdrfp.skprx - DECI4P remote file protocol (DRFP): host0: over SceDeci4pSDfMgr
psp2_module(os0_kd_deci4p_sdrfp KIND kernel COMPILER gcc48 SOURCES
	sdrfp.c
	sdrfpfs.c
	IMPORTS SceDeci4pSDfMgrForKernel_86CFC8C6
)
