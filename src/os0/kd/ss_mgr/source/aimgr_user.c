/* Aimgr syscalls: the OpenPSID for user processes. */
#include "ssmgr_internal.h"
#include <psp2kern/kernel/sysmem.h>
#include <sce/syscall.h>

#define AIMGR_ERROR_INVALID_ARGUMENT	0x800F0C16

/* kept out of line in every build */
__attribute__((noinline))
static int aimgrGetOpenPsIdForUser(SceOpenPsId *openPsId)
{
	SceUInt32 syscallFrame;
	int ret;

	if (!openPsId)
		return AIMGR_ERROR_INVALID_ARGUMENT;

	syscallFrame = sceSyscallEnter();
	ret = ksceSblAimgrGetOpenPsId(openPsId);
	sceSyscallLeave(syscallFrame);
	return ret;
}

int _sceKernelGetOpenPsId(SceOpenPsId *openPsId)
{
	SceOpenPsId *kernelCopy;
	int ret, copyRet;

	if (!openPsId)
		return aimgrGetOpenPsIdForUser(NULL);

	kernelCopy = __builtin_alloca(sizeof(*kernelCopy));
	*kernelCopy = (SceOpenPsId){ { 0 } };
	ret = aimgrGetOpenPsIdForUser(kernelCopy);
	copyRet = ksceKernelCopyToUser(openPsId, kernelCopy, sizeof(*kernelCopy));
	if (copyRet < 0)
		return copyRet;
	return ret;
}
