# ScePhotoExport (vs0:sys/external/libScePhotoExport.suprx), built by Sony with SNC 3.57 in C. No debug
# record names its files; the files are named by what they hold (ours). The compiler's runtime
# (SNC's libsnc) links after the library's file.
enable_language(CXX)
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	photo_export.cpp
	../../../../sdk/target/src/libsnc/i64mod.c
	weak_calls.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
)
# Sony's psp2ld linked with --strip-unused: the runtime routines nothing calls are not in the original.
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
