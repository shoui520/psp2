/*
 * USS-1001's second IICA (I2C) master, on P6.2 (SCLA1) and P6.3 (SDAA1): app/iica.c's master
 * for the rear touch panel. iica1Start's transfer runs in the interrupt, which at its end hands
 * the panel's report to the frame and starts the front panel's (app/csi10.c).
 */
#pragma sfr
#pragma NOP

#include <string.h>
#include "ernie.h"
#include "csi10.h"
#include "iica1.h"
#include "inputreq.h"
#include "touch.h"

#define IICA_ADDRESS		2	/* the address byte sent */
#define IICA_SEND		3
#define IICA_RECEIVE		4
#define IICA_DONE		6

#define IICA_BUSY		0x80	/* *status while a transfer runs */

#define IICA1_WAIT		0xff00	/* the polled transfer's wait for each interrupt */
#define IICA1_STOP_WAIT		0xf000	/* and for its last byte's */

struct Iica1Request iica1Request;

/* enables the IICA and frees the bus; 1 when the bus stayed busy */
unsigned char iica1Init(void)
{
	unsigned char retry;
	unsigned char i;

	if (IICA1EN)
		return 0;
	retry = 2;
	do {
		IICA1EN = 1;
		IICE1 = 0;
		IICWL1 = 0x0b;
		IICWH1 = 0x07;
		SVA1 = 0;
		IICF1 = 0;
		IICCTL11 = 0x0c;
		IICCTL10 = 0x1c;
		IICE1 = 1;
		P6.2 = 0;
		PM6.2 = 0;
		P6.3 = 0;
		PM6.3 = 0;
		IICAIF1 = 0;
		if (STCEN1)
			goto ready;
		SPT1 = 1;
		for (i = 0; i < 0xff; i++) {
			if (IICAIF1) {
				IICAIF1 = 0;
				goto ready;
			}
		}
		/* the bus is held: clock SCL by hand until SDA is released */
		iica1Close();
		PM6.3 = 1;
		for (i = 0; i < 0x12; i++) {
			if (P6.2) {
				P6.2 = 0;
				if (P6.3)
					break;
			} else
				P6.2 = 1;
		}
		P6.2 = 1;
		P6.3 = 1;
		PM6.3 = 0;
	} while (retry-- != 0);
	iica1Close();
	return 1;
ready:
	SPIE1 = 0;
	IICAMK1 = 0;
	return 0;
}

/* starts a transfer; the interrupt runs it */
int iica1Start(struct Iica1Request *request)
{
	iica1Request = *request;
	IICAMK1 = 0;
	STT1 = 1;
	iica1Request.state = IICA_ADDRESS;
	*iica1Request.status = IICA_BUSY;
	IICA1 = iica1Request.address;
	return 0;
}

/* runs a transfer, polling, with the interrupt masked; 1 or 2 when an interrupt did not come */
unsigned char iica1Transfer(struct Iica1Request *request)
{
	unsigned int wait;

	iica1Request = *request;
	IICAMK1 = 1;
	IICAIF1 = 0;
	if (iica1Request.txCount != 0) {
		STT1 = 1;
		iica1Request.state = IICA_ADDRESS;
		IICA1 = iica1Request.address;
	} else if (iica1Request.rxCount != 0) {
		STT1 = 1;
		IICA1 = iica1Request.address | 1;
		iica1Request.state = IICA_ADDRESS;
	} else
		goto done;
	for (;;) {
		wait = IICA1_WAIT;
		while (!IICAIF1) {
			if (wait-- == 0)
				goto noAddressAck;
		}
		IICAIF1 = 0;
		switch (iica1Request.state) {
		case IICA_ADDRESS:
			if (ACKD1) {
				if (TRC1) {
					IICA1 = *iica1Request.txData;
					iica1Request.txData++;
					iica1Request.txCount--;
					iica1Request.state = IICA_SEND;
				} else {
					ACKE1 = 1;
					WTIM1 = 0;
					WREL1 = 1;
					iica1Request.state = IICA_RECEIVE;
				}
			} else {
				*iica1Request.status = 1;
				goto stop;
			}
			break;
		case IICA_SEND:
			if (ACKD1) {
				if (iica1Request.txCount != 0) {
					IICA1 = *iica1Request.txData;
					iica1Request.txData++;
					iica1Request.txCount--;
				} else if (iica1Request.rxCount != 0) {
					STT1 = 1;
					IICA1 = iica1Request.address | 1;
					iica1Request.state = IICA_ADDRESS;
				} else {
					*iica1Request.status = 0;
					goto stop;
				}
			} else {
				*iica1Request.status = 2;
				goto stop;
			}
			break;
		case IICA_RECEIVE:
			*iica1Request.rxData = IICA1;
			iica1Request.rxData++;
			iica1Request.rxCount--;
			if (iica1Request.rxCount != 0) {
				WREL1 = 1;
				break;
			}
			IICAIF1 = 0;
			ACKE1 = 0;
			WTIM1 = 1;
			WREL1 = 1;
			wait = IICA1_STOP_WAIT;
			while (!IICAIF1) {
				if (wait-- == 0)
					goto noStop;
			}
			IICAIF1 = 0;
			*iica1Request.status = 0;
			goto stop;
		}
	}
stop:
	SPT1 = 1;
done:
	iica1Request.state = IICA_DONE;
	return 0;
noAddressAck:
	iica1Request.state = IICA_DONE;
	return 1;
noStop:
	iica1Request.state = IICA_DONE;
	return 2;
}

