# SceGxm: ARM code built at -O3 (prefetching loops, SLP-vectorized stores),
# without strict aliasing.
set(GXM_SOURCES
	background.c
	blend_code_f16.c
	blend_code_f16_assembler.c
	blend_code_f16_optimizer.c
	blend_code_u8.c
	blending.c
	chunk_buffer.c
	context.c
	context_deferred.c
	context_flush.c
	context_init.c
	context_job.c
	context_kick.c
	context_state.c
	device_mem.c
	display_queue.c
	dms.c
	format.c
	fragment_program.c
	globals.c
	heap.c
	init.c
	libc.c
	memory.c
	mte.c
	notification.c
	output_format.c
	pds_sce.c
	precomputed.c
	program.c
	razor.c
	render_target.c
	ring_buffer.c
	shader_patcher.c
	surface.c
	sync_object.c
	texture.c
	transfer.c
	uniforms.c
	usse.c
	usse_builder.c
	vdm.c
	vertex_program.c
	vertex_unpack.c
)
# PARTIAL in Sony's layout: the C still calls sceClibMemcpy, which the original does not import,
# and functions are still missing
psp2_module(os0_us_libgxm_es4 KIND user COMPILER gcc48 PARTIAL SOURCES
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	libgxm_es4.c
	${GXM_SOURCES}
)
set_source_files_properties(${GXM_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O3;-fno-strict-aliasing")
# sceGxmSetUniformDataF converts to half floats with VCVTB.F16.F32, which GCC emits only for
# __fp16 on an FPU with the half-precision extension
set_property(SOURCE uniforms.c APPEND PROPERTY COMPILE_OPTIONS "-mfpu=neon-fp16;-mfp16-format=ieee")

# libgcc's division routines, statically linked first, in ARM code like the library
include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(os0_us_libgxm_es4)
set_source_files_properties(../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S ../../../../../toolchains/libgcc/dvmd_tls.S
	PROPERTIES COMPILE_OPTIONS "-marm")
# The data segment sits where the original's does, leaving room after the text
# for the module's SCE tables.
target_link_options(os0_us_libgxm_es4 PRIVATE -Wl,-Tdata=0x81024000)
