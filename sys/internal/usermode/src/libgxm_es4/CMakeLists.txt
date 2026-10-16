# SceGxm: ARM code built at -O3 (prefetching loops, SLP-vectorized stores),
# without strict aliasing.
set(GXM_SOURCES
	source/blend_code_f16.c
	source/blend_code_f16_assembler.c
	source/blend_code_f16_optimizer.c
	source/blend_code_u8.c
	source/blending.c
	source/chunk_buffer.c
	source/context_state.c
	source/device_mem.c
	source/display_queue.c
	source/dms.c
	source/format.c
	source/fragment_program.c
	source/globals.c
	source/heap.c
	source/init.c
	source/libc.c
	source/memory.c
	source/mte.c
	source/notification.c
	source/output_format.c
	source/pds_sce.c
	source/program.c
	source/razor.c
	source/shader_patcher.c
	source/surface.c
	source/sync_object.c
	source/texture.c
	source/transfer.c
	source/usse.c
	source/usse_builder.c
	source/vdm.c
	source/vertex_unpack.c
)
psp2_module(os0_us_libgxm_es4 KIND user SOURCES
	source/libgxm_es4.c
	${GXM_SOURCES}
)
set_source_files_properties(${GXM_SOURCES} PROPERTIES COMPILE_OPTIONS "-marm;-O3;-fno-strict-aliasing")
# The data segment sits where the original's does, leaving room after the text
# for the module's SCE tables.
target_link_options(os0_us_libgxm_es4 PRIVATE -Wl,-Tdata=0x81024000)
