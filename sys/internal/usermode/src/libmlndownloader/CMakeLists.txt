psp2_module(vs0_sys_external_libmlndownloader KIND user COMPILER snc SOURCES
	mlndl_api.c
	mlndl_engine.c
	mlndl_http.c
	mlndl_protocol.c
	mlndl_mem.c
	mlndl_np.c
	../../../../sdk/target/src/libsnc/weak_calls.c
)
set_source_files_properties(../../../../sdk/target/src/libsnc/weak_calls.c PROPERTIES
	COMPILE_DEFINITIONS SNC_WEAK_CALLS_ON
)
