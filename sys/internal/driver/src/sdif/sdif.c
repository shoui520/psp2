/* SceSdif host controller operations (Sony source: sdif.c). */
#include "sdif_internal.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysevent.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sblaimgr.h>
#include <syscon/syscon.h>
#include <lowio/pervasive.h>
#include <smsc_proxy/smsc_proxy.h>
#include <string.h>

int sdifEnableSubIntr(int interrupt, int subInterrupt);
int sdifDisableSubIntr(int interrupt, int subInterrupt);

#if FW_VERSION >= 0x04000000
static const uint32_t sdifRegisterAddresses[SDIF_PORTS] = {
	SDIF_REG_BASE(0), SDIF_REG_BASE(1), SDIF_REG_BASE(2), SDIF_REG_BASE(3)
};
static const SceKernelIntrCallbacks sdifIntrCallbacks = {
	.size = sizeof(SceKernelIntrCallbacks),
	.postRelease = sdifDisableSubIntr,
	.enable = sdifEnableSubIntr,
	.disable = sdifDisableSubIntr,
};
static const char sdifPortNames[4][12] = {
	"SceSdif0", "SceSdif1", "SceSdif2", "SceSdif3"
};
static const char *const sdifMemblockNames[4] = {
	sdifPortNames[0], sdifPortNames[1], sdifPortNames[2], sdifPortNames[3]
};
static const SceKernelIntrRegisterOpt gSdifIntrOpt = {
	.size = sizeof(SceKernelIntrRegisterOpt),
	.num = 9,
	.handlers = &sdifIntrCallbacks,
};
#else
static const SceKernelIntrCallbacks sdifIntrCallbacks = {
	.size = sizeof(SceKernelIntrCallbacks),
	.postRelease = sdifDisableSubIntr,
	.enable = sdifEnableSubIntr,
	.disable = sdifDisableSubIntr,
};
static const SceKernelIntrRegisterOpt gSdifIntrOpt = {
	.size = sizeof(SceKernelIntrRegisterOpt),
	.num = 9,
	.handlers = &sdifIntrCallbacks,
};
static const char sdifPortNames[4][12] = {
	"SceSdif0", "SceSdif1", "SceSdif2", "SceSdif3"
};
static const char *const sdifMemblockNames[4] = {
	sdifPortNames[0], sdifPortNames[1], sdifPortNames[2], sdifPortNames[3]
};
static const uint32_t sdifRegisterAddresses[SDIF_PORTS] = {
	SDIF_REG_BASE(0), SDIF_REG_BASE(1), SDIF_REG_BASE(2), SDIF_REG_BASE(3)
};
#endif

SdifGeneralState gSdifState = { 0 };
SdifContextGlobal gSdifContexts[3] __attribute__((aligned(64)));

#if !SDIF_NSKBL
int sdifEnableSubIntr(int interrupt, int subInterrupt)
{
	SdifContextGlobal *context = &gSdifContexts[interrupt - 220];
	SceKernelIntrStatus intrState;
	volatile SdifHostRegisters *registers;
	uint32_t mask;
	int result;

	intrState = sdifSpinLock(&context->port.lock);
	if ((unsigned int)(subInterrupt - 2) <= 6) {
		result = sdifEnableFunctionInterrupt(context, subInterrupt - 1);
		if (result != 0) {
			sdifSpinUnlock(&context->port.lock,
							intrState);
			return result;
		}
	}
	mask = context->port.interruptMask | (1U << subInterrupt);
	registers = context->port.registers;
	context->port.interruptMask = mask;
	if (mask & 0x1fc) {
		uint32_t enable = registers->normalInterruptEnable;
		registers->normalInterruptEnable = enable | 0x100;
		enable = registers->normalSignalEnable;
		registers->normalSignalEnable = enable | 0x100;
	}
	__asm__ volatile("dsb sy" : : : "memory");
	sdifSpinUnlock(&context->port.lock, intrState);
	return 0;
}
#endif

void sdifCompleteSyncCommand(SdifCommand *command)
{
	if (!sdifIsPolling(&command->context->port)) {
		if (command->errorCode != 0) {
			sceKernelSetEventFlag(command->eventFlag, 16);
			return;
		}
		sceKernelSetEventFlag(command->eventFlag, 1);
	}
}

#if !SDIF_NSKBL
int sdifDisableSubIntr(int interrupt, int subInterrupt)
{
	SdifContextGlobal *context = &gSdifContexts[interrupt - 220];
	SceKernelIntrStatus intrState;
	uint32_t mask;
	int result;

	intrState = sdifSpinLock(&context->port.lock);
	if ((unsigned int)(subInterrupt - 2) <= 6) {
		result = sdifDisableFunctionInterrupt(context, subInterrupt - 1);
		if (result != 0) {
			sdifSpinUnlock(&context->port.lock,
							intrState);
			return result;
		}
	}
	mask = context->port.interruptMask & ~(1U << subInterrupt);
	context->port.interruptMask = mask;
	if (!(mask & 0x1fc)) {
		context->port.registers->normalInterruptEnable &= ~0x100;
		context->port.registers->normalSignalEnable &= ~0x100;
	}
	__asm__ volatile("dsb sy" : : : "memory");
	sdifSpinUnlock(&context->port.lock, intrState);
	return 0;
}
#endif

int _sceSdifSendCmdCore(SdifContextGlobal *context, SdifCommand *command,
		       int notify)
{
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers = port->registers;
	SdifCommand *secondary;
	SdifCommand *pending;
	uint32_t flags;
	uint16_t commandFlags;
	uint16_t abortFlags;
	uint16_t transferMode;
	uint16_t multiMode;
	uint16_t singleMode;
	uint16_t noCountMode;
	uint16_t multiAutoMode;
	uint16_t singleAutoMode;
	uint16_t noCountAutoMode;
	uint32_t responseType;
	int retries = 10000;

	if (port->index != 2)
		registers->clockControl |= 4;
	command->errorCode = 0;
	command->state = 2;
	command->flags &= 0x3fffffff;
	port->dataCommand = NULL;
	port->commandComplete = 0;
	port->currentCommand = command;
	do {
		if (!(registers->presentState & 1)) {
			flags = command->flags;
			if ((flags & 7) != 4 || !(registers->presentState & 2))
				goto ready;
		}
	} while (--retries != 0);
	SCE_DBG_PRINTF(11, 2199, "sdif[%d] CMD/DATA busy %04X\n",
		       port->index, registers->presentState);
	port->currentCommand = NULL;
	if (notify) {
		secondary = command->secondaryCommand;
		if (command->callback) {
			command->errorCode = 0x8032001a;
			command->flags |= 0x40000000;
			command->callback(command);
		}
		if (secondary) {
			secondary->errorCode = 0x8032001a;
			secondary->flags |= 0x40000000;
			if (secondary->callback)
				secondary->callback(secondary);
		}
		for (pending = sdifCommandQueuePop(&port->pendingCommands); pending;
		     pending = sdifCommandQueuePop(&port->pendingCommands)) {
			secondary = pending->secondaryCommand;
			if (pending->callback) {
				pending->errorCode = 0x8032001a;
				pending->flags |= 0x40000000;
				pending->callback(pending);
			}
			if (secondary) {
				secondary->errorCode = 0x8032001a;
				secondary->flags |= 0x40000000;
				if (secondary->callback)
					secondary->callback(secondary);
			}
		}
	}
	registers = port->registers;
	registers->softwareReset = 2;
	while (registers->softwareReset & 2)
		;
	registers = port->registers;
	registers->softwareReset = 4;
	while (registers->softwareReset & 4)
		;
	port->deviceType = 0;
	port->part = NULL;
	port->removedDeviceType = 0;
	port->removedPart = NULL;
	if (port->index != 2)
		port->registers->clockControl &= ~4;
	return 0x80320022;

ready:
	responseType = flags & 0xf8;
	switch (responseType) {
	case 0x30:
		commandFlags = 9;
		break;
	case 0x40:
	case 0x50:
		commandFlags = 2;
		break;
	case 0x28:
	case 0x78:
		commandFlags = 27;
		break;
	case 0x10:
	case 0x60:
	case 0x80:
	case 0x90:
		commandFlags = 26;
		break;
	default:
		commandFlags = 0;
		break;
	}
	abortFlags = commandFlags | 0xc0;
	registers->normalSignalEnable |= 1;
	switch (flags & 7) {
	case 3:
		if (flags & 0x20000)
			commandFlags = abortFlags;
		/* fall through */
	case 1:
	case 2:
		registers->argument = command->argument;
		registers->transferMode = 0;
		command->startTime = sceKernelGetSystemTimeWide();
		registers->command = commandFlags | (command->command << 8);
		return 0;
	case 4:
		registers->normalSignalEnable &= ~1;
		if (flags & 0x400) {
			sceSdifAdmaKick(command, commandFlags);
			return 0;
		}
		port->dataCommand = command;
		registers->timeoutControl = port->timeoutControl;
		registers->hostControl &= 0xe7;
		flags = command->flags;
		multiMode = (flags & 0x100) ? 50 : 34;
		singleMode = (flags & 0x100) ? 18 : 2;
		multiAutoMode = (flags & 0x100) ? 54 : 38;
		singleAutoMode = (flags & 0x100) ? 22 : 6;
		noCountAutoMode = (flags & 0x100) ? 20 : 4;
		noCountMode = (flags & 0x100) ? 16 : 0;
		registers->blockSize = command->blockSize;
		if (flags & 0x800) {
			multiMode = multiAutoMode;
			singleMode = singleAutoMode;
			noCountMode = noCountAutoMode;
		}
		registers->blockCount = command->blockCount;
		transferMode = singleMode;
		if (command->blockCount != 1)
			transferMode = command->blockCount > 1 ? multiMode : noCountMode;
		registers->argument = command->argument;
		registers->transferMode = transferMode;
		command->startTime = sceKernelGetSystemTimeWide();
		registers->command = commandFlags | (command->command << 8) | 0x20;
		return 0;
	default:
		return 0;
	}
}

