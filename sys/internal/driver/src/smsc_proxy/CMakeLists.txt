# os0:kd/smsc_proxy.skprx - secure module scheduler proxy (SMC sessions, Cry2Arm)
psp2_module(os0_kd_smsc_proxy KIND kernel COMPILER gcc48 SOURCES
	smc.S
	smsc_proxy.c
	tzs_bridge.c
)
