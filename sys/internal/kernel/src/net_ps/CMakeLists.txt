# generated skeleton (T2): stub bodies, original module/export/import tables
# Originals have no SSP imports or guards, including if_clone_list's 16-byte
# stack buffer and Sleep/StartStack's address-taken locals in both TOOL builds.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
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
	strlcpy.c
	net_ps_hmac.c
	net_ps_aes.c
	src/ps/osdependence/ngp/system_os.c
	net_ps.c
)
