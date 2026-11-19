# SceLibMtpHttpWrapper (vs0:vsh/common/libmtphttp_wrapper.suprx), built by Sony with SNC 3.57.
# No debug record names its file; the file is named by the module (ours). SNC's weak-call
# forwarder links after it.
psp2_module(${PSP2_TARGET} KIND user COMPILER snc SOURCES
	libmtphttp_wrapper.c
	../../../../../sys/sdk/target/src/libsnc/weak_calls.c
)
target_compile_definitions(${PSP2_TARGET} PRIVATE PSP2_STRIP_UNUSED)
target_link_options(${PSP2_TARGET} PRIVATE -Wl,--gc-sections)
