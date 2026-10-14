/* SceSdif MMC card access (Sony source: mmc.c). */
#include "sdif_internal.h"
#include <sce/debug.h>
#include <sce/threadmgr.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <string.h>

SdifMmcContext gMmcContexts[2];

#if FW_VERSION >= 0x04000000
static const uint32_t mmcReadDelay[9] = {
	1500, 1600, 1650, 1700, 2000, 2500, 3400, 6800, 13600,
};
#else
static const uint8_t mmcAccessTimeValues[16] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};
#endif

static const uint32_t mmcClockUnits[8] = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};
static const uint32_t mmcAccessTimeUnits[8] = {
	1, 10, 100, 1000, 10000, 100000, 1000000, 10000000,
};

#if FW_VERSION < 0x04000000
static const uint32_t mmcReadDelay[9] = {
	1500, 1600, 1650, 1700, 2000, 2500, 3400, 6800, 13600,
};
#endif
static const uint8_t mmcClockValues[16] = {
	0, 10, 12, 13, 15, 20, 26, 30, 35, 40, 45, 52, 55, 60, 70, 80,
};
#if FW_VERSION >= 0x04000000
static const uint8_t mmcAccessTimeValues[16] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};
#endif

void sceMmcDumpPartitionInfo(SdifMmcContext *part)
{
	int index;
	SdifMmcPartition *partition;

	ksceKernelPrintfWithInfo(0, SCE_DBGINFO(425), "#%d %d\n", 425,
				part->currentPartition->partitionType);
	partition = part->partitions;
	for (index = 0; index < 9; index++, partition++) {
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(428),
			"#%d %d partitionType : %d\n", 428, index,
			partition->partitionType);
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(430),
			"#%d %d va         : %lld\n", 430, index, partition->va);
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(432),
			"#%d %d size       : %lld\n", 432, index, partition->size);
		__builtin_prefetch(&partition[2].accessType);
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(434),
			"#%d %d vsector    : %lld\n", 434, index,
			partition->va >> 9);
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(436),
			"#%d %d pa         : %lld\n", 436, index, partition->pa);
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(438),
			"#%d %d accessType : %d\n", 438, index,
			partition->accessType);
	}
}

int sceMmcSetupPartitionAccess(SdifMmcContext *part, unsigned int sector,
			       unsigned int count)
{
	SdifMmcPartition *partition = part->currentPartition;
	SdifContextGlobal *context;
	SdifCommand *command;
	uint64_t first, end, partitionFirst, partitionSize, partitionEnd;
	int accessType, result;

#if FW_DEBUG
	ksceKernelAssert(partition != NULL,
		SCE_DBGINFO_FUNC(707, "sceMmcIsCurrentPartitionAccess"),
		__builtin_return_address(0));
#endif
	partitionFirst = partition->va;
	partitionSize = partition->size;
	first = (uint64_t)sector * part->blockLength;
	end = (uint64_t)count * part->blockLength + first;
	if (first >= partitionFirst) {
		partitionEnd = partitionSize + partitionFirst;
		if (first < partitionEnd) {
			if (partitionFirst < end) {
				if (partitionEnd >= end)
					return 0;
			}
		}
	}

#define TRY_MMC_PARTITION(index) do { \
	partition = &part->partitions[index]; \
	partitionFirst = partition->va; \
	partitionSize = partition->size; \
	if (first < partitionFirst) \
		break; \
	partitionEnd = partitionSize + partitionFirst; \
	if (first >= partitionEnd) \
		break; \
	if (partitionFirst >= end) \
		break; \
	if (partitionEnd < end) \
		break; \
	goto selected; \
} while (0)
	TRY_MMC_PARTITION(0);
	TRY_MMC_PARTITION(1);
	TRY_MMC_PARTITION(2);
	TRY_MMC_PARTITION(3);
	TRY_MMC_PARTITION(4);
	TRY_MMC_PARTITION(5);
	TRY_MMC_PARTITION(6);
	TRY_MMC_PARTITION(7);
	TRY_MMC_PARTITION(8);
#undef TRY_MMC_PARTITION
	return 0x8032000e;

selected:
	accessType = partition->accessType;
	if (part->currentPartition->accessType == accessType) {
		part->currentPartition = partition;
		return 0;
	}
	context = part->contextGlobal;
	accessType &= 7;
	command = sdifAllocateCommandLocked(context);
	if (command) {
		command->size = sizeof(*command);
		command->flags = 0x2b;
		command->argument = (accessType << 8) | 0x03b30000;
		command->command = 6;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		if (result >= 0) {
			sdifFreeCommandLocked(context, command);
#if FW_DEBUG
			ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(745),
				"Switch HW partition. accessType:%d -> %d\n",
				part->currentPartition->accessType,
				partition->accessType);
#endif
			part->currentPartition = partition;
			return 0;
		}
		sdifFreeCommandLocked(context, command);
	} else {
		result = 0x80320011;
	}
	ksceKernelPrintfWithInfo(0, SCE_DBGINFO(751), "0x%x %d -> %d\n",
		result, part->currentPartition->accessType, partition->accessType);
	return result;
}

