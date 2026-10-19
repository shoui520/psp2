/* SceJpeg: no source-file name survives in this module's debug records. */
#include "../audiodec/audiodec_internal.h"
#include "jpeg_internal.h"
#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <string.h>
#include <intrmgr/syscall.h>
#include <lowio/iftu.h>

static JpegResident jpegResident;

/* Nine-bit fixed-point JPEG YCbCr to RGB coefficients. */
static SceIftuCscParams jpegYCbCrToRgb[2] = { {
	.post_add_0 = 0,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 512, 0, 717 },
		{ 512, 0xf50, 0xe93 },
		{ 512, 907, 0 },
	},
}, {
	.post_add_0 = 64,
	.post_add_1_2 = 514,
	.post_clamp_max_0 = 1023,
	.post_clamp_min_0 = 0,
	.post_clamp_max_1_2 = 0,
	.post_clamp_min_1_2 = 0,
	.ctm = {
		{ 596, 0, 817 },
		{ 596, 0xf38, 0xe60 },
		{ 596, 1032, 0 },
	},
} };

void jpegFinishSplitDecoderCommand(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

void jpegFinishDecodeCommand(VeneziaRpcThunk *thunk)
{
	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
}

void jpegFinishQueryCommand(VeneziaRpcThunk *thunk)
{
	JpegQueryCommand *command = thunk->memories[0].virtualBase;

	sceCodecEngineDsp0ConvertPhysicalToVirtual(thunk);
	sceCodecEngineDsp0MemcpyChain(thunk);
	sceCodecEngineDsp0FreeCommunicationMemories(thunk);
	command->pOutputInfo = thunk->memories[1].virtualBase;
}

int jpegPackSplitDecoderCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			       unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	packed[0] = input[0];
	option.flags = 1;
	option.allocationSize = 96;
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)input[1], 96, &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	ret = sceCodecEngineDsp0MemcpyChain(thunk);
	packed[1] = thunk->memories[0].physicalBase;
	packed[2] = input[2];
	return ret;
}

int jpegPackDecodeCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			  unsigned int *packed)
{
	VeneziaMemoryOption option;
	int ret;

	packed[0] = input[0];
	option.flags = 1;
	option.allocationSize = 44;
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, (void *)input[1], 44, &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	ret = sceCodecEngineDsp0MemcpyChain(thunk);
	packed[1] = thunk->memories[0].physicalBase;
	return ret;
}

int jpegPackQueryCommand(VeneziaRpcThunk *thunk, const unsigned int *input,
			 unsigned int *packed)
{
	VeneziaMemoryOption option;
	JpegQueryCommand *command;
	int ret;

	packed[0] = input[0];
	command = (JpegQueryCommand *)input[1];
	option.flags = 1;
	option.allocationSize = sizeof(*command);
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, command, sizeof(*command), &option);
	option.allocationSize = sizeof(*command->pOutputInfo);
	sceCodecEngineDsp0ConvertVirtualToPhysical(thunk, command->pOutputInfo,
					     sizeof(*command->pOutputInfo), &option);
	sceCodecEngineDsp0AllocCommunicationMemories(thunk);
	command->pOutputInfo = (SceJpegOutputInfo *)thunk->memories[1].physicalBase;
	ret = sceCodecEngineDsp0MemcpyChain(thunk);
	packed[1] = thunk->memories[0].physicalBase;
	return ret;
}

int jpegLockDecodeBuffer(unsigned int *physicalAddress, const void *buffer,
			 SceSize size, unsigned int lockMode, int cacheMode,
			 unsigned int allowCached)
{
	unsigned int offset = (unsigned int)buffer & 255;
	SceSize alignedSize = (size + 255 + offset) & ~255U;
	unsigned int alignedAddress = (unsigned int)buffer & ~255U;
	SceKernelVARange range;
	SceKernelPAVector vector;
	SceKernelPARange physicalRange;
	unsigned int address;
	int ret;

	ret = sceKernelLockRangeWithMode(lockMode, (void *)alignedAddress, alignedSize);
	if (ret < 0)
		return ret;
	range.addr = alignedAddress;
	range.size = alignedSize;
	vector.size = sizeof(vector);
	vector.capacity = 1;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = &physicalRange;
	if (sceKernelVARangeToPAVector(&range, &vector) < 0 || vector.total > 1)
		goto invalid;
	address = physicalRange.addr;
	if (address == 0)
		goto invalid;
	if (address >= 0x20000000 && alignedSize + address <= 0x28000000)
		goto success;
	if (!(allowCached & 1))
		goto invalid;
	if (cacheMode == 2) {
		if (alignedSize >= 1024) {
			sceKernelDcacheCleanInvalidateRange((void *)alignedAddress, 256);
			sceKernelDcacheInvalidateRange((void *)(alignedAddress + 256),
						alignedSize - 512);
			sceKernelDcacheCleanInvalidateRange(
				(void *)(alignedAddress + alignedSize - 256), 256);
			goto success;
		}
	} else if (cacheMode != 3) {
		if (cacheMode == 1)
			sceKernelDcacheCleanRange((void *)alignedAddress, alignedSize);
		goto success;
	}
	sceKernelDcacheCleanInvalidateRange((void *)alignedAddress, alignedSize);
success:
	*physicalAddress = offset + address;
	return 0;
invalid:
	SceSysmemForDriver_22CBE925(lockMode, (void *)alignedAddress, alignedSize);
	return 0x80650053;
}

