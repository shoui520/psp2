# SceJpegEncArm (vs0:sys/external/libscejpegencarm.suprx): the JPEG encoder on the ARM cores,
# built by Sony with GCC 4.8; the libgcc signed division and its divide-by-zero handler come first.
set(MODULE_COMPILE_OPTIONS -fno-optimize-sibling-calls)
psp2_module(vs0_sys_external_libscejpegencarm KIND user COMPILER gcc48 SOURCES
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	libscejpegencarm.c
)

target_include_directories(vs0_sys_external_libscejpegencarm PRIVATE
	../../../../../toolchains/libgcc
	../../../../../toolchains/libgcc/target
	../../../../../contrib/gcc-4.8-2014.05/gcc
	../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	../../../../../contrib/gcc-4.8-2014.05/libgcc
	../../../../../contrib/gcc-4.8-2014.05/include)
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(vs0_sys_external_libscejpegencarm)