static inline uint32_t sceMmcVsector2Psector(SdifMmcContext *part,
					   unsigned int sector)
{
	SdifMmcPartition *partition = part->currentPartition;
	uint32_t blockLength;

#if FW_DEBUG
	ksceKernelAssert(partition != NULL, SCE_DBGINFO(687),
			 __builtin_return_address(0));
#endif
	blockLength = part->blockLength;
#if FW_DEBUG
	ksceKernelAssert((uint64_t)sector * part->blockLength >= partition->va,
			 SCE_DBGINFO(690), __builtin_return_address(0));
#endif
	return ((uint64_t)blockLength * sector + partition->pa - partition->va) /
		part->blockLength;
}

int sceMmcReadSectorInternal(SdifMmcContext *part, unsigned int sector,
			   void *buffer, unsigned int count)
{
	SdifContextGlobal *context = part->contextGlobal;
	SdifCommand *command, *secondary;
	const SceKernelDebugInfo *info;
	uint32_t delay;
	uint64_t transferTime;
	int result, line, delayIndex;

	if (!context)
		return 0x8032000e;
	result = sdifLockPort(context);
	if (result < 0)
		return result;
	result = sceMmcSetupPartitionAccess(part, sector, count);
	if (result < 0) {
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(1101),
			"0x%x %d %d 0x%x(%d) %d\n", result, 1101,
			context->port.index, sector, sector, count);
		sceMmcDumpPartitionInfo(part);
		sdifUnlockPort(context);
		return result;
	}
	command = sdifAllocateCommandLocked(context);
	if (!command)
		goto noCommand;
	command->command = count == 1 ? 17 : 18;
	command->size = sizeof(*command);
	command->flags = 0x514;
	if (!(part->state & 1))
		command->argument = sceMmcVsector2Psector(part, sector) << 9;
	else
		command->argument = sceMmcVsector2Psector(part, sector);
	command->blockSize = part->blockLength;
	command->blockCount = count;
	command->buffer = buffer;
	if (command->command != 18) {
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		if (result < 0)
			goto commandError;
		context->port.lastTransferTime = command->endTime - command->startTime;
		result = SceSdifForDriver_53518827(part->contextGlobal, NULL);
		if (result < 0)
			goto statusError;
	} else {
		secondary = sdifAllocateCommandLocked(context);
		if (!secondary) {
			sdifFreeCommandLocked(context, command);
			goto noCommand;
		}
		secondary->argument = count;
		secondary->size = sizeof(*secondary);
		secondary->flags = 0x13;
		secondary->command = 23;
		secondary->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, secondary, command, 3);
		if (result < 0) {
			sdifFreeCommandLocked(context, secondary);
			goto commandError;
		}
		context->port.lastTransferTime = command->endTime - command->startTime;
		result = SceSdifForDriver_53518827(part->contextGlobal, NULL);
		if (result < 0) {
			sdifFreeCommandLocked(context, secondary);
			goto statusError;
		}
		sdifFreeCommandLocked(context, secondary);
	}
	sdifFreeCommandLocked(context, command);
	if (context->port.slowMode == 1 && context->port.index == 1) {
		transferTime = context->port.lastTransferTime;
		if (count > 128) {
			delay = 47 * count;
		} else {
			if (count <= 1)
				delayIndex = 0;
			else if (count <= 2)
				delayIndex = 1;
			else if (count <= 4)
				delayIndex = 2;
			else if (count <= 8)
				delayIndex = 3;
			else if (count <= 16)
				delayIndex = 4;
			else if (count <= 32)
				delayIndex = 5;
			else if (count <= 64)
				delayIndex = 6;
			else
				delayIndex = 7;
			delay = mmcReadDelay[delayIndex];
		}
		context->port.readRetries++;
		if (transferTime < delay)
			sceKernelDelayThread(delay - transferTime);
	}
	sdifUnlockPort(context);
	return 0;

noCommand:
	sdifUnlockPort(context);
	return 0x80320011;
commandError:
	sdifFreeCommandLocked(context, command);
	info = SCE_DBGINFO(1180);
	line = 1180;
	goto error;
statusError:
	sdifFreeCommandLocked(context, command);
	info = SCE_DBGINFO(1197);
	line = 1197;
error:
	ksceKernelPrintfWithInfo(0, info, "0x%x %d %d 0x%x(%d) %d\n",
		result, line, context->port.index, sector, sector, count);
	sdifUnlockPort(context);
	return result;
}

/* Export B32776C7: change the MMC card's block length under the port lock. */
int SceSdifForDriver_B32776C7(SdifMmcContext *part, uint32_t blockLength)
{
	int result = 0x8032000e;

	if (part) {
		result = sdifLockPort(part->contextGlobal);
		if (result >= 0) {
			result = sdifSetBlockLength(part->contextGlobal, blockLength);
			if (!result)
				part->blockLength = blockLength;
			sdifUnlockPort(part->contextGlobal);
		}
	}
	return result;
}

int sceMmcReadSector(SdifMmcContext *part, unsigned int sector, void *buffer,
		     unsigned int count)
{
	unsigned int firstCount, transferCount;
	uint8_t *cursor = buffer;
	int result;

	if (!buffer || !part || !count)
		return 0x8032000e;
	if (!part->bug16635Target)
		return sceMmcReadSectorInternal(part, sector, buffer, count);
	firstCount = 4096 - (sector & 0xfff);
	if (firstCount >= count)
		return sceMmcReadSectorInternal(part, sector, buffer, count);
	result = sceMmcReadSectorInternal(part, sector, buffer,
					(uint16_t)firstCount);
	if (result < 0)
		return result;
	count -= firstCount;
	cursor += part->blockLength * firstCount;
	sector += firstCount;
#if FW_DEBUG
	ksceKernelAssert((sector & 0xfff) == 0, SCE_DBGINFO(1285),
			 __builtin_return_address(0));
#endif
	while (count) {
		transferCount = count;
		if (transferCount >= 4096)
			transferCount = 4096;
		result = sceMmcReadSectorInternal(part, sector, cursor,
						(uint16_t)transferCount);
		if (result < 0)
			return result;
		sector += transferCount;
		count -= transferCount;
		cursor += part->blockLength * transferCount;
	}
	return result;
}

