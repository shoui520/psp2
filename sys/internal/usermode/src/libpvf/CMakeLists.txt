# SceLibPvf (vs0:sys/external/libpvf.suprx), built by Sony with GCC: the SDK's SHA-1 library
# is linked in with it (the font cache key).
psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../sdk/target/src/sha1/libsha1.c
	../../../kernel/src/sysmem/sha1_transform.S
	libpvf.c
	pvf_gsub.c
	pvf_rasterizer.c
)
# The SDK's libSceSha1, which Sony built with GCC and without stack guards
set_source_files_properties(../../../../sdk/target/src/sha1/libsha1.c
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48
	COMPILE_OPTIONS "-fno-stack-protector;-ffunction-sections")
set_source_files_properties(../../../kernel/src/sysmem/sha1_transform.S
	PROPERTIES COMPILE_DEFINITIONS PSP2_COMPILER_GCC48)
# libpvf passes floats in VFP registers (the hard-float calling convention)
set_source_files_properties(libpvf.c pvf_gsub.c pvf_rasterizer.c
	PROPERTIES COMPILE_OPTIONS "-mfloat-abi=hard")
# libSceFt2's FreeType, whose headers libpvf's rasterizer is written on
target_include_directories(${PSP2_TARGET} PRIVATE ../../../../../contrib/freetype-7f316f22d/include)
# psp2ld does not compare the objects' float ABI attributes (libsha1 is soft-float), GNU ld
# does unless told not to
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--no-warn-mismatch)