int jpegLockEncodeBuffer(unsigned int *physicalAddress, const void *buffer,
			 SceSize size, unsigned int lockMode, int cacheMode,
			 unsigned int allowCached)
{
	unsigned int offset = (unsigned int)buffer & 255;
	SceSize alignedSize = (size + 255 + offset) & ~255U;
	unsigned int alignedAddress = (unsigned int)buffer & ~255U;
	SceKernelVARange range;
	SceKernelPAVector vector;
	SceKernelPARange physicalRange;
	unsigned int address;
	int ret;

	ret = sceKernelLockRangeWithMode(lockMode, (void *)alignedAddress, alignedSize);
	if (ret < 0)
		return ret;
	range.addr = alignedAddress;
	range.size = alignedSize;
	vector = (SceKernelPAVector){
		.size = sizeof(vector),
		.capacity = 1,
		.nDataInVector = 0,
		.total = 0,
		.pRanges = &physicalRange,
	};
	if (sceKernelVARangeToPAVector(&range, &vector) < 0 || vector.total > 1)
		goto invalid;
	address = physicalRange.addr;
	if (address == 0)
		goto invalid;
	if (address >= 0x20000000 && alignedSize + address <= 0x28000000)
		goto success;
	if (!(allowCached & 1))
		goto invalid;
	if (cacheMode == 2) {
		if (alignedSize >= 1024) {
			unsigned int firstAddress = alignedAddress;

			alignedSize -= 512;
			alignedAddress += 256;
			sceKernelDcacheCleanInvalidateRange((void *)firstAddress, 256);
			sceKernelDcacheInvalidateRange((void *)alignedAddress, alignedSize);
			sceKernelDcacheCleanInvalidateRange(
				(void *)(alignedSize + alignedAddress), 256);
			goto success;
		}
	} else if (cacheMode != 3) {
		if (cacheMode == 1)
			sceKernelDcacheCleanRange((void *)alignedAddress, alignedSize);
		goto success;
	}
	sceKernelDcacheCleanInvalidateRange((void *)alignedAddress, alignedSize);
success:
	*physicalAddress = offset + address;
	return 0;
invalid:
	SceSysmemForDriver_22CBE925(lockMode, (void *)alignedAddress, alignedSize);
	return 0x80650206;
}

int initializeJpeg(void)
{
	int key;

	key = sceKernelRegisterKPLS("SceJpeg", 56);
	if (key < 0)
		return 2;
	jpegResident.processStorageKey = key;
	if (sceKernelInitializeFastMutex((SceKernelFastMutex *)&jpegResident.mutex,
					  "SceJpeg", 0x8000, NULL) < 0)
		return 2;
	jpegResident.channel = sceCodecEngineDsp0GetThreadExecGroup(0);
	return 0;
}

int jpegProcessCreateEntry(SceUID pid, SceUInt32 type)
{
	return 0;
}

int jpegProcessExitEntry(SceUID pid, SceUInt32 type)
{
	JpegProcess *process;
	SceJpegSplitDecodeCtrl command;
	unsigned int input[4];
	unsigned int context;
	int ret;

	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		return ret;
	ret = SceProcessmgrForDriver_AF80F39C(pid, jpegResident.processStorageKey,
						     (void **)&process, 0);
	if (ret < 0) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		return ret;
	}
	if (process != NULL) {
		if (process->state == 1)
			process->state = 2;
		context = process->decoderContexts[0];
		input[0] = jpegResident.channel;
		input[1] = (unsigned int)&command;
		input[2] = 0;
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[1];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[2];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[3];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[4];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[5];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[6];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		context = process->decoderContexts[7];
		if (context != 0) {
			command.internalData[1] = context;
			SceCodecEngineWrapperForDriver_391BD4FD(451, input,
				jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
		}
		if (process->pWorkBuffer != NULL)
			SceCodecEngineWrapperForDriver_3A79A935(1, process->pWorkBuffer,
							     process->maxSplitDecoder << 13);
		memset(process, 0, sizeof(*process));
	}
	return sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
}

