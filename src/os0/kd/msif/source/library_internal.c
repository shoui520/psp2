/* Standalone library helpers; source-file names absent from debug records. */
#include "library.h"
#include "msif_context.h"
#include <psp2kern/kernel/threadmgr.h>
#include <sce/threadmgr.h>

void *MsproalInitializeErrorLog(struct MsproalHandle *handle,
	unsigned int capacity, uint32_t *index,
	struct MsproalErrorRecord *records)
{
	void *result;

	result = MsproalUserFill(records, 0, capacity * sizeof(*records));
	*index = 0;
	return result;
}

int MsproalInitializeHandle(struct MsproalHandle *handle,
	struct MsifRegisterWindow *registers)
{
	handle->registers = registers;
	handle->writeEnabled = 1;
	handle->writeProtected = 0;
	handle->powerClass = 0;
	handle->mountFlags = 0;
	handle->interruptFlags = 0;
	handle->busMode = 1;
	handle->requestedBusMode = 1;
	msproal_user_clear_flg(0);
	MsproalUserSetBusClock(1);
	return MsproalResetController(handle);
}

int MsproalIdentifyMedia(struct MsproalHandle *handle, uint32_t *mediaType,
	int *writeEnabled, int *writeProtected)
{
	union {
		struct MsproalMediaRegisters fields;
		uint8_t bytes[sizeof(struct MsproalMediaRegisters)];
	} media;
	const struct MsproalMediaRegisters *registers = &media.fields;
	uint32_t type = 0;
	uint32_t flags;
	unsigned int mediaClass;
	unsigned int category;
	unsigned int subtype;
	int result;

	result = MsproalReadMediaRegisters(handle, 0, sizeof(media), media.bytes);
	if (result)
		return result;
	mediaClass = registers->mediaClass;
	category = registers->category;
	if (registers->status0 & 1) {
		*writeProtected = 1;
		*writeEnabled = 0;
	}
	subtype = registers->subtype;
	if ((uint8_t)(registers->type + 1) <= 1) {
		if (registers->type != mediaClass) {
			if (mediaClass - 1 <= 126)
				type = 0x40;
		} else if (registers->type == subtype || subtype - 1 <= 2) {
			flags = 0x2010;
			type = 0x2011;
			if (registers->type) {
				flags = 0x10;
				type = 0x11;
			}
			if (subtype != 255)
				type = (1U << subtype) | flags;
		}
	} else if (registers->type == 1) {
		if (mediaClass == 0) {
			if (subtype <= 3) {
				if (category == 0) {
					type = (1U << subtype) | 0x2020;
				} else if (category == 7) {
					flags = 0xa820;
					if (!(handle->registers->interruptStatus & 0x400))
						flags = 0xe820;
					type = (1U << subtype) | flags;
				}
			}
		} else if (mediaClass == 0x10) {
			if (category == 0)
				type = 0x2080;
			if (category == 7) {
				type = 0xa880;
				if (!(handle->registers->interruptStatus & 0x400))
					type = 0xe880;
			}
		}
	}
	*mediaType = type;
	return 0;
}

