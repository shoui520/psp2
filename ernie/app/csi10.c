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
#include "inputreq.h"
#include "touch.h"


static __sreg struct Csi10Transfer csi10;

#if BOARD_USS1001
/* USS-1001's panel: SAU0's channel 1 (CSI01) */
#define CSI10_SIO	SIO01
#define CSI10_SSR	SSR01L
#define CSI10_SIR	SIR01L
#else
#define CSI10_SIO	SIO10
#define CSI10_SSR	SSR02L
#define CSI10_SIR	SIR02L
#endif

#if BOARD_USS1001
/* CSI01 on P4.3 (SCK), P4.4 (SI) and P4.5 (SO), P12.6 the panel's select, high */
int csi10Open(void)
{
	if (!SAU0EN) {
		SAU0EN = 1;
		SPS0 &= 0xfff0;
	}
	ST0L.1 = 1;
	SMR01 = 0x0020;
	SCR01 = 0xc007;
	SDR01 = 0x0200;
	SO0 |= 0x0202;
	SOE0L.1 = 1;
	P4.5 = 1;
	PM4.5 = 0;
	PM4.4 = 1;
	P4.3 = 1;
	PM4.3 = 0;
	P12.6 = 1;
	PM12.6 = 0;
	SS0L.1 = 1;
	CSIIF01 = 0;
	CSIMK01 = 0;
	csi10.state = CSI10_OPEN;
	return 0;
}
#elif BOARD_DOL1001
/* DOL-1001: the fat board's channel, its lines' writes (P0.2, PM0.2, PM0.3, P0.4, PM0.4, then
   each panel's select and its mode) going to the dummy byte */
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
	boardDummyPin = 1;
	boardDummyPin = 0;
	boardDummyPin = 1;
	boardDummyPin = 1;
	boardDummyPin = 0;
	boardDummyPin = 1;
	boardDummyPin = 0;
	boardDummyPin = 1;
	boardDummyPin = 0;
	SS0L.2 = 1;
	CSIIF10 = 0;
	CSIMK10 = 0;
	csi10.state = CSI10_OPEN;
	return 0;
}
#else
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
#if FW_SINCE(0x00080302)
	SDR02 = 0x0200;
#else
	SDR02 = 0x3e00;	/* 0x00080003's slower clock */
#endif
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

#endif

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
#if SOURCE_0103
		csi10.delay = transfer->delay;
#endif
		if (csi10.sendCount != 0) {
			CSI10_SIO = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
		} else
			CSI10_SIO = 0;
		return 0;
	}
	return 1;
}

#if FW_SINCE(0x00090502)	/* 0x00090206 has no NoLock forms */
/* csi10Transfer, for a caller that has disabled the interrupts */
int csi10TransferNoLock(struct Csi10Transfer *transfer)
{
	if (!(csi10.state & CSI10_BUSY)) {
		csi10.state = transfer->state | CSI10_BUSY;
		csi10.receive = transfer->receive;
		csi10.send = transfer->send;
		csi10.sendCount = transfer->sendCount;
		csi10.receiveCount = transfer->receiveCount;
#if SOURCE_0103
		csi10.delay = transfer->delay;
#endif
		if (csi10.sendCount != 0) {
			CSI10_SIO = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
		} else
			CSI10_SIO = 0;
		return 0;
	}
	return 1;
}
#endif

#if TOUCH_0105
/* the slim's 0x0105: a transfer run here, byte by byte with the interrupt masked; delay NOPs
   after each byte; 1 if a transfer is running */
int csi10TransferWait(unsigned char __far *receive, unsigned int receiveCount,
		      unsigned char __far *send, unsigned int sendCount, unsigned char delay)
{
	unsigned int count;
	unsigned char dummy;
	unsigned char i;

	DI();
	if (csi10.state & CSI10_BUSY) {
		EI();
		return 1;
	}
	csi10.state |= CSI10_BUSY;
	EI();
	count = receiveCount < sendCount ? sendCount : receiveCount;
	CSIMK01 = 1;
	while (count-- != 0) {
		if (sendCount != 0) {
			sendCount--;
			CSI10_SIO = *send++;
		} else
			CSI10_SIO = 0;
		do
			NOP();
		while (SSR01 & 0x0040);
		if (receiveCount != 0) {
			receiveCount--;
			*receive++ = CSI10_SIO;
		} else
			dummy = CSI10_SIO;
		for (i = 0; i < delay; i++)
			NOP();
	}
	CSIIF01 = 0;
	CSIMK01 = 0;
	csi10.state &= ~CSI10_BUSY;
	return 0;
}
#endif

