/*
 * The BUSS class: one granularity's worth of samples per processor, buss and channel,
 * locked by the racks that fill it and by the CORE that reads it.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>

#include "as_ssp.h"
#include "as_buss.h"

static int sceSasAsBussDestroy(SceSasAsHandle *handle)
{
	SceSasAsBuss *buss = (SceSasAsBuss *)handle;

	sceSasAsMutexDelete(&buss->mutex);
	return 0;
}

static int sceSasAsBussLock(SceSasAsHandle *handle, SceUInt32 block, void **pointer)
{
	SceSasAsBuss *buss = (SceSasAsBuss *)handle;

	if ((block & SCE_SAS_AS_BUSS_BLOCK_MASK) != SCE_SAS_AS_BUSS_BLOCK_BUFFERS)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	*pointer = &buss->block;
	sceSasAsMutexLock(&buss->mutex);
	sceSasAsSpinLock(&buss->lock);
	return 0;
}

static int sceSasAsBussUnlock(SceSasAsHandle *handle, SceUInt32 block)
{
	SceSasAsBuss *buss = (SceSasAsBuss *)handle;

	if ((block & SCE_SAS_AS_BUSS_BLOCK_MASK) != SCE_SAS_AS_BUSS_BLOCK_BUFFERS)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	sceSasAsMutexUnlock(&buss->mutex);
	sceSasAsAtomicOr(&buss->updated, 1);
	return sceSasAsSpinTryUnlock(&buss->lock);
}

/* the smallest power of two not below value (value > 0) */
static inline SceUInt32 sceSasAsRoundUpPowerOfTwo(SceUInt32 value)
{
	SceUInt32 bits;
	SceUInt32 shift;

	shift = 0;
	for (bits = value >> 1; bits != 0; bits >>= 1)
		shift++;
	return (value - 1 + (1u << shift)) & -(1u << shift);
}

static int sceSasAsBussCreate(SceSasAsAllocator *allocator, SceSasAsConfigEntry **config,
			      SceSasAsHandle **handle)
{
	SceSasAsBuss *buss;
	void **buffers;
	SceInt32 numGranularities;
	SceInt32 numChannels;
	SceInt32 numBusses;
	SceInt32 numProcessors;
	SceInt32 alignSize;
	SceSize bufferSize;
	SceSize alignment;
	int processor;
	int index;
	int bus;
	int channel;

	sceSasAsConfigGetInt(config, "numGranularities", 256, &numGranularities);
	sceSasAsConfigGetInt(config, "numProcessors", 1, &numProcessors);
	sceSasAsConfigGetInt(config, "numChannels", 2, &numChannels);
	sceSasAsConfigGetInt(config, "numBusses", 1, &numBusses);
	sceSasAsConfigGetInt(config, "bussBufferAlignSize", 0, &alignSize);
	buffers = sceSasAsAllocatorAlloc(allocator,
					 numProcessors * numBusses * numChannels * sizeof(*buffers), 1);
	bufferSize = numGranularities * sizeof(SceInt32);
	if (alignSize <= 0)
		alignment = 16;
	else if (numGranularities != 0)
		alignment = sceSasAsRoundUpPowerOfTwo(numGranularities) * sizeof(SceInt32);
	else
		alignment = numGranularities;
	index = 0;
	for (processor = 0; processor < numProcessors; processor++) {
		for (bus = 0; bus < numBusses; bus++) {
			for (channel = 0; channel < numChannels; channel++) {
				if (buffers != NULL)
					buffers[index] = sceSasAsAllocatorAllocAligned(allocator, bufferSize,
										       alignment, 1);
				else
					sceSasAsAllocatorAllocAligned(allocator, bufferSize, alignment, 1);
				index++;
			}
		}
	}
	buss = sceSasAsAllocatorAllocAligned(allocator, sizeof(*buss), 16, 1);
	if (buss != NULL) {
		buss->locked.handle = &buss->handle;
		buss->block.buffers = buffers;
		buss->block.numGranularities = numGranularities;
		buss->numGranularities = numGranularities;
		buss->buffers = buffers;
		buss->block.numBusses = numBusses;
		buss->numBusses = numBusses;
		buss->block.numChannels = numChannels;
		buss->numChannels = numChannels;
		buss->numProcessors = numProcessors;
		buss->locked.block = SCE_SAS_AS_BUSS_BLOCK_BUFFERS;
		sceSasAsMutexCreate(&buss->mutex, "SceSasAsBussBuffer");
	}
	if (handle != NULL)
		*handle = &buss->handle;
	return 0;
}

void **sceSasAsBussGetBuffers(const SceSasAsBuss *buss, SceUInt32 *numBusses)
{
	if (numBusses != NULL)
		*numBusses = buss->numBusses;
	return buss->buffers;
}

int sceSasAsBussLockBuffers(SceSasAsBuss *buss)
{
	sceSasAsMutexLock(&buss->mutex);
	return 0;
}

int sceSasAsBussUnlockBuffers(SceSasAsBuss *buss)
{
	sceSasAsMutexUnlock(&buss->mutex);
	return 0;
}

const SceSasAsClass sceSasAsBussClass = {
	.id = SCE_SAS_AS_CLASS_BUSS,
	.name = "BUSS",
	.version = SCE_SAS_AS_CLASS_VERSION,
	.create = sceSasAsBussCreate,
	.destroy = sceSasAsBussDestroy,
	.ops = {
		.lock = sceSasAsBussLock,
		.unlock = sceSasAsBussUnlock,
	},
};
