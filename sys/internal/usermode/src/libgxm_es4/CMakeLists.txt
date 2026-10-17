# SceGxm: ARM code built at -O3 (prefetching loops, SLP-vectorized stores),
# without strict aliasing.
set(GXM_SOURCES
	blend_code_f16.c
	blend_code_f16_assembler.c
	blend_code_f16_optimizer.c
	blend_code_u8.c
	blending.c
	chunk_buffer.c
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
	program.c
	razor.c
	shader_patcher.c
	surface.c
	sync_object.c
	texture.c
	transfer.c
	usse.c
	usse_builder.c
	vdm.c
	vertex_unpack.c
)
# PARTIAL in Sony's layout: the C still calls sceClibMemcpy, which the original does not import,
# and __aeabi_idivmod / __aeabi_uidiv, which the original has from its statically linked libgcc;
# sceGxmFinish (exported) is not written yet
psp2_module(os0_us_libgxm_es4 KIND user COMPILER gcc48 PARTIAL SOURCES
	libgxm_es4.c
	${GXM_SOURCES}
)
set_source_files_properties(${GXM_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O3;-fno-strict-aliasing")
# The data segment sits where the original's does, leaving room after the text
# for the module's SCE tables.
target_link_options(os0_us_libgxm_es4 PRIVATE -Wl,-Tdata=0x81024000)
