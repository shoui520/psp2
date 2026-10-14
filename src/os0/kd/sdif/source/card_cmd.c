/* SceSdif card discovery commands; this unlogged source name is descriptive. */
#include "sdif_internal.h"

int sdifSendInterfaceCondition(SdifContextGlobal *context, uint32_t voltage)
{
	void *storage = __builtin_alloca(sizeof(SdifCommand) + 64 -
					 __alignof__(uint64_t));
	SdifCommand *command = (SdifCommand *)(((uintptr_t)storage + 63) &
					     ~(uintptr_t)63);
	uint32_t response;
	int result;

	command->argument = (voltage << 8) | 0xaa;
	command->size = sizeof(*command);
	command->flags = 0x92;
	command->command = 8;
	command->buffer = NULL;
	result = sdifPrepareSyncCommand(context, command, NULL, 5, 0);
	if (result >= 0) {
		response = command->response[0];
		if ((uint8_t)response == 0xaa) {
			result = 0x80320161;
			if (voltage & (response >> 8))
				result = 0;
		} else {
			result = 0x80320160;
		}
	}
	return result;
}

int sdifSendIoOperationCondition(SdifContextGlobal *context, uint32_t voltages,
				 uint32_t *operationConditions)
{
	void *storage = __builtin_alloca(sizeof(SdifCommand) + 64 -
					 __alignof__(uint64_t));
	SdifCommand *command = (SdifCommand *)(((uintptr_t)storage + 63) &
					     ~(uintptr_t)63);
	int retries, result;
	uint32_t response;

	command->argument = voltages;
	command->size = sizeof(*command);
	command->flags = 0x52;
	command->command = 5;
	command->buffer = NULL;
	if (voltages) {
		retries = 100;
		for (;;) {
			result = sdifPrepareSyncCommand(context, command, NULL, 5, 0);
			if (result < 0)
				return result;
			response = command->response[0];
			if ((int32_t)response < 0) {
				if (operationConditions)
					*operationConditions = response;
				return 0;
			}
			sdifDelay(context, 100000);
			if (!--retries)
				return 0x80320002;
		}
	}
	result = sdifPrepareSyncCommand(context, command, NULL, 5, 0);
	if (result >= 0) {
		if (operationConditions)
			*operationConditions = command->response[0];
		result = 0;
	}
	return result;
}

int sdifSendSdOperationCondition(SdifContextGlobal *context, uint32_t voltages,
				 uint32_t *operationConditions)
{
	void *storage = __builtin_alloca(sizeof(SdifCommand) + 64 -
					 __alignof__(uint64_t));
	SdifCommand *command = (SdifCommand *)(((uintptr_t)storage + 63) &
					     ~(uintptr_t)63);
	int retries, result;
	uint32_t response;

	command->argument = voltages;
	command->size = sizeof(*command);
	command->flags = 0x42;
	command->command = 41;
	command->buffer = NULL;
	if (voltages) {
		retries = 100;
		for (;;) {
			result = sdifSendApplicationCommand(context, command, 5);
			if (result < 0)
				return result;
			response = command->response[0];
			if ((int32_t)response < 0) {
				if (operationConditions)
					*operationConditions = response;
				return 0;
			}
			sdifDelay(context, 100000);
			if (!--retries)
				return 0x80320002;
		}
	}
	result = sdifSendApplicationCommand(context, command, 5);
	if (result >= 0) {
		if (operationConditions)
			*operationConditions = command->response[0];
		result = 0;
	}
	return result;
}

int sdifGetIoOperationCondition(SdifContextGlobal *context,
			       uint32_t *operationConditions)
{
	void *storage = __builtin_alloca(sizeof(SdifCommand) + 64 -
					 __alignof__(uint64_t));
	SdifCommand *command = (SdifCommand *)(((uintptr_t)storage + 63) &
					     ~(uintptr_t)63);
	int result;

	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x52;
	command->command = 5;
	command->argument = 0;
	result = sdifPrepareSyncCommand(context, command, NULL, 5, 0);
	if (result >= 0) {
		result = (int)(uintptr_t)operationConditions;
		if (operationConditions) {
			result = 0;
			*operationConditions = command->response[0];
		}
	}
	return result;
}

int sdifGetSdOperationCondition(SdifContextGlobal *context,
			       uint32_t *operationConditions)
{
	void *storage = __builtin_alloca(sizeof(SdifCommand) + 64 -
					 __alignof__(uint64_t));
	SdifCommand *command = (SdifCommand *)(((uintptr_t)storage + 63) &
					     ~(uintptr_t)63);
	int result;

	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x42;
	command->command = 41;
	command->argument = 0;
	result = sdifSendApplicationCommand(context, command, 5);
	if (result >= 0) {
		result = (int)(uintptr_t)operationConditions;
		if (operationConditions) {
			result = 0;
			*operationConditions = command->response[0];
		}
	}
	return result;
}

int sdifIdentifyCard(SdifContextGlobal *context)
{
	uint32_t operationConditions, savedConditions, voltages;
	int result, version2, ioMemory;

	result = sdifResetCard(context);
	if (result < 0) {
		context->port.deviceType = 0;
		return result;
	}
	result = sdifSendInterfaceCondition(context,
				 (context->port.voltages & 0xff8000) != 0);
	if (result) {
		if (result != (int)0x80320002) {
			context->port.deviceType = 0;
			return result;
		}
		version2 = 0;
	} else {
		version2 = 1;
	}
	ioMemory = 0;
	if (!sdifGetIoOperationCondition(context, &operationConditions)) {
		context->port.operationConditions = operationConditions;
		voltages = context->port.voltages & operationConditions;
		if (!voltages)
			voltages = operationConditions & 0xffffff;
		if (!sdifSendIoOperationCondition(context, voltages, NULL)) {
			if (!(operationConditions & 0x08000000)) {
				context->port.deviceType = 3;
				return 0;
			}
			ioMemory = 1;
		}
	}
	if (!version2) {
		if (sdifGetSdOperationCondition(context, &operationConditions)) {
			context->port.deviceType = 1;
			return 0;
		}
		voltages = operationConditions & context->port.voltages;
		if (!(voltages << 8))
			return 0x80320019;
		if (!sdifSendSdOperationCondition(context,
						 voltages & ~0x41000000,
						 &operationConditions)) {
			context->port.operationConditions = operationConditions;
			context->port.deviceType = 2;
			return 0;
		}
		context->port.deviceType = ioMemory ? 3 : 0;
		return 0;
	}
	result = sdifGetSdOperationCondition(context, &operationConditions);
	if (result < 0)
		return result;
	savedConditions = operationConditions;
	voltages = context->port.voltages & operationConditions;
	if (!(voltages << 8))
		return 0x80320019;
	if (context->port.voltages & 0x01000000)
		voltages |= 0x01000000;
	result = sdifSendSdOperationCondition(context, voltages | 0x40000000,
					    &operationConditions);
	if (!result) {
		context->port.operationConditions = operationConditions;
		context->port.deviceType = 2;
		return 0;
	}
	if (ioMemory) {
		context->port.deviceType = 3;
		return 0;
	}
	if (result == (int)0x80320002 && savedConditions >> 31) {
		context->port.operationConditions = savedConditions;
		context->port.deviceType = 2;
		return 0;
	}
	context->port.deviceType = 0;
	return 0;
}
