# Complete native graph; behavioral verification remains open.
# zlib (SceZlibForDriver) is Sony's zlib 1.2.5 built from contrib.
include(zlib/zlib.cmake)
psp2_zlib_sce(post_ss_mgr_zlib)
set_source_files_properties(${post_ss_mgr_zlib_SOURCES} PROPERTIES COMPILE_OPTIONS -ffunction-sections)
psp2_module(os0_kd_bootimage_post_ss_mgr KIND kernel COMPILER gcc48 SOURCES
	post_ss_mgr_startstop.c
	sealedkey.c
	keystone.c
	portability.c
	${post_ss_mgr_zlib_SOURCES}
	pm_mode.c
	pm_mgr.c
	pm_syscon_jig.c
	rtcmgr.c
	licmgr.c
	ut_mgr.c
	spfso.c
	fwloader.c
	rsa.c
)
target_include_directories(os0_kd_bootimage_post_ss_mgr PRIVATE "${post_ss_mgr_zlib_INCLUDE}")
if(TARGET os0_kd_bootimage_post_ss_mgr)
	get_target_property(type os0_kd_bootimage_post_ss_mgr TYPE)
	if(type STREQUAL "EXECUTABLE")
		target_link_options(os0_kd_bootimage_post_ss_mgr PRIVATE -Wl,--gc-sections
			-Wl,-Map=$<TARGET_FILE:os0_kd_bootimage_post_ss_mgr>.map)
	endif()
endif()
