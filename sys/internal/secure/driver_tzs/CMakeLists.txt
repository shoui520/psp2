# slb2:kbl/SceDriverTzs - the secure world's Pervasive, grab, EMC and crash dump driver
psp2_module(slb2_kbl_SceDriverTzs KIND kernel COMPILER gcc48 PARTIAL SOURCES
	power_entry.S
	pervasive_tzs.c
	pervasive_control_tzs.c
	pervasive_clock_tzs.c
	grab.c
	emc_tzs.c
	power_tzs.c
)
# Sony built pervasive_tzs.c at -O3, as SceLowio: PervasiveInit's mapping
# loop prefetches its table (pld, only with -fprefetch-loop-arrays) and the
# Allegrex sequences are inlined into the power monitor call.
set_source_files_properties(pervasive_tzs.c power_tzs.c PROPERTIES COMPILE_OPTIONS "-O3")
