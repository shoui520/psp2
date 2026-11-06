enable_language(CXX)
# SceLibLocationExtension (vs0:sys/external/liblocation_extension.suprx), built by Sony with SNC
# 3.57 in C++: the Wi-Fi history requests on SceLibLocation's handles, with SceTinyLibc's start
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	src/location_extension.cpp
	src/location_service_ipc_client.cpp
	../../../../../../lowlayer/src/tinylibc/tinylibc.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
