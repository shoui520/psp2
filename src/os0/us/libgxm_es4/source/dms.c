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
			SceUInt32 outputRegs, SceUInt8 phased, GxmDmsInfo *info)
{
	SceUInt32 regs, aligned, budget, taskWords, outputs, perBuffer;

	regs = primaryRegs + 3 + ((tempRegs + 1) & ~1);
	aligned = regs & ~3;
	if (aligned == 0) {
		budget = 8;
		taskWords = 0;
		aligned = 8;
	} else {
		budget = (gxmUsseRegisterCount() - 0x7e - ((secondaryRegs + 0x1f) & ~0x1f)) / 3;
		if (phased && (regs & 0x1c) == 0 && aligned + 4 <= budget)
			aligned += 4;
		taskWords = aligned << 7;
		budget /= aligned;
		if (budget > 7)
			budget = 8;
		aligned = budget & 0xff;
	}
	outputs = (outputRegs + 3) & ~3;
	perBuffer = 0x80 / outputs;
	info->tasks = aligned;
	if (perBuffer > 15)
		perBuffer = 16;
	if (budget <= perBuffer)
		perBuffer = budget;
	info->outputTasks = perBuffer;
	if (perBuffer > 3)
		perBuffer = 4;
	info->word = taskWords | outputs << 23 | (perBuffer - 1) << 23;
}

void gxmDmsFragmentInfo(SceUInt32 primaryRegs, SceUInt32 secondaryRegs, SceUInt32 tempRegs,
		      SceBool phased, SceUInt32 *word)
{
	SceUInt32 regs, aligned, size, secondary, budget, available, limit, tasks, lines;
	SceUInt32 perLine, extra, rest, count;

	regs = tempRegs + 3 + primaryRegs;
	aligned = regs & ~3;
	size = aligned * 4;
	secondary = (secondaryRegs + 0x1f) >> 5;
	if (size == 0) {
		size = 0x18000;
		tasks = 0;
	} else {
		available = (gxmUsseRegisterCount() >> 5) - secondary;
		budget = (available - 2) / 3;
		if (!phased || (regs & 0x1c) != 0) {
			tasks = budget << 5;
		} else {
			tasks = budget * 32;
			if (aligned + 4 < 0x7d && size + 0x10 <= tasks) {
				aligned += 4;
				size += 0x10;
			}
		}
		limit = ((available - 4) >> 2) * 32;
		tasks /= size;
		if (tasks > 3)
			tasks = 4;
		if (limit < size) {
			size = (tasks - 1) << 15;
			tasks = ((tasks & 0x3fffffff) != 0) << 22;
		} else {
			lines = (size * tasks + 0x1f) >> 5;
			rest = available - 4 - (lines - 1) * 2;
			perLine = rest / lines;
			count = limit / size;
			if (count > 15)
				count = 16;
			if (tasks << 2 <= count)
				count = tasks << 2;
			extra = ((rest - lines * perLine) * 32) / size;
			lines = tasks * perLine + extra;
			size = (tasks - 1) << 15;
			if (lines <= count)
				count = lines;
			tasks = (count & 0xf) << 22;
		}
	}
	*word = tasks | size | secondary << 17 | (aligned >> 2) << 27;
}
