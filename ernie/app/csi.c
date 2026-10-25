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
#include "csi.h"
#include "dma.h"
#include "kernel.h"

/* SDR00's low byte, as DMA's SFR address */
#define CSI_SFR_SDR00		0x10
/* DMA's trigger: INTCSI00 */
#define CSI_DMA_TRIGGER		6

static struct DmaRequest csiDmaReceive;
static struct DmaRequest csiDmaSend;

static struct {
	unsigned char	*receive;	/* where the next byte received goes */
	unsigned char	*send;		/* the next byte to send */
	unsigned char	sendCount;	/* the bytes left to send */
	unsigned char	receiveCount;	/* the bytes left to receive */
	unsigned char	state;		/* CSI_* */
} csi;

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
		csiDmaReceive.trigger = CSI_DMA_TRIGGER;
		if (sub_16FFF(&csiDmaReceive) == 0) {
			csiDmaSend.count = length;
			csiDmaSend.address = send;
			csiDmaSend.sfr = CSI_SFR_SDR00;
			csiDmaSend.trigger = CSI_DMA_TRIGGER;
			if (sub_17076(&csiDmaSend) == 0) {
				sub_1704B();
				sub_170CA();
				sub_1704F();
				return 0;
			}
		}
	}
	csi.state &= ~CSI_BUSY;
	return 1;
}

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
		csiDmaReceive.trigger = CSI_DMA_TRIGGER;
		if (sub_16FFF(&csiDmaReceive) == 0) {
			csiDmaSend.count = length;
			csiDmaSend.address = send;
			csiDmaSend.sfr = CSI_SFR_SDR00;
			csiDmaSend.trigger = CSI_DMA_TRIGGER;
			if (sub_17076(&csiDmaSend) == 0) {
				sub_17056();
				sub_1704B();
				sub_170CA();
				return 0;
			}
		}
	}
	csi.state &= ~CSI_BUSY;
	return 1;
}

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
	SPS0L &= 0xf0;
	SMR00 = 0x0020;
	SCR00 = 0xf007;
	SDR00 = 0x0600;
	SO0 = 0x0e0e;
	SOE0L.0 = 1;
	P1.0 = 1;
	P1.1 = 1;
	P1.2 = 1;
	PM1.0 = 0;
	PM1.1 = 1;
	PM1.2 = 0;
	P5.7 = 0;
	PM5.7 = 0;
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
