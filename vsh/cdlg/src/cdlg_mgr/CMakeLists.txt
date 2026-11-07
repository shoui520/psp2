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
	remote_osk_dialog.cpp
	pocketstation_dialog.cpp
	worker_thread.cpp
	common_dialog.cpp
	savedata_dialog.cpp
	netcheck_dialog.cpp
	ime_dialog.cpp
	msg_dialog.cpp
	np_message_dialog.cpp
	friendlist_dialog.cpp
	photoimport_dialog.cpp
	npprofile_dialog.cpp
	trophy_setup_dialog.cpp
	photoreview_dialog.cpp
	store_checkout_dialog.cpp
	cameraimport_dialog.cpp
	compat_dialog.cpp
	np_sns_facebook_dialog.cpp
	npeula_dialog.cpp
	cross_controller_dialog.cpp
	tw_dialog.cpp
	tw_login_dialog.cpp
	near_dialog.cpp
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
set_source_files_properties(remote_osk_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(pocketstation_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(worker_thread.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(common_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(savedata_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(netcheck_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(ime_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(msg_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(np_message_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(friendlist_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(photoimport_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(npprofile_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(trophy_setup_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(photoreview_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(store_checkout_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(cameraimport_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(compat_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(np_sns_facebook_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(npeula_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(cross_controller_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(tw_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(tw_login_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(near_dialog.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
