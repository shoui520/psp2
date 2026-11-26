enable_language(CXX)
# SceLibRudp (vs0:sys/external/librudp.suprx), built by Sony with SNC 3.57 in C++,
# statically linking libSceSha256 (GCC 4.8) and libsnc runtime helpers.
psp2_module(vs0_sys_external_librudp KIND user COMPILER snc SOURCES
	rudp_api.cpp
	rudp_context.cpp
	rudp_scheduler.cpp
	rudpHeader.cpp
	rudp_core.cpp
	rudpNetworker.cpp
	rudp_poll.cpp
	rudp_rbtree.cpp
	rudp_protocol.cpp
	rudp_util.cpp
	rudp_buffer.cpp
	rudp_thread.cpp
	rudpAggregator.cpp
	rudp_mempool.cpp
	rudp_osal.cpp
	../../../../sdk/target/src/libsnc/pure_virt.c
	../../../../sdk/target/src/libsnc/error.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/sha256/libsha256.c
	../../../../sdk/target/src/sha256/sha256.S
)
target_compile_definitions(vs0_sys_external_librudp PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_librudp PRIVATE -Wl,--gc-sections)
set_source_files_properties(
	rudp_rbtree.cpp
	PROPERTIES COMPILE_OPTIONS "-O1"
)
set_source_files_properties(
	rudp_osal.cpp
	PROPERTIES COMPILE_OPTIONS "-Xautoinlinesize=0"
)
set_source_files_properties(
	../../../../sdk/target/src/libsnc/error.c
	PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-Tp;-Xblxcall=1"
)
set_source_files_properties(
	../../../../sdk/target/src/sha256/libsha256.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector"
)
set_source_files_properties(
	../../../../sdk/target/src/sha256/sha256.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
)
