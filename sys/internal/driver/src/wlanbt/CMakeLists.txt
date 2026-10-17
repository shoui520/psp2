# All original functions have C bodies; verification remains incomplete.
set(wlanbt_upstream "${CMAKE_CURRENT_LIST_DIR}/../../../../../contrib/rt2860-7bc30c23")
set(wlanbt_patched "${CMAKE_CURRENT_BINARY_DIR}/cmm_wpa_sce.c")
configure_file("${wlanbt_upstream}/cmm_wpa.c" "${wlanbt_patched}" COPYONLY)
execute_process(COMMAND patch -s "${wlanbt_patched}" "${wlanbt_upstream}/SCE.patch"
	RESULT_VARIABLE wlanbt_patch_result ERROR_VARIABLE wlanbt_patch_error)
if(NOT wlanbt_patch_result EQUAL 0)
	message(FATAL_ERROR "WlanBt upstream adaptation failed: ${wlanbt_patch_error}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
	"${wlanbt_upstream}/cmm_wpa.c" "${wlanbt_upstream}/SCE.patch")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories(${CMAKE_CURRENT_LIST_DIR}/../../../kernel/src/net_ps/include)
psp2_module(os0_kd_bootimage_wlanbt KIND kernel SOURCES
	wlanbt.c
	wlan_syscall.c
	wlan_util.c
	wlan_bss.c
	wlan_password.c
	robin_common.c
	wlan_dcm.c
	wlan_tcm.c
	wlan_mrvl.c
	wlan_robin.c
)