int sceMmcWriteSectorInternal(SdifMmcContext *part, unsigned int sector,
			    const void *buffer, unsigned int count)
{
	SdifContextGlobal *context;
	SdifCommand *command, *secondary;
	const SceKernelDebugInfo *info;
	uint32_t status, delay;
	uint64_t transferTime;
	int result, line;

	if (!buffer || !part || !count || !part->contextGlobal)
		return 0x8032000e;
	context = part->contextGlobal;
	result = sdifLockPort(context);
	if (result < 0)
		return result;
	result = sceMmcSetupPartitionAccess(part, sector, count);
	if (result < 0) {
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(1340),
			"0x%x %d %d 0x%x(%d) %d\n", result, 1340,
			context->port.index, sector, sector, count);
		sceMmcDumpPartitionInfo(part);
		sdifUnlockPort(context);
		return result;
	}
	command = sdifAllocateCommandLocked(context);
	if (!command)
		goto noCommand;
	command->flags = 0x614;
#if FW_DEBUG
	ksceKernelAssert(1, SCE_DBGINFO(1360), __builtin_return_address(0));
#endif
	command->size = sizeof(*command);
	command->command = count == 1 ? 24 : 25;
	if (!(part->state & 1))
		command->argument = sceMmcVsector2Psector(part, sector) << 9;
	else
		command->argument = sceMmcVsector2Psector(part, sector);
	command->blockSize = part->blockLength;
	command->buffer = (void *)buffer;
	command->blockCount = count;
	secondary = NULL;
	if (command->command != 25) {
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	} else {
		secondary = sdifAllocateCommandLocked(context);
		if (!secondary) {
			sdifFreeCommandLocked(context, command);
			goto noCommand;
		}
		secondary->argument = count;
		secondary->size = sizeof(*secondary);
		secondary->flags = 0x13;
		secondary->command = 23;
		secondary->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, secondary, command, 3);
	}
	if (result < 0) {
		if (secondary)
			sdifFreeCommandLocked(context, secondary);
		sdifFreeCommandLocked(context, command);
		info = SCE_DBGINFO(1423);
		line = 1423;
		goto error;
	}
	context->port.lastTransferTime = command->endTime - command->startTime;
	result = SceSdifForDriver_53518827(part->contextGlobal, &status);
	if (result < 0) {
		if (secondary)
			sdifFreeCommandLocked(context, secondary);
		sdifFreeCommandLocked(context, command);
		info = SCE_DBGINFO(1440);
		line = 1440;
		goto error;
	}
	if (secondary)
		sdifFreeCommandLocked(context, secondary);
	sdifFreeCommandLocked(context, command);
	if (context->port.slowMode == 1 && context->port.index == 1) {
		transferTime = context->port.lastTransferTime;
		delay = 205 * count;
		context->port.writeRetries++;
		if (transferTime < delay)
			sceKernelDelayThread(delay - transferTime);
	}
	sdifUnlockPort(context);
	return 0;

noCommand:
	sdifUnlockPort(context);
	return 0x80320011;
error:
	ksceKernelPrintfWithInfo(0, info, "0x%x %d %d 0x%x(%d) %d\n",
		result, line, context->port.index, sector, sector, count);
	sdifUnlockPort(context);
	return result;
}