/* Name unknown: user-facing MJPEG initialization behind the SDK wrapper. */
int SceAvcodecForDriver_48247A20(unsigned int maxSplitDecoder)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (maxSplitDecoder > 8) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650062;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return ret;
	}
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL) {
		ret = 0x80650042;
		goto error;
	}
	if (process->state != 0) {
		ret = 0x80650039;
		goto error;
	}
	if (maxSplitDecoder != 0) {
		process->pWorkBuffer = SceCodecEngineWrapperForDriver_91535820(1,
								maxSplitDecoder << 13);
		if (process->pWorkBuffer == NULL) {
			ret = 0x80650042;
			goto error;
		}
	}
	process->maxSplitDecoder = maxSplitDecoder;
	process->allocatedDecoders = 0;
	process->busyDecoders = 0;
	process->option = 0;
	process->state = 1;
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
error:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: parameterized user-facing MJPEG initialization. */
int SceAvcodecForDriver_6F1CF77F(const SceJpegMJpegInitParam *pInitParam)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (pInitParam == NULL) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650010;
	}
	if (pInitParam->size != sizeof(*pInitParam) || (unsigned int)pInitParam->option > 1) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650067;
	}
	if ((unsigned int)pInitParam->maxSplitDecoder > 8) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650062;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return ret;
	}
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL) {
		ret = 0x80650042;
		goto error;
	}
	if (process->state != 0) {
		ret = 0x80650039;
		goto error;
	}
	if (pInitParam->maxSplitDecoder > 0) {
		process->pWorkBuffer = SceCodecEngineWrapperForDriver_91535820(1,
								pInitParam->maxSplitDecoder << 13);
		if (process->pWorkBuffer == NULL) {
			ret = 0x80650042;
			goto error;
		}
	}
	process->maxSplitDecoder = pInitParam->maxSplitDecoder;
	process->option = pInitParam->option;
	process->allocatedDecoders = 0;
	process->busyDecoders = 0;
	process->state = 1;
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
error:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: user-facing MJPEG termination behind the SDK wrapper. */
int SceAvcodecForDriver_04080710(void)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1 || process->allocatedDecoders != 0) {
		ret = 0x80650039;
		goto unlock;
	}
	if (process->pWorkBuffer != NULL) {
		ret = SceCodecEngineWrapperForDriver_3A79A935(1, process->pWorkBuffer,
							   process->maxSplitDecoder << 13);
		if (ret < 0)
			goto unlock;
	}
	memset(process, 0, sizeof(*process));
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	goto finish;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegDecodeMJpeg's user-facing driver implementation. */
int SceAvcodecForDriver_80F63723(const unsigned char *pJpeg, SceSize isize,
			      void *pRGBA, SceSize osize, int decodeMode,
			      void *pTempBuffer, SceSize tempBufferSize,
			      void *pCoefBuffer, SceSize coefBufferSize)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	SceKernelVARange range;
	SceKernelPAVector vector;
	SceKernelPARange physicalRange;
	JpegDecodeCommand command;
	SceIftuConvParams conversion;
	SceIftuFrameBuf destination;
	SceIftuPlaneState source;
	unsigned int input[4];
	unsigned int sampling = decodeMode & 3;
	unsigned int samplingFlags;
	int downscale = decodeMode >> 3;
	unsigned int jpegBase = (unsigned int)pJpeg & ~255U;
	unsigned int jpegOffset = (unsigned int)pJpeg - jpegBase;
	SceSize jpegSize = (isize + 255 + jpegOffset) & ~255U;
	unsigned int jpegPhysical, tempPhysical, coefPhysical;
	unsigned int sourcePhysical, destinationPhysical;
	unsigned int colorSpace, convertedColorSpace, planeSampling;
	unsigned int width, height, rgbaSize, lumaSize, chromaSize, ycbcrSize;
	int coefLocked, decoded, ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (sampling > 1) {
		ret = 0x80650060;
		goto finish;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		ret = 0x80650039;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		goto finish;
	}
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	if (pTempBuffer == NULL || pJpeg == NULL ||
	    (((unsigned int)pCoefBuffer | (unsigned int)pTempBuffer) & 255)) {
		ret = 0x80650010;
		goto finish;
	}
	if (tempBufferSize == 0 || isize == 0) {
		ret = 0x80650041;
		goto finish;
	}
	samplingFlags = sampling <= 1;
	if (sampling - 1 <= 1)
		samplingFlags |= 2;
	if (decodeMode & 12) {
		ret = 0x80650060;
		goto finish;
	}
	if (downscale == 0)
		downscale = 1;
	else if (((downscale - 1) & downscale) || (unsigned int)(downscale - 1) > 7) {
		ret = 0x80650060;
		goto finish;
	}
	ret = sceKernelLockRangeWithMode(16, (void *)jpegBase, jpegSize);
	if (ret < 0)
		goto finish;
	range.addr = jpegBase;
	range.size = jpegSize;
	vector.size = sizeof(vector);
	vector.capacity = 1;
	vector.nDataInVector = 0;
	vector.total = 0;
	vector.pRanges = &physicalRange;
	if (sceKernelVARangeToPAVector(&range, &vector) < 0 || vector.total > 1 ||
	    physicalRange.addr == 0)
		goto invalidPhysical;
	if (physicalRange.addr < 0x20000000 || jpegSize + physicalRange.addr > 0x28000000) {
		if (!(process->option & 1))
			goto invalidPhysical;
		sceKernelDcacheCleanRange((void *)jpegBase, jpegSize);
	}
	jpegPhysical = physicalRange.addr + jpegOffset;
	ret = jpegLockDecodeBuffer(&tempPhysical, pTempBuffer, tempBufferSize,
				   32, 2, process->option);
	if (ret < 0)
		goto unlockJpeg;
	if (pCoefBuffer == NULL || coefBufferSize == 0) {
		coefLocked = 0;
		coefPhysical = 0;
		coefBufferSize = 0;
	} else {
		coefLocked = 1;
		ret = jpegLockDecodeBuffer(&coefPhysical, pCoefBuffer, coefBufferSize,
					   48, 2, process->option);
		if (ret < 0)
			goto unlockTemp;
	}
	command.pJpeg = (const unsigned char *)jpegPhysical;
	command.isize = isize;
	command.pYCbCr = (unsigned char *)tempPhysical;
	command.osize = tempBufferSize;
	command.samplingFlags = samplingFlags;
	command.downscale = downscale;
	command.colorSpace = 0;
	command.pCoefBuffer = (void *)coefPhysical;
	command.coefBufferSize = coefBufferSize;
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)&command;
	decoded = SceCodecEngineWrapperForDriver_391BD4FD(448, input,
						jpegPackDecodeCommand, jpegFinishDecodeCommand);
	if (decoded >= 0)
		colorSpace = command.colorSpace;
	if (coefLocked)
		SceSysmemForDriver_22CBE925(48, (void *)((unsigned int)pCoefBuffer & ~255U),
			(coefBufferSize + 255 + ((unsigned int)pCoefBuffer & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pTempBuffer & ~255U),
		(tempBufferSize + 255 + ((unsigned int)pTempBuffer & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(16, (void *)jpegBase, jpegSize);
	if (decoded < 0) {
		ret = decoded;
		goto finish;
	}
	planeSampling = colorSpace & 3;
	convertedColorSpace = planeSampling;
	if (planeSampling) {
		convertedColorSpace = colorSpace & 12;
		if (convertedColorSpace) {
			convertedColorSpace = colorSpace & 240;
			if (convertedColorSpace) {
				convertedColorSpace = (1U << (planeSampling - 1)) |
					((unsigned char)colorSpace >> 4 << 16) |
					(1U << (((colorSpace >> 2) & 3) - 1) << 8);
				planeSampling = (unsigned short)convertedColorSpace;
			} else {
				planeSampling = convertedColorSpace;
			}
		} else {
			planeSampling = convertedColorSpace;
		}
	}
	width = ((unsigned int)decoded >> 16) + 15;
	width &= ~15U;
	height = ((unsigned short)decoded + 7) & ~7U;
	rgbaSize = 4 * width * height;
	if (rgbaSize > osize) {
		ret = 0x80650041;
		goto finish;
	}
	if (pRGBA == NULL || ((unsigned int)pRGBA & 15) || ((unsigned int)pTempBuffer & 255)) {
		ret = 0x80650010;
		goto finish;
	}
	if (height - 64 > 0x400 || width - 64 > 0x7b0) {
		ret = 0x80650020;
		goto finish;
	}
	if (planeSampling - 513 > 1) {
		ret = 0x80650050;
		goto finish;
	}
	lumaSize = width * height;
	chromaSize = (((width >> 1) + 15) & ~15U) *
		(height >> ((convertedColorSpace & 3) - 1));
	ycbcrSize = lumaSize + 2 * chromaSize;
	ret = jpegLockDecodeBuffer(&sourcePhysical, pTempBuffer, ycbcrSize,
				   16, 0, process->option);
	if (ret < 0)
		goto finish;
	ret = jpegLockDecodeBuffer(&destinationPhysical, pRGBA, rgbaSize,
				   32, 2, process->option);
	if (ret < 0)
		goto unlockSource;
	source.fb.pixelformat = planeSampling == 514 ?
		SCE_IFTU_PIXELFORMAT_YUV420 : SCE_IFTU_PIXELFORMAT_YUV422;
	source.fb.width = width;
	source.fb.height = height;
	source.fb.leftover_stride = 0;
	source.fb.leftover_align = (width >> 1) & 15;
	source.fb.paddr0 = sourcePhysical;
	source.fb.paddr1 = lumaSize + sourcePhysical;
	source.fb.paddr2 = chromaSize + lumaSize + sourcePhysical;
	source.scaleX = 0x10000;
	source.scaleY = 0x10000;
	source.dst_x = 0;
	source.dst_y = 0;
	source.sampleX = 0;
	source.sampleY = 0;
	source.vtop_padding = 0;
	source.vbot_padding = 0;
	source.hleft_padding = 0;
	source.hright_padding = 0;
	conversion.size = sizeof(conversion);
	conversion.filterMode = 1;
	conversion.csc_params1 = NULL;
	conversion.csc_params2 = &jpegYCbCrToRgb[0];
	conversion.csc_control = 0;
	conversion.backgroundColor[0] = 0;
	conversion.backgroundColor[1] = 514;
	conversion.backgroundColor[2] = 514;
	conversion.alpha = 255;
	conversion.fieldMode = 0;
	destination.pixelformat = SCE_IFTU_PIXELFORMAT_BGRX8888;
	destination.width = width;
	destination.height = height;
	destination.leftover_stride = 0;
	destination.leftover_align = 0;
	destination.paddr0 = destinationPhysical;
	destination.paddr1 = 0;
	destination.paddr2 = 0;
	ret = SceIftuForDriver_67E37EFC(&destination, &source, &conversion);
	if ((process->option & 1) && ret >= 0)
		sceKernelDcacheInvalidateRange((void *)((unsigned int)pRGBA & ~255U),
			(rgbaSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pRGBA & ~255U),
		(rgbaSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pTempBuffer & ~255U),
		(ycbcrSize + 255 + ((unsigned int)pTempBuffer & 255)) & ~255U);
	if (ret >= 0)
		ret = decoded;
	goto finish;
unlockSource:
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pTempBuffer & ~255U),
		(ycbcrSize + 255 + ((unsigned int)pTempBuffer & 255)) & ~255U);
	goto finish;
invalidPhysical:
	ret = 0x80650053;
	goto unlockJpeg;
unlockTemp:
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pTempBuffer & ~255U),
		(tempBufferSize + 255 + ((unsigned int)pTempBuffer & 255)) & ~255U);
