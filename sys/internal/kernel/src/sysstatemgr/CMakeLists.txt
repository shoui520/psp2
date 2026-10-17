# SceSysStateMgr bootstrap and configuration reader.
psp2_module(os0_kd_sysstatemgr KIND kernel COMPILER gcc48 SOURCES
	sysstatemgr.c
	confreader.c
	confreader_utils.c
	after_boot.c
)