void _sceSdifFinalizeCmd(SdifContextGlobal *context)
{
	SdifPortState *port = &context->port;
	SdifCommand *command = port->currentCommand;
	SdifCommand *next;
	SdifCommand *secondary;
	int errorCode;

	if (port->index != 2)
		port->registers->clockControl &= ~4;
	if (command->state != 2) {
		_sceSdifCmdQueueDump(&port->pendingCommands);
		_sceSdifCmdQueueDump(&port->freeCommands);
		return;
	}
	command->state = 3;
	port->currentCommand = NULL;
	if (command->errorCode != 0)
		goto nextCommand;
	if (command->command == 55 && !(command->response[0] & 0x20)) {
		command->errorCode = 0x80320105;
		goto nextCommand;
	}
	next = command->secondaryCommand;
	if (!next) {
nextCommand:
		next = sdifCommandQueuePop(&port->pendingCommands);
	}
	if (next)
		_sceSdifSendCmdCore(context, next, 1);
	if ((command->flags & 0x100400) == 0x100400)
		freeAdmaDescriptorArea(command);
	errorCode = command->errorCode;
	if (errorCode != 0) {
		secondary = command->secondaryCommand;
		if (command->callback) {
			command->flags |= 0x40000000;
			command->callback(command);
		}
		if (secondary) {
			secondary->errorCode = errorCode;
			secondary->flags |= 0x40000000;
			if (secondary->callback)
				secondary->callback(secondary);
		}
	} else if (command->callback) {
		command->callback(command);
	}
}

#if !SDIF_NSKBL
int deinit(void)
{
	SdifContextGlobal *context;
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	SceKernelIntrStatus intrState;
	uint32_t index = 0;
	int interrupt;

	sceKernelUnregisterSysEventHandler(gSdifState.sysEventHandler);
	if (gSdifState.portCount != 0) {
		context = &gSdifContexts[0];
		do {
			port = &context->port;
			intrState = sdifSpinLock(&port->lock);
			registers = port->registers;
			__builtin_prefetch(&context[2].port.registers);
			__builtin_prefetch(&context[2].port.part);
			registers->clockControl = 0;
			registers->hostControl = 0;
			registers->powerControl = 0;
			port->deviceType = 0;
			port->part = NULL;
			port->removedDeviceType = 0;
			port->removedPart = NULL;
			sdifSpinUnlock(&port->lock, intrState);
			interrupt = port->index + 220;
			sceKernelDisableIntr(interrupt);
			sceKernelReleaseIntrHandler(interrupt);
			sceKernelDeleteEventFlag(port->eventFlag);
			sceKernelFinalizeFastMutex(&port->mutex);
			sceKernelFreeMemBlock(port->descriptorMemblock);
			sceKernelFreeMemBlock(port->registerMemblock);
			ScePervasiveForDriver_3D8A8712(index);
			ScePervasiveForDriver_4E43299D(index);
			context++;
			index++;
		} while (gSdifState.portCount > index);
	}
	return 0;
}
#endif

SdifContextGlobal *SceSdifForDriver_DC8F52F8(unsigned int index)
{
	if (index > 2)
		return NULL;
	return &gSdifContexts[index];
}

int sdifLockPort(SdifContextGlobal *context)
{
	if (sdifIsPolling(&context->port))
		return 0;
	return lockFastMutex(&context->port.mutex);
}

int sdifUnlockPort(SdifContextGlobal *context)
{
	if (sdifIsPolling(&context->port))
		return 0;
	return sceKernelUnlockFastMutex(&context->port.mutex);
}

int _sceSdifReset(SdifContextGlobal *context, int flags)
{
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers = port->registers;
	uint32_t revision;
	uint32_t index;
	uint16_t interrupts;
	int retries;

	if (flags & 1) {
		revision = ScePervasiveForDriver_714EEFB7() & 0x1ff00;
		index = port->index;
		if (revision == 0) {
			if (SceSblAIMgrForDriver_C6E83F34()) {
				if (index != 0) {
					ScePervasiveForDriver_9F8E589C(index, 0);
					goto resetPort;
				}
				ScePervasiveForDriver_9F8E589C(0, 512);
				goto resetEmmc;
			}
			if (index > 1)
				goto resetPort;
			ScePervasiveForDriver_9F8E589C(index, 512);
		} else if (revision == 0x100) {
			if ((sdifSysconVersion() & 0xff0000) < 0x800000) {
				if (index != 0) {
					if (index - 1 <= 1)
						ScePervasiveForDriver_043B33F5(index, 2);
					goto resetPort;
				}
				ScePervasiveForDriver_043B33F5(0, 4);
			} else {
				if (index != 0) {
					if (index == 1) {
						if (SceSysrootForDriver_7B7F8171()) {
							ScePervasiveForDriver_043B33F5(1, 6);
							goto resetPort;
						}
					} else if (index != 2) {
						goto resetPort;
					}
					ScePervasiveForDriver_043B33F5(index, 3);
					goto resetPort;
				}
				ScePervasiveForDriver_043B33F5(0, 5);
			}
			goto resetEmmc;
		}
		if (index != 0) {
resetPort:
			ScePervasiveForDriver_3D8A8712(index);
		} else {
resetEmmc:
			if (gSdifState.skipReset == 1) {
				SceDebugForDriver_02B04343(15, SCE_DBGINFO(1020),
						"Skip sdif port[%d] reset\n", 0);
			} else {
				ScePervasiveForDriver_3D8A8712(0);
				SceSblSmSchedProxyForKernel_7894B6F0(3, 0, 0, 0);
			}
		}
		ScePervasiveForDriver_D326E15F(index);
		ScePervasiveForDriver_3CF7A012(index);
		port->timeoutControl = 14;
		switch (index) {
		case 0:
			port->voltages = 128;
			ScePervasiveForDriver_453BD2EC(0, 1);
			break;
		case 1:
		case 3:
			port->voltages = 0x300000;
			ScePervasiveForDriver_453BD2EC(index, 0);
			break;
		case 2:
			port->voltages = 128;
			ScePervasiveForDriver_453BD2EC(2, 1);
			break;
		default:
			port->voltages = 0;
			break;
		}
	}
	if (flags & 2) {
		registers->softwareReset = 1;
		while (registers->softwareReset & 1)
			;
		interrupts = port->index == 2 ? 507 : 251;
		registers->timeoutControl = port->timeoutControl;
		retries = 102;
		while (!(registers->presentState & 0x20000)) {
			if (port->index == 1 && --retries == 0)
				break;
		}
		registers->normalInterruptEnable = 0;
		registers->errorInterruptEnable = 0;
		registers->normalSignalEnable = 0;
		registers->errorSignalEnable = 0;
		registers->normalInterruptStatus = registers->normalInterruptStatus;
		registers->errorInterruptStatus = registers->errorInterruptStatus;
		while (registers->normalInterruptStatus)
			;
		while (registers->errorInterruptStatus)
			;
		registers->normalInterruptEnable = interrupts;
		registers->errorInterruptEnable = 895;
		registers->normalSignalEnable = interrupts;
		registers->errorSignalEnable = 895;
	}
	return 0;
}

