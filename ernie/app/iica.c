/*
 * The IICA (I2C) master: a transfer writes txCount bytes to the device at address, then, when
 * rxCount is not 0, reads rxCount bytes after a repeated start, and leaves its result (0, or
 * 1 and 2 for a missing acknowledge) in *status.
 */
#ifndef ERNIE_UPDATER
#pragma rtos_interrupt INTIICA iicaInterrupt
#endif
#pragma sfr
#pragma NOP

#include "ernie.h"
#include "iica.h"
#include "task.h"

#define IICA_IDLE		0
#define IICA_ADDRESS		2	/* the address byte sent */
#define IICA_SEND		3
#define IICA_RECEIVE		4
#define IICA_DONE		6

#define IICA_WAIT		0xf000	/* the polled transfer's wait for each interrupt */

static struct IicaRequest iicaRequest;

#ifdef ERNIE_UPDATER
/* the updater's: enables the IICA and sends a stop; 1 when it did not go out */
unsigned char iicaInit(void)
{
	unsigned char i;

	if (IICAEN)
		return 0;
	IICAEN = 1;
	IICE = 0;
	IICWL = 0x0b;
	IICWH = 0x08;
	SVA = 0;
	IICF = 0;
	IICCTL1 = 0x0c;
	IICCTL0 = 0x0c;
	IICE = 1;
	P6.0 = 0;
	P6.1 = 0;
	PM6.0 = 0;
	PM6.1 = 0;
	IICAIF = 0;
	if (STCEN)
		return 0;
	SPT = 1;
	SPIE = 1;
	i = 0xff;
	while (!IICAIF) {
		if (i-- == 0)
			return 1;
	}
	IICAIF = 0;
	SPIE = 0;
	return 0;
}
#else
/* enables the IICA and frees the bus; 1 when the bus stayed busy */
unsigned char iicaInit(void)
{
	unsigned char retry;
	unsigned char i;

	if (IICAEN)
		return 0;
	retry = 2;
	do {
		IICAEN = 1;
		IICE = 0;
		IICWL = 0x0b;
		IICWH = 0x08;
		SVA = 0;
		IICF = 0;
		IICCTL1 = 0x0c;
		IICCTL0 = 0x1c;
		IICE = 1;
		P6.0 = 0;
		P6.1 = 0;
		PM6.0 = 0;
		PM6.1 = 0;
		IICAIF = 0;
		if (STCEN)
			goto ready;
		SPT = 1;
		for (i = 0; i < 0xff; i++) {
			if (IICAIF) {
				IICAIF = 0;
				goto ready;
			}
		}
		/* the bus is held: clock SCL by hand until SDA is released */
		iicaClose();
		PM6.1 = 1;
		for (i = 0; i < 0x12; i++) {
			if (P6.0) {
				P6.0 = 0;
				if (P6.1)
					break;
			} else
				P6.0 = 1;
		}
		P6.0 = 1;
		P6.1 = 1;
		PM6.1 = 0;
	} while (retry-- != 0);
	iicaClose();
	return 1;
ready:
	SPIE = 0;
	IICAMK = 0;
	return 0;
}
#endif

/* starts a transfer; the interrupt runs it */
int iicaStart(struct IicaRequest *request)
{
	iicaRequest = *request;
	STT = 1;
	iicaRequest.state = IICA_ADDRESS;
	IICA = iicaRequest.address;
	return 0;
}

#ifdef ERNIE_UPDATER
/* PLACEHOLDER: the updater's, called by nothing; named when its use is known */
int sub_39FB5(void)
{
	return 0;
}
#endif

