# The development kits' SceNetPs: net_ps's sources, with packet capture
# (sceNetPsDumpCreate opens a capture where the bootimage's only checks its arguments)
include(../net_ps.cmake)
add_compile_definitions(NET_PS_PACKET_CAPTURE)
psp2_module(os0_kd_net_ps_dev KIND kernel COMPILER gcc48 SOURCES ${net_ps_sources})
target_include_directories(os0_kd_net_ps_dev PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include"
	"${CMAKE_CURRENT_SOURCE_DIR}/..")
net_ps_link(os0_kd_net_ps_dev)
