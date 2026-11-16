# SceLibFios2 (vs0:sys/external/libfios2.suprx), built by Sony with SNC 3.57 in C++: the
# FIOS2 runtime (scheduler, chunker, I/O filters, PSARC dearchiver) over the SCE WWS
# platform layer, with SNC's runtime (libsnc) and the SDK's libSceDeflt for the archives'
# zlib streams.
enable_language(CXX)
psp2_module(vs0_sys_external_libfios2 KIND user COMPILER snc PARTIAL SOURCES
	prx.cpp
	zlib_codec.cpp
	time.cpp
	platform.cpp
	allocator.cpp
	api.cpp
	libfios2.c
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64mod.c
	../../../../sdk/target/src/libsnc/i64shr.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/deflt/zlibhdr.c
	../../../../sdk/target/src/deflt/deflate.c
	../../../../sdk/target/src/deflt/deflate_common.c
)
if(TARGET vs0_sys_external_libfios2)
	# the C++ sources use the SDK's C++ library (<algorithm>), whose headers need the SDK's C
	# headers under them
	target_include_directories(vs0_sys_external_libfios2 BEFORE PRIVATE
		$<$<COMPILE_LANGUAGE:CXX>:$ENV{PSP2_SDK}/target/include>)
	target_include_directories(vs0_sys_external_libfios2 PRIVATE
		"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt")
endif()
# SNC's runtime (libsnc): its personality routines call through ip
set_source_files_properties(../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/unwind_pr2.c PROPERTIES COMPILE_OPTIONS -Xblxcall=1)
# the SDK's libSceDeflt.a, which Sony built with GCC 4.8 as its objects' DWARF records:
# -Os -funwind-tables -ffunction-sections -fdata-sections, none of the kernel's code flags
set(fios2_deflt_options -Os -fno-stack-protector -foptimize-sibling-calls
	-fdelete-null-pointer-checks -fsection-anchors -fshrink-wrap -funwind-tables
	-ffunction-sections -fdata-sections)
set_source_files_properties(../../../../sdk/target/src/deflt/zlibhdr.c
	../../../../sdk/target/src/deflt/deflate.c
	../../../../sdk/target/src/deflt/deflate_common.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS "${fios2_deflt_options}")
# deflate.o calls sceClibMemcpy (Sony's name for NID 2E3B02A1, the tree's sceClibMemcpy_safe);
# here it binds to the library's own sceClibMemcpy
set_property(SOURCE ../../../../sdk/target/src/deflt/deflate.c APPEND PROPERTY
	COMPILE_DEFINITIONS sceClibMemcpy_safe=sceClibMemcpy)
