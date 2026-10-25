/*
 * The updater's serial events: serial4Event steps the UART4 link (mode 2, the events
 * 0x0200-0x0800) and the UART3 link (mode 1, 0x0002-0x0010) as the main loop passes them, and
 * the links' timeouts post the updater's events. The application has these at the end of
 * app/serial4.c; the updater links them between its serial.c and serial2.c.
 */
#include "serial.h"
#include "serial4.h"
#include "timer.h"
#include "updater.h"

/* the events of event flag 3: UART4's in mode 2, UART3's in mode 1 */
unsigned char serial4Event(FLGPTN ptn)
{
	unsigned char result;

	result = 0;
	if ((ptn & 0x0e00) && (serial4GetMode() & SERIAL_MODE) == 2) {
		if (ptn & 0x0400) {
			if (serial4.state == SERIAL_RECEIVE)
				serial4.state = SERIAL_RECEIVED;
			else
				serial4.state = SERIAL_RESTART;
		} else if (ptn & 0x0200) {
			serial4.state = SERIAL_NOTIFY;
		} else if (ptn & 0x0800) {
		}
		switch (serial4.state) {
		case 0:
		case SERIAL_RECEIVE:
		case SERIAL_RECEIVED:
			timerStop(0x11);
			serial4.status = serial4Receive();
			serial4Dispatch();
			break;
		case SERIAL_ANSWER:
			serial4SendAnswer();
			break;
		case SERIAL_NOTIFY:
			serial4Notify();
			break;
		case SERIAL_RESTART:
			serial4.state = SERIAL_RECEIVE;
			break;
		}
	}
	if ((ptn & 0x001e) && (serialGetMode() & SERIAL_MODE) == 1) {
		if (ptn & 0x0010) {
			serial.state = SERIAL_SEND;
		} else if (ptn & 0x0004) {
			if (serial.state == SERIAL_RECEIVE)
				serial.state = SERIAL_RECEIVED;
			else
				serial.state = SERIAL_RESTART;
		} else if (ptn & 0x0002) {
			serial.state = SERIAL_NOTIFY;
		} else if (ptn & 0x0008) {
		}
		switch (serial.state) {
		case 0:
		case SERIAL_RECEIVE:
		case SERIAL_RECEIVED:
			timerStop(0x12);
			serial.status = serialReceive();
			serialDispatch();
			break;
		case SERIAL_ANSWER:
			serialSendAnswer();
			break;
		case SERIAL_NOTIFY:
			serialNotify();
			break;
		case SERIAL_SEND:
			serialSend(&serial);
			break;
		case SERIAL_RESTART:
			serial.state = SERIAL_RECEIVE;
			break;
		}
	}
	return result;
}

/* the updater's events 1 and 0x32 */
void serial4PostTimeout(void)
{
	updEventSet(1);
}

/* UART3's timeout (as serialPostTimeout) */
void serial4PostTimeout3(void)
{
	updEventSet(0x32);
}

/* the timers of the two links' timeouts: 0x11 UART4's, 0x12 UART3's */
void serial4StartTimers(void)
{
	struct TimerHandler handler;

	handler.handler = serial4PostTimeout;
	timerSetHandler(0x11, &handler);
	handler.handler = serial4PostTimeout3;
	timerSetHandler(0x12, &handler);
}
