/* SceSdif SD card access (Sony source: sd.c). */
#include "sdif_internal.h"
#include <sysmem/debug.h>
#include <string.h>
#include <threadmgr/threadmgr.h>

SdifContextPart gSdContexts[3];

static const uint8_t sdClockValues[16] = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};
static const uint32_t sdAllocationUnitBlocks[16] = {
	0, 32, 64, 128, 256, 512, 1024, 2048,
	4096, 8192, 16384, 24576, 32768, 49152, 65536, 131072,
};
static const uint32_t sdClockUnits[8] = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};

int SceSdifForDriver_C1271539(unsigned int index, SdifContextPart **part)
{
	SdifContextGlobal *context, *commandContext;
	SdifContextPart *sd;
	SdifSdCardParameters *parameters;
	SdifCommand *command;
	const uint8_t *csd, *scr;
	uint8_t *ssr;
	uint8_t buffer[64], powerControl, mask;
	uint32_t voltages, relativeCardAddress, cardSize, multiplier, bits;
	int result, byte, bit;

	if (part)
		*part = NULL;
	context = SceSdifForDriver_DC8F52F8(index);
	if (!context)
		return 0x80320013;
	if (sceSdifReset(context, 2) < 0)
		return 0x80320015;
	if (sdifIsCardInserted(context) <= 0)
		return 0x8032001a;
	voltages = context->port.voltages;
	if (voltages & 0x300000)
		powerControl = 14;
	else if (voltages & 0x60000)
		powerControl = 12;
	else if (voltages & 0x80)
		powerControl = 10;
	else
		powerControl = 0;
	if (sdifPowerOn(context, powerControl) < 0)
		return 0x80320014;
	sd = &gSdContexts[context->port.index];
	memset(sd, 0, sizeof(*sd));
	sd->state = 0;
	sd->blockLength = 512;
	sd->contextGlobal = context;
	context->port.deviceType = 0;
	context->port.part = NULL;
	context->port.relativeCardAddress = 0;
	if (sdifIdentifyCard(context) < 0)
		return 0x80320016;
	if (context->port.deviceType != 2)
		return 0x80320017;
	result = sdifReadCid(sd->contextGlobal, sd->info.cid.word);
	if (result < 0)
		return result;
	commandContext = sd->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x82;
	command->command = 3;
	command->argument = 0;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
	if (result < 0)
		goto freeError;
	relativeCardAddress = command->response[0] >> 16;
	sdifFreeCommandLocked(commandContext, command);
	context->port.relativeCardAddress = relativeCardAddress;
	if (!relativeCardAddress) {
		commandContext = sd->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->size = sizeof(*command);
		command->flags = 0x82;
		command->command = 3;
		command->argument = 0;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		if (result < 0)
			goto freeError;
		relativeCardAddress = command->response[0] >> 16;
		sdifFreeCommandLocked(commandContext, command);
	}
	context->port.relativeCardAddress = relativeCardAddress;
	result = sdifSelectCard(sd->contextGlobal, 1);
	if (result < 0)
		return result;
	commandContext = sd->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x13;
	command->command = 42;
	command->argument = 0;
	result = sdifSendCheckedApplicationCommand(sd->contextGlobal, command, 3);
	if (result < 0)
		goto freeError;
	result = sdifGetCardStatusError(command->response[0]);
	if (result < 0)
		goto freeError;
	sdifFreeCommandLocked(commandContext, command);
	commandContext = sd->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->buffer = buffer;
	command->flags = 0x114;
	command->command = 13;
	command->blockSize = 64;
	command->argument = 0;
	command->blockCount = 0;
	result = sdifSendCheckedApplicationCommand(sd->contextGlobal, command, 3);
	if (result < 0)
		goto freeError;
	ssr = (uint8_t *)sd->info.ssr;
	for (byte = 0; byte < 63; byte++)
		ssr[byte] = buffer[63 - byte];
	ssr[63] = 0;
	sdifFreeCommandLocked(commandContext, command);
	parameters = &sd->parameters;
	parameters->dataBusWidth = ssr[63] >> 6;
	parameters->securedMode = (ssr[63] & 0x20) != 0;
	parameters->sdCardType = ssr[60];
	bits = 0;
	mask = 1;
	byte = 56;
	for (bit = 0; bit < 32; bit++) {
		if (ssr[byte] & mask)
			bits |= 1u << bit;
		mask *= 2;
		if (!mask) {
			byte++;
			mask = 1;
		}
	}
	parameters->protectedAreaSize = bits;
	parameters->speedClass = ssr[55];
	parameters->performanceMove = ssr[54];
	parameters->allocationUnitSize = ssr[53] >> 4;
	parameters->eraseSize = ssr[51];
	parameters->eraseTimeout = ssr[50] >> 2;
	parameters->eraseOffset = ssr[50] & 3;
	sdifSelectCard(sd->contextGlobal, 0);
	result = sdifReadCsd(sd->contextGlobal, sd->info.csd.word);
	if (result < 0)
		return result;
	result = sdifSelectCard(sd->contextGlobal, 1);
	if (result < 0)
		return result;
	csd = (const uint8_t *)&sd->info.csd;
	parameters->csdStructure = (csd[15] & 0x80 ? 2 : 0) |
		((csd[15] & 0x40) != 0);
	if (parameters->csdStructure == 0) {
		parameters->maximumClock = sdClockUnits[csd[12] & 7] *
			sdClockValues[(csd[12] >> 3) & 15];
		parameters->commandClasses = (csd[10] >> 4) | (csd[11] << 4);
		parameters->readBlockLength = csd[10] & 15;
		cardSize = (csd[7] >> 6) | (csd[8] << 2) | ((csd[9] & 3) << 10);
		multiplier = (csd[5] >> 7) | ((csd[6] & 3) << 1);
		parameters->blockCount = (cardSize + 1) << (multiplier + 2);
		if (csd[5] & 0x40) {
			parameters->eraseSectorSize = 512;
			parameters->eraseGroupSize = 0x2000 << parameters->allocationUnitSize;
		} else {
			parameters->eraseSectorSize = (((csd[4] >> 7) |
				((csd[5] & 0x3f) << 1)) + 1) << parameters->readBlockLength;
			parameters->eraseGroupSize = parameters->eraseSectorSize;
		}
	} else if (parameters->csdStructure == 1) {
		parameters->maximumClock = sdClockUnits[csd[12] & 7] *
			sdClockValues[(csd[12] >> 3) & 15];
		parameters->commandClasses = (csd[10] >> 4) | (csd[11] << 4);
		cardSize = 0;
		mask = 1;
		byte = 6;
		for (bit = 0; bit < 22; bit++) {
			if (csd[byte] & mask)
				cardSize |= 1u << bit;
			mask *= 2;
			if (!mask) {
				byte++;
				mask = 1;
			}
		}
		parameters->blockCount = (cardSize + 1) << 10;
		parameters->readBlockLength = csd[10] & 15;
		parameters->eraseSectorSize = 512;
		parameters->eraseGroupSize = 0x2000 << parameters->allocationUnitSize;
		if (context->port.operationConditions & 0x40000000)
			sd->state |= 1;
	} else {
		return 0x80320013;
	}
	parameters->capacity = (uint64_t)parameters->blockCount << parameters->readBlockLength;
	if (parameters->capacity > 0x20000000000ULL)
		parameters->capacity = 0x20000000000ULL;
	commandContext = sd->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->buffer = &sd->info.scr;
	command->flags = 0x114;
	command->command = 51;
	command->argument = 0;
	command->blockSize = 8;
	command->blockCount = 0;
	result = sdifSendCheckedApplicationCommand(sd->contextGlobal, command, 3);
	if (result < 0)
		goto freeError;
	sdifFreeCommandLocked(commandContext, command);
	scr = (const uint8_t *)&sd->info.scr;
	parameters->sdSpecification = scr[0] & 15;
	parameters->sdBusWidths = scr[1] & 15;
	parameters->scrStructure = scr[0] >> 4;
	parameters->dataStatusAfterErase = scr[1] >> 7;
	parameters->sdSecurity = (scr[1] >> 4) & 7;
	if ((scr[0] & 15) == 2 && scr[2] >> 7)
		parameters->sdSpecification = 3;
	parameters->additionalScrFields = scr[3] | ((scr[2] << 8) & 0x3fff);
	result = sdifSetBlockLength(sd->contextGlobal, 512);
	if (result == 0)
		sd->blockLength = 512;
	else if (result < 0)
		return result;
	if (parameters->commandClasses & 0x400) {
		commandContext = sd->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->blockSize = 64;
		command->buffer = buffer;
		command->blockCount = 0;
		command->size = sizeof(*command);
		command->flags = 0x114;
		command->command = 6;
		command->argument = 0x00fffff1;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		if (result < 0)
			goto freeError;
		result = sdifGetCardStatusError(command->response[0]);
		if (result < 0)
			goto freeError;
		sdifFreeCommandLocked(commandContext, command);
		if (buffer[13] & 2)
			parameters->highSpeedClock = 50000000;
		if (!(parameters->commandClasses & 0x400) || !parameters->highSpeedClock)
			goto normalClock;
		commandContext = sd->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->blockSize = 64;
		command->buffer = buffer;
		command->blockCount = 0;
		command->size = sizeof(*command);
		command->flags = 0x114;
		command->command = 6;
		command->argument = 0x80fffff1;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		if (result < 0)
			goto freeError;
		result = sdifGetCardStatusError(command->response[0]);
		if (result < 0)
			goto freeError;
		sdifFreeCommandLocked(commandContext, command);
		if ((buffer[16] & 15) == 1) {
			result = sdifSetClock(context, parameters->highSpeedClock, 1);
			if (result < 0)
				return result;
		}
		goto busWidth;
	}
normalClock:
	result = sdifSetClock(context, parameters->maximumClock, 0);
	if (result < 0)
		return result;
busWidth:
	if (parameters->sdBusWidths & 4) {
		commandContext = sd->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command)
			return 0x80320011;
		command->buffer = NULL;
		command->size = sizeof(*command);
		command->flags = 0x13;
		command->command = 6;
		command->argument = 2;
		result = sdifSendCheckedApplicationCommand(sd->contextGlobal, command, 3);
		if (result < 0)
			goto freeError;
		result = sdifGetCardStatusError(command->response[0]);
		if (result < 0)
			goto freeError;
		sdifFreeCommandLocked(commandContext, command);
		result = sdifSetBusWidth(context, 4);
		if (result < 0)
			return result;
	}
	sd->writeProtected = sdifIsWriteProtected(context);
	context->port.part = sd;
	if (part)
		*part = sd;
	return 0;

