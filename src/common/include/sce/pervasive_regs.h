#pragma once

#include <stdint.h>
#include <stddef.h>

/* Lowio's ScePervasiveReset page at physical 0xe3101000.
 * PLACEHOLDER: unidentified control banks have no published register meanings.
 * Known peripheral fields identify base registers, not peripheral counts.
 */
typedef union ScePervasiveResetRegisters {
	struct {
		volatile uint32_t unidentifiedBeforeVenezia[13];
		volatile uint32_t veneziaReset;
		volatile uint32_t unidentifiedBeforeDsi[18];
		volatile uint32_t dsiReset;
		volatile uint32_t unidentifiedBeforeMsif[11];
		volatile uint32_t msifReset;
		volatile uint32_t unidentifiedBeforeGpio[19];
		volatile uint32_t gpioReset;
		volatile uint32_t spiReset;
		volatile uint32_t unidentifiedBeforeUart[6];
		volatile uint32_t uartReset;
		volatile uint32_t unidentifiedAfterUart[951];
	} controls;
	volatile uint32_t registers[1024];
} ScePervasiveResetRegisters;

/* ScePervasiveTas0 through Tas7 occupy eight pages at 0xe3108000.
 * PLACEHOLDER: only the dedicated test-and-set lock register is identified;
 * generic physical register access also exposes the remaining words.
 */
typedef struct ScePervasiveTasChannel {
	volatile uint32_t lock;
	volatile uint32_t unidentifiedRegisters[1023];
} ScePervasiveTasChannel;

typedef struct ScePervasiveTasRegisters {
	ScePervasiveTasChannel channels[8];
} ScePervasiveTasRegisters;

_Static_assert(offsetof(ScePervasiveResetRegisters, controls.veneziaReset) == 0x34,
	"Venezia reset register");
_Static_assert(offsetof(ScePervasiveResetRegisters, controls.dsiReset) == 0x80,
	"DSI reset register base");
_Static_assert(offsetof(ScePervasiveResetRegisters, controls.msifReset) == 0xb0,
	"MSIF reset register");
_Static_assert(offsetof(ScePervasiveResetRegisters, controls.gpioReset) == 0x100,
	"GPIO reset register");
_Static_assert(offsetof(ScePervasiveResetRegisters, controls.spiReset) == 0x104,
	"SPI reset register base");
_Static_assert(offsetof(ScePervasiveResetRegisters, controls.uartReset) == 0x120,
	"UART reset register base");
_Static_assert(sizeof(ScePervasiveResetRegisters) == 0x1000, "reset register page");
_Static_assert(sizeof(ScePervasiveTasChannel) == 0x1000, "TAS channel page");
_Static_assert(sizeof(ScePervasiveTasRegisters) == 0x8000, "TAS register pages");
