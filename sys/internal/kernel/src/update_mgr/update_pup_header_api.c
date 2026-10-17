#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>

int sceSblUsVerifyPupHeaderForUser(const char *path)
{
	char buffer[1024];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = ksceKernelStrncpyFromUser(buffer, path, sizeof(buffer));
	if (result < 0 || result == 1024)
		result = (int)0x800f0216;
	else
		result = sceSblUsVerifyPupHeader(buffer);
	EXIT_SYSCALL(state);
	return result;
}
