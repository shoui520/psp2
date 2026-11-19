enable_language(CXX)
# SceMailApiForLocalLibc (vs0:sys/external/mail_api_for_local_libc.suprx), built by Sony with
# SNC 3.57 in C++ (Dinkumware's string): the local mail API's message writer. No debug record
# names its file; the file is named by the module (ours). SNC's runtime follows it.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	mail_api_for_local_libc.cpp
	../../../../sdk/target/src/libsnc/pure_virt.c
	../../../../sdk/target/src/libsnc/weak_calls.c
	../../../../sdk/target/src/libsnc/error.c
)
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
# the C++ library is compiled with the SDK's headers, which its templates need whole
target_include_directories(${PSP2_TARGET} BEFORE PRIVATE $ENV{PSP2_SDK}/target/include)
set_source_files_properties(
	../../../../sdk/target/src/libsnc/error.c PROPERTIES
	LANGUAGE CXX COMPILE_OPTIONS "-Tp;-Xblxcall=1")
