# SceNetCtl (vs0:sys/external/libnetctl.suprx), built by Sony with SNC 3.57.
# PARTIAL: the ad-hoc authentication handshake, cryptographic and so
# deferred, has no C yet; its relinked listing, frames.S, was
# deleted on 2026-10-13. Nothing links until it is written.
psp2_module(vs0_sys_external_libnetctl KIND user PARTIAL SOURCES
	netctl.c
	scan.c
	netctl_ipc.c
	adhoc_info.c
	upnp.c
	upnp_http.c
	upnp_xml.c
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
)