#if !SDIF_NSKBL
int sceSdifResume0(int phase, int event)
{
	SdifContextGlobal *context;
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	SdifMmcContext *part;
	SceKernelIntrStatus intrState;
	uint32_t index;
	int result;

	if (phase != 0) {
		if (phase == 1) {
			if (event == 0x10009) {
				gSdifState.skipReset = 0;
				if (gSdifState.portCount != 0) {
					index = 0;
					context = &gSdifContexts[0];
					do {
						port = &context->port;
						port->polling = 1;
						intrState = sdifSpinLock(&port->lock);
						_sceSdifReset(context, 3);
						sdifSpinUnlock(&port->lock, intrState);
						if (index != 0) {
							port->polling = 0;
							sceKernelEnableIntr(index + 220);
						} else {
							part = NULL;
							result = sdifStartMmcInitialization(0, &part);
							if (result < 0)
								SceDebugForDriver_02B04343(15, SCE_DBGINFO(719),
											"Can't resume eMMC. 0x%x\n", result);
						}
						++index;
						++context;
					} while (index < gSdifState.portCount);
				}
				gSdifState.skipReset = 0;
			} else if (event == 0x1000c) {
				result = sdifCompleteMmcInitialization(0);
				if (result < 0)
					SceDebugForDriver_02B04343(15, SCE_DBGINFO_FUNC(744, "sceSdifResume1"),
								"Can't resume eMMC. 0x%x\n", result);
				gSdifContexts[0].port.polling = 0;
				sceKernelEnableIntr(220);
			}
		}
	} else if (event == 0x400c && gSdifState.portCount != 0) {
		context = &gSdifContexts[0];
		index = 0;
		do {
			port = &context->port;
			sceKernelDisableIntr(index + 220);
			intrState = sdifSpinLock(&port->lock);
			registers = port->registers;
			__builtin_prefetch(&context[3].port.registers);
			registers->clockControl = 0;
			registers->hostControl = 0;
			registers->powerControl = 0;
			port->deviceType = 0;
			port->part = NULL;
			port->removedDeviceType = 0;
			port->removedPart = NULL;
			sdifSpinUnlock(&port->lock, intrState);
			ScePervasiveForDriver_3D8A8712(index);
			ScePervasiveForDriver_4E43299D(index);
			++index;
			++context;
		} while (index < gSdifState.portCount);
	}
	return 0;
}
#endif

#if !SDIF_NSKBL
int sceSdifInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SdifContextGlobal *context;
	SdifPortState *port;
	SdifCommand *command;
	SceKernelIntrStatus intrState;
	const SceKernelDebugInfo *debugInfo;
	const char *name;
	void *registerBase;
	void *address;
	uint32_t leadingPhysicalAddress;
	uint32_t trailingPhysicalAddress;
	uint32_t index = 0;
	uint32_t commandIndex;
	int interrupt;
	int result;

	memset(gSdifContexts, 0, sizeof(gSdifContexts));
	gSdifState.portCount = 3;
	do {
		context = &gSdifContexts[index];
		port = &context->port;
		name = sdifMemblockNames[index];
		port->index = index;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.paddr = index < SDIF_PORTS ? sdifRegisterAddresses[index] : 0;
		opt.attr = 2;
		result = sceKernelAllocMemBlockForKernel(name, 0x20100806,
					       index < SDIF_PORTS ? SDIF_REG_SIZE : 0, &opt);
		if (result < 0)
			goto cleanupPorts;
		port->registerMemblock = result;
		sceKernelGetMemBlockBase(result, &registerBase);
		port->registers = registerBase;
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x200000;
		result = sceKernelAllocMemBlockForKernel(name, 0x1020d006, 0x10000, &opt);
		if (result < 0)
			goto freeRegisters;
		port->descriptorMemblock = result;
		sceKernelGetMemBlockBase(result, &port->descriptorBuffer);
		result = sceKernelInitializeFastMutex(&port->mutex, name, 2, NULL);
		if (result < 0)
			goto freeDescriptors;
		result = sceKernelCreateEventFlag(name, 0, 0, NULL);
		port->eventFlag = result;
		if (result < 0)
			goto finalizeMutex;
		interrupt = port->index + 220;
		result = sceKernelRegisterIntrHandler(interrupt, name, 0,
						      (SceKernelIntrHandler)sceSdifInterrupt, context, 128, 15,
						      &gSdifIntrOpt);
		if (result < 0)
			goto deleteEventFlag;
		sdifCommandQueueInitialize(&port->freeCommands);
		sdifCommandQueueInitialize(&port->pendingCommands);
		command = &context->commands[0];
		commandIndex = 0;
		do {
			command->state = 0;
			command->index = commandIndex;
			__builtin_prefetch(&command[3].state);
			address = command->leadingBuffer;
			result = sceKernelVAtoPA(address, &leadingPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3710, "_sceSdifInitDmaAlignmentBuffer");
				goto addressError;
			}
			address = command->trailingBuffer;
			result = sceKernelVAtoPA(address, &trailingPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3717, "_sceSdifInitDmaAlignmentBuffer");
				goto addressError;
			}
			command->leadingPhysicalAddress = leadingPhysicalAddress;
			command->trailingPhysicalAddress = trailingPhysicalAddress;
			address = command->defaultDescriptors;
			result = sceKernelVAtoPA(address, &command->defaultDescriptorPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3690, "_sceSdifInitCmd");
				goto addressError;
			}
			++commandIndex;
			sdifCommandQueuePush(&port->freeCommands, command++);
		} while (commandIndex != 16);
		++index;
		intrState = sdifSpinLock(&port->lock);
		_sceSdifReset(context, 3);
		sdifSpinUnlock(&port->lock, intrState);
		sceKernelEnableIntr(interrupt);
	} while (gSdifState.portCount > index);
	result = sceKernelRegisterSysEventHandler("SceSdif",
						 (SceKernelSysEventHandler)sceSdifResume0, NULL);
	gSdifState.sysEventHandler = result;
#if FW_DEBUG
	if (result < 0)
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(627),
					     "Can't register sdif sys event handler\n");
#endif
	return 0;

addressError:
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0, debugInfo,
				     "VA to PA error:0x%x  VA:%p\n", result, address);
#else
	(void)debugInfo;
#endif
	sceKernelReleaseIntrHandler(interrupt);
deleteEventFlag:
	sceKernelDeleteEventFlag(port->eventFlag);
finalizeMutex:
	sceKernelFinalizeFastMutex(&port->mutex);
freeDescriptors:
	sceKernelFreeMemBlock(port->descriptorMemblock);
freeRegisters:
	sceKernelFreeMemBlock(port->registerMemblock);
cleanupPorts:
	if (index != 0) {
		commandIndex = 0;
		context = &gSdifContexts[0];
		do {
			port = &context->port;
			interrupt = port->index + 220;
			__builtin_prefetch(&context[3].port.index);
			sceKernelDisableIntr(interrupt);
			sceKernelReleaseIntrHandler(interrupt);
			sceKernelDeleteEventFlag(port->eventFlag);
			sceKernelFinalizeFastMutex(&port->mutex);
			sceKernelFreeMemBlock(port->descriptorMemblock);
			sceKernelFreeMemBlock(port->registerMemblock);
			context++;
			commandIndex++;
		} while (commandIndex != index);
	}
	return result;
}
#else
/*
 * The boot loader's bring-up: the host registers are used at their physical addresses, and
 * a port needs only its descriptor buffer (no mutex, event flag or interrupt handler).
 */
int sceSdifInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SdifContextGlobal *context;
	SdifPortState *port;
	SdifCommand *command;
	SceKernelIntrStatus intrState;
	const SceKernelDebugInfo *debugInfo;
	const char *name;
	void *address;
	uint32_t leadingPhysicalAddress;
	uint32_t trailingPhysicalAddress;
	uint32_t index = 0;
	uint32_t commandIndex;
	int result;

	memset(gSdifContexts, 0, sizeof(gSdifContexts));
	gSdifState.portCount = 3;
	do {
		context = &gSdifContexts[index];
		port = &context->port;
		name = sdifMemblockNames[index];
		port->index = index;
		port->registers = (volatile SdifHostRegisters *)
			(index < SDIF_PORTS ? sdifRegisterAddresses[index] : 0);
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x200000;
		result = sceKernelAllocMemBlockForKernel(name, 0x1020d006, 0x10000, &opt);
		if (result < 0)
			goto cleanupPorts;
		port->descriptorMemblock = result;
		sceKernelGetMemBlockBase(result, &port->descriptorBuffer);
		sdifCommandQueueInitialize(&port->freeCommands);
		sdifCommandQueueInitialize(&port->pendingCommands);
		command = &context->commands[0];
		commandIndex = 0;
		do {
			command->state = 0;
			command->index = commandIndex;
			__builtin_prefetch(&command[3].state);
			address = command->leadingBuffer;
			result = sceKernelVAtoPA(address, &leadingPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3710, "_sceSdifInitDmaAlignmentBuffer");
				goto addressError;
			}
			address = command->trailingBuffer;
			result = sceKernelVAtoPA(address, &trailingPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3717, "_sceSdifInitDmaAlignmentBuffer");
				goto addressError;
			}
			command->leadingPhysicalAddress = leadingPhysicalAddress;
			command->trailingPhysicalAddress = trailingPhysicalAddress;
			address = command->defaultDescriptors;
			result = sceKernelVAtoPA(address, &command->defaultDescriptorPhysicalAddress);
			if (result < 0) {
				debugInfo = SCE_DBGINFO_FUNC(3690, "_sceSdifInitCmd");
				goto addressError;
			}
			++commandIndex;
			sdifCommandQueuePush(&port->freeCommands, command++);
		} while (commandIndex != 16);
		intrState = sdifSpinLock(&port->lock);
		_sceSdifReset(context, 3);
		sdifSpinUnlock(&port->lock, intrState);
		++index;
	} while (gSdifState.portCount > index);
	return 0;

