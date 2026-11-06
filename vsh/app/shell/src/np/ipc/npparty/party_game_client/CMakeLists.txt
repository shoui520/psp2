enable_language(CXX)
# SceNpPartyGameUtil (vs0:sys/external/np_party.suprx), built by Sony with SNC 3.57 in C++: the
# party library's client of the shell's party service
# its SceIpmi imports take the C++ link names SceShellSvc exports them under
set(MODULE_IMPORT_NAMES vsh/lowlayer/src/shell_svc/exports.yml)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	party_game_client.cpp
	party_game_system.cpp
	party_game_ipc_client.cpp
	../../../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
)