int csi10Cancel(void)
{
	return 0;
}

#if BOARD_USS1001
void csi10StopChannel(void)
{
	ST0L.1 = 1;
}

void csi10Close(void)
{
	ST0L.1 = 1;
	CSIMK01 = 1;
	CSIIF01 = 0;
	SOE0L.1 = 0;
	P4.3 = 0;
	P4.5 = 0;
	P12.6 = 0;
	csi10.state = 0;
}
#else
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
#if BOARD_DOL1001
	boardDummyPin = 0;
	boardDummyPin = 0;
	boardDummyPin = 0;
	boardDummyPin = 0;
#else
	P0.4 = 0;
	P0.2 = 0;
	P0.1 = 0;
	P0.0 = 0;
#endif
	csi10.state = 0;
}
#endif

unsigned char csi10GetState(void)
{
	return csi10.state;
}

#if FW_SINCE(0x00080307)	/* 0x00080003's and 0x00080302's transfers end the channel open themselves */
/* ends the running transfer */
void csi10Release(void)
{
#if FW_SINCE(0x01000306)
	DI();
	csi10.state = CSI10_OPEN;
	EI();
#else
	csi10.state = CSI10_OPEN;
#endif
}
#endif

/* a byte went both ways: store the one received, send the next; at a transfer's end, start
   the next phase */
#if SOURCE_0103
/* USS-1001: panel 0 alone on the link (the rear panel is on IICA1); its report ends the frame,
   whose report carries one byte more (its status) for family 0x82 variant 0x2b. DOL-1001 (on the
   fat board's channel) keeps panel 1's phases */
void csi10Interrupt(void)
{
	unsigned char data;
	unsigned char i;

	if (CSI10_SSR & 0x01) {
		CSI10_SIR = 0x01;
		data = 0;
	} else
		data = CSI10_SIO;
	if (csi10.receiveCount != 0) {
		*csi10.receive = data;
		csi10.receive++;
		csi10.receiveCount--;
	}
	for (i = 0; i < csi10.delay; i++)
		NOP();
	if (csi10.sendCount != 0) {
		CSI10_SIO = *csi10.send;
		csi10.send++;
		csi10.sendCount--;
	} else if (csi10.receiveCount != 0)
		CSI10_SIO = 0;
	if (csi10.receiveCount == 0 && csi10.sendCount == 0 && !(CSI10_SSR & 0x40)) {
		csi10.state &= ~CSI10_BUSY;
		switch (csi10.state & CSI10_PHASE) {
		case CSI10_TRANSFER:
			csi10.state = CSI10_DONE;
			break;
#if BOARD_DOL1001
		case CSI10_COMMAND1:
			csi10.state = CSI10_REPORT1;
			break;
		case CSI10_REPORT1:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			TOUCH_SELECT1 = 1;
#if SOURCE_0104
			if (touch.panel[1].features.bits.family82)
				touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATUS] =
					touchFrames[touch.frame].panel[1][TOUCH_REPORT_SIZE];
#endif
			if (touch.flags & TOUCH_PANEL0)
				csi10.state = CSI10_COMMAND0;
			else
				csi10.state = CSI10_FRAME;
			break;
#endif
		case CSI10_COMMAND0:
			csi10.state = CSI10_REPORT0;
			break;
		case CSI10_REPORT0:
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
			TOUCH_SELECT0 = 1;
#if SOURCE_0104
			if (touch.panel[0].features.bits.family82)
				touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATUS] =
					touchFrames[touch.frame].panel[0][TOUCH_REPORT_SIZE];
#endif
			csi10.state = CSI10_FRAME;
			break;
		}
		switch (csi10.state) {
		case CSI10_DONE:
			break;
		case CSI10_REPORT1:
#if SOURCE_0104	/* panel 0's feature, also for panel 1 */
			if (touch.panel[0].features.bits.family82)
				csi10.receiveCount = TOUCH_REPORT_SIZE + 1;
			else
				csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.receive = touchFrames[touch.frame].panel[1];
#else
			csi10.receive = touchFrames[touch.frame].panel[1];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
#endif
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_COMMAND0:
			TOUCH_SELECT0 = 0;
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
			CSI10_SIO = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_REPORT0:
#if SOURCE_0104
			if (touch.panel[0].features.bits.family82)
#else
			if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x2b)
#endif
				csi10.receiveCount = TOUCH_REPORT_SIZE + 1;
			else
				csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.receive = touchFrames[touch.frame].panel[0];
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_FRAME:
			/* family 0x82 variant 0x2b reports its status as the report's byte 0x34
			   (SOURCE_0104: copied at each report's end) */
