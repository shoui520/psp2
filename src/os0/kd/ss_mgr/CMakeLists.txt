# C implementation of the secure services manager.
psp2_module(os0_kd_ss_mgr KIND kernel SCE_LAYOUT SOURCES
	source/ss_mgr.c
	source/qaf_mgr.c
	source/dmac5mgr.c
	source/dmac5mgr_key.c
	source/aimgr_api.c
	source/aimgr_secure_module_comm.c
	source/aimgr_user.c
	source/rng.c
)
get_target_property(SS_MGR_LINK_OPTIONS os0_kd_ss_mgr LINK_OPTIONS)
list(FILTER SS_MGR_LINK_OPTIONS EXCLUDE REGEX "prx\\.ld$")
set_property(TARGET os0_kd_ss_mgr PROPERTY LINK_OPTIONS "${SS_MGR_LINK_OPTIONS}")
set(SS_MGR_QAF_BSS_ORDER_4.00-DEBUGGER
	qafSysconVersion qafCachedSysconFlag qafSysconFlagInitialized qafBaryonVersion)
foreach(SS_MGR_RELEASE_FW 3.650.011-TOOL 3.630.011-DEBUGGER 3.65-CEX)
	set(SS_MGR_QAF_BSS_ORDER_${SS_MGR_RELEASE_FW}
		qafBaryonVersion qafSysconVersion qafCachedSysconFlag qafSysconFlagInitialized)
endforeach()
fw_value(SS_MGR_QAF_BSS_ORDER SS_MGR_QAF_BSS_ORDER)
set(SS_MGR_DATA_BASE_4.00-DEBUGGER 0x81007000)
foreach(SS_MGR_RELEASE_FW 3.650.011-TOOL 3.630.011-DEBUGGER 3.65-CEX)
	set(SS_MGR_DATA_BASE_${SS_MGR_RELEASE_FW} 0x81006000)
endforeach()
fw_value(SS_MGR_DATA_BASE SS_MGR_DATA_BASE)
set(SS_MGR_QAF_BSS_INPUTS "")
foreach(SS_MGR_QAF_GLOBAL IN LISTS SS_MGR_QAF_BSS_ORDER)
	string(APPEND SS_MGR_QAF_BSS_INPUTS
		"\t\t*(.bss.ssMgr.${SS_MGR_QAF_GLOBAL})\n")
endforeach()
configure_file(fw/layout.ld "${CMAKE_CURRENT_BINARY_DIR}/layout.ld" @ONLY)
target_link_options(os0_kd_ss_mgr BEFORE PRIVATE
	"-Wl,-T,${CMAKE_CURRENT_BINARY_DIR}/layout.ld")
