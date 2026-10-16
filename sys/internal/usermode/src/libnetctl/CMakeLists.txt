# SceNetCtl (vs0:sys/external/libnetctl.suprx), built by Sony with SNC 3.57.
# PARTIAL: the ad-hoc authentication handshake, cryptographic and so
# deferred, has no C yet; its relinked listing, frames.S, was
# deleted on 2026-10-13. Nothing links until it is written.
psp2_module(vs0_sys_external_libnetctl KIND user PARTIAL SOURCES
	source/netctl.c
	source/scan.c
	source/netctl_ipc.c
	source/adhoc_info.c
	source/upnp.c
	source/upnp_http.c
	source/upnp_xml.c
	../../../../../common/runtime/unwind_pr0.c
	../../../../../common/runtime/unwind_pr1.c
)
