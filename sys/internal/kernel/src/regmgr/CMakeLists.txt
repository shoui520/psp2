# os0:kd/bootimage/regmgr - SceRegistryMgr. Sony built it at -O3: its loops are
# prefetched and unrolled, and the number parser is cloned for constant arguments.
set(MODULE_COMPILE_OPTIONS -O3)
psp2_module(os0_kd_bootimage_regmgr KIND kernel COMPILER gcc48 SKELETON SOURCES
	regmgr.c
	regmgr_movevalue.c
	regmgr_convertvalue.c
	regmgr_wrapreg.c
	regmgr_service.c
	regmgr_addin.c
	regmgr_syscalls.c
	regmgr_debugger.c
	regmgr_tool.c
	reg_registry.c
	reg_category.c
	reg_key.c
	reg_platform.c
	reg_memory.c
)
