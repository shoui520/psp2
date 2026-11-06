enable_language(CXX)
# SceLibLocation (vs0:sys/external/liblocation.suprx), built by Sony with SNC 3.57 in C++: the
# client of the shell's location services (Sony's vsh/app/shell/src/liblocation/src/location_client)
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	src/location.cpp
	src/location_client.cpp
	src/location_service_ipc_client.cpp
	src/location_heap.cpp
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
