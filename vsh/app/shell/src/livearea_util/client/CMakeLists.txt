enable_language(CXX)
# SceLiveAreaUtil (vs0:sys/external/livearea_util.suprx), built by Sony with SNC 3.57 in C++ (the
# shell's IPMI client)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	livearea_util.cpp
	livearea_util_ipc_client.cpp
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
