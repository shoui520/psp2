/* PRO operations; source-file names absent from debug records. */
#include "library.h"
#include "msif_auth.h"
#include <arm_neon.h>

/*
 * Memory Stick PRO sequencer microcode programs (loaded into the controller's
 * 256-halfword instruction FIFO by MsproalLoadSequencerProgram).
 * Attribute and sector reads use identical programs in separate tables.
 */
#define MSPROAL_PRO_READ_SERIAL_PROGRAM \
	MSIF_SEQ_BRA(2), \
	MSIF_SEQ_WAIT(0x01), \
	MSIF_SEQ_TPC_CMD_FIFO(MSIF_TPC_EX_SET_CMD | MSIF_TPC_SEQ_AUTO | 7), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_SERIAL_INT, \
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK)), \
	MSIF_SEQ_BNE(22), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED, \
		MSPRO_SERIAL_INT(MSPRO_INT_CED | MSPRO_INT_BREQ)), \
	MSIF_SEQ_BEQ(48), \
	/* sector read loop (pc = 15) */ \
	MSIF_SEQ_TPC_DMA_READ(MSIF_TPC_READ_LONG_DATA | MSIF_TPC_SEQ_AUTO | 512), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_SERIAL_INT, \
		MSPRO_SERIAL_INT(MSPRO_INT_ERR)), \
	MSIF_SEQ_BNE(16), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED, \
		MSPRO_SERIAL_INT(MSPRO_INT_CED | MSPRO_INT_BREQ)), \
	MSIF_SEQ_BEQ(35), \
	MSIF_SEQ_DEC_CMP(MSIF_SEQ_REG_SECTOR_COUNT, 0), \
	MSIF_SEQ_BNE(-15), \
	MSIF_SEQ_HALT(0), \
	/* initial command error (pc = 32) */ \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED, \
		MSPRO_SERIAL_INT(MSPRO_INT_CMDNK)), \
	MSIF_SEQ_BEQ(4), \
	MSIF_SEQ_SET_ERROR(1), \
	MSIF_SEQ_BRA(-37), \
	/* stop transfer on sector read error (pc = 39) */ \
	MSIF_SEQ_TPC_INLINE_CMD(MSIF_TPC_EX_SET_CMD | 7, MSPRO_CMD_STOP), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1), \
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_SERIAL_INT, \
		MSPRO_SERIAL_INT(MSPRO_INT_CED), \
		MSPRO_SERIAL_INT(MSPRO_INT_CED | MSPRO_INT_CMDNK)), \
	MSIF_SEQ_BNE(4), \
	MSIF_SEQ_SET_ERROR(11), \
	MSIF_SEQ_BRA(-53), \
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED, \
		0, MSPRO_SERIAL_INT(MSPRO_INT_CMDNK)), \
	MSIF_SEQ_BNE(4), \
	MSIF_SEQ_SET_ERROR(4), \
	MSIF_SEQ_BRA(-60), \
	MSIF_SEQ_SET_ERROR(3), \
	MSIF_SEQ_BRA(-63)

#define MSPROAL_PRO_READ_PARALLEL_PROGRAM \
	MSIF_SEQ_BRA(2), \
	MSIF_SEQ_WAIT(0x01), \
	MSIF_SEQ_TPC_CMD_FIFO(MSIF_TPC_EX_SET_CMD | MSIF_TPC_SEQ_AUTO | 7), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_PARALLEL_INT, \
		MSPRO_INT_ERR | MSPRO_INT_CMDNK), \
	MSIF_SEQ_BNE(20), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED, \
		MSPRO_INT_CED | MSPRO_INT_BREQ), \
	MSIF_SEQ_BEQ(44), \
	/* sector read loop (pc = 13) */ \
	MSIF_SEQ_TPC_DMA_READ(MSIF_TPC_READ_LONG_DATA | MSIF_TPC_SEQ_AUTO | 512), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_PARALLEL_INT, MSPRO_INT_ERR), \
	MSIF_SEQ_BNE(16), \
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED, \
		MSPRO_INT_CED | MSPRO_INT_BREQ), \
	MSIF_SEQ_BEQ(33), \
	MSIF_SEQ_DEC_CMP(MSIF_SEQ_REG_SECTOR_COUNT, 0), \
	MSIF_SEQ_BNE(-13), \
	MSIF_SEQ_HALT(0), \
	/* initial command error (pc = 28) */ \
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED, \
		0, MSPRO_INT_CMDNK), \
	MSIF_SEQ_BEQ(4), \
	MSIF_SEQ_SET_ERROR(1), \
	MSIF_SEQ_BRA(-33), \
	/* stop transfer on sector read error (pc = 35) */ \
	MSIF_SEQ_TPC_INLINE_CMD(MSIF_TPC_EX_SET_CMD | 7, MSPRO_CMD_STOP), \
	MSIF_SEQ_WAIT(0x08), \
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_PARALLEL_INT, \
		MSPRO_INT_CED, MSPRO_INT_CED | MSPRO_INT_CMDNK), \
	MSIF_SEQ_BNE(4), \
	MSIF_SEQ_SET_ERROR(11), \
	MSIF_SEQ_BRA(-47), \
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED, \
		0, MSPRO_INT_CMDNK), \
	MSIF_SEQ_BNE(4), \
	MSIF_SEQ_SET_ERROR(4), \
	MSIF_SEQ_BRA(-54), \
	MSIF_SEQ_SET_ERROR(3), \
	MSIF_SEQ_BRA(-57)

