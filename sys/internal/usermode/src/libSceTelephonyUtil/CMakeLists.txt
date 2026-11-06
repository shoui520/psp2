enable_language(CXX)
# SceTelephonyUtil (vs0:sys/external/libSceTelephonyUtil.suprx), built by Sony with SNC 3.57 in
# C++ (the shell's IPMI client)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	telephony_util.cpp
	telephony_util_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
