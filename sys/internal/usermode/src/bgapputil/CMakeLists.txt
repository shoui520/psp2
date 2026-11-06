enable_language(CXX)
# SceBgAppUtil (vs0:sys/external/bgapputil.suprx), built by Sony with SNC 3.57 in C++ (the shell's
# IPMI client)
psp2_module(vs0_sys_external_bgapputil KIND user COMPILER snc SOURCES
	bgapputil.cpp
	bgapputil_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
