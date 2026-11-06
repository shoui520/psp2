enable_language(CXX)
# SceDbrecoveryUtility (vs0:sys/external/dbrecovery_utility.suprx), built by Sony with SNC 3.57
# in C++ (paf's strings and vectors in the dialog's parameters): its paf imports take libpaf's
# link names.
# budgets 28 to 48 inline the dialog parameters' destructor, and every vector's but the
# items', as Sony's build did (2026-11-06)
set(MODULE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:C,CXX>:-Xautoinlinesize=28>)
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_dbrecovery_utility KIND user COMPILER snc SOURCES
	dbrecovery_utility.cpp
	dbrecovery_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(vs0_sys_external_dbrecovery_utility PRIVATE
	"${PSP2_SRC}/vsh/paf/include" "${PSP2_SRC}/vsh/paf")
# SNC writes the inline destructors out of line as well; Sony's linker kept only what is called
target_link_options(vs0_sys_external_dbrecovery_utility PRIVATE -Wl,--gc-sections)
