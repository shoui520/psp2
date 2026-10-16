# Allocator adapters are shared with the original's bootimage/net_ps copy.
# The remaining export bodies are honest placeholders during C conversion.
# All present originals lack SSP guard/fail imports and guards on stack locals.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
psp2_module(os0_kd_net_ps_dev KIND kernel PARTIAL SOURCES
	../../../../../common/netps_allocator_adapters.c
	../../../../../common/netps_thread.c
	../../../../../common/netps_process.c
	../../../../../common/netps_resource.c
	../../../../../common/netps_driver.c
	../../../../../common/netps_socket_adapters.c
	../../../../../common/netps_packet_adapters.c
	../../../../../common/netps_process_callbacks.c
	../../../../../common/netps_packet_cleanup.c
	../../../../../common/netps_driver_updates.c
	source/net_ps_dump.c
	../../../../../common/netps_dump_reader.c
	source/net_ps_dev.c
)
target_include_directories(os0_kd_net_ps_dev PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../net_ps/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/../net_ps/source"
)
