enable_language(CXX)
# ScePocsDialogPlugin (vs0:sys/external/libcdlg_pocketstation.suprx), built by Sony with SNC 3.57 in C++.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_pocketstation KIND user COMPILER snc SOURCES
	pocketstation_dialog_plugin.cpp
	pocketstation_menu.cpp
	../companion_dialog/src/furl_stack.cpp
	../friendlist2_dialog/src/popup_list_view.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_pocketstation PRIVATE
	"${PSP2_SRC}/vsh/cdlg/src/pocketstation_dialog"
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
target_compile_definitions(vs0_sys_external_libcdlg_pocketstation PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_pocketstation PRIVATE -Wl,--gc-sections)
