# Allocator adapters are shared with the original's bootimage/net_ps copy.
# The remaining export bodies are honest placeholders during C conversion.
# All present originals lack SSP guard/fail imports and guards on stack locals.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
psp2_module(os0_kd_net_ps_dev KIND kernel PARTIAL SOURCES
	../net_ps/netps_allocator_adapters.c
	../net_ps/netps_thread.c
	../net_ps/netps_process.c
	../net_ps/netps_resource.c
	../net_ps/netps_driver.c
	../net_ps/netps_socket_adapters.c
	../net_ps/netps_packet_adapters.c
	../net_ps/netps_process_callbacks.c
	../net_ps/netps_packet_cleanup.c
	../net_ps/netps_driver_updates.c
	net_ps_dump.c
	../net_ps/netps_dump_reader.c
	net_ps_dev.c
)
target_include_directories(os0_kd_net_ps_dev PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../net_ps/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../net_ps"
)
