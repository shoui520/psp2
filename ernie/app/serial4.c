/*
 * The command link on UART4: app/serial.c's link, in mode 2 only (the commands of
 * serialCommands2), stepped by the events 0x0200-0x0800 of event flag 3. serial4Event also
 * takes UART3's events (0x0002-0x0010) for app/serial.c's link in mode 1.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "kernel.h"
#include "serial.h"
#include "serial2.h"
#include "serial4.h"
#include "timer.h"
#include "uart.h"
#include "uart4.h"
#include "util.h"

extern ER sub_2C2ED(unsigned char flgid, FLGPTN setptn);

struct Serial serial4;
unsigned char serial4Text[0x80];
unsigned char serial4Frame[SERIAL_FRAME_SIZE];

unsigned char serial4GetMode(void)
{
	return serial4.mode;
}

unsigned char serial4GetNextMode(void)
{
	return serial4.nextMode;
}

unsigned char serial4SetNextMode(unsigned char mode)
{
	return serial4.nextMode = mode;
}

unsigned char serial4GetBaud(void)
{
	return serial4.baud;
}

unsigned char serial4SetBaud(unsigned char baud)
{
	return serial4.baud = baud;
}

/* the baud for a host's code; 1 if there is none */
unsigned char serial4SelectBaud(unsigned char code)
{
	unsigned char bauds[6][2] = {
		{ 0x00, 2 }, { 0x01, 3 }, { 0x10, 0 }, { 0x11, 1 }, { 0x12, 2 }, { 0x13, 3 }
	};
	unsigned char i;

	for (i = 0; i < 6; i++) {
		if (bauds[i][0] == code) {
			serial4.baud = bauds[i][1];
			return 0;
		}
	}
	return 1;
}

unsigned char serial4GetState(void)
{
	return serial4.state;
}

unsigned char serial4SetState(unsigned char state)
{
	return serial4.state = state;
}

unsigned char serial4IsLocked(void)
{
	return serial4.locked;
}

/* stops the link, locked, in mode */
void serial4Reset(unsigned char mode)
{
	serial4.link = 0;
	serial4.locked = 1;
	serial4.mode = mode;
	serial4.state = 0;
	if (mode & SERIAL_BINARY)
		serial4.buffer = serial4Frame;
	else
		serial4.buffer = serial4Text;
	serial4Stop();
}

void serial4Stop(void)
{
	uart4Close();
	return;
}

/* starts receiving in mode */
void serial4Start(unsigned char mode)
{
	serial4.mode = mode;
	serial4.state = SERIAL_RECEIVE;
	if (mode & SERIAL_BINARY)
		serial4.buffer = serial4Frame;
	else
		serial4.buffer = serial4Text;
	serial4Flush(SERIAL4_PORT);
}

/* the frame received: its fields into serial4; SERIAL_OK or what is wrong with it */
unsigned char serial4Receive(void)
{
	unsigned int length;
	unsigned int size;

	length = serial4Read(SERIAL4_PORT, serial4.buffer,
			     uartReceived(SERIAL4_PORT) < 0x7f ? uartReceived(SERIAL4_PORT) : 0x7f);
	serial4Flush(SERIAL4_PORT);
	serial4Trace(serial4.buffer);
	if (hexDecode((unsigned char *)&serial4.command, (char *)serial4.buffer, 2)) {
		serial4.command = 0xffff;
		return SERIAL_BAD_HEX;
	}
	if (length == 0)
		return SERIAL_BAD_FRAME;
	if (serial4.mode & SERIAL_BINARY) {
		if (length < 7 || length > 0x40)
			return SERIAL_BAD_FRAME;
		size = length;
		memcpy(serial4Frame, serial4.buffer, size);
	} else {
		if (length < 0x10 || length > 0x80 || (length & 1))
			return SERIAL_BAD_FRAME;
		if (*(serial4.buffer + length - 2) != '\r' || *(serial4.buffer + length - 1) != '\n')
			return SERIAL_BAD_FRAME;
		size = (length - 2) / 2;
		if (hexDecode(serial4Frame, (char *)serial4.buffer, size))
			return SERIAL_BAD_HEX;
	}
	serial4.checksum = (unsigned int)*(serial4Frame + size - 2) |
			   (unsigned int)*(serial4Frame + size - 1) << 8;
	if (serial4.checksum != serial4Checksum(serial4Frame, size - 2))
		return SERIAL_BAD_CHECKSUM;
	serial4.command = (unsigned int)serial4Frame[SERIAL_FRAME_COMMAND] |
			  (unsigned int)serial4Frame[SERIAL_FRAME_COMMAND + 1] << 8;
	serial4.status = serial4Frame[SERIAL_FRAME_STATUS];
	if (serial4.status != 0)
		;	/* Sony's tests the status and does nothing */
	serial4.length = (unsigned int)serial4Frame[SERIAL_FRAME_LENGTH] |
			 (unsigned int)serial4Frame[SERIAL_FRAME_LENGTH + 1] << 8;
	if (!(serial4.mode & SERIAL_BINARY)) {
		if (serial4.length & 1)
			return SERIAL_BAD_LENGTH;
		serial4.length >>= 1;
	}
	if (serial4.length > SERIAL_DATA_SIZE || size - 7 != serial4.length)
		return SERIAL_BAD_LENGTH;
	return SERIAL_OK;
}

