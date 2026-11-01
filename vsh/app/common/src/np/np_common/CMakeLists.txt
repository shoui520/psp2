# SceNpCommon (vs0:sys/external/np_common.suprx), built by Sony with SNC 3.57.
psp2_module(vs0_sys_external_np_common KIND user COMPILER snc PARTIAL SOURCES
	ticket.c
	base64.c
	callout.c
	util.c
	jid.c
	strtol.c
	np_common.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
