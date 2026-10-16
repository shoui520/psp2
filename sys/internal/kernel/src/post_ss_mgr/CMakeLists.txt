# Complete native graph; behavioral verification remains open.
# zlib (SceZlibForDriver) is Sony's zlib 1.2.5 built from contrib.
include(${PSP2_SRC}/common/zlib/zlib.cmake)
psp2_zlib_sce(post_ss_mgr_zlib)
psp2_module(os0_kd_bootimage_post_ss_mgr KIND kernel SKELETON SOURCES
	source/post_ss_mgr.c
	source/post_ss_mgr_startstop.c
	source/sealedkey.c
	source/keystone.c
	source/portability.c
	${post_ss_mgr_zlib_SOURCES}
	source/pm_mode.c
	source/pm_mgr.c
	source/pm_syscon_jig.c
	source/rtcmgr.c
	source/licmgr.c
	source/ut_mgr.c
	source/spfso.c
	source/fwloader.c
	source/rsa.c
)
target_include_directories(os0_kd_bootimage_post_ss_mgr PRIVATE "${post_ss_mgr_zlib_INCLUDE}")
