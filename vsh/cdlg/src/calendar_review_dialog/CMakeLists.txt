enable_language(CXX)
# SceCalendarDialogPlugin (vs0:sys/external/libcdlg_calendar_review.suprx), built by Sony
# with SNC 3.57 in C++: calendar event review common dialog plugin.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_calendar_review KIND user COMPILER snc SOURCES
	src/calendar_review_dialog_plugin.cpp
	../companion_dialog/src/furl_stack.cpp
	../friendlist2_dialog/src/popup_list_view.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_calendar_review PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
)
target_compile_definitions(vs0_sys_external_libcdlg_calendar_review PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_calendar_review PRIVATE -Wl,--gc-sections)
