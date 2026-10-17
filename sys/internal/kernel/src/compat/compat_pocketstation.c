/* PocketStation title syscall; source filename inferred. */

#include <psp2kern/types.h>
#include <psp2kern/kernel/acmgr.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>

int compatIsPocketStationTitle(const signed char *titleId);
int sceCompatIsPocketStationTitle(const char *titleId);

int sceCompatIsPocketStationTitle(const char *titleId)
{
	signed char localTitleId[10];
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (ksceSblACMgrIsSystemProgram(0) == 1) {
		result = ksceKernelCopyFromUser(localTitleId, titleId,
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
