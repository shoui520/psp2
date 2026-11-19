enable_language(CXX)
# SceMailApiForLocal (vs0:vsh/common/mail_api_for_local.suprx), built by Sony with SNC 3.57 in C++
# on paf: the local mail API's message writer, with the paf PRX loader and the SDK's start code
# linked in. No debug record names its file; the file is named by the module (ours).
set(MODULE_IMPORT_NAMES vsh/paf/fw/3.65-CEX.exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	mail_api_for_local.cpp
	${PSP2_SRC}/vsh/paf/src/prx_loader.cpp
	${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c
	${PSP2_SRC}/sys/sdk/target/src/libsnc/weak_calls.c
)
target_include_directories(${PSP2_TARGET} PRIVATE "${PSP2_SRC}/vsh/paf/include" "${PSP2_SRC}/vsh/paf")
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
# the 3.65 SDK's start code
set_source_files_properties(${PSP2_SRC}/sys/sdk/target/src/crt0/crt0_main.c PROPERTIES
	COMPILE_DEFINITIONS "SCE_CRT0_SDK_VERSION=0x03650011;exit=sce_paf_private_exit")
# Sony's build kept the loader's unused unload call
set_source_files_properties(${PSP2_SRC}/vsh/paf/src/prx_loader.cpp PROPERTIES
	COMPILE_DEFINITIONS PAF_PRX_LOADER_KEEP_UNLOAD)
