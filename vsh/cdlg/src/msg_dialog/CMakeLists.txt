enable_language(CXX)
# SceMsgDialogPlugin (vs0:sys/external/libcdlg_msg.suprx), built by Sony with SNC 3.57 in C++:
# the message dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_msg KIND user COMPILER snc SOURCES
	msg_dialog_plugin.cpp
	msg_dialog.cpp
	msg_dialog_util.cpp
	msg_dialog_param.cpp
	../../../lowlayer/src/shell_svc/weak_call.cpp
)
target_include_directories(vs0_sys_external_libcdlg_msg PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_msg PRIVATE PSP2_STRIP_UNUSED)
# psp2ld kept weak_call.cpp's two wrappers, which nothing calls
target_link_options(vs0_sys_external_libcdlg_msg PRIVATE -Wl,--gc-sections
	-Wl,--undefined=_Z17shellSvcWeakCall0iii -Wl,--undefined=_Z17shellSvcWeakCall1iii)
