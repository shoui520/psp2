enable_language(CXX)
# SceCheckoutDialogPlugin (vs0:sys/external/libcdlg_checkout.suprx), built by Sony with SNC 3.57
# in C++: the store checkout dialog's plugin, which loads store_checkout_plugin. Its paf imports
# take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_checkout KIND user COMPILER snc SOURCES
	checkout_dialog_plugin.cpp
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
target_include_directories(vs0_sys_external_libcdlg_checkout PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_checkout PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_checkout PRIVATE -Wl,--gc-sections)
set_source_files_properties(checkout_dialog_plugin.cpp PROPERTIES COMPILE_OPTIONS -Xautoinlinesize=28)
