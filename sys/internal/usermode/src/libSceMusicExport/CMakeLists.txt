enable_language(CXX)
# SceMusicExport (vs0:sys/external/libSceMusicExport.suprx), built by Sony with SNC 3.57 in C++. No debug
# record names its files; the files are named by what they hold (ours). The media service's methods
# link after the library's file, then the compiler's runtime (SNC's libsnc).
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	music_export.cpp
	music_export_ipc_client.cpp
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/i64mod.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
)
# SNC writes inline functions out of line as well; Sony's linker kept only what is called
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# Sony's psp2ld --strip-unused dropped what nothing references
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
