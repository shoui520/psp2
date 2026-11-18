enable_language(CXX)
# SceShell (vs0:vsh/shell/shell.self), built by Sony with SNC 3.57 in C++: the system software's
# shell, with its resident plugins linked in. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_vsh_shell_shell KIND user COMPILER snc PARTIAL SOURCES
	boot.cpp
	boot_screen.cpp
	boot_tasks.cpp
	lsdb_start.cpp
	framework.cpp
	modules.cpp
	mtp.cpp
	net.cpp
	event_server.cpp
	start.cpp
	startup.cpp
	theme.cpp
	trigger_mgr_start.cpp
	${PSP2_SRC}/vsh/paf/src/prx_loader.cpp
	${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c
	${PSP2_SRC}/vsh/app/game/game_plugin/game_plugin.cpp
	savedata_backup.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/install_cleanup.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/appinfo.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/game_lsdb.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/mounter.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/license.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/gc_promote.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/version_check.cpp
	${PSP2_SRC}/vsh/app/gamedata_plus_util/src/plus_app_list.cpp
	${PSP2_SRC}/vsh/app/gamedata_plus_util/src/plus_sd_list.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/game_main.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/param_job.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/theme_install.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/eboot_sig.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/sfo_reader.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/eboot_param.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/sfo_param.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/theme_reader.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/ps1_discinfo.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/npdrm_license.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/ur0_setup.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/theme_copy.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/up_status.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/license_file.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/promoter.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/promote_patch.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/install_job.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/install_ex_job.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/app_promoter.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/app_source.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/psp_source.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/update_checker.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/livearea_copier.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/psm_livearea_writer.cpp
	${PSP2_SRC}/vsh/app/game/game_plugin/appmeta.cpp
	shell.c
)
target_include_directories(vs0_vsh_shell_shell PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src" "${PSP2_SRC}/vsh/lowlayer/src")
# SNC's inlining budget: map::operator[] (0x814096ac) stays out of line in Sony's callers
set_source_files_properties(boot.cpp lsdb_start.cpp net.cpp event_server.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
# the 3.65 SDK's start code
set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c PROPERTIES
	COMPILE_DEFINITIONS SCE_CRT0_SDK_VERSION=0x03650011)