/* Export 60642F49: write controller-generated data in bounded transfers. */
int SceSdifForDriver_60642F49(SdifMmcContext *part, unsigned int sector,
			    unsigned int count)
{
	SdifContextGlobal *context;
	SdifCommand *command, *secondary;
	const SceKernelDebugInfo *info;
	unsigned int transferCount;
	uint32_t status, delay;
	uint64_t transferTime;
	uint16_t maximumCount;
	int result, line;

	if (!part || !count)
		return 0x8032000e;
	maximumCount = 16 * (4096U / part->blockLength);
	for (;;) {
		context = part->contextGlobal;
		if (!context)
			return 0x8032000e;
		result = sdifLockPort(context);
		if (result < 0)
			goto retry;
		transferCount = count;
		if (transferCount >= maximumCount)
			transferCount = maximumCount;
		result = sceMmcSetupPartitionAccess(part, sector, transferCount);
		if (result < 0) {
			ksceKernelPrintfWithInfo(0,
				SCE_DBGINFO_FUNC(1340, "sceMmcWriteSectorInternal"),
				"0x%x %d %d 0x%x(%d) %d\n", result, 1340,
				context->port.index, sector, sector, transferCount);
			sceMmcDumpPartitionInfo(part);
			sdifUnlockPort(context);
			goto retry;
		}
		command = sdifAllocateCommandLocked(context);
		if (!command)
			goto noCommand;
		command->flags = 0x614;
#if FW_DEBUG
		ksceKernelAssert(1,
			SCE_DBGINFO_FUNC(1360, "sceMmcWriteSectorInternal"),
			__builtin_return_address(0));
#endif
		command->command = transferCount == 1 ? 24 : 25;
		command->size = sizeof(*command);
		if (!(part->state & 1))
			command->argument = sceMmcVsector2Psector(part, sector) << 9;
		else
			command->argument = sceMmcVsector2Psector(part, sector);
		command->blockSize = part->blockLength;
		command->blockCount = transferCount;
		command->buffer = NULL;
		command->flags |= 0x1000000;
		secondary = NULL;
		if (command->command != 25) {
			result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		} else {
			secondary = sdifAllocateCommandLocked(context);
			if (!secondary) {
				sdifFreeCommandLocked(context, command);
				goto noCommand;
			}
			secondary->buffer = NULL;
			secondary->argument = transferCount;
			secondary->size = sizeof(*secondary);
			secondary->flags = 0x13;
			secondary->command = 23;
			result = sdifSendCheckedSyncCommand(context, secondary, command, 3);
		}
		if (result < 0) {
			if (secondary)
				sdifFreeCommandLocked(context, secondary);
			sdifFreeCommandLocked(context, command);
			info = SCE_DBGINFO_FUNC(1423, "sceMmcWriteSectorInternal");
			line = 1423;
			goto error;
		}
		context->port.lastTransferTime = command->endTime - command->startTime;
		result = SceSdifForDriver_53518827(part->contextGlobal, &status);
		if (result < 0) {
			if (secondary)
				sdifFreeCommandLocked(context, secondary);
			sdifFreeCommandLocked(context, command);
			info = SCE_DBGINFO_FUNC(1440, "sceMmcWriteSectorInternal");
			line = 1440;
			goto error;
		}
		if (secondary)
			sdifFreeCommandLocked(context, secondary);
		sdifFreeCommandLocked(context, command);
		if (context->port.slowMode == 1 && context->port.index == 1) {
			transferTime = context->port.lastTransferTime;
			delay = 205 * transferCount;
			context->port.writeRetries++;
			if (transferTime < delay)
				sceKernelDelayThread(delay - transferTime);
		}
		sector += transferCount;
		sdifUnlockPort(context);
		count = (uint16_t)(count - transferCount);
		if (!count)
			return 0;
		continue;

noCommand:
		result = 0x80320011;
		sdifUnlockPort(context);
		goto retry;
error:
		ksceKernelPrintfWithInfo(0, info, "0x%x %d %d 0x%x(%d) %d\n",
			result, line, context->port.index, sector, sector, transferCount);
		sdifUnlockPort(context);
retry:
		if (result != (int)0x80320011 || !count)
			return result;
	}
}

int ksceSdifReadCmd56(SdifMmcContext *part, void *buffer, int length)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint16_t blockLength;
	int result;

	if (!buffer || !part || part->blockLength != length)
		return 0x8032000e;
	context = part->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x514;
	command->command = 56;
	command->argument = 1;
	blockLength = part->blockLength;
	command->buffer = buffer;
	command->blockCount = 1;
	command->blockSize = blockLength;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	sdifFreeCommandLocked(context, command);
	return result;
}

int sceMmcSendGeneralCmdData(SdifMmcContext *part, void *buffer, int length)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint16_t blockLength;
	int result;

	if (part && buffer && part->blockLength == length) {
		context = part->contextGlobal;
		command = sdifAllocateCommandLocked(context);
		if (command) {
			command->size = sizeof(*command);
			command->flags = 0x614;
			command->command = 56;
			command->argument = 0;
			blockLength = part->blockLength;
			command->buffer = buffer;
			command->blockCount = 1;
			command->blockSize = blockLength;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
			sdifFreeCommandLocked(context, command);
			if (result >= 0)
				return result;
		} else {
			result = 0x80320011;
		}
	} else {
		result = 0x8032000e;
	}
#if FW_DEBUG
	ksceKernelPrintfLevelWithInfo(1, 0, SCE_DBGINFO(1540),
		"GC error:0x%x media ctrl cmd opcode:0x%x\n", result,
		((uint32_t *)buffer)[8]);
#endif
	return result;
}

