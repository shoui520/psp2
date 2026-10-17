# Allocator adapters are shared with the original's bootimage/net_ps copy.
# The remaining export bodies are honest placeholders during C conversion.
# All present originals lack SSP guard/fail imports and guards on stack locals.
set(MODULE_COMPILE_OPTIONS -fno-stack-protector)
psp2_module(os0_kd_net_ps_dev KIND kernel COMPILER gcc48 PARTIAL SOURCES
	../netps_allocator_adapters.c
	../netps_thread.c
	../netps_process.c
	../netps_resource.c
	../netps_driver.c
	../netps_socket_adapters.c
	../netps_packet_adapters.c
	../netps_process_callbacks.c
	../netps_packet_cleanup.c
	../netps_driver_updates.c
	net_ps_dump.c
	../netps_dump_reader.c
	net_ps_dev.c
)
target_include_directories(os0_kd_net_ps_dev PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/.."
)