addressError:
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0, debugInfo,
				     "VA to PA error:0x%x  VA:%p\n", result, address);
#else
	(void)debugInfo;
#endif
	sceKernelFreeMemBlock(port->descriptorMemblock);
cleanupPorts:
	if (index != 0) {
		commandIndex = 0;
		context = &gSdifContexts[0];
		do {
			sceKernelFreeMemBlock(context->port.descriptorMemblock);
			context++;
			commandIndex++;
		} while (commandIndex != index);
	}
	return result;
}
#endif

#if !SDIF_NSKBL
int module_start(SceSize arglen, const void *argp)
{
	return (unsigned int)sceSdifInit() >> 31;
}
#endif

int sceSdifReset(SdifContextGlobal *context, int flags)
{
	SceKernelIntrStatus intrState;
	int result;

	intrState = sdifSpinLock(&context->port.lock);
	result = _sceSdifReset(context, flags);
	sdifSpinUnlock(&context->port.lock, intrState);
	return result;
}

int sdifPowerOn(SdifContextGlobal *context, uint8_t powerControl)
{
	SdifPortState *port = &context->port;
	SceKernelIntrStatus intrState;
	volatile SdifHostRegisters *registers;
	int retries = 102;

	intrState = sdifSpinLock(&port->lock);
	registers = port->registers;
	while (!(registers->presentState & 0x20000)) {
		if (port->index == 1 && --retries == 0)
			break;
	}
	if (registers->presentState & 0x10000) {
		registers->powerControl &= ~1U;
		registers->powerControl |= powerControl;
		registers->powerControl |= 1;
		port->powerControl = powerControl;
		registers->clockControl = 0;
		registers->clockControl = 0x8001;
		while (!(registers->clockControl & 2))
			;
		if (port->index == 2)
			port->registers->clockControl |= 4;
		registers->hostControl = 0;
	}
	sdifSpinUnlock(&port->lock, intrState);
	return 0;
}

int sdifIsCardInserted(SdifContextGlobal *context)
{
	return (context->port.registers->presentState & 0x10000) != 0;
}

int sdifIsWriteProtected(SdifContextGlobal *context)
{
	return ((context->port.registers->presentState ^ 0x80000) & 0x80000) != 0;
}

int sdifSetClock(SdifContextGlobal *context, uint32_t frequency, int highSpeed)
{
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	SceKernelIntrStatus intrState;
	uint8_t shift;
	uint16_t clockControl;
	uint8_t hostControl;
	int retries = 41;
	volatile int delay;

	intrState = sdifSpinLock(&context->port.lock);
	if (frequency > 47999999)
		shift = 0;
	else if (frequency > 23999999)
		shift = 1;
	else if (frequency > 11999999)
		shift = 2;
	else if (frequency > 5999999)
		shift = 3;
	else if (frequency > 2999999)
		shift = 4;
	else if (frequency > 1499999)
		shift = 5;
	else if (frequency > 749999)
		shift = 6;
	else if (frequency > 374999)
		shift = 7;
	else {
		clockControl = 0x8001;
		goto setClock;
	}
	clockControl = ((128 << shift) & 0xff00) | 1;
setClock:
	port = &context->port;
	registers = port->registers;
	registers->clockControl = 0;
	registers->clockControl = clockControl;
	while (!(registers->clockControl & 2)) {
		if (--retries == 0) {
			sdifSpinUnlock(&port->lock, intrState);
			return 0x80320004;
		}
		for (delay = 25000; delay != 0; --delay)
			;
	}
	if (port->index == 2)
		registers->clockControl |= 4;
	hostControl = registers->hostControl;
	if (highSpeed)
		registers->hostControl = hostControl | 4;
	else
		registers->hostControl = hostControl & 0xfb;
	sdifSpinUnlock(&port->lock, intrState);
	return 0;
}

int sdifSetBusWidth(SdifContextGlobal *context, int width)
{
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers;
	SceKernelIntrStatus intrState;
	uint8_t hostControl;

	intrState = sdifSpinLock(&port->lock);
	registers = port->registers;
	port->busWidth = width;
	hostControl = registers->hostControl;
	if (width == 8)
		hostControl = (hostControl & ~2) | 0x20;
	else if (width == 4)
		hostControl = (hostControl & ~0x20) | 2;
	else
		hostControl &= 0xdd;
	registers->hostControl = hostControl;
	sdifSpinUnlock(&port->lock, intrState);
	return 0;
}

int _sceSdifDumpReg(SdifContextGlobal *context)
{
	volatile SdifHostRegisters *registers = context->port.registers;

	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1437),
				"s+++++++++++++++++++++++++++++++++++++++i\n");
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1438), "%d\n", context->port.index);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1439), "0x%04X 0x%04X 0x%08X\n",
				registers->normalInterruptStatus, registers->errorInterruptStatus,
				registers->presentState);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1440), "0x%04X 0x%04X 0x%08X\n",
				registers->transferMode, registers->command, registers->argument);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1441), "0x%08X 0x%08X 0x%08X 0x%08X\n",
				registers->response[0], registers->response[1],
				registers->response[2], registers->response[3]);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1442), "0x%08X 0x%08X 0x%08X\n",
				registers->dmaAddress, registers->blockSize, registers->blockCount);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1443), "0x%08X 0x%08X 0x%08X\n",
				registers->clockControl, registers->hostControl, registers->powerControl);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1444), "0x%08X 0x%08X 0x%08X\n",
				registers->blockGapControl, registers->wakeupControl, registers->timeoutControl);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1445), "0x%04X 0x%04X\n",
				registers->normalInterruptEnable, registers->errorInterruptEnable);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1446), "0x%04X 0x%04X 0x%04X\n",
				registers->normalSignalEnable, registers->errorSignalEnable, registers->autoCmd12Error);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1447), "0x%08X 0x%08X\n",
				registers->capabilities, registers->maximumCurrent);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1448), "0x%04X 0x%04X\n",
				registers->forceAutoCmd12Error, registers->forceErrorInterrupt);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1449), "0x%04X 0x%04X\n",
				registers->AdmaErrStatus, registers->admaSystemAddress);
	SceDebugForDriver_02B04343(0, SCE_DBGINFO(1450), "0x%04X 0x%04X\n",
				registers->ceataControl, registers->slotInterruptStatus);
	return SceDebugForDriver_02B04343(0, SCE_DBGINFO(1451),
				      "d---------------------------------------f\n");
}

int sceSdifInterrupt(int interrupt, SdifContextGlobal *context, unsigned int intrPriority)
{
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers = port->registers;
	SceKernelIntrStatus intrState;
	uint16_t normalStatus;
	unsigned int errorStatus;
	int removed = 0;
	SdifCommand *command;

	intrState = sdifSpinLock(&port->lock);
	normalStatus = registers->normalInterruptStatus;
	errorStatus = registers->errorInterruptStatus;
	if (errorStatus & 0x200) {
		unsigned int admaStatus = registers->AdmaErrStatus;
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(1553),
			"sceSdifInterrupt(SDIF%d): AdmaErrStatus:0x%x\n",
			port->index, admaStatus);
#else
		(void)admaStatus;
#endif
	}
	if (errorStatus & 0x100) {
		unsigned int autoCmdStatus = registers->autoCmd12Error;
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(1569),
			"sceSdifInterrupt(SDIF%d): A12_ERR_STS=0x%04X\n",
			port->index, autoCmdStatus);
#else
		(void)autoCmdStatus;
