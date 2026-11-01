# SceNetCtl (vs0:sys/external/libnetctl.suprx), built by Sony with SNC 3.57.
psp2_module(vs0_sys_external_libnetctl KIND user COMPILER snc SOURCES
	netctl.c
	adhoc_auth.c
	adhoc_auth_sys.c
	adhoc_auth_peer.c
	scan.c
	netctl_ipc.c
	adhoc_info.c
	upnp.c
	upnp_desc.c
	upnp_http.c
	upnp_xml.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr0.c
	../../../../../sys/sdk/target/src/libsnc/unwind_pr1.c
)
