# SceClipboard (vs0:sys/external/libclipboard.suprx), built by Sony with SNC 3.57, with libSceCes's
# UCS.c (the SDK's static library)
psp2_module(vs0_sys_external_libclipboard KIND user COMPILER snc SOURCES
	libclipboard.c
	../ces/libces/src/UCS/UCS.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
)
