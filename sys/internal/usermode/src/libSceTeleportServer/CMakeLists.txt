enable_language(CXX)
# SceTeleportServer (vs0:sys/external/libSceTeleportServer.suprx), built by Sony with SNC 3.57 in
# C++ (the shell's IPMI client); the IPC method links first
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	teleport_server_ipc_client.cpp
	teleport_server.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
)
