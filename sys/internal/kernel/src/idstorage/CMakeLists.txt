# SceIdStorage sector index and cached leaf storage. Sony built it at -O3: the partition scan
# is unrolled and the commit's index move is vectorized.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_idstorage KIND kernel COMPILER gcc48 SOURCES
	idstorage.c
)
