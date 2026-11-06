enable_language(CXX)
# SceIncomingDialog (vs0:sys/external/incoming_dialog.suprx), built by Sony with SNC 3.57 in C++
# (the shell's IPMI client)
psp2_module(vs0_sys_external_incoming_dialog KIND user COMPILER snc SOURCES
	incoming_dialog.cpp
	incoming_dialog_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
