enable_language(CXX)
# SceFriendList2DialogPlugin (vs0:sys/external/libcdlg_friendlist2.suprx), built by Sony with
# SNC 3.57 in C++: the friend list 2 dialog's plugin and embedded SceFriendSelect / SceFriendLib.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_friendlist2 KIND user COMPILER snc SOURCES
	src/friendlist2_dialog_plugin.cpp
	src/friend_select_plugin.cpp
	src/friend_select_webapi.cpp
	src/friend_lib.cpp
	src/friend_lib_http.cpp
	src/popup_list_view.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_friendlist2 PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_friendlist2 PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_friendlist2 PRIVATE -Wl,--gc-sections)
