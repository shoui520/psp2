# SceGpuEs4CoreDump: GPU crash capture, analysis and dump files (Sony's file names unknown).
psp2_module(os0_kd_gpucoredump_es4 KIND kernel COMPILER gcc48 SOURCES
	gpucoredump_file.c
	gpucoredump.c
	gpucoredump_analysis.c
	gpucoredump_writer.c
	gpucoredump_log.c
	gpucoredump_tables.c
)
# Sony built this module with -Os (literal pools, no loop hoisting) and without strict
# aliasing (list fields reloaded after every store)
target_compile_options(os0_kd_gpucoredump_es4 PRIVATE -Os -fno-strict-aliasing)

include(${CMAKE_CURRENT_LIST_DIR}/../gpu/sgx_ddk.cmake)
target_include_directories(os0_kd_gpucoredump_es4 PRIVATE ${PSP2_SGX_DDK_INCLUDE_DIRS})
