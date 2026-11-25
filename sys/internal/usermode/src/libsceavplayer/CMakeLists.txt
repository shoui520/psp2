enable_language(CXX)

psp2_module(vs0_sys_external_libsceavplayer KIND user COMPILER snc SOURCES
	avplayer_api.cpp
	avplayer_demux.cpp
	avplayer_controller.cpp
	avplayer_mp4_source.cpp
	avplayer_codec.cpp
	avplayer_fifo.cpp
	avplayer_decoder.cpp
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
)
target_link_options(vs0_sys_external_libsceavplayer PRIVATE -Wl,--gc-sections)
target_compile_definitions(vs0_sys_external_libsceavplayer PRIVATE PSP2_STRIP_UNUSED)
