# SceNetPs's sources and build settings, shared by its two products: the bootimage's net_ps
# (this folder) and the development kits' net_ps_dev (dev/), one tree built twice
# Originals have no SSP imports or guards, including if_clone_list's 16-byte
# stack buffer and Sleep/StartStack's address-taken locals in both TOOL builds.
# Each function in a section of its own: the link keeps only what the exports and
# module_start reach (net_ps_link), as Sony's image has none of NetBSD's functions
# nothing calls (m_devget, sbappend, the m_tag functions, ...) and no out-of-line
# m_split, which its one caller inlines.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector -ffunction-sections)
# NetBSD's network stack, built as kernel code on NetBSD's own headers (include/: the psp2 port's
# machine/ and Sony's changes, the rest from contrib)
add_compile_definitions(_KERNEL)
# NetBSD's own files, built from the pristine tree in contrib (or with Sony's patch). Sony built
# them as NetBSD's kernel builds are: without strict aliasing (callout_hardclock reloads
# hardclock_ticks after every store to a bucket) and with gnu89 inline semantics (uvm_extern.h's
# extern inline functions are never emitted), and with NET_PS_NETBSD for what Sony changed in
# them only (libkern.h's memcpy)
set(netbsd ${CMAKE_CURRENT_LIST_DIR}/../../../../../contrib/netbsd-2.1/sys)
# the files where Sony followed NetBSD 3.0 (PPPoE and the PPP it runs, TCP)
set(netbsd3 ${CMAKE_CURRENT_LIST_DIR}/../../../../../contrib/netbsd-3.0/sys)
include(${CMAKE_CURRENT_LIST_DIR}/vendor_patch.cmake)
net_ps_prepare_patched(${netbsd}/kern/kern_subr.c ${CMAKE_CURRENT_LIST_DIR}/kern_subr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/kern_subr.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_domain.c ${CMAKE_CURRENT_LIST_DIR}/uipc_domain_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_domain.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_mbuf.c ${CMAKE_CURRENT_LIST_DIR}/uipc_mbuf_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_mbuf.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_socket.c ${CMAKE_CURRENT_LIST_DIR}/uipc_socket_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket.c)
net_ps_prepare_patched(${netbsd}/kern/uipc_socket2.c ${CMAKE_CURRENT_LIST_DIR}/uipc_socket2_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket2.c)
net_ps_prepare_patched(${netbsd}/net/if.c ${CMAKE_CURRENT_LIST_DIR}/if_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if.c)
net_ps_prepare_patched(${netbsd}/net/if_ethersubr.c ${CMAKE_CURRENT_LIST_DIR}/if_ethersubr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_ethersubr.c)
net_ps_prepare_patched(${netbsd}/net/if_loop.c ${CMAKE_CURRENT_LIST_DIR}/if_loop_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_loop.c)
net_ps_prepare_patched(${netbsd3}/net/if_pppoe.c ${CMAKE_CURRENT_LIST_DIR}/if_pppoe_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_pppoe.c)
net_ps_prepare_patched(${netbsd3}/net/if_spppsubr.c ${CMAKE_CURRENT_LIST_DIR}/if_spppsubr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_spppsubr.c)
net_ps_prepare_patched(${netbsd}/net/radix.c ${CMAKE_CURRENT_LIST_DIR}/radix_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/radix.c)
net_ps_prepare_patched(${netbsd}/net/raw_usrreq.c ${CMAKE_CURRENT_LIST_DIR}/raw_usrreq_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/raw_usrreq.c)
net_ps_prepare_patched(${netbsd}/net/route.c ${CMAKE_CURRENT_LIST_DIR}/route_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/route.c)
net_ps_prepare_patched(${netbsd}/net/rtsock.c ${CMAKE_CURRENT_LIST_DIR}/rtsock_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/rtsock.c)
net_ps_prepare_patched(${netbsd}/netinet/if_arp.c ${CMAKE_CURRENT_LIST_DIR}/if_arp_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/if_arp.c)
net_ps_prepare_patched(${netbsd}/netinet/in4_cksum.c ${CMAKE_CURRENT_LIST_DIR}/in4_cksum_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in4_cksum.c)
net_ps_prepare_patched(${netbsd}/netinet/in.c ${CMAKE_CURRENT_LIST_DIR}/in_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in.c)
net_ps_prepare_patched(${netbsd}/netinet/in_cksum.c ${CMAKE_CURRENT_LIST_DIR}/in_cksum_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_cksum.c)
net_ps_prepare_patched(${netbsd}/netinet/in_pcb.c ${CMAKE_CURRENT_LIST_DIR}/in_pcb_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_pcb.c)
net_ps_prepare_patched(${netbsd}/netinet/in_proto.c ${CMAKE_CURRENT_LIST_DIR}/in_proto_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_proto.c)
net_ps_prepare_patched(${netbsd}/netinet/ip_icmp.c ${CMAKE_CURRENT_LIST_DIR}/ip_icmp_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_icmp.c)
net_ps_prepare_patched(${netbsd}/netinet/ip_id.c ${CMAKE_CURRENT_LIST_DIR}/ip_id_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_id.c)
net_ps_prepare_patched(${netbsd}/netinet/ip_input.c ${CMAKE_CURRENT_LIST_DIR}/ip_input_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_input.c)
net_ps_prepare_patched(${netbsd}/netinet/ip_output.c ${CMAKE_CURRENT_LIST_DIR}/ip_output_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_output.c)
net_ps_prepare_patched(${netbsd}/netinet/raw_ip.c ${CMAKE_CURRENT_LIST_DIR}/raw_ip_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/raw_ip.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_input.c ${CMAKE_CURRENT_LIST_DIR}/tcp_input_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_input.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_output.c ${CMAKE_CURRENT_LIST_DIR}/tcp_output_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_output.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_sack.c ${CMAKE_CURRENT_LIST_DIR}/tcp_sack_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_sack.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_subr.c ${CMAKE_CURRENT_LIST_DIR}/tcp_subr_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_subr.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_timer.c ${CMAKE_CURRENT_LIST_DIR}/tcp_timer_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_timer.c)
net_ps_prepare_patched(${netbsd3}/netinet/tcp_usrreq.c ${CMAKE_CURRENT_LIST_DIR}/tcp_usrreq_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_usrreq.c)
net_ps_prepare_patched(${netbsd}/netinet/udp_usrreq.c ${CMAKE_CURRENT_LIST_DIR}/udp_usrreq_sce.patch
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/udp_usrreq.c)
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
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/in_proto.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_icmp.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_id.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_input.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/ip_output.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/raw_ip.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_input.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_output.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_sack.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_subr.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_timer.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/tcp_usrreq.c
	${CMAKE_CURRENT_LIST_DIR}/netinet/udpp2p_usrreq.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/netinet/udp_usrreq.c
)
set_source_files_properties(${netbsd_sources} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-fgnu89-inline"
	COMPILE_DEFINITIONS NET_PS_NETBSD)
