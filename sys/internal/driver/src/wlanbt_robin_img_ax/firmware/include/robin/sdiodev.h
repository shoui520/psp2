/*
 * The card's three SDIO functions as the RAM code drives them: function 0
 * carries the WLAN commands, events and data ("sdio"), function 1 the
 * Bluetooth HCI ("bt_sdio"), function 2 the AMP HCI ("amp_sdio"). Each is an
 * mdev driver of the Mask ROM whose callbacks (sys/sdiodev.c) move packets
 * between the host's ports and the firmware's queues. Names are inferred.
 */
#ifndef ROBIN_SDIODEV_H
#define ROBIN_SDIODEV_H

#include <stddef.h>
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/mdev.h"

#define SDIO_FUNCTIONS		3
#define SDIO_FUNCTION_WLAN	0
#define SDIO_FUNCTION_BT	1
#define SDIO_FUNCTION_AMP	2
#define SDIO_PORTS		16	/* ports of a function: one bit in a bitmap each */

/* The register block of one function (0x80000100 + 0x100 * function). */
struct SdioFunctionRegs {
	uint8_t reserved0[2];
	uint8_t hostState;		/* bit 1: the host enabled the function */
	uint8_t reserved3;
	uint16_t sendPorts;		/* ports holding data for the host */
	uint16_t receivePorts;		/* ports the host wrote to; written: ports offered to it */
	uint16_t sendLength[SDIO_PORTS];
	uint8_t hostFlags;		/* bit 2: the host's access is blocked */
	uint8_t reserved29[7];
	uint8_t hostIrq;		/* cause raised to the host */
	uint8_t reserved31[3];
	uint8_t irqMask;
	uint8_t reserved35[3];
	uint8_t irqStatus;		/* what the host did; written back to acknowledge */
	uint8_t reserved39[3];
	uint8_t resetLatch;
	uint8_t reserved3d[3];
	uint32_t sendBuffer;		/* where the data for the host is read from */
	uint32_t receiveBuffer;		/* where the host's data is written to */
	uint8_t sendSlot;
	uint8_t receiveSlot;
	uint8_t reserved4a[0x1c];
	uint16_t sendInfo;
	uint16_t lengthWindow;		/* the length of the packet in flight */
	uint8_t dmaState;
	uint8_t dmaFlags;
	uint32_t dmaControl;
} __attribute__((packed));

_Static_assert(offsetof(struct SdioFunctionRegs, sendPorts) == 0x04, "sdio send ports");
_Static_assert(offsetof(struct SdioFunctionRegs, sendLength) == 0x08, "sdio send lengths");
_Static_assert(offsetof(struct SdioFunctionRegs, hostFlags) == 0x28, "sdio host flags");
_Static_assert(offsetof(struct SdioFunctionRegs, hostIrq) == 0x30, "sdio host irq");
_Static_assert(offsetof(struct SdioFunctionRegs, irqMask) == 0x34, "sdio irq mask");
_Static_assert(offsetof(struct SdioFunctionRegs, irqStatus) == 0x38, "sdio irq status");
_Static_assert(offsetof(struct SdioFunctionRegs, resetLatch) == 0x3c, "sdio reset latch");
_Static_assert(offsetof(struct SdioFunctionRegs, sendBuffer) == 0x40, "sdio send buffer");
_Static_assert(offsetof(struct SdioFunctionRegs, receiveBuffer) == 0x44, "sdio receive buffer");
_Static_assert(offsetof(struct SdioFunctionRegs, sendSlot) == 0x48, "sdio send slot");
_Static_assert(offsetof(struct SdioFunctionRegs, sendInfo) == 0x66, "sdio send info");
_Static_assert(offsetof(struct SdioFunctionRegs, lengthWindow) == 0x68, "sdio length window");
_Static_assert(offsetof(struct SdioFunctionRegs, dmaState) == 0x6a, "sdio dma state");
_Static_assert(offsetof(struct SdioFunctionRegs, dmaControl) == 0x6c, "sdio dma control");

/* The block-size part of a function's registers (0x80000160 + 0x100 * function). */
struct SdioFunctionBlocks {
	uint16_t window;		/* 0xfedc when the function is reset */
	uint8_t blocks;
	uint8_t shift;
	uint8_t reserved4;
};

/* The wake line the host configures: a GPIO pulsed to wake the host. */
struct SdioWakeLine {
	uint16_t pin;			/* 0xffff when there is none */
	uint16_t polarity;		/* 1: low pulse, else high pulse */
	uint16_t width;			/* microseconds */
};

