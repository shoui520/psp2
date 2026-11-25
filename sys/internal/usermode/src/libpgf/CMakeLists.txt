# SceLibPgf (vs0:sys/external/libpgf.suprx), built by Sony with GCC: the SDK's SHA-1 library
# is linked in with it (the font cache key).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../sdk/target/src/sha1/libsha1.c
	../../../kernel/src/sysmem/sha1_transform.S
	libpgf.c
)
# The SDK's libSceSha1, built with GCC and without stack guards
set_source_files_properties(../../../../sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(../../../kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
# libpgf passes floats in VFP registers (the hard-float calling convention)
set_source_files_properties(libpgf.c
	PROPERTIES COMPILE_OPTIONS "-mfloat-abi=hard")
# GNU ld warns when linking soft-float libsha1 with hard-float libpgf unless suppressed
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--no-warn-mismatch)
