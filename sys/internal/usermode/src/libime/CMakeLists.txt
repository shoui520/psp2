# SceIme (vs0:sys/external/libime.suprx), built by Sony with SNC 3.57. No debug record names its files;
# the file is named by the module (ours). SNC's forwarding stub (libsnc) links after the code.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	libime.c
	../../../../sdk/target/src/libsnc/weak_calls.c
)
# Sony's psp2ld linked with --strip-unused
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