int MsproalRecordError(struct MsproalHandle *handle, int operation,
	int result, uint32_t *index, struct MsproalErrorRecord *record)
{
	struct MsifControllerRegisters *controller;
	struct MsifRegisterWindow *registers;
	uint32_t sequence = *index;
	int busMode;

	record->operation = operation;
	record->result = result;
	record->sequence = sequence;
	controller = MSPROAL_CONTROLLER(handle);
	record->registerSnapshot[0] = controller->sequencerSnapshot[0];
	record->registerSnapshot[1] = controller->sequencerSnapshot[1];
	record->registerSnapshot[2] = controller->sequencerSnapshot[2];
	record->registerSnapshot[3] = controller->sequencerSnapshot[3];
	record->registerSnapshot[4] = controller->sequencerSnapshot[4];
	record->registerSnapshot[5] = controller->sequencerSnapshot[5];
	record->registerSnapshot[6] = controller->sequencerSnapshot[6];
	record->registerSnapshot[7] = controller->sequencerSnapshot[7];
	record->registerSnapshot[8] =
		MSPROAL_CONTROLLER(handle)->sequencer.finalDiagnostic;
	record->registerSnapshot[9] = handle->registers->commandSnapshot;
	registers = handle->registers;
	record->registerSnapshot[10] = registers->interruptSnapshot;
	record->registerSnapshot[11] = registers->controlSnapshot;
	record->registerSnapshot[12] = registers->diagnostic[0];
	record->registerSnapshot[13] = registers->diagnostic[1];
	registers = handle->registers;
	record->registerSnapshot[14] = registers->dma[0].dataAddress;
	record->registerSnapshot[15] = registers->dma[0].nextDescriptorAddress;
	record->registerSnapshot[16] = registers->dma[0].transferControl;
	record->registerSnapshot[17] = registers->dma[0].diagnostic;
	record->registerSnapshot[18] = registers->dma[1].dataAddress;
	record->registerSnapshot[19] = registers->dma[1].nextDescriptorAddress;
	record->registerSnapshot[20] = registers->dma[1].transferControl;
	record->registerSnapshot[21] = registers->dma[1].diagnostic;
	record->registerSnapshot[22] = registers->powerSupply;
	*index = sequence + 1;
	result = MsproalResetController(handle);
	if (result)
		return result;
	busMode = handle->busMode;
	if (busMode == 2)
		busMode = handle->mediaType & 0x120 ? 4 : 3;
	return MsproalSetHostBusMode(handle, busMode);
}

struct MsproalHandle *MsproalClearDmaChannels(struct MsproalHandle *handle)
{
	handle->registers->dma[0].transferControl = 0;
	handle->registers->dma[1].transferControl = 0;
	return handle;
}

int MsproalExecuteSequencer(struct MsproalHandle *handle, int command)
{
	struct MsifControllerRegisters *controller;
	int64_t commandStartTime;
	uint32_t flags;
	uint16_t status;

	MSPROAL_CONTROLLER(handle)->sequencer.command = command;
	handle->interruptFlags = 0;
	if (!MsproalUserPollEventFlag(0x80000000)) {
		msproal_user_clear_flg(0x7fffffff);
		return -12;
	}
	msproal_user_clear_flg(0x9fffffff);
	commandStartTime = sceKernelGetSystemTimeWide();
	controller = MSPROAL_CONTROLLER(handle);
	gMsifContext.commandStartTime = commandStartTime;
	controller->sequencer.control |= 1;
	if (MsproalUserWaitEventFlag(0xa0000000, 20000))
		return -4;
	flags = handle->interruptFlags;
	msproal_user_clear_flg(0x5fffffff);
	if ((int32_t)flags < 0) {
		if (flags & 0x20000000)
			return -12;
		if (MsproalUserWaitEventFlag(0x20000000, 20000))
			return -4;
		msproal_user_clear_flg(0xdfffffff);
		return -12;
	}
	controller = MSPROAL_CONTROLLER(handle);
	status = controller->sequencer.result;
	if (status & 0x40)
		return -(status & 15);
	if (status & 0x20) {
		if ((uint8_t)controller->sequencer.command == 2)
			return -controller->sequencer.commandError;
		return handle->registers->interruptStatus & 0x1000 ? -3 : -4;
	}
	return status & 0x80 ? -3 : -4;
}

int MsproalLoadSequencerProgram(struct MsproalHandle *handle,
	int address, int count, const uint16_t *program)
{
	struct MsifControllerRegisters *controller;
	volatile uint16_t *fifo;
	const uint16_t *end;
	int remaining;

	if ((count <= 0) | (address < 0))
		return -1;
	if ((int)((uint32_t)address + count) > 256)
		return -1;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.command = address;
	fifo = &controller->sequencer.instructionFifo;
	remaining = count - 1;
	if (count > 16) {
		const uint16_t *next = program + 16;

		do {
			uint16_t first = next[-16];

			remaining -= 16;
			__builtin_prefetch(next + 18);
			program = next;
			next += 16;
			*fifo = first;
			*fifo = program[-15];
			*fifo = program[-14];
			*fifo = program[-13];
			*fifo = program[-12];
			*fifo = program[-11];
			*fifo = program[-10];
			*fifo = program[-9];
			*fifo = program[-8];
			*fifo = program[-7];
			*fifo = program[-6];
			*fifo = program[-5];
			*fifo = program[-4];
			*fifo = program[-3];
			*fifo = program[-2];
			*fifo = program[-1];
		} while (remaining > 15);
	}
	end = program + remaining + 1;
	do {
		*fifo = *program++;
	} while (program != end);
	return 0;
}

