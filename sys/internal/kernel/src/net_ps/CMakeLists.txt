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
set(netbsd_sources
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/kern_subr.c
	${netbsd}/kern/kern_timeout.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_domain.c
	${netbsd}/kern/uipc_mbuf2.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_mbuf.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket2.c
	${CMAKE_CURRENT_BINARY_DIR}/netbsd/kern/uipc_socket.c
)
set_source_files_properties(${netbsd_sources} PROPERTIES COMPILE_OPTIONS "-fno-strict-aliasing;-fgnu89-inline"
	COMPILE_DEFINITIONS NET_PS_NETBSD)
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
