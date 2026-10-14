psp2_module(os0_kd_bootimage_gpuinit_es4 KIND kernel SOURCES
	source/sgxinit.c
	source/programs.S
)

# Bootimage payloads occupy the original writable segment at +0x2000.
target_compile_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Os)
target_link_options(os0_kd_bootimage_gpuinit_es4 PRIVATE -Wl,-Tdata=0x81002000)