unlockJpeg:
	SceSysmemForDriver_22CBE925(16, (void *)jpegBase, jpegSize);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegDecodeMJpegYCbCr's user-facing driver. */
int SceAvcodecForDriver_2A0AF392(const unsigned char *pJpeg, SceSize isize,
			      unsigned char *pYCbCr, SceSize osize, int decodeMode,
			      void *pCoefBuffer, SceSize coefBufferSize)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	JpegDecodeCommand command;
	unsigned int input[4];
	unsigned int jpegPhysical, outputPhysical, coefPhysical;
	unsigned int outputBase = (unsigned int)pYCbCr & ~255U;
	SceSize outputSize = (osize + 255 + ((unsigned int)pYCbCr & 255)) & ~255U;
	unsigned int jpegBase = (unsigned int)pJpeg & ~255U;
	SceSize jpegSize = (isize + 255 + ((unsigned int)pJpeg & 255)) & ~255U;
	unsigned int sampling = decodeMode & 3;
	unsigned int samplingFlags;
	int downscale = decodeMode >> 3;
	int coefLocked, ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		ret = 0x80650039;
		goto finish;
	}
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	if (pYCbCr == NULL || pJpeg == NULL ||
	    (((unsigned int)pYCbCr | (unsigned int)pCoefBuffer) & 255)) {
		ret = 0x80650010;
		goto finish;
	}
	if (osize == 0 || isize == 0) {
		ret = 0x80650041;
		goto finish;
	}
	samplingFlags = sampling <= 1;
	if (sampling - 1 <= 1)
		samplingFlags |= 2;
	if (decodeMode & 12) {
		ret = 0x80650060;
		goto finish;
	}
	if (downscale == 0)
		downscale = 1;
	else if (((downscale - 1) & downscale) || (unsigned int)(downscale - 1) > 7) {
		ret = 0x80650060;
		goto finish;
	}
	ret = jpegLockDecodeBuffer(&jpegPhysical, pJpeg, isize, 16, 1, process->option);
	if (ret < 0)
		goto finish;
	ret = jpegLockDecodeBuffer(&outputPhysical, pYCbCr, osize, 32, 2, process->option);
	if (ret < 0)
		goto unlockJpeg;
	if (pCoefBuffer == NULL || coefBufferSize == 0) {
		coefLocked = 0;
		coefPhysical = 0;
		coefBufferSize = 0;
	} else {
		coefLocked = 1;
		ret = jpegLockDecodeBuffer(&coefPhysical, pCoefBuffer, coefBufferSize,
					   48, 2, process->option);
		if (ret < 0)
			goto unlockOutput;
	}
	command.pJpeg = (const unsigned char *)jpegPhysical;
	command.isize = isize;
	command.pYCbCr = (unsigned char *)outputPhysical;
	command.osize = osize;
	command.samplingFlags = samplingFlags;
	command.downscale = downscale;
	command.colorSpace = 0;
	command.pCoefBuffer = (void *)coefPhysical;
	command.coefBufferSize = coefBufferSize;
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)&command;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(448, input,
						    jpegPackDecodeCommand, jpegFinishDecodeCommand);
	if (coefLocked)
		SceSysmemForDriver_22CBE925(48, (void *)((unsigned int)pCoefBuffer & ~255U),
			(coefBufferSize + 255 + ((unsigned int)pCoefBuffer & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(32, (void *)outputBase, outputSize);
	SceSysmemForDriver_22CBE925(16, (void *)jpegBase, jpegSize);
	if ((process->option & 1) && ret >= 0)
		sceKernelDcacheInvalidateRange((void *)outputBase, outputSize);
	goto finish;
unlockOutput:
	SceSysmemForDriver_22CBE925(32, (void *)outputBase, outputSize);
unlockJpeg:
	SceSysmemForDriver_22CBE925(16, (void *)jpegBase, jpegSize);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegMJpegCsc's user-facing driver. */
int SceAvcodecForDriver_EAAE7737(void *pRGBA, const unsigned char *pYCbCr,
			      int xysize, int iFrameWidth, int colorOption, int sampling)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	SceIftuConvParams conversion;
	SceIftuPlaneState source;
	SceIftuFrameBuf destination;
	SceIftuCscParams *matrix;
	unsigned int width, height, lumaSize, chromaSize, sourceSize, outputSize;
	unsigned int sourcePhysical, destinationPhysical;
	int format = colorOption & 15;
	int bytesShift, pixelFormat, ret;

	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || (bytesShift = process->state) != 1) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		ret = 0x80650039;
		goto finish;
	}
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	height = ((unsigned short)xysize + 7) & ~7U;
	width = (((unsigned int)xysize >> 16) + 15) & ~15U;
	if (pRGBA == NULL || ((unsigned int)pRGBA & 15) ||
	    pYCbCr == NULL || ((unsigned int)pYCbCr & 255)) {
		ret = 0x80650010;
		goto finish;
	}
	if (width - 64 > 0x7b0 || height - 64 > 0x400 ||
	    (unsigned int)iFrameWidth > 0x7f0 || width > (unsigned int)iFrameWidth)
		goto invalidSize;
	if (format == 3 || format > 4 || (unsigned int)(colorOption >> 4) > 1 ||
	    (unsigned int)(sampling - 513) > 1) {
		ret = 0x80650050;
		goto finish;
	}
	if (!(colorOption & 3)) {
		if (iFrameWidth & 3)
			goto invalidSize;
		pixelFormat = format == 0 ? SCE_IFTU_PIXELFORMAT_BGRX8888 :
			SCE_IFTU_PIXELFORMAT_RGBX8888;
		bytesShift = 2;
	} else {
		if (iFrameWidth & 7)
			goto invalidSize;
		pixelFormat = format == 1 ? SCE_IFTU_PIXELFORMAT_BGR565 :
			SCE_IFTU_PIXELFORMAT_BGRA5551;
	}
	lumaSize = width * height;
	chromaSize = (height >> ((sampling & 3) - 1)) * (((width >> 1) + 15) & ~15U);
	sourceSize = lumaSize + 2 * chromaSize;
	ret = jpegLockDecodeBuffer(&sourcePhysical, pYCbCr, sourceSize,
				   16, 0, process->option);
	if (ret < 0)
		goto finish;
	outputSize = (iFrameWidth * height) << bytesShift;
	ret = jpegLockDecodeBuffer(&destinationPhysical, pRGBA, outputSize,
				   32, 2, process->option);
	if (ret < 0)
		goto unlockSource;
	matrix = &jpegYCbCrToRgb[colorOption >> 4];
	conversion.backgroundColor[0] = matrix->post_add_0;
	conversion.backgroundColor[1] = matrix->post_add_1_2;
	conversion.backgroundColor[2] = conversion.backgroundColor[1];
	conversion.csc_params1 = NULL;
	conversion.csc_params2 = matrix;
	conversion.csc_control = 0;
	conversion.size = sizeof(conversion);
	conversion.filterMode = 1;
	conversion.alpha = 255;
	conversion.fieldMode = 0;
	source.fb.pixelformat = sampling == 514 ? SCE_IFTU_PIXELFORMAT_YUV420 :
		SCE_IFTU_PIXELFORMAT_YUV422;
	source.fb.width = width;
	source.fb.height = height;
	source.fb.leftover_stride = 0;
	source.fb.leftover_align = (width >> 1) & 15;
	source.fb.paddr0 = sourcePhysical;
	source.fb.paddr1 = lumaSize + sourcePhysical;
	source.fb.paddr2 = chromaSize + lumaSize + sourcePhysical;
	source.scaleX = 0x10000;
	source.scaleY = 0x10000;
	source.dst_x = 0;
	source.dst_y = 0;
	source.sampleX = 0;
	source.sampleY = 0;
	source.vtop_padding = 0;
	source.vbot_padding = 0;
	source.hleft_padding = 0;
	source.hright_padding = 0;
	destination.pixelformat = pixelFormat;
	destination.width = width;
	destination.height = height;
	destination.leftover_stride = (iFrameWidth - width) << bytesShift;
	destination.leftover_align = 0;
	destination.paddr0 = destinationPhysical;
	destination.paddr1 = 0;
	destination.paddr2 = 0;
	ret = SceIftuForDriver_67E37EFC(&destination, &source, &conversion);
	if ((process->option & 1) && ret >= 0)
		sceKernelDcacheInvalidateRange((void *)((unsigned int)pRGBA & ~255U),
			(outputSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pRGBA & ~255U),
		(outputSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
unlockSource:
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pYCbCr & ~255U),
		(sourceSize + 255 + ((unsigned int)pYCbCr & 255)) & ~255U);
finish:
	sceSyscallLeave(syscallState);
	return ret;
invalidSize:
	ret = 0x80650020;
	goto finish;
}

