/*
 * The touch panels' SPI link: serial channel 2 of SAU0 as CSI10, to the two touch panel
 * controllers (panel 0 selected by P0.1, panel 1 by P0.0). A transfer receives and sends at
 * once, one byte each INTCSI10, as csi.c's CSI00 does. A transfer started in a panel's
 * command phase goes on by itself: the interrupt reads that panel's report into the frame
 * being filled, then the other panel's and the statuses the touch module asks for, and at
 * the end gives the frame to the host's streams 0x0300-0x0302.
 */
#pragma interrupt INTCSI10 csi10Interrupt
#pragma sfr
#pragma DI
#pragma EI
#pragma NOP

#include "csi10.h"
#include "host.h"
#include "touch.h"

extern struct HostStream hostStream0300;
extern struct HostStream hostStream0301;
extern struct HostStream hostStream0302;

static __sreg struct Csi10Transfer csi10;

/* opens the channel */
int csi10Open(void)
{
	if (!SAU0EN) {
		SAU0EN = 1;
		NOP();
		NOP();
		NOP();
		NOP();
		SPS0 &= 0xfff0;
	}
	ST0L.2 = 1;
	SMR02 = 0x0020;
	SCR02 = 0xc007;
	SDR02 = 0x0200;
	SO0 |= 0x0404;
	SOE0L.2 = 1;
	P0.2 = 1;
	PM0.2 = 0;
	PM0.3 = 1;
	P0.4 = 1;
	PM0.4 = 0;
	P0.1 = 1;
	P0.0 = 1;
	PM0.1 = 0;
	PM0.0 = 0;
	SS0L.2 = 1;
	CSIIF10 = 0;
	CSIMK10 = 0;
	csi10.state = CSI10_OPEN;
	return 0;
}

/* starts a transfer that INTCSI10 runs; 1 if one is running */
int csi10Transfer(struct Csi10Transfer *transfer)
{
	DI();
	if (csi10.state & CSI10_BUSY)
		EI();
	else {
		csi10.state = transfer->state | CSI10_BUSY;
		EI();
		csi10.receive = transfer->receive;
		csi10.send = transfer->send;
		csi10.sendCount = transfer->sendCount;
		csi10.receiveCount = transfer->receiveCount;
		if (csi10.sendCount != 0) {
			SIO10 = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
		} else
			SIO10 = 0;
		return 0;
	}
	return 1;
}

/* csi10Transfer, for a caller that has disabled the interrupts */
int csi10TransferNoLock(struct Csi10Transfer *transfer)
{
	if (!(csi10.state & CSI10_BUSY)) {
		csi10.state = transfer->state | CSI10_BUSY;
		csi10.receive = transfer->receive;
		csi10.send = transfer->send;
		csi10.sendCount = transfer->sendCount;
		csi10.receiveCount = transfer->receiveCount;
		if (csi10.sendCount != 0) {
			SIO10 = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
		} else
			SIO10 = 0;
		return 0;
	}
	return 1;
}

int csi10Cancel(void)
{
	return 0;
}

void csi10StopChannel(void)
{
	ST0L.2 = 1;
}

/* stops the channel and lets its pins go low */
void csi10Close(void)
{
	ST0L.2 = 1;
	CSIMK10 = 1;
	CSIIF10 = 0;
	SOE0L.2 = 0;
	P0.4 = 0;
	P0.2 = 0;
	P0.1 = 0;
	P0.0 = 0;
	csi10.state = 0;
}

unsigned char csi10GetState(void)
{
	return csi10.state;
}

/* ends the running transfer */
void csi10Release(void)
{
	DI();
	csi10.state = CSI10_OPEN;
	EI();
}

/* a byte went both ways: store the one received, send the next; at a transfer's end, start
   the next phase */
void csi10Interrupt(void)
{
	unsigned char data;
	unsigned char i;

	if (0x01 & SSR02L) {
		SIR02L = 0x01;
		data = 0;
	} else
		data = SIO10;
	if (csi10.receiveCount != 0) {
		*csi10.receive = data;
		csi10.receive++;
		csi10.receiveCount--;
	}
	for (i = 0; i < touchDelays.byte; i++)
		NOP();
	if (csi10.sendCount != 0) {
		SIO10 = *csi10.send;
		csi10.send++;
		csi10.sendCount--;
	} else if (csi10.receiveCount != 0)
		SIO10 = 0;
	if (csi10.receiveCount == 0 && csi10.sendCount == 0 && !(0x40 & SSR02L)) {
		csi10.state &= ~CSI10_BUSY;

		/* the phase that ended */
		switch (csi10.state & CSI10_PHASE) {
		case CSI10_TRANSFER:
			csi10.state = CSI10_DONE;
			break;
		case CSI10_COMMAND1:
			csi10.state = CSI10_REPORT1;
			break;
		case CSI10_REPORT1:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			P0.0 = 1;
			if (touch.flags & TOUCH_PANEL0)
				csi10.state = CSI10_COMMAND0;
			else if (touch.flags & TOUCH_STATUS1) {
				csi10.state = CSI10_STATUS1;
				for (i = 0; i < touchDelays.select; i++)
					NOP();
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_COMMAND0:
			csi10.state = CSI10_REPORT0;
			break;
		case CSI10_REPORT0:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			P0.1 = 1;
			if (touch.flags & TOUCH_STATUS1)
				csi10.state = CSI10_STATUS1;
			else if (touch.flags & TOUCH_STATUS0) {
				csi10.state = CSI10_STATUS0;
				for (i = 0; i < touchDelays.select; i++)
					NOP();
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_STATUS1:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			P0.0 = 1;
			touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATUS] = touchReply.data[0];
			if (touch.flags & TOUCH_STATUS0)
				csi10.state = CSI10_STATUS0;
			else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_STATUS0:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			P0.1 = 1;
			touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATUS] = touchReply.data[0];
			csi10.state = CSI10_FRAME;
			break;
		}

		/* the next */
		switch (csi10.state) {
		case CSI10_DONE:
			break;
		case CSI10_REPORT1:
			csi10.receive = touchFrames[touch.frame].panel[1];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state |= CSI10_BUSY;
			SIO10 = 0;
			break;
		case CSI10_COMMAND0:
			P0.1 = 0;
			touchFrames[touch.frame].time0 = TCR02;
			touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATE] = touch.state.value;
			for (i = 0; i < touchDelays.select; i++)
				NOP();
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = 0;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			SIO10 = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_REPORT0:
			csi10.receive = touchFrames[touch.frame].panel[0];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state |= CSI10_BUSY;
			SIO10 = 0;
			break;
		case CSI10_STATUS1:
			P0.0 = 0;
			for (i = 0; i < touchDelays.select; i++)
				NOP();
			touchCommand.address = touch.panel[1].objects.status | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = TOUCH_STATUS_LENGTH;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			SIO10 = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_STATUS0:
			P0.1 = 0;
			for (i = 0; i < touchDelays.select; i++)
				NOP();
			touchCommand.address = touch.panel[0].objects.status | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = TOUCH_STATUS_LENGTH;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			SIO10 = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_FRAME:
			hostStream0300.first = hostStream0301.first = hostStream0302.first = touch.frame;
			csi10.state = CSI10_OPEN;
			touch.flags |= TOUCH_FRAME;
			break;
		}
	}
}
