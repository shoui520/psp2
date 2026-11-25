enable_language(CXX)
# SceNpSnsFacebookDialogPlugin (vs0:sys/external/libcdlg_np_sns_fb.suprx), built by Sony with SNC
# 3.57 in C++: the Facebook permission / feed dialog's plugin. Its paf imports take libpaf's link
# names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_np_sns_fb KIND user COMPILER snc SOURCES
	fb_dialog_plugin.cpp
	fb_dialog_job.cpp
	fb_dialog_view_mgr.cpp
	fb_dialog_controller.cpp
	fb_dialog_views.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_np_sns_fb PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_np_sns_fb PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_np_sns_fb PRIVATE -Wl,--gc-sections)
