/* SceSdif SDIO card access (Sony source: sdio.c). */
#include "sdif_internal.h"
#include <string.h>

int sdifSendAsyncCommand(SdifContextGlobal *context, SdifCommand *command,
			void (*callback)(SdifCommand *), SdifCommand *secondary,
			void (*secondaryCallback)(SdifCommand *));
SdifCommand *sdifRemovePendingCommand(SdifContextGlobal *context, uint32_t index);
int sdifWaitPendingCommand(SdifContextGlobal *context, uint32_t index,
			  uint32_t reserved, SdifCommand **completed);
int sdifGetCommandState(SdifContextGlobal *context, uint32_t index);
int sdifDeselectCard(SdifContextGlobal *context);

SdifSdioContext gSdioContexts[3];

static const int sdioClockHighSpeed[3] __attribute__((aligned(16))) = {
	0, 0, 1,
};

static const uint32_t sdioClockFrequencies[3] __attribute__((aligned(16))) = {
	400000, 25000000, 50000000,
};

#if FW_VERSION >= 0x04000000
static const uint32_t sdioClockUnits[8] __attribute__((aligned(16))) = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};
#endif

static const uint32_t sdioClockValues[16] __attribute__((aligned(16))) = {
	0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80,
};

#if FW_VERSION < 0x04000000
static const uint32_t sdioClockUnits[8] __attribute__((aligned(16))) = {
	10000, 100000, 1000000, 10000000, 0, 0, 0, 0,
};
#endif

void sdifCompleteIoCommand(SdifCommand *command)
{
	SdifIoResult result;
	uint32_t status;

	if (command->ioTransfer.completion) {
		result.errorCode = command->errorCode;
		status = command->response[0];
		if (!result.errorCode) {
			if (status & 0x8000)
				result.errorCode = 0x80320154;
			else if (status & 0x4000)
				result.errorCode = 0x80320153;
			else if (status & 0x800)
				result.errorCode = 0x80320152;
			else if (status & 0x200)
				result.errorCode = 0x80320151;
			else if (status & 0x100)
				result.errorCode = 0x80320150;
		}
		result.index = command->ioTransfer.index;
		result.argument = command->ioTransfer.argument;
		result.response[0] = status;
		result.response[1] = command->response[1];
		result.response[2] = command->response[2];
		result.response[3] = command->response[3];
		sdifFreeCommand(command->context, command);
		command->ioTransfer.completion(&result);
	} else {
		sdifFreeCommand(command->context, command);
	}
}

int sdifIoReadWriteDirect(SdifContextGlobal *context, uint32_t address,
			 uint32_t data, uint32_t flags)
{
	SdifCommand *command = sdifAllocateCommandLocked(context);
	uint32_t status;
	int result;

	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = ((address & 0x1ffff) << 9) | (flags << 31) |
		data | ((flags & 2) << 26);
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0)
		goto done;
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	result = (uint8_t)status;
done:
	sdifFreeCommandLocked(context, command);
	return result;
}

int sdifIoReadWriteExtended(SdifSdioContext *sdio, int number, uint32_t address,
			    void *buffer, uint32_t size, uint8_t flags)
{
	SdifContextGlobal *context = sdio->contextGlobal;
	SdifCommand *command = sdifAllocateCommandLocked(context);
	SdifCommand *secondary;
	SdifSdioFunction *function = &sdio->functions[number];
	uint32_t length = size, blockSize, blocks, transferred, remaining;
	uint32_t status, secondaryStatus = 0, argument;
	int result;

	if (!command)
		return 0x80320011;
	if (size >> 16)
		length = (uint16_t)size * (size >> 16);
	blockSize = function->blockSize;
	if (length <= blockSize) {
		command->buffer = buffer;
		command->blockSize = length;
		command->size = sizeof(*command);
		command->flags = flags & 1 ? 0x664 : 0x564;
		command->command = 53;
		command->blockCount = 0;
		command->secondaryCommand = NULL;
		command->argument = ((function->number & 7) << 28) |
			((address & 0x1ffff) << 9) | ((!!(flags & 2)) << 26) |
			(length & 0x1ff) | ((uint32_t)(flags & 1) << 31);
	} else {
		blocks = length / blockSize;
		command->blockCount = blocks;
		command->blockSize = blockSize;
		command->buffer = buffer;
		command->flags = flags & 1 ? 0x664 : 0x564;
		transferred = blockSize * blocks;
		argument = ((address & 0x1ffff) << 9) |
			((function->number & 7) << 28) | 0x8000000 |
			((!!(flags & 2)) << 26) | (blocks & 0x1ff);
		command->size = sizeof(*command);
		command->command = 53;
		remaining = length - transferred;
		command->argument = argument | ((uint32_t)(flags & 1) << 31);
		command->secondaryCommand = NULL;
		if (remaining) {
			secondary = sdifAllocateCommandLocked(context);
			argument = (address & 0x1ffff) << 9;
			if (!secondary) {
				sdifFreeCommandLocked(context, command);
				return 0x80320011;
			}
			if (!(flags & 2))
				argument = ((transferred + address) & 0x1ffff) << 9;
			secondary->buffer = (uint8_t *)buffer + transferred;
			secondary->blockSize = remaining;
			secondary->flags = flags & 1 ? 0x664 : 0x564;
			secondary->argument = ((!!(flags & 2)) << 26) |
				((function->number & 7) << 28) | (remaining & 0x1ff) |
				argument | ((uint32_t)(flags & 1) << 31);
			secondary->command = 53;
			secondary->size = sizeof(*secondary);
			secondary->blockCount = 0;
			secondary->secondaryCommand = NULL;
			result = sdifSendCheckedSyncCommand(context, command, secondary, 0);
			if (result < 0) {
				sdifFreeCommandLocked(context, secondary);
				goto commandError;
			}
			status = command->response[0];
			secondaryStatus = secondary->response[0];
			sdifFreeCommandLocked(context, secondary);
			goto decodeStatus;
		}
	}
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0)
		goto commandError;
	status = command->response[0];
	secondary = NULL;
