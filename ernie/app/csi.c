/*
 * The SPI link to the host: serial channel 0 of SAU0 as CSI00. A transfer receives and sends
 * at once, one byte for each byte; the side with fewer bytes receives into nothing or sends
 * zeros. It runs by interrupt (csiTransfer: INTCSI00 takes each byte, and sets bit 0x0002 of
 * event flag 1 at the end), by polling (csiTransferWait), or by DMA (channel 0 receives,
 * channel 1 sends). Only one transfer runs at a time.
 */
#pragma rtos_interrupt INTCSI00 csiInterrupt
#pragma sfr
#pragma DI
#pragma EI
#pragma NOP

#include "chip.h"
#include "board.h"
#include "csi.h"
#if BOARD_USS1001
#include "dtc.h"
#else
#include "dma.h"
#endif
#include "kernel.h"

/* SDR00's low byte, as DMA's SFR address */
#define CSI_SFR_SDR00		0x10

#if BOARD_USS1001
/* SDR00's address, for the DTC */
#define CSI_DTC_SDR00		0xff10

/* USS-1001's DTC transfer: the bytes received, then the bytes sent after the first */
static struct DtcControl csiDtc[2];
#else
static struct DmaRequest csiDmaReceive;
static struct DmaRequest csiDmaSend;
#endif

struct Csi csi;

/* opens the channel, the least significant bit first */
int csiOpenLsb(void)
{
	SAU0EN = 1;
	csi.state = 0;
	csi.sendCount = 0;
	csi.receiveCount = 0;
	SPS0 &= 0xfff0;
	ST0L.0 = 1;
	SMR00 = 0x0020;
	SCR00 = 0xf087;
	SDR00 = 0x0200;
	SO0 = 0x0e0e;
	SOE0L.0 = 1;
	P1.2 = 1;
	PM1.2 = 0;
	PM1.1 = 1;
	P1.0 = 1;
	PM1.0 = 0;
	P90 = 1;
	PM90 = 0;
	SS0L.0 = 1;
	CSIMK00 = 1;
	CSIIF00 = 0;
	csi.state = CSI_OPEN;
	return 0;
}

/* starts a transfer that INTCSI00 runs; 1 if one is running */
int csiTransfer(unsigned char *receive, unsigned int receiveLength,
		unsigned char *send, unsigned int sendLength)
{
	DI();
	if (csi.state & CSI_BUSY) {
		EI();
		return 1;
	}
	csi.state |= CSI_BUSY;
	EI();
	CSIIF00 = 0;
	CSIMK00 = 0;
	csi.receiveCount = receiveLength;
	csi.sendCount = sendLength;
	csi.receive = receive;
	csi.send = send;
	if (sendLength != 0) {
		SIO00 = *csi.send;
		csi.send++;
		csi.sendCount--;
	} else
		SIO00 = 0;
	return 0;
}

/* a transfer polled byte by byte; 1 if one is running */
int csiTransferWait(unsigned char *receive, unsigned int receiveLength,
		    unsigned char *send, unsigned int sendLength)
{
	unsigned int count;
	unsigned char data;

	DI();
	if (csi.state & CSI_BUSY) {
		EI();
		return 1;
	}
	csi.state |= CSI_BUSY;
	EI();
	count = receiveLength < sendLength ? sendLength : receiveLength;
	while (count--) {
		if (sendLength != 0) {
			sendLength--;
			SIO00 = *send++;
		} else
			SIO00 = 0;
		do {
			NOP();
		} while (SSR00 & 0x0040);
		if (receiveLength != 0) {
			receiveLength--;
			*receive++ = SIO00;
		} else
			data = SIO00;
	}
	CSIIF00 = 0;
	csi.state &= ~CSI_BUSY;
	return 0;
}

#if BOARD_USS1001
/* USS-1001's: a transfer by the DTC (channel 0, started by CSI00), the first byte sent here; the
   caller waits for the last byte. 1 if it cannot start */
int csiTransferDmaWait(unsigned char *receive, unsigned char *send, unsigned int length)
{
	unsigned char result;

	DI();
	if (csi.state & CSI_BUSY) {
		EI();
		return 1;
	}
	csi.state |= CSI_BUSY;
	EI();
	csi.receiveCount = length;
	csi.sendCount = length;
	csi.receive = receive;
	csi.send = send;
	if (length < 0x101 && length != 0) {
		csiDtc[0].mode = 0x18;
		csiDtc[0].blockSize = 1;
		csiDtc[0].count = (unsigned char)length - 1;
		csiDtc[0].reload = 0;
		csiDtc[0].source = CSI_DTC_SDR00;
		csiDtc[0].destination = (unsigned int)receive;
		csiDtc[1].mode = 4;
		csiDtc[1].blockSize = 1;
		csiDtc[1].count = (unsigned char)length - 1;
		csiDtc[1].reload = 0;
		csiDtc[1].source = (unsigned int)send + 1;
		csiDtc[1].destination = CSI_DTC_SDR00;
		result = dtcStart(0, csiDtc);
		if (result == 0) {
			CSIMK00 = 1;
			CSIIF00 = 0;
			SIO00 = *send;
			return 0;
		}
	}
	csi.state &= ~CSI_BUSY;
	return 1;
}
#else
#if !BOARD_DOL1001	/* not in DOL-1001's */
/* a transfer by DMA, with channel 0's interrupt at the end; 1 if it cannot start */
int csiTransferDma(unsigned char *receive, unsigned char *send, unsigned int length)
{
	DI();
	if (csi.state & CSI_BUSY) {
		EI();
		return 1;
	}
	csi.state |= CSI_BUSY;
	EI();
	csi.receiveCount = length;
	csi.sendCount = length;
	csi.receive = receive;
	csi.send = send;
	if (length < 0x400 && length != 0) {
		csiDmaReceive.count = length;
		csiDmaReceive.address = receive;
		csiDmaReceive.sfr = CSI_SFR_SDR00;
		csiDmaReceive.trigger = DMA_TRIGGER_CSI00;
		if (dma0Setup(&csiDmaReceive) == 0) {
			csiDmaSend.count = length;
			csiDmaSend.address = send;
			csiDmaSend.sfr = CSI_SFR_SDR00;
			csiDmaSend.trigger = DMA_TRIGGER_CSI00;
			if (dma1Setup(&csiDmaSend) == 0) {
				dma0Start();
				dma1Start();
				dma0EnableInterrupt();
				return 0;
			}
		}
	}
	csi.state &= ~CSI_BUSY;
	return 1;
}
#endif

