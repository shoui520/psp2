# swagner (vs0:app/NPXS10080/eboot.bin), built by Sony with GCC 4.8 in C and C++. No
# debug record names its files; the build-path strings name Sony's dtcpip repository
# (DTCPIP-MW, DTCPIP-SW, SWKeyManager) and the files are placed there. The SDK's start
# code (the 3.65 variant) links first.
enable_language(CXX)
psp2_module(vs0_app_NPXS10080_eboot KIND user COMPILER snc PARTIAL SOURCES
	crt0.c
	main.cpp
	eboot.c
	DTCPIP-MW/Src/Activate/dtcpip_activate.c
	DTCPIP-MW/Src/Comm/vita/SwMwCommServer.cpp
	DTCPIP-MW/Src/Mw/dtcpip_download.c
	DTCPIP-MW/Src/Mw/SwMwMain.cpp
	DTCPIP-MW/Src/Mw/SwMwManagerDispatch.cpp
	DTCPIP-MW/Src/Mw/SwMwSinkDispatch.cpp
	DTCPIP-MW/Src/Mw/SwMwSync.cpp
	DTCPIP-MW/Src/Mw/SwMwTrace.c
)

# The app's own code is SNC's -O0 (every local in its stack slot, every call result stored
# and reloaded, SNC's accumulator register walking the expressions). The SDK's start code, whose
# object ships precompiled, is SNC's -O2 instead.
target_compile_options(vs0_app_NPXS10080_eboot PRIVATE -O0)
set_source_files_properties(crt0.c PROPERTIES COMPILE_OPTIONS "-O2")