freeError:
	sdifFreeCommandLocked(commandContext, command);
	return result;
}

SdifContextPart *SceSdifForDriver_B9EA5B1E(unsigned int index)
{
	SdifContextGlobal *context = SceSdifForDriver_DC8F52F8(index);

	if (!context)
		return NULL;
	if (context->port.deviceType == 2)
		return context->port.part;
	return NULL;
}

int SceSdifForDriver_35BA9DF8(unsigned int index, SceUID callback)
{
	SdifContextGlobal *context = SceSdifForDriver_DC8F52F8(index);
	SdifContextPart *sd;
	SdifSdCardParameters *parameters;
	SdifCommand *command;
	uint32_t batchSize, sector = 0, remaining, blockCount, count;
	int groups = 10, timeout, result;
	int byteAddressed;

	if (!context)
		return 0x80320001;
	sd = context->port.part;
	if (!sd)
		return 0x80320013;
	if (context->port.deviceType != 2)
		return 0x80320017;
	parameters = &sd->parameters;
	if (!(parameters->commandClasses & 0x20) || parameters->csdStructure > 1)
		return 0x80320013;
	if (parameters->eraseSize && parameters->eraseTimeout) {
		timeout = 2700 - 1000 * parameters->eraseOffset;
		groups = 1;
		if (timeout > 0) {
			groups = (uint32_t)(parameters->eraseSize * timeout) /
				 (1000 * parameters->eraseTimeout);
			if (!groups)
				groups = 1;
		}
	}
	if (parameters->allocationUnitSize)
		batchSize = sdAllocationUnitBlocks[parameters->allocationUnitSize] * groups;
	else
		batchSize = 32 * groups;
	byteAddressed = !(sd->state & 1);
	result = sdifLockPort(context);
	if (result < 0)
		return result;
	command = sdifAllocateCommandLocked(context);
	if (!command) {
		sdifUnlockPort(context);
		return 0x80320011;
	}
	blockCount = parameters->blockCount;
	remaining = blockCount;
	command->size = sizeof(*command);
	command->buffer = NULL;
	if (blockCount) {
		for (;;) {
			command->command = 32;
			count = sector + batchSize >= blockCount ? blockCount - sector : batchSize;
			command->flags = 0x13;
			command->argument = byteAddressed ? sector << 9 : sector;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
			if (result < 0)
				break;
			result = sdifGetCardStatusError(command->response[0]);
			if (result < 0)
				break;
			command->flags = 0x13;
			command->command = 33;
			command->argument = byteAddressed ?
				(sector + (UINT32_MAX >> 9) + count) << 9 : sector - 1 + count;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
			if (result < 0)
				break;
			result = sdifGetCardStatusError(command->response[0]);
			if (result < 0)
				break;
			command->flags = 0x2b;
			command->argument = 0;
			command->command = 38;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
			if (result < 0)
				break;
			result = sdifGetCardStatusError(command->response[0]);
			if (result < 0)
				break;
			sector += count;
			remaining -= count;
			if (callback != -1) {
				uint32_t total = parameters->blockCount;
				uint32_t progress = total;

				if (total)
					progress = (uint32_t)(100ULL * (total - remaining) / total) | 0x640000;
				if (sceKernelNotifyCallback(callback, progress) == (int)0x800280a1)
					callback = -1;
			}
			if (!remaining)
				break;
			blockCount = parameters->blockCount;
		}
	}
	sdifFreeCommandLocked(context, command);
	sdifUnlockPort(context);
	return result;
}

