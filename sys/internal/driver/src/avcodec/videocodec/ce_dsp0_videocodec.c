/* Sony source: videocodec/ce_dsp0_videocodec.c. */
#include "../audiodec/audiodec_internal.h"
#include "videocodec_internal.h"
#include <sysmem/debug.h>
#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <sysmem/cpu.h>
#include <string.h>

int dsp0VideocodecLocalStorageId = -1;

#if FW_DEBUG
SCE_DBGINFO_DEFINE(static, dsp0VideocodecCreateStorage, 415,
		   "_sceDsp0VideocodecProcessCreateEntry");
SCE_DBGINFO_DEFINE(static, dsp0VideocodecExitStorage, 436,
		   "_sceDsp0VideocodecProcessExitEntry");
SCE_DBGINFO_DEFINE(static, dsp0VideocodecGlobalStorage, 319,
		   "_dsp0GetDsp0VideocodecGlobal");
#endif

/* RPC addresses are virtual on input and physical after packing. */
typedef struct Dsp0VideocodecRpcBuffers {
	uintptr_t bufferAddress;
	uintptr_t optionalBufferAddress;
	SceSize bufferSize;
	SceSize optionalBufferSize;
} Dsp0VideocodecRpcBuffers;

_Static_assert(sizeof(Dsp0VideocodecRpcBuffers) == 16, "DSP0 RPC buffers size");

extern int SceCodecEngineWrapperForDriver_0D3BB164(unsigned int command, ...);
extern unsigned int SceCodecEngineWrapperForDriver_F9495589(unsigned int physical);
extern int SceCodecEngineWrapperForDriver_3C5E9CC3(unsigned int argument,
						unsigned int address, SceSize size);
int _lockProcessSuspendCore(void);
int _unlockProcessSuspendCore(void);
int dsp0VideocodecCalculateLibraryMemory(const Dsp0VideocodecInitParam *param,
				       SceSize *memorySize, SceSize *librarySize);
int dsp0VideocodecReleaseLibraryBuffers(Dsp0VideocodecGlobal *global);

/* The command's interpreting helper awaits its complete host layout. */
struct Dsp0VideocodecDecoderCommand;
struct Dsp0VideocodecLibraryCommand;
struct Dsp0VideocodecEncoderCommand;
int dsp0VideocodecCallDecoderCommand(struct Dsp0VideocodecDecoderCommand *command,
		unsigned int operation, const unsigned int *input);
int dsp0VideocodecCallEncoderCommand(struct Dsp0VideocodecEncoderCommand *command,
		unsigned int operation, const unsigned int *input);

void dsp0VideocodecFinishCommand(VeneziaRpcThunk *thunk)
{
	SceCodecEngineWrapperForDriver_07C53B4E(thunk);
	SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
	SceCodecEngineWrapperForDriver_1C2BC9F7(thunk);
}

/* The RPC pack ABI serializes Dsp0VideocodecRpcBuffers as four words. */
int dsp0VideocodecPackCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			    unsigned int *packed)
{
	VeneziaMemoryOption option;
	SceSize optionalSize;
	int ret;

	option.flags = 1;
	option.allocationSize = input[2];
	optionalSize = input[3];
	SceCodecEngineWrapperForDriver_47AED6FF(thunk, (void *)input[0],
					      option.allocationSize, &option);
	if (optionalSize != 0) {
		option.allocationSize = optionalSize;
		SceCodecEngineWrapperForDriver_47AED6FF(thunk, (void *)input[1],
						      optionalSize, &option);
		SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
		ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
		packed[0] = thunk->memories[0].physicalBase;
		packed[1] = thunk->memories[1].physicalBase;
	} else {
		SceCodecEngineWrapperForDriver_03EA6C2A(thunk);
		ret = SceCodecEngineWrapperForDriver_D1F94D9D(thunk);
		packed[0] = thunk->memories[0].physicalBase;
		packed[1] = input[1];
	}
	packed[2] = input[2];
	packed[3] = input[3];
	return ret;
}

void *dsp0VideocodecAllocMemory(unsigned int argument, SceSize size)
{
	unsigned int physical;

	physical = SceCodecEngineWrapperForDriver_0D3BB164(100, argument, size);
	return (void *)SceCodecEngineWrapperForDriver_F9495589(physical);
}