# Sony's kernel options: a PADT answers data for a session nobody owns
set_property(SOURCE ${CMAKE_CURRENT_BINARY_DIR}/netbsd/net/if_pppoe.c APPEND PROPERTY COMPILE_DEFINITIONS
	PPPOE_TERM_UNKNOWN_SESSIONS)
set(net_ps_sources
	${CMAKE_CURRENT_LIST_DIR}/netps_allocator_adapters.c
	${CMAKE_CURRENT_LIST_DIR}/netps_thread.c
	${CMAKE_CURRENT_LIST_DIR}/netps_process.c
	${CMAKE_CURRENT_LIST_DIR}/netps_resource.c
	${CMAKE_CURRENT_LIST_DIR}/netps_driver.c
	${CMAKE_CURRENT_LIST_DIR}/netps_socket_adapters.c
	${CMAKE_CURRENT_LIST_DIR}/netps_packet_adapters.c
	${CMAKE_CURRENT_LIST_DIR}/netps_process_callbacks.c
	${CMAKE_CURRENT_LIST_DIR}/netps_packet_cleanup.c
	${CMAKE_CURRENT_LIST_DIR}/netps_driver_updates.c
	${CMAKE_CURRENT_LIST_DIR}/netps_dump_reader.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_glue.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_statistics.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_startup.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_packet.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_emulation.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_socket.c
	${netbsd_sources}
	${netbsd3}/lib/libkern/intoa.c
	${netbsd3}/lib/libkern/md5c.c
	${CMAKE_CURRENT_LIST_DIR}/strlcpy.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_random.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_hmac.c
	${CMAKE_CURRENT_LIST_DIR}/net_ps_aes.c
	${CMAKE_CURRENT_LIST_DIR}/src/ps/osdependence/ngp/system_os.c
)
# The link of a SceNetPs module: sceNetPsDestroySocket calls
# sceNetPsRemoveSocketIdFromDescriptors in Sony's image, where ours drops the call
# (GCC finds the function const), so the link keeps it by name. The map gives
# tools/dbginfo.py the objects of records whose strings were discarded.
function(net_ps_link target)
	if(TARGET ${target})
		get_target_property(type ${target} TYPE)
		if(type STREQUAL "EXECUTABLE")
			target_link_options(${target} PRIVATE -Wl,--gc-sections
				-Wl,--undefined=sceNetPsRemoveSocketIdFromDescriptors
				-Wl,-Map=$<TARGET_FILE:${target}>.map)
		endif()
	endif()
endfunction()
