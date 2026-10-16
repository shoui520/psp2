# SceDeci4pLoadp: Sony's loadp.c.
psp2_module(os0_kd_deci4p_loadp KIND kernel SOURCES
	source/loadp.c
)
# Sony built this module with -O3 (prefetching loop in pdLoadpProcSpawnCmd)
target_compile_options(os0_kd_deci4p_loadp PRIVATE -O3 -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
