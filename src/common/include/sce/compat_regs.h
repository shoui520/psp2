/*
 * The PSP compatibility subsystem's pages as the ARM side maps them:
 * SceCompatMailbox (0xe5070000; doorbells to the Allegrex side, raised by
 * SceCompat and SceDeci2p), SceCompatLCDDMA (0xe5071000) and
 * SceCompatSharedSram (0xe8100000, SceCompatSharedSram in sce/compat.h).
 * Names follow what SceCompat does with each word.
 */
#pragma once

#include <stddef.h>

#define COMPAT_MAILBOX_BASE		0xe5070000
#define COMPAT_LCDDMA_BASE		0xe5071000
#define COMPAT_SHARED_SRAM_BASE		0xe8100000

/* one doorbell per word: SceCompat sets the word in enableInterrupt, then raises it */
typedef struct SceCompatMailboxRegisters {
	volatile unsigned int raiseInterrupt[32];
	volatile unsigned int enableInterrupt[32];
	unsigned int reserved[960];
} SceCompatMailboxRegisters;

_Static_assert(offsetof(SceCompatMailboxRegisters, enableInterrupt) == 0x80,
	"compat mailbox enable");
_Static_assert(sizeof(SceCompatMailboxRegisters) == 0x1000, "compat mailbox page");

/* SceCompatLCDDMA: the LCD DMA the compat module drives; unused spans have no accesses. */
typedef struct SceCompatLcdDmaRegisters {
	volatile unsigned int stopCommand;
	volatile unsigned int completionAck;
	volatile unsigned int activeStatus;
	unsigned int reservedBeforeInitialization[6];
	volatile unsigned int initializationMask;
	volatile unsigned int initializationControl;
	unsigned int reservedBeforeStatus;
	volatile unsigned int status;
	volatile unsigned int width;
	volatile unsigned int widthSetup;
	volatile unsigned int height;
	volatile unsigned int heightSetup;
	unsigned int reservedBeforeAddress[2];
	volatile unsigned int physicalAddress;
	volatile unsigned int stride;
	unsigned int reservedBeforeBufferSetup[9];
	volatile unsigned int bufferSetup;
	unsigned int reservedBeforeTransferSetup[2];
	volatile unsigned int transferSetup;
	unsigned int reservedBeforePixelFormat[3];
	volatile unsigned int pixelFormat;
	volatile unsigned int enable;
	unsigned int reservedBeforeStopControl[8];
	volatile unsigned int stopControl;
	volatile unsigned int startControl;
	unsigned int reservedTail[975];
} SceCompatLcdDmaRegisters;

_Static_assert(sizeof(SceCompatLcdDmaRegisters) == 4096,
	"compat LCDDMA mapped aperture");
_Static_assert(offsetof(SceCompatLcdDmaRegisters, stopControl) == 0xbc,
	"compat LCDDMA stop control");
