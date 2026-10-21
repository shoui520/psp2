#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>

int sceSblUsVerifyPupForUser(const char *path)
{
	char buffer[1024];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	result = sceKernelStrncpyFromUser(buffer, path, sizeof(buffer));
	if (result >= 0 && result != 1024)
		result = sceSblUsVerifyPup(buffer);
	else
		result = (int)0x800f0216;
	sceSyscallLeave(state);
	return result;
}
