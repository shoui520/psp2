enable_language(CXX)

psp2_module(vs0_sys_external_libscemp4 KIND user COMPILER snc SOURCES
	mp4_demux.cpp
	mp4_codec.cpp
	mp4_stream_reader.cpp
	mp4_fragment.cpp
	mp4_sample_table.cpp
	mp4_atom.cpp
	mp4_movie.cpp
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
)
target_link_options(vs0_sys_external_libscemp4 PRIVATE -Wl,--gc-sections)
target_compile_definitions(vs0_sys_external_libscemp4 PRIVATE PSP2_STRIP_UNUSED)