decodeStatus:
	sdifFreeCommandLocked(context, command);
	if (status & 0x8000)
		return 0x80320154;
	if (status & 0x4000)
		return 0x80320153;
	if (status & 0x800)
		return 0x80320152;
	if (status & 0x200)
		return 0x80320151;
	if (status & 0x100)
		return 0x80320150;
	if (!secondary)
		return 0;
	if (secondaryStatus & 0x8000)
		return 0x80320154;
	if (secondaryStatus & 0x4000)
		return 0x80320153;
	if (secondaryStatus & 0x800)
		return 0x80320152;
	if (secondaryStatus & 0x200)
		return 0x80320151;
	if (secondaryStatus & 0x100)
		return 0x80320150;
	return 0;
commandError:
	sdifFreeCommandLocked(context, command);
	return result;
}

int sdifConfigureIoBusWidth(SdifContextGlobal *context, SdifSdioContext *sdio)
{
	uint8_t value;

	value = sdifIoReadWriteDirect(sdio->contextGlobal, 4, 0, 0);
	sdifIoReadWriteDirect(sdio->contextGlobal, 4, value & 0xfe, 1);
	value = sdifIoReadWriteDirect(sdio->contextGlobal, 7, 0, 0);
	sdifIoReadWriteDirect(sdio->contextGlobal, 7, (value & 0xfc) | 2, 1);
	sdifSetBusWidth(context, 4);
	value = sdifIoReadWriteDirect(sdio->contextGlobal, 4, 0, 0);
	sdifIoReadWriteDirect(sdio->contextGlobal, 4, value | 1, 1);
	return 0;
}

int sdifReadIoCis(SdifSdioContext *sdio, uint8_t *cis, uint32_t address)
{
	uint32_t cursor = address, length, position, next, payload;
	int total = 0, result;

	for (;;) {
		result = sdifIoReadWriteDirect(sdio->contextGlobal, cursor, 0, 0);
		if (result < 0)
			return result;
		if ((uint8_t)result == 0xff) {
			length = total + 1;
			break;
		}
		result = sdifIoReadWriteDirect(sdio->contextGlobal, cursor + 1, 0, 0);
		if (result < 0)
			return result;
		total += 2;
		if ((uint8_t)result != 0xff) {
			total += (uint8_t)result;
			cursor += (uint8_t)result + 2;
			if (total <= 255)
				continue;
		}
		length = total;
		break;
	}
	if (length > 256)
		return 0x80320016;
	cursor = address;
	position = 0;
	while (position < length) {
		result = sdifIoReadWriteDirect(sdio->contextGlobal, cursor, 0, 0);
		if (result < 0)
			return result;
		cis[position] = result;
		next = position + 1;
		if ((uint8_t)result == 0xff)
			return next;
		if (length <= next)
			return length;
		result = sdifIoReadWriteDirect(sdio->contextGlobal, cursor + 1, 0, 0);
		if (result < 0)
			return result;
		cis[next] = result;
		payload = (uint8_t)result;
		next = position + 2;
		if (payload == 0xff)
			return next;
		if (length < next + payload)
			return length;
		for (uint32_t index = 0; index < payload; index++) {
			result = sdifIoReadWriteDirect(sdio->contextGlobal,
						   cursor + 2 + index, 0, 0);
			if (result < 0)
				return result;
			cis[next + index] = result;
		}
		next += payload;
		cursor += payload + 2;
		if (length <= next)
			return next;
		position = next;
	}
	return length;
}

int sdifReadIoFunctionCis(SdifSdioContext *sdio, int number)
{
	uint32_t base = number << 8;
	int high, middle, low, result;

	high = sdifIoReadWriteDirect(sdio->contextGlobal, base + 11, 0, 0);
	if (high < 0)
		return high;
	middle = sdifIoReadWriteDirect(sdio->contextGlobal, base + 10, 0, 0);
	if (middle < 0)
		return middle;
	low = sdifIoReadWriteDirect(sdio->contextGlobal, base + 9, 0, 0);
	if (low < 0)
		return low;
	sdio->functions[number].cisLength = 0;
	result = sdifReadIoCis(sdio, sdio->functions[number].cis,
		(uint8_t)low | (((uint8_t)middle | ((uint8_t)high << 8)) << 8));
	if (result >= 0)
		sdio->functions[number].cisLength = result;
	return result;
}

int SceSdifForDriver_3428884D(SdifSdioFunction *function)
{
	SdifContextGlobal *context = function->context->contextGlobal;
	int result = sdifLockPort(context);

	if (result < 0)
		return result;
	result = sdifIoReadWriteDirect(function->context->contextGlobal, 2, 0, 0);
	if (result >= 0) {
		result = sdifIoReadWriteDirect(function->context->contextGlobal, 2,
			(uint8_t)(result | (1U << function->number)), 1);
		if (result >= 0) {
			for (;;) {
				result = sdifIoReadWriteDirect(function->context->contextGlobal,
							   3, 0, 0);
				if (result < 0)
					break;
				if ((uint8_t)result & (1U << function->number)) {
					result = 0;
					break;
				}
				sdifDelay(context, 1000);
			}
		}
	}
	sdifUnlockPort(context);
	return result;
}

int SceSdifForDriver_E80293EF(SdifSdioFunction *function)
{
	SdifContextGlobal *context = function->context->contextGlobal;
	int result = sdifLockPort(context);

	if (result < 0)
		return result;
	result = sdifIoReadWriteDirect(function->context->contextGlobal, 2, 0, 0);
	if (result >= 0)
		result = sdifIoReadWriteDirect(function->context->contextGlobal, 2,
			(uint8_t)result & ~(1U << function->number), 1);
	sdifUnlockPort(context);
	return result;
}

int SceSdifForDriver_55BAEB2D(SdifSdioFunction *function)
{
	SdifContextGlobal *context = function->context->contextGlobal;
	SdifCommand *command = sdifAllocateCommandLocked(context);
	uint32_t status;
	int result;

	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = 3 << 9;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	sdifFreeCommandLocked(context, command);
	return !!((1 << function->number) & (uint8_t)status);
}

int SceSdifForDriver_FE6F3E7B(SdifSdioFunction *function)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t status;
	int result;

	if (!function)
		return 0;
	context = function->context->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = 14 << 9;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	sdifFreeCommandLocked(context, command);
	return !!((uint8_t)status & (1 << function->number));
}