/* 0xffff less the sum of the bytes */
unsigned int serial4Checksum(unsigned char *data, unsigned int length)
{
	unsigned int sum;

	sum = 0;
	while (length--)
		sum += *data++;
	return 0xffff - sum;
}

/* runs the command received (unless its status is not 0) and answers it */
unsigned char serial4Dispatch(void)
{
	unsigned int i;
	struct SerialBuffer request;
	struct SerialBuffer response;

	if (serial4.status != 0)
		goto answer;
	i = serialCommands2Count();
	while (i--) {
		if (serialCommands2[i].command == serial4.command) {
			request.data = &serial4Frame[SERIAL_FRAME_DATA];
			request.length = serial4.length;
			serial4.status = serialCommands2[i].handler(&request, &response);
			serial4.data = response.data;
			serial4.dataLength = response.length;
			if (serial4.state == SERIAL_ANSWER)
				serial4PostAnswer();
			return serial4.status;
		}
	}
	serial4.status = SERIAL_UNKNOWN;
answer:
	serial4.data = NULL;
	serial4.dataLength = 0;
	serial4.state = SERIAL_ANSWER;
	serial4PostAnswer();
	return serial4.status;
}

/* sends the answer: the command with bit 15, the status and the answer's data */
unsigned char serial4SendAnswer(void)
{
	unsigned int checksum;
	unsigned int textLength;

	serial4.command |= 0x8000;
	memcpy(&serial4Frame[SERIAL_FRAME_COMMAND], &serial4.command, 2);
	serial4Frame[SERIAL_FRAME_STATUS] = serial4.status;
	if (serial4.dataLength >= SERIAL_DATA_SIZE)
		serial4.dataLength = 0;
	if (serial4.mode & SERIAL_BINARY) {
		memcpy(&serial4Frame[SERIAL_FRAME_LENGTH], &serial4.dataLength, 2);
		if (serial4.dataLength != 0 && serial4.data != NULL)
			memcpy(&serial4Frame[SERIAL_FRAME_DATA], serial4.data, serial4.dataLength);
		checksum = serial4Checksum(serial4Frame, serial4.dataLength + SERIAL_FRAME_DATA);
		memcpy(&serial4Frame[SERIAL_FRAME_DATA + serial4.dataLength], &checksum, 2);
		serial4Write(SERIAL4_PORT, serial4Frame, serial4.dataLength + 7);
	} else {
		textLength = serial4.dataLength * 2;
		memcpy(&serial4Frame[SERIAL_FRAME_LENGTH], &textLength, 2);
		if (serial4.dataLength != 0 && serial4.data != NULL)
			memcpy(&serial4Frame[SERIAL_FRAME_DATA], serial4.data, serial4.dataLength);
		checksum = serial4Checksum(serial4Frame, serial4.dataLength + SERIAL_FRAME_DATA);
		memcpy(&serial4Frame[SERIAL_FRAME_DATA + serial4.dataLength], &checksum, 2);
		hexEncode((char *)serial4Text, serial4Frame, serial4.dataLength + 7);
		serial4Text[textLength + 14] = '\r';
		serial4Text[textLength + 15] = '\n';
		serial4Write(SERIAL4_PORT, serial4Text, textLength + 16);
		serial4Trace(serial4.buffer);
	}
	return 0;
}

unsigned char serial4Read(int port, unsigned char *buf, int size)
{
	return uart4Read(buf, size);
}

unsigned char serial4Write(int port, unsigned char *buf, int size)
{
	return uart4Write(buf, size);
}

void serial4Flush(int port)
{
	uartReset(port);
}

/* opens UART4 again at baud, 13 ticks on */
unsigned char serial4Reopen(int baud)
{
	dly_tsk(13);
	return (unsigned char)uart4Open(baud);
}

unsigned char serial4Open2(int baud)
{
	if (serial4GetMode())
		serial4Close2();
	uart4Open(baud);
	serial4Start(2);
	serial2SetP71(0);
	serial4.link = 2;
	return 0;
}

