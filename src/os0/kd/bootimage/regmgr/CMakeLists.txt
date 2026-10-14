# os0:kd/bootimage/regmgr - SceRegistryMgr. Sony built it at -O3: its loops are
# prefetched and unrolled, and the number parser is cloned for constant arguments.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_regmgr KIND kernel SKELETON SOURCES
	source/regmgr.c
	source/regmgr_movevalue.c
	source/regmgr_convertvalue.c
	source/regmgr_wrapreg.c
	source/regmgr_service.c
	source/regmgr_addin.c
	source/regmgr_syscalls.c
	source/regmgr_debugger.c
	source/regmgr_tool.c
	source/reg_registry.c
	source/reg_category.c
	source/reg_key.c
	source/reg_platform.c
	source/reg_memory.c
)
