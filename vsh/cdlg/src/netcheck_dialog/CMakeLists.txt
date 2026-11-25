enable_language(CXX)
# SceNetCheckDialogPlugin (vs0:sys/external/libcdlg_netcheck.suprx), built by Sony
# with SNC 3.57 in C++: network connectivity and PSN check common dialog plugin.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_netcheck KIND user COMPILER snc SOURCES
	netcheck_dialog_plugin.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_netcheck PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
target_compile_definitions(vs0_sys_external_libcdlg_netcheck PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_netcheck PRIVATE -Wl,--gc-sections)
