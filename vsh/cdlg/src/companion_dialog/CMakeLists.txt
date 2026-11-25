enable_language(CXX)
# SceCompanionDialogPlugin (vs0:sys/external/libcdlg_companion.suprx), built by Sony with SNC 3.57
# in C++: the PS4 Link / Companion common dialog plugin. Its paf imports take libpaf's link names.
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(vs0_sys_external_libcdlg_companion KIND user COMPILER snc SOURCES
	companion_dialog_plugin.cpp
	src/furl_stack.cpp
	src/remote_play/remoteplay_db.cpp
	src/remote_play/remoteplay_regist.cpp
	src/remote_play/ui/remoteplay_ui_devreg.cpp
	../../../../sys/sdk/target/src/hmac/sha256.c
	../../../../sys/sdk/target/src/hmac/libhmac.c
	../../../../sys/sdk/target/src/sha256/libsha256.c
	../../../../sys/sdk/target/src/sha256/sha256.S
	../../../../sys/sdk/target/src/aes/libaes.c
	../../../../sys/sdk/target/src/aes/aes.S
	../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
target_include_directories(vs0_sys_external_libcdlg_companion PRIVATE
	"${PSP2_SRC}/vsh/paf/include"
	"${PSP2_SRC}/vsh/paf"
	"${PSP2_SRC}/vsh/lowlayer/src/common_gui_dialog"
	"${PSP2_SRC}/vsh/cdlg/src/common_dialog_main"
	"${PSP2_SRC}/sys/sdk/target/include"
	"${PSP2_SRC}/sys/sdk/target/src/hmac"
)
# Sony linked with psp2ld --strip-unused: a section per SNC function, and --gc-sections
target_compile_definitions(vs0_sys_external_libcdlg_companion PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libcdlg_companion PRIVATE -Wl,--gc-sections)
# The SDK's libSceHmac, libSceSha256 and libSceAes, which Sony built with GCC and without stack guards
set_source_files_properties(
	../../../../sys/sdk/target/src/hmac/sha256.c
	../../../../sys/sdk/target/src/hmac/libhmac.c
	../../../../sys/sdk/target/src/sha256/libsha256.c
	../../../../sys/sdk/target/src/aes/libaes.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(
	../../../../sys/sdk/target/src/sha256/sha256.S
	../../../../sys/sdk/target/src/aes/aes.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
