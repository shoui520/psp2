# SceSblACMgr: program and filesystem access checks.
# Sony's build keeps small static helpers out of line (fatfsGetAttr,
# pfsSetAttr, getAuthInfoAndCheckAttribute, isGameProgramForUser).
set(MODULE_COMPILE_OPTIONS -fno-inline-small-functions)
psp2_module(os0_kd_acmgr KIND kernel SOURCES
	acmgr.c
	acmgr_fatfs_attr.c
	acmgr_pfs_attr.c
	acmgr_type7_attr.c
	acmgr_idstorage_attr.c
	acmgr_for_user.c
)
