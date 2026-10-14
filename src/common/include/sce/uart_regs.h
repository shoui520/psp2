/* UART register bank used by Sysmem and the baseband transport. */
#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct UartRegs {
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
} UartRegs;

_Static_assert(sizeof(UartRegs) == 0x7c, "UART register bank size");
_Static_assert(offsetof(UartRegs, transmitControl) == 0x24, "UART transmit control");
_Static_assert(offsetof(UartRegs, interruptStatus) == 0x5c, "UART interrupt status");
_Static_assert(offsetof(UartRegs, tx) == 0x70, "UART transmit FIFO");
_Static_assert(offsetof(UartRegs, rx) == 0x78, "UART receive FIFO");
