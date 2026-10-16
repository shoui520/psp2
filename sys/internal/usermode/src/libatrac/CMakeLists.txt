# SceAtrac (vs0:sys/external/libatrac.suprx): ATRAC9 streaming on SceAudiodec,
# built by Sony with SNC 3.57 from several translation units (the layout of
# the original follows them).  i32ops.c and unwind_pr*.c are the parts of the
# SNC runtime (libsnc) the original links in.
psp2_module(vs0_sys_external_libatrac KIND user SOURCES
	source/atrac_util.c
	source/atrac_decoder.c
	source/atrac_stream.c
	source/atrac.c
	source/atrac_outputable.c
	source/atrac_header.c
	../../../../../common/runtime/i32ops.c
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
	../../../../../common/runtime/unwind_pr2.c
)
