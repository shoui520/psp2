# MarlinAppLib (vs0:sys/external/libmlnapplib.suprx), built by Sony with SNC 3.57. No debug record
# names its files; the file is named by the module (ours). The compiler's runtime (SNC's libsnc)
# links after the library's code.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	libmlnapplib.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/weak_calls.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
)
set_source_files_properties(../../../../sdk/target/src/libsnc/weak_calls.c
	PROPERTIES COMPILE_DEFINITIONS SNC_WEAK_CALLS_ON)
# Sony's psp2ld linked with --strip-unused: the runtime routines nothing calls are not in the original.
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
