/*
 * The debug SDIO function's shared memory, as SceSDbgSdio (os0:kd/sdbgsdio.skprx,
 * 0xE5800000) and SceDbgSdio (os0:kd/dbgsdio.skprx, 0xE5810000) drive it.
 * Names are ours: neither module carries debug names for it.
 *
 *	0x040			CIS valid flag (cleared on suspend)
 *	0x100 + 0x10 * n	channel n control (word) ...
 *	0x108 + 0x10 * n	... and status (word, write 1s to clear)
 *	0x140			mailbox interrupt pending, bit n per channel
 *	0x400, 0x480		CIS of SDIO function 0 and 1
 *	0x800 + 0x100 * n	channel n receive window (host to target)
 *	0xC00 + 0x100 * n	channel n transmit window (target to host)
 *
 * A window starts with a 16-byte descriptor and, at +0x10, the reply to it.
 * Messages of up to 0xE0 bytes are carried inline after them (at +0x20);
 * longer ones go through the whole window in chunks, one handshake per chunk,
 * or (SceDbgSdio) by DMA to the window's physical address.
 */

#pragma once

#include <scetypes.h>

/* the windows' physical addresses: SceSDbgSdio0 and SceDbgSdio1 */
#define SDIO_WINDOW_PADDR(n)	(0xe5800000 + (n) * SDIO_WINDOW_SIZE)
#define SDIO_WINDOW_SIZE	0x10000

/* a window's last words: its bus error log, the secure words first */
#define SDIO_WINDOW_SECURE_BUS_ERROR_ADDRESS	0xffe8
#define SDIO_WINDOW_SECURE_BUS_ERROR_ATTRIBUTE	0xffec
#define SDIO_WINDOW_BUS_ERROR_ADDRESS		0xfff0
#define SDIO_WINDOW_BUS_ERROR_ATTRIBUTE		0xfff4

/* the window offsets the descriptors carry (SdioWindowDesc.offset) */
#define SHM_RX_WINDOW(n)	(0x800 + 0x100 * (n))
#define SHM_TX_WINDOW(n)	(0xc00 + 0x100 * (n))

/* a channel window */
#define WINDOW_SIZE		0x100
#define WINDOW_REPLY		0x10		/* the reply to the descriptor */
#define WINDOW_INLINE		0x20		/* inline data after them */
#define INLINE_MAX		0xe0

/* status register: write the bit back to acknowledge it */
#define STATUS_PENDING		0xffff
#define STATUS_RX_NOTIFY	0x00001		/* host posted a message */
#define STATUS_RX_MESSAGE	0x00002		/* host posted a descriptor */
#define STATUS_RX_DATA		0x00004		/* host filled the window */
#define STATUS_ERROR		0x01000		/* the link failed */
#define STATUS_TX_ACCEPTED	0x20000		/* host took our descriptor */
#define STATUS_TX_DATA		0x40000		/* host emptied the window */

/* control register */
#define CTRL_TX_MESSAGE		0x00002		/* descriptor posted */
#define CTRL_TX_DATA		0x00004		/* window filled (DMA: cleared by the host) */
#define CTRL_RX_REQUEST		0x20000		/* reply posted, send the data */
#define CTRL_RX_DATA		0x40000		/* window emptied */

/* SdioWindowDesc.flags */
#define DESC_DMA		0x1		/* through the DMA window */
#define DESC_INLINE		0x2

/* a transmit descriptor, or our reply to a host descriptor */
typedef struct SdioWindowDesc {
	SceUInt32 offset;			/* of the data in shared memory */
	SceSize size;
	SceUInt32 flags;			/* DESC_* */
	SceSize chunkSize;
} SdioWindowDesc;

/* the descriptor of a message from the host */
typedef struct SdioHostDesc {
	SceSize size;
	SceUInt32 flags;			/* DESC_* */
	SceUInt32 reserved[2];
} SdioHostDesc;

/* one direction of a channel: its window in shared memory */
typedef struct SdioPipe {
	volatile SceUInt8 *desc;		/* descriptor (tx: ours, rx: the host's) */
	volatile SceUInt8 *reply;		/* the reply to it */
	volatile SceUInt8 *inlineData;
	volatile SceUInt8 *data;		/* chunked data: the whole window */
	SceSize inlineMax;
	SceUInt32 dmaOffset;			/* of the DMA window: always 0 */
	SceUInt32 offset;			/* of the window in shared memory */
	SceSize chunkSize;
} SdioPipe;

/* one channel's registers */
typedef struct SdioChannelRegisters {
	SceUInt32 control;			/* CTRL_* */
	SceUInt32 reserved0;
	SceUInt32 status;			/* STATUS_*, write 1s to clear */
	SceUInt32 reserved1;
} SdioChannelRegisters;

/* a channel window: descriptor, reply, inline data */
typedef struct SdioWindow {
	SceUInt8 desc[0x10];
	SceUInt8 reply[0x10];
	SceUInt8 inlineData[INLINE_MAX];
} SdioWindow;

typedef struct SdioSharedMemory {
	SceUInt8 reserved0[0x40];
	SceUInt32 cisValid;			/* cleared on suspend */
	SceUInt8 reserved1[0xbc];
	SdioChannelRegisters channel[4];
	SceUInt32 intrPending;			/* mailbox interrupt, bit n per channel */
	SceUInt8 reserved2[0x2bc];
	SceUInt8 cisFunc0[0x80];		/* CIS of SDIO function 0 ... */
	SceUInt8 cisFunc1[0x380];		/* ... and 1 */
	SdioWindow rxWindow[4];			/* host to target */
	SdioWindow txWindow[4];			/* target to host */
} SdioSharedMemory;

_Static_assert(sizeof(SdioWindow) == WINDOW_SIZE, "SdioWindow size");
_Static_assert(__builtin_offsetof(SdioSharedMemory, cisValid) == 0x40, "cisValid");
_Static_assert(__builtin_offsetof(SdioSharedMemory, channel) == 0x100, "channel registers");
_Static_assert(__builtin_offsetof(SdioSharedMemory, intrPending) == 0x140, "intrPending");
_Static_assert(__builtin_offsetof(SdioSharedMemory, cisFunc0) == 0x400, "cisFunc0");
_Static_assert(__builtin_offsetof(SdioSharedMemory, cisFunc1) == 0x480, "cisFunc1");
_Static_assert(__builtin_offsetof(SdioSharedMemory, rxWindow) == SHM_RX_WINDOW(0), "rxWindow");
_Static_assert(__builtin_offsetof(SdioSharedMemory, txWindow) == SHM_TX_WINDOW(0), "txWindow");
