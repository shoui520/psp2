# SceLibPvf (vs0:sys/external/libpvf.suprx), built by Sony with GCC: the SDK's SHA-1 library
# is linked in with it (the font cache key).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 PARTIAL SOURCES
	../../../../sdk/target/src/sha1/libsha1.c
	../../../kernel/src/sysmem/sha1_transform.S
	libpvf.c
)
# The SDK's libSceSha1, which Sony built with GCC and without stack guards
set_source_files_properties(../../../../sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(../../../kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
