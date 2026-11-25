enable_language(CXX)
# SceCamImportDialogPlugin (vs0:sys/external/libcdlg_cameraimport.suprx), built by Sony with SNC
# 3.57 in C++: the camera import dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_cameraimport KIND user COMPILER snc SOURCES
	src/cameraimport_dialog_plugin.cpp
	src/cameraimport_dialog_view.cpp
	src/camera_view_widget.cpp
	src/motion_listener.cpp
	src/xui_shell_util.cpp
	src/camera_core.cpp
	src/picture_file_creator.cpp
	../photoreview_dialog/src/event_dispatcher.cpp
	../photoreview_dialog/src/input_listener.cpp
	../photoreview_dialog/src/dialog_page.cpp
	../photoreview_dialog/src/plugin_app.cpp
	../photoreview_dialog/src/gui_dialog.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_cameraimport PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_cameraimport PRIVATE
	PSP2_STRIP_UNUSED
	CDLG_NO_DIALOG_PAGE_SLIDEOUT
)
target_link_options(vs0_sys_external_libcdlg_cameraimport PRIVATE -Wl,--gc-sections)
