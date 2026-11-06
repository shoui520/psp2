enable_language(CXX)
# SceLibLocationInternal (vs0:sys/external/liblocation_internal.suprx), built by Sony with SNC
# 3.57 in C++: the system's own location client, which makes SceLibLocation's list in the system
# mode, with its own build of SceLibLocation's client code (its list without confirmPending)
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	src/location_internal.cpp
	src/location_client.cpp
	src/location_internal_module.cpp
	src/location_service_ipc_client.cpp
	../location_client/src/location_heap.cpp
	../../../../../../lowlayer/src/tinylibc/tinylibc.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
# the list as this build of the client code lays it out (location_client.h)
target_compile_definitions(${PSP2_TARGET} PRIVATE LOCATION_INTERNAL)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
