# SceLibPspnetAdhoc (vs0:sys/external/pspnet_adhoc.suprx), built by Sony with SNC 3.57.
psp2_module(vs0_sys_external_pspnet_adhoc KIND user COMPILER snc SOURCES
	adhocctl.c
	adhoc_auth.c
	adhoc_auth_sys.c
	adhoc_auth_peer.c
	adhoc.c
	adhoc_pdp.c
	adhoc_ptp.c
	callout.c
	mbuf.c
	slpque.c
	spl.c
	r521.c
	thread.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
