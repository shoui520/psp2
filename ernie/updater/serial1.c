/*
 * The serial link's mode 1, as the updater has it (an older revision of the application's):
 * the sixteen commands of serialCommands1, sorted by number, which serialCommand1 finds by
 * bisection and runs when the link's state lets it (a command's flags), and the notification
 * of serialNotifies1. Each answers from serial1Data, or with no data.
 */
#pragma sfr

#include <string.h>

#include "charge.h"
#include "chip.h"
#include "config.h"
#include "nvs.h"
#include "serial.h"
#include "serial1.h"
#include "serialauth.h"
#include "sleepfactor.h"
#include "spidev.h"
#include "state.h"
#include "updater.h"
#include "version.h"

extern const struct SerialCommand1 __far serialCommands1[];

/* the application's command 0x0900 passphrase (app/serial1.c): the updater has not the command,
   only the bytes */
const unsigned char serial1Password[16] = {
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
		unsigned char	flags;		/* resetFlags */
		unsigned char	reserved[3];
	} version;				/* command 0x0101's */
	struct ConfigVersions versions;		/* command 0x0140's */
} serial1Data;

unsigned char serial1State;
static unsigned char serial1Reserved[4];	/* read and written nowhere */

static unsigned int serial1Key;		/* SERIAL1_KEY_OPEN or SERIAL1_KEY_CLOSED */

void serial1Reset(void)
{
	serialLock(1);
	sleepFactorSet25(0);
	memset(serial1Buffer, 0, sizeof(serial1Buffer));
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

/* an input bit's handler: nothing in the updater */
void serial1InputHandler(void)
{
}

/* sleep factor 25 (sleepFactorSet25), in the low word of the factors: always 0; never
   called */
unsigned int serial1SleepFactor(void)
{
	return (unsigned int)(sleepFactorGet() & 0x02000000);
}

unsigned char serialCommands1Count(void)
{
	return 0x10;
}

unsigned char serialNotifies1Count(void)
{
	return 1;
}

/* 0x0100: nothing */
static unsigned char serial1Command0100(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0101: the hardware, the versions and getResetFlags's flags */
static unsigned char serial1Command0101(struct SerialBuffer *request, struct SerialBuffer *response)
{
	getHardwareInfo(serial1Data.version.hardwareInfo);
	getErnieVersion(serial1Data.version.version);
	getErnieDlVersion(serial1Data.version.dlVersion);
	serial1Data.version.flags = resetFlags;
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
	struct StateEvent event;
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = stateGet();
	mode = request->data[0];
	response->length = 0;
	if (mode == 0) {
		if (state != 0x0020) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		event.transition = 0x53;
		event.wakeMask = stateGetWakeMask();
		result = updStateEvent(&event);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 2) {
		if (state != 0x0004 && state != 0x0008) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		if (!chargeMayStart()) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		event.transition = 9;
		event.wakeMask = stateGetWakeMask();
		result = updStateEvent(&event);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 3) {
		if (state != 0x0004) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		event.transition = 0x11;
		event.wakeMask = stateGetWakeMask();
		result = updStateEvent(&event);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}

/* 0x0106: the system state, with bit 14 set (the updater's) */
static unsigned char serial1Command0106(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned int state;

	response->length = 2;
	state = stateGet();
	state |= 0x4000;
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

/* 0x0120: the SPI device's register 0x0120, 0x20 bytes (in mode 0x11, state 0x20); the
   updater only selects the device and answers serial1Data's zeros */
static unsigned char serial1Command0120(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	union PowerRequest mode;
	unsigned char send[3];

	result = 0;
	response->length = 0;
	mode = *stateGetWakeupFactor();
	if (mode.bits.mode != 0x11 || stateGet() != 0x0020) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x40;
	}
	memset(serial1Data.byte, 0, sizeof(serial1Data));
	send[0] = SPIDEV_READ;
	send[1] = 0x01;
	send[2] = 0x20;
	P90 = 0;
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
	union PowerRequest mode;
	union Serial1Word reg;

	result = 0;
	response->length = 0;
	mode = *stateGetWakeupFactor();
	if (mode.bits.mode != 0x11 || stateGet() != 0x0020) {
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

/* 0x0140: the banks' versions and counts (configGetVersions) */
static unsigned char serial1Command0140(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 8;
	result = configGetVersions(&serial1Data.versions);
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

/* 0x0141: the offset, the size (at most 0xff): a read of the settings (configRead) */
static unsigned char serial1Command0141(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word id;
	unsigned int size;
	unsigned char result;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	result = configRead(id.value, (unsigned char)(size < 0xff ? size : 0xff), serial1Data.byte);
	if (result == 0)
		response->length = size;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result == 0 ? 0 : result;
}

const struct SerialCommand1 __far serialCommands1[] = {
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
	{ 0x0300, serial1Command0300, 0 },
	{ 0x0301, serial1Command0301, 0 },
};

const struct SerialNotify __far serialNotifies1[] = {
	{ 0x0102, serial1Notify0102 },
};
