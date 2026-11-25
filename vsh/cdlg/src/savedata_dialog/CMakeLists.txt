enable_language(CXX)
# SceSaveDataDialogPlugin (vs0:sys/external/libcdlg_savedata.suprx), built by Sony with SNC 3.57 in C++.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_savedata KIND user COMPILER snc SOURCES
	savedata_dialog_plugin.cpp
	savedata_job.cpp
	savedata_dialog.cpp
	savedata_list_view.cpp
	filelist_view.cpp
	../msg_dialog/msg_dialog_util.cpp
	../companion_dialog/src/furl_stack.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_savedata PRIVATE
	"${PSP2_SRC}/vsh/cdlg/src/savedata_dialog"
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
target_compile_definitions(vs0_sys_external_libcdlg_savedata PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_savedata PRIVATE -Wl,--gc-sections)
