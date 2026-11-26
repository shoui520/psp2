enable_language(CXX)
# SceLibGameUpdate (vs0:sys/external/libgameupdate.suprx), built by Sony with SNC 3.57
# in C++ and statically linked against SceNpCommon's XML parser, libSceHmac, and libSceSha256.
psp2_module(vs0_sys_external_libgameupdate KIND user COMPILER snc SOURCES
	game_update.cpp
	download_client.cpp
	../../../../../vsh/app/common/src/np/np_common/xml.c
	shell_svc_patchcheck.cpp
	../../../../sdk/target/src/hmac/sha256.c
	../../../../sdk/target/src/hmac/libhmac.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/sha256/libsha256.c
	../../../../sdk/target/src/sha256/sha256.S
)
target_include_directories(vs0_sys_external_libgameupdate PRIVATE
	"${PSP2_SRC}/vsh/lowlayer/src"
	"${PSP2_SRC}/vsh/app/common/src/np/np_common"
	"${PSP2_SRC}/sys/sdk/target/src/hmac"
)
# Sony linked with psp2ld --strip-unused
target_compile_definitions(vs0_sys_external_libgameupdate PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libgameupdate PRIVATE -Wl,--gc-sections)
# The SDK's libSceHmac and libSceSha256, built by Sony with GCC and without stack guards
set_source_files_properties(
	../../../../sdk/target/src/hmac/sha256.c
	../../../../sdk/target/src/hmac/libhmac.c
	../../../../sdk/target/src/sha256/libsha256.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(
	../../../../sdk/target/src/sha256/sha256.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
