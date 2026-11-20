enable_language(CXX)
# SceCommonGuiDialog (vs0:sys/external/common_gui_dialog.suprx), built by Sony with SNC 3.57 in
# C++: the dialogs paf applications and the common dialogs show. Its paf imports take libpaf's
# link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_common_gui_dialog KIND user COMPILER snc SOURCES
	module.cpp
	layouts.cpp
	error_dialog.cpp
	list_dialog_param.cpp
	common_gui_dialog.cpp
	input_dialog.cpp
	dialog.cpp
	${PSP2_SRC}/vsh/cdlg/src/msg_dialog/msg_dialog_util.cpp
	${PSP2_SRC}/vsh/lowlayer/src/shell_svc/weak_call.cpp
)
target_include_directories(vs0_sys_external_common_gui_dialog PRIVATE
	"${PSP2_SRC}/vsh/paf/include" "${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/vsh/cdlg/src/msg_dialog")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_common_gui_dialog PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_common_gui_dialog PRIVATE -Wl,--gc-sections
	# psp2ld kept weak_call.cpp's two wrappers, which nothing calls
	-Wl,--undefined=_Z17shellSvcWeakCall0iii -Wl,--undefined=_Z17shellSvcWeakCall1iii)
