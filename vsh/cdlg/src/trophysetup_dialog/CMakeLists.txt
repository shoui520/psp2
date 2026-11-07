enable_language(CXX)
# SceTrophySetupDialogPlugin (vs0:sys/external/libcdlg_np_trophy_setup.suprx), built by Sony with
# SNC 3.57 in C++: the trophy setup dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_np_trophy_setup KIND user COMPILER snc SOURCES
	np_trophy_setup_dialog_plugin.cpp
	open_job.cpp
	page.cpp
	setup.cpp
	setup_job.cpp
	dialog_controller.cpp
	dialogs.cpp
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(vs0_sys_external_libcdlg_np_trophy_setup PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_np_trophy_setup PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_np_trophy_setup PRIVATE -Wl,--gc-sections)
