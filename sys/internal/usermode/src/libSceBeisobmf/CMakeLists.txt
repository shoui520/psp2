enable_language(CXX)
# SceBeisobmf (vs0:sys/external/libSceBeisobmf.suprx), built by Sony with SNC 3.57 in C++.
psp2_module(vs0_sys_external_libSceBeisobmf KIND user COMPILER snc SOURCES
	be_boxes_ext.cpp
	be_clib.cpp
	be_isobmf_api.cpp
	be_isobmf_parser.cpp
	be_list.cpp
	be_field.cpp
	be_mem.cpp
	be_tree.cpp
	be_box.cpp
	be_full_box.cpp
	be_boxes_core.cpp
	be_command.cpp
	../../../../sdk/target/src/libsnc/i64ops.c
	../../../../sdk/target/src/libsnc/i64mod.c
	../../../../sdk/target/src/libsnc/i32ops.c
	../../../../sdk/target/src/libsnc/unwind_pr0.c
	../../../../sdk/target/src/libsnc/unwind_pr1.c
	../../../../sdk/target/src/libsnc/aeabi_div0.c
)
set_source_files_properties(
	../../../../sdk/target/src/libsnc/i32ops.c
	PROPERTIES COMPILE_DEFINITIONS LIBSNC_OMIT_MODULO
)
target_compile_definitions(vs0_sys_external_libSceBeisobmf PRIVATE PSP2_STRIP_UNUSED)
target_link_options(vs0_sys_external_libSceBeisobmf PRIVATE -Wl,--gc-sections)