int SceSdifForDriver_FD9E5CFA(unsigned int index)
{
	SdifContextGlobal *context = SceSdifForDriver_DC8F52F8(index);

	if (context)
		return sdifIsCardInserted(context) > 0;
	return 0x80320013;
}

int SceSdifForDriver_AF702FE7(SdifContextPart *sd, uint32_t blockLength)
{
	int result = sdifSetBlockLength(sd->contextGlobal, blockLength);

	if (!result)
		sd->blockLength = blockLength;
	return result;
}

int SceSdifForDriver_B9593652(SdifContextPart *sd, uint32_t sector,
			void *buffer, uint32_t count)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t status, state;
	uint16_t blockLength;
	int result;

	if (!buffer || !sd || !count)
		return 0x8032000e;
	context = sd->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	state = sd->state;
	if (count == 1) {
		command->flags = 0x514;
		command->command = 17;
	} else {
		command->flags = 0xd14;
		command->command = 18;
	}
	blockLength = sd->blockLength;
	command->blockCount = count;
	command->argument = state & 1 ? sector : sector << 9;
	command->size = sizeof(*command);
	command->buffer = buffer;
	command->blockSize = blockLength;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	context->port.lastTransferTime = command->endTime - command->startTime;
	result = SceSdifForDriver_53518827(sd->contextGlobal, &status);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	sdifFreeCommandLocked(context, command);
	return 0;
}

