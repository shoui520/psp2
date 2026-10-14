# SceSblACMgr: program and filesystem access checks.
# Sony's build keeps small static helpers out of line (fatfsGetAttr,
# pfsSetAttr, getAuthInfoAndCheckAttribute, isGameProgramForUser).
set(MODULE_COMPILE_OPTIONS -fno-inline-small-functions)
psp2_module(os0_kd_acmgr KIND kernel SOURCES
	source/acmgr.c
	source/acmgr_fatfs_attr.c
	source/acmgr_pfs_attr.c
	source/acmgr_type7_attr.c
	source/acmgr_idstorage_attr.c
	source/acmgr_for_user.c
)
