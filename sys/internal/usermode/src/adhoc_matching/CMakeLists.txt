# SceNetAdhocMatching (vs0:sys/external/adhoc_matching.suprx), built by Sony with SNC 3.57.
psp2_module(vs0_sys_external_adhoc_matching KIND user COMPILER snc SOURCES
	adhoc_matching.c
	matching_event.c
	matching_context.c
	matching_send.c
	matching_recv.c
	callout.c
	../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c
)
set_source_files_properties(../../../../../sys/sdk/target/src/libsnc/i32ops.c PROPERTIES
	COMPILE_DEFINITIONS "LIBSNC_OMIT_SIGNED_DIVISION"
)
set_source_files_properties(../../../../../sys/sdk/target/src/libsnc/aeabi_div0.c PROPERTIES
	COMPILE_DEFINITIONS "LIBSNC_OMIT_LDIV0"
)
