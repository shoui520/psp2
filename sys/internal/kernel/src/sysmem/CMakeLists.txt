# Partial Sysmem foundations; remaining bodies are explicit stubs.
set(sysmem_dir ${CMAKE_CURRENT_SOURCE_DIR})
include(sources.cmake)

psp2_module(os0_kd_sysmem KIND kernel COMPILER gcc48 SOURCES ${sysmem_sources})
sysmem_settings(os0_kd_sysmem)

# The data segment where the original has it, with room for the VELF tables after the text.
set(SYSMEM_DATA_BASE_4.00-DEBUGGER 0x81049000)
set(SYSMEM_DATA_BASE_3.650.011-TOOL 0x81030000)
set(SYSMEM_DATA_BASE_3.65-CEX 0x81030000)
set(SYSMEM_DATA_BASE_3.630.011-DEBUGGER 0x81030000)
fw_value(SYSMEM_DATA_BASE SYSMEM_DATA_BASE)
target_link_options(os0_kd_sysmem PRIVATE -Wl,-Tdata,${SYSMEM_DATA_BASE})