int SceSdifForDriver_F1A24EDD(SdifSdioFunction *function)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t status;
	int result;

	if (!function)
		return 0;
	context = function->context->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = 15 << 9;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	sdifFreeCommandLocked(context, command);
	return !!((uint8_t)status & (1 << function->number));
}

int SceSdifForDriver_D0F78D9B(SdifSdioFunction *function, uint32_t address,
			    uint32_t size, uint8_t *buffer, uint32_t flags)
{
	uint8_t *end;
	SdifSdioContext *sdio;
	uint32_t number, status;
	int result;

	if (flags & 1) {
		end = buffer + size;
		if (buffer >= end)
			return 0;
		number = (function->number & 7) << 28;
		sdio = function->context;
		for (;;) {
			SdifContextGlobal *context = sdio->contextGlobal;
			SdifCommand *command = sdifAllocateCommandLocked(context);

			if (!command)
				return 0x80320011;
			command->buffer = NULL;
			command->size = sizeof(*command);
			command->flags = 0x63;
			command->command = 52;
			command->argument = number | ((address & 0x1ffff) << 9);
			address += ((flags ^ 2) & 2) != 0;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
			if (result < 0) {
				sdifFreeCommandLocked(context, command);
				return result;
			}
			status = command->response[0];
			result = (uint8_t)status;
			if (status & 0x8000) {
				sdifFreeCommandLocked(context, command);
				return 0x80320154;
			}
			if (status & 0x4000) {
				sdifFreeCommandLocked(context, command);
				return 0x80320153;
			}
			if (status & 0x800) {
				sdifFreeCommandLocked(context, command);
				return 0x80320152;
			}
			if (status & 0x200) {
				sdifFreeCommandLocked(context, command);
				return 0x80320151;
			}
			if (status & 0x100) {
				sdifFreeCommandLocked(context, command);
				return 0x80320150;
			}
			sdifFreeCommandLocked(context, command);
			*buffer++ = result;
			if (buffer == end)
				return result;
		}
	}
	if (!(flags & 2))
		return sdifIoReadWriteExtended(function->context, function->number,
			address, buffer, size, 2);
	return sdifIoReadWriteExtended(function->context, function->number,
		address, buffer, size, flags & 1);
}

int SceSdifForDriver_3C4CDC8B(SdifSdioFunction *function, uint32_t address,
			    uint32_t size, uint8_t *buffer, uint32_t flags)
{
	uint8_t *end, *cursor;
	SdifSdioContext *sdio;
	uint32_t number, status, data;
	int result;

	if (flags & 1) {
		end = buffer + size;
		if (buffer >= end)
			return 0;
		cursor = buffer - 1;
		sdio = function->context;
		number = ((function->number & 7) << 28) | 0x80000000;
		end--;
		for (;;) {
			SdifContextGlobal *context;
			SdifCommand *command;

			data = *++cursor;
			context = sdio->contextGlobal;
			command = sdifAllocateCommandLocked(context);
			if (!command)
				return 0x80320011;
			command->buffer = NULL;
			command->size = sizeof(*command);
			command->flags = 0x63;
			command->command = 52;
			command->argument = data | number | ((address & 0x1ffff) << 9);
			address += ((flags ^ 2) & 2) != 0;
			result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
			if (result < 0) {
				sdifFreeCommandLocked(context, command);
				return result;
			}
			status = command->response[0];
			if (status & 0x8000) {
				sdifFreeCommandLocked(context, command);
				return 0x80320154;
			}
			if (status & 0x4000) {
				sdifFreeCommandLocked(context, command);
				return 0x80320153;
			}
			if (status & 0x800) {
				sdifFreeCommandLocked(context, command);
				return 0x80320152;
			}
			if (status & 0x200) {
				sdifFreeCommandLocked(context, command);
				return 0x80320151;
			}
			if (status & 0x100) {
				sdifFreeCommandLocked(context, command);
				return 0x80320150;
			}
			sdifFreeCommandLocked(context, command);
			if (cursor == end)
				return (uint8_t)status;
		}
	}
	return sdifIoReadWriteExtended(function->context, function->number,
		address, buffer, size, flags & 2 ? 1 : 3);
}

int SceSdifForDriver_3B6AB29E(SdifSdioFunction *function,
			    SdifIoTransfer *transfer, uint32_t *index)
{
	SdifContextGlobal *context;
	SdifCommand *command, *secondary = NULL;
	uint32_t blockSize, length, fixedAddress, blocks, transferred, address;
	uint16_t remaining;
	int result;

	if (!transfer || !function || !transfer->completion ||
	    !transfer->buffer || (transfer->flags & 1))
		return 0x8032000e;
	context = function->context->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	blockSize = function->blockSize;
	length = transfer->length;
	fixedAddress = !!(transfer->flags & 2);
	if (length <= blockSize) {
		command->blockSize = length;
		command->buffer = transfer->buffer;
		command->blockCount = 0;
		command->secondaryCommand = NULL;
		command->size = sizeof(*command);
		command->flags = 0x564;
		command->command = 53;
		command->argument = (length & 0x1ff) |
			((transfer->address & 0x1ffff) << 9) | (fixedAddress << 26) |
			((function->number & 7) << 28);
	} else {
		blocks = length / blockSize;
		transferred = blockSize * blocks;
		command->blockCount = blocks;
		command->argument = ((transfer->address & 0x1ffff) << 9) |
			0x8000000 | (blocks & 0x1ff) | (fixedAddress << 26) |
			((function->number & 7) << 28);
		command->flags = 0x564;
		command->command = 53;
		command->secondaryCommand = NULL;
		command->buffer = transfer->buffer;
		remaining = length - transferred;
		command->blockSize = blockSize;
		command->size = sizeof(*command);
		if (length != transferred) {
			secondary = sdifAllocateCommandLocked(context);
			if (!secondary) {
				sdifFreeCommandLocked(context, command);
				return 0x80320011;
			}
			address = transfer->address;
			if (!fixedAddress)
				address += transferred;
			secondary->size = sizeof(*secondary);
			secondary->command = 53;
			secondary->argument = (remaining & 0x1ff) |
				(fixedAddress << 26) | ((function->number & 7) << 28) |
				((address & 0x1ffff) << 9);
			secondary->blockCount = 0;
			secondary->buffer = (uint8_t *)transfer->buffer + transferred;
			secondary->blockSize = remaining;
			secondary->flags = 0x564;
			secondary->secondaryCommand = NULL;
		}
	}
	command->ioTransfer = *transfer;
	command->ioTransfer.index = command->index;
	*index = command->index;
	if (secondary) {
		secondary->ioTransfer = command->ioTransfer;
		command->ioTransfer.completion = NULL;
	}
	result = sdifSendAsyncCommand(context, command, sdifCompleteIoCommand,
		secondary, sdifCompleteIoCommand);
	if (result >= 0)
		return 0;
	if (secondary)
		sdifFreeCommandLocked(context, secondary);
	sdifFreeCommandLocked(context, command);
	return result;
}

