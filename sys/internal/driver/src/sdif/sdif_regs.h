/*
 * The SD host controllers (SD Host Controller Standard register map): SceSdif0
 * at 0xe0b00000 drives the eMMC, SceSdif1-3 from 0xe0c00000 the game card, the
 * WLAN/BT SDIO and the memory card slot. Field names after the SD Host
 * Controller Simplified Specification; SceSdif and first_loader use them.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define SDIF_PORTS			4
#define SDIF_REG_BASE(n)		((n) == 0 ? 0xe0b00000 : 0xe0c00000 + ((n) - 1) * 0x10000)
#define SDIF_REG_SIZE			0x1000

typedef struct SdifHostRegisters {
	uint32_t dmaAddress;
	uint16_t blockSize;
	uint16_t blockCount;
	uint32_t argument;
	uint16_t transferMode;
	uint16_t command;
	uint32_t response[4];
	union {
		uint8_t byte;
		uint16_t halfword;
		uint32_t word;
	} buffer;
	uint32_t presentState;
	uint8_t hostControl;
	uint8_t powerControl;
	uint8_t blockGapControl;
	uint8_t wakeupControl;
	uint16_t clockControl;
	uint8_t timeoutControl;
	uint8_t softwareReset;
	uint16_t normalInterruptStatus;
	uint16_t errorInterruptStatus;
	uint16_t normalInterruptEnable;
	uint16_t errorInterruptEnable;
	uint16_t normalSignalEnable;
	uint16_t errorSignalEnable;
	uint16_t autoCmd12Error;
	uint16_t hostControl2;
	uint32_t capabilities;
	uint32_t capabilitiesHigh;
	uint32_t maximumCurrent;
	uint32_t maximumCurrentHigh;
	uint16_t forceAutoCmd12Error;
	uint16_t forceErrorInterrupt;
	uint8_t AdmaErrStatus;
	uint8_t reservedAdma[3];
	uint32_t admaSystemAddress;
	uint32_t admaSystemAddressHigh;
	uint8_t reservedStandard[32];
	uint16_t ceataControl;
	uint8_t reservedVendor[122];
	uint16_t slotInterruptStatus;
	uint16_t hostVersion;
} SdifHostRegisters;

_Static_assert(offsetof(SdifHostRegisters, presentState) == 0x24, "SDHC present state");
_Static_assert(offsetof(SdifHostRegisters, normalInterruptStatus) == 0x30, "SDHC interrupt status");
_Static_assert(offsetof(SdifHostRegisters, admaSystemAddress) == 0x58, "SDHC ADMA address");
_Static_assert(offsetof(SdifHostRegisters, hostVersion) == 0xfe, "SDHC host version");
