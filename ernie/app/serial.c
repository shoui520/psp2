/*
 * The command link on UART3: a frame received is checked (its length, its checksum; in text,
 * its "\r\n" and its digits), run by the handler its mode has for the command, and answered
 * with the handler's status and data. The events of event flag 3 (bits 0x0002-0x0010) step
 * it through serial4Event (app/serial4.c); serialEvent, which would take them, is never
 * called.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#ifndef ERNIE_UPDATER
#include "kernel.h"
#endif
#include "serial.h"
#include "serial2.h"
#ifndef ERNIE_UPDATER
#include "task.h"
#endif
#include "timer.h"
#include "uart.h"
#include "util.h"
#ifdef ERNIE_UPDATER
#include "../updater/updater.h"
#endif
#if BOARD_USS1001
#include "uart4.h"
#endif
#if SOURCE_0103
#include "version.h"
#endif

/* the PS TV's updater (0x01030603) builds this file too: it has no kernel, so it waits by
   powerWaitLong and posts the answer and the timeout as its events 0x34 and 0x32 */
#ifdef ERNIE_UPDATER
#define serialWait(ticks)	powerWaitLong(ticks)
#else
#define serialWait(ticks)	dly_tsk(ticks)
#endif

/* the slim's 0x0105 logs the mode 1 commands and their answers */
#define SERIAL_LOG		(BOARD_EVENT_LOG && SOURCE_0104)
#if SERIAL_LOG
#include "eventlog.h"
#endif


struct Serial serial;
unsigned char serialText[0x80];
unsigned char serialFrame[SERIAL_FRAME_SIZE];