int SceSdifForDriver_855C95E1(SdifSdioFunction *function,
			    SdifIoTransfer *transfer, uint32_t *index)
{
	SdifContextGlobal *context;
	SdifCommand *command, *secondary = NULL;
	uint32_t blockSize, length, fixedAddress, blocks, transferred, address;
	uint32_t remaining;
	int result;

	if (!transfer || !function || !transfer->completion ||
	    !transfer->buffer || (transfer->flags & 1))
		return 0x8032000e;
	context = function->context->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	blockSize = function->blockSize;
	length = transfer->length;
	fixedAddress = !!(transfer->flags & 2);
	if (length <= blockSize) {
		command->buffer = transfer->buffer;
		command->blockSize = length;
		command->blockCount = 0;
		command->secondaryCommand = NULL;
		command->size = sizeof(*command);
		command->command = 53;
		command->argument = (transfer->length & 0x1ff) | 0x80000000 |
			((transfer->address & 0x1ffff) << 9) | (fixedAddress << 26) |
			((function->number & 7) << 28);
		command->flags = 0x664;
	} else {
		blocks = length / blockSize;
		transferred = blockSize * blocks;
		command->blockCount = blocks;
		command->buffer = transfer->buffer;
		remaining = length - transferred;
		command->blockSize = blockSize;
		command->secondaryCommand = NULL;
		command->argument = ((transfer->address & 0x1ffff) << 9) |
			0x88000000 | (blocks & 0x1ff) | (fixedAddress << 26) |
			((function->number & 7) << 28);
		command->size = sizeof(*command);
		command->flags = 0x664;
		command->command = 53;
		if (remaining) {
			secondary = sdifAllocateCommandLocked(context);
			if (!secondary) {
				sdifFreeCommandLocked(context, command);
				return 0x80320011;
			}
			address = transfer->address;
			if (!fixedAddress)
				address += transferred;
			secondary->size = sizeof(*secondary);
			secondary->argument = (remaining & 0x1ff) |
				(fixedAddress << 26) | 0x80000000 |
				((function->number & 7) << 28) |
				((address & 0x1ffff) << 9);
			secondary->buffer = (uint8_t *)transfer->buffer + transferred;
			secondary->flags = 0x664;
			secondary->blockSize = remaining;
			secondary->command = 53;
			secondary->blockCount = 0;
			secondary->secondaryCommand = NULL;
		}
	}
	command->ioTransfer = *transfer;
	command->ioTransfer.index = command->index;
	*index = command->index;
	if (secondary) {
		secondary->ioTransfer = command->ioTransfer;
		command->ioTransfer.completion = NULL;
	}
	result = sdifSendAsyncCommand(context, command, sdifCompleteIoCommand,
		secondary, sdifCompleteIoCommand);
	if (result >= 0)
		return 0;
	if (secondary)
		sdifFreeCommandLocked(context, secondary);
	sdifFreeCommandLocked(context, command);
	return result;
}

int SceSdifForDriver_235AD556(void)
{
	return 0x8032000f;
}

int SceSdifForDriver_0F157F49(SdifSdioFunction *function, uint32_t index)
{
	SdifCommand *command, *secondary;

	command = sdifRemovePendingCommand(function->context->contextGlobal, index);
	if (!command)
		return 0x80320012;
	sdifFreeCommandLocked(function->context->contextGlobal, command);
	secondary = command->secondaryCommand;
	if (secondary)
		sdifFreeCommandLocked(function->context->contextGlobal, secondary);
	return 0;
}

int SceSdifForDriver_849E3216(SdifSdioFunction *function, uint32_t index,
			    uint32_t reserved, SdifIoResult *result)
{
	SdifCommand *command = NULL;
	int error = sdifWaitPendingCommand(function->context->contextGlobal,
		index, reserved, &command);

	if (command) {
		result->index = command->ioTransfer.index;
		result->argument = command->ioTransfer.argument;
		result->response[0] = command->response[0];
		result->response[1] = command->response[1];
		result->response[2] = command->response[2];
		result->response[3] = command->response[3];
		sdifFreeCommandLocked(function->context->contextGlobal, command);
	}
	result->errorCode = error;
	return error;
}

int SceSdifForDriver_B05EFF68(SdifSdioFunction *function, uint32_t index)
{
	return sdifGetCommandState(function->context->contextGlobal, index);
}

int SceSdifForDriver_D3C1E2B6(SdifSdioFunction *function, uint32_t offset,
			    uint32_t size, uint8_t *buffer)
{
	SdifSdioContext *sdio;
	uint8_t *end;
	uint32_t address, status;
	int result;

	if (!buffer || !function || size - 1 > 255 || offset > 256 ||
	    offset - 1 + size > 256)
		return 0x8032000e;
	end = buffer + size;
	if (buffer >= end)
		return 0;
	address = offset + (function->number << 8);
	sdio = function->context;
	for (;;) {
		SdifContextGlobal *context = sdio->contextGlobal;
		SdifCommand *command = sdifAllocateCommandLocked(context);

		if (!command)
			return 0x80320011;
		command->argument = (address & 0x1ffff) << 9;
		command->size = sizeof(*command);
		command->flags = 0x63;
		command->command = 52;
		command->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
		if (result < 0) {
			sdifFreeCommandLocked(context, command);
			return result;
		}
		status = command->response[0];
		result = (uint8_t)status;
		if (status & 0x8000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320154;
		}
		if (status & 0x4000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320153;
		}
		if (status & 0x800) {
			sdifFreeCommandLocked(context, command);
			return 0x80320152;
		}
		if (status & 0x200) {
			sdifFreeCommandLocked(context, command);
			return 0x80320151;
		}
		if (status & 0x100) {
			sdifFreeCommandLocked(context, command);
			return 0x80320150;
		}
		sdifFreeCommandLocked(context, command);
		*buffer++ = result;
		if (buffer == end)
			return result;
		address++;
	}
}

