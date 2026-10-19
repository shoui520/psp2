# SceSblACMgr: program and filesystem access checks.
# Sony's build keeps small static helpers out of line (fatfsGetAttr,
# pfsSetAttr, getAuthInfoAndCheckAttribute, isGameProgramForUser), and each
# function in its own section (a 64-bit literal ldrd reads sits 8-aligned).
set(MODULE_COMPILE_OPTIONS -fno-inline-small-functions -ffunction-sections)
psp2_module(os0_kd_acmgr KIND kernel COMPILER gcc48 SOURCES
	acmgr.c
	acmgr_fatfs_attr.c
	acmgr_pfs_attr.c
	acmgr_type7_attr.c
	acmgr_idstorage_attr.c
	acmgr_for_user.c
)