int jpegCscForUser(void *pRGBA, const unsigned char *pYCbCr, int xysize,
		   int iFrameWidth, int colorOption, int sampling)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	JpegCscCommand command;
	unsigned int input[4];
	unsigned int sourcePhysical, destinationPhysical;
	unsigned int width, height, sourceSize;
	unsigned long long outputSize;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		ret = 0x80650039;
		goto finish;
	}
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	if (pRGBA == NULL || ((unsigned int)pRGBA & 255) ||
	    pYCbCr == NULL || ((unsigned int)pYCbCr & 255)) {
		ret = 0x80650010;
		goto finish;
	}
	width = (unsigned int)xysize >> 16;
	height = (unsigned short)xysize;
	outputSize = (long long)iFrameWidth * (unsigned long long)(4 * height);
	if ((width & 7) || height == 0 || width <= 7 ||
	    ((unsigned int)iFrameWidth >> 28) || width > (unsigned int)iFrameWidth ||
	    outputSize != (unsigned int)outputSize) {
		ret = 0x80650020;
		goto finish;
	}
	if ((colorOption & 15) || ((unsigned int)colorOption >> 4) || sampling != 257) {
		ret = 0x80650050;
		goto finish;
	}
	sourceSize = 3 * height * width;
	ret = jpegLockDecodeBuffer(&sourcePhysical, pYCbCr, sourceSize,
				   16, 0, process->option);
	if (ret < 0)
		goto finish;
	ret = jpegLockDecodeBuffer(&destinationPhysical, pRGBA, outputSize,
				   32, 2, process->option);
	if (ret < 0)
		goto unlockSource;
	command.pYCbCr = (const unsigned char *)sourcePhysical;
	command.isize = sourceSize;
	command.pRGBA = (void *)destinationPhysical;
	command.osize = iFrameWidth * 4 * height;
	command.colorFormat = 5;
	command.xysize = xysize;
	command.iFrameWidth = iFrameWidth;
	command.conversionMode = 2;
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)&command;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(454, input,
						    jpegPackDecodeCommand, jpegFinishDecodeCommand);
	if ((process->option & 1) && ret >= 0)
		sceKernelDcacheInvalidateRange((void *)((unsigned int)pRGBA & ~255U),
			(outputSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
	SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pRGBA & ~255U),
		(outputSize + 255 + ((unsigned int)pRGBA & 255)) & ~255U);
