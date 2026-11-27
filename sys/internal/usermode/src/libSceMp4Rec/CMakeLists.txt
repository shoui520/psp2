enable_language(CXX)
# SceLibMp4Recorder (vs0:sys/external/libSceMp4Rec.suprx): MP4 video/audio
# recorder, built with SNC 3.57, linking FF4 muxer, libces (UCS), libScePng,
# libSceDeflt (GCC 4.8 -Os), and libsnc.
psp2_module(vs0_sys_external_libSceMp4Rec KIND user COMPILER snc SOURCES
	mp4rec_api.cpp
	mp4rec_mutex.cpp
	mp4rec_allocator_if.cpp
	mp4rec_mem_alloc.cpp
	mp4rec_ring_queue.cpp
	mp4rec_buf_pool.cpp
	mp4rec_sample_buf.cpp
	mp4rec_thr_write.cpp
	mp4rec_aac_enc.cpp
	mp4rec_avc_enc.cpp
	mp4rec_avrec.cpp
	mp4rec_recorder.cpp
	mp4rec_muxer.cpp
	mp4rec_png.c
	ff4/ff4_session.c
	ff4/ff4_writer.c
	ff4/ff4_box.c
	ff4/ff4_moov.c
	ff4/ff4_trak.c
	ff4/ff4_mdia.c
	ff4/ff4_minf.c
	ff4/ff4_stbl.c
	ff4/ff4_stsd.c
	ff4/ff4_moof.c
	ff4/ff4_mfra.c
	ff4/ff4_uuid.c
	ff4/ff4_util.c
	../ces/libces/src/ces_context.c
	../ces/libces/src/UCS/ces_ucs_context.c
	../ces/libces/src/UCS/ces_ucs_callback.c
	../ces/libces/src/UCS/Str_UTF8_UTF16.c
	../ces/libces/src/UCS/UTF16_UTF8.c
	../ces/libces/src/UCS/UTF8.c
	../ces/libces/src/UCS/UTF16.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/error.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
	../../../../sdk/target/src/libsnc/pure_virt.c
	../../../../sdk/target/src/deflt/crc.S
	../../../../sdk/target/src/deflt/vadler.S
	../../../../sdk/target/src/deflt/zlibhdr.c
	../../../../sdk/target/src/deflt/deflate.S
	../../../../sdk/target/src/deflt/deflate2.S
	../../../../sdk/target/src/deflt/deflate_common.c
)

if(TARGET vs0_sys_external_libSceMp4Rec)
	target_include_directories(vs0_sys_external_libSceMp4Rec PRIVATE
		"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt"
		"${CMAKE_SOURCE_DIR}/sys/internal/usermode/src/ces/libces/src"
		"${CMAKE_SOURCE_DIR}/sys/internal/usermode/src/ces/libces/src/UCS")
endif()

set_source_files_properties(
	../../../../sdk/target/src/libsnc/error.c
	PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-Tp;-Xblxcall=1")

set_source_files_properties(
	../../../../sdk/target/src/libsnc/i64ops.c
	PROPERTIES COMPILE_DEFINITIONS LIBSNC_OMIT_SIGNED_DIVISION)

set_source_files_properties(
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
	PROPERTIES COMPILE_OPTIONS -Xblxcall=1)

set(mp4rec_deflt_options -Os -fno-stack-protector -foptimize-sibling-calls
	-fdelete-null-pointer-checks -fsection-anchors -fshrink-wrap -funwind-tables
	-ffunction-sections -fdata-sections)
set_source_files_properties(
	../../../../sdk/target/src/deflt/zlibhdr.c
	../../../../sdk/target/src/deflt/deflate_common.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "${mp4rec_deflt_options}")
set_source_files_properties(
	../../../../sdk/target/src/deflt/deflate.S
	../../../../sdk/target/src/deflt/deflate2.S
	PROPERTIES COMPILE_DEFINITIONS DEFLATE_MEMCPY=sceClibMemcpy_safe)