int dsp0VideocodecInitializeMemory(void *memory)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)memory;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(99,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

int dsp0VideocodecQueryMemory(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(96,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

int dsp0VideocodecCreateDecoder(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	/* With zero optional size, the packer forwards this channel unchanged. */
	buffers.optionalBufferAddress = SceCodecEngineWrapperForDriver_6A1184EA(0);
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(97,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

int dsp0VideocodecDeleteDecoder(void *command)
{
	Dsp0VideocodecRpcBuffers buffers;
	int ret;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(98,
			(const unsigned int *)&buffers, dsp0VideocodecPackCommand,
			dsp0VideocodecFinishCommand);
	if (ret & 0x10000000)
		ret = (int)0x80620280;
	else if (ret < 0)
		ret = (int)0x806202c0;
	else if (ret != 0)
		ret &= 0x10000000;
	return ret;
}

int dsp0VideocodecQueryDecoderMemory(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 0,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecInitializeDecoder(struct Dsp0VideocodecDecoderCommand *command,
				   struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallDecoderCommand(command, 1,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecQueryDecoderContext(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 3,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecInitializeDecoderContext(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 4,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecDecodePicture(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 5,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecStopDecoder(struct Dsp0VideocodecDecoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallDecoderCommand(command, 6,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecTerminateDecoder(struct Dsp0VideocodecDecoderCommand *command,
				  struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 256;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallDecoderCommand(command, 7,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecQueryEncoderMemory(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 16,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecInitializeEncoder(struct Dsp0VideocodecEncoderCommand *command,
				   struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallEncoderCommand(command, 17,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecEncodePicture(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 18,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecStopEncoder(struct Dsp0VideocodecEncoderCommand *command)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.bufferAddress = (uintptr_t)command;
	buffers.optionalBufferAddress = 0;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 0;
	return dsp0VideocodecCallEncoderCommand(command, 19,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecTerminateEncoder(struct Dsp0VideocodecEncoderCommand *command,
				  struct Dsp0VideocodecLibraryCommand *library)
{
	Dsp0VideocodecRpcBuffers buffers;

	buffers.optionalBufferAddress = (uintptr_t)library;
	buffers.bufferSize = 512;
	buffers.optionalBufferSize = 256;
	buffers.bufferAddress = (uintptr_t)command;
	return dsp0VideocodecCallEncoderCommand(command, 20,
					      (const unsigned int *)&buffers);
}

int dsp0VideocodecCalculateLibraryMemory(const Dsp0VideocodecInitParam *param,
				       SceSize *memorySize, SceSize *librarySize)
{
	Dsp0VideocodecLibraryCommand *command;
	unsigned int pictureSize, decoderMemory;
	int ret;

	command = dsp0VideocodecAllocMemory(0, sizeof(*command));
	if (command == NULL)
		return (int)0x80620203;
	memset(command, 0, sizeof(*command));
	command->commandSize = sizeof(*command);
	command->numOfDecoders = param->numOfStreams;
	command->horizontal = param->horizontal;
	command->numOfEncoders = 1;
	command->coprocessorQueryData[0] = 0;
	command->coprocessorQueryData[1] = 0;
	ret = dsp0VideocodecQueryMemory(command);
	if (ret < 0) {
		ret = (int)0x8062020d;
	} else {
		*librarySize = (command->workSize + 255) & ~255u;
		pictureSize = (((3 * param->vertical * param->horizontal) >> 2) +
			       255) & 0x7fffff00;
		decoderMemory = pictureSize < 81920 ? 92544 : pictureSize + 10624;
		*memorySize = (((command->workSize + 255) & ~255u) +
			1536 * command->numOfEncoders + 262911 +
			((188 * command->numOfEncoders + 255) & ~255u) +
			((116 * command->numOfDecoders + 255) & ~255u) +
			decoderMemory * (2 * command->numOfDecoders)) >> 18 << 18;
	}
	SceCodecEngineWrapperForDriver_3C5E9CC3(0, (uintptr_t)command, sizeof(*command));
	return ret;
}

int initializeDsp0Videocodec(void)
{
	int ret = ksceKernelCreateProcessLocalStorage("SceDsp0Videodec",
						    sizeof(Dsp0VideocodecGlobal));

	if (ret >= 0) {
		void *storage = NULL;

		dsp0VideocodecLocalStorageId = ret;
		ret = ksceKernelGetProcessLocalStorageAddrForPid(0, ret, &storage, 1);
		if (ret >= 0) {
			Dsp0VideocodecGlobal *global;
			struct ThreadmgrFastMutex *mutex;

#if FW_DEBUG
			SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecCreateStorage,
					 __builtin_return_address(0));
#endif
			global = storage;
			mutex = &global->mutex;
			memset(storage, 0, sizeof(*global));
			ret = ksceKernelInitializeFastMutex((SceKernelFastMutex *)mutex,
						   "SceDsp0Vcodec", 2, NULL);
			if (ret < 0) {
				SceKernelIntrStatus intr;

				if (global->pMutex != NULL) {
					ksceKernelFinalizeFastMutex(
						(SceKernelFastMutex *)global->pMutex);
					global->pMutex = NULL;
				}
				intr = SceCpuForDriver_D32ACE9E(
					&global->statusLock);
				global->status = 0;
				sceKernelSpinlockLowUnlockCpuResumeIntr(
					&global->statusLock, intr);
			} else {
				global->pMutex = mutex;
				global->statusLock = 0;
				global->status = 0;
			}
		}
	}
	return ret;
}

int _sceDsp0VideocodecProcessCreateEntry(SceUID pid)
{
	void *storage = NULL;
	int ret = ksceKernelGetProcessLocalStorageAddrForPid(pid,
					 dsp0VideocodecLocalStorageId, &storage, 1);

	if (ret >= 0) {
		Dsp0VideocodecGlobal *global;
		struct ThreadmgrFastMutex *mutex;

#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecCreateStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		mutex = &global->mutex;
		memset(storage, 0, sizeof(*global));
		ret = ksceKernelInitializeFastMutex((SceKernelFastMutex *)mutex,
						   "SceDsp0Vcodec", 2, NULL);
		if (ret < 0) {
			SceKernelIntrStatus intr;

			if (global->pMutex != NULL) {
				ksceKernelFinalizeFastMutex((SceKernelFastMutex *)global->pMutex);
				global->pMutex = NULL;
			}
			intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
			global->status = 0;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		} else {
			global->pMutex = mutex;
			global->statusLock = 0;
			global->status = 0;
		}
	}
	return ret;
}

int _sceDsp0VideocodecProcessExitEntry(SceUID pid)
{
	void *storage = NULL;
	int ret = ksceKernelGetProcessLocalStorageAddrForPid(pid,
					 dsp0VideocodecLocalStorageId, &storage, 0);

	if (ret >= 0) {
		Dsp0VideocodecGlobal *global;
		SceKernelIntrStatus intr;

#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecExitStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		if (global->pMutex != NULL) {
			ksceKernelFinalizeFastMutex((SceKernelFastMutex *)global->pMutex);
			global->pMutex = NULL;
		}
		intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
		global->status = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		ret = 0;
	}
	return ret;
}

int dsp0VideocodecIsProcessActive(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;

	if (pid != 0) {
		if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	return status == 1;
}

int dsp0VideocodecIsProcessSuspended(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;
	int suspended = 1;

	if (pid != 0) {
		if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	if (status != 0) {
		storage = NULL;
		if (pid != 0) {
			if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
					dsp0VideocodecLocalStorageId, &storage, 0) < 0)
				storage = NULL;
		} else {
			storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
		}
#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
		status = global->status;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		suspended = status == 3;
	}
	return suspended;
}

int dsp0VideocodecProcessResume(SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int status;

	if (pid != 0) {
		if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
	status = global->status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	if (status == 3) {
		storage = NULL;
		if (pid != 0) {
			if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
					dsp0VideocodecLocalStorageId, &storage, 0) < 0)
				storage = NULL;
		} else {
			storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
		}
#if FW_DEBUG
		SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
				 __builtin_return_address(0));
#endif
		global = storage;
		intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
		global->status = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
	}
	return 0;
}

int dsp0VideocodecQueryLibraryMemory(const Dsp0VideocodecInitParam *param,
				   SceSize *memorySize)
{
	Dsp0VideocodecGlobal *global;
	SceSize requiredSize;
	SceSize librarySize;
	int ret;

	global = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
#if FW_DEBUG
	SceDebugForDriver_35A35322(global != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	if (param == NULL || memorySize == NULL)
		return (int)0x80620209;
	if (global->initialized != 0)
		return (int)0x80620208;
	if (param->horizontal - 64 > 1216 || param->vertical - 64 > 656 ||
	    ((param->horizontal | param->vertical) & 15) != 0 ||
	    param->numOfRefFrames - 1 > 15 || param->numOfStreams - 1 > 3)
		return (int)0x80620201;
	ret = _lockProcessSuspendCore();
	if (ret >= 0) {
		ksceKernelLockFastMutex((SceKernelFastMutex *)global->pMutex);
		ret = dsp0VideocodecCalculateLibraryMemory(param, &requiredSize, &librarySize);
		if (ret >= 0)
			*memorySize = requiredSize;
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
		_unlockProcessSuspendCore();
	}
	return ret;
}

int dsp0VideocodecTermLibraryForProcess(unsigned int codecType, SceUID pid)
{
	void *storage = NULL;
	Dsp0VideocodecGlobal *global;
	SceKernelIntrStatus intr;
	int ret;

	if (pid != 0) {
		if (ksceKernelGetProcessLocalStorageAddrForPid(pid,
				dsp0VideocodecLocalStorageId, &storage, 0) < 0)
			storage = NULL;
	} else {
		storage = ksceKernelGetProcessLocalStorageAddr(dsp0VideocodecLocalStorageId);
	}
#if FW_DEBUG
	SceDebugForDriver_35A35322(storage != NULL, &dsp0VideocodecGlobalStorage,
			 __builtin_return_address(0));
#endif
	global = storage;
	if (codecType != 1)
		return (int)0x80620201;
	if (global->initialized != 1)
		return (int)0x8062020c;
	ret = _lockProcessSuspendCore();
	if (ret >= 0) {
		ksceKernelLockFastMutex((SceKernelFastMutex *)global->pMutex);
		ret = dsp0VideocodecDeleteDecoder(global->library);
		if (ret < 0)
			ret = (int)0x8062020d;
		dsp0VideocodecReleaseLibraryBuffers(global);
		if (global->library != NULL) {
			if (global->library->contextAddress != 0)
				global->library->contextAddress = 0;
			global->library = NULL;
		}
		if (global->workMemory != NULL)
			global->workMemory = NULL;
		global->initialized = 0;
		intr = SceCpuForDriver_D32ACE9E(&global->statusLock);
		global->status = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&global->statusLock, intr);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)global->pMutex);
		_unlockProcessSuspendCore();
	}
	return ret;
}