#endif
		registers->errorInterruptStatus = 0x100;
	}
	if (normalStatus & 0x100) {
		registers->normalInterruptStatus = 0x100;
		if (port->interruptMask & 0x1fc)
			sdifStartFunctionInterrupt(context, interrupt, intrPriority);
	}
	if ((normalStatus & 0xc0) == 0xc0 &&
	    !(port->registers->presentState & 0x10000)) {
		registers->normalInterruptStatus = 0x40;
		normalStatus &= ~0x40;
	}
	if (normalStatus & 0x80) {
		if (port->part) {
			port->removedDeviceType = port->deviceType;
			port->removedPart = port->part;
		}
		port->deviceType = 0;
		port->part = NULL;
		registers->normalInterruptStatus = 0x80;
		removed = 1;
		if (port->interruptMask & 2)
			sceKernelCallSubIntrHandler(interrupt, 1, (void *)0x80);
	}
	if (normalStatus & 0x40) {
		registers->normalInterruptStatus = 0x40;
		if (port->interruptMask & 1)
			sceKernelCallSubIntrHandler(interrupt, 0, (void *)0x40);
	}
	if ((normalStatus & 1) || (errorStatus & 0xf)) {
		command = port->currentCommand;
		registers->normalInterruptStatus = 1;
		registers->errorInterruptStatus = 0xf;
		if (command) {
			command->errorCode = 0;
			if (errorStatus) {
				uint16_t errorEnable;
				int errorCode;
				volatile SdifHostRegisters *recoveryRegisters;

				if (errorStatus & 1)
					errorCode = 0x80320002;
				else if (errorStatus & 2)
					errorCode = 0x80320007;
				else if (errorStatus & 4)
					errorCode = 0x80320008;
				else if (errorStatus & 8)
					errorCode = 0x80320009;
				else if (errorStatus & 0x10)
					errorCode = 0x80320003;
				else if (errorStatus & 0x20)
					errorCode = 0x8032000a;
				else if (errorStatus & 0x40)
					errorCode = 0x8032000b;
				else if (errorStatus & 0x200)
					errorCode = 0x8032000c;
				else if (errorStatus & 0x100)
					errorCode = 0x8032000d;
				else
					errorCode = 0x8032001d;
				command->errorCode = errorCode;
				command->flags |= 0x40000000;
				if (command->command != 3 && command->command != 16 &&
				    command->command != 55 && (command->command & ~8U) != 5 &&
				    command->command != 8 && command->command != 52) {
#if FW_DEBUG
					SceDebugForDriver_D9703808(1, 0,
						SCE_DBGINFO_FUNC(1957, "_sceSdifCmdInterrupt"),
						"ErrIntStatus:0x%x errorCode:0x%x\n",
						errorStatus, command->errorCode);
#endif
					_sceSdifDumpReg(context);
				}
				recoveryRegisters = port->registers;
				errorEnable = recoveryRegisters->errorInterruptEnable;
				recoveryRegisters->errorInterruptEnable = 0;
				while (recoveryRegisters->errorInterruptEnable != 0)
					;
				if (errorStatus & 0xf) {
					recoveryRegisters->softwareReset = 2;
					while (recoveryRegisters->softwareReset & 2)
						;
				}
				if (errorStatus & 0x70) {
					recoveryRegisters->softwareReset = 4;
					while (recoveryRegisters->softwareReset & 4)
						;
				}
				recoveryRegisters->errorInterruptStatus = recoveryRegisters->errorInterruptStatus;
				while (recoveryRegisters->errorInterruptStatus != 0)
					;
				recoveryRegisters->errorInterruptEnable = errorEnable;
				while (recoveryRegisters->errorInterruptEnable != errorEnable)
					;
				_sceSdifFinalizeCmd(context);
			} else if (normalStatus & 1) {
				uint32_t flags = command->flags;
				volatile SdifHostRegisters *responseRegisters = port->registers;

				port->commandComplete = 1;
				command->errorCode = 0;
				switch (flags & 0xf8) {
				case 0x10:
				case 0x28:
					command->response[0] = responseRegisters->response[0];
					if (flags & 0x800)
						command->response[3] = responseRegisters->response[3];
					break;
				case 0x30: {
					uint32_t first = responseRegisters->response[0];
					uint32_t second = responseRegisters->response[1];
					uint32_t third = responseRegisters->response[2];
					uint32_t fourth = responseRegisters->response[3];

					command->response[0] = first << 8;
					command->response[1] = (second << 8) | (first >> 24);
					command->response[2] = (third << 8) | (second >> 24);
					command->response[3] = (fourth << 8) | (third >> 24);
					break;
				}
				case 0x40:
				case 0x50:
				case 0x60:
				case 0x78:
				case 0x80:
				case 0x90:
					command->response[0] = responseRegisters->response[0];
					break;
				}
				if (!port->dataCommand && !(flags & 8)) {
					command->flags = flags | 0x80000000;
					_sceSdifFinalizeCmd(context);
				}
			}
		}
	}
	if ((normalStatus & 0x3a) || (errorStatus & 0x270)) {
		command = port->currentCommand;
		registers->normalInterruptStatus = 0x3a;
		registers->errorInterruptStatus = 0x270;
		if (command) {
			command->endTime = sceKernelGetSystemTimeWide();
			command->errorCode = 0;
			if (port->dataCommand) {
				if (errorStatus) {
					uint16_t errorEnable;
					int errorCode;
					volatile SdifHostRegisters *recoveryRegisters;

					port->dataCommand = NULL;
					if (errorStatus & 1)
						errorCode = 0x80320002;
					else if (errorStatus & 2)
						errorCode = 0x80320007;
					else if (errorStatus & 4)
						errorCode = 0x80320008;
					else if (errorStatus & 8)
						errorCode = 0x80320009;
					else if (errorStatus & 0x10)
						errorCode = 0x80320003;
					else if (errorStatus & 0x20)
						errorCode = 0x8032000a;
					else if (errorStatus & 0x40)
						errorCode = 0x8032000b;
					else if (errorStatus & 0x200)
						errorCode = 0x8032000c;
					else if (errorStatus & 0x100)
						errorCode = 0x8032000d;
					else
						errorCode = 0x8032001d;
					command->errorCode = errorCode;
					command->flags |= 0x40000000;
					recoveryRegisters = port->registers;
					errorEnable = recoveryRegisters->errorInterruptEnable;
					recoveryRegisters->errorInterruptEnable = 0;
					while (recoveryRegisters->errorInterruptEnable != 0)
						;
					if (errorStatus & 0xf) {
						recoveryRegisters->softwareReset = 2;
						while (recoveryRegisters->softwareReset & 2)
							;
					}
					if (errorStatus & 0x70) {
						recoveryRegisters->softwareReset = 4;
						while (recoveryRegisters->softwareReset & 4)
							;
					}
					recoveryRegisters->errorInterruptStatus = recoveryRegisters->errorInterruptStatus;
					while (recoveryRegisters->errorInterruptStatus != 0)
						;
					recoveryRegisters->errorInterruptEnable = errorEnable;
					while (recoveryRegisters->errorInterruptEnable != errorEnable)
						;
					_sceSdifFinalizeCmd(context);
				} else {
					if (normalStatus & 8)
						port->registers->dmaAddress = port->registers->dmaAddress;
					if ((normalStatus & 0x20) && (command->flags & 0x700) == 0x100) {
						uint8_t *buffer = command->buffer;
						volatile SdifHostRegisters *dataRegisters = port->registers;
						unsigned int offset = 0, length = command->blockSize;
						int aligned = ((uintptr_t)buffer & 3) == 0;

						while (!(dataRegisters->presentState & 0x800))
							;
						while (offset < length) {
							unsigned int remaining = length - offset;

							if (remaining == 1) {
								buffer[offset++] = dataRegisters->buffer.byte;
								length = command->blockSize;
							} else if (remaining <= 3) {
								uint16_t value = dataRegisters->buffer.halfword;

								buffer[offset] = value;
								buffer[offset + 1] = value >> 8;
								offset += 2;
								length = command->blockSize;
							} else {
								uint32_t value = dataRegisters->buffer.word;

								if (aligned)
									*(uint32_t *)(buffer + offset) = value;
								else {
									buffer[offset] = value;
									buffer[offset + 1] = value >> 8;
									buffer[offset + 2] = value >> 16;
									buffer[offset + 3] = value >> 24;
									length = command->blockSize;
								}
								offset += 4;
							}
						}
						command->buffer = (uint8_t *)command->buffer + length;
					}
					if ((normalStatus & 0x10) && (command->flags & 0x700) == 0x200) {
						const uint8_t *buffer = command->buffer;
						volatile SdifHostRegisters *dataRegisters = port->registers;
						unsigned int offset = 0, length;
						int aligned = ((uintptr_t)buffer & 3) == 0;

						while (!(dataRegisters->presentState & 0x400))
							;
						while ((length = command->blockSize) > offset) {
							unsigned int remaining = length - offset;

							if (remaining == 1)
								dataRegisters->buffer.byte = buffer[offset++];
							else if (remaining <= 3) {
								dataRegisters->buffer.halfword = buffer[offset] |
									((uint16_t)buffer[offset + 1] << 8);
								offset += 2;
							} else {
								if (aligned)
									dataRegisters->buffer.word = *(const uint32_t *)(buffer + offset);
								else
									dataRegisters->buffer.word = buffer[offset] |
										((uint32_t)buffer[offset + 1] << 8) |
										((uint32_t)buffer[offset + 2] << 16) |
										((uint32_t)buffer[offset + 3] << 24);
								offset += 4;
							}
						}
						command->buffer = (uint8_t *)command->buffer + length;
					}
					if (normalStatus & 2) {
						if (port->commandComplete) {
							command->flags |= 0x80000000;
							port->dataCommand = NULL;
							_sceSdifFinalizeCmd(context);
						} else
							port->dataCommand = NULL;
					}
				}
			} else if ((normalStatus & 2) && (port->currentCommand->flags & 8)) {
				command->flags |= 0x80000000;
				_sceSdifFinalizeCmd(context);
			}
		}
	}
	if (removed) {
		command = port->currentCommand;
		if (command) {
			SdifCommand *secondary = command->secondaryCommand;

			if (command->callback) {
				command->errorCode = 0x8032001a;
				command->flags |= 0x40000000;
				command->callback(port->currentCommand);
			}
			if (secondary) {
				secondary->errorCode = 0x8032001a;
				secondary->flags |= 0x40000000;
				if (secondary->callback)
					secondary->callback(secondary);
			}
			port->currentCommand = NULL;
		}
		while ((command = sdifCommandQueuePop(&port->pendingCommands)) != NULL) {
			SdifCommand *secondary = command->secondaryCommand;

			if (command->callback) {
				command->errorCode = 0x8032001a;
				command->flags |= 0x40000000;
				command->callback(command);
			}
			if (secondary) {
				secondary->errorCode = 0x8032001a;
				secondary->flags |= 0x40000000;
				if (secondary->callback)
					secondary->callback(secondary);
			}
		}
	}
	sdifSpinUnlock(&port->lock, intrState);
	sdifDispatchFunctionInterrupt(context);
	return -1;
}

