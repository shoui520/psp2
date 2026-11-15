# SceNpUtility (vs0:sys/external/np_utility.suprx), built by Sony with SNC 3.57. No debug
# record names its files: they are split where its parts are (ours).
psp2_module(vs0_sys_external_np_utility KIND user COMPILER snc SOURCES
	word_filter.c
	lookup.c
	client.c
	request.c
	query.c
	xml.c
	memory.c
	title.c
	trans.c
	util.c
	worker.c
	bandwidth_test.c
	../../../../../../sys/sdk/target/src/libsnc/i64ops.c
	../../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
)

# the libsnc runtime objects the module does not call are not linked
target_compile_definitions(vs0_sys_external_np_utility PRIVATE PSP2_STRIP_UNUSED)
target_compile_definitions(vs0_sys_external_np_utility PRIVATE LIBSNC_OMIT_SIGNED_DIVISION)
