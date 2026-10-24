# generated skeleton (T2): stub bodies, original module/export/import tables
# Originals have no SSP imports or guards, including if_clone_list's 16-byte
# stack buffer and Sleep/StartStack's address-taken locals in both TOOL builds.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
# NetBSD's network stack, built as kernel code on NetBSD's own headers (include/: the psp2 port's
# machine/ and Sony's changes, the rest from contrib)
add_compile_definitions(_KERNEL)
# NetBSD's own files, built from the pristine tree in contrib (or with Sony's patch). Sony built
# them as NetBSD's kernel builds are: without strict aliasing (callout_hardclock reloads
# hardclock_ticks after every store to a bucket) and with gnu89 inline semantics (uvm_extern.h's
# extern inline functions are never emitted), and with NET_PS_NETBSD for what Sony changed in
# them only (libkern.h's memcpy)
set(netbsd ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/netbsd-2.1/sys)
# the files where Sony followed NetBSD 3.0 (PPPoE and the PPP it runs)
set(netbsd3 ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/netbsd-3.0/sys)
include(${CMAKE_CURRENT_SOURCE_DIR}/vendor_patch.cmake)
net_ps_prepare_patched(${netbsd}/kern/kern_subr.c ${CMAKE_CURRENT_SOURCE_DIR}/kern_subr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/kern_subr.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_domain.c ${CMAKE_CURRENT_SOURCE_DIR}/uipc_domain_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_domain.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_mbuf.c ${CMAKE_CURRENT_SOURCE_DIR}/uipc_mbuf_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_mbuf.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_socket.c ${CMAKE_CURRENT_SOURCE_DIR}/uipc_socket_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_socket2.c ${CMAKE_CURRENT_SOURCE_DIR}/uipc_socket2_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket2.c)
net_ps_prepare_patched(${netbsd}/net/if.c ${CMAKE_CURRENT_SOURCE_DIR}/if_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if.c)
net_ps_prepare_patched(${netbsd}/net/if_ethersubr.c ${CMAKE_CURRENT_SOURCE_DIR}/if_ethersubr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_ethersubr.c)
net_ps_prepare_patched(${netbsd}/net/if_loop.c ${CMAKE_CURRENT_SOURCE_DIR}/if_loop_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_loop.c)
net_ps_prepare_patched(${netbsd3}/net/if_pppoe.c ${CMAKE_CURRENT_SOURCE_DIR}/if_pppoe_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_pppoe.c)
net_ps_prepare_patched(${netbsd3}/net/if_spppsubr.c ${CMAKE_CURRENT_SOURCE_DIR}/if_spppsubr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_spppsubr.c)
net_ps_prepare_patched(${netbsd}/net/radix.c ${CMAKE_CURRENT_SOURCE_DIR}/radix_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/radix.c)
net_ps_prepare_patched(${netbsd}/net/raw_usrreq.c ${CMAKE_CURRENT_SOURCE_DIR}/raw_usrreq_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/raw_usrreq.c)
net_ps_prepare_patched(${netbsd}/net/route.c ${CMAKE_CURRENT_SOURCE_DIR}/route_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/route.c)
net_ps_prepare_patched(${netbsd}/net/rtsock.c ${CMAKE_CURRENT_SOURCE_DIR}/rtsock_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/rtsock.c)
net_ps_prepare_patched(${netbsd}/netinet/if_arp.c ${CMAKE_CURRENT_SOURCE_DIR}/if_arp_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/if_arp.c)
net_ps_prepare_patched(${netbsd}/netinet/in4_cksum.c ${CMAKE_CURRENT_SOURCE_DIR}/in4_cksum_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in4_cksum.c)
net_ps_prepare_patched(${netbsd}/netinet/in.c ${CMAKE_CURRENT_SOURCE_DIR}/in_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in.c)
net_ps_prepare_patched(${netbsd}/netinet/in_cksum.c ${CMAKE_CURRENT_SOURCE_DIR}/in_cksum_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_cksum.c)
net_ps_prepare_patched(${netbsd}/netinet/in_pcb.c ${CMAKE_CURRENT_SOURCE_DIR}/in_pcb_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_pcb.c)
set(netbsd_sources
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/kern_subr.c
	${netbsd}/kern/kern_timeout.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_domain.c
	${netbsd}/kern/uipc_mbuf2.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_mbuf.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket2.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_ethersubr.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_loop.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_pppoe.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_spppsubr.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/radix.c
	${netbsd}/net/raw_cb.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/raw_usrreq.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/route.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/rtsock.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/if_arp.c
	${netbsd}/netinet/igmp.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in4_cksum.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_cksum.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_pcb.c
)
set_source_files_properties(${netbsd_sources} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-fgnu89-inline"
	COMPILE_DEFINITIONS NET_PS_NETBSD)
# Sony's kernel options: a PADT answers data for a session nobody owns
set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_pppoe.c APPEND PROPERTY COMPILE_DEFINITIONS
	PPPOE_TERM_UNKNOWN_SESSIONS)
psp2_module(os0_kd_bootimage_net_ps KIND kernel COMPILER gcc48 PARTIAL SOURCES
	netps_allocator_adapters.c
	netps_thread.c
	netps_process.c
	netps_resource.c
	netps_driver.c
	netps_socket_adapters.c
	netps_packet_adapters.c
	netps_process_callbacks.c
	netps_packet_cleanup.c
	netps_driver_updates.c
	netps_dump_reader.c
	net_ps_glue.c
	net_ps_statistics.c
	net_ps_startup.c
	net_ps_packet.c
	net_ps_emulation.c
	net_ps_socket.c
	${netbsd_sources}
	strlcpy.c
	net_ps_hmac.c
	net_ps_aes.c
	src/ps/osdependence/ngp/system_os.c
	net_ps.c
)
