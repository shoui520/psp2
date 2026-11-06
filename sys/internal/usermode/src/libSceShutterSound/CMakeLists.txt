enable_language(CXX)
# SceShutterSound (vs0:sys/external/libSceShutterSound.suprx), built by Sony with SNC 3.57 in C++
# (the shell's IPMI client)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	shutter_sound.cpp
	shutter_sound_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
