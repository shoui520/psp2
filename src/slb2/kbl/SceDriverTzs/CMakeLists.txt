# slb2:kbl/SceDriverTzs - the secure world's Pervasive, grab, EMC and crash dump driver
psp2_module(slb2_kbl_SceDriverTzs KIND kernel PARTIAL SOURCES
	source/pervasive_tzs.c
	source/pervasive_control_tzs.c
	source/pervasive_clock_tzs.c
	source/grab.c
	source/emc_tzs.c
)
# Sony built pervasive_tzs.c at -O3, as SceLowio: PervasiveInit's mapping
# loop prefetches its table (pld, only with -fprefetch-loop-arrays) and the
# Allegrex sequences are inlined into the power monitor call.
set_source_files_properties(source/pervasive_tzs.c PROPERTIES COMPILE_OPTIONS "-O3")
