#include <string.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include "update_internal.h"
#include <acmgr/acmgr.h>
#include <intrmgr/syscall.h>

union UpdateSwInfoName {
	uint32_t words[8];
	char text[32];
};

int sceSblUsSetSwInfoIntForUser(const char *name, SceSize nameLength,
			      uint32_t value)
{
	union UpdateSwInfoName nameBuffer;
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if ((name == NULL) | (nameLength - 1 > 30)) {
		sceSyscallLeave(state);
		return (int)0x800f0216;
	}
	nameBuffer = (union UpdateSwInfoName){0};
	result = sceKernelCopyFromUser(nameBuffer.text, name, nameLength);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	nameBuffer.text[31] = 0;
	result = DeciUtilSetSwInfoInt(nameBuffer.text, nameLength, value);
	sceSyscallLeave(state);
	return result;
}

int sceSblUsSetSwInfoStrForUser(const char *name, SceSize nameLength,
			      const char *value, SceSize valueLength)
{
	union UpdateSwInfoName nameBuffer;
	char valueBuffer[128];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	if (((nameLength - 1 > 30) | (name == NULL)) ||
	    ((valueLength - 1 > 126) | (value == NULL))) {
		sceSyscallLeave(state);
		return (int)0x800f0216;
	}
	nameBuffer = (union UpdateSwInfoName){0};
	memset(valueBuffer, 0, sizeof(valueBuffer));
	result = sceKernelCopyFromUser(nameBuffer.text, name, nameLength);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	nameBuffer.text[31] = 0;
	result = sceKernelCopyFromUser(valueBuffer, value, valueLength);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	valueBuffer[127] = 0;
	result = DeciUtilSetSwInfoStr(nameBuffer.text, nameLength, valueBuffer, valueLength);
	sceSyscallLeave(state);
	return result;
}

int sceSblUsSetSwInfoBinForUser(const char *name, SceSize nameLength,
			      const void *value, SceSize valueLength)
{
	union UpdateSwInfoName nameBuffer;
	unsigned char valueBuffer[128];
	unsigned int state;
	int result;

	if (!SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0))
		return (int)0x800f022c;
	state = sceSyscallEnter();
	/* The first validation failure retains the entered syscall state. */
	if ((name == NULL) | (nameLength - 1 > 30))
		return (int)0x800f0216;
	if ((value == NULL) | (valueLength - 1 > 126)) {
		sceSyscallLeave(state);
		return (int)0x800f0216;
	}
	nameBuffer = (union UpdateSwInfoName){0};
	memset(valueBuffer, 0, sizeof(valueBuffer));
	result = sceKernelCopyFromUser(nameBuffer.text, name, nameLength);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	nameBuffer.text[31] = 0;
	result = sceKernelCopyFromUser(valueBuffer, value, valueLength);
	if (result != 0) {
		sceSyscallLeave(state);
		return result;
	}
	valueBuffer[127] = 0;
	result = DeciUtilSetSwInfoBin(nameBuffer.text, nameLength, valueBuffer, valueLength);
	sceSyscallLeave(state);
	return result;
}
