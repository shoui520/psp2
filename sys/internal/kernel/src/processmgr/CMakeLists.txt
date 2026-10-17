psp2_module(os0_kd_processmgr KIND kernel COMPILER gcc48 SOURCES
	processmgr.c
	budget.c
	libctime.c
)
# Sony's flags: reads through null pointers are kept as they are written, calls
# stay calls (the debug asserts pass the caller's return address), and loops
# are not turned into memset calls
target_compile_options(os0_kd_processmgr PRIVATE -fno-delete-null-pointer-checks
	-fno-optimize-sibling-calls -fno-tree-loop-distribute-patterns)
# the NID 0 probes variable is the table of the static probe descriptors
target_link_options(os0_kd_processmgr PRIVATE
	-Wl,--defsym=module_nid0_8CE938B1=__start_processmgr_probes)