int sendWriteProtectStatus(SdifMmcContext *part, unsigned int sector,
			   unsigned int count, int *writeProtected)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	unsigned int endSector, cursor;
	uint64_t groupSize;
	uint32_t status;
	int result;

	if (!count || !part || !writeProtected)
		return 0x8032000e;
	endSector = sector - 1 + count;
	groupSize = ((uint64_t)part->extendedCsd[221] *
		     part->extendedCsd[224]) << 10;
	for (cursor = sector; cursor <= endSector; cursor += groupSize) {
		context = part->contextGlobal;
		status = 0;
		if (!context)
			return 0x8032000e;
		result = sdifLockPort(context);
		if (result < 0)
			return result;
		result = sceMmcSetupPartitionAccess(part, cursor, (uint16_t)groupSize);
		if (result < 0) {
			ksceKernelPrintfWithInfo(0, SCE_DBGINFO(909),
				"0x%x %d %d 0x%x(%d)\n", result, 909,
				context->port.index, cursor, cursor);
			sceMmcDumpPartitionInfo(part);
			sdifUnlockPort(context);
			return result;
		}
		command = sdifAllocateCommandLocked(context);
		if (!command) {
			sdifUnlockPort(context);
			return 0x80320011;
		}
		command->size = sizeof(*command);
		command->flags = 0x114;
		command->command = 30;
		if (part->state & 1)
			command->argument = sceMmcVsector2Psector(part, cursor);
		else
			command->argument = sceMmcVsector2Psector(part, cursor) << 9;
		command->buffer = &status;
		command->blockSize = 4;
		command->blockCount = 0;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		if (result < 0) {
			ksceKernelPrintfWithInfo(0, SCE_DBGINFO(944),
				"0x%x %d %d 0x%x(%d)\n", result, 944,
				context->port.index, cursor, cursor);
			sdifFreeCommandLocked(context, command);
			sdifUnlockPort(context);
			return result;
		}
		sdifFreeCommandLocked(context, command);
		sdifUnlockPort(context);
		if (((const uint8_t *)&status)[3] & 1) {
			*writeProtected = 1;
			return 0;
		}
	}
	if (!(sector % (uint32_t)groupSize)) {
		*writeProtected = 0;
		return 0;
	}
	context = part->contextGlobal;
	status = 0;
	if (!context)
		return 0x8032000e;
	result = sdifLockPort(context);
	if (result < 0)
		return result;
	result = sceMmcSetupPartitionAccess(part, endSector, 1);
	if (result < 0) {
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(909),
			"0x%x %d %d 0x%x(%d)\n", result, 909,
			context->port.index, endSector, endSector);
		sceMmcDumpPartitionInfo(part);
		sdifUnlockPort(context);
		return result;
	}
	command = sdifAllocateCommandLocked(context);
	if (!command) {
		sdifUnlockPort(context);
		return 0x80320011;
	}
	command->size = sizeof(*command);
	command->flags = 0x114;
	command->command = 30;
	if (part->state & 1)
		command->argument = sceMmcVsector2Psector(part, endSector);
	else
		command->argument = sceMmcVsector2Psector(part, endSector) << 9;
	command->buffer = &status;
	command->blockSize = 4;
	command->blockCount = 0;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0) {
		ksceKernelPrintfWithInfo(0, SCE_DBGINFO(944),
			"0x%x %d %d 0x%x(%d)\n", result, 944,
			context->port.index, endSector, endSector);
		sdifFreeCommandLocked(context, command);
		sdifUnlockPort(context);
		return result;
	}
	sdifFreeCommandLocked(context, command);
	sdifUnlockPort(context);
	*writeProtected = (((const uint8_t *)&status)[3] & 1) != 0;
	return 0;
}

int sdifStartMmcInitialization(unsigned int index, SdifMmcContext **part)
{
	SdifContextGlobal *context, *commandContext;
	SdifMmcContext *mmc;
	SdifCommand *command;
	uint32_t voltages, argument, response;
	uint8_t powerControl, retries;
	int result;

	if (part)
		*part = NULL;
	context = ksceSdifGetSdContextGlobal(index);
	if (!context)
		return 0x80320013;
	if (sceSdifReset(context, 2) < 0)
		return 0x80320015;
	result = sdifIsCardInserted(context);
	if (result < 0)
		return result;
	if (!result)
		return 0x80320013;
	voltages = context->port.voltages;
	if (voltages & 0x300000)
		powerControl = 14;
	else if (voltages & 0x60000)
		powerControl = 12;
	else if (voltages & 0x80)
		powerControl = 10;
	else
		powerControl = 0;
	result = sdifPowerOn(context, powerControl);
	if (result < 0)
		return result;
	if (index == 0)
		mmc = &gMmcContexts[0];
	else if (index == 1)
		mmc = &gMmcContexts[1];
	else
		return 0x80320013;
	mmc->blockLength = 512;
	mmc->contextGlobal = context;
	mmc->state = 0;
	mmc->bug16635Target = 0;
	context->port.deviceType = 0;
	context->port.part = NULL;
	context->port.relativeCardAddress = 0;
	retries = sceKernelGetSystemTimeWide();
	context->port.readRetries = retries;
	context->port.writeRetries = retries;
	if (sdifIdentifyCard(context) < 0)
		return 0x80320016;
	if (context->port.deviceType != 1)
		return 0x80320017;
	result = sdifResetCard(context);
	if (result < 0)
		return result;
	voltages = context->port.voltages;
	context->port.operationConditions = 0;
	commandContext = mmc->contextGlobal;
	argument = voltages & 0xff8000 ? 0x40ff8000 : 0x40000080;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->command = 1;
	command->argument = argument;
	command->size = sizeof(*command);
	command->flags = 0x42;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(commandContext, command);
		if (result != (int)0x80320002)
			return result;
	} else {
		response = command->response[0];
		if ((int)response < 0) {
			sdifFreeCommandLocked(commandContext, command);
			context->port.operationConditions = response;
		} else {
			sdifDelay(commandContext, 1000);
			sdifFreeCommandLocked(commandContext, command);
		}
	}
	if (part)
		*part = mmc;
	return 0;
}

