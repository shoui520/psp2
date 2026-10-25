/*
 * The DMA controller's two channels: channel 0 moves bytes from an SFR to RAM, channel 1 from
 * RAM to an SFR, one each time the trigger's interrupt comes. INTDMA0 ends both channels'
 * transfer and tells the task that waits for it.
 */
#pragma rtos_interrupt INTDMA0 dma0Interrupt
#pragma sfr
#pragma NOP

#include "csi.h"
#include "dma.h"
#include "kernel.h"

extern void sub_2C342(ID flgid, FLGPTN setptn);

/* sets channel 0 up for a transfer from request->sfr; 1 if one is running */
unsigned char dma0Setup(struct DmaRequest *request)
{
	if (DST0)
		return 1;
	DEN0 = 1;
	DSA0 = request->sfr;
	DRA0 = (unsigned int)request->address;
	DBC0 = request->count & 0x03ff;
	DMC0 = request->trigger & 0x0f;
	DMAPR00 = 1;
	DMAPR10 = 1;
	return 0;
}

void dma0Start(void)
{
	DST0 = 1;
}

void dma0EnableInterrupt(void)
{
	DMAIF0 = 0;
	DMAMK0 = 0;
}

void dma0DisableInterrupt(void)
{
	DMAMK0 = 1;
	DMAIF0 = 0;
}

void dma0Stop(void)
{
	DST0 = 0;
	NOP();
	NOP();
	DEN0 = 0;
	DMAMK0 = 1;
	DMAIF0 = 0;
}

/* turns channel 0 off at the end of its transfer */
void dma0End(void)
{
	DEN0 = 0;
	DMAMK0 = 1;
	DMAIF0 = 0;
}

/* sets channel 1 up for a transfer to request->sfr; 1 if one is running */
unsigned char dma1Setup(struct DmaRequest *request)
{
	if (DST1)
		return 1;
	DEN1 = 1;
	DSA1 = request->sfr;
	DRA1 = (unsigned int)request->address;
	DBC1 = request->count & 0x03ff;
	DMC1 = request->trigger & 0x0f | 0x40;
	DMAPR01 = 1;
	DMAPR11 = 1;
	DMAMK1 = 1;
	DMAIF1 = 0;
	return 0;
}

/* starts channel 1 and moves its first byte */
void dma1Start(void)
{
	DST1 = 1;
	STG1 = 1;
}

void dma1Stop(void)
{
	DST1 = 0;
	NOP();
	NOP();
	DEN1 = 0;
}

void dma1End(void)
{
	DEN1 = 0;
}

/* channel 0's transfer ended: both channels are, so tell the trigger's user */
void dma0Interrupt(void)
{
	dma0End();
	dma1End();
	if ((0x0f & DMC0) == DMA_TRIGGER_SR0)
		sub_2C342(2, 0x0002);
	else if ((0x0f & DMC0) == DMA_TRIGGER_CSI10)
		sub_2C342(2, 0x0001);
	else if ((0x0f & DMC0) == DMA_TRIGGER_CSI00) {
		csi.state &= ~CSI_BUSY;
		sub_2C342(1, 0x0002);
	}
}
