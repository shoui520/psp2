/*
 * The serial link's mode 1: the commands of serialCommands1, sorted by number, which
 * serialCommand1 finds by bisection and runs when the link's and the keys' state lets it (a
 * command's flags), and the notifications of serialNotifies1. Each answers from
 * serial1Data, or with no data.
 */
#pragma sfr
#pragma DI

#include <string.h>

#include "chip.h"
#include "config.h"
#include "csi.h"
#include "kernel.h"
#include "keepram.h"
#include "nvs.h"
#include "rtc.h"
#include "serial.h"
#include "serial1.h"
#include "serial2.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "spidev.h"
#include "sysmode.h"
#include "version.h"

extern void sub_2C2ED(int, int);
extern unsigned int sub_2AB46(void);
extern unsigned char sub_2AB49(int);
extern unsigned char sub_29C4B(void);
extern unsigned char sub_29CC0(void);
extern unsigned char sub_2D6DE(void);
extern unsigned char sub_2C66A(unsigned char *data);
extern unsigned char sub_2C6F0(unsigned int id, unsigned char size, unsigned char *data);
extern unsigned char sub_2C736(unsigned int id, unsigned char size, unsigned char *data);
extern unsigned char sub_2C7A1(void);
extern unsigned char sub_2CC93(void);
extern unsigned char sub_2C7EE(void);
extern unsigned char sub_2C998(unsigned int id, unsigned char size, unsigned char *data,
			       int flag);
