/* SceSdif SDIO interrupt command callbacks. */
#include "sdif_internal.h"

void sdifCompleteFunctionMaskWrite(SdifCommand *command)
{
	SdifContextGlobal *context = command->context;
	int errorCode = command->errorCode;
	volatile SdifHostRegisters *registers;

	sdifFreeCommand(context, command);
	registers = context->port.registers;
	if ((uint32_t)errorCode == 0x8032001a) {
		context->port.pendingFunctions = 0;
		context->port.interruptDispatchPending = 0;
	} else {
		context->port.interruptDispatchPending = 1;
	}
	registers->normalSignalEnable |= 0x100;
	registers->normalInterruptEnable |= 0x100;
}

void sdifCompleteFunctionEnable(SdifCommand *command)
{
	sdifFreeCommand(command->context, command);
}

void sdifCompleteFunctionDisable(SdifCommand *command)
{
	sdifFreeCommand(command->context, command);
}

void sdifCompleteFunctionMaskRead(SdifCommand *command)
{
	SdifContextGlobal *context = command->context;
	SdifSdioContext *sdio = context->port.part;
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers;
	SdifCommand *next;
	uint32_t interruptFunctions, enabledFunctions, mask;
	int errorCode;

	port->interruptFunctions = command->response[0];
	errorCode = command->errorCode;
	sdifFreeCommand(context, command);
	if (errorCode == (int)0x8032001a || !sdio) {
		registers = port->registers;
		port->pendingFunctions = 0;
		port->interruptDispatchPending = 0;
		registers->normalSignalEnable |= 0x100;
		registers->normalInterruptEnable |= 0x100;
		return;
	}
	interruptFunctions = port->interruptFunctions;
	enabledFunctions = port->enabledFunctions;
	port->pendingFunctions = interruptFunctions & enabledFunctions;
	mask = enabledFunctions & ~interruptFunctions & ~(sdio->masterInterruptEnabled & 1);
	if (port->deviceType) {
		next = sdifAllocateCommand(context);
		if (next) {
			next->argument = mask | 0x88000800;
			next->buffer = NULL;
			next->context = context;
			next->eventFlag = 0;
			next->secondaryCommand = NULL;
			next->callback = sdifCompleteFunctionMaskWrite;
			next->size = sizeof(*next);
			next->flags = 0x63;
			next->command = 52;
			sdifQueueCommand(context, next, 1);
		}
	}
}

void sdifStartFunctionInterrupt(SdifContextGlobal *context, int interrupt,
			       unsigned int intrPriority)
{
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers = port->registers;
	SdifCommand *command;
	uint16_t signalEnable;
	uint32_t deviceType;

	port->pendingFunctions = 0;
	signalEnable = registers->normalSignalEnable;
	port->interruptNumber = interrupt;
	port->interruptArgument = intrPriority;
	port->interruptDispatchPending = 0;
	deviceType = port->deviceType;
	registers->normalSignalEnable = signalEnable & 0xfeff;
	registers->normalInterruptEnable &= 0xfeff;
	if (deviceType) {
		command = sdifAllocateCommand(context);
		if (command) {
			command->buffer = NULL;
			command->context = context;
			command->eventFlag = 0;
			command->secondaryCommand = NULL;
			command->callback = sdifCompleteFunctionMaskRead;
			command->size = sizeof(*command);
			command->flags = 0x63;
			command->command = 52;
			command->argument = 0xa00;
			sdifQueueCommand(context, command, 1);
			return;
		}
		registers = port->registers;
	}
	port->pendingFunctions = 0;
	signalEnable = registers->normalSignalEnable;
	port->interruptDispatchPending = 0;
	registers->normalSignalEnable = signalEnable | 0x100;
	registers->normalInterruptEnable |= 0x100;
}

void sdifDispatchFunctionInterrupt(SdifContextGlobal *context)
{
	SdifPortState *port = &context->port;

	if (!port->interruptDispatchPending || !port->pendingFunctions)
		return;
	if (port->pendingFunctions & 2)
		ksceKernelTriggerSubIntr(port->interruptNumber, 2, (void *)0x100);
	if (port->pendingFunctions & 4)
		ksceKernelTriggerSubIntr(port->interruptNumber, 3,
			(void *)(uintptr_t)port->interruptFunctions);
	if (port->pendingFunctions & 8)
		ksceKernelTriggerSubIntr(port->interruptNumber, 4,
			(void *)(uintptr_t)port->interruptFunctions);
	if (port->pendingFunctions & 0x10)
		ksceKernelTriggerSubIntr(port->interruptNumber, 5,
			(void *)(uintptr_t)port->interruptFunctions);
	if (port->pendingFunctions & 0x20)
		ksceKernelTriggerSubIntr(port->interruptNumber, 6,
			(void *)(uintptr_t)port->interruptFunctions);
	if (port->pendingFunctions & 0x40)
		ksceKernelTriggerSubIntr(port->interruptNumber, 7,
			(void *)(uintptr_t)port->interruptFunctions);
	if (port->pendingFunctions & 0x80)
		ksceKernelTriggerSubIntr(port->interruptNumber, 8,
			(void *)(uintptr_t)port->interruptFunctions);
	port->interruptDispatchPending = 0;
	port->pendingFunctions = 0;
}

int sdifEnableFunctionInterrupt(SdifContextGlobal *context, int function)
{
	uint32_t mask = (uint8_t)((1U << function) |
				context->port.enabledFunctions);
	SdifCommand *command;

	if (mask & 0xfe)
		mask |= 1;
	context->port.enabledFunctions = mask;
	if (!context->port.deviceType)
		return 0x8032001a;
	command = sdifAllocateCommand(context);
	if (!command)
		return 0x80320010;
	command->argument = mask | 0x80000800;
	command->callback = sdifCompleteFunctionEnable;
	command->context = context;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->buffer = NULL;
	command->eventFlag = 0;
	command->secondaryCommand = NULL;
	sdifQueueCommand(context, command, 1);
	return 0;
}

int sdifDisableFunctionInterrupt(SdifContextGlobal *context, int function)
{
	uint32_t mask = context->port.enabledFunctions & ~(1U << function);
	SdifCommand *command;

	if (mask & 0xfe)
		mask |= 1;
	context->port.enabledFunctions = mask;
	if (!context->port.deviceType)
		return 0x8032001a;
	command = sdifAllocateCommand(context);
	if (!command)
		return 0x80320010;
	command->argument = mask | 0x80000800;
	command->callback = sdifCompleteFunctionDisable;
	command->context = context;
	command->size = sizeof(*command);
	command->flags = 0x63;
	command->command = 52;
	command->buffer = NULL;
	command->eventFlag = 0;
	command->secondaryCommand = NULL;
	sdifQueueCommand(context, command, 1);
	return 0;
}
