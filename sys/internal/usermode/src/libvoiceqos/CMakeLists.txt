# SceVoiceQoS (vs0:sys/external/libvoiceqos.suprx), built by Sony with GCC; libgcc's unsigned
# division and 64-bit to float conversion are linked in with it.
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	../../../../../toolchains/libgcc/addsubsf3.S
	libvoiceqos.c
	libc.c
)
target_include_directories(${PSP2_TARGET} PRIVATE
	../../../../../toolchains/libgcc
	../../../../../toolchains/libgcc/target
	../../../../../contrib/gcc-4.8-2014.05/gcc
	../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	../../../../../contrib/gcc-4.8-2014.05/libgcc
	../../../../../contrib/gcc-4.8-2014.05/include)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})
