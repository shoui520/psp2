# os0:kd/smsc_proxy.skprx - secure module scheduler proxy (SMC sessions, Cry2Arm)
psp2_module(os0_kd_smsc_proxy KIND kernel SOURCES
	source/smc.S
	source/smsc_proxy.c
	source/tzs_bridge.c
)