/* runs a transfer, polling; 1 or 2 when an interrupt did not come */
int iicaTransfer(struct IicaRequest *request)
{
	unsigned int wait;

	iicaRequest = *request;
	STT = 1;
	iicaRequest.state = IICA_ADDRESS;
	IICA = iicaRequest.address;
	for (;;) {
		wait = IICA_WAIT;
		while (!IICAIF) {
			if (wait-- == 0)
				goto noAddressAck;
		}
		IICAIF = 0;
		switch (iicaRequest.state) {
		case IICA_ADDRESS:
			if (ACKD) {
				if (TRC) {
					IICA = *iicaRequest.txData;
					iicaRequest.txData++;
					iicaRequest.txCount--;
					iicaRequest.state = IICA_SEND;
				} else {
					ACKE = 1;
					WTIM = 0;
					WREL = 1;
					iicaRequest.state = IICA_RECEIVE;
				}
			} else {
				*iicaRequest.status = 1;
				goto stop;
			}
			break;
		case IICA_SEND:
			if (ACKD) {
				if (iicaRequest.txCount != 0) {
					IICA = *iicaRequest.txData;
					iicaRequest.txData++;
					iicaRequest.txCount--;
				} else if (iicaRequest.rxCount != 0) {
					STT = 1;
					IICA = iicaRequest.address | 1;
					iicaRequest.state = IICA_ADDRESS;
				} else {
					*iicaRequest.status = 0;
					goto stop;
				}
			} else {
				*iicaRequest.status = 2;
				goto stop;
			}
			break;
		case IICA_RECEIVE:
			*iicaRequest.rxData = IICA;
			iicaRequest.rxData++;
			iicaRequest.rxCount--;
			if (iicaRequest.rxCount != 0) {
				WREL = 1;
				break;
			}
			IICAIF = 0;
			ACKE = 0;
			WTIM = 1;
			WREL = 1;
			wait = IICA_WAIT;
			while (!IICAIF) {
				if (wait-- == 0)
					goto noStop;
			}
			IICAIF = 0;
			*iicaRequest.status = 0;
			goto stop;
		}
	}
stop:
	SPT = 1;
	iicaRequest.state = IICA_DONE;
	return 0;
noAddressAck:
	iicaRequest.state = IICA_DONE;
	return 1;
noStop:
	iicaRequest.state = IICA_DONE;
	return 2;
}

void iicaClose(void)
{
	P6.0 = 1;
	P6.1 = 1;
	PM6.0 = 0;
	PM6.1 = 0;
	IICE = 0;
	IICAMK = 1;
	IICAIF = 0;
	iicaRequest.state = IICA_IDLE;
	IICAEN = 0;
}

#ifndef ERNIE_UPDATER
/* INTIIC0: the next step of iicaStart's transfer; at its end, event 0x10 of flag 4 */
void iicaInterrupt(void)
{
	switch (iicaRequest.state) {
	case IICA_ADDRESS:
		if (ACKD) {
			if (TRC) {
				IICA = *iicaRequest.txData;
				iicaRequest.txData++;
				iicaRequest.txCount--;
				iicaRequest.state = IICA_SEND;
			} else {
				ACKE = 1;
				WTIM = 0;
				WREL = 1;
				iicaRequest.state = IICA_RECEIVE;
			}
		} else {
			*iicaRequest.status = 1;
			goto stop;
		}
		break;
	case IICA_SEND:
		if (ACKD) {
			if (iicaRequest.txCount != 0) {
				IICA = *iicaRequest.txData;
				iicaRequest.txData++;
				iicaRequest.txCount--;
			} else if (iicaRequest.rxCount != 0) {
				STT = 1;
				IICA = iicaRequest.address | 1;
				iicaRequest.state = IICA_ADDRESS;
			} else {
				*iicaRequest.status = 0;
				goto stop;
			}
		} else {
			*iicaRequest.status = 2;
			goto stop;
		}
		break;
	case IICA_RECEIVE:
		*iicaRequest.rxData = IICA;
		iicaRequest.rxData++;
		iicaRequest.rxCount--;
		if (iicaRequest.rxCount != 0) {
			WREL = 1;
			break;
		}
		IICAIF = 0;
		ACKE = 0;
		WTIM = 1;
		WREL = 1;
		while (!IICAIF)
			NOP();
		*iicaRequest.status = 0;
		goto stop;
	default:
		break;
	}
	return;
stop:
	SPT = 1;
	iicaRequest.state = IICA_DONE;
	task5SetFlag(4, 0x0010);
}
#endif
