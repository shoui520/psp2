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
	shell.c
)
target_include_directories(vs0_vsh_shell_shell PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf" "${PSP2_SRC}/contrib/sqlite-3.7.6.3" "${PSP2_SRC}/vsh/app/common/src"
	"${PSP2_SRC}/sys/internal/usermode/src" "${PSP2_SRC}/vsh/lowlayer/src")
# SNC's inlining budget: map::operator[] (0x814096ac) stays out of line in Sony's callers
set_source_files_properties(boot.cpp lsdb_start.cpp net.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
