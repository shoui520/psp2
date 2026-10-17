# SceAtrac (vs0:sys/external/libatrac.suprx): ATRAC9 streaming on SceAudiodec,
# built by Sony with SNC 3.57 from several translation units (the layout of
# the original follows them).  i32ops.c and unwind_pr*.c are the parts of the
# SNC runtime (libsnc) the original links in.
psp2_module(vs0_sys_external_libatrac KIND user SOURCES
	atrac_util.c
	atrac_decoder.c
	atrac_stream.c
	atrac.c
	atrac_outputable.c
	atrac_header.c
	../../../../../sys/sdk/target/src/libsnc/i32ops.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr2.c
)
