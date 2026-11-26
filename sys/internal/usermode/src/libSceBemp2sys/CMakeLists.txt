enable_language(CXX)
# SceBemp2sys (vs0:sys/external/libSceBemp2sys.suprx), built by Sony with SNC 3.57 in C++.
psp2_module(vs0_sys_external_libSceBemp2sys KIND user COMPILER snc SOURCES
	be_clib.cpp
	bit_reader.cpp
	be_prx_mutex.cpp
	be_prx_mutex_light.cpp
	mem_block_pool.cpp
	be_mem.cpp
	cat_packetizer.cpp
	pat_packetizer.cpp
	pes_parser.cpp
	pes_packetizer.cpp
	pmt_packetizer.cpp
	si_packetizer.cpp
	ts_packetizer.cpp
	packetizer_factory.cpp
	section_storage.cpp
	ts_demuxer.cpp
	stream_info.cpp
	ts_parser.cpp
	section_parser.cpp
	pes_payload_parser.cpp
	be_mp2sys_api.cpp
	be_log.cpp
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
)
target_compile_definitions(vs0_sys_external_libSceBemp2sys PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libSceBemp2sys PRIVATE -Wl,--gc-sections)
