/* PocketStation title syscall; source filename inferred. */

#include <scetypes.h>
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <compat/compat.h>
#include "compat_internal.h"

int sceCompatIsPocketStationTitle(const char *titleId)
{
	signed char localTitleId[10];
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (sceSblACMgrIsSystemProgram(0) == 1) {
		result = sceKernelCopyFromUser(localTitleId, titleId,
			sizeof(localTitleId));
		if (result >= 0) {
			if (localTitleId[9] != 0)
				return (int)0x80010016;
			result = compatIsPocketStationTitle(localTitleId);
		}
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}
