/*
 * The motion sensor's SPI link: serial channel 1 of SAU0 as CSI01, to the motion sensor
 * (selected by P4.2). A transfer receives and sends at once, one byte each INTCSI01, as
 * csi10.c's CSI10 does. At a transfer's end the interrupt lets the sensor go, and a frame's
 * transfer gives its block to the host's stream 0x0400.
 */
#pragma interrupt INTCSI01 csi01Interrupt
#pragma sfr
#pragma DI
#pragma EI

#include "csi01.h"
#include "host.h"
#include "inputreq.h"
#include "motion.h"

static __sreg struct Csi01Transfer csi01;

/* opens the channel */
int csi01Open(void)
{
	ST0L.1 = 1;
	SMR01 = 0x0020;
	SCR01 = 0xc007;
	SDR01 = 0x3e00;
	SO0 |= 0x0202;
	SOE0L.1 = 1;
	P4.5 = 1;
	PM4.5 = 0;
	PM4.4 = 1;
	P4.3 = 1;
	PM4.3 = 0;
	P4.2 = 1;
	PM4.2 = 0;
	SS0L.1 = 1;
	CSIIF01 = 0;
	CSIMK01 = 0;
	csi01.state = CSI01_OPEN;
	return 0;
}

/* starts a transfer that INTCSI01 runs; 1 if one is running */
int csi01Transfer(struct Csi01Transfer *transfer)
{
	DI();
	if (csi01.state & CSI01_BUSY)
		EI();
	else {
		csi01.state = transfer->state | CSI01_BUSY;
		EI();
		csi01.receive = transfer->receive;
		csi01.send = transfer->send;
		csi01.sendCount = transfer->sendCount;
		csi01.receiveCount = transfer->receiveCount;
		if (csi01.sendCount != 0) {
			SIO01 = *csi01.send;
			csi01.send++;
			csi01.sendCount--;
		} else
			SIO01 = 0;
		return 0;
	}
	return 1;
}

int csi01Cancel(void)
{
	return 0;
}

void csi01StopChannel(void)
{
	ST0L.1 = 1;
}

/* stops the channel and lets its pins go low */
void csi01Close(void)
{
	ST0L.1 = 1;
	CSIMK01 = 1;
	CSIIF01 = 0;
	SOE0L.1 = 0;
	P4.3 = 0;
	P4.5 = 0;
	P4.2 = 0;
	csi01.state = 0;
}

unsigned char csi01GetState(void)
{
	return csi01.state;
}

/* a byte went both ways: store the one received, send the next; at a transfer's end, let the
   sensor go and hand a frame to stream 0x0400 */
void csi01Interrupt(void)
{
	unsigned char data;

	if (SSR01L & 0x01) {
		SIR01L = 0x01;
		data = 0;
	} else
		data = SIO01;
	if (csi01.receiveCount != 0) {
		*csi01.receive = data;
		csi01.receive++;
		csi01.receiveCount--;
	}
	if (csi01.sendCount != 0) {
		SIO01 = *csi01.send;
		csi01.send++;
		csi01.sendCount--;
	} else if (csi01.receiveCount != 0)
		SIO01 = 0;
	if (csi01.receiveCount == 0 && csi01.sendCount == 0 && !(SSR01L & 0x40)) {
		csi01.state &= ~CSI01_BUSY;
		P4.2 = 1;
		switch (csi01.state & CSI01_PHASE) {
		case CSI01_TRANSFER:
			break;
		case CSI01_FRAME:
			hostStream0400.first = recordFEBBC.block;
			recordFEBBC.flag |= 0x80;
			break;
		}
		csi01.state = CSI01_OPEN;
	}
}
