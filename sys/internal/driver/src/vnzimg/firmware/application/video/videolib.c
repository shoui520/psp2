/*
 * The video library itself (0x82224c-0x8224d4): checks the ARM's sizes,
 * and keeps the codec service (the shared library, 0x8602xx-) in the first
 * 64 bytes of the work memory, the service's own memory after it.
 */
#include "video.h"

#define LIBRARY_SIZE			64
#define MAX_STREAMS			32
#define MAX_ENCODERS			1
#define MAX_CORES			32

#define VIDEO_ERROR_PARAM		((int32_t)0x80000000)
#define VIDEO_ERROR_ALIGNMENT		((int32_t)0x80000001)
#define VIDEO_ERROR_WORK_SIZE		((int32_t)0x80000002)
#define VIDEO_ERROR_STATE		((int32_t)0x10000002)


uint32_t videoLibraryQueryMemory(const VideoLibraryParam *param)
{
	int32_t numOfDecoders = param->numOfDecoders;
	int32_t numOfEncoders = param->numOfEncoders;
	int32_t commandSize = param->commandSize;
	uint32_t horizontal = param->horizontal;

	if (numOfDecoders < 0 || numOfDecoders > MAX_STREAMS)
		return 0;
	if (numOfEncoders < 0 || numOfEncoders > MAX_ENCODERS)
		return 0;
	if (numOfDecoders + numOfEncoders < 0 || numOfDecoders + numOfEncoders > MAX_STREAMS)
		return 0;
	if (commandSize < 32 || commandSize > 8192)
		return 0;
	if (horizontal < 16 || horizontal > 1920)
		return 0;
	if (commandSize & 31 || horizontal & 15)
		return 0;
	if ((uint32_t)commandSize * 16 < horizontal)
		return 0;
	return LIBRARY_SIZE + videoServiceQueryMemory(numOfDecoders, numOfEncoders, commandSize,
		horizontal);
}

int32_t videoLibraryInitialize(uint32_t *library, const VideoLibraryParam *param,
	uint32_t workAddress, uint32_t workSize)
{
	uint32_t numOfDecoders = param->numOfDecoders;
	uint32_t numOfEncoders = param->numOfEncoders;
	uint32_t commandSize = param->commandSize;
	uint32_t horizontal = param->horizontal;
	uint32_t core = param->core, core2 = param->core2;
	uint32_t service = workAddress + LIBRARY_SIZE;
	uint32_t serviceInfo[2] = { 0, 0 };
	volatile uint8_t *lock = 0;
	VideoLibrary *created;
	int32_t handle = -1;
	uint32_t size;

	*library = 0;
	if (((workAddress + 63) & ~63) != workAddress)
		return VIDEO_ERROR_ALIGNMENT;
	size = videoLibraryQueryMemory(param);
	if (!size || workSize < size)
		return VIDEO_ERROR_WORK_SIZE;
	if ((int32_t)core < 0 || core >= MAX_CORES)
		return VIDEO_ERROR_PARAM;
	if ((int32_t)core2 < 0 || core2 >= MAX_CORES)
		return VIDEO_ERROR_PARAM;
	videoServiceQueryMemory(numOfDecoders, numOfEncoders, commandSize, horizontal);

	if (param->resource) {
		videoServiceSetup(service, param->resource, 1 << core, 1 << core2, numOfDecoders,
			numOfEncoders, commandSize, horizontal, param->callback[0],
			param->callback[1], param->callback[2], avcencVersion());
		handle = videoServiceOpen(service, &serviceInfo[0], &serviceInfo[1],
			numOfDecoders, numOfEncoders, horizontal);
		lock = vnzLockAllocate();
		if (handle != -1 && lock) {
			created = (VideoLibrary *)vnzPublicOpenPrivate(workAddress, LIBRARY_SIZE);
			created->serviceMemory = service;
			created->callback[0] = param->callback[0];
			created->callback[1] = param->callback[1];
			created->service = handle;
			created->callback[2] = param->callback[2];
			created->serviceInfo[0] = serviceInfo[0];
			created->serviceInfo[1] = serviceInfo[1];
			created->coreMask = 1 << core;
			created->horizontal = horizontal;
			created->active = 1;
			created->coreMask2 = 1 << core2;
			created->numOfDecoders = numOfDecoders;
			created->lock = lock;
			created->numOfEncoders = numOfEncoders;
			created->reserved3 = 0;
			created->reserved4 = 0;
			*library = vnzPublicClosePrivate((uint32_t)created, LIBRARY_SIZE);
			return 0;
		}
		if (lock)
			vnzLockFree(lock);
	}
	if (handle != -1)
		videoServiceClose(service, handle);
	return VIDEO_ERROR_PARAM;
}

int32_t videoLibraryTerminate(VideoLibrary *library)
{
	volatile uint8_t *lock;

	if (!library || !library->lock)
		return VIDEO_ERROR_PARAM;
	lock = library->lock;
	vnzSpinLock(lock);
	if (library->active != 1) {
		vnzSpinUnlock(lock);
		return VIDEO_ERROR_STATE;
	}
	library->active = 0;
	library->lock = 0;
	vnzSpinUnlock(lock);
	vnzLockFree(lock);
	videoServiceClose(library->serviceMemory, library->service);
	return 0;
}
