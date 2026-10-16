/*
 * User data: a process's coredump handler writes its own data into a buffer that
 * becomes the user-data segment (seg_user_data.c), under userDataMutexId. Also
 * the SceCoredump syscalls that register the handler. The userData* names are
 * not Sony's (unknown).
 */

#include <psp2kern/types.h>
#include <psp2kern/kernel/sysmem.h>
#include <sce/coredump.h>
#include <sce/syscall.h>

extern int oalMutexInit(SceUID *mutexId);
extern int oalMutexFinal(SceUID mutexId);
extern int oalMutexLock(SceUID mutexId);
extern int oalMutexUnlock(SceUID mutexId);
extern int oalRegisterCoredumpHandler(SceCoredumpHandler handler,
	SceSize stackSize, void *pCommon);
extern int oalUnregisterCoredumpHandler(void);

SceSize userDataCapacity;
SceSize userDataUsedSize;
SceUID userDataMutexId;
void *userDataBuffer;

int userDataInit(void)
{
	int result;

	result = oalMutexInit(&userDataMutexId);
	if (result == 0) {
		userDataBuffer = NULL;
		userDataCapacity = 0;
		userDataUsedSize = 0;
	}
	return result;
}

int userDataFinal(void)
{
	return oalMutexFinal(userDataMutexId);
}

int sceCoredumpRegisterCoredumpHandler(SceCoredumpHandler handler,
	SceSize stackSize, void *pCommon)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = oalRegisterCoredumpHandler(handler, stackSize, pCommon);
	sceSyscallLeave(state);
	return result;
}

int sceCoredumpUnregisterCoredumpHandler(void)
{
	SceUInt32 state = sceSyscallEnter();
	int result;

	result = oalUnregisterCoredumpHandler();
	sceSyscallLeave(state);
	return result;
}

/* appends up to size bytes; returns the bytes written */
int sceCoredumpWriteUserData(const void *data, SceSize size)
{
	SceUInt32 state = sceSyscallEnter();
	SceSize length;
	int result;

	result = oalMutexLock(userDataMutexId);
	if (result == 0) {
		if (userDataBuffer == NULL) {
			result = SCE_COREDUMP_ERROR_NOT_IN_COREDUMP_HANDLER;
			oalMutexUnlock(userDataMutexId);
		} else {
			length = userDataCapacity - userDataUsedSize;
			if (size < length)
				length = size;
			if (ksceKernelCopyFromUser((char *)userDataBuffer + userDataUsedSize,
						   data, length) < 0) {
				result = SCE_COREDUMP_ERROR_PARAM;
				oalMutexUnlock(userDataMutexId);
			} else {
				result = length;
				userDataUsedSize += length;
				oalMutexUnlock(userDataMutexId);
			}
		}
	}
	sceSyscallLeave(state);
	return result;
}

int userDataGetUsedSize(void)
{
	SceSize usedSize;
	int result;

	result = oalMutexLock(userDataMutexId);
	if (result != 0)
		return result;
	usedSize = userDataUsedSize;
	result = oalMutexUnlock(userDataMutexId);
	if (result != 0)
		return result;
	return usedSize;
}

int userDataSetBuffer(void *buffer, SceSize capacity)
{
	int result;

	result = oalMutexLock(userDataMutexId);
	if (result != 0)
		return result;
	userDataBuffer = buffer;
	userDataCapacity = capacity;
	userDataUsedSize = 0;
	return oalMutexUnlock(userDataMutexId);
}

int userDataClearBuffer(void)
{
	int result;

	result = oalMutexLock(userDataMutexId);
	if (result != 0)
		return result;
	userDataBuffer = NULL;
	userDataCapacity = 0;
	userDataUsedSize = 0;
	return oalMutexUnlock(userDataMutexId);
}
