enable_language(CXX)
# SceLibLocationFactory (vs0:sys/external/liblocation_factory.suprx), built by Sony with SNC
# 3.57 in C++: the location service's factory commands, on SceLibLocation's heap
# (location_heap.cpp) and SceTinyLibc's runtime
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	src/location_factory.cpp
	src/location_service_ipc_client.cpp
	src/location_factory_module.cpp
	../location_client/src/location_heap.cpp
	../../../../../../lowlayer/src/tinylibc/cxa_atexit.c
	../../../../../../lowlayer/src/tinylibc/tinylibc.c
	../../../../../../lowlayer/src/tinylibc/term.c
	../../../../../../lowlayer/src/tinylibc/malloc.c
	../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
# the heap's memory block is the debug client's
set_source_files_properties(../location_client/src/location_heap.cpp
	PROPERTIES COMPILE_DEFINITIONS LOCATION_HEAP_DEBUG_CLIENT)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