int SceSdifForDriver_5BAC6E70(SdifSdioFunction *function, uint32_t offset,
			    uint32_t size, uint8_t *buffer)
{
	SdifSdioContext *sdio;
	uint8_t *end;
	uint32_t address, status, data;
	int result;

	if (!buffer || !function || size - 1 > 255 || offset > 256 ||
	    offset - 1 + size > 256)
		return 0x8032000e;
	end = buffer + size;
	address = offset + (function->number << 8);
	if (buffer >= end)
		return 0;
	sdio = function->context;
	for (;;) {
		SdifContextGlobal *context = sdio->contextGlobal;
		SdifCommand *command;

		data = *buffer;
		command = sdifAllocateCommandLocked(context);
		data |= ((address++ & 0x1ffff) << 9) | 0x80000000;
		if (!command)
			return 0x80320011;
		command->argument = data;
		command->size = sizeof(*command);
		command->flags = 0x63;
		command->command = 52;
		command->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
		if (result < 0) {
			sdifFreeCommandLocked(context, command);
			return result;
		}
		status = command->response[0];
		if (status & 0x8000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320154;
		}
		if (status & 0x4000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320153;
		}
		if (status & 0x800) {
			sdifFreeCommandLocked(context, command);
			return 0x80320152;
		}
		if (status & 0x200) {
			sdifFreeCommandLocked(context, command);
			return 0x80320151;
		}
		if (status & 0x100) {
			sdifFreeCommandLocked(context, command);
			return 0x80320150;
		}
		sdifFreeCommandLocked(context, command);
		if (++buffer == end)
			return (uint8_t)status;
	}
}

int SceSdifForDriver_180E7395(SdifSdioFunction *function, void *buffer,
			    uint32_t size)
{
	if (!buffer || !function || size > 256)
		return 0x8032000e;
	if (!function->cisLength)
		sdifReadIoFunctionCis(function->context, function->number);
	memcpy(buffer, function->cis, size);
	return 0;
}

int SceSdifForDriver_1847B18C(SdifSdioFunction *function)
{
	if (!function)
		return 0x8032000e;
	if (!function->cisLength)
		sdifReadIoFunctionCis(function->context, function->number);
	return function->cisLength;
}

int SceSdifForDriver_01E8EB6C(SdifSdioContext *sdio, void *buffer, uint32_t size)
{
	int high, middle, low, length;

	if (!buffer || !sdio || size > 256)
		return 0x8032000e;
	if (!sdio->commonCisLength) {
		high = sdifIoReadWriteDirect(sdio->contextGlobal, 11, 0, 0);
		if (high < 0)
			return high;
		middle = sdifIoReadWriteDirect(sdio->contextGlobal, 10, 0, 0);
		if (middle < 0)
			return middle;
		low = sdifIoReadWriteDirect(sdio->contextGlobal, 9, 0, 0);
		if (low < 0)
			return low;
		sdio->commonCisLength = 0;
		length = sdifReadIoCis(sdio, sdio->commonCis,
			(uint8_t)low | (((uint8_t)middle | ((uint8_t)high << 8)) << 8));
		if (length < 0)
			return length;
		sdio->commonCisLength = length;
	}
	memcpy(buffer, sdio->commonCis, size);
	return 0;
}

int SceSdifForDriver_763F1075(SdifSdioContext *sdio)
{
	int high, middle, low, length;

	if (!sdio)
		return 0x8032000e;
	if (sdio->commonCisLength)
		return sdio->commonCisLength;
	high = sdifIoReadWriteDirect(sdio->contextGlobal, 11, 0, 0);
	if (high < 0)
		return high;
	middle = sdifIoReadWriteDirect(sdio->contextGlobal, 10, 0, 0);
	if (middle < 0)
		return middle;
	low = sdifIoReadWriteDirect(sdio->contextGlobal, 9, 0, 0);
	if (low < 0)
		return low;
	sdio->commonCisLength = 0;
	length = sdifReadIoCis(sdio, sdio->commonCis,
		(uint8_t)low | (((uint8_t)middle | ((uint8_t)high << 8)) << 8));
	if (length >= 0)
		sdio->commonCisLength = length;
	return length;
}

int SceSdifForDriver_DECE963B(SdifSdioContext *sdio, uint32_t offset,
			    uint32_t size, uint8_t *buffer)
{
	uint8_t *end;
	uint32_t address = offset, status;
	int result;

	if (!buffer || !sdio || size - 1 > 255 || offset > 255 ||
	    offset - 1 + size > 255)
		return 0x8032000e;
	end = buffer + size;
	if (buffer >= end)
		return 0;
	for (;;) {
		SdifContextGlobal *context = sdio->contextGlobal;
		SdifCommand *command = sdifAllocateCommandLocked(context);

		if (!command)
			return 0x80320011;
		command->argument = (address & 0x1ffff) << 9;
		command->size = sizeof(*command);
		command->flags = 0x63;
		command->command = 52;
		command->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
		if (result < 0) {
			sdifFreeCommandLocked(context, command);
			return result;
		}
		status = command->response[0];
		result = (uint8_t)status;
		if (status & 0x8000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320154;
		}
		if (status & 0x4000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320153;
		}
		if (status & 0x800) {
			sdifFreeCommandLocked(context, command);
			return 0x80320152;
		}
		if (status & 0x200) {
			sdifFreeCommandLocked(context, command);
			return 0x80320151;
		}
		if (status & 0x100) {
			sdifFreeCommandLocked(context, command);
			return 0x80320150;
		}
		sdifFreeCommandLocked(context, command);
		*buffer++ = result;
		if (buffer == end)
			return result;
		address++;
	}
}

