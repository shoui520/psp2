#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>

int sceSblUsInformUpdateStartedForUser(int number1, int number2,
				     const char *str, SceSize len)
{
	char buffer[64];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if ((len - 1 > 63) | (str == NULL)) {
		sceSyscallLeave(state);
		return (int)0x800f0216;
	}
	memset(buffer, 0, sizeof(buffer));
	result = sceKernelCopyFromUser(buffer, str, len);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	buffer[63] = 0;
	result = DeciUtilInformUpdateStarted(number1, number2, buffer, len);
	sceSyscallLeave(state);
	return result;
}

int sceSblUsInformUpdateOngoingForUser(int number1, int number2)
{
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	result = DeciUtilInformUpdateOngoing(number1, number2);
	sceSyscallLeave(state);
	return result;
}

int sceSblUsInformUpdateFinishedForUser(int number, const char *str, SceSize len)
{
	char buffer[64];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if ((len - 1 > 63) | (str == NULL)) {
		sceSyscallLeave(state);
		return (int)0x800f0216;
	}
	memset(buffer, 0, sizeof(buffer));
	result = sceKernelCopyFromUser(buffer, str, len);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	buffer[63] = 0;
	result = DeciUtilInformUpdateFinished(number, buffer, len);
	sceSyscallLeave(state);
	return result;
}
