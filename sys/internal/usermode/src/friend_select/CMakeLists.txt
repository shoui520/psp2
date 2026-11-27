enable_language(CXX)
# SceFriendSelect (vs0:sys/external/friend_select.suprx), built by Sony with
# SNC 3.57 in C++: standalone PAF plugin packaging the SceFriendSelect,
# SceFriendLib, and PopupListView translation units.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_friend_select KIND user COMPILER snc SOURCES
	friend_select_module.cpp
	friend_select_app.cpp
	friend_select_webapi.cpp
	friend_select_ui.cpp
	friend_select_item.cpp
	friend_lib_list.cpp
	friend_lib_core.cpp
	friend_lib_update_button.cpp
	friend_lib_http.cpp
	popup_list_view.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_friend_select PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_friend_select PRIVATE PSP2_STRIP_UNUSED)
target_compile_options(vs0_sys_external_friend_select PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Xautoinlinesize=0>)
target_link_options(vs0_sys_external_friend_select PRIVATE -Wl,--gc-sections)