/* The argument of the ioctls SDIODEV_CMD_SET_WAKE_LINE and SDIODEV_CMD_GET_WAKE_LINE. */
struct SdioWakeLineArgument {
	uint16_t reserved;
	struct SdioWakeLine line;
};

/*
 * How the chip is brought out of sleep for the SDIO functions and what it
 * wakes with; the argument of SDIODEV_CMD_SET_PARAMETERS and _GET_PARAMETERS.
 */
struct SdioSleepConfig {
	uint16_t queue;			/* 0xffff when none: the delay for the idle request */
	uint16_t timer;			/* 0xffff when none: the delay for the active request */
	uint8_t channel;		/* event channel: 4, 16 or 18 */
	uint8_t param;
	uint16_t config;		/* 1: channel and param are used */
};

/* The commands of the function drivers' ioctl callback. */
#define SDIODEV_CMD_SET_SEND_DONE	4	/* the release of a sent packet (function 1, 2) */
#define SDIODEV_CMD_SEND_BUSY		8
#define SDIODEV_CMD_QUIESCE		9
#define SDIODEV_CMD_ENABLE_IRQ		11
#define SDIODEV_CMD_RESET_WINDOW	13
#define SDIODEV_CMD_RESET_WINDOW2	14
#define SDIODEV_CMD_FLUSH		19
#define SDIODEV_CMD_PREPARE_RECEIVE	20
#define SDIODEV_CMD_HOST_ENABLED	21
#define SDIODEV_CMD_SET_WAKE_LINE	23
#define SDIODEV_CMD_GET_WAKE_LINE	24
#define SDIODEV_CMD_QUIESCE_ANY		25
#define SDIODEV_CMD_SET_AUX_HOOK	26
#define SDIODEV_CMD_TX_POOL_BUSY	27
#define SDIODEV_CMD_SET_PARAMETERS	28
#define SDIODEV_CMD_GET_PARAMETERS	29
#define SDIODEV_CMD_SET_IDLE		30
#define SDIODEV_CMD_SET_RECEIVE_HOOK	32
#define SDIODEV_CMD_FREE_RECEIVE	0x41
#define SDIODEV_CMD_REPOST_RECEIVE	0x42
#define SDIODEV_CMD_NOTIFY_HOST		0x45
#define SDIODEV_CMD_SEND_PENDING	0x46
#define SDIODEV_CMD_HOLD_DATA		0x48
#define SDIODEV_CMD_CLEAR_PENDING	0x49
#define SDIODEV_CMD_GET_PENDING		0x4a
#define SDIODEV_CMD_BLOCK_SIZE		0x4c
#define SDIODEV_CMD_SET_CHANNEL		0x60
#define SDIODEV_CMD_HOLD_CHANNEL	0x61
#define SDIODEV_CMD_DROP_QUEUED		0x62

/* Packet types of the host interface (the second half-word of a packet's header). */
#define SDIODEV_TYPE_DATA		0
#define SDIODEV_TYPE_EVENT		1
#define SDIODEV_TYPE_COMMAND		3

/* What the host is told (the cause written to SdioFunctionRegs.hostIrq). */
#define SDIODEV_CAUSE_RECEIVE		0x01	/* a port is ready for the host to write */
#define SDIODEV_CAUSE_SEND		0x02	/* data is ready for the host to read */

/* The receive hook of a function: what the mdev gives each packet the host wrote to. */
typedef void (*SdioReceiveHook)(struct MdevDriver *driver, OsaBuffer *buffer, uint32_t unused,
				uint32_t type);

extern uint8_t btHciWlanAsleep;
extern uint8_t sdioReceiveSlots[SDIO_FUNCTIONS];	/* 0x04000c9c */
/* The number of buffers the WLAN function offers the host to write to. */
#define wlanTxBufferCount		(sdioReceiveSlots[SDIO_FUNCTION_WLAN])

extern struct OsaBufferPool wlanHostBufferPool;		/* 0x04000e10 */
extern struct OsaBufferPool wlanTxFramePool;		/* 0x04000e38 */
extern struct OsaMsgList wlanHostEventQueue;

void sdioDevicesInit(void);

/* Not converted yet; the names are provisional. Addresses are those of the 4.00 image. */
void uapSqramAdjustEntry(unsigned int pin, unsigned int mask);	/* SQRAM 0xc0009b38, via the veneer at 0x53064 */

#endif /* ROBIN_SDIODEV_H */