static const uint16_t gMsproalProWriteParallelProgram[52] = {
	MSIF_SEQ_BRA(2),
	MSIF_SEQ_WAIT(0x01),
	MSIF_SEQ_TPC_CMD_FIFO(MSIF_TPC_EX_SET_CMD | MSIF_TPC_SEQ_AUTO | 7),
	MSIF_SEQ_WAIT(0x0b),
	/* sector write loop (pc = 5) */
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_PARALLEL_INT,
		MSPRO_INT_ERR | MSPRO_INT_CMDNK),
	MSIF_SEQ_BNE(20),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED,
		MSPRO_INT_CED | MSPRO_INT_BREQ),
	MSIF_SEQ_BEQ(37),
	MSIF_SEQ_TPC_DMA_WRITE(MSIF_TPC_WRITE_LONG_DATA | MSIF_TPC_SEQ_AUTO | 512),
	MSIF_SEQ_WAIT(0x0b),
	MSIF_SEQ_DEC_CMP(MSIF_SEQ_REG_SECTOR_COUNT, 0),
	MSIF_SEQ_BNE(-13),
	/* post-loop completion check (pc = 19) */
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_PARALLEL_INT,
		MSPRO_INT_ERR | MSPRO_INT_CMDNK),
	MSIF_SEQ_BNE(6),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED,
		MSPRO_INT_CED | MSPRO_INT_BREQ),
	MSIF_SEQ_BEQ(23),
	MSIF_SEQ_HALT(0),
	/* write error handler (pc = 28) */
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		MSPRO_INT_ERR, MSPRO_INT_ERR | MSPRO_INT_CMDNK),
	MSIF_SEQ_BNE(4),
	MSIF_SEQ_SET_ERROR(3),
	MSIF_SEQ_BRA(-33),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		MSPRO_INT_ERR | MSPRO_INT_CMDNK, MSPRO_INT_ERR | MSPRO_INT_CMDNK),
	MSIF_SEQ_BNE(4),
	MSIF_SEQ_SET_ERROR(10),
	MSIF_SEQ_BRA(-40),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		0, MSPRO_INT_CMDNK),
	MSIF_SEQ_BEQ(4),
	MSIF_SEQ_SET_ERROR(1),
	MSIF_SEQ_BRA(-47),
	MSIF_SEQ_SET_ERROR(4),
	MSIF_SEQ_BRA(-50),
};

static const uint16_t gMsproalProAttributeSerialProgram[65] = {
	MSPROAL_PRO_READ_SERIAL_PROGRAM,
};

static const uint16_t gMsproalProReadSerialProgram[65] = {
	MSPROAL_PRO_READ_SERIAL_PROGRAM,
};

static const uint16_t gMsproalProReadParallelProgram[59] = {
	MSPROAL_PRO_READ_PARALLEL_PROGRAM,
};

static const uint16_t gMsproalProInitializeProgram[36] = {
	MSIF_SEQ_BRA(2),
	MSIF_SEQ_ABORT_VECTOR,
	MSIF_SEQ_SET_ERROR(0),
	MSIF_SEQ_LOAD_COUNT(1000),
	/* poll INT.CED up to 1000 times (pc = 6) */
	MSIF_SEQ_DELAY(4),
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_INIT, MSIF_SEQ_REG_SERIAL_INT,
		MSPRO_SERIAL_INT(MSPRO_INT_CED), MSPRO_SERIAL_INT(MSPRO_INT_CED)),
	MSIF_SEQ_BEQ(5),
	MSIF_SEQ_DEC_CMP(MSIF_SEQ_REG_SECTOR_COUNT, 0),
	MSIF_SEQ_BEQ(20),
	MSIF_SEQ_BRA(-10),
	/* check post-init write-protect status (pc = 17) */
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_INIT, MSIF_SEQ_REG_SERIAL_INT,
		MSPRO_SERIAL_INT(MSPRO_INT_ERR), MSPRO_SERIAL_INT(MSPRO_INT_ERR)),
	MSIF_SEQ_BEQ(5),
	MSIF_SEQ_WRITE_REG(MSIF_SEQ_REG_MEDIA_STATUS, 0x0100),
	MSIF_SEQ_HALT(0),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_INIT, MSIF_SEQ_REG_LATCHED,
		MSPRO_SERIAL_INT(MSPRO_INT_CMDNK), MSPRO_SERIAL_INT(MSPRO_INT_CMDNK)),
	MSIF_SEQ_BNE(5),
	MSIF_SEQ_WRITE_REG(MSIF_SEQ_REG_MEDIA_STATUS, 0x0000),
	MSIF_SEQ_BRA(-8),
	MSIF_SEQ_HALT(5),
};

