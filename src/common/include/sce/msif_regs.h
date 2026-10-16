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
