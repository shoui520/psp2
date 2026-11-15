/*
 * The card's three SDIO functions as mdev drivers (0x245d4..0x262c0): function
 * 0 ("sdio") carries the WLAN commands, events and data, function 1
 * ("bt_sdio") the Bluetooth HCI and function 2 ("amp_sdio") the AMP HCI. Each
 * function has a register block that tells the firmware what the host did
 * (wrote a packet to a port, read one, reset the function) and receives what
 * the firmware offers it (buffers for the host to write to, packets to read).
 *
 * The WLAN function runs in multi-port mode: 15 ports for the host's data,
 * port 0 for its commands, and up to 16 send slots. The Bluetooth and AMP
 * functions have one port. The ROM's mdev framework calls the five callbacks
 * of each driver: init (sdioFunctionInit), write (sdioFunctionWrite, a packet
 * from the firmware for the host), ioctl (sdioFunctionIoctl) and the interrupt
 * service (sdioFunctionInterrupt).
 *
 * Marvell published no source; every name is inferred from use.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/btpool.h"
#include "robin/amp.h"
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/eventpower.h"
#include "robin/hostcmd.h"
#include "robin/hostps.h"
#include "robin/mactx.h"
#include "robin/phy.h"
#include "robin/psctl.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/sdiodev.h"
#include "robin/sleep.h"
#include "robin/soc.h"
#include "robin/sys.h"
#include "robin/txring.h"
#include "robin/wlan.h"

int ampPalFrameDataOffset(void);			/* 0x236ea */
extern struct OsaBufferPool ampTransmitPool;		/* 0x54794 */

/* SOC_REG(0x64): bits 28..27 ask the clock logic for the idle or the active clock. */
#define SDIO_CLOCK_REQUEST		SOC_REG(0x64)
#define SDIO_CLOCK_REQUEST_MASK		0x18000000
#define SDIO_CLOCK_REQUEST_IDLE		0x10000000
#define SDIO_CLOCK_REQUEST_ACTIVE	0x08000000

#define SDIO_WAKE_GPIO			PERIPH_REG(0x20c)

/* Bits of SdioFunctionRegs.irqStatus: what the host did. */
#define SDIO_STATUS_WRITTEN		0x01	/* it wrote a port */
#define SDIO_STATUS_READ		0x02	/* it read the sent data */
#define SDIO_STATUS_RESET		0x10	/* it reset or enabled the function */

#define SDIO_HOST_BLOCKED		0x04	/* SdioFunctionRegs.hostFlags */
#define SDIO_RECEIVE_FREE_PORTS(regs)	((regs)->receivePorts)

/* Largest packets the Bluetooth and AMP functions take from the host. */
#define SDIO_BT_PACKET_MAX		1029
#define SDIO_AMP_PACKET_MAX		1500

/* The offset of the data in a buffer the firmware sends to the host (the header is in front). */
#define SDIO_SEND_DATA_OFFSET		28
#define SDIO_RECEIVE_DATA_OFFSET	176
#define SDIO_PACKET_DATA_OFFSET		32

#define SDIO_HOST_POOL_BLOCK		0x7d0
#define SDIO_AMP_POOL_BLOCK		0x5d4

