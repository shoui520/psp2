enable_language(CXX)
# SceTriggerUtil (vs0:sys/external/trigger_util.suprx), built by Sony with SNC 3.57 in C++: the
# shell's timer service client, on SceTinyLibc's runtime
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	trigger_util.cpp
	trigger_util_ipc_client.cpp
	../../../../lowlayer/src/tinylibc/aeabi_atexit.c
	../../../../lowlayer/src/tinylibc/cxa_atexit.c
	../../../../lowlayer/src/tinylibc/tinylibc.c
	../../../../lowlayer/src/tinylibc/term.c
	../../../../lowlayer/src/tinylibc/alloc.c
	../../../../lowlayer/src/tinylibc/malloc.c
	../../../../lowlayer/src/tinylibc/cxxabi.cpp
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
