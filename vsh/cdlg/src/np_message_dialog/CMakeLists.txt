enable_language(CXX)
# SceNpMessageDialogPlugin (vs0:sys/external/libcdlg_np_message.suprx), built by Sony with
# SNC 3.57 in C++: the NP message dialog plugin and embedded SceFriendSelect / SceFriendLib.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_np_message KIND user COMPILER snc SOURCES
	np_message_dialog_plugin.cpp
	../friendlist2_dialog/src/friend_select_plugin.cpp
	../friendlist2_dialog/src/friend_select_webapi.cpp
	../friendlist2_dialog/src/friend_lib.cpp
	../friendlist2_dialog/src/friend_lib_http.cpp
	../friendlist2_dialog/src/popup_list_view.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_np_message PRIVATE
	"${PSP2_SRC}/vsh/cdlg/src/np_message_dialog"
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
target_compile_definitions(vs0_sys_external_libcdlg_np_message PRIVATE
	PSP2_STRIP_UNUSED
	NP_MESSAGE_DIALOG_NO_COMMON_GUI_DIALOG
)
target_link_options(vs0_sys_external_libcdlg_np_message PRIVATE -Wl,--gc-sections)