unlockSource:
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pYCbCr & ~255U),
		(sourceSize + 255 + ((unsigned int)pYCbCr & 255)) & ~255U);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegGetOutputInfo's user-facing driver. */
int SceAvcodecForDriver_F305E3B3(const unsigned char *pJpeg, SceSize isize,
			      int outputFormat, int decodeMode, SceJpegOutputInfo *pOutputInfo)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	JpegQueryCommand command;
	unsigned int input[4];
	unsigned int jpegPhysical;
	unsigned int sampling = decodeMode & 3;
	unsigned int samplingFlags, colorSpace;
	int downscale = decodeMode >> 3;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (pOutputInfo == NULL || pJpeg == NULL) {
		ret = 0x80650010;
		goto finish;
	}
	if (isize == 0) {
		ret = 0x80650041;
		goto finish;
	}
	if ((unsigned int)(outputFormat + 1) > 1 && outputFormat != 4) {
		ret = 0x80650050;
		goto finish;
	}
	if (outputFormat != -1) {
		if (sampling > 1)
			goto invalidMode;
		outputFormat = 0;
	}
	samplingFlags = sampling <= 1;
	if (sampling - 1 <= 1)
		samplingFlags |= 2;
	if (decodeMode & 12)
		goto invalidMode;
	if (downscale == 0)
		downscale = 1;
	else if (((downscale - 1) & downscale) || (unsigned int)(downscale - 1) > 7)
		goto invalidMode;
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
		ret = 0x80650039;
		goto finish;
	}
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	ret = jpegLockDecodeBuffer(&jpegPhysical, pJpeg, isize, 16, 1, process->option);
	if (ret < 0)
		goto finish;
	memset(pOutputInfo, 0, sizeof(*pOutputInfo));
	command.pJpeg = (const unsigned char *)jpegPhysical;
	command.isize = isize;
	command.samplingFlags = samplingFlags;
	command.downscale = downscale;
	command.outputFormat = outputFormat;
	command.pOutputInfo = pOutputInfo;
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)&command;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(449, input,
						    jpegPackQueryCommand, jpegFinishQueryCommand);
	colorSpace = pOutputInfo->colorSpace;
	if (!(colorSpace & 3))
		colorSpace = 0;
	else if (!(colorSpace & 12))
		colorSpace &= 12;
	else if (!(colorSpace & 0xf0))
		colorSpace &= 0xf0;
	else
		colorSpace = (1U << ((colorSpace & 3) - 1)) |
			(((colorSpace >> 4) & 15) << 16) |
			((1U << (((colorSpace >> 2) & 3) - 1)) << 8);
	pOutputInfo->colorSpace = colorSpace;
	SceSysmemForDriver_22CBE925(16, (void *)((unsigned int)pJpeg & ~255U),
		(isize + 255 + ((unsigned int)pJpeg & 255)) & ~255U);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
