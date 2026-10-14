# SceSysStateMgr bootstrap and configuration reader.
psp2_module(os0_kd_sysstatemgr KIND kernel SOURCES
	source/sysstatemgr.c
	source/confreader.c
	source/confreader_utils.c
	source/after_boot.c
)
