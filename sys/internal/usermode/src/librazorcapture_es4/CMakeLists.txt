# SceRazorCapture (vs0:sys/external/librazorcapture_es4.suprx), built by Sony with GCC 4.8 (-marm).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	librazorcapture_es4.c
	capture.c
	analyse.c
	capture_file.c
	capture_hooks.S
	host_comm.c
	sgx_decode.c
	salvage.c
	usse_decode.c
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
target_compile_options(${PSP2_TARGET} PRIVATE -marm)
