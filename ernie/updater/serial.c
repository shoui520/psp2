/*
 * The command link on UART3: a frame received is checked (its length, its checksum; in text,
 * its "\r\n" and its digits), run by the handler its mode has for the command, and answered
 * with the handler's status and data. The updater's: its main loop steps it through
 * serialEvent with the updater's events, and the answers are posted as events 0x34 and 0x32.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "serial.h"
#include "serial2.h"
#include "timer.h"
#include "uart.h"
#include "updater.h"
#include "util.h"


struct Serial serial;
unsigned char serialText[0x80];
unsigned char serialFrame[SERIAL_FRAME_SIZE];

/* the updater's serial events (0x0002 a notification, 0x0004 a frame or the timeout, 0x0008
   the answer sent, 0x0010 send) */
unsigned char serialEvent(FLGPTN ptn)
{
	unsigned char result;

	result = 0;
	if ((serial.mode & SERIAL_MODE) == 2) {
		if (ptn & 0x0004) {
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
		case SERIAL_RESTART:
			serial.state = SERIAL_RECEIVE;
			break;
		}
	} else if ((serial.mode & SERIAL_MODE) == 1) {
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

unsigned char serialGetLink(void)
{
	return serial.link;
}

unsigned char serialGetMode(void)
{
	return serial.mode;
}

unsigned char serialGetNextMode(void)
{
	return serial.nextMode;
}

unsigned char serialSetNextMode(unsigned char mode)
{
	return serial.nextMode = mode;
}

unsigned char serialGetBaud(void)
{
	return serial.baud;
}

unsigned char serialSetBaud(unsigned char baud)
{
	return serial.baud = baud;
}

/* the baud for a host's code; 1 if there is none */
unsigned char serialSelectBaud(unsigned char code)
{
	unsigned char bauds[6][2] = {
		{ 0x00, 2 }, { 0x01, 3 }, { 0x10, 0 }, { 0x11, 1 }, { 0x12, 2 }, { 0x13, 3 }
	};
	unsigned char i;

	for (i = 0; i < 6; i++) {
		if (bauds[i][0] == code) {
			serial.baud = bauds[i][1];
			return 0;
		}
	}
	return 1;
}

unsigned char serialGetState(void)
{
	return serial.state;
}

unsigned char serialSetState(unsigned char state)
{
	return serial.state = state;
}

unsigned char serialIsLocked(void)
{
	return serial.locked;
}

/* stops the link, locked, in mode */
void serialReset(unsigned char mode)
{
	serial.link = 0;
	serial.locked = 1;
	serial.mode = mode;
	serial.state = 0;
	if (mode & SERIAL_BINARY)
		serial.buffer = serialFrame;
	else
		serial.buffer = serialText;
	serialStop(SERIAL_PORT);
}

void serialStop(int port)
{
	uartStopTx(port);
	uartStopRx(port);
}

/* starts receiving in mode */
void serialStart(unsigned char mode)
{
	serial.mode = mode;
	serial.state = SERIAL_RECEIVE;
	if (mode & SERIAL_BINARY)
		serial.buffer = serialFrame;
	else
		serial.buffer = serialText;
	serialFlush(SERIAL_PORT);
}

/* the frame received: its fields into serial; SERIAL_OK or what is wrong with it */
unsigned char serialReceive(void)
{
	unsigned int length;
	unsigned int size;

	length = serialRead(SERIAL_PORT, serial.buffer,
			    uartReceived(SERIAL_PORT) < 0x7f ? uartReceived(SERIAL_PORT) : 0x7f);
	serialFlush(SERIAL_PORT);
	if (hexDecode((unsigned char *)&serial.command, (char *)serial.buffer, 2)) {
		serial.command = 0xffff;
		return SERIAL_BAD_HEX;
	}
	if (length == 0)
		return SERIAL_BAD_FRAME;
	if (serial.mode & SERIAL_BINARY) {
		if (length < 7 || length > 0x40)
			return SERIAL_BAD_FRAME;
		size = length;
		memcpy(serialFrame, serial.buffer, size);
	} else {
		if (length < 0x10 || length > 0x80 || (length & 1))
			return SERIAL_BAD_FRAME;
		if (*(serial.buffer + length - 2) != '\r' || *(serial.buffer + length - 1) != '\n')
			return SERIAL_BAD_FRAME;
		size = (length - 2) / 2;
		if (hexDecode(serialFrame, (char *)serial.buffer, size))
			return SERIAL_BAD_HEX;
	}
	serial.checksum = (unsigned int)*(serialFrame + size - 2) |
			  (unsigned int)*(serialFrame + size - 1) << 8;
	if (serial.checksum != serialChecksum(serialFrame, size - 2))
		return SERIAL_BAD_CHECKSUM;
	serial.command = (unsigned int)serialFrame[SERIAL_FRAME_COMMAND] |
			 (unsigned int)serialFrame[SERIAL_FRAME_COMMAND + 1] << 8;
	serial.status = serialFrame[SERIAL_FRAME_STATUS];
	if (serial.status != 0)
		;	/* Sony's tests the status and does nothing */
	serial.length = (unsigned int)serialFrame[SERIAL_FRAME_LENGTH] |
			(unsigned int)serialFrame[SERIAL_FRAME_LENGTH + 1] << 8;
	if (!(serial.mode & SERIAL_BINARY)) {
		if (serial.length & 1)
			return SERIAL_BAD_LENGTH;
		serial.length >>= 1;
	}
	if (serial.length > 0x31 || size - 7 != serial.length)	/* less than an answer carries */
		return SERIAL_BAD_LENGTH;
	return SERIAL_OK;
}

/* 0xffff less the sum of the bytes */
unsigned int serialChecksum(unsigned char *data, unsigned int length)
{
	unsigned int sum;

	sum = 0;
	while (length--)
		sum += *data++;
	return 0xffff - sum;
}

/* runs the command received (unless its status is not 0) and answers it */
unsigned char serialDispatch(void)
{
	unsigned int i;
	struct SerialBuffer request;
	struct SerialBuffer response;

	if ((serial.mode & SERIAL_MODE) == 2) {
		if (serial.status != 0)
			goto answer;
		i = serialCommands2Count();
		while (i--) {
			if (serialCommands2[i].command == serial.command) {
				request.data = &serialFrame[SERIAL_FRAME_DATA];
				request.length = serial.length;
				serial.status = serialCommands2[i].handler(&request, &response);
				serial.data = response.data;
				serial.dataLength = response.length;
				if (serial.state == SERIAL_ANSWER)
					serialPostAnswer();
				return serial.status;
			}
		}
		serial.status = SERIAL_UNKNOWN;
	} else if ((serial.mode & SERIAL_MODE) == 1) {
		if (serial.status != 0)
			goto answer;
		request.data = &serialFrame[SERIAL_FRAME_DATA];
		request.length = serial.length;
		serial.status = serialCommand1(serial.command, &request, &response);
		serial.data = response.data;
		serial.dataLength = response.length;
		if (serial.state == SERIAL_ANSWER)
			serialPostAnswer();
		return serial.status;
	}
answer:
	serial.data = NULL;
	serial.dataLength = 0;
	serial.state = SERIAL_ANSWER;
	serialPostAnswer();
	return serial.status;
}

/* sends the answer: the command with bit 15, the status and the answer's data */
unsigned char serialSendAnswer(void)
{
	unsigned int checksum;
	unsigned int textLength;

	serial.command |= 0x8000;
	memcpy(&serialFrame[SERIAL_FRAME_COMMAND], &serial.command, 2);
	serialFrame[SERIAL_FRAME_STATUS] = serial.status;
	if (serial.dataLength >= SERIAL_DATA_SIZE)
		serial.dataLength = 0;
	if (serial.mode & SERIAL_BINARY) {
		memcpy(&serialFrame[SERIAL_FRAME_LENGTH], &serial.dataLength, 2);
		if (serial.dataLength != 0 && serial.data != NULL)
			memcpy(&serialFrame[SERIAL_FRAME_DATA], serial.data, serial.dataLength);
		checksum = serialChecksum(serialFrame, serial.dataLength + SERIAL_FRAME_DATA);
		memcpy(&serialFrame[SERIAL_FRAME_DATA + serial.dataLength], &checksum, 2);
		serialWrite(SERIAL_PORT, serialFrame, serial.dataLength + 7);
	} else {
		textLength = serial.dataLength * 2;
		memcpy(&serialFrame[SERIAL_FRAME_LENGTH], &textLength, 2);
		if (serial.dataLength != 0 && serial.data != NULL)
			memcpy(&serialFrame[SERIAL_FRAME_DATA], serial.data, serial.dataLength);
		checksum = serialChecksum(serialFrame, serial.dataLength + SERIAL_FRAME_DATA);
		memcpy(&serialFrame[SERIAL_FRAME_DATA + serial.dataLength], &checksum, 2);
		hexEncode((char *)serialText, serialFrame, serial.dataLength + 7);
		serialText[textLength + 14] = '\r';
		serialText[textLength + 15] = '\n';
		serialWrite(SERIAL_PORT, serialText, textLength + 16);
	}
	return 0;
}

unsigned char serialRead(int port, unsigned char *buf, int size)
{
	return uartRead(port, buf, size);
}

unsigned char serialWrite(int port, unsigned char *buf, int size)
{
	return uartWrite(port, buf, size);
}

void serialFlush(int port)
{
	uartReset(port);
}

/* opens UART3 again at baud, 13 ticks on */
unsigned char serialReopen(int baud)
{
	sub_4DC5(13);
	return uart3Open(baud);
}

unsigned char serialOpen1(int baud)
{
	sub_A5F9(0);
	uart3Open(baud);
	serialStart(1);
	serial.link = 1;
	return 0;
}

unsigned char serialClose1(void)
{
	serialReset(0);
	serial.link = 0;
	return 0;
}

unsigned char serialOpen2(int baud)
{
	uart3Open(baud);
	serialStart(2);
	sub_A5F9(1);
	serial2SetP71(0);
	serial.link = 2;
	return 0;
}

unsigned char serialClose2(void)
{
	serial2SetP71(1);
	serialReset(0);
	serial.link = 0;
	return 0;
}

unsigned char serialOpen3(int baud)
{
	uart3Open(baud);
	serialStart(3);
	serial.link = 3;
	return 0;
}

unsigned char serialClose3(void)
{
	serialClose();
	return 0;
}

unsigned char serialClose(void)
{
	serialReset(0);
	serial.link = 0;
	return 0;
}

/* lets the commands marked locked run, if the link is open as link */
unsigned char serialUnlock(unsigned char link)
{
	if (serial.link != link)
		return SERIAL_NOT_OPEN_UNLOCK;
	serial.locked = 0;
	return 0;
}

unsigned char serialLock(unsigned char link)
{
	if (serial.link != link)
		return SERIAL_NOT_OPEN_LOCK;
	serial.locked = 1;
	return 0;
}

/* never called */
unsigned char serialSetP41(unsigned char level)
{
	P4.1 = level;
	return level & 1;
}

/* sends a frame of s's command, status and data (mode 1's event 0x0010: the frame received
   back) */
unsigned char serialSend(struct Serial *s)
{
	unsigned int checksum;
	unsigned int textLength;

	memcpy(&serialFrame[SERIAL_FRAME_COMMAND], &s->command, 2);
	serialFrame[SERIAL_FRAME_STATUS] = s->status;
	if (s->length >= SERIAL_DATA_SIZE)
		s->length = 0;
	if (serial.mode & SERIAL_BINARY) {
		memcpy(&serialFrame[SERIAL_FRAME_LENGTH], &s->length, 2);
		if (s->length != 0)
			memcpy(&serialFrame[SERIAL_FRAME_DATA], s->buffer, s->length);
		checksum = serialChecksum(serialFrame, s->length + SERIAL_FRAME_DATA);
		memcpy(&serialFrame[SERIAL_FRAME_DATA + s->length], &checksum, 2);
		serialWrite(SERIAL_PORT, serialFrame, s->length + 7);
	} else {
		textLength = s->length * 2;
		memcpy(&serialFrame[SERIAL_FRAME_LENGTH], &textLength, 2);
		if (s->length != 0)
			memcpy(&serialFrame[SERIAL_FRAME_DATA], s->buffer, s->length);
		checksum = serialChecksum(serialFrame, s->length + SERIAL_FRAME_DATA);
		memcpy(&serialFrame[SERIAL_FRAME_DATA + s->length], &checksum, 2);
		hexEncode((char *)serialText, serialFrame, s->length + 7);
		serialText[textLength + 14] = '\r';
		serialText[textLength + 15] = '\n';
		serialWrite(SERIAL_PORT, serialText, textLength + 16);
	}
	return 0;
}

/* runs the handler of the notification for the command received, back to receiving */
unsigned char serialNotify(void)
{
	unsigned char i;
	unsigned char result;

	if ((serial.mode & SERIAL_MODE) == 2) {
		i = serialNotifies2Count();
		while (i--) {
			if ((serial.command & 0x7fff) == serialNotifies2[i].command) {
				result = serialNotifies2[i].handler();
				serial.state = SERIAL_RECEIVE;
				return result;
			}
		}
	} else if ((serial.mode & SERIAL_MODE) == 1) {
		i = serialNotifies1Count();
		while (i--) {
			if ((serial.command & 0x7fff) == serialNotifies1[i].command) {
				result = serialNotifies1[i].handler();
				serial.state = SERIAL_RECEIVE;
				return result;
			}
		}
	}
	serial.state = SERIAL_RECEIVE;
	return 0;
}

/* answers now (SERIAL_ANSWER) or later (SERIAL_ANSWER_LATER) */
void serialSetAnswer(unsigned char state)
{
	switch (state) {
	case SERIAL_ANSWER_LATER:
		serial.state = state;
		break;
	case SERIAL_ANSWER:
		serial.state = state;
		break;
	}
}

/* the updater's events 0x34 and 0x32 */
void serialPostAnswer(void)
{
	updEventSet(0x34);
}

void serialPostTimeout(void)
{
	updEventSet(0x32);
}

/* never called */
void serialStartTimer(void)
{
	struct TimerHandler handler;

	handler.handler = serialPostTimeout;
	timerSetHandler(0x12, &handler);
}

/* answers command 0x0105, if it waits to (status 0, no data) */
unsigned char serialAnswer0105(void)
{
	if (serial.state != SERIAL_ANSWER_LATER || serial.command != 0x0105)
		return 0;
	serial.command |= 0x8000;
	serial.status = 0;
	serial.data = NULL;
	serial.dataLength = 0;
	serialSetAnswer(SERIAL_ANSWER);
	serialPostAnswer();
	return 0;
}
