enable_language(CXX)
# SceCommonDialog (vs0:sys/external/libcdlg.suprx), built by Sony with SNC 3.57 in C++: the
# manager every common dialog shares (common_dialog_mgr.cpp), the worker thread and each
# dialog's API. Its paf import takes libpaf's link name. Being converted: the skeleton's stubs
# (libcdlg.c) are left out until the last dialog is C.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg KIND user COMPILER snc PARTIAL SOURCES
	common_dialog_mgr.cpp
	videoimport_dialog.cpp
	calendar_review_dialog.cpp
	friendlist2_dialog.cpp
	companion_util_dialog.cpp
	game_custom_data_dialog.cpp
	invitation_dialog.cpp
	worker_thread.cpp
)
target_include_directories(vs0_sys_external_libcdlg PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg PRIVATE -Wl,--gc-sections)
set_source_files_properties(common_dialog_mgr.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(videoimport_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(calendar_review_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(friendlist2_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(companion_util_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(game_custom_data_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(invitation_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(worker_thread.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