SDIF_SYNC_STATIC int sceSdifWaitCmdCompletionCore(SdifCommand *command)
{
	SdifContextGlobal *context = command->context;
	SdifPortState *port = &context->port;
	int result;

	if (sdifIsPolling(port))
		goto polling;
	if (port->index == 1 && (command->command == 8 || command->command == 6) &&
	    port->deviceType == 1) {
		SceUInt timeout = 5000000;

		SCE_DBG_ASSERT(command->secondaryCommand == NULL, 2617);
		result = sceKernelWaitEventFlag(command->eventFlag, 0x11, 5, NULL, &timeout);
		if (result < 0) {
			SceKernelIntrStatus intrState;
			SdifCommand *current;

			intrState = sdifSpinLock(&port->lock);
			current = port->currentCommand;
			if (current) {
				current->errorCode = 0x80320021;
				current->flags |= 0x40000000;
				port->currentCommand = NULL;
			} else {
				int clearResult = sceKernelClearEventFlag(command->eventFlag, 0);

#if FW_DEBUG
				if (clearResult)
					SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO(2640),
						"%s clear event flag error:0x%x\n",
						"sceSdifWaitCmdCompletionCore", clearResult);
#else
				(void)clearResult;
#endif
			}
			sdifSpinUnlock(&port->lock, intrState);
			result = 0x80320021;
			SceDebugForDriver_02B04343(0, SCE_DBGINFO(2648), "0x%x\n", result);
		}
	} else
		result = sceKernelWaitEventFlag(command->eventFlag, 0x11, 5, NULL, NULL);
	if (result)
		goto checkResult;
completed:
	result = command->errorCode;
checkResult:
	if (result == (int)0x8032000c) {
		_sceSdifDumpReg(command->context);
		sceSdifDumpAdmaInfo(command);
	}
	return result;
polling:
	{
		volatile SdifHostRegisters *registers = port->registers;

		while (!(command->flags & 0xc0000000)) {
			while (!registers->normalInterruptStatus && !registers->errorInterruptStatus)
				;
			sceSdifInterrupt(port->index + 220, context, 0);
		}
	}
	goto completed;
}

SDIF_SYNC_STATIC int sceSdifSendSyncCmd(SdifContextGlobal *context, SdifCommand *command,
					SdifCommand *secondary, int retries, int checkCard)
{
	void *buffer = command->buffer;
	uint32_t flags = command->flags;
	void *secondaryBuffer = secondary ? secondary->buffer : NULL;
	uint32_t secondaryFlags = secondary ? secondary->flags : 0;
	SdifPortState *port = &context->port;
	int result;

	for (;;) {
		SceKernelIntrStatus intrState;
		SdifCommand *completion;

		intrState = sdifSpinLock(&port->lock);
		if (checkCard && (!(port->registers->powerControl & 1) || !port->deviceType)) {
			sdifSpinUnlock(&port->lock, intrState);
			result = 0x8032001a;
			break;
		}
		if (port->currentCommand) {
			sdifCommandQueuePush(&port->pendingCommands, command);
			command->state = 1;
			sdifSpinUnlock(&port->lock, intrState);
		} else {
			result = _sceSdifSendCmdCore(context, command, 0);
			sdifSpinUnlock(&port->lock, intrState);
			if (result)
				break;
		}
		completion = command->secondaryCommand;
		if (!completion)
			completion = command;
		result = sceSdifWaitCmdCompletionCore(completion);
		if (result >= 0 || result == (int)0x8032001a)
			break;
		command->buffer = buffer;
		command->flags = flags;
		if (secondary) {
			secondary->buffer = secondaryBuffer;
			secondary->flags = secondaryFlags;
		}
		if (--retries <= 0)
			break;
	}
	if (!sdifIsPolling(port))
		sceKernelUnlockFastMutex(&port->mutex);
	if (secondary && (secondary->flags & 0x400))
		freeAdmaDescriptorArea(secondary);
	if (command->flags & 0x400)
		freeAdmaDescriptorArea(command);
	return result;
}

#if !SDIF_NSKBL
SdifCommand *sdifRemovePendingCommand(SdifContextGlobal *context, unsigned int index)
{
	SceKernelSpinlock *lock = &context->port.lock;
	SceKernelIntrStatus intrState;
	SdifCommand *command;

	intrState = sdifSpinLock(lock);
	command = sdifCommandQueueRemove(&context->port.pendingCommands, index);
	sdifSpinUnlock(lock, intrState);
	return command;
}

int sdifWaitPendingCommand(SdifContextGlobal *context, unsigned int index,
			  unsigned int reserved, SdifCommand **completed)
{
	SceKernelSpinlock *lock = &context->port.lock;
	SceKernelIntrStatus intrState;
	SdifCommand *command;
	SdifCommand *completion;
	SceUID eventFlag;
	int result;

	(void)reserved;
	intrState = sdifSpinLock(lock);
	command = context->port.currentCommand;
	if (!command || command->index != index)
		command = sdifCommandQueueFind(&context->port.pendingCommands, index);
	if (!command) {
		sdifSpinUnlock(lock, intrState);
		return 0x80320012;
	}
	eventFlag = sceKernelCreateEventFlag("SceSdif", 0, 0, NULL);
	if (eventFlag < 0) {
		sdifSpinUnlock(lock, intrState);
		return eventFlag;
	}
	completion = command->secondaryCommand;
	if (!completion)
		completion = command;
	completion->callback = sdifCompleteSyncCommand;
	completion->eventFlag = eventFlag;
	completion->context = context;
	sdifSpinUnlock(lock, intrState);
	result = sceSdifWaitCmdCompletionCore(completion);
	*completed = completion;
	sceKernelDeleteEventFlag(eventFlag);
	return result;
}

int sdifGetCommandState(SdifContextGlobal *context, unsigned int index)
{
	SceKernelSpinlock *lock = &context->port.lock;
	SceKernelIntrStatus intrState;
	SdifCommand *command;

	intrState = sdifSpinLock(lock);
	command = context->port.currentCommand;
	if (command && command->index == index) {
		sdifSpinUnlock(lock, intrState);
		return 1;
	}
	if (sdifCommandQueueFind(&context->port.pendingCommands, index)) {
		sdifSpinUnlock(lock, intrState);
		return 2;
	}
	sdifSpinUnlock(lock, intrState);
	return 0;
}