unsigned char serial4Close2(void)
{
	serial2SetP71(1);
	serial4Reset(0);
	serial4.link = 0;
	return 0;
}

unsigned char serial4Close(void)
{
	serial4Reset(0);
	serial4.link = 0;
	return 0;
}

/* lets the commands marked locked run, if the link is open as link */
unsigned char serial4Unlock(unsigned char link)
{
	if (serial4.link != link)
		return SERIAL_NOT_OPEN_UNLOCK;
	serial4.locked = 0;
	return 0;
}

unsigned char serial4Lock(unsigned char link)
{
	if (serial4.link != link)
		return SERIAL_NOT_OPEN_LOCK;
	serial4.locked = 1;
	return 0;
}

/* runs the handler of the notification for the command received, back to receiving */
unsigned char serial4Notify(void)
{
	unsigned char i;
	unsigned char result;

	i = serialNotifies2Count();
	while (i--) {
		if ((serial4.command & 0x7fff) == serialNotifies2[i].command) {
			result = serialNotifies2[i].handler();
			serial4.state = SERIAL_RECEIVE;
			return result;
		}
	}
	serial4.state = SERIAL_RECEIVE;
	return 0;
}

/* answers now (SERIAL_ANSWER) or later (SERIAL_ANSWER_LATER) */
void serial4SetAnswer(unsigned char state)
{
	switch (state) {
	case SERIAL_ANSWER_LATER:
		serial4.state = state;
		break;
	case SERIAL_ANSWER:
		serial4.state = state;
		break;
	}
}

void serial4PostAnswer(void)
{
	sub_2C2ED(3, 0x0800);
}

/* answers command 0x0205, if it waits to (status 0, no data) */
unsigned char serial4Answer0205(void)
{
	if (serial4.state != SERIAL_ANSWER_LATER || serial4.command != 0x0205)
		return 0;
	serial4.command |= 0x8000;
	serial4.status = 0;
	serial4.data = NULL;
	serial4.dataLength = 0;
	serial4SetAnswer(SERIAL_ANSWER);
	serial4PostAnswer();
	return 0;
}

unsigned int serial4GetCommand(void)
{
	return serial4.command;
}

unsigned char serial4GetStatus(void)
{
	return serial4.status;
}

unsigned char serial4GetP71(void)
{
	return (unsigned char)P7.1;
}

/* a frame received or sent (does nothing) */
void serial4Trace(unsigned char *buffer)
{
}

/* the events of event flag 3: UART4's in mode 2, UART3's in mode 1 */
unsigned char serial4Event(FLGPTN ptn)
{
	unsigned char result;

	result = 0;
	if ((ptn & 0x0e00) && (serial4GetMode() & SERIAL_MODE) == 2) {
		if (ptn & 0x0400) {
			clr_flg(3, ~0x0400);
			if (serial4.state == SERIAL_RECEIVE)
				serial4.state = SERIAL_RECEIVED;
			else
				serial4.state = SERIAL_RESTART;
		} else if (ptn & 0x0200) {
			clr_flg(3, ~0x0200);
			serial4.state = SERIAL_NOTIFY;
		} else if (ptn & 0x0800) {
			clr_flg(3, ~0x0800);
		}
		switch (serial4.state) {
		case 0:
		case SERIAL_RECEIVE:
		case SERIAL_RECEIVED:
			timerStop(0x0f);
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
			clr_flg(3, ~0x0010);
			serial.state = SERIAL_SEND;
		} else if (ptn & 0x0004) {
			clr_flg(3, ~0x0004);
			if (serial.state == SERIAL_RECEIVE)
				serial.state = SERIAL_RECEIVED;
			else
				serial.state = SERIAL_RESTART;
		} else if (ptn & 0x0002) {
			clr_flg(3, ~0x0002);
			serial.state = SERIAL_NOTIFY;
		} else if (ptn & 0x0008) {
			clr_flg(3, ~0x0008);
		}
		switch (serial.state) {
		case 0:
		case SERIAL_RECEIVE:
		case SERIAL_RECEIVED:
			timerStop(0x10);
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

void serial4PostTimeout(void)
{
	sub_2C2ED(3, 0x0400);
}

/* UART3's timeout (as serialPostTimeout) */
void serial4PostTimeout3(void)
{
	sub_2C2ED(3, 0x0004);
}

/* the timers of the two links' timeouts: 0x0f UART4's, 0x10 UART3's */
void serial4StartTimers(void)
{
	struct TimerHandler handler;

	handler.handler = serial4PostTimeout;
	timerSetHandler(0x0f, &handler);
	handler.handler = serial4PostTimeout3;
	timerSetHandler(0x10, &handler);
}