int SceSdifForDriver_5D65E66B(SdifSdioContext *sdio, uint32_t offset,
			    uint32_t size, uint8_t *buffer)
{
	uint8_t *end;
	uint32_t address = offset, status, data;
	int result;

	if (!buffer || !sdio || size - 1 > 255 || offset > 255 ||
	    offset - 1 + size > 255)
		return 0x8032000e;
	end = buffer + size;
	if (buffer >= end)
		return 0;
	for (;;) {
		SdifContextGlobal *context = sdio->contextGlobal;
		SdifCommand *command;

		data = *buffer;
		command = sdifAllocateCommandLocked(context);
		data |= ((address++ & 0x1ffff) << 9) | 0x80000000;
		if (!command)
			return 0x80320011;
		command->argument = data;
		command->size = sizeof(*command);
		command->flags = 0x63;
		command->command = 52;
		command->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
		if (result < 0) {
			sdifFreeCommandLocked(context, command);
			return result;
		}
		status = command->response[0];
		if (status & 0x8000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320154;
		}
		if (status & 0x4000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320153;
		}
		if (status & 0x800) {
			sdifFreeCommandLocked(context, command);
			return 0x80320152;
		}
		if (status & 0x200) {
			sdifFreeCommandLocked(context, command);
			return 0x80320151;
		}
		if (status & 0x100) {
			sdifFreeCommandLocked(context, command);
			return 0x80320150;
		}
		sdifFreeCommandLocked(context, command);
		if (++buffer == end)
			return (uint8_t)status;
	}
}

SdifSdioContext *SceSdifForDriver_6A8235FC(unsigned int index)
{
	SdifContextGlobal *context = SceSdifForDriver_DC8F52F8(index);

	if (!context)
		return NULL;
	if (context->port.deviceType == 3)
		return context->port.part;
	return NULL;
}

int SceSdifForDriver_AB0222F2(SdifSdioContext *sdio)
{
	return sdifDeselectCard(sdio->contextGlobal);
}

SdifSdioFunction *SceSdifForDriver_0C66E36F(SdifSdioContext *sdio,
					const SdifSdioFunctionIdentifier *identifier)
{
	uint32_t count = sdio->functionCount;
	uint32_t index;
	SdifSdioFunction *function;

	if (!count)
		return NULL;
	if (identifier->interfaceCode != UINT32_MAX) {
		for (index = 1, function = &sdio->functions[1]; index <= count;
		     index++, function++) {
			if (identifier->interfaceCode != function->interfaceCode)
				continue;
			if (identifier->manufacturerId != UINT32_MAX &&
			    identifier->manufacturerId != function->manufacturerId)
				continue;
			if (identifier->productId == UINT32_MAX ||
			    identifier->productId == function->productId)
				return function;
		}
	} else if (identifier->manufacturerId == UINT32_MAX) {
		for (index = 1, function = &sdio->functions[1]; index <= count;
		     index++, function++) {
			if (identifier->productId == UINT32_MAX ||
			    identifier->productId == function->productId)
				return function;
		}
	} else {
		for (index = 1, function = &sdio->functions[1]; index <= count;
		     index++, function++) {
			if (identifier->manufacturerId != function->manufacturerId)
				continue;
			if (identifier->productId == UINT32_MAX ||
			    identifier->productId == function->productId)
				return function;
		}
	}
	return NULL;
}

int SceSdifForDriver_BC45C83D(SdifSdioContext *sdio, unsigned int mode)
{
	SdifContextGlobal *context;
	SdifCommand *command;
	uint32_t frequency, status;
	int highSpeed, result;
	uint8_t value;

	if (mode > 2) {
		frequency = 50000000;
		highSpeed = 1;
	} else {
		highSpeed = sdioClockHighSpeed[mode];
		frequency = sdioClockFrequencies[mode];
	}
	if (!highSpeed)
		return sdifSetClock(sdio->contextGlobal, frequency, highSpeed);
	if (!sdio)
		return 0x8032000e;
	context = sdio->contextGlobal;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = 19 << 9;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	value = status;
	sdifFreeCommandLocked(context, command);
	if (!(value & 1))
		return 0x80320013;
	context = sdio->contextGlobal;
	value |= 2;
	command = sdifAllocateCommandLocked(context);
	if (!command)
		return 0x80320011;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->argument = value | 0x80002600;
	command->buffer = NULL;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
	if (result < 0) {
		sdifFreeCommandLocked(context, command);
		return result;
	}
	status = command->response[0];
	if (status & 0x8000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320154;
	}
	if (status & 0x4000) {
		sdifFreeCommandLocked(context, command);
		return 0x80320153;
	}
	if (status & 0x800) {
		sdifFreeCommandLocked(context, command);
		return 0x80320152;
	}
	if (status & 0x200) {
		sdifFreeCommandLocked(context, command);
		return 0x80320151;
	}
	if (status & 0x100) {
		sdifFreeCommandLocked(context, command);
		return 0x80320150;
	}
	sdifFreeCommandLocked(context, command);
	return sdifSetClock(sdio->contextGlobal, frequency, highSpeed);
}

