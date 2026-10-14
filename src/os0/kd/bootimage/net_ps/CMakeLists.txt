# generated skeleton (T2): stub bodies, original module/export/import tables
# Originals have no SSP imports or guards, including if_clone_list's 16-byte
# stack buffer and Sleep/StartStack's address-taken locals in both TOOL builds.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
psp2_module(os0_kd_bootimage_net_ps KIND kernel PARTIAL SOURCES
	../../../../common/netps_allocator_adapters.c
	../../../../common/netps_thread.c
	../../../../common/netps_process.c
	../../../../common/netps_resource.c
	../../../../common/netps_driver.c
	../../../../common/netps_socket_adapters.c
	../../../../common/netps_packet_adapters.c
	../../../../common/netps_process_callbacks.c
	../../../../common/netps_packet_cleanup.c
	../../../../common/netps_driver_updates.c
	../../../../common/netps_dump_reader.c
	source/net_ps_glue.c
	source/net_ps_statistics.c
	source/net_ps_startup.c
	source/net_ps_packet.c
	source/net_ps_emulation.c
	source/net_ps_socket.c
	source/strlcpy.c
	source/net_ps_hmac.c
	source/net_ps_aes.c
	source/system_os.c
	source/net_ps.c
)
