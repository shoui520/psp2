enable_language(CXX)
# SceTwLoginDialogPlugin (vs0:sys/external/libcdlg_tw_login.suprx), built by Sony with SNC 3.57
# in C++: the Twitter login common dialog plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_tw_login KIND user COMPILER snc SOURCES
	tw_login_dialog_plugin.cpp
	tw_lx_util.cpp
	tw_lx_http.cpp
	tw_lx_oauth.cpp
	tw_dialog_common.cpp
	../msg_dialog/msg_dialog_util.cpp
	../../../../sys/sdk/target/src/hmac/sha1.c
	../../../../sys/sdk/target/src/hmac/libhmac.c
	../../../../sys/sdk/target/src/sha1/libsha1.c
	../../../../sys/internal/kernel/src/sysmem/sha1_transform.S
	../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../sys/sdk/target/src/libsnc/i64shr.c
	../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_tw_login PRIVATE "${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf")
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_tw_login PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_tw_login PRIVATE -Wl,--gc-sections)
# The SDK's libSceHmac and libSceSha1, which Sony built with GCC and without stack guards
set_source_files_properties(
	../../../../sys/sdk/target/src/hmac/sha1.c
	../../../../sys/sdk/target/src/hmac/libhmac.c
	../../../../sys/sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(../../../../sys/internal/kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
