enable_language(CXX)
# SceDtcpIp (vs0:sys/external/libSceDtcpIp.suprx), built by Sony with SNC 3.57 in C. No
# debug record names its files; the files are named by what they hold (ours).
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
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/weak_calls.c
)
