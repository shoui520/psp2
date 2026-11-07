enable_language(CXX)
# SceCommonDialogMain (vs0:sys/external/libcdlg_main.suprx), built by Sony with SNC 3.57 in C++
# (vsh/cdlg/src/cdlg_main/common_dialog_main.cpp in the 0.9x builds' paths): the common dialog's
# framework, plugin loads and dimmer, run by libcdlg's worker thread. Its paf imports take
# libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_main KIND user COMPILER snc SOURCES
	common_dialog_main.cpp
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_main PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
set_source_files_properties(common_dialog_main.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
# Sony linked with psp2ld --strip-unused, which drops the inline destructors SNC emits but nothing
# calls (paf::Plugin::InitParam, Argument, the argument map and CommonDialogPluginParam): a
# section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_main PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_main PRIVATE -Wl,--gc-sections)