#if !SOURCE_0104
			if (touch.panel[0].family == 0x82 && touch.panel[0].variant == 0x2b)
				touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATUS] =
					touchFrames[touch.frame].panel[0][TOUCH_REPORT_SIZE];
#endif
			hostStream0300.first = hostStream0301.first = hostStream0302.first = touch.frame;
			csi10.state = CSI10_OPEN;
			touch.flags |= TOUCH_FRAME;
			touch.flags &= ~TOUCH_RUNNING;
			break;
		}
	}
}
#elif !FW_SINCE(0x00080302)
/* 0x00080003: as 0x00090804's, without the delays, and a transfer's end leaves the channel
   open */
void csi10Interrupt(void)
{
	unsigned char data;

	if (0x01 & SSR02L) {
		SIR02L = 0x01;
		data = 0;
	} else
		data = CSI10_SIO;
	if (csi10.receiveCount != 0) {
		*csi10.receive = data;
		csi10.receive++;
		csi10.receiveCount--;
	}
	if (csi10.sendCount != 0) {
		CSI10_SIO = *csi10.send;
		csi10.send++;
		csi10.sendCount--;
	} else if (csi10.receiveCount != 0)
		CSI10_SIO = 0;
	if (csi10.receiveCount == 0 && csi10.sendCount == 0 && !(0x40 & SSR02L)) {
		csi10.state &= ~CSI10_BUSY;
		switch (csi10.state & CSI10_PHASE) {
		case CSI10_TRANSFER:
			csi10.state = CSI10_OPEN;
			break;
		case CSI10_COMMAND1:
			csi10.receive = touchFrames[touch.frame].panel[1];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state = CSI10_REPORT1 | CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_REPORT1:
			P0.0 = 1;
			if (touch.flags & TOUCH_PANEL0) {
				P0.1 = 0;
				touchFrames[touch.frame].time0 = TCR02;
				touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
				csi10.receive = (unsigned char *)&touchReply;
				csi10.receiveCount = 0;
				csi10.send = (unsigned char *)&touchCommand;
				csi10.sendCount = 2;
				csi10.state = CSI10_COMMAND0 | CSI10_BUSY;
				CSI10_SIO = *csi10.send;
				csi10.send++;
				csi10.sendCount--;
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_COMMAND0:
			csi10.receive = touchFrames[touch.frame].panel[0];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state = CSI10_REPORT0 | CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_REPORT0:
			P0.1 = 1;
			csi10.state = CSI10_FRAME;
			break;
		}
		if (csi10.state == CSI10_FRAME) {
			hostStream0300.first = hostStream0301.first = hostStream0302.first = touch.frame;
			csi10.state = CSI10_OPEN;
			touch.flags |= TOUCH_FRAME;
		}
	}
}
#elif !FW_SINCE(0x00090903)
/* 0x00090804: each phase's end starts the next itself, and the frame has no status reads */
void csi10Interrupt(void)
{
	unsigned char data;
	unsigned char i;

	if (0x01 & SSR02L) {
		SIR02L = 0x01;
		data = 0;
	} else
		data = CSI10_SIO;
	if (csi10.receiveCount != 0) {
		*csi10.receive = data;
		csi10.receive++;
		csi10.receiveCount--;
	}
#if !FW_0302
	for (i = 0; i < TOUCH_DELAY_BYTE; i++)
		NOP();
#endif
	if (csi10.sendCount != 0) {
		CSI10_SIO = *csi10.send;
		csi10.send++;
		csi10.sendCount--;
	} else if (csi10.receiveCount != 0)
		CSI10_SIO = 0;
	if (csi10.receiveCount == 0 && csi10.sendCount == 0 && !(0x40 & SSR02L)) {
		csi10.state &= ~CSI10_BUSY;
		switch (csi10.state & CSI10_PHASE) {
		case CSI10_TRANSFER:
#if FW_0302
			csi10.state = CSI10_OPEN;	/* 0x00080302 has no CSI10_DONE */
#else
			csi10.state = CSI10_DONE;
#endif
			break;
		case CSI10_COMMAND1:
			csi10.receive = touchFrames[touch.frame].panel[1];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state = CSI10_REPORT1 | CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_REPORT1:
			P0.0 = 1;
			if (touch.flags & TOUCH_PANEL0) {
				P0.1 = 0;
				touchFrames[touch.frame].time0 = TCR02;
#if FW_SINCE(0x00090804)
				touchFrames[touch.frame].state0 = touch.state.value;
#endif
#if !FW_0302
				for (i = 0; i < TOUCH_DELAY_SELECT; i++)
					NOP();
#endif
				touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
				csi10.receive = (unsigned char *)&touchReply;
				csi10.receiveCount = 0;
				csi10.send = (unsigned char *)&touchCommand;
				csi10.sendCount = 2;
				csi10.state = CSI10_COMMAND0 | CSI10_BUSY;
				CSI10_SIO = *csi10.send;
				csi10.send++;
				csi10.sendCount--;
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_COMMAND0:
			csi10.receive = touchFrames[touch.frame].panel[0];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state = CSI10_REPORT0 | CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_REPORT0:
			P0.1 = 1;
			csi10.state = CSI10_FRAME;
			break;
		}
		if (csi10.state == CSI10_FRAME) {
			hostStream0300.first = hostStream0301.first = hostStream0302.first = touch.frame;
			csi10.state = CSI10_OPEN;
			touch.flags |= TOUCH_FRAME;
		}
	}
}
#else
void csi10Interrupt(void)
{
	unsigned char data;
	unsigned char i;

	if (0x01 & SSR02L) {
		SIR02L = 0x01;
		data = 0;
	} else
		data = CSI10_SIO;
	if (csi10.receiveCount != 0) {
		*csi10.receive = data;
		csi10.receive++;
		csi10.receiveCount--;
	}
	for (i = 0; i < TOUCH_DELAY_BYTE; i++)
		NOP();
	if (csi10.sendCount != 0) {
		CSI10_SIO = *csi10.send;
		csi10.send++;
		csi10.sendCount--;
	} else if (csi10.receiveCount != 0)
		CSI10_SIO = 0;
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
#if FW_SINCE(0x01000306)
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
#endif
			P0.0 = 1;
			if (touch.flags & TOUCH_PANEL0)
				csi10.state = CSI10_COMMAND0;
			else if (touch.flags & TOUCH_STATUS1) {
				csi10.state = CSI10_STATUS1;
				for (i = 0; i < TOUCH_DELAY_SELECT; i++)
					NOP();
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_COMMAND0:
			csi10.state = CSI10_REPORT0;
			break;
		case CSI10_REPORT0:
#if FW_SINCE(0x01000306)
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
#endif
			P0.1 = 1;
			if (touch.flags & TOUCH_STATUS1)
				csi10.state = CSI10_STATUS1;
			else if (touch.flags & TOUCH_STATUS0) {
				csi10.state = CSI10_STATUS0;
				for (i = 0; i < TOUCH_DELAY_SELECT; i++)
					NOP();
			} else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_STATUS1:
#if FW_SINCE(0x01000306)
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
#endif
			P0.0 = 1;
			touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATUS] = touchReply.data[0];
			if (touch.flags & TOUCH_STATUS0)
				csi10.state = CSI10_STATUS0;
			else
				csi10.state = CSI10_FRAME;
			break;
		case CSI10_STATUS0:
#if FW_SINCE(0x01000306)
			for (i = 0; i < touchDelays.deselect; i++)
				NOP();
#endif
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
			CSI10_SIO = 0;
			break;
		case CSI10_COMMAND0:
			P0.1 = 0;
			touchFrames[touch.frame].time0 = TCR02;
			touchFrames[touch.frame].panel[0][TOUCH_PANEL_STATE] = touch.state.value;
			for (i = 0; i < TOUCH_DELAY_SELECT; i++)
				NOP();
			touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = 0;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_REPORT0:
			csi10.receive = touchFrames[touch.frame].panel[0];
			csi10.receiveCount = TOUCH_REPORT_SIZE;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 0;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = 0;
			break;
		case CSI10_STATUS1:
			P0.0 = 0;
			for (i = 0; i < TOUCH_DELAY_SELECT; i++)
				NOP();
			touchCommand.address = touch.panel[1].objects.status | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = TOUCH_STATUS_LENGTH;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = *csi10.send;
			csi10.send++;
			csi10.sendCount--;
			break;
		case CSI10_STATUS0:
			P0.1 = 0;
			for (i = 0; i < TOUCH_DELAY_SELECT; i++)
				NOP();
			touchCommand.address = touch.panel[0].objects.status | TOUCH_READ;
			csi10.receive = (unsigned char *)&touchReply;
			csi10.receiveCount = TOUCH_STATUS_LENGTH;
			csi10.send = (unsigned char *)&touchCommand;
			csi10.sendCount = 2;
			csi10.state |= CSI10_BUSY;
			CSI10_SIO = *csi10.send;
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
#endif