int sdifQueueCommand(SdifContextGlobal *context, SdifCommand *command, int priority)
{
	if (!context->port.currentCommand)
		return _sceSdifSendCmdCore(context, command, 0);
	if (priority == 1)
		sdifCommandQueuePushFront(&context->port.pendingCommands, command);
	else
		sdifCommandQueuePush(&context->port.pendingCommands, command);
	command->state = 1;
	return 0;
}

int sdifSendAsyncCommand(SdifContextGlobal *context, SdifCommand *command,
			void (*callback)(SdifCommand *), SdifCommand *secondary,
			void (*secondaryCallback)(SdifCommand *))
{
	SceUID *eventFlag = &context->port.eventFlag;
	uint32_t flags = command->flags;
	SceKernelSpinlock *lock;
	SceKernelIntrStatus intrState;
	int result;

	command->callback = callback;
	command->context = context;
	command->eventFlag = *eventFlag;
	command->secondaryCommand = secondary;
	if (flags & 0x400) {
		command->leadingLength = 0;
		command->trailingLength = 0;
		command->flags = flags | 0x100000;
		result = setAdmaDescriptorsForAlignedData(command);
		if (result)
			return result;
	}
	if (secondary) {
		flags = secondary->flags;
		secondary->context = context;
		secondary->callback = secondaryCallback;
		secondary->eventFlag = *eventFlag;
		secondary->flags = flags & 0x3fffffff;
		secondary->secondaryCommand = NULL;
		if (flags & 0x400) {
			secondary->leadingLength = 0;
			secondary->trailingLength = 0;
			secondary->flags |= 0x100000;
			result = setAdmaDescriptorsForAlignedData(secondary);
			if (result) {
				freeAdmaDescriptorArea(command);
				return result;
			}
		}
	}
	lock = &context->port.lock;
	intrState = sdifSpinLock(lock);
	if ((context->port.registers->powerControl & 1) && context->port.deviceType) {
		if (context->port.currentCommand) {
			sdifCommandQueuePush(&context->port.pendingCommands, command);
			command->state = 1;
			result = 0;
		} else
			result = _sceSdifSendCmdCore(context, command, 0);
		sdifSpinUnlock(lock, intrState);
		return result;
	}
	sdifSpinUnlock(lock, intrState);
	return 0x8032001a;
}
#endif

SDIF_PREPARE_INLINE int sdifPrepareSyncCommand(SdifContextGlobal *context, SdifCommand *command,
					       SdifCommand *secondary, int retries, int checkCard)
{
	SdifPortState *port = &context->port;
	SceUID *eventFlag = &port->eventFlag;
	uint32_t flags;
	int result;

	if (!sdifIsPolling(port)) {
		result = lockFastMutex(&port->mutex);
		if (result < 0)
			return result;
	}
	flags = command->flags;
	command->context = context;
	command->secondaryCommand = secondary;
	command->callback = sdifCompleteSyncCommand;
	command->eventFlag = *eventFlag;
	if (flags & 0x400) {
		command->flags = flags & ~0x100000U;
		command->leadingLength = 0;
		command->trailingLength = 0;
		result = setAdmaDescriptorsForAlignedData(command);
		if (result)
			goto unlock;
	}
	if (secondary) {
		flags = secondary->flags;
		secondary->callback = sdifCompleteSyncCommand;
		secondary->eventFlag = *eventFlag;
		secondary->secondaryCommand = NULL;
		secondary->context = context;
		secondary->flags = flags & 0x3fffffff;
		if (flags & 0x400) {
			secondary->leadingLength = 0;
			secondary->flags = flags & 0x3fefffff;
			secondary->trailingLength = 0;
			result = setAdmaDescriptorsForAlignedData(secondary);
			if (result) {
				freeAdmaDescriptorArea(command);
				goto unlock;
			}
		}
		command->callback = NULL;
	}
	return sceSdifSendSyncCmd(context, command, secondary, retries, checkCard);
unlock:
	if (!sdifIsPolling(port))
		sceKernelUnlockFastMutex(&port->mutex);
	return result;
}

int sdifSendCheckedSyncCommand(SdifContextGlobal *context, SdifCommand *command,
			   SdifCommand *secondary, int retries)
{
	return sdifPrepareSyncCommand(context, command, secondary, retries, 1);
}

int sdifSendCheckedApplicationCommand(SdifContextGlobal *context, SdifCommand *secondary,
				      int retries)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->flags = 0x13;
	command->command = 55;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->argument = (uint32_t)context->port.relativeCardAddress << 16;
	return sdifPrepareSyncCommand(context, command, secondary, retries, 1);
}

int sdifSendApplicationCommand(SdifContextGlobal *context, SdifCommand *secondary,
				      int retries)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->flags = 0x13;
	command->command = 55;
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->argument = (uint32_t)context->port.relativeCardAddress << 16;
	return sdifPrepareSyncCommand(context, command, secondary, retries, 0);
}

int sdifResetCard(SdifContextGlobal *context)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	SdifPortState *port = &context->port;
	volatile SdifHostRegisters *registers;
	int result, intrState, retries;
	volatile int delay;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 1;
	command->command = 0;
	command->argument = 0;
	result = sdifPrepareSyncCommand(context, command, NULL, 0, 0);
	if (result < 0)
		return result;
	intrState = sdifSpinLock(&port->lock);
	registers = port->registers;
	registers->clockControl = 0;
	registers->clockControl = 0x4001;
	retries = 41;
	while (!(registers->clockControl & 2)) {
		if (!--retries)
			goto unlock;
		for (delay = 25000; delay; --delay)
			;
	}
	if (port->index == 2)
		registers->clockControl |= 4;
	registers->hostControl &= ~4U;
unlock:
	sdifSpinUnlock(&port->lock, intrState);
	intrState = sdifSpinLock(&port->lock);
	registers = port->registers;
	port->busWidth = 1;
	registers->hostControl &= 0xdd;
	sdifSpinUnlock(&port->lock, intrState);
	return 0;
}

int sdifReadCid(SdifContextGlobal *context, uint32_t cid[4])
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	int result;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->buffer = NULL;
	command->size = sizeof(*command);
	command->flags = 0x32;
	command->command = 2;
	command->argument = 0;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0)
		return result;
	__builtin_memcpy(cid, command->response, sizeof(command->response));
	return 0;
}

int sdifReadCsd(SdifContextGlobal *context, uint32_t csd[4])
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	SdifPortState *port = &context->port;
	int result;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->flags = 0x33;
	command->command = 9;
	command->size = sizeof(*command);
	command->argument = (uint32_t)port->relativeCardAddress << 16;
	command->buffer = NULL;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0)
		return result;
	__builtin_memcpy(csd, command->response, sizeof(command->response));
	return 0;
}

int sdifSelectCard(SdifContextGlobal *context, int select)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	SdifPortState *port = &context->port;
	uint32_t flags;
	int result;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->size = sizeof(*command);
	flags = 3;
	if (select) {
		flags = 0x13;
		select = (uint32_t)port->relativeCardAddress << 16;
	}
	command->flags = flags;
	command->command = 7;
	command->argument = select;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	return result < 0 ? result : 0;
}

#if !SDIF_NSKBL
int sdifDeselectCard(SdifContextGlobal *context)
{
	SdifPortState *port = &context->port;
	uint16_t relativeCardAddress = port->relativeCardAddress;
	int result;

	port->relativeCardAddress = relativeCardAddress + 1;
	result = sdifSelectCard(context, 0);
	port->relativeCardAddress = relativeCardAddress;
	return result;
}
#endif

int sdifGetCardStatusError(uint32_t status)
{
	uint32_t errors = status & 0xfdffe008;

	if (!errors)
		return errors;
	return 0x80320100U + __builtin_ctz(errors);
}

int SceSdifForDriver_53518827(SdifContextGlobal *context, uint32_t *status)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	SdifPortState *port = &context->port;
	int result;
	uint32_t response, errors;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->flags = 0x13;
	command->command = 13;
	command->size = sizeof(*command);
	command->argument = (uint32_t)port->relativeCardAddress << 16;
	command->buffer = NULL;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0)
		return result;
	response = command->response[0];
	if (status)
		*status = response;
	errors = response & 0xfdffe008;
	if (errors) {
		result = 0x80320100U + __builtin_ctz(errors);
		if (result < 0)
			return result;
	}
	return 0;
}

int sdifSetBlockLength(SdifContextGlobal *context, uint32_t blockLength)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	int result;
	uint32_t response, errors;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	command->argument = blockLength;
	command->size = sizeof(*command);
	command->flags = 0x13;
	command->command = 16;
	command->buffer = NULL;
	result = sdifSendCheckedSyncCommand(context, command, NULL, 3);
	if (result < 0)
		return result;
	response = command->response[0];
	errors = response & 0xfdffe008;
	if (errors) {
		result = 0x80320100U + __builtin_ctz(errors);
		if (result < 0)
			return result;
	}
	return 0;
}