int SceSdifForDriver_AABAA0F0(unsigned int index, SdifSdioContext **part)
{
	SdifContextGlobal *context = SceSdifForDriver_DC8F52F8(index);
	SdifContextGlobal *commandContext;
	SdifSdioContext *sdio;
	SdifSdioFunction *function;
	SdifCommand *command;
	uint32_t voltages, number, offset, next, length, code, address;
	uint32_t relativeCardAddress, byte, lowSpeed;
	uint8_t powerControl, registers[18], *cis, *tuple;
	uint16_t blockSize;
	int result, high, middle, low, resumed;

	if (!context) {
		result = 0x80320013;
		goto fail;
	}
	if (sceSdifReset(context, 2) < 0) {
		result = 0x80320015;
		goto fail;
	}
	if (sdifIsCardInserted(context) <= 0) {
		result = 0x8032001a;
		goto fail;
	}
	voltages = context->port.voltages;
	if (voltages & 0x300000)
		powerControl = 14;
	else if (voltages & 0x60000)
		powerControl = 12;
	else if (voltages & 0x80)
		powerControl = 10;
	else
		powerControl = 0;
	if (sdifPowerOn(context, powerControl) < 0) {
		result = 0x80320014;
		goto fail;
	}
	sdifDelay(context, 20000);
	sdio = &gSdioContexts[context->port.index];
	context->port.deviceType = 3;
	context->port.part = sdio;
	sdio->contextGlobal = context;
	sdio->masterInterruptEnabled = 1;
	command = sdifAllocateCommandLocked(context);
	if (command) {
		command->size = sizeof(*command);
		command->flags = 0x82;
		command->command = 3;
		command->argument = 0;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
		if (result >= 0) {
			resumed = 1;
			context->port.relativeCardAddress = command->response[0] >> 16;
			sdifFreeCommandLocked(context, command);
			goto assignAddress;
		}
		sdifFreeCommandLocked(context, command);
	}
	memset(sdio, 0, sizeof(*sdio));
	sdio->contextGlobal = context;
	sdio->masterInterruptEnabled = 1;
	sdifIoReadWriteDirect(context, 6, 8, 1);
	context->port.deviceType = 0;
	context->port.part = NULL;
	context->port.relativeCardAddress = 0;
	if (sdifIdentifyCard(context) < 0) {
		result = 0x80320016;
		goto fail;
	}
	if (context->port.deviceType != 3) {
		result = 0x80320017;
		goto fail;
	}
	sdio->functionCount = (context->port.operationConditions >> 28) & 7;
	for (number = 1; number <= sdio->functionCount; number++) {
		function = &sdio->functions[number];
		memset(function, 0, sizeof(*function));
		function->number = number;
		function->context = sdio;
		function->cisLength = 0;
	}
	resumed = 0;

assignAddress:
	commandContext = sdio->contextGlobal;
	command = sdifAllocateCommandLocked(commandContext);
	if (!command) {
		result = 0x80320011;
		goto fail;
	}
	command->size = sizeof(*command);
	command->flags = 0x82;
	command->command = 3;
	command->argument = 0;
	result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
	if (result < 0) {
		sdifFreeCommandLocked(commandContext, command);
		goto fail;
	}
	relativeCardAddress = command->response[0] >> 16;
	sdifFreeCommandLocked(commandContext, command);
	if (!relativeCardAddress) {
		commandContext = sdio->contextGlobal;
		command = sdifAllocateCommandLocked(commandContext);
		if (!command) {
			result = 0x80320011;
			goto fail;
		}
		command->size = sizeof(*command);
		command->flags = 0x82;
		command->command = 3;
		command->argument = 0;
		result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 3);
		if (result < 0) {
			sdifFreeCommandLocked(commandContext, command);
			goto fail;
		}
		relativeCardAddress = command->response[0] >> 16;
		sdifFreeCommandLocked(commandContext, command);
	}
	context->port.relativeCardAddress = relativeCardAddress;
	result = sdifSelectCard(sdio->contextGlobal, 1);
	if (result < 0)
		goto fail;
	result = sdifIoReadWriteDirect(sdio->contextGlobal, 7, 0, 0);
	if (result < 0)
		goto fail;
	result = sdifIoReadWriteDirect(sdio->contextGlobal, 7,
				       (uint8_t)result | 0x80, 1);
	if (result < 0)
		goto fail;
	result = sdifIoReadWriteDirect(sdio->contextGlobal, 8, 0, 0);
	if (result < 0)
		goto fail;
	lowSpeed = result & 0x40;
	if (!lowSpeed || (result & 0x80))
		sdifConfigureIoBusWidth(context, sdio);
	cis = sdio->commonCis;
	if (!resumed) {
		high = sdifIoReadWriteDirect(sdio->contextGlobal, 11, 0, 0);
		if (high < 0) {
			result = high;
			goto fail;
		}
		middle = sdifIoReadWriteDirect(sdio->contextGlobal, 10, 0, 0);
		if (middle < 0) {
			result = middle;
			goto fail;
		}
		low = sdifIoReadWriteDirect(sdio->contextGlobal, 9, 0, 0);
		if (low < 0) {
			result = low;
			goto fail;
		}
		sdio->commonCisLength = 0;
		address = (uint8_t)low | ((uint8_t)middle << 8) | ((uint8_t)high << 16);
		result = sdifReadIoCis(sdio, cis, address);
		if (result < 0)
			goto fail;
		sdio->commonCisLength = result;
	}
	for (offset = 0; ; offset = next) {
		code = cis[offset];
		next = offset + 1;
		if (code == 255)
			break;
		if (offset == 255)
			goto badCommonCis;
		length = cis[next];
		next = offset + 2 + length;
		if (next > 256)
			goto badCommonCis;
		tuple = &cis[offset];
		if (code == 32) {
			if (length != 4)
				goto badCommonCis;
			sdio->manufacturerId = tuple[2] + (tuple[3] << 8);
			sdio->productId = tuple[4] + (tuple[5] << 8);
		} else if (code == 34) {
			if (length != 4)
				goto badCommonCis;
			sdio->maximumBlockSize = tuple[3] + (tuple[4] << 8);
			sdio->maximumClock = sdioClockValues[(tuple[5] >> 3) & 15] *
				sdioClockUnits[tuple[5] & 7];
		}
		if (next == 256)
			break;
	}
	for (number = 1; number <= sdio->functionCount; number++) {
		function = &sdio->functions[number];
		__builtin_prefetch(function + 1);
		function->manufacturerId = 0;
		__builtin_prefetch(&function[1].productId);
		function->productId = 0;
		if (!resumed && sdifReadIoFunctionCis(sdio, number) < 0)
			continue;
		address = function->number << 8;
		for (byte = 0; byte < sizeof(registers); byte++) {
			commandContext = function->context->contextGlobal;
			command = sdifAllocateCommandLocked(commandContext);
			if (!command)
				goto nextFunction;
			command->buffer = NULL;
			command->argument = ((address + byte) & 0x1ffff) << 9;
			command->size = sizeof(*command);
			command->flags = 0x63;
			command->command = 52;
			result = sdifSendCheckedSyncCommand(commandContext, command, NULL, 0);
			if (result < 0 || (command->response[0] & 0x8000) ||
			    (command->response[0] & 0x4000) ||
			    (command->response[0] & 0x800) ||
			    (command->response[0] & 0x200) ||
			    (command->response[0] & 0x100)) {
				sdifFreeCommandLocked(commandContext, command);
				goto nextFunction;
			}
			result = command->response[0];
			sdifFreeCommandLocked(commandContext, command);
			registers[byte] = result;
		}
		__builtin_prefetch(&function[1].cisLength);
		function->standardInterface = registers[0] & 15;
		function->interfaceCode = function->standardInterface;
		function->extendedInterface = registers[1];
		if (function->cisLength) {
			cis = function->cis;
			for (offset = 0; ; offset = next) {
				code = cis[offset];
				next = offset + 1;
				if (code == 255)
					break;
				if (offset == 255)
					goto badFunctionCis;
				length = cis[next];
				next = offset + 2 + length;
				if (next > 256)
					goto badFunctionCis;
				tuple = &cis[offset];
				if (code == 32) {
					if (length != 4)
						goto badFunctionCis;
					function->manufacturerId = tuple[2] + (tuple[3] << 8);
					function->productId = tuple[4] + (tuple[5] << 8);
				} else if (code == 34) {
					if (length != 42)
						goto badFunctionCis;
					function->extensionType = tuple[2];
					function->functionInfo = tuple[3];
					function->standardIoRevision = tuple[4];
					function->serialNumber = tuple[5] + (((uint32_t)tuple[6] +
						((tuple[7] + (tuple[8] << 8)) << 8)) << 8);
					function->codeStorageAreaSize = tuple[9] + (((uint32_t)tuple[10] +
						((tuple[11] + (tuple[12] << 8)) << 8)) << 8);
					function->codeStorageAreaProperty = tuple[13];
					function->maximumBlockSize = tuple[14] + (tuple[15] << 8);
					function->operationConditions = tuple[16] + (((uint32_t)tuple[17] +
						((tuple[18] + (tuple[19] << 8)) << 8)) << 8);
					function->operatingMinimumPower = tuple[20];
					function->operatingAveragePower = tuple[21];
					function->operatingMaximumPower = tuple[22];
					function->standbyMinimumPower = tuple[23];
					function->standbyAveragePower = tuple[24];
					function->standbyMaximumPower = tuple[25];
					function->minimumBandwidth = tuple[26] + (tuple[27] << 8);
					function->optimumBandwidth = tuple[28] + (tuple[29] << 8);
					function->enableTimeout = tuple[30] + (tuple[31] << 8);
					function->standardPowerAverage = tuple[32] + (tuple[33] << 8);
					function->standardPowerMaximum = tuple[34] + (tuple[35] << 8);
					function->highPowerAverage = tuple[36] + (tuple[37] << 8);
					function->highPowerMaximum = tuple[38] + (tuple[39] << 8);
					function->lowPowerAverage = tuple[40] + (tuple[41] << 8);
					function->lowPowerMaximum = tuple[42] + (tuple[43] << 8);
				}
				if (next == 256)
					break;
			}
		}
		blockSize = function->maximumBlockSize;
		__builtin_prefetch(&function[1].maximumBlockSize);
		function->blockSize = 0;
		for (byte = 0; byte < sizeof(blockSize); byte++) {
			result = sdifIoReadWriteDirect(function->context->contextGlobal,
				(function->number << 8) + 16 + byte,
				((uint8_t *)&blockSize)[byte], 1);
			if (result < 0)
				goto nextFunction;
		}
		function->blockSize = blockSize;
		goto nextFunction;
badFunctionCis:
		function->manufacturerId = 0;
		function->productId = 0;
nextFunction:
		;
	}
	if (lowSpeed) {
		result = sdifSetClock(sdio->contextGlobal, 400000, 0);
	} else {
		result = sdifIoReadWriteDirect(sdio->contextGlobal, 19, 0, 0);
		if (result >= 0 && (result & 1) &&
		    sdifIoReadWriteDirect(sdio->contextGlobal, 19,
			(uint8_t)result | 2, 1) >= 0 &&
		    sdifSetClock(sdio->contextGlobal, 50000000, 1) >= 0)
			goto success;
		if (sdifSetClock(sdio->contextGlobal, 25000000, 0) >= 0)
			goto success;
		result = SceSdifForDriver_BC45C83D(sdio, 0);
	}
	if (result < 0)
		goto fail;
