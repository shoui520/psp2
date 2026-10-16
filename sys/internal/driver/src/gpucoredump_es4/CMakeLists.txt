# SceGpuEs4CoreDump: GPU crash capture, analysis and dump files (Sony's file names unknown).
psp2_module(os0_kd_gpucoredump_es4 KIND kernel SOURCES
	source/gpucoredump_file.c
	source/gpucoredump.c
	source/gpucoredump_analysis.c
	source/gpucoredump_writer.c
	source/gpucoredump_log.c
	source/gpucoredump_tables.c
)
# Sony built this module with -Os (literal pools, no loop hoisting) and without strict
# aliasing (list fields reloaded after every store)
target_compile_options(os0_kd_gpucoredump_es4 PRIVATE -Os -fno-strict-aliasing)
