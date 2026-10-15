/* UART register bank used by Sysmem and the baseband transport. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

/* SceUartReg: seven ports, one 0x10000-byte window each; SceUartClkgenReg */
#define UART_PORTS			7
#define UART_REG_BASE			0xe2030000
#define UART_REG_SIZE			0x10000
#define UART_CLKGEN_BASE		0xe3105000

typedef struct SceUartRegisters {
	SceUInt32 reserved0;
	SceUInt32 enable;
	SceUInt32 reserved1[2];
	SceUInt32 control;
	SceUInt32 reserved2[3];
	SceUInt32 mode;
	SceUInt32 transmitControl;
	SceUInt32 status;
	SceUInt32 reserved4;
	SceUInt32 flowControl;
	SceUInt32 reserved5[3];
	SceUInt32 irqConfig;
	SceUInt32 reserved6[3];
	SceUInt32 irqMask;
	SceUInt32 rxAck;
	SceUInt32 reserved7;
	SceUInt32 interruptStatus;
	SceUInt32 fifoConfig;
	SceUInt32 fifoControl;
	SceUInt32 fifoStatus;
	SceUInt32 reserved8;
	SceUInt32 tx;
	SceUInt32 reserved9;
	SceUInt32 rx;
} SceUartRegisters;

_Static_assert(sizeof(SceUartRegisters) == 0x7c, "UART register bank size");
_Static_assert(offsetof(SceUartRegisters, transmitControl) == 0x24, "UART transmit control");
_Static_assert(offsetof(SceUartRegisters, interruptStatus) == 0x5c, "UART interrupt status");
_Static_assert(offsetof(SceUartRegisters, tx) == 0x70, "UART transmit FIFO");
_Static_assert(offsetof(SceUartRegisters, rx) == 0x78, "UART receive FIFO");

/*
 * SceUartClkgenReg: one clock generator word per port. Bits 17:16 select the
 * input clock (1 = 3 MHz, 2 = ~12 MHz), bits 15:0 are the integer prescaler
 * (0 stops the port).
 */
typedef struct SceUartClkgenRegisters {
	SceUInt32 port[UART_PORTS];
} SceUartClkgenRegisters;

_Static_assert(sizeof(SceUartClkgenRegisters) == 0x1c, "UART clkgen size");