void iica1Close(void)
{
	IICA1_CLOSE();
}

/*
 * INTIICA1: the next step of iica1Start's transfer. At its end the rear panel's report goes into
 * the frame, then panel 0's report command goes out on CSI10, or, with panel 0 off, the frame
 * is complete.
 */
__rtos_interrupt void iica1Interrupt(void)
{
	unsigned char i;
	unsigned char report;
	struct Csi10Transfer transfer;

	switch (iica1Request.state) {
	case IICA_ADDRESS:
		if (ACKD1) {
			if (TRC1) {
				IICA1 = *iica1Request.txData;
				iica1Request.txData++;
				iica1Request.txCount--;
				iica1Request.state = IICA_SEND;
			} else {
				ACKE1 = 1;
				WTIM1 = 0;
				WREL1 = 1;
				iica1Request.state = IICA_RECEIVE;
			}
		} else {
			*iica1Request.status = 1;
			goto stop;
		}
		break;
	case IICA_SEND:
		if (ACKD1) {
			if (iica1Request.txCount != 0) {
				IICA1 = *iica1Request.txData;
				iica1Request.txData++;
				iica1Request.txCount--;
			} else if (iica1Request.rxCount != 0) {
				STT1 = 1;
				IICA1 = iica1Request.address | 1;
				iica1Request.state = IICA_ADDRESS;
			} else {
				*iica1Request.status = 0;
				goto stop;
			}
		} else {
			*iica1Request.status = 2;
			goto stop;
		}
		break;
	case IICA_RECEIVE:
		*iica1Request.rxData = IICA1;
		iica1Request.rxData++;
		iica1Request.rxCount--;
		if (iica1Request.rxCount != 0) {
			WREL1 = 1;
			break;
		}
		IICAIF1 = 0;
		ACKE1 = 0;
		WTIM1 = 1;
		WREL1 = 1;
		while (!IICAIF1)
			NOP();
		*iica1Request.status = 0;
		goto stop;
	default:
		break;
	}
	return;
stop:
	SPT1 = 1;
	iica1Request.state = IICA_DONE;
	if (*iica1Request.status == 0) {
		if (touch.panel[1].version == 9 && touch.panel[1].build < 0x12)
			report = TOUCH_REAR_REPORT_OLD;
		else
			report = TOUCH_REAR_REPORT;
		memcpy(&touchFrames[touch.frame].panel[1][TOUCH_REAR_TOUCH],
		       &touchFrames[touch.frame].panel[1][report], TOUCH_REAR_TOUCH_SIZE);
		touchFrames[touch.frame].panel[1][TOUCH_PANEL_STATUS] =
			touchFrames[touch.frame].panel[1][report + TOUCH_REAR_TOUCH_SIZE];
		memset(&touchFrames[touch.frame].panel[1][report], 0x80, TOUCH_REAR_TOUCH_SIZE + 1);
	}
	if (touch.flags & TOUCH_PANEL0) {
		P12.6 = 0;
		touchFrames[touch.frame].time0 = TCR02;
		TOUCH_FRAME_STATE(touch.frame, 0) = touch.state.value;
		for (i = 0; i < TOUCH_DELAY_SELECT; i++)
			NOP();
		touchCommand.address = touch.panel[0].objects.report | TOUCH_READ;
		transfer.receive = (unsigned char *)&touchReply;
		transfer.receiveCount = 0;
		transfer.send = (unsigned char *)&touchCommand;
		transfer.sendCount = 2;
		transfer.state = CSI10_COMMAND0;
		transfer.delay = TOUCH_DELAY_BYTE;
		csi10TransferNoLock(&transfer);
	} else {
		hostStream0300.first = hostStream0301.first = hostStream0302.first = touch.frame;
		touch.flags |= TOUCH_FRAME;
		touch.flags &= ~TOUCH_RUNNING;
	}
}