invalidMode:
	ret = 0x80650060;
	goto finish;
}

/* Name unknown: SDK sceJpegCreateSplitDecoder's user-facing driver. */
int SceAvcodecForDriver_361238E2(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	unsigned int input[5];
	unsigned int allocated;
	int index, ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (pCtrl == NULL || ((unsigned int)pCtrl & 3)) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650010;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		ret = 0x80650039;
		goto unlock;
	}
	allocated = process->allocatedDecoders;
	index = 31 - __builtin_clz((allocated + 1) ^ allocated);
	if (process->maxSplitDecoder <= (unsigned int)index || index < 0) {
		ret = 0x80650062;
		goto unlock;
	}
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)pCtrl;
	input[2] = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(450, input,
					jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
	if (ret < 0)
		goto unlock;
	pCtrl->internalData[2] = index;
	process->decoderContexts[index] = pCtrl->internalData[1];
	process->allocatedDecoders |= 1U << index;
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	goto finish;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegDeleteSplitDecoder's user-facing driver. */
int SceAvcodecForDriver_864FC454(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	unsigned int input[5];
	unsigned int index;
	int ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (pCtrl == NULL || ((unsigned int)pCtrl << 30)) {
		_unlockProcessSuspendCore();
		sceSyscallLeave(syscallState);
		return 0x80650010;
	}
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		ret = 0x80650039;
		goto unlock;
	}
	index = pCtrl->internalData[2];
	if (!((process->allocatedDecoders >> index) & 1)) {
		ret = 0x80650010;
		goto unlock;
	}
	if ((process->busyDecoders >> index) & 1) {
		ret = 0x80650039;
		goto unlock;
	}
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)pCtrl;
	input[2] = 0;
	ret = SceCodecEngineWrapperForDriver_391BD4FD(451, input,
					jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
	if (ret < 0)
		goto unlock;
	pCtrl->internalData[2] = 8;
	process->decoderContexts[index] = 0;
	process->allocatedDecoders &= ~(1U << index);
	ret = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	goto finish;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
}

