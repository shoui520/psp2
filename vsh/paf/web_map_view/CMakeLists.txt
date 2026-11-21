enable_language(CXX)
# ScePafWebMapView (vs0:sys/external/libpaf_web_map_view.suprx), built by Sony with SNC 3.57 in
# C++: paf's web view widget as a library. The web view code is the email app's too, and is
# compiled from vsh/app/common/src/webview. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
set(WEBVIEW ../../app/common/src/webview)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	module.cpp
	${WEBVIEW}/context_menu_info.cpp
	${WEBVIEW}/web_view.cpp
	${WEBVIEW}/web_ime.cpp
	${WEBVIEW}/web_ipc.cpp
	${WEBVIEW}/web_view_plane.cpp
	${WEBVIEW}/web_view_gesture.cpp
	${WEBVIEW}/web_ipc_channel.cpp
	${WEBVIEW}/finger_pointer.cpp
	${WEBVIEW}/tilebackend.cpp
	${WEBVIEW}/cg/programs.S
	${WEBVIEW}/tilebackend_pool.cpp
	${WEBVIEW}/geolocation_monitor.cpp
	../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../sys/sdk/target/src/libsnc/weak_calls.c
	../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED SNC_WEAK_CALLS_ON)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# the tile backend's GPU programs, from their Cg sources
include("${PSP2_SRC}/vsh/paf/cg/cg.cmake")
psp2_paf_cg(${PSP2_TARGET} ${WEBVIEW}/cg)