int MsproalResetSequencer(struct MsproalHandle *handle)
{
	struct MsifControllerRegisters *controller = MSPROAL_CONTROLLER(handle);
	uint32_t powerSupply = handle->registers->powerSupply;

	controller->sequencer.reset |= 1;
	MsproalTimerStart(1);
	while (MSPROAL_CONTROLLER(handle)->sequencer.reset & 1) {
		if (MsproalTimerCheck())
			return -4;
	}
	MsproalTimerStop();
	handle->registers->powerSupply = powerSupply;
	MSPROAL_CONTROLLER(handle)->sequencer.reset |= 4;
	return 0;
}

int MsproalSetDmaChannel(struct MsproalHandle *handle,
	const struct MsifDmaDescriptor *descriptor, int channel)
{
	if (channel == 1) {
		handle->registers->dma[0].dataAddress = descriptor->dataAddress;
		handle->registers->dma[0].nextDescriptorAddress =
			descriptor->nextDescriptorAddress;
		handle->registers->dma[0].transferControl = descriptor->transferControl;
		return 0;
	}
	if (channel == 2) {
		handle->registers->dma[1].dataAddress = descriptor->dataAddress;
		handle->registers->dma[1].nextDescriptorAddress =
			descriptor->nextDescriptorAddress;
		handle->registers->dma[1].transferControl = descriptor->transferControl;
		return 0;
	}
	return -1;
}

int MsproalUpdatePowerSupply(struct MsproalHandle *handle,
	uint32_t value, uint32_t preserveMask)
{
	struct MsifRegisterWindow *registers = handle->registers;

	value &= ~preserveMask;
	registers->powerSupply = (registers->powerSupply & preserveMask) | value;
	return 0;
}

int MsproalSwitchBusMode(struct MsproalHandle *handle, int busMode)
{
	uint8_t command[1];
	int mode;
	int result;

	switch (busMode) {
	case 3:
		mode = 2;
		command[0] = 0x88;
		break;
	case 4:
		mode = 2;
		command[0] = 0;
		break;
	case 1:
		mode = 1;
		command[0] = 0x80;
		break;
	default:
		mode = busMode;
		command[0] = 0x40;
		break;
	}
	result = MsproalSetRegisterWindow(handle, 2, 1, 16, 1);
	if (result)
		return result;
	result = MsproalWriteRegisters(handle, 1, command);
	if (result)
		return result;
	result = MsproalSetHostBusMode(handle, busMode);
	if (result)
		return result;
	handle->busMode = mode;
	return 0;
}

int MsproalSetModeAndWait(struct MsproalHandle *handle, int mode)
{
	int result;

	result = MsproalUserSetMode(mode);
	if (!result)
		MsproalUserWaitMilliseconds(10);
	return result;
}

int MsproalExecuteExtendedCommand(struct MsproalHandle *handle,
	int command, uint32_t sector, int count, int timeout)
{
	uint8_t interrupt;
	int result;

	result = MsproalSendExtendedCommand(handle, command, sector, count);
	if (result)
		return result;
	result = MsproalWaitTransfer(handle, timeout, 0);
	if (result)
		return result == -4 ? -3 : result;
	if (handle->busMode == 1) {
		result = MsproalGetInterrupt(handle, &interrupt);
		if (result)
			return result;
	} else {
		interrupt = handle->interruptFlags;
	}
	if (interrupt & 0x41)
		return interrupt & 1 ? -105 : -106;
	return interrupt & 0xa0 ? 0 : -4;
}

int MsproalWaitCommand(struct MsproalHandle *handle, int timeout,
	uint8_t *interrupt)
{
	uint8_t status;
	int result;

	result = MsproalWaitTransfer(handle, timeout, 0);
	if (result)
		return result == -4 ? -3 : result;
	if (handle->busMode == 1) {
		result = MsproalGetInterrupt(handle, &status);
		if (result)
			return result;
	} else {
		status = handle->interruptFlags;
	}
	*interrupt = status;
	if (status & 0x41)
		return status & 1 ? -105 : -106;
	return status & 0xa0 ? 0 : -4;
}
