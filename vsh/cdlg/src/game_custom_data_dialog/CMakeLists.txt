enable_language(CXX)
# SceGameCustomDataDlgPlugin (vs0:sys/external/libcdlg_game_custom_data.suprx), built by Sony
# with SNC 3.57 in C++: the game custom data dialog's plugin, Sony's copy of the invitation
# dialog's, which it builds with GAME_CUSTOM_DATA_DIALOG; and the NP community helper. Its paf
# imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_game_custom_data KIND user COMPILER snc SOURCES
	../invitation_dialog/invitation_dialog_plugin.cpp
	../../../app/common/src/np/np_community/np_community.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(vs0_sys_external_libcdlg_game_custom_data PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
target_compile_definitions(vs0_sys_external_libcdlg_game_custom_data PRIVATE GAME_CUSTOM_DATA_DIALOG)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_game_custom_data PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_game_custom_data PRIVATE -Wl,--gc-sections)
set_source_files_properties(../invitation_dialog/invitation_dialog_plugin.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