static const uint16_t gMsproalProWriteSerialProgram[56] = {
	MSIF_SEQ_BRA(2),
	MSIF_SEQ_WAIT(0x01),
	MSIF_SEQ_TPC_CMD_FIFO(MSIF_TPC_EX_SET_CMD | MSIF_TPC_SEQ_AUTO | 7),
	MSIF_SEQ_WAIT(0x0b),
	/* sector write loop (pc = 5) */
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_SERIAL_INT,
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK)),
	MSIF_SEQ_BNE(22),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED,
		MSPRO_SERIAL_INT(MSPRO_INT_CED | MSPRO_INT_BREQ)),
	MSIF_SEQ_BEQ(39),
	MSIF_SEQ_TPC_DMA_WRITE(MSIF_TPC_WRITE_LONG_DATA | MSIF_TPC_SEQ_AUTO | 512),
	MSIF_SEQ_WAIT(0x0b),
	MSIF_SEQ_DEC_CMP(MSIF_SEQ_REG_SECTOR_COUNT, 0),
	MSIF_SEQ_BNE(-15),
	/* post-loop completion check (pc = 21) */
	MSIF_SEQ_TPC_GET_INT(MSIF_TPC_GET_INT | 1),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_SERIAL_INT,
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK)),
	MSIF_SEQ_BNE(6),
	MSIF_SEQ_TEST_CLEAR(MSIF_SEQ_REG_LATCHED,
		MSPRO_SERIAL_INT(MSPRO_INT_CED | MSPRO_INT_BREQ)),
	MSIF_SEQ_BEQ(23),
	MSIF_SEQ_HALT(0),
	/* write error handler (pc = 32) */
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		MSPRO_SERIAL_INT(MSPRO_INT_ERR),
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK)),
	MSIF_SEQ_BNE(4),
	MSIF_SEQ_SET_ERROR(3),
	MSIF_SEQ_BRA(-37),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK),
		MSPRO_SERIAL_INT(MSPRO_INT_ERR | MSPRO_INT_CMDNK)),
	MSIF_SEQ_BNE(4),
	MSIF_SEQ_SET_ERROR(10),
	MSIF_SEQ_BRA(-44),
	MSIF_SEQ_TEST_MATCH(MSIF_SEQ_OP_TEST_STATUS, MSIF_SEQ_REG_LATCHED,
		0, MSPRO_SERIAL_INT(MSPRO_INT_CMDNK)),
	MSIF_SEQ_BEQ(4),
	MSIF_SEQ_SET_ERROR(1),
	MSIF_SEQ_BRA(-51),
	MSIF_SEQ_SET_ERROR(4),
	MSIF_SEQ_BRA(-54),
};

static const uint16_t gMsproalProAttributeParallelProgram[59] = {
	MSPROAL_PRO_READ_PARALLEL_PROGRAM,
};

int MsproalReadPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer)
{
	struct MsifControllerRegisters *controller;
	const uint16_t *program;
	uint16_t sectorHigh;
	int programSize, result;

	if (!count)
		return 0;
	if (count < 0)
		return -1;
	if (handle->busMode == 1) {
		program = gMsproalProReadSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProReadParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		return result;
	sectorHigh = sector >> 16;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = count;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 32;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo =
		(uint16_t)count >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = count;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector;
	result = MsproalSetDmaChannel(handle, transfer, 2);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	return result;
}

int MsproalResetPro(struct MsproalHandle *handle)
{
	struct MsifControllerRegisters *controller;
	struct MsifRegisterWindow *registers;
	uint32_t mediaStatus;
	int result, busMode, powerClass;

	result = MsproalSetModeAndWait(handle, 0);
	if (result)
		return result;
	result = MsproalResetController(handle);
	if (result)
		return result;
	result = MsproalSetModeAndWait(handle, 1);
	if (result)
		return result;
	MsproalUserSetBusClock(1);
	registers = handle->registers;
	controller = (struct MsifControllerRegisters *)((uint8_t *)registers -
		offsetof(struct MsifControllerRegisters, transfer));
	registers->transferControl &= ~0x2000;
	controller->sequencer.control = 96;
	result = MsproalLoadSequencerProgram(handle, 0, 36,
		gMsproalProInitializeProgram);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	registers = handle->registers;
	registers->transferControl |= 0x2000;
	if (result)
		return result;
	controller = (struct MsifControllerRegisters *)((uint8_t *)registers -
		offsetof(struct MsifControllerRegisters, transfer));
	mediaStatus = controller->sequencer.mediaStatus;
	busMode = handle->busMode;
	if ((uint8_t)mediaStatus != 1)
		handle->writeEnabled = 0;
	switch (busMode) {
	case 2:
		result = MsproalSwitchBusMode(handle, 4);
		if (result)
			return result;
		break;
	case 5:
		result = MsproalSwitchBusMode(handle, 4);
		if (result)
			return result;
		result = MsproalSwitchBusMode(handle, 5);
		if (result)
			return result;
		break;
	case 1:
		break;
	default:
		return -4;
	}
	powerClass = handle->powerClass;
	if (!powerClass)
		return 0;
	result = MsproalExecuteExtendedCommand(handle, 22, 0,
		(uint16_t)powerClass, 1000);
	if (result == -105)
		return -1;
	if (!result)
		handle->powerClass = powerClass;
	return result;
}

int MsproalStopPro(struct MsproalHandle *handle)
{
	uint8_t interrupt;
	int result;

	result = MsproalGetInterrupt(handle, &interrupt);
	if (result)
		return result;
	if (!(interrupt & 0x20))
		return 0;
	result = MsproalExecuteCommand(handle, 37, 3000);
	if (!result || result == -106)
		return 0;
	return result == -105 ? -3 : result;
}

int MsproalStopAndCheckStatusPro(struct MsproalHandle *handle)
{
	uint8_t status[4];
	int result;

	result = MsproalExecuteCommand(handle, 37, 3500);
	if (result == -105)
		return 0;
	if (result)
		return result;
	result = MsproalReadMediaRegisters(handle, 2, 1, status);
	if (result)
		return result;
	return status[0] & 2 ? -3 : 0;
}

int MsproalSetPowerClassPro(struct MsproalHandle *handle, int powerClass)
{
	int result;

	result = MsproalExecuteExtendedCommand(handle, 22, 0,
		(uint16_t)powerClass, 1000);
	if (result) {
		if (result == -105)
			return -1;
	} else {
		handle->powerClass = powerClass;
	}
	return result;
}

int MsproalWritePro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer)
{
	struct MsifControllerRegisters *controller;
	const uint16_t *program;
	uint16_t sectorHigh;
	int programSize, result;

	if (!count)
		return 0;
	if (count < 0)
		return -1;
	if (handle->busMode == 1) {
		program = gMsproalProWriteSerialProgram;
		programSize = 56;
	} else {
		program = gMsproalProWriteParallelProgram;
		programSize = 52;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		return result;
	sectorHigh = sector >> 16;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = count;
	controller->sequencer.control = 144;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 33;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo =
		(uint16_t)count >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = count;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector;
	result = MsproalSetDmaChannel(handle, transfer, 2);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	if (result == -10)
		handle->writeEnabled = 0;
	MsproalClearDmaChannels(handle);
	return result;
}

int MsproalReadAttributesPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer)
{
	struct MsifControllerRegisters *controller;
	const uint16_t *program;
	uint16_t sectorHigh;
	int programSize, result;

	if (count <= 0)
		return -1;
	if (handle->busMode == 1) {
		program = gMsproalProAttributeSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProAttributeParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		return result;
	sectorHigh = sector >> 16;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = count;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 36;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo =
		(uint16_t)count >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = count;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector;
	result = MsproalSetDmaChannel(handle, transfer, 2);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	return result;
}

int MsproalReadSystemInformationPro(struct MsproalHandle *handle)
{
	struct MsproalAttributeDirectory *directory =
		(void *)handle->workBuffer;
	struct MsproalAttributeEntry *entry;
	struct MsproalSystemInformation *information;
	struct MsifDmaDescriptor transfer;
	struct MsifControllerRegisters *controller;
	const uint16_t *program;
	uint32_t address, size, mediaType, deviceType, accessType;
	int count, index, id, programSize, result;

	msproal_user_virt_to_bus(handle->workBuffer, &transfer.dataAddress);
	transfer.nextDescriptorAddress = 0;
	transfer.transferControl = 0x80070080;
	MsproalUserInvalidateCache(handle->workBuffer, 512);
	if (handle->busMode == 1) {
		program = gMsproalProAttributeSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProAttributeParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		goto transferError;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = 1;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 36;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 1;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	result = MsproalSetDmaChannel(handle, &transfer, 2);
	if (result)
		goto transferError;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	if (result)
		goto transferError;
	if (directory->signature[0] != 0xa5 || directory->signature[1] != 0xc3)
		return -5;
	count = directory->count;
	if ((unsigned int)(count - 1) > 11)
		return -5;
	MsproalUserCopy(&handle->attributeDirectory, directory,
		sizeof(handle->attributeDirectory));
	for (index = 0; index < count; index++) {
		entry = &directory->entries[index];
		id = entry->id;
		if (id == 0x10 || id == 0x13 || id == 0x14)
			goto foundInformation;
	}
	return -5;

foundInformation:
	size = entry->size[3] | ((uint32_t)entry->size[2] << 8) |
		((entry->size[1] | ((uint32_t)entry->size[0] << 8)) << 16);
	if (size != sizeof(*information))
		return -5;
	address = entry->address[3] | ((uint32_t)entry->address[2] << 8) |
		((entry->address[1] | ((uint32_t)entry->address[0] << 8)) << 16);
	if (address - 416 > 0x7e00)
		return -5;
	if (address != 416) {
		transfer.transferControl = 0x80070100;
		MsproalUserInvalidateCache(handle->workBuffer, 1024);
		result = MsproalReadAttributesPro(handle, address >> 9, 2, &transfer);
		if (result)
			goto transferError;
		address &= 511;
	}
	information = (void *)(handle->workBuffer + address);
	if (id == 0x10) {
		if (information->mediaClass != 2)
			return -5;
		mediaType = handle->mediaType;
	} else {
		if (information->mediaClass != 3)
			return -5;
		mediaType = (handle->mediaType & ~0x20) | 0x100;
	}
	deviceType = information->deviceType;
	if (deviceType > 3)
		goto invalidDevice;
	if (mediaType & 1) {
		accessType = 1 << deviceType;
	} else if (mediaType & 2) {
		if (!(deviceType & 0xfd))
			goto invalidDevice;
		accessType = 1 << deviceType;
	} else if (mediaType & 4) {
		if (!deviceType)
			goto invalidDevice;
		accessType = 1 << deviceType;
	} else {
		if (!deviceType)
			goto invalidDevice;
		accessType = 8;
	}
	mediaType = (mediaType & ~15) | accessType;
	handle->mediaType = mediaType;
	if (!(mediaType & 1))
		handle->writeEnabled = 0;
	handle->mediaType = mediaType | ((information->mediaSubclass & 0x18) << 6);
	MsproalUserCopy(&handle->systemInformation, information, sizeof(*information));
	return information->mediaSubclass & 0xc0 ? -9 : 0;

invalidDevice:
	handle->mediaType = 0;
	return -7;

transferError:
	return result == -11 ? -5 : result;
}

int MsproalGetModelNamePro(struct MsproalHandle *handle, char *name)
{
	struct MsproalAttributeEntry *entry;
	struct MsifDmaDescriptor transfer;
	struct MsproalSystemInformation *information = &handle->systemInformation;
	uint32_t address, blockSize, userBlocks, sectors;
	char first, second, third, unit;
	int index, result;

	for (index = 0; index < handle->attributeDirectory.count; index++) {
		entry = &handle->attributeDirectory.entries[index];
		if (entry->id == 0x15)
			goto readModel;
	}
	name[0] = 'M';
	name[1] = 'e';
	name[2] = 'm';
	name[3] = 'o';
	name[4] = 'r';
	name[5] = 'y';
	name[6] = ' ';
	name[7] = 'S';
	name[8] = 't';
	name[9] = 'i';
	name[10] = 'c';
	name[11] = 'k';
	if (handle->mediaType & 0x100) {
		name[12] = ' ';
		name[13] = 'X';
		name[14] = 'C';
		if (handle->mediaType & 4) {
			name[15] = '-';
			name[16] = 'R';
			name[17] = ' ';
			name[18] = 'D';
			name[19] = 'u';
			name[20] = 'o';
			name[21] = ' ';
			name[22] = ' ';
			name[23] = ' ';
		} else if (handle->mediaType & 2) {
			name[15] = '-';
			name[16] = 'R';
			name[17] = 'O';
			name[18] = 'M';
			name[19] = ' ';
			name[20] = 'D';
			name[21] = 'u';
			name[22] = 'o';
			name[23] = ' ';
		} else {
			name[15] = ' ';
			name[16] = 'D';
			name[17] = 'u';
			name[18] = 'o';
			name[19] = ' ';
			name[20] = ' ';
			name[21] = ' ';
			name[22] = ' ';
			name[23] = ' ';
		}
		blockSize = information->xcBlockSize[3] |
			((uint32_t)information->xcBlockSize[2] << 8) |
			((information->xcBlockSize[1] |
			((uint32_t)information->xcBlockSize[0] << 8)) << 16);
		userBlocks = information->geometry.xcUserBlockCount[3] |
			((uint32_t)information->geometry.xcUserBlockCount[2] << 8) |
			((information->geometry.xcUserBlockCount[1] |
			((uint32_t)information->geometry.xcUserBlockCount[0] << 8)) << 16);
	} else {
		name[12] = ' ';
		name[13] = 'P';
		name[14] = 'R';
		name[15] = 'O';
		if (handle->mediaType & 0x8800)
			__builtin_memcpy(name + 16, "-HG Duo ", 8);
		else if (handle->mediaType & 4)
			__builtin_memcpy(name + 16, "-R Duo  ", 8);
		else if (handle->mediaType & 2)
			__builtin_memcpy(name + 16, "-ROM Duo", 8);
		else
			__builtin_memcpy(name + 16, " Duo    ", 8);
		blockSize = information->blockSize[1] |
			((uint32_t)information->blockSize[0] << 8);
		userBlocks = information->geometry.pro.userBlockCount[1] |
			((uint32_t)information->geometry.pro.userBlockCount[0] << 8);
	}
	sectors = blockSize * userBlocks * (information->unitSize[0] >> 1);
	name[24] = ' ';
	name[25] = '(';
	name[26] = ' ';
	first = ' ';
	unit = 'M';
	if (sectors < 0x10000) {
		second = '3';
		third = '2';
	} else if (sectors < 0x20000) {
		second = '6';
		third = '4';
	} else if (sectors < 0x40000) {
		first = '1';
		second = '2';
		third = '8';
	} else if (sectors < 0x80000) {
		first = '2';
		second = '5';
		third = '6';
	} else if (sectors < 0x100000) {
		first = '5';
		second = '1';
		third = '2';
	} else {
		unit = 'G';
		second = ' ';
		if (sectors < 0x200000)
			third = '1';
		else if (sectors < 0x400000)
			third = '2';
		else if (sectors < 0x800000)
			third = '4';
		else if (sectors < 0x1000000)
			third = '8';
		else if (sectors < 0x2000000) {
			second = '1';
			third = '6';
		} else if (sectors < 0x4000000) {
			second = '3';
			third = '2';
		} else if (sectors < 0x8000000) {
			second = '6';
			third = '4';
		} else if (sectors < 0x10000000) {
			first = '1';
			second = '2';
			third = '8';
		} else if (sectors < 0x20000000) {
			first = '2';
			second = '5';
			third = '6';
		} else if (sectors < 0x40000000) {
			first = '5';
			second = '1';
			third = '2';
		} else {
			third = sectors & 0x80000000 ? '2' : '1';
			unit = 'T';
		}
	}
	name[27] = first;
	name[28] = second;
	name[29] = third;
	name[30] = unit;
	name[31] = 'B';
	name[32] = ')';
	name[33] = ' ';
	name[34] = ' ';
	name[35] = ' ';
	name[36] = ' ';
	name[37] = ' ';
	name[38] = ' ';
	name[39] = ' ';
	name[40] = ' ';
	name[41] = ' ';
	name[42] = ' ';
	name[43] = ' ';
	name[44] = ' ';
	name[45] = ' ';
	name[46] = ' ';
	name[47] = '\0';
	return 0;

readModel:
	address = entry->address[3] | ((uint32_t)entry->address[2] << 8) |
		((entry->address[1] | ((uint32_t)entry->address[0] << 8)) << 16);
	msproal_user_virt_to_bus(handle->workBuffer, &transfer.dataAddress);
	transfer.nextDescriptorAddress = 0;
	transfer.transferControl = 0x80070100;
	MsproalUserInvalidateCache(handle->workBuffer, 1024);
	result = MsproalReadAttributesPro(handle, address >> 9, 2, &transfer);
	if (result)
		return result;
	MsproalUserCopy(name, handle->workBuffer + (address & 511), 48);
	return 0;
}

int MsproalReadSectorsPro(struct MsproalHandle *handle, uint32_t sector,
	int count, const struct MsifDmaDescriptor *transfer)
{
	struct MsifControllerRegisters *controller;
	const uint16_t *program;
	uint16_t sectorHigh;
	int programSize, result;

	if (count <= 0)
		return -1;
	if (handle->busMode == 1) {
		program = gMsproalProReadSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProReadParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		return result;
	sectorHigh = sector >> 16;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = count;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 32;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo =
		(uint16_t)count >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = count;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sectorHigh;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector >> 8;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = sector;
	result = MsproalSetDmaChannel(handle, transfer, 2);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	return result;
}

int MsproalReadExfatPartitionPro(struct MsproalHandle *handle,
	uint32_t *sectorCount)
{
	struct MsproalDosMasterBootRecord *mbr = (void *)handle->workBuffer;
	struct MsproalDosPartitionEntry *partition = &mbr->partitions[0];
	struct MsproalSystemInformation *information = &handle->systemInformation;
	SceExfatfsBootSector *boot = (void *)handle->workBuffer;
	struct MsifControllerRegisters *controller;
	struct MsifDmaDescriptor transfer;
	const uint16_t *program;
	const uint8_t *alternateSector;
	const uint8_t *signature = (void *)&boot->exfat.signature;
	uint32_t start, count, capacity, blockSize, userBlocks, sector;
	int programSize, index, result;

	msproal_user_virt_to_bus(handle->workBuffer, &transfer.dataAddress);
	transfer.nextDescriptorAddress = 0;
	transfer.transferControl = 0x80070080;
	MsproalUserInvalidateCache(handle->workBuffer, 512);
	if (handle->busMode == 1) {
		program = gMsproalProReadSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProReadParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		goto transferError;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = 1;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 32;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 1;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	result = MsproalSetDmaChannel(handle, &transfer, 2);
	if (result)
		goto transferError;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	if (result)
		goto transferError;
	blockSize = information->xcBlockSize[3] |
		((uint32_t)information->xcBlockSize[2] << 8) |
		((information->xcBlockSize[1] |
		((uint32_t)information->xcBlockSize[0] << 8)) << 16);
	userBlocks = information->geometry.xcUserBlockCount[3] |
		((uint32_t)information->geometry.xcUserBlockCount[2] << 8) |
		((information->geometry.xcUserBlockCount[1] |
		((uint32_t)information->geometry.xcUserBlockCount[0] << 8)) << 16);
	capacity = (information->unitSize[0] >> 1) * blockSize * userBlocks;
	start = partition->startingSector[0] |
		((uint32_t)partition->startingSector[1] << 8) |
		((partition->startingSector[2] |
		((uint32_t)partition->startingSector[3] << 8)) << 16);
	count = partition->sectorCount[0] |
		((uint32_t)partition->sectorCount[1] << 8) |
		((partition->sectorCount[2] |
		((uint32_t)partition->sectorCount[3] << 8)) << 16);
	if (partition->bootIndicator & 0x7f)
		return -6;
	if (mbr->signature[0] == 0x55 && mbr->signature[1] == 0xaa &&
		partition->type == 7 && capacity >= count + start) {
		sector = start;
	} else if (!partition->bootIndicator && mbr->signature[0] == 0x55 &&
		mbr->signature[1] == 0xaa && !partition->type && count + start == 0) {
		alternateSector = mbr->alternatePartition.alternatePartitionSector;
		sector = alternateSector[0] | ((uint32_t)alternateSector[1] << 8) |
			((alternateSector[2] | ((uint32_t)alternateSector[3] << 8)) << 16);
	} else {
		return -6;
	}
	MsproalUserInvalidateCache(handle->workBuffer, 512);
	result = MsproalReadSectorsPro(handle, sector, 1, &transfer);
	if (result)
		goto transferError;
	if ((!boot->exfat.jump[0] && !boot->exfat.jump[1] &&
		!boot->exfat.jump[2]) || signature[0] != 0x55 || signature[1] != 0xaa ||
		boot->exfat.fileSystemName[0] != 'E' ||
		boot->exfat.fileSystemName[1] != 'X' ||
		boot->exfat.fileSystemName[2] != 'F' ||
		boot->exfat.fileSystemName[3] != 'A' ||
		boot->exfat.fileSystemName[4] != 'T' ||
		boot->exfat.fileSystemName[5] != ' ' ||
		boot->exfat.fileSystemName[6] != ' ' ||
		boot->exfat.fileSystemName[7] != ' ')
		return -6;
	for (index = 0; index < sizeof(boot->exfat.mustBeZero); index++) {
		if (boot->exfat.mustBeZero[index])
			return -6;
	}
	handle->partitionStart = sector;
	if (sectorCount)
		*sectorCount = capacity;
	return 0;

transferError:
	return result == -11 ? -6 : result;
}

int MsproalReadFatPartitionPro(struct MsproalHandle *handle)
{
	struct MsproalDosMasterBootRecord *mbr = (void *)handle->workBuffer;
	struct MsproalDosPartitionEntry *partition = &mbr->partitions[0];
	SceExfatfsBootSector *boot = (void *)handle->workBuffer;
	struct MsifControllerRegisters *controller;
	struct MsifDmaDescriptor transfer;
	const uint16_t *program;
	const uint8_t *signature = (void *)&boot->fat16.signature;
	uint32_t sector = 0;
	int programSize, result;

	msproal_user_virt_to_bus(handle->workBuffer, &transfer.dataAddress);
	transfer.nextDescriptorAddress = 0;
	transfer.transferControl = 0x80070080;
	MsproalUserInvalidateCache(handle->workBuffer, 512);
	if (handle->busMode == 1) {
		program = gMsproalProReadSerialProgram;
		programSize = 65;
	} else {
		program = gMsproalProReadParallelProgram;
		programSize = 59;
	}
	result = MsproalLoadSequencerProgram(handle, 0, programSize, program);
	if (result)
		goto transferError;
	controller = MSPROAL_CONTROLLER(handle);
	controller->sequencer.sectorCount = 1;
	controller->sequencer.control = 224;
	controller->sequencer.configuration &= ~0x300000;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 32;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 1;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	MSPROAL_CONTROLLER(handle)->sequencer.commandPort.commandFifo = 0;
	result = MsproalSetDmaChannel(handle, &transfer, 2);
	if (result)
		goto transferError;
	result = MsproalExecuteSequencer(handle, 0);
	MsproalClearDmaChannels(handle);
	if (result)
		goto transferError;
	if (partition->bootIndicator == 0x80 && mbr->signature[0] == 0x55 &&
		mbr->signature[1] == 0xaa && partition->type &&
		(partition->startingSector[0] || partition->startingSector[1] ||
		partition->startingSector[2] || partition->startingSector[3]) &&
		(partition->sectorCount[0] || partition->sectorCount[1] ||
		partition->sectorCount[2] || partition->sectorCount[3])) {
		sector = partition->startingSector[0] |
			((uint32_t)partition->startingSector[2] << 16) |
			((uint32_t)partition->startingSector[1] << 8) |
			((uint32_t)partition->startingSector[3] << 24);
		MsproalUserInvalidateCache(handle->workBuffer, 512);
		result = MsproalReadSectorsPro(handle, sector, 1, &transfer);
		if (result)
			goto transferError;
	}
	if ((boot->fat16.parameters.jump[0] & 0xfd) != 0xe9 ||
		signature[0] != 0x55 || signature[1] != 0xaa)
		return -6;
	handle->partitionStart = sector;
	return 0;

transferError:
	return result == -11 ? -6 : result;
}

int MsproalMountPro(struct MsproalHandle *handle, int busMode)
{
	struct MsifRegisterWindow *registers = handle->registers;
	struct MsifControllerRegisters *controller =
		(void *)((uint8_t *)registers -
		offsetof(struct MsifControllerRegisters, transfer));
	uint32_t sectorCount, capacitySectors;
	uint64x1_t capacity;
	uint8_t mediaId[16];
	int result, capacityExponent;

	registers->transferControl &= ~0x2000;
	controller->sequencer.control = 96;
	result = MsproalLoadSequencerProgram(handle, 0, 36,
		gMsproalProInitializeProgram);
	if (result)
		return result;
	result = MsproalExecuteSequencer(handle, 0);
	registers = handle->registers;
	registers->transferControl |= 0x2000;
	if (result)
		return result;
	controller = (void *)((uint8_t *)registers -
		offsetof(struct MsifControllerRegisters, transfer));
	sectorCount = controller->sequencer.mediaStatus;
	handle->requestedBusMode = busMode;
	if ((uint8_t)sectorCount != 1)
		handle->writeEnabled = 0;
	if (busMode != 1) {
		result = MsproalSwitchBusMode(handle, 4);
		if (result)
			return result;
	}
	result = MsproalReadSystemInformationPro(handle);
	if (result)
		return result;
	capacityExponent = MsifAuthenticateMedia(handle, mediaId);
	if (capacityExponent < 0)
		return capacityExponent;
	if (!(handle->mediaType & 0x100))
		return MsproalReadFatPartitionPro(handle);
	result = MsproalReadExfatPartitionPro(handle, &sectorCount);
	if (result)
		return result;
	if (!capacityExponent)
		return capacityExponent;
	/* VSHL takes its signed shift count from the low byte of the lane. */
	capacity = vshl_u64(vdup_n_u64(1),
		vreinterpret_s64_s32(vdup_n_s32(capacityExponent)));
	capacitySectors = vget_lane_u64(vshr_n_u64(capacity, 9), 0);
	if (capacitySectors < sectorCount || sectorCount < capacitySectors >> 1)
		return -205;
	return msif_check_mid(handle, mediaId);
}

int MsproalFormatPro(struct MsproalHandle *handle, int mode)
{
	struct MsifDmaDescriptor transfer;
	uint8_t interrupt[8], transferSize[8];
	struct MsproalFormatWorkspace *workspace = &handle->formatWorkspace;
	const uint8_t *startingSector;
	int result;

	transferSize[0] = 0;
	result = MsproalWriteMediaRegisters(handle, 23, 1, transferSize);
	if (result)
		goto done;
	result = MsproalSendExtendedCommand(handle, 16, 0, mode != 0);
	if (result)
		goto done;
	result = MsproalWaitCommand(handle, 3000, interrupt);
	if (result) {
		if (result == -105)
			goto command_error;
		goto check_command_error;
	}
	do {
		if (!(interrupt[0] & 0x20))
			break;
		result = MsproalReadShortData(handle, sizeof(workspace->progress),
			&workspace->progress);
		if (result)
			goto done;
		result = MsproalWaitCommand(handle, 3000, interrupt);
	} while (!result);
	if (result) {
		if (result != -3)
			goto check_command_error;
		if ((handle->interruptFlags & 0x300) ||
			workspace->progress.completionCounters[0] !=
			workspace->progress.completionCounters[1])
			goto command_error;
		result = MsproalResetPro(handle);
		if (result)
			goto done;
	}
	msproal_user_virt_to_bus(&workspace->masterBootRecord, &transfer.dataAddress);
	transfer.nextDescriptorAddress = 0;
	transfer.transferControl = 0x80070080;
	MsproalUserInvalidateCache(&workspace->masterBootRecord,
		sizeof(workspace->masterBootRecord));
	result = MsproalReadSectorsPro(handle, 0, 1, &transfer);
	if (result) {
		if (result == -11)
			result = -6;
		goto done;
	}
	startingSector = workspace->masterBootRecord.partitions[0].startingSector;
	handle->partitionStart = startingSector[0] |
		((uint32_t)startingSector[1] << 8) |
		((startingSector[2] | ((uint32_t)startingSector[3] << 8)) << 16);
done:
	return result;

check_command_error:
	if (result != -106)
		goto done;
command_error:
	result = -3;
	goto done;
}

int MsproalSleepPro(struct MsproalHandle *handle)
{
	int result;

	result = MsproalExecuteCommand(handle, 17, 1000);
	return result == -105 ? -3 : result;
}

int MsproalCommandWriteShortData(struct MsproalHandle *handle, int command,
	int count, const void *data, int timeout)
{
	uint8_t interrupt[4];
	uint8_t transferSize[8];
	int result;

	switch (count) {
	case 32:
		transferSize[0] = 0;
		break;
	case 64:
		transferSize[0] = 1;
		break;
	case 128:
		transferSize[0] = 2;
		break;
	case 256:
		transferSize[0] = 3;
		break;
	default:
		return -1;
	}
	result = MsproalWriteMediaRegisters(handle, 23, 1, transferSize);
	if (result)
		return result;
	result = MsproalExecuteCommand(handle, command, timeout);
	if (result)
		return result;
	result = MsproalWriteShortData(handle, count, data);
	if (result)
		return result;
	return MsproalWaitCommand(handle, timeout, interrupt);
}

int MsproalCommandReadShortData(struct MsproalHandle *handle, int command,
	int count, void *data, int timeout)
{
	uint8_t transferSize[8];
	uint32_t interrupt;
	int result;

	switch (count) {
	case 32:
		transferSize[0] = 0;
		break;
	case 64:
		transferSize[0] = 1;
		break;
	case 128:
		transferSize[0] = 2;
		break;
	case 256:
		transferSize[0] = 3;
		break;
	default:
		return -1;
	}
	result = MsproalWriteMediaRegisters(handle, 23, 1, transferSize);
	if (result)
		return result;
	result = MsproalExecuteCommand(handle, command, timeout);
	if (result)
		return result;
	result = MsproalReadShortData(handle, count, data);
	if (result)
		return result;
	return MsproalWaitCommand(handle, timeout, (uint8_t *)&interrupt);
}

int MsproalCommandReadLongData(struct MsproalHandle *handle, int command,
	void *data, int timeout)
{
	uint8_t interrupt[4];
	int result;

	result = MsproalExecuteCommand(handle, command, timeout);
	if (result)
		return result;
	result = MsproalReadLongData(handle, data);
	if (result)
		return result;
	return MsproalWaitCommand(handle, timeout, interrupt);
}