extern unsigned char sub_2AB41(void);
extern unsigned long sub_2AA79(void);
extern unsigned char sub_2DAEA(unsigned char *data);
extern unsigned char sub_2DB73(void);
extern unsigned char sub_2DBDF(void);
extern unsigned char sub_2DC1F(unsigned int id, unsigned char *data, unsigned char *length);
extern unsigned char sub_2DC3C(unsigned int id, unsigned char size, unsigned char *data);
extern unsigned char sub_2DC57(unsigned int id);
extern unsigned char sub_2DC8B(void);
extern unsigned char sub_2D05F(void);
extern void sub_2C315(int, int);
extern unsigned char sub_11D4B(void);
extern unsigned char sub_261B5(void);
extern unsigned int sub_261A5(void);
extern unsigned char sub_261AC(void);
extern unsigned char sub_2C15B(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char sub_2C199(unsigned int offset, unsigned int size, unsigned char *buf);
extern void sub_2C229(unsigned char *base);

extern const struct SerialCommand1 serialCommands1[];

/* a level of commands 0x0950 and 0x0951 and its byte of keepSettings */
struct Serial1Level {
	unsigned char	channel;		/* 0x81-0x84 */
	unsigned char	*setting;
};

#define SERIAL1_LEVELS	4

extern const struct Serial1Level serial1Levels[SERIAL1_LEVELS];

/* the bytes command 0x0900 compares its request with (the PS Dev Wiki's "service 0x900 passphrase") */
static const unsigned char serial1Password[16] = {
	0x93, 0xce, 0x8e, 0xbe, 0xdf, 0x7f, 0x69, 0xa9,
	0x6f, 0x35, 0xdd, 0xe3, 0xbe, 0xcb, 0x97, 0xd5,
};

union Serial1Word {
	unsigned int	value;
	unsigned char	byte[2];
};

unsigned char serial1Buffer[0x28];		/* commands 0x0300 and 0x0301's */

/* the answers' data */
static union {
	unsigned char	byte[0x28];
	struct {
		unsigned char	hardwareInfo[4];
		unsigned char	version[4];
		unsigned char	dlVersion[4];
		unsigned char	flags;		/* sub_2D6DE's */
		unsigned char	reserved[3];
	} version;				/* command 0x0101's */
} serial1Data;

/* the operation task 4 runs (sub_2BBF2: 0x0180-0x0182, 0x0940-0x0945), 0 when none, and
   its arguments */
unsigned char serial1State;
union {
	unsigned char	byte[12];
	unsigned int	word[6];
} serial1Args;

static unsigned int serial1Key;		/* SERIAL1_KEY_OPEN or SERIAL1_KEY_CLOSED */

void serial1Reset(void)
{
	serialLock(1);
	sleepFactorSet25(0);
	serial1BufferClear();
	memset(&serial1Data, 0, sizeof(serial1Data));
	serialAuthInit();
	serial1State = 0;
	serial1Key = SERIAL1_KEY_CLOSED;
}

/* runs command; status 1 if no command has its number */
unsigned char serialCommand1(unsigned int command, struct SerialBuffer *request,
			     struct SerialBuffer *response)
{
	unsigned int low;
	unsigned int middle;
	unsigned int high;
	const struct SerialCommand1 *entry;
	unsigned char flags;

	response->length = 0;
	low = 0;
	high = serialCommands1Count();
	while (low < high) {
		middle = (low + high) >> 1;
		entry = &serialCommands1[middle];
		if (entry->command == command) {
			flags = entry->flags;
			if ((flags & SERIAL1_UNLOCKED) && serialIsLocked()) {
				serialSetAnswer(SERIAL_ANSWER);
				return 0x20;
			}
			if ((flags & SERIAL1_NVS_KEY2) && !serialAuthUnlocked1()) {
				serialSetAnswer(SERIAL_ANSWER);
				return 0x10;
			}
			if ((flags & SERIAL1_KEY) && serial1KeyOpen() == 0) {
				serialSetAnswer(SERIAL_ANSWER);
				return 1;
			}
			if ((flags & SERIAL1_NVS_KEY3) && !serialAuthUnlocked14()) {
				serialSetAnswer(SERIAL_ANSWER);
				return 0x10;
			}
			return entry->handler(request, response);
		}
		if (command < entry->command)
			high = middle;
		else
			low = middle + 1;
	}
	serialSetAnswer(SERIAL_ANSWER);
	return 1;
}

/* copies size bytes at offset of serial1Buffer into data; 4 if they are not all in it */
unsigned char serial1BufferRead(unsigned char offset, unsigned char size, unsigned char *data)
{
	if ((unsigned char)(offset + size) > sizeof(serial1Buffer))
		return 4;
	memcpy(data, &serial1Buffer[offset], size);
	return 0;
}

unsigned char serial1BufferWrite(unsigned char offset, unsigned char size, unsigned char *data)
{
	if ((unsigned char)(offset + size) > sizeof(serial1Buffer))
		return 4;
	memcpy(&serial1Buffer[offset], data, size);
	return 0;
}

unsigned char serial1BufferClear(void)
{
	memset(serial1Buffer, 0, sizeof(serial1Buffer));
	return 0;
}

/* an input bit's handler: event flag 3's bit 0x0001 */
void serial1InputHandler(void)
{
	sub_2C2ED(3, 0x0001);
}

/* sleep factor 25 (sleepFactorSet25), in the low word of the factors: always 0; never
   called */
unsigned int serial1SleepFactor(void)
{
	return (unsigned int)(sleepFactorGet() & 0x02000000);
}

unsigned char serialCommands1Count(void)
{
	return 0x31;
}

unsigned char serialNotifies1Count(void)
{
	return 3;
}

/* 0x0100: nothing */
static unsigned char serial1Command0100(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0101: the hardware, the versions and sub_2D6DE's flags */
static unsigned char serial1Command0101(struct SerialBuffer *request, struct SerialBuffer *response)
{
	getHardwareInfo(serial1Data.version.hardwareInfo);
	getErnieVersion(serial1Data.version.version);
	getErnieDlVersion(serial1Data.version.dlVersion);
	serial1Data.version.flags = sub_2D6DE();
	memset(serial1Data.version.reserved, 0, sizeof(serial1Data.version.reserved));
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Data.version);
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0102: the mode (0 text, 1 binary) and the baud (0 or 1) the link restarts in, at the
   notification */
static unsigned char serial1Command0102(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char mode;
	unsigned char baud;

	response->length = 0;
	if (request->length != 4) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x32;
	}
	mode = request->data[0];
	baud = request->data[1];
	if (mode == 0) {
		serial.nextMode = 1;
	} else if (mode == 1) {
		serial.nextMode = SERIAL_BINARY | 1;
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	if (baud == 0 || baud == 1) {
		baud += 2;
		serial.baud = baud;
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

static unsigned char serial1Notify0102(void)
{
	serialStart(serial.nextMode);
	return serialReopen(serial.baud);
}

/* 0x0103 */
static unsigned char serial1Command0103(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serialUnlock(1);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0104 */
static unsigned char serial1Command0104(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serialLock(1);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0105: a state change (0: 0x53 from state 0x20; 2: 9 from 4 or 8; 3: 0x11 from 4),
   answered when it is made */
static unsigned char serial1Command0105(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = sub_2AB46();
	mode = request->data[0];
	response->length = 0;
	if (mode == 0) {
		if (state != 0x0020) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		result = sub_2AB49(0x53);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 2) {
		if (state != 0x0004 && state != 0x0008) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		if (!sub_29C4B()) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		if (sub_29CC0()) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
		if (serial2TestAreaUsed() == 0 && !sub_2C658()->startCheckOff && serial2InTime() != 0) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x52;
		}
		result = sub_2AB49(9);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 3) {
		if (state != 0x0004) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		result = sub_2AB49(0x11);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}

/* 0x0106: the system state */
static unsigned char serial1Command0106(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned int state;

	response->length = 2;
	state = sub_2AB46();
	memcpy(serial1Data.byte, &state, response->length);
	response->data = serial1Data.byte;
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0110: serialAuthCommand on the request's 0x28 bytes, which answers in their place */
static unsigned char serial1Command0110(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	if (request->length != sizeof(serial1Data)) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x32;
	}
	memcpy(serial1Data.byte, request->data, sizeof(serial1Data));
	result = serialAuthCommand(serial1Data.byte);
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Data);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0120: the SPI device's register 0x0120, 0x20 bytes (in mode 0x11, state 0x20) */
static unsigned char serial1Command0120(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	struct SystemMode mode;
	unsigned char send[3];

	result = 0;
	response->length = 0;
	mode = *sub_2AB39();
	if (mode.mode != 0x11 || sub_2AB46() != 0x0020) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x40;
	}
	memset(serial1Data.byte, 0, sizeof(serial1Data));
	send[0] = SPIDEV_READ;
	send[1] = 0x01;
	send[2] = 0x20;
	P90 = 0;
	result = (unsigned char)csiTransfer(serial1Data.byte, 3 + 0x20, send, sizeof(send));
	dly_tsk(3);
	P90 = 1;
	response->data = &serial1Data.byte[3];
	response->length = 0x20;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0121: a register of the SPI device (not 5-0xfff), 4 bytes (in mode 0x11, state 0x20) */
static unsigned char serial1Command0121(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	struct SystemMode mode;
	union Serial1Word reg;

	result = 0;
	response->length = 0;
	mode = *sub_2AB39();
	if (mode.mode != 0x11 || sub_2AB46() != 0x0020) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x40;
	}
	reg.byte[1] = request->data[0];
	reg.byte[0] = request->data[1];
	if (reg.value >= 5 && reg.value < 0x1000) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x40;
	}
	memset(serial1Data.byte, 0x11, sizeof(serial1Data));
	spiDevRead(0x80, reg.value, serial1Data.byte);
	response->data = serial1Data.byte;
	response->length = 4;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0131: the NVS's read: the offset, the size */
static unsigned char serial1Command0131(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word offset;
	unsigned int size;
	unsigned char result;

	result = 0;
	response->length = 0;
	offset.byte[0] = request->data[0];
	offset.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	response->length = size;
	result = nvsRead(offset.value, size, serial1Data.byte, 0);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0132: the NVS's write: the offset, the size, the data */
static unsigned char serial1Command0132(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word offset;
	unsigned int size;
	unsigned char result;

	result = 0;
	response->length = 0;
	offset.byte[0] = request->data[0];
	offset.byte[1] = request->data[1];
	size = request->data[2];
	result = nvsWrite(offset.value, size, &request->data[3], 0);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0170: the NVS's state and info */
static unsigned char serial1Command0170(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	union Serial1Word info;

	response->data = serial1Data.byte;
	response->length = 5;
	memset(serial1Data.byte, 0, 5);
	serial1Data.byte[0] = nvsGetState();
	result = nvsGetInfo(&serial1Data.byte[1], &serial1Data.byte[2], &info.value);
	serial1Data.byte[3] = info.byte[0];
	serial1Data.byte[4] = info.byte[1];
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0300: the request's data into serial1Buffer; sets sleep factor 25 */
static unsigned char serial1Command0300(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serial1BufferWrite(0, (unsigned char)request->length, request->data);
	if (result == 0)
		sleepFactorSet25(1);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0301: serial1Buffer */
static unsigned char serial1Command0301(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Buffer);
	result = serial1BufferRead(0, sizeof(serial1Buffer), serial1Data.byte);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0140: sub_2C66A's eight bytes */
static unsigned char serial1Command0140(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 8;
	result = sub_2C66A(serial1Data.byte);
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0141: the id, the size (at most 0xff): sub_2C6F0's read */
static unsigned char serial1Command0141(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word id;
	unsigned int size;
	unsigned char result;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	result = sub_2C6F0(id.value, (unsigned char)(size < 0xff ? size : 0xff), serial1Data.byte);
	if (result == 0)
		response->length = size;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0142: the id, the size, the data: sub_2C736's write */
static unsigned char serial1Command0142(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word id;
	unsigned int size;
	unsigned char result;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	result = sub_2C736(id.value, (unsigned char)(size < 0xff ? size : 0xff), &request->data[3]);
	response->data = serial1Data.byte;
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0143 */
static unsigned char serial1Command0143(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 0;
	result = sub_2C7A1();
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0144 */
static unsigned char serial1Command0144(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 0;
	result = sub_2CC93();
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0145 */
static unsigned char serial1Command0145(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 0;
	result = sub_2C7EE();
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0146: the id, the size, the data: sub_2C998's write */
static unsigned char serial1Command0146(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	unsigned char size;
	unsigned char *data;
	union Serial1Word id;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	data = &request->data[3];
	response->data = serial1Data.byte;
	response->length = 0;
	result = sub_2C998(id.value, size, data, 0);
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0147: as 0x0146, sub_2C998's other write */
static unsigned char serial1Command0147(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	unsigned char size;
	unsigned char *data;
	union Serial1Word id;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	data = &request->data[3];
	response->data = serial1Data.byte;
	response->length = 0;
	result = sub_2C998(id.value, size, data, 1);
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0171: sub_2D6DE's flags, the reset's cause, sub_2AB41, the system mode, sub_2AA79 */
static unsigned char serial1Command0171(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned long state;

	response->data = serial1Data.byte;
	response->length = 9;
	serial1Data.byte[0] = sub_2D6DE();
	serial1Data.byte[1] = getResetCause();
	serial1Data.byte[2] = sub_2AB41();
	memcpy(&serial1Data.byte[3], sub_2AB39(), 2);
	state = sub_2AA79();
	memcpy(&serial1Data.byte[5], &state, 4);
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0172: the clock, the alarm and its mode */
static unsigned char serial1Command0172(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->data = serial1Data.byte;
	response->length = 9;
	memcpy(&serial1Data.byte[0], getClock().byte, 4);
	memcpy(&serial1Data.byte[4], getAlarm().byte, 4);
	serial1Data.byte[8] = getAlarmMode();
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* the status of 0x0150-0x0152's result */
static unsigned char serial1Status015x(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 2:
		return 0x94;
	case 3:
		return 0x95;
	default:
		return 0x92;
	}
}

/* 0x0150: sub_2DAEA's eight bytes */
static unsigned char serial1Command0150(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->data = serial1Data.byte;
	result = sub_2DAEA(serial1Data.byte);
	result = serial1Status015x(result);
	if (result == 0)
		response->length = 8;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0151 */
static unsigned char serial1Command0151(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	result = sub_2DB73();
	result = serial1Status015x(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0152 */
static unsigned char serial1Command0152(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	result = sub_2DBDF();
	result = serial1Status015x(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0153: the id: sub_2DC1F's read */
static unsigned char serial1Command0153(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int id;
	unsigned char length;

	memcpy(&id, request->data, 2);
	response->data = serial1Data.byte;
	result = sub_2DC1F(id, response->data, &length);
	result = serial1Status015x(result);
	if (result == 0)
		response->length = length;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0154: the id, the size: sub_2DC3C's read */
static unsigned char serial1Command0154(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	union Serial1Word id;
	unsigned char size;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	result = sub_2DC3C(id.value, size, response->data);
	result = serial1Status015x(result);
	if (result == 0)
		response->length = size;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0155: the id: sub_2DC57 */
static unsigned char serial1Command0155(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int id;

	memcpy(&id, request->data, 2);
	result = sub_2DC57(id);
	result = serial1Status015x(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0156 */
static unsigned char serial1Command0156(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	result = sub_2DC8B();
	result = serial1Status015x(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0160: in state 4, clears (1) the NVS's syscon area (mask bit 0, then sub_2DC8B) and
   sub_2D05F's (bit 1), or (2) runs nvsFormat and sub_2DC8B */
static unsigned char serial1Command0160(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char what;
	unsigned char mask;
	unsigned int offset;
	unsigned char *buffer;
	unsigned int state;

	state = sub_2AB46();
	what = request->data[0];
	mask = request->data[1];
	if (state != 0x0004) {
		result = 0x90;
		goto done;
	}
	if (mask == 0)
		mask = 0xff;
	if (what == 1) {
		if (mask & 0x01) {
			sub_2DBDF();
			buffer = serial1Data.byte;
			memset(buffer, 0xff, 0x20);
			for (offset = 0; offset < 0x40; offset += 0x20) {
				result = nvsWrite(offset, 0x20, buffer, 1);
				if (result != 0) {
					result = 0x91;
					goto done;
				}
			}
			result = sub_2DC8B();
			if (result != 0) {
				result = 0x92;
				goto done;
			}
		}
		if (mask & 0x02) {
			result = sub_2D05F();
			if (result != 0) {
				result = 0x93;
				goto done;
			}
		}
	} else if (what == 2) {
		result = nvsFormat();
		if (result != 0) {
			result = 0x91;
			goto done;
		}
		result = sub_2DC8B();
		if (result != 0) {
			result = 0x92;
			goto done;
		}
	} else {
		result = 0x33;
	}
done:
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0161: the system's reset, at the notification */
static unsigned char serial1Command0161(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

static unsigned char serial1Notify0161(void)
{
	DI();
	setResetCause(0x40);
	resetSystem();
	return 0;
}

/* 0x0162: the chip's reset, at the notification */
static unsigned char serial1Command0162(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

static unsigned char serial1Notify0162(void)
{
	DI();
	resetChip();
	return 0;
}

/* 0x0180-0x0182: task 4's operations 7-9, answered when they are done; status 0x61 while one
   runs */
static unsigned char serial1Command0180(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 7;
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serialSetAnswer(SERIAL_ANSWER_LATER);
	sub_2C315(4, 0x4000);
	return result;
}

static unsigned char serial1Command0181(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 8;
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serialSetAnswer(SERIAL_ANSWER_LATER);
	sub_2C315(4, 0x4000);
	return result;
}

static unsigned char serial1Command0182(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 9;
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serialSetAnswer(SERIAL_ANSWER_LATER);
	sub_2C315(4, 0x4000);
	return result;
}

/* 0x0190 */
static unsigned char serial1Command0190(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int value;

	response->data = serial1Data.byte;
	response->length = 8;
	serial1Data.byte[0] = sub_11D4B();
	serial1Data.byte[1] = sub_261B5();
	value = sub_261A5();
	memcpy(&serial1Data.byte[2], &value, 2);
	serial1Data.byte[4] = sub_261AC();
	serial1Data.byte[5] = (unsigned char)serial2TestAreaUsed();
	serial1Data.byte[6] = (unsigned char)serial2GetArmed();
	serial1Data.byte[7] = (unsigned char)serial2InTime();
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

int serial1KeyOpen(void)
{
	return serial1Key == SERIAL1_KEY_OPEN;
}

/* 0x0900: opens the commands marked SERIAL1_KEY when the request is serial1Password, closes
   them otherwise */
static unsigned char serial1Command0900(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char *data;

	data = request->data;
	if (memcmp(data, serial1Password, sizeof(serial1Password)) == 0)
		serial1Key = SERIAL1_KEY_OPEN;
	else
		serial1Key = SERIAL1_KEY_CLOSED;
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0901: closes them */
static unsigned char serial1Command0901(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	serial1Key = SERIAL1_KEY_CLOSED;
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0940, 0x0942, 0x0944: task 4's operations 1, 3 and 5, with a byte */
static unsigned char serial1Command0940(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		switch (serial.command) {
		case 0x0940:
			serial1State = 1;
			break;
		case 0x0942:
			serial1State = 3;
			break;
		case 0x0944:
			serial1State = 5;
			break;
		}
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serial1Args.byte[0] = request->data[0];
	serialSetAnswer(SERIAL_ANSWER_LATER);
	sub_2C315(4, 0x4000);
	return result;
}

/* 0x0941, 0x0943, 0x0945: operations 2, 4 and 6, with three bytes */
static unsigned char serial1Command0941(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		switch (serial.command) {
		case 0x0941:
			serial1State = 2;
			break;
		case 0x0943:
			serial1State = 4;
			break;
		case 0x0945:
			serial1State = 6;
			break;
		}
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serial1Args.byte[0] = request->data[0];
	serial1Args.byte[1] = request->data[1];
	serial1Args.byte[2] = request->data[2];
	serialSetAnswer(SERIAL_ANSWER_LATER);
	sub_2C315(4, 0x4000);
	return result;
}

/* 0x0950: a channel's level, as 1400 less 10 for each step above sub_2C229's base, most
   significant byte first */
static unsigned char serial1Command0950(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char i;
	unsigned char channel;
	unsigned char step;
	unsigned char level;
	unsigned char base;
	union Serial1Word value;

	response->length = 0;
	if (request->length != 1) {
		result = 0x32;
		goto done;
	}
	channel = request->data[0];
	if (channel < 0x81 || channel >= 0x85) {
		result = 0x33;
		goto done;
	}
	for (i = 0; i < SERIAL1_LEVELS; i++) {
		if (serial1Levels[i].channel == channel) {
			result = sub_2C15B(serial1Levels[i].setting - keepSettings, 1, &level);
			break;
		}
	}
	sub_2C229(&base);
	step = level - base;
	value.value = 1400 - step * 10;
	response->length = 2;
	response->data = serial1Data.byte;
	response->data[0] = value.byte[1];
	response->data[1] = value.byte[0];
done:
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0951: sets a channel's level: 770-1400, a multiple of 10 */
static unsigned char serial1Command0951(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char i;
	unsigned char channel;
	unsigned char step;
	unsigned char level;
	unsigned char base;
	union Serial1Word value;

	response->length = 0;
	if (request->length != 4) {
		result = 0x32;
		goto done;
	}
	channel = request->data[0];
	if (channel < 0x81 || channel >= 0x85) {
		result = 0x33;
		goto done;
	}
	value.byte[1] = request->data[2];
	value.byte[0] = request->data[3];
	if (value.value < 770 || value.value > 1400 || value.value % 10 != 0) {
		result = 0x33;
		goto done;
	}
	step = (unsigned char)((1400 - value.value) / 10);
	sub_2C229(&base);
	level = step + base;
	for (i = 0; i < SERIAL1_LEVELS; i++) {
		if (serial1Levels[i].channel == channel) {
			result = sub_2C199(serial1Levels[i].setting - keepSettings, 1, &level);
			break;
		}
	}
done:
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* the commands, by number */
const struct SerialCommand1 serialCommands1[] = {
	{ 0x0100, serial1Command0100, 0 },
	{ 0x0101, serial1Command0101, 0 },
	{ 0x0102, serial1Command0102, 0 },
	{ 0x0103, serial1Command0103, 0 },
	{ 0x0104, serial1Command0104, 0 },
	{ 0x0105, serial1Command0105, SERIAL1_UNLOCKED },
	{ 0x0106, serial1Command0106, 0 },
	{ 0x0110, serial1Command0110, SERIAL1_UNLOCKED },
	{ 0x0120, serial1Command0120, SERIAL1_UNLOCKED },
	{ 0x0121, serial1Command0121, SERIAL1_UNLOCKED },
	{ 0x0131, serial1Command0131, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY2 },
	{ 0x0132, serial1Command0132, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY2 },
	{ 0x0140, serial1Command0140, 0 },
	{ 0x0141, serial1Command0141, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0142, serial1Command0142, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0143, serial1Command0143, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0144, serial1Command0144, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0145, serial1Command0145, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0146, serial1Command0146, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0147, serial1Command0147, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0150, serial1Command0150, SERIAL1_UNLOCKED },
	{ 0x0151, serial1Command0151, SERIAL1_UNLOCKED },
	{ 0x0152, serial1Command0152, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0153, serial1Command0153, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0154, serial1Command0154, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0155, serial1Command0155, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0156, serial1Command0156, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0160, serial1Command0160, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0161, serial1Command0161, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0162, serial1Command0162, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0170, serial1Command0170, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0171, serial1Command0171, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0172, serial1Command0172, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0180, serial1Command0180, SERIAL1_UNLOCKED },
	{ 0x0181, serial1Command0181, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0182, serial1Command0182, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0190, serial1Command0190, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0300, serial1Command0300, 0 },
	{ 0x0301, serial1Command0301, 0 },
	{ 0x0900, serial1Command0900, SERIAL1_NVS_KEY3 },
	{ 0x0901, serial1Command0901, SERIAL1_NVS_KEY3 },
	{ 0x0940, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0941, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0942, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0943, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0944, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0945, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0950, serial1Command0950, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0951, serial1Command0951, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
};

const struct SerialNotify serialNotifies1[] = {
	{ 0x0102, serial1Notify0102 },
	{ 0x0161, serial1Notify0161 },
	{ 0x0162, serial1Notify0162 },
};

const struct Serial1Level serial1Levels[SERIAL1_LEVELS] = {
	{ 0x81, &keepSettings[0x28] },
	{ 0x82, &keepSettings[0x29] },
	{ 0x83, &keepSettings[0x2a] },
	{ 0x84, &keepSettings[0x2b] },
};