#if !SOURCE_0103	/* not in the slim's */
/* the events of event flag 3 (never called) */
unsigned char serialEvent(FLGPTN ptn)
{
	unsigned char result;

	result = 0;
	if (SERIAL_MODE_OF(serial) == 2) {
#if BOARD_SERIAL2
		if (ptn & 0x0004) {
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
			timerStop(TIMER(0x10));
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
#endif
	} else if (SERIAL_MODE_OF(serial) == 1) {
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
			timerStop(TIMER(0x10));
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
	} else {
		clr_flg(3, ~0x001e);
	}
	return result;
}
#endif

unsigned char serialGetLink(void)
{
	return serial.link;
}

unsigned char serialGetMode(void)
{
#if SERIAL_MODE_BITS
	return SERIAL_MODE_OF(serial);
#else
	return serial.mode;
#endif
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
	SERIAL_MODE_BYTE(serial) = mode;
	serial.state = 0;
#if SERIAL_MODE_BITS
	if (SERIAL_IS_BINARY(serial))
#else
	if (mode & SERIAL_BINARY)
#endif
		serial.buffer = serialFrame;
	else
		serial.buffer = serialText;
	serialStop(SERIAL_PORT);
}

void serialStop(int port)
{
#if BOARD_USS1001	/* the slim's UART4 */
	uart4Close();
#else
	uartStopTx(port);
	uartStopRx(port);
#endif
}

/* starts receiving in mode */
void serialStart(unsigned char mode)
{
	SERIAL_MODE_BYTE(serial) = mode;
	serial.state = SERIAL_RECEIVE;
#if SERIAL_MODE_BITS
	if (SERIAL_IS_BINARY(serial))
#else
	if (mode & SERIAL_BINARY)
#endif
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
	if SERIAL_IS_BINARY(serial) {
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
	if (!SERIAL_IS_BINARY(serial)) {
		if (serial.length & 1)
			return SERIAL_BAD_LENGTH;
		serial.length >>= 1;
	}
	if (serial.length > SERIAL_RECEIVE_SIZE || size - 7 != serial.length)
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
#if BOARD_SERIAL2 || !FW_SINCE(0x0100060B)
	unsigned int i;		/* 0x0100060B leaves it out where it is unused */
#endif
	struct SerialBuffer request;
	struct SerialBuffer response;

	if (SERIAL_MODE_OF(serial) == 2) {
#if BOARD_SERIAL2
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
#endif
	} else if (SERIAL_MODE_OF(serial) == 1) {
		if (serial.status != 0)
			goto answer;
#if FW_SINCE(0x00090502)
		request.data = &serialFrame[SERIAL_FRAME_DATA];
		request.length = serial.length;
#if SERIAL_LOG
		/* kind 0x11, the command's high byte: its low byte, its length and its first two
		   bytes */
		eventLogCommand[0] = serial.command & 0xff;
		eventLogCommand[1] = request.length;
		eventLogCommand[2] = request.data[0];
		eventLogCommand[3] = request.data[1];
		eventLogRecord(0x11, serial.command >> 8, eventLogCommand, 4);
#endif
		serial.status = serialCommand1(serial.command, &request, &response);
		serial.data = response.data;
		serial.dataLength = response.length;
		if (serial.state == SERIAL_ANSWER)
			serialPostAnswer();
		return serial.status;
#else
		/* 0x00090206 searches mode 1's commands as mode 2's */
		i = serialCommands1Count();
		while (i--) {
			if (serialCommands1[i].command == serial.command) {
				request.data = &serialFrame[SERIAL_FRAME_DATA];
				request.length = serial.length;
				serial.status = serialCommands1[i].handler(&request, &response);
				serial.data = response.data;
				serial.dataLength = response.length;
				if (serial.state == SERIAL_ANSWER)
					serialPostAnswer();
				return serial.status;
			}
		}
		serial.status = SERIAL_UNKNOWN;
#endif
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
#if SERIAL_LOG
	/* kind 0x11, the command's high byte (with bit 7): its low byte, the data's length, the
	   status and the data's first byte */
	eventLogCommand[0] = serial.command & 0xff;
	eventLogCommand[1] = serial.dataLength;
	eventLogCommand[2] = serial.status;
	if (serial.dataLength != 0 && serial.data != NULL)
		eventLogCommand[3] = serial.data[0];
	else
		eventLogCommand[3] = 0;
	eventLogRecord(0x11, serial.command >> 8, eventLogCommand, 4);
#endif
	if SERIAL_IS_BINARY(serial) {
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
#if BOARD_USS1001	/* the slim's UART4 */
	return uart4Read(buf, size);
#else
	return uartRead(port, buf, size);
#endif
}

unsigned char serialWrite(int port, unsigned char *buf, int size)
{
#if BOARD_USS1001	/* the slim's UART4 */
	return uart4Write(buf, size);
#else
	return uartWrite(port, buf, size);
#endif
}

void serialFlush(int port)
{
	uartReset(port);
}

#if BOARD_USS1001	/* the slim's UART4 */
#define serialUartOpen(baud)	((unsigned char)uart4Open(baud))
#else
#define serialUartOpen(baud)	uart3Open(baud)
#endif

/* opens the UART again at baud, 13 ticks on */
unsigned char serialReopen(int baud)
{
	serialWait(13);
	return serialUartOpen(baud);
}

#if SOURCE_0103
/* 0x55s, which the other end's receiver takes its speed from */
static const unsigned char serialSync[3] = { 0x55, 0x55, 0x55 };

/* with the hardware info's bit 3, the sync bytes go out at once, 300 ticks before a command */
unsigned char serialOpen1(int baud)
{
	unsigned char info[4];

	serialUartOpen(baud);
	serialStart(1);
	serial.link = 1;
	getHardwareInfo(info);
	if (info[0] & 0x08) {
		serialWrite(SERIAL_PORT, (unsigned char *)serialSync, 3);
		WDTE = 0xac;
		serialWait(250);
		WDTE = 0xac;
		serialWait(50);
		WDTE = 0xac;
	}
	return 0;
}
#else
unsigned char serialOpen1(int baud)
{
	serial2SetLink(0);
	uart3Open(baud);
	serialStart(1);
	serial.link = 1;
	return 0;
}
#endif

unsigned char serialClose1(void)
{
	serialReset(0);
	serial.link = 0;
	return 0;
}

unsigned char serialOpen2(int baud)
{
	serialUartOpen(baud);
	serialStart(2);
#if !SOURCE_0103
	serial2SetLink(1);
	serial2SetP71(0);
#endif
	serial.link = 2;
	return 0;
}

unsigned char serialClose2(void)
{
#if !SOURCE_0103
	serial2SetP71(1);
#endif
	serialReset(0);
	serial.link = 0;
	return 0;
}

unsigned char serialOpen3(int baud)
{
	serialUartOpen(baud);
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

#if !SOURCE_0103	/* not in the slim's */
/* never called */
unsigned char serialSetP41(unsigned char level)
{
	P4.1 = level;
	return level & 1;
}
#endif

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
	if SERIAL_IS_BINARY(serial) {
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

	if (SERIAL_MODE_OF(serial) == 2) {
#if BOARD_SERIAL2
		i = serialNotifies2Count();
		while (i--) {
			if ((serial.command & 0x7fff) == serialNotifies2[i].command) {
				result = serialNotifies2[i].handler();
				serial.state = SERIAL_RECEIVE;
				return result;
			}
		}
#endif
#if FW_SINCE(0x0100060B)
	} else if (SERIAL_MODE_OF(serial) == 1 && serial.status == 0) {
#else
	} else if (SERIAL_MODE_OF(serial) == 1) {
#endif
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

void serialPostAnswer(void)
{
#ifdef ERNIE_UPDATER
	updEventSet(0x34);
#else
	task2SetFlag(3, 0x0008);
#endif
}

void serialPostTimeout(void)
{
#ifdef ERNIE_UPDATER
	updEventSet(0x32);
#else
	task2ISetFlag(0x0004);
#endif
}

/* never called */
void serialStartTimer(void)
{
	struct TimerHandler handler;

	handler.handler = serialPostTimeout;
	timerSetHandler(TIMER(0x10), &handler);
}

/* answers command 0x0105, if it waits to (status 0, no data) */
unsigned char serialAnswer0105(void)
{
#if SOURCE_0104
	/* 0x0104 (DOL-1002, the slim's 0x0105): command 0x01d5's too */
	if (serial.state != SERIAL_ANSWER_LATER ||
	    (serial.command != 0x0105 && serial.command != 0x01d5))
		return 0;
#else
	if (serial.state != SERIAL_ANSWER_LATER || serial.command != 0x0105)
		return 0;
#endif
	serial.command |= 0x8000;
	serial.status = 0;
	serial.data = NULL;
	serial.dataLength = 0;
	serialSetAnswer(SERIAL_ANSWER);
	serialPostAnswer();
	return 0;
}
