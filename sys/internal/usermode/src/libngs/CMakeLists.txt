psp2_module(${PSP2_TARGET} KIND user COMPILER gcc48 SOURCES
	../../../../../toolchains/libgcc/udiv.S
	../../../../../toolchains/libgcc/idiv.S
	../../../../../toolchains/libgcc/dvmd_tls.S
	libngs.c
	ngs_clib.c
	ngs_at9.c
	ngs_params.c
	ngs_sulpha_tables.c
	ngs_sulpha_agents.c
	ngs_sulpha_ngs.c
	ngs_atomic.S
)

target_include_directories(${PSP2_TARGET} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../toolchains/libgcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../toolchains/libgcc/target
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/gcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/gcc/ginclude
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/libgcc
	${CMAKE_CURRENT_SOURCE_DIR}/../../../../../contrib/gcc-4.8-2014.05/include
)

include(../../../../../toolchains/libgcc/lib1funcs.cmake)
gcc48_use_lib1funcs(${PSP2_TARGET})

set_source_files_properties(
	libngs.c
	ngs_clib.c
	ngs_at9.c
	ngs_params.c
	ngs_sulpha_tables.c
	ngs_sulpha_agents.c
	ngs_sulpha_ngs.c
	PROPERTIES COMPILE_OPTIONS "-mfloat-abi=hard"
)

target_link_options(${PSP2_TARGET} PRIVATE -Wl,--no-warn-mismatch)
