enable_language(CXX)
# SceNearDialogPlugin (vs0:sys/external/libcdlg_near.suprx), built by Sony with SNC 3.57 in C++:
# the near dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(vs0_sys_external_libcdlg_near KIND user COMPILER snc SOURCES
	near_dialog_plugin.cpp
	near_dialog.cpp
	near_dialog_util.cpp
	near_dialog_state.cpp
	near_finish_info_list.cpp
	near_checkin_lib.cpp
	near_checkin_lib_job.cpp
	near_checkin_lib_connect_job.cpp
	near_checkin_lib_status_job.cpp
	near_checkin_lib_term_job.cpp
	near_bg_service.cpp
	near_bg_service_client.cpp
	near_bg_service_ipc_client.cpp
	near_auto_checkin_param.cpp
	../../../lowlayer/src/shell_svc/weak_call.cpp
)
target_include_directories(vs0_sys_external_libcdlg_near PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_near PRIVATE PSP2_STRIP_UNUSED)
# psp2ld kept the service client's Update and weak_call.cpp's two wrappers, which nothing calls
target_link_options(vs0_sys_external_libcdlg_near PRIVATE -Wl,--gc-sections
	-Wl,--undefined=_ZN19NearBGServiceClient6UpdateEv
	-Wl,--undefined=_Z17shellSvcWeakCall0iii -Wl,--undefined=_Z17shellSvcWeakCall1iii)
