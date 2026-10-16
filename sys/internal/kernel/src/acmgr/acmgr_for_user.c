/*
 * User syscalls (library SceSblACMgr). No debug records; the file name follows
 * Sony's <module>_for_user.c (iofilemgr_for_user.c, threadmgr_for_user.c).
 */
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include "acmgr_internal.h"

static int isGameProgramForUser(SceUInt32 *result);

int _sceSblACMgrIsGameProgram(SceUInt32 *pResult)
{
	SceUInt32 *result;
	int ret, err;

	if (pResult == NULL)
		return isGameProgramForUser(NULL);
	result = __builtin_alloca(sizeof(*result));
	*result = 0;
	ret = isGameProgramForUser(result);
	err = ksceKernelCopyToUser(pResult, result, sizeof(*result));
	return err < 0 ? err : ret;
}

/* name unknown: the calling process's answer */
static int __attribute__((noinline)) isGameProgramForUser(SceUInt32 *result)
{
	if (result == NULL)
		return ACMGR_ERROR_INVALID_ARGUMENT;
	*result = sceSblACMgrIsGameProgram(0);
	return 0;
}
