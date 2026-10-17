psp2_module(os0_kd_bootimage_gpuinit_es4 KIND kernel COMPILER gcc48 SOURCES
	devices/sgx/sgxinit.c
	programs.S
)

# Bootimage payloads occupy the original writable segment at +0x2000.
target_compile_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Os)
target_link_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Wl,-Tdata=0x81002000)

include(${CMAKE_CURRENT_LIST_DIR}/../gpu/sgx_ddk.cmake)
target_include_directories(os0_kd_bootimage_gpuinit_es4 PRIVATE ${PSP2_SGX_DDK_INCLUDE_DIRS})