int sdifCompleteMmcInitialization(unsigned int index)
{
	SdifContextGlobal *context, *commandContext;
	SdifMmcContext *mmc;
	SdifCommand *command;
	SdifMmcCardParameters *parameters;
	const uint8_t *cid, *csd, *extendedCsd;
	uint32_t argument, response, cardSize, multiplier, eraseSize, eraseMultiplier;
	uint64_t enhancedUnit, enhancedStart, enhancedSize, userSize;
	uint64_t generalSize[4], nextAddress, userStart;
	int result, retries;

	context = ksceSdifGetSdContextGlobal(index);
	if (!context)
		return 0x80320013;
	if (index == 0)
		mmc = &gMmcContexts[0];
	else if (index == 1)
		mmc = &gMmcContexts[1];
	else
		return 0x80320013;
	if (!context->port.operationConditions) {
		commandContext = mmc->contextGlobal;
		argument = context->port.voltages & 0xff8000 ? 0x40ff8000 : 0x40000080;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->argument = argument;
		command->buffer = NULL;
		command->size = sizeof(*command);
		command->flags = 0x42;
		command->command = 1;
		retries = 1000;
		for (;;) {
			result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 0);
			if (result < 0) {
				sdifFreeCommandLocked(commandContext, command);
				return result;
			}
			response = command->response[0];
			if ((int)response < 0)
				break;
			sdifDelay(commandContext, 1000);
			if (--retries == 0) {
				sdifFreeCommandLocked(commandContext, command);
				return 0x80320002;
			}
		}
		sdifFreeCommandLocked(commandContext, command);
		context->port.operationConditions = response;
	}
	result = sdifReadCid(mmc->contextGlobal, mmc->cid.word);
	if (result < 0)
		return result;
	cid = (const uint8_t *)&mmc->cid;
	mmc->bug16635Target = cid[15] == 21 && strncmp((const char *)&cid[7], "MF4GAM", 6) == 0;
	commandContext = mmc->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x13;
	command->command = 3;
	command->argument = 0x10000;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
	if (result < 0) {
		sdifFreeCommandLocked(commandContext, command);
		return result;
	}
	result = sdifGetCardStatusError(command->response[0]);
	sdifFreeCommandLocked(commandContext, command);
	if (result < 0)
		return result;
	context->port.relativeCardAddress = 1;
	result = sdifReadCsd(mmc->contextGlobal, mmc->csd.word);
	if (result < 0)
		return result;
	csd = (const uint8_t *)&mmc->csd;
	parameters = &mmc->parameters;
	parameters->csdStructure = (csd[15] & 0x40 ? 1 : 0) | (csd[15] & 0x80 ? 2 : 0);
	parameters->specificationVersion = (csd[15] & 0x4 ? 1 : 0) | (csd[15] & 0x8 ? 2 : 0) |
		(csd[15] & 0x10 ? 4 : 0) | (csd[15] & 0x20 ? 8 : 0);
	cardSize = (csd[7] & 0x40 ? 1 : 0) | (csd[7] & 0x80 ? 2 : 0) | (csd[8] & 0x1 ? 4 : 0) |
		(csd[8] & 0x2 ? 8 : 0) | (csd[8] & 0x4 ? 16 : 0) | (csd[8] & 0x8 ? 32 : 0) |
		(csd[8] & 0x10 ? 64 : 0) | (csd[8] & 0x20 ? 128 : 0) | (csd[8] & 0x40 ? 256 : 0) |
		(csd[8] & 0x80 ? 512 : 0) | (csd[9] & 0x1 ? 1024 : 0) | (csd[9] & 0x2 ? 2048 : 0);
	multiplier = (csd[5] & 0x80 ? 1 : 0) | (csd[6] & 0x1 ? 2 : 0) | (csd[6] & 0x2 ? 4 : 0);
	parameters->blockCount = (cardSize + 1) << (multiplier + 2);
	parameters->accessTime = mmcAccessTimeUnits[csd[14] & 7] *
		mmcAccessTimeValues[(csd[14] & 0x8 ? 1 : 0) | (csd[14] & 0x10 ? 2 : 0) | (csd[14] & 0x20 ? 4 : 0) |
		(csd[14] & 0x40 ? 8 : 0)] / 10;
	parameters->accessClocks = 100 * csd[13];
	parameters->maximumClock = mmcClockValues[(csd[12] & 0x8 ? 1 : 0) | (csd[12] & 0x10 ? 2 : 0) |
		(csd[12] & 0x20 ? 4 : 0) | (csd[12] & 0x40 ? 8 : 0)] *
		mmcClockUnits[csd[12] & 7];
	parameters->commandClasses = (csd[10] & 0x10 ? 1 : 0) | (csd[10] & 0x20 ? 2 : 0) |
		(csd[10] & 0x40 ? 4 : 0) | (csd[10] & 0x80 ? 8 : 0) | (csd[11] & 0x1 ? 16 : 0) |
		(csd[11] & 0x2 ? 32 : 0) | (csd[11] & 0x4 ? 64 : 0) | (csd[11] & 0x8 ? 128 : 0);
	parameters->readToWriteFactor = (csd[3] & 0x4 ? 1 : 0) | (csd[3] & 0x8 ? 2 : 0) | (csd[3] & 0x10 ? 4 : 0);
	parameters->writeBlockLength = (csd[2] & 0x40 ? 1 : 0) | (csd[2] & 0x80 ? 2 : 0) |
		(csd[3] & 0x1 ? 4 : 0) | (csd[3] & 0x2 ? 8 : 0);
	parameters->readBlockLength = csd[10] & 15;
	parameters->readBlockPartial = (csd[9] & 0x80) != 0;
	parameters->writeBlockMisalign = (csd[9] & 0x40) != 0;
	parameters->readBlockMisalign = (csd[9] & 0x20) != 0;
	parameters->writeBlockPartial = (csd[2] & 0x20) != 0;
	parameters->capacity = (uint64_t)parameters->blockCount << parameters->readBlockLength;
	eraseSize = (csd[4] & 0x20 ? 1 : 0) | (csd[4] & 0x40 ? 2 : 0) | (csd[4] & 0x80 ? 4 : 0) |
		(csd[5] & 0x1 ? 8 : 0) | (csd[5] & 0x2 ? 16 : 0);
	eraseMultiplier = (csd[5] & 0x4 ? 1 : 0) | (csd[5] & 0x8 ? 2 : 0) | (csd[5] & 0x10 ? 4 : 0) |
		(csd[5] & 0x20 ? 8 : 0) | (csd[5] & 0x40 ? 16 : 0);
	parameters->eraseGroupSize = parameters->writeBlockLength *
		(eraseSize + 1 + eraseMultiplier * (eraseSize + 1));
	result = sdifSelectCard(mmc->contextGlobal, 1);
	if (result < 0)
		return result;
	commandContext = mmc->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x2b;
	command->command = 6;
	command->argument = 0x03af0100;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
	sdifFreeCommandLocked(commandContext, command);
	if (result < 0)
		return result;
	commandContext = mmc->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->blockSize = 512;
	command->blockCount = 0;
	command->buffer = mmc->extendedCsd;
	command->size = sizeof(*command);
	command->flags = 0x114;
	command->command = 8;
	command->argument = 0;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
	sdifFreeCommandLocked(commandContext, command);
	if (result < 0)
		return result;
	extendedCsd = mmc->extendedCsd;
	if ((context->port.operationConditions & 0x60000000) == 0x40000000)
		mmc->state |= 1;
	parameters->sectorCount = extendedCsd[212] | (extendedCsd[213] << 8) |
		(extendedCsd[214] << 16) | (extendedCsd[215] << 24);
	if (extendedCsd[196] & 14)
		parameters->highSpeedClock = 52000000;
	else
		parameters->highSpeedClock = extendedCsd[196] & 1 ? 26000000 : 0;
	parameters->eraseGroupDefinition = extendedCsd[175];
	parameters->highCapacityEraseGroupSize = extendedCsd[224] << 19;
	if ((context->port.operationConditions & 0x60000000) == 0x40000000)
		parameters->capacity = (uint64_t)parameters->sectorCount << 9;
	else if (parameters->capacity > 0x20000000000ULL)
		parameters->capacity = 0x20000000000ULL;
	mmc->activeEraseGroupSize = parameters->eraseGroupDefinition ?
		parameters->highCapacityEraseGroupSize : parameters->eraseGroupSize;
	if (sdifLockPort(mmc->contextGlobal) >= 0) {
		if (sdifSetBlockLength(mmc->contextGlobal, 512) == 0)
			mmc->blockLength = 512;
		sdifUnlockPort(mmc->contextGlobal);
	}
	mmc->partitions[0].va = 0;
	mmc->partitions[0].size = parameters->capacity;
	mmc->partitions[0].pa = 0;
	mmc->partitions[0].partitionType = 0;
	mmc->partitions[0].accessType = 0;
	mmc->partitions[1].va = 0;
	mmc->partitions[1].size = 0;
	mmc->partitions[1].pa = 0;
	mmc->partitions[1].partitionType = 1;
	mmc->partitions[1].accessType = 4;
	mmc->partitions[2].va = 0;
	mmc->partitions[2].size = 0;
	mmc->partitions[2].pa = 0;
	mmc->partitions[2].partitionType = 2;
	mmc->partitions[2].accessType = 5;
	mmc->partitions[3].va = 0;
	mmc->partitions[3].size = 0;
	mmc->partitions[3].pa = 0;
	mmc->partitions[3].partitionType = 3;
	mmc->partitions[3].accessType = 6;
	mmc->partitions[4].va = 0;
	mmc->partitions[4].size = 0;
	mmc->partitions[4].pa = 0;
	mmc->partitions[4].partitionType = 4;
	mmc->partitions[4].accessType = 7;
	mmc->partitions[5].va = 0;
	mmc->partitions[5].size = 0;
	mmc->partitions[5].pa = 0;
	mmc->partitions[5].partitionType = 5;
	mmc->partitions[5].accessType = 0;
	mmc->partitions[6].va = 0;
	mmc->partitions[6].size = 0;
	mmc->partitions[6].pa = 0;
	mmc->partitions[6].partitionType = 6;
	mmc->partitions[6].accessType = 1;
	mmc->partitions[7].va = 0;
	mmc->partitions[7].size = 0;
	mmc->partitions[7].pa = 0;
	mmc->partitions[7].partitionType = 7;
	mmc->partitions[7].accessType = 2;
	mmc->partitions[8].va = 0;
	mmc->partitions[8].size = 0;
	mmc->partitions[8].pa = 0;
	mmc->partitions[8].partitionType = 8;
	mmc->partitions[8].accessType = 3;
	mmc->currentPartition = mmc->partitions;
	enhancedStart = 0;
	enhancedSize = 0;
	userSize = parameters->capacity;
	generalSize[0] = 0;
	generalSize[1] = 0;
	generalSize[2] = 0;
	generalSize[3] = 0;
	if (extendedCsd[160]) {
		enhancedUnit = (uint64_t)(extendedCsd[221] * extendedCsd[224]) << 19;
		enhancedStart = extendedCsd[136] + ((uint64_t)(extendedCsd[137] +
			((extendedCsd[138] + (extendedCsd[139] << 8)) << 8)) << 8);
		enhancedSize = enhancedUnit * (extendedCsd[140] +
			(extendedCsd[141] << 8) + (uint64_t)(extendedCsd[142] << 16));
		mmc->partitions[5].size = enhancedSize;
		if (mmc->state & 1)
			enhancedStart <<= 9;
		mmc->partitions[5].pa = enhancedStart;
		if (enhancedStart) {
			userSize = enhancedStart;
			mmc->partitions[0].pa = 0;
		} else {
			userSize = parameters->capacity - enhancedSize;
			mmc->partitions[0].pa = enhancedSize;
		}
		mmc->partitions[0].size = userSize;
		generalSize[0] = enhancedUnit * (extendedCsd[143] +
			(uint64_t)(extendedCsd[144] << 8) + (uint64_t)(extendedCsd[145] << 16));
		generalSize[1] = enhancedUnit * (extendedCsd[146] +
			((uint64_t)extendedCsd[147] << 8) + (uint64_t)(extendedCsd[148] << 16));
		generalSize[2] = enhancedUnit * (extendedCsd[149] +
			(uint64_t)(extendedCsd[150] << 8) + (uint64_t)(extendedCsd[151] << 16));
		generalSize[3] = enhancedUnit * (extendedCsd[152] +
			((uint64_t)extendedCsd[153] << 8) + (uint64_t)(extendedCsd[154] << 16));
		mmc->partitions[1].size = generalSize[0];
		mmc->partitions[2].size = generalSize[1];
		mmc->partitions[3].size = generalSize[2];
		mmc->partitions[4].size = generalSize[3];
		mmc->partitions[6].size = 0;
		mmc->partitions[7].size = 0;
		mmc->partitions[8].size = extendedCsd[168] << 17;
	}
	nextAddress = userSize + enhancedSize;
	if (enhancedStart || !enhancedSize) {
		userStart = 0;
	} else {
		userSize = enhancedStart;
		userStart = enhancedSize;
	}
	mmc->partitions[1].va = nextAddress;
	mmc->partitions[2].va = nextAddress + generalSize[0];
	mmc->partitions[3].va = nextAddress + generalSize[0] + generalSize[1];
	mmc->partitions[4].va = nextAddress + generalSize[0] + generalSize[1] + generalSize[2];
	nextAddress += generalSize[0] + generalSize[1] + generalSize[2] + generalSize[3];
	mmc->partitions[6].va = nextAddress;
	mmc->partitions[7].va = nextAddress;
	mmc->partitions[8].va = nextAddress;
	mmc->partitions[5].va = userSize;
	mmc->partitions[0].va = userStart;
	if (parameters->highSpeedClock) {
		commandContext = mmc->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->size = sizeof(*command);
		command->flags = 0x2b;
		command->command = 6;
		command->argument = 0x03b90100;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		sdifFreeCommandLocked(commandContext, command);
		if (result < 0)
			return result;
		result = sdifSetClock(context, parameters->highSpeedClock, 1);
	} else {
		result = sdifSetClock(context, parameters->maximumClock, 0);
	}
	if (result < 0)
		return result;
	if (context->port.index != 0) {
		if (parameters->specificationVersion <= 3)
			goto complete;
		commandContext = mmc->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->size = sizeof(*command);
		command->flags = 0x2b;
		command->command = 6;
		command->argument = 0x03b70100;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		sdifFreeCommandLocked(commandContext, command);
		if (result < 0)
			return result;
		result = sdifSetBusWidth(context, 4);
	} else {
		if (parameters->specificationVersion <= 3)
			goto complete;
		commandContext = mmc->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->size = sizeof(*command);
		command->flags = 0x2b;
		command->command = 6;
		command->argument = 0x03b70200;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		sdifFreeCommandLocked(commandContext, command);
		if (result < 0)
			return result;
		result = sdifSetBusWidth(context, 8);
	}
	if (result < 0)
		return result;
complete:
	context->port.part = mmc;
	return 0;
}

int ksceSdifInitializeMmcDevice(unsigned int index, SdifMmcContext **part)
{
	SdifMmcContext *mmc = NULL;
	int result;

	if (part)
		*part = NULL;
	result = sdifStartMmcInitialization(index, &mmc);
	if (result < 0)
		return result;
	result = sdifCompleteMmcInitialization(index);
	if (result < 0)
		return result;
	if (part)
		*part = mmc;
	return 0;
}

SdifMmcContext *ksceSdifGetSdContextPartValidateMmc(unsigned int index)
{
	SdifContextGlobal *context = ksceSdifGetSdContextGlobal(index);

	if (!context)
		return NULL;
	if (context->port.deviceType == 1)
		return context->port.part;
	return NULL;
}

int ksceSdifGetCardInsertState1(unsigned int index)
{
	SdifContextGlobal *context = ksceSdifGetSdContextGlobal(index);

	if (!context)
		return 0x80320013;
	return sdifIsCardInserted(context) > 0;
}

int ksceSdifGetCID(SdifMmcContext *part, void *cid)
{
	if (!part || !cid)
		return 0x8032000e;
	__builtin_memcpy(cid, &part->cid, sizeof(part->cid));
	return 0;
}
