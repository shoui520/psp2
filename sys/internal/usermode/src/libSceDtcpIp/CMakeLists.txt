enable_language(CXX)
# SceDtcpIp (vs0:sys/external/libSceDtcpIp.suprx), built by Sony with SNC 3.57 in C. No
# debug record names its files; the files are named by what they hold (ours). The SNC
# runtime objects the original carries link after the library's files.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc PARTIAL SOURCES
	dtcpip.c
	dtcpip_locks.c
	dtcpip_core.c
	dtcpip_memory.c
	dtcpip_sink.c
	dtcpip_move_sink.c
	dtcpip_sink_base.c
	dtcpip_stream.c
	dtcpip_http.c
	dtcpip_swipc.c
	dtcpip_swpipe.c
	dtcpip_swsys.c
	dtcpip_swstrm.c
	dtcpip_runtime.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/weak_calls.c
)
# libsnc's i32ops.c without the signed division the library does not call
target_compile_definitions(${PSP2_TARGET} PRIVATE LIBSNC_OMIT_SIGNED_DIVISION)
# the module's original carries both weak-call stubs (the flag 0 and flag 1 forms)
target_compile_definitions(${PSP2_TARGET} PRIVATE SNC_WEAK_CALLS_ON)
# Sony's linker kept only the runtime routines the library calls
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
