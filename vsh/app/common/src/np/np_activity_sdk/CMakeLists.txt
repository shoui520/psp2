enable_language(CXX)
# SceNpActivity (vs0:sys/external/np_activity_sdk.suprx), built by Sony with SNC 3.57 in C++
# (the shell's IPMI client)
psp2_module(vs0_sys_external_np_activity_sdk KIND user COMPILER snc SOURCES
	np_activity_sdk.cpp
	np_activity_utf8.cpp
	np_activity_client.cpp
	np_activity_ipc_client.cpp
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
