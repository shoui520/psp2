psp2_module(os0_kd_deci4p_drfp KIND kernel SOURCES
	drfp.c
	drfpif.c
	drfpfs.c
)
target_compile_options(os0_kd_deci4p_drfp PRIVATE -fno-optimize-sibling-calls
	-fno-delete-null-pointer-checks)
