/*
 * The Memory Stick interface controller (SceMsif). The window SceMsif maps
 * starts with the media sequencer, then the transfer engine and its two DMA
 * channels. No source names the registers, so the names are descriptive.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

/* the controller's 4 KiB register window */
#define MSIF_REG_BASE		0xe0900000
#define MSIF_REG_SIZE		0x1000

typedef struct MsifDmaChannelRegisters {
	volatile uint32_t dataAddress;
	volatile uint32_t nextDescriptorAddress;
	volatile uint32_t transferControl;
	volatile uint32_t diagnostic;
} MsifDmaChannelRegisters;

/* the transfer engine, after the sequencer bank */
typedef struct MsifRegisterWindow {
	union {
		struct {
			volatile uint16_t transferCommand;
			volatile uint16_t commandDiagnostic;
		};
		volatile uint32_t commandSnapshot;
	};
	volatile uint32_t transferFifo;
	union {
		struct {
			volatile uint16_t interruptStatus;
			volatile uint16_t interruptDiagnostic;
		};
		volatile uint32_t interruptSnapshot;
	};
	union {
		struct {
			volatile uint16_t transferControl;
			volatile uint16_t controlDiagnostic;
		};
		volatile uint32_t controlSnapshot;
	};
	volatile uint32_t diagnostic[2];
	uint8_t reserved[56];
	struct MsifDmaChannelRegisters dma[2];
	volatile uint32_t powerSupply;
} MsifRegisterWindow;

typedef struct MsifSequencerRegisters {
	volatile uint16_t control;
	volatile uint16_t command;
	volatile uint16_t reset;
	volatile uint16_t resetDiagnostic;
	volatile uint16_t result;
	volatile uint16_t resultDiagnostic;
	volatile uint32_t configuration;
	volatile uint16_t sectorCount;
	volatile uint16_t countDiagnostic;
	volatile uint16_t diagnostic;
	volatile uint16_t commandError;
	volatile uint32_t diagnosticWords[2];
	union {
		volatile uint32_t instructionDiagnostic;
		volatile uint16_t instructionFifo;
	};
	union {
		volatile uint32_t diagnostic;
		volatile uint8_t commandFifo;
	} commandPort;
	volatile uint32_t mediaStatus;
	volatile uint32_t finalDiagnostic;
} MsifSequencerRegisters;

typedef struct MsifControllerRegisters {
	union {
		struct MsifSequencerRegisters sequencer;
		volatile uint32_t sequencerSnapshot[12];
	};
	struct MsifRegisterWindow transfer;
} MsifControllerRegisters;

_Static_assert(offsetof(MsifRegisterWindow, dma) == 0x50, "MSIF DMA channels");
_Static_assert(offsetof(MsifRegisterWindow, powerSupply) == 0x70, "MSIF power supply");
_Static_assert(offsetof(MsifControllerRegisters, transfer) == 0x30, "MSIF transfer engine");

/* Transfer Protocol Commands (TPC) and Memory Stick PRO media status bits */
#define MSIF_TPC_READ_LONG_DATA		0x2000
#define MSIF_TPC_READ_SHORT_DATA	0x3000
#define MSIF_TPC_READ_REG		0x4000
#define MSIF_TPC_GET_INT		0x7000
#define MSIF_TPC_SET_R_W_REG_ADRS	0x8000
#define MSIF_TPC_EX_SET_CMD		0x9000
#define MSIF_TPC_WRITE_REG		0xb000
#define MSIF_TPC_WRITE_SHORT_DATA	0xc000
#define MSIF_TPC_WRITE_LONG_DATA	0xd000
#define MSIF_TPC_SET_CMD		0xe000
#define MSIF_TPC_SEQ_AUTO		0x0800

#define MSPRO_CMD_READ_DATA		0x20
#define MSPRO_CMD_WRITE_DATA		0x21
#define MSPRO_CMD_READ_ATRB		0x24
#define MSPRO_CMD_STOP			0x25

#define MSPRO_INT_CMDNK			0x01
#define MSPRO_INT_BREQ			0x20
#define MSPRO_INT_ERR			0x40
#define MSPRO_INT_CED			0x80
#define MSPRO_SERIAL_INT(bits)		((uint16_t)((bits) << 8))

/* Hardware sequencer registers and microcode instruction encoders */
#define MSIF_SEQ_REG_LATCHED		0x00
#define MSIF_SEQ_REG_SECTOR_COUNT	0x60
#define MSIF_SEQ_REG_MEDIA_STATUS	0xa9
#define MSIF_SEQ_REG_SERIAL_INT		0xb4
#define MSIF_SEQ_REG_PARALLEL_INT	0xb8

#define MSIF_SEQ_OP_TEST_STATUS		0x8900
#define MSIF_SEQ_OP_TEST_INIT		0x8a00

#define MSIF_SEQ_HALT(code)		((uint16_t)(code))
#define MSIF_SEQ_ABORT_VECTOR		0xe000
#define MSIF_SEQ_DELAY(ticks)		((uint16_t)(0x0200 | (ticks)))
#define MSIF_SEQ_WAIT(events)		((uint16_t)(0x0300 | (events)))
#define MSIF_SEQ_TPC_GET_INT(tpc)	0x24a1, ((uint16_t)(tpc))
#define MSIF_SEQ_TPC_CMD_FIFO(tpc)	0x2721, ((uint16_t)(tpc))
#define MSIF_SEQ_TPC_DMA_READ(tpc)	0x27a2, ((uint16_t)(tpc))
#define MSIF_SEQ_TPC_DMA_WRITE(tpc)	0x27a4, ((uint16_t)(tpc))
#define MSIF_SEQ_TPC_INLINE_CMD(tpc, cmd) \
	0x3421, ((uint16_t)(tpc)), ((uint16_t)((cmd) << 8)), 0x0000, 0x0000, 0x0000
#define MSIF_SEQ_LOAD_COUNT(count)	0x4400, ((uint16_t)(count))
#define MSIF_SEQ_WRITE_REG(reg, val)	0x4600, ((uint16_t)(val)), ((uint16_t)(0x6200 | (reg)))
#define MSIF_SEQ_SET_ERROR(code)	0x4700, ((uint16_t)(code))
#define MSIF_SEQ_DEC_CMP(reg, val)	((uint16_t)(0x8400 | (reg))), ((uint16_t)(val))
#define MSIF_SEQ_TEST_CLEAR(reg, bits) \
	((uint16_t)(MSIF_SEQ_OP_TEST_STATUS | (reg))), \
	((uint16_t)~(bits)), ((uint16_t)~(bits))
#define MSIF_SEQ_TEST_MATCH(op, reg, expected, bits) \
	((uint16_t)((op) | (reg))), ((uint16_t)(expected)), ((uint16_t)~(bits))
#define MSIF_SEQ_BEQ(offset)		((uint16_t)(0xa080 | ((offset) & 0x7f)))
#define MSIF_SEQ_BNE(offset)		((uint16_t)(0xa180 | ((offset) & 0x7f)))
#define MSIF_SEQ_BRA(offset)		((uint16_t)(0xa280 | ((offset) & 0x7f)))

