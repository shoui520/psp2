/*
 * SceGxm: USSE register budgets (data master setup).
 *
 * How many tasks of a program fit in the USSE register file decides the
 * DMS words the GPU takes with it.
 */

#include "gxm_internal.h"

/* the attribute registers the hardware has, from the driver */
static inline SceUInt32 gxmUsseRegisterCount(void)
{
	return gxmGlobals.top.driverState[42];
}

void gxmDmsVertexInfo(SceUInt32 primaryRegs, SceUInt32 secondaryRegs, SceUInt32 tempRegs,
			SceUInt32 outputRegCount, SceUInt8 phased, GxmDmsInfo *info)
{
	SceUInt32 regs, paRequired, paAvailableInSubdiv, taskWords, outputs, perBuffer;
	SceUInt32 maxInstancesPerTask, maxVerticesPerPartition;

	GXM_DBG_ASSERT(23, outputRegCount != 0);
	regs = primaryRegs + 3 + ((tempRegs + 1) & ~1);
	paRequired = regs & ~3;
	if (paRequired == 0) {
		maxInstancesPerTask = 8;
		taskWords = 0;
	} else {
		paAvailableInSubdiv = (gxmUsseRegisterCount() - 0x7e -
				       ((secondaryRegs + 0x1f) & ~0x1f)) / 3;
		if (phased && (regs & 0x1c) == 0 && paRequired + 4 <= paAvailableInSubdiv)
			paRequired += 4;
		GXM_DBG_ASSERT(51, paRequired <= paAvailableInSubdiv);
		taskWords = paRequired << 7;
		maxInstancesPerTask = paAvailableInSubdiv / paRequired;
		if (maxInstancesPerTask > 7)
			maxInstancesPerTask = 8;
	}
	outputs = (outputRegCount + 3) & ~3;
	maxVerticesPerPartition = 0x80 / outputs;
	if (maxVerticesPerPartition > 15)
		maxVerticesPerPartition = 16;
	if (maxInstancesPerTask <= maxVerticesPerPartition)
		maxVerticesPerPartition = maxInstancesPerTask;
	perBuffer = maxVerticesPerPartition;
	if (perBuffer > 3)
		perBuffer = 4;
	GXM_DBG_ASSERT(76, maxInstancesPerTask > 0);
	GXM_DBG_ASSERT(77, maxVerticesPerPartition > 0);
	info->maxInstancesPerTask = maxInstancesPerTask;
	info->maxVerticesPerPartition = maxVerticesPerPartition;
	info->word = taskWords | outputs << 23 | (perBuffer - 1) << 23;
}

void gxmDmsFragmentInfo(SceUInt32 primaryRegs, SceUInt32 secondaryRegs, SceUInt32 tempRegs,
		      SceBool phased, SceUInt32 *word)
{
	SceUInt32 regs, pixelSize, paRegRequired, secondary, available, paRegionSizeInChunks, limit, tasks;
	SceUInt32 maxUsseTaskSizeInChunks, maxUsseTaskChunksAvailable, perLine, extra, count, size;

	regs = tempRegs + 3 + primaryRegs;
	pixelSize = regs & ~3;
	GXM_DBG_ASSERT(98, pixelSize <= 31*4);
	paRegRequired = pixelSize * 4;
	secondary = (secondaryRegs + 0x1f) >> 5;
	if (paRegRequired == 0) {
		size = 0x18000;
		tasks = 0;
	} else {
		available = (gxmUsseRegisterCount() >> 5) - secondary;
		paRegionSizeInChunks = (available - 2) / 3;
		if (phased && (regs & 0x1c) == 0 && pixelSize + 4 < 0x7d &&
		    paRegRequired + 0x10 <= paRegionSizeInChunks*EURASIA_PDS_CHUNK_SIZE) {
			pixelSize += 4;
			paRegRequired += 0x10;
		}
		GXM_DBG_ASSERT(128, paRegRequired <= paRegionSizeInChunks*EURASIA_PDS_CHUNK_SIZE);
		limit = ((available - 4) >> 2) * 32;
		tasks = paRegionSizeInChunks*EURASIA_PDS_CHUNK_SIZE / paRegRequired;
		if (tasks > 3)
			tasks = 4;
		maxUsseTaskSizeInChunks = (paRegRequired * tasks + 0x1f) >> 5;
		maxUsseTaskChunksAvailable = available - 4 - (maxUsseTaskSizeInChunks - 1) * 2;
		GXM_DBG_ASSERT(137, maxUsseTaskSizeInChunks <= maxUsseTaskChunksAvailable);
		if (limit < paRegRequired) {
			size = (tasks - 1) << 15;
			tasks = ((tasks & 0x3fffffff) != 0) << 22;
		} else {
			perLine = maxUsseTaskChunksAvailable / maxUsseTaskSizeInChunks;
			count = limit / paRegRequired;
			if (count > 15)
				count = 16;
			if (tasks << 2 <= count)
				count = tasks << 2;
			extra = ((maxUsseTaskChunksAvailable - maxUsseTaskSizeInChunks * perLine) * 32) /
				paRegRequired;
			size = (tasks - 1) << 15;
			if (tasks * perLine + extra <= count)
				count = tasks * perLine + extra;
			tasks = (count & 0xf) << 22;
		}
	}
	*word = tasks | size | secondary << 17 | (pixelSize >> 2) << 27;
}