#if !SDIF_NSKBL
int SceSdifForDriver_3DF7E207(unsigned int index, SdifHostInfo *info)
{
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	uint8_t hostControl;
	uint16_t hostVersion;

	if (index > 2)
		return 0x80320001;
	port = &gSdifContexts[index].port;
	registers = port->registers;
	info->registers = registers;
	info->voltages = port->voltages;
	info->baseClock = 48000000;
	hostControl = registers->hostControl;
	if (hostControl & 0x20)
		info->busWidth = 8;
	else
		info->busWidth = hostControl & 2 ? 4 : 1;
	info->clockFrequency = 48000000U >> (registers->clockControl >> 8);
	info->timeoutControl = registers->timeoutControl;
	hostVersion = port->registers->hostVersion;
	info->specificationVersion = hostVersion + 1;
	info->vendorVersion = hostVersion >> 8;
	info->lastTransferTime = port->lastTransferTime;
	return 0;
}

int SceSdifForDriver_76D2B87B(unsigned int index, SdifDeviceInfo *info)
{
	SdifPortState *port;
	uint32_t operationConditions, deviceType;

	if (index > 2)
		return 0x80320001;
	port = &gSdifContexts[index].port;
	if (!port->part)
		return 0x8032001a;
	operationConditions = port->operationConditions;
	deviceType = port->deviceType;
	info->relativeCardAddress = port->relativeCardAddress;
	info->deviceType = deviceType;
	info->operationConditions = operationConditions;
	return 0;
}

int SceSdifForDriver_6CC8E28D(unsigned int index)
{
	SdifCommand *command = __builtin_alloca(sizeof(*command) + 64 - __alignof__(uint64_t));
	SdifContextGlobal *context;
	SdifPortState *port;
	volatile SdifHostRegisters *registers;
	uint8_t powerControl;
	uint32_t flags;
	int result, intrState;

	command = (SdifCommand *)(((uintptr_t)command + 63) & ~63U);
	if (index > 2)
		return 0x80320001;
	context = &gSdifContexts[index];
	if (index != 1)
		return 0x80320001;
	port = &context->port;
	registers = port->registers;
	if (!sdifIsPolling(port))
		lockFastMutex(&port->mutex);
	sceKernelDelayThread(25000);
	if (!(registers->presentState & 0x10000))
		goto unlock;
	powerControl = registers->powerControl;
	command->size = sizeof(*command);
	command->flags = 0x13;
	command->command = 13;
	registers->powerControl = port->powerControl;
	command->buffer = NULL;
	registers->powerControl |= 1;
	command->argument = (uint32_t)port->relativeCardAddress << 16;
	if (!sdifIsPolling(port)) {
		result = lockFastMutex(&port->mutex);
		if (result < 0)
			goto restore;
	}
	flags = command->flags;
	command->secondaryCommand = NULL;
	command->context = context;
	command->callback = sdifCompleteSyncCommand;
	command->eventFlag = port->eventFlag;
	if (flags & 0x400) {
		command->flags = flags & ~0x100000U;
		command->leadingLength = 0;
		command->trailingLength = 0;
		result = setAdmaDescriptorsForAlignedData(command);
		if (result) {
			if (!sdifIsPolling(port))
				sceKernelUnlockFastMutex(&port->mutex);
			goto restore;
		}
	}
	result = sceSdifSendSyncCmd(context, command, NULL, 0, 0);
	if (!result && (command->response[0] & 0x1e00) == 0x800) {
		intrState = sdifSpinLock(&port->lock);
		if (!port->part) {
			port->deviceType = port->removedDeviceType;
			port->part = port->removedPart;
			port->removedDeviceType = 0;
			port->removedPart = NULL;
		}
		sdifSpinUnlock(&port->lock, intrState);
		if (!sdifIsPolling(port))
			sceKernelUnlockFastMutex(&port->mutex);
		return result;
	}
restore:
	registers->powerControl = powerControl;
unlock:
	if (!sdifIsPolling(port))
		sceKernelUnlockFastMutex(&port->mutex);
	return 0x80320020;
}

int SceSdifForDriver_B83F7518(unsigned int index, SdifCommandInfo *info)
{
	SdifContextGlobal *context;
	SdifPortState *port;
	SdifCommand *command;
	uint32_t flags;
	uint32_t response0, response1, response2, response3;
	int result, intrState;

	if (index > 2)
		return 0x80320001;
	context = &gSdifContexts[index];
	port = &context->port;
	intrState = sdifSpinLock(&port->lock);
	command = sdifCommandQueuePop(&port->freeCommands);
	sdifSpinUnlock(&port->lock, intrState);
	if (!command)
		return 0x80320011;
	command->flags = info->flags;
	command->command = info->command;
	command->argument = info->argument;
	command->blockSize = info->blockSize;
	command->buffer = info->buffer;
	command->blockCount = info->blockCount;
	command->endTime = 0;
	command->size = sizeof(*command);
	if (!sdifIsPolling(port)) {
		result = lockFastMutex(&port->mutex);
		if (result < 0)
			goto release;
	}
	flags = command->flags;
	command->context = context;
	command->callback = sdifCompleteSyncCommand;
	command->eventFlag = port->eventFlag;
	command->secondaryCommand = NULL;
	if (flags & 0x400) {
		command->leadingLength = 0;
		command->flags = flags & ~0x100000U;
		command->trailingLength = 0;
		result = setAdmaDescriptorsForAlignedData(command);
		if (result) {
			if (!sdifIsPolling(port))
				sceKernelUnlockFastMutex(&port->mutex);
			goto complete;
		}
	}
	result = sceSdifSendSyncCmd(context, command, NULL, 0, 1);
complete:
	if (result >= 0) {
		response0 = command->response[0];
		response1 = command->response[1];
		response2 = command->response[2];
		response3 = command->response[3];
		info->response[0] = response0;
		info->response[1] = response1;
		info->response[2] = response2;
		info->response[3] = response3;
		info->errorCode = command->errorCode;
	}
release:
	port->lastTransferTime = command->endTime - command->startTime;
	intrState = sdifSpinLock(&port->lock);
	command->state = 0;
	sdifCommandQueuePush(&port->freeCommands, command);
	sdifSpinUnlock(&port->lock, intrState);
	return result;
}

int SceSdifForDriver_475D8E45(unsigned int index)
{
	if (index > 2)
		return 0x80320001;
	return sdifResetCard(&gSdifContexts[index]);
}

int sceSdifEnableSlowCardMode(void)
{
	volatile uint8_t ports[] = { 1, 3 };
	unsigned int index;

	for (index = 0; index < sizeof(ports); ++index) {
		unsigned int port = ports[index];

		if (port <= 2)
			gSdifContexts[port].port.slowMode = 1;
	}
	return 0;
}

int sceSdifDisableSlowCardMode(void)
{
	volatile uint8_t ports[] = { 1, 3 };
	unsigned int index;

	for (index = 0; index < sizeof(ports); ++index) {
		unsigned int port = ports[index];

		if (port <= 2)
			gSdifContexts[port].port.slowMode = 0;
	}
	return 0;
}

SdifCommand *sdifAllocateCommand(SdifContextGlobal *context)
{
	return sdifCommandQueuePop(&context->port.freeCommands);
}
#endif

SdifCommand *sdifAllocateCommandLocked(SdifContextGlobal *context)
{
	SceKernelSpinlock *lock = &context->port.lock;
	SdifCommand *command;
	int intrState;

	intrState = sdifSpinLock(lock);
	command = sdifCommandQueuePop(&context->port.freeCommands);
	sdifSpinUnlock(lock, intrState);
	return command;
}

#if !SDIF_NSKBL
void sdifFreeCommand(SdifContextGlobal *context, SdifCommand *command)
{
	command->state = 0;
	sdifCommandQueuePush(&context->port.freeCommands, command);
}
#endif

void sdifFreeCommandLocked(SdifContextGlobal *context, SdifCommand *command)
{
	SceKernelSpinlock *lock = &context->port.lock;
	int intrState;

	intrState = sdifSpinLock(lock);
	command->state = 0;
	sdifCommandQueuePush(&context->port.freeCommands, command);
	sdifSpinUnlock(lock, intrState);
}

void sdifDelay(SdifContextGlobal *context, int microseconds)
{
	volatile int delay;

	if (!sdifIsPolling(&context->port)) {
		sceKernelDelayThread(microseconds);
		return;
	}
	for (delay = 100 * microseconds; delay; --delay)
		;
}