/* a transfer by DMA without its interrupt (the caller polls DST0); 1 if it cannot start */
int csiTransferDmaWait(unsigned char *receive, unsigned char *send, unsigned int length)
{
	DI();
	if (csi.state & CSI_BUSY) {
		EI();
		return 1;
	}
	csi.state |= CSI_BUSY;
	EI();
	csi.receiveCount = length;
	csi.sendCount = length;
	csi.receive = receive;
	csi.send = send;
	if (length < 0x400 && length != 0) {
		csiDmaReceive.count = length;
		csiDmaReceive.address = receive;
		csiDmaReceive.sfr = CSI_SFR_SDR00;
		csiDmaReceive.trigger = DMA_TRIGGER_CSI00;
		if (dma0Setup(&csiDmaReceive) == 0) {
			csiDmaSend.count = length;
			csiDmaSend.address = send;
			csiDmaSend.sfr = CSI_SFR_SDR00;
			csiDmaSend.trigger = DMA_TRIGGER_CSI00;
			if (dma1Setup(&csiDmaSend) == 0) {
				dma0DisableInterrupt();
				dma0Start();
				dma1Start();
				return 0;
			}
		}
	}
	csi.state &= ~CSI_BUSY;
	return 1;
}

#endif

/* ends the running transfer; 1 if none runs */
int csiCancel(void)
{
	DI();
	if (csi.state & CSI_BUSY) {
		csi.state = CSI_OPEN;
		csi.receiveCount = 0;
		csi.receiveCount = 0;
		EI();
		return 0;
	}
	EI();
	return 1;
}

void csiStopChannel(void)
{
	ST0L.0 = 1;
}

/* stops the channel and lets its pins go low */
void csiClose(void)
{
	ST0L.0 = 1;
	CSIMK00 = 1;
	CSIIF00 = 0;
	csi.state = 0;
	SOE0L.0 = 0;
	P1.0 = 0;
	P1.1 = 0;
	P1.2 = 0;
	P90 = 0;
}

/* opens the channel, the most significant bit first, at a quarter of csiOpenLsb's clock */
void csiOpenMsb(void)
{
	SAU0EN = 1;
	csi.state = 0;
	csi.sendCount = 0;
	csi.receiveCount = 0;
#if (BOARD_DOL1001 && !BOARD_DOL1002) || !FW_SINCE(0x00080302)
	SPS0L = 0xf0 & SPS0L;
#else
	SPS0L &= 0xf0;
#endif
	SMR00 = 0x0020;
	SCR00 = 0xf007;
	SDR00 = 0x0600;
	SO0 = 0x0e0e;
	SOE0L.0 = 1;
#if BOARD_USS1001
	/* USS-1001's: each line's mode after it, the device's power on P1.5 */
	P1.0 = 1;
	PM1.0 = 0;
	P1.1 = 1;
	PM1.1 = 1;
	P1.2 = 1;
	PM1.2 = 0;
	P1.5 = 0;
	PM1.5 = 0;
#elif BOARD_DOL1001
	/* DOL-1001's: each line's mode after it, then P5.7 as the fat boards' */
	P1.0 = 1;
	PM1.0 = 0;
	P1.1 = 1;
	PM1.1 = 1;
	P1.2 = 1;
	PM1.2 = 0;
	P5.7 = 0;
	PM5.7 = 0;
#else
	P1.0 = 1;
	P1.1 = 1;
	P1.2 = 1;
	PM1.0 = 0;
	PM1.1 = 1;
	PM1.2 = 0;
	P5.7 = 0;
	PM5.7 = 0;
#endif
	P1.6 = 0;
	PM1.6 = 1;
	P90 = 1;
	PM90 = 0;
	SS0L.0 = 1;
	CSIIF00 = 0;
	CSIMK00 = 0;
	csi.state = CSI_OPEN;
}

unsigned int csiGetState(void)
{
	return csi.state;
}

/* marks the running transfer done (after a DMA transfer) */
void csiRelease(void)
{
	DI();
	csi.state &= ~CSI_BUSY;
	EI();
}

/* a byte went both ways: store the one received, send the next */
void csiInterrupt(void)
{
	unsigned char data;

	if (SSR00 & 0x0001) {
		SIR00 = 0x0001;
		data = 0;
	} else
		data = SIO00;
	if (csi.receiveCount != 0) {
		*csi.receive++ = data;
		csi.receiveCount--;
	}
	if (csi.sendCount != 0) {
		SIO00 = *csi.send;
		csi.send++;
		csi.sendCount--;
	} else if (csi.receiveCount != 0)
		SIO00 = 0;
	if ((csi.receiveCount | csi.sendCount) == 0 && !(SSR00 & 0x0040)) {
		csi.state &= ~CSI_BUSY;
		CSIMK00 = 1;
		CSIIF00 = 0;
		iset_flg(1, 0x0002);
	}
}
