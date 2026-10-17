# Complete native graph; behavioral verification remains open.
# zlib (SceZlibForDriver) is Sony's zlib 1.2.5 built from contrib.
include(zlib/zlib.cmake)
psp2_zlib_sce(post_ss_mgr_zlib)
psp2_module(os0_kd_bootimage_post_ss_mgr KIND kernel COMPILER gcc48 SKELETON SOURCES
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
