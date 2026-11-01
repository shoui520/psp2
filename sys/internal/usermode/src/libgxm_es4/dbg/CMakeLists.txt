# vs0:sys/external/libgxm_dbg_es4 - SceGxmDbg, the debug build of SceGxm (..). Sony built
# both from one source tree: the same exports, with the SCE_DBG_ASSERT checks and the
# [SceGxmDbg] error messages compiled in (gxm_internal.h), and the files only the debug build
# has (context_debug.c, features.c, ...).
set(GXM_DBG_SOURCES
	../background_object.c
	../blend_code_f16.c
	../blend_code_f16_assembler.c
	../blend_code_f16_optimizer.c
	../blend_code_u8.c
	../blending.c
	../chunk_buffer.c
	../context.c
	../context_debug.c
	../context_deferred.c
	../context_flush.c
	../context_init.c
	../context_job.c
	../context_kick.c
	../context_state.c
	../device_mem.c
	../display_queue.c
	../dms.c
	../features.c
	../fragment_program.c
	../globals.c
	../heap.c
	../init.c
	../memexport.c
	../libc.c
	../memory.c
	../mte.c
	../slot_table.c
	../notification.c
	../output_format.c
	../pds_sce.c
	../pixel_event.c
	../precomputation.c
	../program.c
	../razor.c
	../render_target.c
	../ring_buffer.c
	../shader_patcher.c
	../surface.c
	../sync_object.c
	../texture.c
	../transfer.c
	../uniforms.c
	../usse.c
	../usse_builder.c
	../util.c
	../vdm.c
	../vertex_program.c
	../vertex_unpack.c
)
psp2_module(vs0_sys_external_libgxm_dbg_es4 KIND user COMPILER gcc48 SOURCES
	../../../../../../toolchains/libgcc/udiv.S
	../../../../../../toolchains/libgcc/idiv.S
	../../../../../../toolchains/libgcc/dvmd_tls.S
	libgxm_dbg_es4.c
	${GXM_DBG_SOURCES}
)
set_source_files_properties(${GXM_DBG_SOURCES} PROPERTIES
	COMPILE_OPTIONS "-marm;-O3;-fno-strict-aliasing"
	COMPILE_DEFINITIONS "SCE_DBG_ASSERTS_ENABLED=1")
set_property(SOURCE ../uniforms.c APPEND PROPERTY COMPILE_OPTIONS "-mfpu=neon-fp16;-mfp16-format=ieee")

# libgcc's division routines, statically linked first, in ARM code like the library
include(../../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(vs0_sys_external_libgxm_dbg_es4)
set_source_files_properties(../../../../../../toolchains/libgcc/udiv.S
	../../../../../../toolchains/libgcc/idiv.S ../../../../../../toolchains/libgcc/dvmd_tls.S
	PROPERTIES COMPILE_OPTIONS "-marm")
target_link_options(vs0_sys_external_libgxm_dbg_es4 PRIVATE -Wl,-Tdata=0x81059000)
