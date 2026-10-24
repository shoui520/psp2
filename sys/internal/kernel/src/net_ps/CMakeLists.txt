include(net_ps.cmake)
psp2_module(os0_kd_bootimage_net_ps KIND kernel COMPILER gcc48 SOURCES ${net_ps_sources})
net_ps_link(os0_kd_bootimage_net_ps)
