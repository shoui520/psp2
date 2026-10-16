#include <string.h>
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include "update_internal.h"

int ksceSblACIsSystemProgram(SceUID pid);

int sceSblUsInformUpdateStartedForUser(int number1, int number2,
				     const char *str, SceSize len)
{
	char buffer[64];
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if ((len - 1 > 63) | (str == NULL)) {
		EXIT_SYSCALL(state);
		return (int)0x800f0216;
	}
	memset(buffer, 0, sizeof(buffer));
	result = ksceKernelCopyFromUser(buffer, str, len);
	if (result != 0) {
		EXIT_SYSCALL(state);
		return result;
	}
	buffer[63] = 0;
	result = DeciUtilInformUpdateStarted(number1, number2, buffer, len);
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsInformUpdateOngoingForUser(int number1, int number2)
{
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	result = DeciUtilInformUpdateOngoing(number1, number2);
	EXIT_SYSCALL(state);
	return result;
}

int sceSblUsInformUpdateFinishedForUser(int number, const char *str, SceSize len)
{
	char buffer[64];
	unsigned int state;
	int result;

	if (!ksceSblACIsSystemProgram(0))
		return (int)0x800f022c;
	ENTER_SYSCALL(state);
	if ((len - 1 > 63) | (str == NULL)) {
		EXIT_SYSCALL(state);
		return (int)0x800f0216;
	}
	memset(buffer, 0, sizeof(buffer));
	result = ksceKernelCopyFromUser(buffer, str, len);
	if (result != 0) {
		EXIT_SYSCALL(state);
		return result;
	}
	buffer[63] = 0;
	result = DeciUtilInformUpdateFinished(number, buffer, len);
	EXIT_SYSCALL(state);
	return result;
}