/* The function drivers' state; the initial values are the image's. */
uint8_t btHciWlanAsleep __attribute__((section(".dtcm.rw")));	/* 0x04000c90 */
static uint8_t sdioClockRequest __attribute__((section(".dtcm.rw")));	/* an active (0) or idle (1) request is out */
static uint8_t sdioWakePending __attribute__((section(".dtcm.rw")));	/* the wake handler ran, the next interrupt releases */
static uint8_t sdioResetWindow[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = { 1, 1, 1 };
/* Written by the ROM before the drivers start (the image holds zeros). */
static uint8_t sdioSinglePort[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));
static uint8_t sdioChannel[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = { 0xff, 0xff, 0xff };
uint8_t sdioReceiveSlots[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* 0x04000c9c */
static uint8_t sdioSendSlots[SDIO_FUNCTIONS + 2] __attribute__((section(".dtcm.rw")));
static uint32_t sdioWakeCount __attribute__((section(".dtcm.rw")));	/* host accesses blocked while the host slept */
static uint32_t sdioSendPendingFlag __attribute__((section(".dtcm.rw")));	/* something was raised to the host and not collected */
static struct {
	void (*sendDone)(OsaBuffer *buffer);	/* what releases a sent packet */
	void *aux;
} sdioHooks[2] __attribute__((section(".dtcm.rw")));	/* functions 1 and 2 */
static uint32_t sdioReceiveReclaim __attribute__((section(".dtcm.rw")));	/* the next receive slot to hand up */
static uint32_t sdioSendFill __attribute__((section(".dtcm.rw")));	/* the next send slot to fill */
static uint32_t sdioReceiveFill __attribute__((section(".dtcm.rw")));	/* the next receive slot to allocate */
static uint32_t sdioSendReclaim __attribute__((section(".dtcm.rw")));	/* the next send slot the host emptied */
static uint32_t sdioSendStalls __attribute__((section(".dtcm.rw")));	/* port 0 was occupied when a packet came */
static uint16_t sdioSendPending[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* ports staged, not yet offered */
static uint16_t sdioReceiveReported[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* ports reported to the host */
static uint16_t sdioReceiveFree[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* ports the host left free, at the last check */
static uint16_t sdioReceivePosted[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* ports offered since the last report */
static uint16_t sdioReceiveMask[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw")));	/* ports that have a buffer */
static struct SdioWakeLine sdioWakeLine __attribute__((section(".dtcm.rw")));
static struct SdioSleepConfig sdioSleepConfig __attribute__((section(".dtcm.rw"))) = {
	.queue = 0xffff,
	.timer = 0xffff,
};
static const char *sdioFunctionNames[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = {
	"sdio", "bt_sdio", "amp_sdio",
};
static volatile struct SdioFunctionRegs *sdioRegs[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = {
	(volatile struct SdioFunctionRegs *)0x80000100,
	(volatile struct SdioFunctionRegs *)0x80000200,
	(volatile struct SdioFunctionRegs *)0x80000300,
};
static volatile struct SdioFunctionBlocks *sdioBlocks[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = {
	(volatile struct SdioFunctionBlocks *)0x80000160,
	(volatile struct SdioFunctionBlocks *)0x80000260,
	(volatile struct SdioFunctionBlocks *)0x80000360,
};
static uint32_t sdioIdle[SDIO_FUNCTIONS] __attribute__((section(".dtcm.rw"))) = { 0, 0, 1 };

static uint32_t sdioHostActive[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));	/* 0x0400443c */
static uint32_t sdioHciPending[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));	/* an HCI exchange awaits its reply */
static uint32_t sdioChannelHeld[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static struct OsaMsgList sdioDataQueue[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static struct OsaMsgList sdioEventQueue[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static struct OsaMsgList sdioResponseQueue[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static OsaBuffer *sdioReceiveBuffers[SDIO_FUNCTIONS][SDIO_PORTS] __attribute__((section(".dtcm.bss")));
static OsaBuffer *sdioSendBuffers[SDIO_FUNCTIONS][SDIO_PORTS] __attribute__((section(".dtcm.bss")));
static uint32_t sdioDataHold[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static SdioReceiveHook sdioReceiveHooks[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));
static struct MdevDriver sdioDrivers[SDIO_FUNCTIONS] __attribute__((section(".dtcm.bss")));

static __attribute__((noinline, noclone)) void sdioSetFunctionBusy(unsigned int fn, int idle);
static __attribute__((noinline, noclone)) void sdioStageSendBuffer(unsigned int fn, int type);
static __attribute__((noinline, noclone)) void sdioPostSendPorts(unsigned int fn, uint16_t ports);
static __attribute__((noinline, noclone)) void sdioWakeLinePulse(void);
static __attribute__((noinline, noclone)) void sdioRaiseHostIrq(unsigned int fn, uint32_t cause);
static __attribute__((noinline, noclone)) void sdioAllocReceive(unsigned int fn);
static __attribute__((noinline, noclone)) int sdioReleaseCommand(OsaBuffer *buffer);
static __attribute__((noinline, noclone)) int sdioReleaseData(OsaBuffer *buffer);

/* The data address of the buffer the host's next packet is written to. */
static inline __attribute__((always_inline)) void sdioSetReceiveBuffer(volatile struct SdioFunctionRegs *regs, OsaBuffer *buffer, unsigned int offset)
{
	__aeabi_uwrite4((int)(uintptr_t)buffer + (int)offset, (void *)&regs->receiveBuffer);
}

/* 0x245d4: gives the host the buffers it writes to; the function starts with one port, or all of them. */
static __attribute__((noinline, noclone)) void sdioPostReceiveBuffers(struct MdevDriver *dev)
{
	unsigned int fn = dev->index;
	volatile struct SdioFunctionRegs *regs;
	OsaBuffer *buffer;
	unsigned int slot;

	if (sdioSinglePort[fn] != 0) {
		if (fn == SDIO_FUNCTION_WLAN) {
			buffer = wlanTxBufferAlloc(&wlanBlockPool28, wlanMgmtFrameSize, 0, 0);
			sdioReceiveBuffers[dev->index][0] = buffer;
			if (buffer != NULL) {
				buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET;
				regs = sdioRegs[dev->index];
				regs->receiveSlot = 0;
				sdioSetReceiveBuffer(regs, sdioReceiveBuffers[dev->index][0],
						     sdioReceiveBuffers[dev->index][0]->dataOffset);
				sdioReceiveBuffers[dev->index][0]->packetClass = 8;
				regs->receivePorts = 1;
				sdioReceiveMask[dev->index] = 1;
				sdioReceiveSlots[dev->index] = 1;
				sdioSendSlots[dev->index] = 1;
			}
		}
		fn = dev->index;
		if (fn == SDIO_FUNCTION_BT) {
			buffer = osa_BufferAlloc(&btPacketPool, 0, 0);
			sdioReceiveBuffers[dev->index][0] = buffer;
		} else if (fn == SDIO_FUNCTION_AMP) {
			buffer = osa_BufferAlloc(&ampTransmitPool, SDIO_AMP_POOL_BLOCK, 0);
			sdioReceiveBuffers[dev->index][0] = buffer;
			if (buffer != NULL) {
				int offset = ampPalFrameDataOffset();

				sdioReceiveBuffers[dev->index][0]->dataOffset = offset;
				regs = sdioRegs[dev->index];
				sdioSetReceiveBuffer(regs, sdioReceiveBuffers[dev->index][0], offset);
				sdioReceiveBuffers[dev->index][0]->packetClass = 8;
				regs->receivePorts = 1;
				sdioReceiveMask[dev->index] = 1;
				sdioReceiveSlots[dev->index] = 1;
				sdioSendSlots[dev->index] = 1;
			}
			return;
		} else {
			return;
		}
		if (buffer != NULL) {
			buffer->dataOffset = SDIO_PACKET_DATA_OFFSET;
			regs = sdioRegs[dev->index];
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[dev->index][0],
					     SDIO_PACKET_DATA_OFFSET);
			sdioReceiveBuffers[dev->index][0]->packetClass = 8;
			regs->receivePorts = 1;
			sdioReceiveMask[dev->index] = 1;
			sdioReceiveSlots[dev->index] = 1;
			sdioSendSlots[dev->index] = 1;
		}
		return;
	}
	sdioReceiveSlots[fn] = 1;
	sdioSendSlots[dev->index] = SDIO_PORTS;
	for (slot = 1; slot < SDIO_PORTS; slot++) {
		buffer = wlanTxBufferAlloc(&wlanBlockPool28, wlanMgmtFrameSize, 0, 0);
		sdioReceiveBuffers[dev->index][slot] = buffer;
		if (buffer != NULL) {
			buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET;
			regs = sdioRegs[dev->index];
			regs->receiveSlot = slot;
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[dev->index][slot],
					     sdioReceiveBuffers[dev->index][slot]->dataOffset);
			sdioReceiveBuffers[dev->index][slot]->packetClass = 8;
			sdioReceivePosted[dev->index] |= 1 << slot;
			regs->receivePorts = 1 << slot;
			sdioReceiveMask[dev->index] |= 1 << sdioReceiveSlots[dev->index]++;
		}
	}
}

/* 0x247d4: pulses the line that wakes the host. */
static __attribute__((noinline, noclone)) void sdioWakeLinePulse(void)
{
	uint32_t bit, high, low;
	int cycles;

	if (sdioWakeLine.pin == 0xffff)
		return;
	bit = 1u << (sdioWakeLine.pin & 0xff);
	if (sdioWakeLine.polarity == 1) {
		low = SDIO_WAKE_GPIO & ~bit;
		high = bit | low;
	} else {
		high = SDIO_WAKE_GPIO & ~bit;
		low = bit | high;
		/* the first value written is the asserted level */
		bit = high;
		high = low;
		low = bit;
	}
	cycles = (int)(romMul32x32(sdioWakeLine.width, sysClockFrequency) >> 10);
	SDIO_WAKE_GPIO = low;
	if (cycles != 0)
		robinDelayLoop(cycles);
	SDIO_WAKE_GPIO = high;
}

/* 0x24838: tells the host (the cause is a SDIODEV_CAUSE_ bit set) and pulses the wake line. */
static __attribute__((noinline, noclone)) void sdioRaiseHostIrq(unsigned int fn, uint32_t cause)
{
	if (sdioSinglePort[fn] != 0 || (cause & SDIODEV_CAUSE_RECEIVE) == 0 ||
	    sdioReceiveReported[fn] != 1 || (cause &= ~SDIODEV_CAUSE_RECEIVE) != 0) {
		if (fn == SDIO_FUNCTION_WLAN)
			sdioSendPendingFlag = 1;
		sdioRegs[fn]->hostIrq = cause;
		sdioWakeLinePulse();
	}
}

/* 0x24870: the init callback; resets the function's registers and offers the host its buffers. */
static __attribute__((noinline, noclone)) int sdioFunctionInit(struct MdevDriver *dev)
{
	volatile struct SdioFunctionRegs *regs = sdioRegs[dev->index];
	volatile struct SdioFunctionBlocks *blocks;
	OsaBuffer *buffer;
	uint32_t cause;

	regs->receivePorts = 0;
	regs->sendPorts = 0;
	regs->irqStatus = 1;
	regs->irqStatus = 0;
	if (sdioSinglePort[dev->index] == 0) {
		uint8_t *control = (uint8_t *)&sdioRegs[dev->index]->dmaControl;

		__aeabi_uwrite4(__aeabi_uread4(control) | 0xc00, control);
		sdioRegs[dev->index]->dmaFlags |= 0x10;
	}
	sdioReceiveReclaim = 1;
	sdioSendFill = 1;
	sdioReceiveFill = 1;
	sdioSendReclaim = 1;
	if (sdioSinglePort[dev->index] == 0) {
		sdioReceiveSlots[dev->index] = 1;
		sdioSendSlots[dev->index] = 1;
		buffer = osa_BufferAlloc(&wlanHostBufferPool, SDIO_HOST_POOL_BLOCK, 0);
		sdioReceiveBuffers[dev->index][0] = buffer;
		if (buffer != NULL) {
			buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET;
			regs = sdioRegs[dev->index];
			regs->receiveSlot = 0;
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[dev->index][0],
					     sdioReceiveBuffers[dev->index][0]->dataOffset);
			sdioReceiveBuffers[dev->index][0]->packetClass = 8;
			sdioReceivePosted[dev->index] = 1;
			regs = sdioRegs[dev->index];
			regs->receivePorts = 1;
			sdioReceiveMask[dev->index] |= 1;
		}
	}
	sdioPostReceiveBuffers(dev);
	sdioRegs[dev->index]->irqMask = 7;
	if (dev->index == SDIO_FUNCTION_BT)
		sdioRegs[SDIO_FUNCTION_BT]->irqMask = 23;
	sdioRegs[dev->index]->resetLatch = 0;
	if (dev->index == SDIO_FUNCTION_WLAN) {
		sdioWakeLine.pin = 0xffff;
		sdioWakeLine.polarity = 0xffff;
		sdioWakeLine.width = 0xffff;
	}
	sdioRegs[dev->index]->dmaState = 0;
	blocks = sdioBlocks[dev->index];
	blocks->shift = 5;
	blocks->blocks = 0;
	blocks->reserved4 = 0;
	regs = sdioRegs[dev->index];
	if (regs->receivePorts != 0) {
		sdioReceiveReported[dev->index] = sdioReceivePosted[dev->index];
		sdioReceivePosted[dev->index] = 0;
		cause = 13;
	} else {
		cause = 12;
	}
	sdioRaiseHostIrq(dev->index, cause);
	return 0;
}

/* 0x24a32: offers the host the ports that hold data. */
static __attribute__((noinline, noclone)) void sdioPostSendPorts(unsigned int fn, uint16_t ports)
{
	if (fn != SDIO_FUNCTION_WLAN || btHciWlanAsleep == 0) {
		sdioRegs[fn]->sendPorts = ports;
		sdioRaiseHostIrq(fn, SDIODEV_CAUSE_SEND);
	} else {
		sdioRegs[SDIO_FUNCTION_WLAN]->sendPorts = ports;
		sdioRaiseHostIrq(SDIO_FUNCTION_WLAN, 3);
		btHciWlanAsleep = 0;
	}
}

/* 0x24a70: takes the next packet of the type's queue and puts it where the host reads it. */
static __attribute__((noinline, noclone)) void sdioStageSendBuffer(unsigned int fn, int type)
{
	unsigned int slot = 0;
	uint8_t *packet = NULL;
	volatile struct SdioFunctionRegs *regs;
	struct OsaMsgList *queue;
	OsaBuffer *buffer;

	if (fn == SDIO_FUNCTION_WLAN) {
		if (type != 3 && type != 131) {
			if (type == 5) {
				slot = (uint8_t)sdioSendFill;
				sdioSendBuffers[fn][slot] = osa_MsgQueueRecv(&sdioDataQueue[fn]);
				goto staged;
			}
			if (type != 2 && type != 130)
				goto staged;
			queue = &sdioResponseQueue[fn];
		} else {
			queue = &sdioEventQueue[fn];
		}
		sdioSendBuffers[fn][0] = osa_MsgQueueRecv(queue);
staged:
		buffer = sdioSendBuffers[fn][slot];
		packet = (uint8_t *)buffer + buffer->dataOffset;
		sdioRegs[fn]->sendLength[slot] = *(uint16_t *)packet;
		if (type == 2)
			sdioRegs[fn]->sendInfo = 8 * *(uint16_t *)(packet + 4) + 3;
	} else if (fn == SDIO_FUNCTION_BT || fn == SDIO_FUNCTION_AMP) {
		if (type == 3 || type == 5 || type == 2 || type == 8)
			sdioSendBuffers[fn][0] = osa_MsgQueueRecv(&sdioEventQueue[fn]);
		buffer = sdioSendBuffers[fn][0];
		if (buffer == NULL)
			return;
		if (type == 3) {
			if (fn == SDIO_FUNCTION_BT)
				buffer->dataOffset = SDIO_PACKET_DATA_OFFSET;
			btLmpLcSetupContext(fn == SDIO_FUNCTION_AMP,
					    (uint8_t *)buffer + buffer->dataOffset + 4);
		}
		packet = (uint8_t *)sdioSendBuffers[fn][0] + sdioSendBuffers[fn][0]->dataOffset;
		sdioRegs[fn]->lengthWindow = *(uint16_t *)packet;
		sdioBlocks[fn]->blocks = (*(uint16_t *)packet + (1 << sdioBlocks[fn]->shift) - 1) >>
					 sdioBlocks[fn]->shift;
		sdioRegs[fn]->sendInfo = packet[3];
	} else {
		return;
	}
	regs = sdioRegs[fn];
	if (sdioSinglePort[fn] == 0) {
		uint32_t ports;

		regs->sendSlot = slot;
		regs->sendLength[slot] = *(uint16_t *)packet;
		sdioSendPending[fn] |= 1 << slot;
		ports = (uint8_t)sdioSendSlots[fn];
		if (ports > 1 && slot != 0 && ports == ++sdioSendFill)
			sdioSendFill = 1;
	}
	__aeabi_uwrite4((int)(uintptr_t)packet, (void *)&regs->sendBuffer);
	if (sdioSinglePort[fn] != 0 || regs->sendPorts == 0) {
		if (type == 131 || type == 130) {
			sdioPostSendPorts(fn, 1);
			sdioSendPending[fn] = 2 * (sdioSendPending[fn] >> 1);
		} else if (fn != SDIO_FUNCTION_WLAN || wlanPsHostBlocked(5) != 0) {
			sdioPostSendPorts(fn, sdioSendPending[fn]);
			sdioSendPending[fn] = 0;
		} else if (sdioSendPending[SDIO_FUNCTION_WLAN] & 1) {
			sdioPostSendPorts(SDIO_FUNCTION_WLAN, 1);
			sdioSendPending[SDIO_FUNCTION_WLAN] = 2 * (sdioSendPending[SDIO_FUNCTION_WLAN] >> 1);
		}
	}
}

/* 0x24ca2: the function is idle (the other functions allowing) or in use; asks for the clock to match. */
static __attribute__((noinline, noclone)) void sdioSetFunctionBusy(unsigned int fn, int idle)
{
	uint32_t active = SOC_EVENT_STATE;
	uint32_t request, source;
	unsigned int i;

	if (sdioIdle[fn] == (uint32_t)idle)
		return;
	sdioIdle[fn] = idle;
	if (idle != 0) {
		for (i = 0; i < SDIO_FUNCTIONS; i++)
			if (sdioIdle[i] != (uint32_t)idle && ((1 << (i + 1)) & active) != 0)
				return;
	}
	if (sdioClockRequest == idle)
		return;
	sdioClockRequest = idle;
	request = SDIO_CLOCK_REQUEST & ~SDIO_CLOCK_REQUEST_MASK;
	if (idle != 0) {
		if (sdioSleepConfig.timer == 0xffff)
			return;
		sysDelayUs(sdioSleepConfig.timer);
		SDIO_CLOCK_REQUEST = request | SDIO_CLOCK_REQUEST_IDLE;
		switch (sdioSleepConfig.channel) {
		case 4:
			source = 12;
			break;
		case 16:
			source = 11;
			break;
		case 18:
			source = 10;
			break;
		default:
			return;
		}
		sysEventSourceReset(source);
		return;
	}
	if (sdioSleepConfig.config == 0 && sdioSleepConfig.queue != 0xffff) {
		sysDelayUs(sdioSleepConfig.queue);
		SDIO_CLOCK_REQUEST = request | SDIO_CLOCK_REQUEST_ACTIVE;
	}
}

/* 0x24d40: the write callback; queues a packet for the host and stages it when its port is free. */
static __attribute__((noinline, noclone)) int sdioFunctionWrite(struct MdevDriver *dev, OsaBuffer *buffer, uint16_t length, uint32_t type)
{
	uint8_t *packet;
	UINT posture;
	int result;

	if (buffer == NULL)
		return 0;
	if (dev->index == SDIO_FUNCTION_WLAN) {
		uint8_t *data = (uint8_t *)buffer;

		if (type == 2) {
			unsigned int command;

			packet = data + buffer->dataOffset;
			buffer->dataOffset += 8;
			length = (packet[11] << 8) | packet[10];
			data += buffer->dataOffset;
			data[2] = 3;
			data[3] = 0;
			*(uint16_t *)data = length + 8;
			posture = sysIrqDisable();
			command = (packet[13] << 8) | packet[12];
			if (command == 0xa)
				osa_MsgQueueSendFront(&sdioResponseQueue[dev->index], buffer, 0, 20);
			else
				osa_MsgQueueSend(&sdioResponseQueue[dev->index], buffer);
			if (sdioSendBuffers[dev->index][0] == NULL && wlanPsHciDeferred(command) != 0 &&
			    wlanPsHostIsActive() == 0) {
				sdioSetFunctionBusy(dev->index, 0);
				if (sdioClockRequest == 0)
					sdioStageSendBuffer(dev->index, 2);
			}
			sysIrqRestore(posture);
			return 0;
		}
		if (type == 3) {
			buffer->dataOffset = SDIO_SEND_DATA_OFFSET;
			length = ((data[35] << 8) | data[34]) + 4;
			data[SDIO_SEND_DATA_OFFSET] = data[34] + 4;
			data[29] = length >> 8;
			data[30] = SDIODEV_TYPE_EVENT;
			data[31] = 0;
			posture = sysIrqDisable();
			osa_MsgQueueSend(&sdioEventQueue[dev->index], buffer);
			if (sdioSendBuffers[dev->index][0] != NULL) {
				sysIrqRestore(posture);
				return 1;
			}
			sdioStageSendBuffer(dev->index, 3);
			sysIrqRestore(posture);
			return 0;
		}
		if (type != 5)
			return 0;
		buffer->dataOffset = SDIO_PACKET_DATA_OFFSET;
		if (((data[35] << 8) | data[34]) == 0) {
			wlanReleasePacket(buffer);
			return 1;
		}
		buffer->dataOffset = SDIO_SEND_DATA_OFFSET;
		length = ((data[35] << 8) | data[34]) + ((data[37] << 8) | data[36]) + 4;
		data[SDIO_SEND_DATA_OFFSET] = data[34] + data[36] + 4;
		data[29] = length >> 8;
		data[30] = SDIODEV_TYPE_DATA;
		data[31] = 0;
		posture = sysIrqDisable();
		osa_MsgQueueSend(&sdioDataQueue[dev->index], buffer);
		if (sdioSendBuffers[dev->index][sdioSendFill] == NULL && wlanPsHostBlocked(5) != 0 &&
		    sdioDataHold[dev->index] == 0)
			sdioStageSendBuffer(dev->index, 5);
		sysIrqRestore(posture);
		return 0;
	}
	if (dev->index != SDIO_FUNCTION_BT && dev->index != SDIO_FUNCTION_AMP)
		return 0;
	buffer->transportTag = type;
	result = 1;
	if (dev->index == SDIO_FUNCTION_AMP) {
		buffer->dataOffset -= 4;
	} else {
		buffer->dataOffset = SDIO_PACKET_DATA_OFFSET;
		sdioSetFunctionBusy(dev->index, 0);
	}
	packet = (uint8_t *)buffer + buffer->dataOffset;
	*(uint16_t *)packet = length + 4;
	packet[2] = 0;
	posture = sysIrqDisable();
	if (sdioHciPending[dev->index] != 0 && (type != 3 || !btLmpLcConvertContext(0, packet + 4)))
		btLmpLcFormatContext(0, 0, 11, 0);
	switch (type) {
	case 2:
	case 8:
		packet[3] = type == 2 ? 4 : 3;
		osa_MsgQueueSend(&sdioEventQueue[dev->index], buffer);
		if (sdioSendBuffers[dev->index][0] == NULL && sdioHciPending[dev->index] == 0 &&
		    sdioClockRequest == 0) {
			sdioStageSendBuffer(dev->index, 2);
			result = 0;
		}
		break;
	case 3:
		packet[3] = 0xfe;
		if (!btLmpLcConvertContext(0, packet + 4)) {
			osa_MsgQueueSend(&sdioEventQueue[dev->index], buffer);
		} else {
			osa_MsgQueueSendFront(&sdioEventQueue[dev->index], buffer, 0, 20);
			sdioHciPending[dev->index] = 0;
			if (dev->index != SDIO_FUNCTION_AMP)
				sdioSetFunctionBusy(dev->index, 0);
		}
		if (sdioSendBuffers[dev->index][0] == NULL && sdioHciPending[dev->index] == 0 &&
		    sdioClockRequest == 0) {
			sdioStageSendBuffer(dev->index, 3);
			result = 0;
		}
		break;
	case 5:
		packet[3] = 2;
		osa_MsgQueueSend(&sdioEventQueue[dev->index], buffer);
		if (sdioSendBuffers[dev->index][0] == NULL && sdioHciPending[dev->index] == 0 &&
		    sdioClockRequest == 0) {
			sdioStageSendBuffer(dev->index, 5);
			result = 0;
		}
		break;
	}
	sysIrqRestore(posture);
	return result;
}

/* 0x25070: the host is not to be waited for any more. */
static __attribute__((noinline, noclone)) void sdioFunctionQuiesce(unsigned int fn)
{
	UINT posture = sysIrqDisable();

	sysWaitForEventTransactions();
	sdioRegs[fn]->irqStatus = 0xef;
	if (fn != SDIO_FUNCTION_WLAN)
		sdioRegs[fn]->irqMask = 23;
	sdioHostActive[fn] = 0;
	sysIrqRestore(posture);
}

/* 0x250a8: frees every buffer of the function, in its slots and queues, and restarts the rings. */
static __attribute__((noinline, noclone)) void sdioFunctionFlush(unsigned int fn)
{
	unsigned int slot;
	OsaBuffer *buffer;

	sdioRegs[fn]->irqMask = 0;
	sdioRegs[fn]->irqStatus = 1;
	sdioRegs[fn]->irqStatus = 0;
	for (slot = 0; sdioReceiveSlots[fn] > slot; slot++) {
		if (sdioReceiveBuffers[fn][slot] != NULL) {
			osa_BufferFree(sdioReceiveBuffers[fn][slot]);
			sdioReceiveBuffers[fn][slot] = NULL;
		}
	}
	for (slot = 0; sdioSendSlots[fn] > slot; slot++) {
		if (sdioSendBuffers[fn][slot] != NULL) {
			wlanReleasePacket(sdioSendBuffers[fn][slot]);
			sdioSendBuffers[fn][slot] = NULL;
		}
	}
	while (osa_MsgQueuePeek(&sdioEventQueue[fn]) != NULL) {
		buffer = osa_MsgQueueRecv(&sdioEventQueue[fn]);
		sdioSendBuffers[fn][0] = buffer;
		wlanReleasePacket(buffer);
	}
	while (osa_MsgQueuePeek(&sdioDataQueue[fn]) != NULL) {
		buffer = osa_MsgQueueRecv(&sdioDataQueue[fn]);
		sdioSendBuffers[fn][0] = buffer;
		wlanReleasePacket(buffer);
	}
	while (osa_MsgQueuePeek(&sdioResponseQueue[fn]) != NULL) {
		buffer = osa_MsgQueueRecv(&sdioResponseQueue[fn]);
		sdioSendBuffers[fn][0] = buffer;
		wlanReleasePacket(buffer);
	}
	sdioSendBuffers[fn][0] = NULL;
	sdioReceiveReclaim = 1;
	sdioSendFill = 1;
	sdioSendReclaim = 1;
	sdioReceiveFill = 1;
}

/* 0x25192: the event source handler the idle clock request is released by. */
static __attribute__((noinline, noclone)) void sdioWakeHandler(void)
{
	unsigned int i;

	SDIO_CLOCK_REQUEST = (SDIO_CLOCK_REQUEST & ~SDIO_CLOCK_REQUEST_MASK) | SDIO_CLOCK_REQUEST_ACTIVE;
	sdioClockRequest = 0;
	gpioPinWrite(sdioSleepConfig.param, 0);
	sdioWakePending = 1;
	for (i = 0; i < SDIO_FUNCTIONS; i++) {
		if (i == SDIO_FUNCTION_WLAN) {
			if (osa_MsgQueuePeek(&sdioResponseQueue[0]) != NULL ||
			    osa_MsgQueuePeek(&wlanHostEventQueue) != NULL) {
				sdioSetFunctionBusy(0, 0);
				wlanPsHostSleepEnd();
				wlanHostEventFlush();
				sdioStageSendBuffer(0, 2);
			}
		} else if (i == SDIO_FUNCTION_BT) {
			if (osa_MsgQueuePeek(&sdioEventQueue[1]) != NULL) {
				sdioSetFunctionBusy(1, 0);
				if (sdioHciPending[1] == 0)
					sdioStageSendBuffer(1, 3);
			}
		}
	}
}

/* 0x2523c: offers the host its receive buffers: port 0 and the ones the host freed (WLAN), or the only one. */
static __attribute__((noinline, noclone)) void sdioAllocReceive(unsigned int fn)
{
	volatile struct SdioFunctionRegs *regs;
	unsigned int slot, bit;
	OsaBuffer *buffer;

	if (fn == SDIO_FUNCTION_WLAN) {
		regs = sdioRegs[0];
		sdioReceiveFree[0] = regs->receivePorts;
		sdioReceivePosted[0] = 0;
		if (sdioReceiveFree[0] == 0xffff || sdioReceiveFree[0] == 0xfffe)
			return;
		for (slot = (uint16_t)sdioReceiveFill;; slot = (uint16_t)sdioReceiveFill) {
			if (sdioReceiveBuffers[0][slot] != NULL)
				break;
			bit = 1 << slot;
			sdioReceiveFree[0] |= bit;
			buffer = wlanTxBufferAlloc(&wlanBlockPool28, wlanMgmtFrameSize, 0, 0);
			sdioReceiveBuffers[0][slot] = buffer;
			if (buffer == NULL)
				break;
			buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET;
			regs = sdioRegs[0];
			regs->receiveSlot = slot;
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[0][slot],
					     sdioReceiveBuffers[0][slot]->dataOffset);
			sdioReceivePosted[0] |= bit;
			regs->receivePorts = bit;
			sdioReceiveBuffers[0][slot]->packetClass = 8;
			if (sdioReceiveSlots[0] == ++sdioReceiveFill)
				sdioReceiveFill = 1;
		}
	} else if (fn == SDIO_FUNCTION_BT) {
		buffer = osa_BufferAlloc(&btPacketPool, 0, 0);
		sdioReceiveBuffers[1][0] = buffer;
		if (buffer != NULL) {
			buffer->dataOffset = SDIO_PACKET_DATA_OFFSET;
			regs = sdioRegs[1];
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[1][0], SDIO_PACKET_DATA_OFFSET);
			regs->receivePorts = 1;
			sdioReceiveBuffers[1][0]->packetClass = 8;
		}
	} else if (fn == SDIO_FUNCTION_AMP) {
		buffer = osa_BufferAlloc(&ampTransmitPool, SDIO_AMP_POOL_BLOCK, 0);
		sdioReceiveBuffers[2][0] = buffer;
		if (buffer != NULL) {
			int offset = ampPalFrameDataOffset();

			sdioReceiveBuffers[2][0]->dataOffset = offset;
			regs = sdioRegs[2];
			sdioSetReceiveBuffer(regs, sdioReceiveBuffers[2][0], offset);
			regs->receivePorts = 1;
			sdioReceiveBuffers[2][0]->packetClass = 8;
		}
	}
}

/* 0x25388: the WLAN function's port 0 gets a buffer when the host has emptied it. */
static __attribute__((noinline, noclone)) void sdioBuildReceive(unsigned int fn)
{
	volatile struct SdioFunctionRegs *regs = sdioRegs[fn];
	OsaBuffer **slot;
	OsaBuffer *buffer;

	sdioReceiveFree[fn] = regs->receivePorts & sdioReceiveMask[fn];
	if ((sdioReceiveFree[fn] & 1) == 0) {
		slot = &sdioReceiveBuffers[fn][0];
		if (*slot == NULL) {
			buffer = osa_BufferAlloc(&wlanHostBufferPool, SDIO_HOST_POOL_BLOCK, 0);
			*slot = buffer;
			if (buffer != NULL) {
				buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET;
				regs = sdioRegs[fn];
				regs->receiveSlot = 0;
				sdioSetReceiveBuffer(regs, *slot, (*slot)->dataOffset);
				regs->receivePorts = 1;
				(*slot)->packetClass = 8;
			}
		}
	}
}

/* 0x253f6: the ioctl callback. */
static __attribute__((noinline, noclone)) int sdioFunctionIoctl(struct MdevDriver *dev, int command, void *argument)
{
	UINT posture;
	unsigned int fn, i;
	uint32_t ports;

	switch (command) {
	case SDIODEV_CMD_SET_PARAMETERS: {
		struct SdioSleepConfig *config = argument;
		uint32_t source = 0;
		int reset;

		posture = sysIrqDisable();
		if (config->config == 1) {
			sdioSleepConfig.config = 1;
			sdioSleepConfig.channel = config->channel;
			sdioSleepConfig.param = config->param;
			switch (sdioSleepConfig.channel) {
			case 4:
				source = 12;
				break;
			case 16:
				source = 11;
				break;
			case 18:
				source = 10;
				break;
			default:
				sysIrqRestore(posture);
				return 0;
			}
			if (sysSetEventSourceHandler(source, sdioWakeHandler) == 1) {
				reset = sysChannelReset(config->param) != 0;
				if (reset) {
					sdioSleepConfig.timer = config->timer;
					sdioSleepConfig.queue = 0;
				}
				sysIrqRestore(posture);
				return reset ? 0 : 1;
			}
		} else {
			sdioSleepConfig.config = config->config;
			sdioSleepConfig.queue = config->queue;
			sdioSleepConfig.timer = config->timer;
		}
		sysIrqRestore(posture);
		return 0;
	}
	case SDIODEV_CMD_SET_SEND_DONE:
		posture = sysIrqDisable();
		if (dev->index == SDIO_FUNCTION_BT)
			sdioHooks[0].sendDone = (void (*)(OsaBuffer *))argument;
		else if (dev->index == SDIO_FUNCTION_AMP)
			sdioHooks[1].sendDone = (void (*)(OsaBuffer *))argument;
		sysIrqRestore(posture);
		return 0;
	case SDIODEV_CMD_SEND_BUSY:
		fn = dev->index;
		if (fn != SDIO_FUNCTION_WLAN) {
			if (fn == SDIO_FUNCTION_BT || fn == SDIO_FUNCTION_AMP)
				return sdioDataQueue[fn].words[2] + sdioEventQueue[fn].words[2] +
				       sdioResponseQueue[fn].words[2];
			return 0;
		}
		if (osa_PoolQuery(&wlanBlockPool28, 4) != 0)
			return 1;
		return osa_PoolQuery(&wlanBlockPool28, 2) != 0;
	case SDIODEV_CMD_QUIESCE:
		fn = dev->index;
		if (fn == SDIO_FUNCTION_WLAN) {
			if (sleepStats.mode != 1)
				return 0;
		} else if (fn == SDIO_FUNCTION_BT) {
			if (sdioHostActive[1] == 0)
				return 0;
		} else if (fn == SDIO_FUNCTION_AMP) {
			if (sdioHostActive[2] == 0)
				return 0;
		} else {
			return 0;
		}
		sdioFunctionQuiesce(fn);
		return 0;
	case SDIODEV_CMD_ENABLE_IRQ:
		if (dev->index == SDIO_FUNCTION_BT)
			sdioRegs[1]->irqMask = 23;
		else if (dev->index == SDIO_FUNCTION_AMP)
			sdioRegs[2]->irqMask = 23;
		return 0;
	case SDIODEV_CMD_RESET_WINDOW:
	case SDIODEV_CMD_RESET_WINDOW2:
		fn = dev->index;
		if (fn != 0) {
			if (fn == 1 || fn == 2) {
				if (sdioResetWindow[fn] != 0) {
					sdioRegs[fn]->lengthWindow = 0xfedc;
					sdioBlocks[fn]->window = 0xfedc;
					sdioResetWindow[fn] = 0;
				}
			}
		} else if (sdioResetWindow[0] != 0) {
			sdioRegs[0]->lengthWindow = 0xfedc;
			sdioBlocks[0]->window = 0xfedc;
			sdioResetWindow[0] = 0;
		}
		fn = dev->index;
		if (((uint16_t)sdioRegs[fn]->receivePorts & sdioReceiveMask[fn]) != 0)
			sdioRaiseHostIrq(fn, 1);
		return 0;
	case SDIODEV_CMD_FLUSH:
		sdioFunctionFlush(dev->index);
		return 0;
	case SDIODEV_CMD_PREPARE_RECEIVE:
		posture = sysIrqDisable();
		fn = dev->index;
		if (sdioSinglePort[fn] != 0) {
			if (sdioReceiveBuffers[fn][0] == NULL) {
				sdioAllocReceive(fn);
				fn = dev->index;
				if (fn == 1 || fn == 2 || (btHciWlanAsleep | fn) == 0)
					sdioRaiseHostIrq(fn, 1);
			}
			sysIrqRestore(posture);
			return 0;
		}
		ports = (uint16_t)(sdioRegs[fn]->receivePorts & sdioReceiveMask[fn]);
		if (sdioReceiveSlots[fn] <= 1) {
			if ((ports & 1) == 0)
				sdioBuildReceive(fn);
		} else if (ports & 1) {
			if (ports >> 1 != 0) {
				sysIrqRestore(posture);
				return 1;
			}
			goto allocate;
		} else {
			sdioBuildReceive(fn);
		}
		if (ports >> 1 != 0) {
			sysIrqRestore(posture);
			return 0;
		}
allocate:
		sdioAllocReceive(dev->index);
		fn = dev->index;
		ports = sdioReceivePosted[fn];
		if (ports >> 1 == 0) {
			sysIrqRestore(posture);
			return 1;
		}
		if (btHciWlanAsleep == 0 && sdioReceivePosted[fn] != 0) {
			sdioReceiveReported[fn] = ports;
			sdioReceivePosted[dev->index] = 0;
			sdioRaiseHostIrq(dev->index, 1);
		}
		sysIrqRestore(posture);
		return 0;
	case SDIODEV_CMD_HOST_ENABLED:
		return sdioRegs[dev->index]->hostState & 2;
	case SDIODEV_CMD_SET_WAKE_LINE: {
		struct SdioWakeLineArgument *wake = argument;

		if (sdioWakeLine.pin > 0xf && sdioWakeLine.pin != 0xffff)
			return 1;
		sdioWakeLine.pin = wake->line.pin;
		sdioWakeLine.polarity = wake->line.polarity;
		sdioWakeLine.width = wake->line.width;
		if (wake->line.pin != 0xffff) {
			uapSqramAdjustEntry((uint8_t)wake->line.pin, 0xffff);
			gpioPinWrite((sdioWakeLine.pin), (sdioWakeLine.polarity));
			gpioPinSelect((uint8_t)sdioWakeLine.pin, 1);
		}
		return 0;
	}
	case SDIODEV_CMD_GET_WAKE_LINE: {
		struct SdioWakeLineArgument *wake = argument;

		wake->line.pin = sdioWakeLine.pin;
		wake->line.polarity = sdioWakeLine.polarity;
		wake->line.width = sdioWakeLine.width;
		return 0;
	}
	case SDIODEV_CMD_QUIESCE_ANY:
		fn = dev->index;
		if (fn <= 2)
			sdioFunctionQuiesce(fn);
		return 0;
	case SDIODEV_CMD_SET_AUX_HOOK:
		posture = sysIrqDisable();
		if (dev->index == SDIO_FUNCTION_BT)
			sdioHooks[0].aux = argument;
		else if (dev->index == SDIO_FUNCTION_AMP)
			sdioHooks[1].aux = argument;
		sysIrqRestore(posture);
		return 0;
	case SDIODEV_CMD_TX_POOL_BUSY:
		if (dev->index != SDIO_FUNCTION_WLAN)
			return 0;
		return osa_PoolQuery(&wlanTxFramePool, 2);
	case SDIODEV_CMD_GET_PARAMETERS: {
		struct SdioSleepConfig *config = argument;

		config->queue = sdioSleepConfig.queue;
		config->timer = sdioSleepConfig.timer;
		config->config = sdioSleepConfig.config;
		if (sdioSleepConfig.config == 1) {
			config->channel = sdioSleepConfig.channel;
			config->param = sdioSleepConfig.param;
		}
		return 0;
	}
	case SDIODEV_CMD_SET_IDLE:
		sdioSetFunctionBusy(dev->index, 1);
		return 0;
	case SDIODEV_CMD_SET_RECEIVE_HOOK:
		sdioReceiveHooks[dev->index] = (SdioReceiveHook)argument;
		return 0;
	case SDIODEV_CMD_FREE_RECEIVE:
		fn = dev->index;
		if (sdioSinglePort[fn] == 0) {
			for (i = 1; sdioReceiveSlots[dev->index] > i; i++) {
				OsaBuffer *buffer = sdioReceiveBuffers[dev->index][i];

				if (buffer != NULL) {
					osa_BufferFree(buffer);
					sdioReceiveBuffers[dev->index][i] = NULL;
				}
			}
			return 0;
		}
		sdioReceiveMask[fn] = 1;
		sdioReceiveReclaim = 1;
		sdioReceiveFill = 1;
		goto repost;
	case SDIODEV_CMD_REPOST_RECEIVE:
		sdioReceiveMask[dev->index] = 1;
		sdioReceiveReclaim = 1;
		sdioReceiveFill = 1;
repost:
		sdioPostReceiveBuffers(dev);
		sdioRaiseHostIrq(dev->index, 1);
		return 0;
	case SDIODEV_CMD_NOTIFY_HOST:
		sdioRegs[dev->index]->hostIrq = 1;
		btHciWlanAsleep = 0;
		return 0;
	case SDIODEV_CMD_SEND_PENDING:
		fn = dev->index;
		if (fn == SDIO_FUNCTION_WLAN) {
			int pending = osa_MsgQueuePeek(&sdioDataQueue[0]) != NULL ||
				      osa_PoolQuery(&wlanTxFramePool, 2) != 0;

			for (i = 1; sdioSendSlots[fn] > i; i++)
				pending = pending ? 1 : sdioSendBuffers[fn][i] != NULL;
			return pending;
		}
		if (fn == SDIO_FUNCTION_BT || fn == SDIO_FUNCTION_AMP)
			return sdioSendBuffers[fn][0] != NULL;
		goto holdData;
	case SDIODEV_CMD_HOLD_DATA:
holdData:
		sdioDataHold[dev->index] = argument != NULL;
		posture = sysIrqDisable();
		fn = dev->index;
		if (sdioDataHold[fn] == 0 && sdioSendBuffers[fn][sdioSendFill] == NULL &&
		    osa_MsgQueuePeek(&sdioDataQueue[fn]) != NULL)
			sdioStageSendBuffer(dev->index, 5);
		sysIrqRestore(posture);
		return 0;
	case SDIODEV_CMD_CLEAR_PENDING:
		if (dev->index == SDIO_FUNCTION_WLAN)
			sdioSendPendingFlag = 0;
		return 0;
	case SDIODEV_CMD_GET_PENDING:
		if (dev->index != SDIO_FUNCTION_WLAN)
			return 0;
		return sdioSendPendingFlag;
	case SDIODEV_CMD_BLOCK_SIZE:
		if (dev->index != SDIO_FUNCTION_WLAN)
			return 0;
		ports = (*(volatile uint8_t *)0x80000029 << 8 & 0x1ff) | *(volatile uint8_t *)0x80000028;
		if (ports == 0)
			return 512;
		return ports;
	case SDIODEV_CMD_SET_CHANNEL:
		posture = sysIrqDisable();
		sdioChannel[dev->index] = (uint8_t)(uintptr_t)argument;
		for (i = 0; dev->index == i || (uint8_t)(uintptr_t)argument != sdioChannel[i]; ) {
			if (++i >= SDIO_FUNCTIONS) {
				sysIrqRestore(posture);
				gpioPinWrite(sdioChannel[dev->index], 1);
				return 0;
			}
		}
		sysIrqRestore(posture);
		return 0;
	case SDIODEV_CMD_HOLD_CHANNEL: {
		uint32_t active = SOC_EVENT_STATE;
		int release = 0;
		int noMatch = 0;

		posture = sysIrqDisable();
		fn = dev->index;
		for (i = 0; fn == i || ((1 << (i + 1)) & active) == 0 || sdioChannel[fn] != sdioChannel[i]; ) {
			if (++i >= SDIO_FUNCTIONS) {
				noMatch = 1;
				break;
			}
		}
		if (!noMatch) {
			if (argument != NULL) {
				if (fn == SDIO_FUNCTION_WLAN) {
					if (sdioChannelHeld[0] == 0 && wlanHostSleepArmed())
						release = 1;
				} else if (sdioChannelHeld[fn] == 0 && sdioHciPending[1] != 0) {
					release = 1;
				}
			} else if (sdioChannelHeld[fn] != 0 && sdioChannelHeld[i] == 0) {
				release = 1;
			}
		}
		if (noMatch || release) {
			if (!noMatch && argument != NULL)
				sdioRegs[dev->index]->dmaState = 1;
			gpioPinWrite(sdioChannel[dev->index], argument == NULL);
		}
		sdioChannelHeld[dev->index] = (uint32_t)(uintptr_t)argument;
		sysIrqRestore(posture);
		return 0;
	}
	case SDIODEV_CMD_DROP_QUEUED: {
		OsaBuffer *buffer;
		uint32_t state, slot;

		if (dev->index != SDIO_FUNCTION_WLAN)
			return 0;
		posture = sysIrqDisable();
		while ((buffer = osa_MsgQueueRecv(&sdioDataQueue[dev->index])) != NULL)
			wlanReleasePacket(buffer);
		for (;;) {
			fn = dev->index;
			state = sdioSendPending[fn];
			if (state >> 1 == 0)
				break;
			if (--sdioSendFill != 0) {
				slot = sdioSendFill;
			} else {
				slot = (uint8_t)sdioSendSlots[fn] - 1;
				sdioSendFill = slot;
			}
			if ((state & (1 << slot)) != 0) {
				buffer = sdioSendBuffers[fn][slot];
				if (buffer != NULL) {
					wlanReleasePacket(buffer);
					sdioSendBuffers[dev->index][sdioSendFill] = NULL;
				}
			}
			sdioSendPending[dev->index] &= ~(1 << sdioSendFill);
		}
		sysIrqRestore(posture);
		return 0;
	}
	}
	return 0;
}

/* 0x25b96: the release of a command buffer the host wrote; gives the caller the next step, a new receive. */
static __attribute__((noinline, noclone)) void sdioNotifyReceive(void)
{
	uint32_t argument = 1;

	mdevIoctl(wlanSdioDev, 20, (uintptr_t)&argument);
}

static __attribute__((noinline, noclone)) int sdioReleaseCommand(OsaBuffer *buffer)
{
	if (sdioSinglePort[0] != 0)
		wlanTxQuotaRelease(buffer);
	return (int)(uintptr_t)sdioNotifyReceive;
}

static __attribute__((noinline, noclone)) void sdioNotifyReceiveMore(void)
{
	uint32_t argument = 4;

	mdevIoctl(wlanSdioDev, 20, (uintptr_t)&argument);
}

static __attribute__((noinline, noclone)) int sdioReleaseData(OsaBuffer *buffer)
{
	wlanTxQuotaRelease(buffer);
	return (int)(uintptr_t)sdioNotifyReceiveMore;
}

/* 0x25bc8: the interrupt callback; collects what the host did to the three functions. */
static __attribute__((noinline, noclone)) int sdioFunctionInterrupt(struct MdevDriver *dev)
{
	unsigned int i;
	uint32_t status, flags;

	if (sdioWakePending != 0) {
		gpioPinWrite(sdioSleepConfig.param, 1);
		sdioWakePending = 0;
	}
	for (i = 0; i < SDIO_FUNCTIONS; i++) {
		volatile struct SdioFunctionRegs *regs = sdioRegs[i];
		int staged = 0;

		status = regs->irqStatus;
		regs->irqStatus = ~status | SDIO_STATUS_RESET;
		flags = regs->hostFlags;
		if (i == SDIO_FUNCTION_WLAN) {
			uint32_t sent, ready, saved;
			unsigned int k, next;

			if (status & SDIO_STATUS_READ) {
				int released = 0;

				sent = (uint16_t)regs->sendPorts;
				regs->dmaState = 0;
				if ((sent & 1) == 0 && (sdioSendPending[0] & 1) == 0 && sdioSendBuffers[0][0] != NULL) {
					sdioSendBuffers[0][0]->dataOffset = SDIO_PACKET_DATA_OFFSET;
					wlanPsHciTraffic(sdioSendBuffers[0][0]);
					wlanReleasePacket(sdioSendBuffers[0][0]);
					sdioSendBuffers[0][0] = NULL;
				}
				for (k = 1; sdioSendSlots[0] > k; k++) {
					if (((1 << sdioSendReclaim) & sent) == 0 &&
					    (sdioSendPending[0] & (1 << sdioSendReclaim)) == 0) {
						next = (uint8_t)sdioSendReclaim;
						if (sdioSendBuffers[0][next] != NULL) {
							released = 1;
							wlanReleasePacket(sdioSendBuffers[0][next]);
							sdioSendBuffers[0][next] = NULL;
							if (sdioSendSlots[0] == ++sdioSendReclaim)
								sdioSendReclaim = 1;
						}
					}
				}
				if (released)
					macTxPostEvent(4);
				if (sdioSendPending[0] != 0 && sent == 0) {
					if (wlanPsHostBlocked(5) != 0) {
						sdioPostSendPorts(0, sdioSendPending[0]);
						sdioSendPending[0] = 0;
					} else if (sdioSendPending[0] & 1) {
						sdioPostSendPorts(0, 1);
						sdioSendPending[0] = 2 * (sdioSendPending[0] >> 1);
					} else {
						if (osa_MsgQueuePeek(&sdioEventQueue[0]) != NULL) {
							sdioStageSendBuffer(0, 131);
						} else if (osa_MsgQueuePeek(&sdioResponseQueue[0]) != NULL &&
							   wlanPsHostIsActive() == 0) {
							sdioStageSendBuffer(0, 130);
						}
					}
				} else {
					if (((sdioSendPending[0] | sent) & 1) == 0) {
						int type = 0;

						if (sdioSendBuffers[0][0] != NULL)
							sdioSendStalls++;
						if (osa_MsgQueuePeek(&sdioEventQueue[0]) != NULL)
							type = 3;
						else if (osa_MsgQueuePeek(&sdioResponseQueue[0]) != NULL &&
							 wlanPsHostIsActive() == 0)
							type = 2;
						if (type != 0)
							sdioStageSendBuffer(0, type);
					}
					if (sdioSendBuffers[0][sdioSendFill] == NULL && wlanPsHostBlocked(5) != 0 &&
					    sdioDataHold[dev->index] == 0 &&
					    osa_MsgQueuePeek(&sdioDataQueue[0]) != NULL)
						sdioStageSendBuffer(0, 5);
				}
				wlanPsHostReady();
			}
			if (status & SDIO_STATUS_WRITTEN) {
				if ((flags & SDIO_HOST_BLOCKED) != 0) {
					sdioWakeCount++;
				} else {
					regs->dmaState = 0;
					if (sysEventEngineIsActive() != 0)
						sysDispatchMiddleEventBits(0x100000);
					ready = (uint16_t)(regs->receivePorts & sdioReceiveMask[0]);
					saved = ready;
					wlanPsHostSleepEnd();
					if (sdioReceiveMask[0] != ready) {
						uint8_t slotNumber;

						if ((ready & 1) == 0) {
							OsaBuffer *buffer = sdioReceiveBuffers[0][0];

							if (buffer != NULL) {
								uint8_t *packet = (uint8_t *)buffer + buffer->dataOffset;

								ready |= 1;
								if (((packet[3] << 8) | packet[2]) == SDIODEV_TYPE_EVENT) {
									uint8_t *command = packet + 4;

									sdioReceiveBuffers[0][0]->packetClass = 16;
									buffer->flags |= 1;
									buffer->release = sdioReleaseCommand;
									buffer->dataOffset = SDIO_RECEIVE_DATA_OFFSET + 4;
									buffer->context = NULL;
									sdioReceiveBuffers[0][0] = NULL;
									if (hostCmdIsNodeSet(command) != 0)
										hostCmdNode(command + 8);
									sdioReceiveHooks[0](&sdioDrivers[0], buffer, 0, 1);
								} else {
									regs->receiveSlot = 0;
									sdioSetReceiveBuffer(regs, buffer, buffer->dataOffset);
									regs->receivePorts = 1;
								}
							}
						}
						slotNumber = 1;
						for (;; slotNumber = (uint8_t)(slotNumber + 1)) {
							unsigned int index;

							if (sdioReceiveSlots[0] <= slotNumber)
								break;
							if (((1 << sdioReceiveReclaim) & ready) != 0)
								break;
							index = (uint8_t)sdioReceiveReclaim;
							if (sdioReceiveBuffers[0][index] == NULL)
								break;
							ready = (uint16_t)((1 << sdioReceiveReclaim) | ready);
							if (wlanAnyConnectionFlags2Bit4() != 0) {
								OsaBuffer *buffer = sdioReceiveBuffers[0][index];

								buffer->packetClass = 2;
								buffer->flags |= 1;
								buffer->release = sdioReleaseData;
								buffer->dataOffset += 4;
								buffer->context = NULL;
								sdioReceiveBuffers[0][index] = NULL;
								if ((*((uint8_t *)buffer + buffer->dataOffset + 13) & 8) != 0) {
									btHciWlanAsleep = 1;
									wlanPsStageConfirm();
								}
								sdioReceiveHooks[0](&sdioDrivers[0], buffer, 0, 4);
							} else if (sdioReceiveBuffers[0][index] != NULL) {
								osa_BufferFree(sdioReceiveBuffers[0][index]);
								sdioReceiveBuffers[0][index] = NULL;
							}
							if (sdioReceiveSlots[0] == ++sdioReceiveReclaim)
								sdioReceiveReclaim = 1;
						}
						if (sdioReceiveSlots[0] == 1 || saved >> 1 == 0) {
							sdioAllocReceive(0);
							if (sdioReceivePosted[0] >> 1 != 0 && btHciWlanAsleep == 0) {
								sdioReceiveReported[0] = sdioReceivePosted[0];
								sdioReceivePosted[0] = 0;
								sdioRaiseHostIrq(0, 1);
							}
						}
					}
				}
			} else if ((flags & SDIO_HOST_BLOCKED) != 0) {
				sdioWakeCount++;
			}
			wlanPsStageAdvance();
		} else if (i == SDIO_FUNCTION_BT || i == SDIO_FUNCTION_AMP) {
			if ((status & SDIO_STATUS_WRITTEN) && (regs->hostFlags & SDIO_HOST_BLOCKED) == 0) {
				OsaBuffer *buffer = sdioReceiveBuffers[i][0];
				uint8_t *packet = (uint8_t *)buffer + buffer->dataOffset;
				uint32_t limit = i == SDIO_FUNCTION_AMP ? SDIO_AMP_PACKET_MAX : SDIO_BT_PACKET_MAX;

				regs->dmaState = 0;
				if (*(uint16_t *)packet <= limit) {
					buffer->packetClass = 2;
					btLmpLcFormatContext(i == SDIO_FUNCTION_AMP, 0, 7, 0);
					sdioAllocReceive(i);
					if (sdioReceiveBuffers[i][0] != NULL)
						sdioRaiseHostIrq(i, 1);
					buffer->dataOffset += 4;
					if (packet[3] == 254)
						packet[3] = 1;
					buffer->transportTag = packet[3];
					buffer->context = &sdioDrivers[i];
					sdioReceiveHooks[i](&sdioDrivers[i], buffer, 0, packet[3]);
				} else {
					sdioRaiseHostIrq(i, 1);
				}
				staged = 1;
			}
			if (status & SDIO_STATUS_READ) {
				OsaBuffer *sent = sdioSendBuffers[i][0];
				void (*done)(OsaBuffer *);
				struct OsaMsgList *queue;
				OsaBuffer *next;

				regs->dmaState = 0;
				if (sent != NULL) {
					int amp, started;

					done = i == SDIO_FUNCTION_AMP ? sdioHooks[1].sendDone : sdioHooks[0].sendDone;
					if (i == SDIO_FUNCTION_BT) {
						if (btLmpLcClearContext(0, (uint8_t *)sent + sent->dataOffset + 4))
							sdioHciPending[1] = 1;
						started = staged;
						amp = 0;
					} else {
						amp = 1;
						started = 0;
					}
					btLmpLcFormatContext(amp, 0, 6, started);
					sent = sdioSendBuffers[i][0];
					if (done != NULL)
						done(sent);
					else
						osa_BufferFree(sent);
					sdioSendBuffers[i][0] = NULL;
				}
				queue = &sdioEventQueue[i];
				next = osa_MsgQueuePeek(queue);
				if (next == NULL)
					next = osa_MsgQueuePeek(queue);
				if (next == NULL)
					next = osa_MsgQueuePeek(queue);
				if (next != NULL) {
					if (i == SDIO_FUNCTION_BT && sdioHciPending[1] != 0)
						btLmpLcFormatContext(0, 0, 11, 0);
					else
						sdioStageSendBuffer(i, (uint8_t)next->transportTag);
				}
			}
			if ((regs->irqMask & 0x10) != 0 && (status & SDIO_STATUS_RESET) != 0) {
				if (i != SDIO_FUNCTION_AMP)
					sdioSetFunctionBusy(i, 0);
				sdioHostActive[i] = 1;
				regs->irqMask = 7;
				btLmpLcFormatContext(i == SDIO_FUNCTION_AMP, 0, 9, 0);
			}
		}
	}
	return 0;
}

/* 0x26224: creates the queues of the three functions and registers them as mdev drivers. */
void sdioDevicesInit(void)
{
	unsigned int i;

	for (i = 0; i < SDIO_FUNCTIONS; i++) {
		struct MdevDriver *driver = &sdioDrivers[i];

		sdioReceiveHooks[i] = NULL;
		__aeabi_memclr4(&sdioDataQueue[i], sizeof(struct OsaMsgList));
		__aeabi_memclr4(&sdioEventQueue[i], sizeof(struct OsaMsgList));
		__aeabi_memclr4(&sdioResponseQueue[i], sizeof(struct OsaMsgList));
		driver->index = i;
		driver->name = sdioFunctionNames[i];
		driver->callbacks[1] = NULL;
		driver->callbacks[2] = NULL;
		driver->callbacks[3] = NULL;
		driver->callbacks[0] = sdioFunctionInit;
		driver->callbacks[4] = sdioFunctionWrite;
		driver->callbacks[5] = sdioFunctionIoctl;
		driver->callbacks[6] = sdioFunctionInterrupt;
		mdevRegisterDriver(driver, sdioFunctionInterrupt);
	}
}
