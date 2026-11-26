# SceHandwriting (vs0:sys/external/libhandwriting.suprx): online handwriting
# recognition library built with SNC 3.57, linking libsnc and libSceDeflt (GCC 4.8 -Os).
psp2_module(vs0_sys_external_libhandwriting KIND user COMPILER snc SOURCES
	hwr_api.c
	hwr_engine.c
	hwr_stroke.c
	hwr_code.c
	hwr_match.c
	hwr_util.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
	../../../../sdk/target/src/deflt/gziphdr.c
	../../../../sdk/target/src/deflt/deflate.S
	../../../../sdk/target/src/deflt/deflate2.S
	../../../../sdk/target/src/deflt/deflate_common.c
)
if(TARGET vs0_sys_external_libhandwriting)
	target_include_directories(vs0_sys_external_libhandwriting PRIVATE
		"${CMAKE_SOURCE_DIR}/sys/sdk/target/src/deflt")
	target_link_options(vs0_sys_external_libhandwriting PRIVATE -Wl,--gc-sections)
endif()
set_source_files_properties(hwr_api.c hwr_engine.c hwr_code.c PROPERTIES
	COMPILE_OPTIONS -Xautoinlinesize=0)
set_source_files_properties(../../../../sdk/target/src/libsnc/i32ops.c PROPERTIES
	COMPILE_DEFINITIONS LIBSNC_OMIT_MODULO)
set_source_files_properties(../../../../sdk/target/src/libsnc/aeabi_div0.c PROPERTIES
	COMPILE_DEFINITIONS LIBSNC_OMIT_LDIV0)
set_source_files_properties(../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c PROPERTIES COMPILE_OPTIONS -Xblxcall=1)
set(hwr_deflt_options -Os -fno-stack-protector -foptimize-sibling-calls
	-fdelete-null-pointer-checks -fsection-anchors -fshrink-wrap -funwind-tables
	-ffunction-sections -fdata-sections)
set_source_files_properties(../../../../sdk/target/src/deflt/gziphdr.c
	../../../../sdk/target/src/deflt/deflate_common.c PROPERTIES
	COMPILE_DEFINITIONS PSP2_COMPILER_GCC48 COMPILE_OPTIONS "${hwr_deflt_options}")
set_source_files_properties(../../../../sdk/target/src/deflt/deflate.S
	../../../../sdk/target/src/deflt/deflate2.S PROPERTIES
	COMPILE_DEFINITIONS DEFLATE_MEMCPY=sceClibMemcpy_safe)
