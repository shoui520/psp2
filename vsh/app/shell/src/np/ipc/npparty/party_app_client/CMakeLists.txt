enable_language(CXX)
# SceNpPartyAppUtil (vs0:app/NPXS10001/np_party_app.suprx), built by Sony with SNC 3.57 in C++:
# the party app's client of the shell's party service
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	party_app_client.cpp
	party_system.cpp
	party_app_ipc_client.cpp
	../../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
