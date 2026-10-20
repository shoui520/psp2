# SceSysStateMgr bootstrap and configuration reader.
psp2_module(os0_kd_sysstatemgr KIND kernel COMPILER gcc48 SOURCES
	sysstatemgr.c
	confreader.c
	confreader_utils.c
	after_boot.c
)
# Sony built this module with -Os (literal pools, push for the frame, loops not rotated).
target_compile_options(os0_kd_sysstatemgr PRIVATE -Os)
# Sony's -Os divides by 1000 with a multiply; ours calls __aeabi_idiv, which no kernel
# module links, so the one file that divides keeps -O2.
set_source_files_properties(sysstatemgr.c PROPERTIES COMPILE_OPTIONS -O2)
