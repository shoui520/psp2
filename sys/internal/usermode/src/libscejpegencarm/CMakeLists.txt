# SceJpegEncArm (vs0:sys/external/libscejpegencarm.suprx): the JPEG encoder on the ARM cores,
# built by Sony with GCC 4.8; the libgcc signed division it links comes first.
set(MODULE_COMPILE_OPTIONS -fno-optimize-sibling-calls)
psp2_module(vs0_sys_external_libscejpegencarm KIND user COMPILER gcc48 PARTIAL SOURCES
	../../../../../toolchains/libgcc/idiv.S
	libscejpegencarm.c
)
