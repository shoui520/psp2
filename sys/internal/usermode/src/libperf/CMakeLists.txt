# ScePerf (vs0:sys/external/libperf.suprx), built by Sony with GCC 4.8.
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../../toolchains/libgcc/uldivmod.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	../../../../../toolchains/libgcc/udiv.S
	razor_debug_marker.S
	razor_cpu.c
	arm_pmon.c
	libperf.c
	razor_hooks.S
	activity_monitor.c
	perf_internal.c
	../../../../../toolchains/libgcc/uldivmod64.c
	../../../../../toolchains/libgcc/udiv64.c
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
set_source_files_properties(
	../../../../../toolchains/libgcc/uldivmod64.c
	../../../../../toolchains/libgcc/udiv64.c
	PROPERTIES COMPILE_OPTIONS "-fno-stack-protector;-fbuilding-libgcc"
	COMPILE_DEFINITIONS "IN_LIBGCC2")
target_compile_options(${PSP2_TARGET} PRIVATE -O3)