success:
	context->port.part = sdio;
	*part = sdio;
	return 0;
badCommonCis:
	result = 0x8032001f;
fail:
	*part = NULL;
	return result;
}

SdifSdioContext *SceSdifForDriver_53962379(unsigned int index)
{
	SdifSdioContext *sdio = NULL;

	SceSdifForDriver_AABAA0F0(index, &sdio);
	return sdio;
}

int SceSdifForDriver_733BC373(SdifSdioFunction *function, unsigned int size)
{
	SdifSdioContext *sdio;
	SdifContextGlobal *context;
	SdifCommand *command;
	uint16_t blockSize = size;
	uint32_t address, end, data, status;
	uint8_t *byte = (uint8_t *)&blockSize;
	int result;

	if (function->maximumBlockSize < size)
		return 0x8032000e;
	sdio = function->context;
	address = (function->number << 8) + 16;
	end = address + sizeof(blockSize);
	for (;;) {
		context = sdio->contextGlobal;
		data = *byte++;
		command = sdifAllocateCommandLocked(context);
		status = data | ((address++ & 0x1ffff) << 9) | 0x80000000;
		if (!command)
			return 0x80320011;
		command->size = sizeof(*command);
		command->flags = 0x63;
		command->command = 52;
		command->argument = status;
		command->buffer = NULL;
		result = sdifSendCheckedSyncCommand(context, command, NULL, 0);
		if (result < 0) {
			sdifFreeCommandLocked(context, command);
			return result;
		}
		status = command->response[0];
		if (status & 0x8000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320154;
		}
		if (status & 0x4000) {
			sdifFreeCommandLocked(context, command);
			return 0x80320153;
		}
		if (status & 0x800) {
			sdifFreeCommandLocked(context, command);
			return 0x80320152;
		}
		if (status & 0x200) {
			sdifFreeCommandLocked(context, command);
			return 0x80320151;
		}
		if (status & 0x100) {
			sdifFreeCommandLocked(context, command);
			return 0x80320150;
		}
		sdifFreeCommandLocked(context, command);
		if (address == end) {
			function->blockSize = size;
			return 0;
		}
	}
}
