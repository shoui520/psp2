# swagner (vs0:app/NPXS10080/eboot.bin), built by Sony with GCC 4.8 in C and C++. No
# debug record names its files; the build-path strings name Sony's dtcpip repository
# (DTCPIP-MW, DTCPIP-SW, SWKeyManager) and the files are placed there. The SDK's start
# code (the 3.65 variant) links first.
psp2_module(vs0_app_NPXS10080_eboot KIND user COMPILER gcc48 PARTIAL SOURCES
	crt0.c
	main.c
	eboot.c
)