/* Name unknown: SDK sceJpegSplitDecodeMJpeg's user-facing driver. */
int SceAvcodecForDriver_B6882013(SceJpegSplitDecodeCtrl *pCtrl)
{
	SceUInt32 syscallState = sceSyscallEnter();
	JpegProcess *process;
	SceJpegSplitDecodeCtrl command;
	unsigned int input[4];
	unsigned int physicalAddress, index, mode, colorSpace;
	unsigned int streamBase, streamEnd, writeBase;
	int cacheMode, lockRet, ret;

	ret = SceCodecEngineWrapperForDriver_CB8703D8();
	if (ret < 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	if (pCtrl == NULL || ((unsigned int)pCtrl << 30))
		goto invalidBuffer;
	mode = pCtrl->internalData[0];
	if (mode > 3) {
		ret = 0x80650039;
		goto finish;
	}
	streamBase = (unsigned int)pCtrl->pStreamBuffer;
	if (streamBase == 0 || (streamBase & 255) ||
	    ((unsigned int)pCtrl->pOutputBuffer & 255) ||
	    ((unsigned int)pCtrl->pCoefBuffer & 255))
		goto invalidBuffer;
	if (pCtrl->streamBufferSize <= 255) {
		ret = 0x80650041;
		goto finish;
	}
	if (!pCtrl->isEndOfStream && (pCtrl->streamBufferSize & 255))
		goto invalidBuffer;
	writeBase = (unsigned int)pCtrl->pWriteBuffer;
	streamEnd = streamBase + pCtrl->streamBufferSize;
	if (writeBase != 0 && (streamBase > writeBase || writeBase > streamEnd ||
			       streamEnd < writeBase + pCtrl->writeBufferSize))
		goto invalidBuffer;
	ret = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret < 0)
		goto finish;
	process = SceProcessmgrForDriver_EE694840(jpegResident.processStorageKey);
	if (process == NULL || process->state != 1) {
		ret = 0x80650039;
		goto unlock;
	}
	index = pCtrl->internalData[2];
	if (!((process->allocatedDecoders >> index) & 1)) {
		ret = 0x80650010;
		goto unlock;
	}
	if ((process->busyDecoders >> index) & 1) {
		ret = 0x80650039;
		goto unlock;
	}
	process->busyDecoders |= 1U << index;
	lockRet = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (ret == 0)
		ret = lockRet;
	if (ret < 0)
		goto finish;
	memcpy(&command, pCtrl, sizeof(command));
	command.internalData[1] = process->decoderContexts[index];
	command.pStreamBuffer = NULL;
	command.pOutputBuffer = NULL;
	command.pCoefBuffer = NULL;
	ret = jpegLockDecodeBuffer(&physicalAddress, pCtrl->pStreamBuffer,
				   pCtrl->streamBufferSize, 48, 3, process->option);
	if (ret < 0)
		goto cleanup;
	command.pStreamBuffer = (unsigned char *)physicalAddress;
	command.pWriteBuffer = pCtrl->pWriteBuffer;
	if (command.pWriteBuffer != NULL)
		command.pWriteBuffer = (unsigned char *)
			((unsigned int)command.pWriteBuffer - streamBase + physicalAddress);
	cacheMode = mode == 2 ? 2 : 0;
	if (pCtrl->pOutputBuffer != NULL) {
		ret = jpegLockDecodeBuffer(&physicalAddress, pCtrl->pOutputBuffer,
			pCtrl->outputInfo.outputBufferSize, 32, cacheMode, process->option);
		if (ret < 0)
			goto cleanup;
		command.pOutputBuffer = (void *)physicalAddress;
	}
	if (pCtrl->pCoefBuffer != NULL) {
		ret = jpegLockDecodeBuffer(&physicalAddress, pCtrl->pCoefBuffer,
			pCtrl->outputInfo.coefBufferSize, 48, cacheMode, process->option);
		if (ret < 0)
			goto cleanup;
		command.pCoefBuffer = (void *)physicalAddress;
	}
	input[0] = jpegResident.channel;
	input[1] = (unsigned int)&command;
	input[2] = (unsigned int)process->pWorkBuffer + (index << 13);
	ret = SceCodecEngineWrapperForDriver_391BD4FD(452, input,
					jpegPackSplitDecoderCommand, jpegFinishSplitDecoderCommand);
	if (ret < 0) {
		if (ret == (int)0x80650063)
			pCtrl->outputInfo = command.outputInfo;
		else if (ret != (int)0x80650013 && ret != (int)0x80650016)
			goto copyResult;
		colorSpace = command.outputInfo.colorSpace;
		if (!(colorSpace & 3))
			colorSpace = 0;
		else if (!(colorSpace & 12))
			colorSpace &= 12;
		else if (!(colorSpace & 0xf0))
			colorSpace &= 0xf0;
		else
			colorSpace = (1U << ((colorSpace & 3) - 1)) |
				(((colorSpace >> 4) & 15) << 16) |
				((1U << (((colorSpace >> 2) & 3) - 1)) << 8);
		pCtrl->outputInfo.colorSpace = colorSpace;
	}
copyResult:
	pCtrl->pWriteBuffer = command.pWriteBuffer;
	if (pCtrl->pWriteBuffer != NULL)
		pCtrl->pWriteBuffer = (unsigned char *)((unsigned int)pCtrl->pWriteBuffer -
			(unsigned int)command.pStreamBuffer + (unsigned int)pCtrl->pStreamBuffer);
	pCtrl->writeBufferSize = command.writeBufferSize;
	pCtrl->internalData[0] = command.internalData[0];
	if (process->option & 1) {
		if (!pCtrl->isEndOfStream)
			sceKernelDcacheInvalidateRange((void *)((unsigned int)pCtrl->pStreamBuffer & ~255U),
				(pCtrl->streamBufferSize + 255 + ((unsigned int)pCtrl->pStreamBuffer & 255)) & ~255U);
		if (ret >= 0)
			sceKernelDcacheInvalidateRange((void *)((unsigned int)pCtrl->pOutputBuffer & ~255U),
				(pCtrl->outputInfo.outputBufferSize + 255 + ((unsigned int)pCtrl->pOutputBuffer & 255)) & ~255U);
	}
cleanup:
	if (command.pCoefBuffer != NULL)
		SceSysmemForDriver_22CBE925(48, (void *)((unsigned int)pCtrl->pCoefBuffer & ~255U),
			(pCtrl->outputInfo.coefBufferSize + 255 + ((unsigned int)pCtrl->pCoefBuffer & 255)) & ~255U);
	if (command.pOutputBuffer != NULL)
		SceSysmemForDriver_22CBE925(32, (void *)((unsigned int)pCtrl->pOutputBuffer & ~255U),
			(pCtrl->outputInfo.outputBufferSize + 255 + ((unsigned int)pCtrl->pOutputBuffer & 255)) & ~255U);
	if (command.pStreamBuffer != NULL)
		SceSysmemForDriver_22CBE925(48, (void *)((unsigned int)pCtrl->pStreamBuffer & ~255U),
			(pCtrl->streamBufferSize + 255 + ((unsigned int)pCtrl->pStreamBuffer & 255)) & ~255U);
	lockRet = lockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	if (lockRet == 0) {
		process->busyDecoders &= ~(1U << index);
		lockRet = sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
	}
	if ((ret >= 0) & ((unsigned int)lockRet >> 31))
		ret = lockRet;
	goto finish;
unlock:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&jpegResident.mutex);
finish:
	_unlockProcessSuspendCore();
	sceSyscallLeave(syscallState);
	return ret;
invalidBuffer:
	ret = 0x80650010;
	goto finish;
}
