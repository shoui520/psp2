/* Aimgr syscalls: the OpenPSID for user processes. */
#include "ssmgr_internal.h"
#include <sysmem/sysmem.h>
#include <intrmgr/syscall.h>
#include <sysmem/sblaimgr.h>

/* kept out of line in every build */
__attribute__((noinline))
static int aimgrGetOpenPsIdForUser(SceKernelOpenPsId *openPsId)
{
	SceUInt32 syscallFrame;
	int ret;

	if (!openPsId)
		return SCE_SBL_ERROR_AIMGR_INVALID_ARGUMENT;

	syscallFrame = sceSyscallEnter();
	ret = sceSblAimgrGetOpenPsId(openPsId);
	sceSyscallLeave(syscallFrame);
	return ret;
}

int _sceKernelGetOpenPsId(SceKernelOpenPsId *openPsId)
{
	SceKernelOpenPsId *kernelCopy;
	int ret, copyRet;

	if (!openPsId)
		return aimgrGetOpenPsIdForUser(NULL);

	kernelCopy = __builtin_alloca(sizeof(*kernelCopy));
	*kernelCopy = (SceKernelOpenPsId){ { 0 } };
	ret = aimgrGetOpenPsIdForUser(kernelCopy);
	copyRet = sceKernelCopyToUser(openPsId, kernelCopy, sizeof(*kernelCopy));
	if (copyRet < 0)
		return copyRet;
	return ret;
}
