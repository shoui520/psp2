enable_language(CXX)
# SceShell (vs0:vsh/shell/shell.self), built by Sony with SNC 3.57 in C++: the system software's
# shell, with its resident plugins linked in. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_vsh_shell_shell KIND user COMPILER snc PARTIAL SOURCES
	boot.cpp
	shell.c
)
target_include_directories(vs0_vsh_shell_shell PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# SNC's inlining budget: map::operator[] (0x814096ac) stays out of line in Sony's callers
set_source_files_properties(boot.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