int sceSdWriteSectorInternal(SdifContextPart *sd, uint32_t sector,
			    const void *buffer, uint32_t count)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t status;
	int result;

	if (!buffer || !sd || !count)
		return 0x8032000e;
	if (sd->writeProtected)
		return 0x8032001c;
	context = sd->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	if (count == 1) {
		command->flags = 0x614;
		command->command = 24;
	} else {
		command->flags = 0xe14;
		command->command = 25;
	}
	command->size = sizeof(*command);
#if FW_DEBUG
	SceDebugForDriver_35A35322(1, SCE_DBGINFO(1413), __builtin_return_address(0));
#endif
	command->blockCount = count;
	command->buffer = (void *)buffer;
	command->blockSize = sd->blockLength;
	command->argument = sd->state & 1 ? sector : sector << 9;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	context->port.lastTransferTime = command->endTime - command->startTime;
	result = SceSdifForDriver_53518827(sd->contextGlobal, &status);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	sdifFreeCommandLocked(context, command);
	return 0;
}

int SceSdifForDriver_0203ECDC(SdifContextPart *sd, uint32_t sector,
			    uint32_t count)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t transferCount, status;
	uint16_t maximumCount;
	int result;

	if (!count || !sd)
		return 0x8032000e;
	maximumCount = 16 * (4096U / sd->blockLength);
	for (;;) {
		transferCount = count;
		if (transferCount >= maximumCount)
			transferCount = maximumCount;
		if (sd->writeProtected)
			return 0x8032001c;
		context = sd->contextGlobal;
		command = sdifAllocateCommandLocked(context);
		if (!command) {
			result = 0x80320011;
			goto retry;
		}
		command->flags = transferCount == 1 ? 0x614 : 0xe14;
		command->command = transferCount == 1 ? 24 : 25;
		command->size = sizeof(*command);
#if FW_DEBUG
		SceDebugForDriver_35A35322(1, SCE_DBGINFO_FUNC(1413, "sceSdWriteSectorInternal"),
				 __builtin_return_address(0));
#endif
		command->blockCount = transferCount;
		command->blockSize = sd->blockLength;
		command->flags |= 0x1000000;
		command->buffer = NULL;
		command->argument = sd->state & 1 ? sector : sector << 9;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		if (result >= 0) {
			context->port.lastTransferTime = command->endTime - command->startTime;
			result = SceSdifForDriver_53518827(sd->contextGlobal, &status);
			if (result >= 0) {
				sdifFreeCommandLocked(context, command);
				count = (uint16_t)(count - transferCount);
				sector += transferCount;
				if (!count)
					return 0;
				continue;
			}
		}
		sdifFreeCommandLocked(context, command);
		if (result != (int)0x80320011)
			return result;
retry:
		if (!count)
			return result;
	}
}

int SceSdifForDriver_E091BA2E(SdifContextPart *sd, uint32_t sector,
			    uint32_t count, int *writeProtected)
{
	int invalid = count == 0;

	if (count)
		invalid = sd == NULL;
	if (invalid || !writeProtected)
		return 0x8032000e;
	if (sd->writeProtected) {
		*writeProtected = 1;
		return 0;
	}
	*writeProtected = 0;
	return 0;
}
