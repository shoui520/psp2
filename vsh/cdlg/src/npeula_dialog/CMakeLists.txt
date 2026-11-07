enable_language(CXX)
# SceNpEulaDialogPlugin (vs0:sys/external/libcdlg_npeula.suprx), built by Sony with SNC 3.57 in
# C++: the NP EULA dialog's plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml vsh/cdlg/src/npeula_dialog/state_check.imports.yml)
psp2_module(vs0_sys_external_libcdlg_npeula KIND user COMPILER snc SOURCES
	npeula_dialog_plugin.cpp
	eula_view.cpp
	eula_index.cpp
	eula_index_parser.cpp
	eula_http_job.cpp
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_npeula PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_npeula PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_npeula PRIVATE -Wl,--gc-sections)
set_source_files_properties(eula_http_job.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=8)
set_source_files_properties(eula_view.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=16)
set_source_files_properties(npeula_dialog_plugin.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=30)
