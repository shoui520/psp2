/*
 * The serial link's mode 1: the commands of serialCommands1, sorted by number, which
 * serialCommand1 finds by bisection and runs when the link's and the keys' state lets it (a
 * command's flags), and the notifications of serialNotifies1. Each answers from
 * serial1Data, or with no data.
 */
#pragma sfr
#pragma DI

#include <string.h>

#include "ernie.h"
#include "battery.h"
#include "bbmc.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "csi.h"
#if BOARD_USS1001
#include "analog.h"
#include "bstore.h"
#include "chipid.h"
#include "eventlog.h"
#include "inputreq.h"
#endif
#include "keepram.h"
#include "kernel.h"
#include "log.h"
#include "nvs.h"
#include "param.h"
#include "powerseq.h"
#include "rtc.h"
#include "serial.h"
#include "serial1.h"
#include "serial2.h"
#include "serial4.h"
#include "serialauth.h"
#if !FW_SINCE(0x00090903)
#include "serialreq.h"
#endif
#include "sleepfactor.h"
#include "spidev.h"
#include "state.h"
#include "sysinfo.h"
#include "task.h"
#include "task5.h"
#include "touch.h"
#if BOARD_USS1001
#include "updatereq.h"
#endif
#include "version.h"

extern unsigned char intp11Low(void);

#if FW_SINCE(0x00090804)
extern const struct SerialCommand1 serialCommands1[];
#else
/* 0x00090206's table has no flags: serialDispatch searches it as mode 2's, and the commands
   test the link's lock (status 0x20) themselves */
extern const struct SerialCommand serialCommands1[];

#define SERIAL1_UNLOCKED_CHECK()					\
	if (serialIsLocked()) {						\
		serialSetAnswer(SERIAL_ANSWER);				\
		return 0x20;						\
	}
#endif

/* command 0x0105's tests of the int charge helpers: before 0x01000104 and since 0x0100060B they read only the low byte */
#if FW_SINCE(0x01000104) && !FW_SINCE(0x0100060B)
#define CHARGE_TEST(call)	(call)
#else
#define CHARGE_TEST(call)	((unsigned char)(call))
#endif

#if !FW_SINCE(0x0100060B)
/* a level of commands 0x0950 and 0x0951 and its byte of the parameter block */
struct Serial1Level {
	unsigned char	channel;		/* 0x81-0x84 */
	unsigned char	*setting;
};

#define SERIAL1_LEVELS	4

extern const struct Serial1Level serial1Levels[SERIAL1_LEVELS];
#endif

#if FW_SINCE(0x00090804)
/* the bytes command 0x0900 compares its request with (the PS Dev Wiki's "service 0x900 passphrase") */
static const unsigned char serial1Password[16] = {
	0x93, 0xce, 0x8e, 0xbe, 0xdf, 0x7f, 0x69, 0xa9,
	0x6f, 0x35, 0xdd, 0xe3, 0xbe, 0xcb, 0x97, 0xd5,
};
#endif

union Serial1Word {
	unsigned int	value;
	unsigned char	byte[2];
};

#if !FW_SINCE(0x00090903)
/* request 0x20's (its code follows serial1Reset) */
struct HostRequest serialRequest;
unsigned char serialRequestData[0x1c];		/* the offset, the size, the data; the answer */
#endif

unsigned char serial1Buffer[0x28];		/* commands 0x0300 and 0x0301's */

/* the answers' data */
static union {
	unsigned char	byte[0x28];
	struct {
		unsigned char	hardwareInfo[4];
		unsigned char	version[4];
		unsigned char	dlVersion[4];
#if FW_SINCE(0x00090206)
		unsigned char	flags;		/* getResetFlags's */
#if BOARD_USS1001
		unsigned char	resetCause;	/* getResetCause's */
		unsigned char	powerCause;	/* stateGetCause's */
		unsigned char	wakeupFactor;	/* stateGetWakeupFactor's first byte */
#else
		unsigned char	reserved[3];
#endif
#else
		unsigned char	reserved[4];	/* 0x00090009 reports no reset flags */
#endif
	} version;				/* command 0x0101's */
	struct ConfigVersions versions;		/* command 0x0140's */
#if NVS_INFO_MAP
	struct {
		unsigned char	state;
		unsigned char	mark;
		unsigned char	missing;
		unsigned char	reserved;
		unsigned long	moves;
		struct NvsMap	map;
	} nvsInfo;				/* command 0x0170's */
#endif
#if BOARD_USS1001
	struct {
		unsigned char	timeStamp[12];	/* getTimeStamp's */
		unsigned char	hardwareInfo2[16];
	} build;				/* command 0x0107's */
#endif
} serial1Data;

unsigned char serial1State;
union Serial1Args serial1Args;

#if FW_SINCE(0x00090804)
static unsigned int serial1Key;		/* SERIAL1_KEY_OPEN or SERIAL1_KEY_CLOSED */
#endif

void serial1Reset(void)
{
	serialLock(1);
	sleepFactorSet25(0);
#if FW_SINCE(0x00090903)
	serial1BufferClear();
#else
	memset(serial1Buffer, 0, sizeof(serial1Buffer));
#endif
	memset(&serial1Data, 0, sizeof(serial1Data));
	serialAuthInit();
	serial1State = 0;
#if FW_SINCE(0x00090804)
	serial1Key = SERIAL1_KEY_CLOSED;
#endif
}

#if !FW_SINCE(0x00090903)
/* 0x00090804 has request 0x20 here, after serial1Reset; it has its own object (serialreq.c)
   since 0x00090903 */
#include "serialreq.c"
#endif

#if FW_SINCE(0x00090804)
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
#if BOARD_USS1001
				return 0xff;
#else
				return 1;
#endif
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
#endif

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

#if FW_SINCE(0x00090903)
unsigned char serial1BufferClear(void)
{
	memset(serial1Buffer, 0, sizeof(serial1Buffer));
	return 0;
}
#endif

/* an input bit's handler: event flag 3's bit 0x0001 */
void serial1InputHandler(void)
{
	task2SetFlag(3, 0x0001);
}

/* sleep factor 25 (sleepFactorSet25), in the low word of the factors: always 0; never
   called */
unsigned int serial1SleepFactor(void)
{
	return (unsigned int)(sleepFactorGet() & 0x02000000);
}

#if FW_SINCE(0x01000104)
unsigned char serialCommands1Count(void)
{
#if BOARD_USS1001 && FW_SINCE(0x01030511)
	return 0x5e;
#elif BOARD_USS1001
	return 0x59;	/* 0x01030505: no 0x0961-0x0965 */
#elif FW_SINCE(0x0100060B)
	return 0x37;
#elif BOARD_KERNEL_COMMANDS
	return 0x38;
#else
	return 0x31;
#endif
}

unsigned char serialNotifies1Count(void)
{
#if FW_SINCE(0x0100060B)
	return 4;
#else
	return 3;
#endif
}
#else
/* before 0x01000104 no commands 0x0161, 0x0162, 0x0172 and 0x0190 (0x00090804: no 0x0180-0x0182,
   and 0x0910-0x0916) */
unsigned char serialCommands1Count(void)
{
#if BOARD_KERNEL_COMMANDS && FW_SINCE(0x00090903)
	return 0x34;
#elif FW_SINCE(0x00090903)
	return 0x2d;
#elif FW_SINCE(0x00090804)
	return 0x31;
#elif FW_SINCE(0x00090009)
	return 0x25;
#else
	return 0x1f;
#endif
}

unsigned char serialNotifies1Count(void)
{
	return 1;
}
#endif

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
#if BOARD_USS1001
	union PowerRequest factor;

#endif
	getHardwareInfo(serial1Data.version.hardwareInfo);
	getErnieVersion(serial1Data.version.version);
	getErnieDlVersion(serial1Data.version.dlVersion);
#if BOARD_USS1001
	serial1Data.version.flags = getResetFlags();
	serial1Data.version.resetCause = getResetCause();
	serial1Data.version.powerCause = stateGetCause();
	factor = *stateGetWakeupFactor();
	serial1Data.version.wakeupFactor = factor.byte[0];
#else
#if FW_SINCE(0x01000107)
	serial1Data.version.flags = getResetFlags();
#elif FW_SINCE(0x00090206)
	serial1Data.version.flags = resetFlags;
#endif
	memset(serial1Data.version.reserved, 0, sizeof(serial1Data.version.reserved));
#endif
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

#if BOARD_USS1001
/* the link's result (serialLock, serialUnlock, serialReopen) as an answer's: 0x20-0x25, 0xff
   for any other */
static unsigned char serial1SerialResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 0x10:
		return 0x20;
	case 0x11:
		return 0x21;
	case 0x12:
		return 0x22;
	case 0x13:
		return 0x23;
	case 0x14:
		return 0x24;
	case 0x15:
		return 0x25;
	default:
		return 0xff;
	}
}

/* stateTransition's result as an answer's: 0x54-0x56 (0x01030505: 0x54 and 0x55), 0xff for any
   other */
static unsigned char serial1StateResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 1:
		return 0x54;
	case 2:
		return 0x55;
#if FW_SINCE(0x01030511)	/* 0x01030505's stateTransition has no result 3 */
	case 3:
		return 0x56;
#endif
	default:
		return 0xff;
	}
}

/* serialAuthCommand's result as an answer's: 0x10 and 0x11, 0xff for any other */
static unsigned char serial1AuthResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 0x10:
		return 0x10;
	case 0x11:
		return 0x11;
	default:
		return 0xff;
	}
}

static unsigned char serial1Notify0102(void)
{
	unsigned char result = 0;

	serialStart(serial.nextMode);
	result = serialReopen(serial.baud);
	return serial1SerialResult(result);
}
#else
static unsigned char serial1Notify0102(void)
{
	serialStart(serial.nextMode);
	return serialReopen(serial.baud);
}
#endif

/* 0x0103 */
static unsigned char serial1Command0103(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serialUnlock(1);
	serialSetAnswer(SERIAL_ANSWER);
#if BOARD_USS1001
	return serial1SerialResult(result);
#else
	return result;
#endif
}

/* 0x0104 */
static unsigned char serial1Command0104(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serialLock(1);
	serialSetAnswer(SERIAL_ANSWER);
#if BOARD_USS1001
	return serial1SerialResult(result);
#else
	return result;
#endif
}

#if BOARD_USS1001
/* 0x0105: a state change (0: 0x53 from state 0x20, 0x200 or 0x400; 2: 9 from 4 or 8; 3: 0x11
   from 4; 0x10 and 0x11 are refused), answered when it is made */
static unsigned char serial1Command0105(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = stateGet();
	mode = request->data[0];
	response->length = 0;
	if (mode == 0) {
		if (state != 0x0020 && state != 0x0200 && state != 0x0400) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		result = stateTransition(0x53);
		result = serial1StateResult(result);
		if (result != 0)
			serialSetAnswer(SERIAL_ANSWER);
		else
			serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 2) {
		if (state != 0x0004 && state != 0x0008) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		if (!CHARGE_TEST(chargeMayStart())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		if (CHARGE_TEST(chargeAlarm0On())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
		result = stateTransition(9);
		result = serial1StateResult(result);
		if (result != 0)
			serialSetAnswer(SERIAL_ANSWER);
		else
			serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 3) {
		if (state != 0x0004) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		if (!CHARGE_TEST(chargeMayStart())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		if (CHARGE_TEST(chargeAlarm0On())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
		result = stateTransition(0x11);
		result = serial1StateResult(result);
		if (result != 0)
			serialSetAnswer(SERIAL_ANSWER);
		else
			serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 0x10) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	} else if (mode == 0x11) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}
#else
/* 0x0105: a state change (0: 0x53 from state 0x20, 0x00090206 also 8; 2: 9 from 4 or 8; 3: 0x11
   from 4), answered when it is made */
static unsigned char serial1Command0105(struct SerialBuffer *request, struct SerialBuffer *response)
{
	STATE_TRANSITION_EVENT
	unsigned char mode;
	unsigned char result;
	unsigned int state;

	result = 0;
	state = stateGet();
	mode = request->data[0];
	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	if (mode == 0) {
#if FW_SINCE(0x00090804)
		if (state != 0x0020) {
#else
		if (state != 0x0020 && state != 0x0008) {	/* 0x00090206: or state 8 */
#endif
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
		result = STATE_TRANSITION(0x53);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 2) {
		if (state != 0x0004 && state != 0x0008) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
#if FW_SINCE(0x00090804)	/* 0x00090206 does not ask the charger */
		if (!CHARGE_TEST(chargeMayStart())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
#endif
#if FW_SINCE(0x01000104)
		if (CHARGE_TEST(chargeAlarm0On())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
		if (serial2TestAreaUsed() == 0 && !configGet()->test.bits.startCheckOff && serial2InTime() != 0) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x52;
		}
#endif
		result = STATE_TRANSITION(9);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else if (mode == 3) {
		if (state != 0x0004) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x50;
		}
#if FW_SINCE(0x0100060B)
		if (!CHARGE_TEST(chargeMayStart())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x51;
		}
		if (CHARGE_TEST(chargeAlarm0On())) {
			serialSetAnswer(SERIAL_ANSWER);
			return 0x53;
		}
#endif
		result = STATE_TRANSITION(0x11);
		serialSetAnswer(SERIAL_ANSWER_LATER);
	} else {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	return result;
}

#endif

/* 0x0106: the system state */
static unsigned char serial1Command0106(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned int state;

	response->length = 2;
	state = stateGet();
	memcpy(serial1Data.byte, &state, response->length);
	response->data = serial1Data.byte;
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

#if BOARD_USS1001
/* 0x0107: the build's time stamp and the hardware info 2 */
static unsigned char serial1Command0107(struct SerialBuffer *request, struct SerialBuffer *response)
{
	getTimeStamp(serial1Data.build.timeStamp);
	getHardwareInfo2(serial1Data.build.hardwareInfo2);
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Data.build);
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}
#endif

/* 0x0110: serialAuthCommand on the request's 0x28 bytes, which answers in their place */
static unsigned char serial1Command0110(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	if (request->length != sizeof(serial1Data)) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x32;
	}
	memcpy(serial1Data.byte, request->data, sizeof(serial1Data));
	result = serialAuthCommand(serial1Data.byte);
#if BOARD_USS1001
	result = serial1AuthResult(result);
#endif
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Data);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0120: the SPI device's register 0x0120, 0x20 bytes (in mode 0x11, state 0x20) */
static unsigned char serial1Command0120(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	union PowerRequest mode;
	unsigned char send[3];

	result = 0;
	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
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
	result = (unsigned char)csiTransfer(serial1Data.byte, 3 + 0x20, send, sizeof(send));
#if BOARD_USS1001
	if (result != 0)
		result = 0xff;
#endif
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
	union PowerRequest mode;
	union Serial1Word reg;

	result = 0;
	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
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

#if BOARD_USS1001
/* the flash library's result as an answer's: 0xc0-0xc9, 0xff for any other */
static unsigned char serial1FlashResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 1:
		return 0xc0;
	case 5:
		return 0xc1;
	case 0x10:
		return 0xc2;
	case 0x1a:
		return 0xc3;
	case 0x1b:
		return 0xc4;
	case 0x1c:
		return 0xc6;
	case 0x1d:
		return 0xc7;
	case 0x1e:
		return 0xc8;
	case 0x1f:
		return 0xc9;
	default:
		return 0xff;
	}
}

/* the NVS's result as an answer's: 0xa0-0xae, else the flash library's */
static unsigned char serial1NvsResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 2:
		return 0xa0;
	case 3:
		return 0xa1;
	case 4:
		return 0xa2;
	case 6:
		return 0xa3;
	case 7:
		return 0xa4;
	case 8:
		return 0xa5;
	case 9:
		return 0xa6;
	case 0x0a:
		return 0xa7;
	case 0x0b:
		return 0xa8;
	case 0x20:
		return 0xa9;
	case 0x21:
		return 0xaa;
	case 0x22:
		return 0xab;
	case 0x23:
		return 0xac;
	case 0x30:
		return 0xad;
	case 0x31:
		return 0xae;
	default:
		return serial1FlashResult(result);
	}
}
#endif

/* 0x0131: the NVS's read: the offset, the size */
static unsigned char serial1Command0131(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word offset;
	unsigned int size;
	unsigned char result;

	result = 0;
	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
	if (!serialAuthUnlocked1()) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x10;
	}
#endif
	offset.byte[0] = request->data[0];
	offset.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	response->length = size;
	result = nvsRead(offset.value, size, serial1Data.byte, 0);
	serialSetAnswer(SERIAL_ANSWER);
#if BOARD_USS1001
	return serial1NvsResult(result);
#else
	return result;
#endif
}

/* 0x0132: the NVS's write: the offset, the size, the data */
static unsigned char serial1Command0132(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word offset;
	unsigned int size;
	unsigned char result;

	result = 0;
	response->length = 0;
#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
	if (!serialAuthUnlocked1()) {
		serialSetAnswer(SERIAL_ANSWER);
		return 0x10;
	}
#endif
	offset.byte[0] = request->data[0];
	offset.byte[1] = request->data[1];
	size = request->data[2];
	result = nvsWrite(offset.value, size, &request->data[3], 0);
	serialSetAnswer(SERIAL_ANSWER);
#if BOARD_USS1001
	return serial1NvsResult(result);
#else
	return result;
#endif
}

#if FW_SINCE(0x00090804)
/* 0x0170: the NVS's state and info */
static unsigned char serial1Command0170(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
#if !NVS_INFO_MAP
	union Serial1Word info;
#endif

#if NVS_INFO_MAP
	response->data = serial1Data.byte;
	response->length = sizeof(serial1Data.nvsInfo);
	memset(serial1Data.byte, 0, response->length);
	serial1Data.nvsInfo.state = nvsGetState();
	result = nvsGetInfo(&serial1Data.nvsInfo.mark, &serial1Data.nvsInfo.missing,
			    &serial1Data.nvsInfo.moves, &serial1Data.nvsInfo.map);
	serialSetAnswer(SERIAL_ANSWER);
	return serial1NvsResult(result);
#else
	response->data = serial1Data.byte;
	response->length = 5;
	memset(serial1Data.byte, 0, 5);
	serial1Data.byte[0] = nvsGetState();
	result = nvsGetInfo(&serial1Data.byte[1], &serial1Data.byte[2], &info.value);
	serial1Data.byte[3] = info.byte[0];
	serial1Data.byte[4] = info.byte[1];
	serialSetAnswer(SERIAL_ANSWER);
#if BOARD_USS1001
	return serial1NvsResult(result);	/* 0x01030505 */
#else
	return result == 0 ? 0 : result;
#endif
#endif
}
#endif

/* 0x0300: the request's data into serial1Buffer; sets sleep factor 25 */
static unsigned char serial1Command0300(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->length = 0;
	result = serial1BufferWrite(0, (unsigned char)request->length, request->data);
#if BOARD_USS1001
	if (result != 0)
		result = 0x99;
	else
		sleepFactorSet25(1);
#else
	if (result == 0)
		sleepFactorSet25(1);
#endif
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
#if BOARD_USS1001
	if (result != 0)
		result = 0x99;
#endif
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

#if BOARD_USS1001
/* the settings' result (config.c) as an answer's: 0xd0-0xda, else the flash library's */
static unsigned char serial1ConfigResult(unsigned char result)
{
	switch (result) {
	case 0:
		return 0;
	case 2:
		return 0xd0;
	case 3:
		return 0xd1;
	case 6:
		return 0xd2;
	case 7:
		return 0xd3;
	case 8:
		return 0xd4;
	case 0x20:
		return 0xd5;
	case 0x21:
		return 0xd6;
	case 0x22:
		return 0xd7;
	case 0x3c:
		return 0xd8;
	case 0x3b:
		return 0xd9;
	case 0x3e:
		return 0xda;
	default:
		return serial1FlashResult(result);
	}
}

#define SERIAL1_CONFIG_RESULT(result)	serial1ConfigResult(result)
#else
#define SERIAL1_CONFIG_RESULT(result)	((result) == 0 ? 0 : (result))
#endif

#if FW_SINCE(0x00090009) /* 0x00080504 has no settings commands */
/* 0x0140: the banks' versions and counts (configGetVersions) */
static unsigned char serial1Command0140(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

	response->data = serial1Data.byte;
	response->length = 8;
	result = configGetVersions(&serial1Data.versions);
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0141: the offset, the size (at most 0xff): a read of the settings (configRead) */
static unsigned char serial1Command0141(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word id;
	unsigned int size;
	unsigned char result;

#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
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
	return SERIAL1_CONFIG_RESULT(result);
}

#endif

#if FW_SINCE(0x0100060B)
/* 0x0168: sleepFactorGetOffInfo's */
static unsigned char serial1Command0168(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 4;
	response->data = serial1Data.byte;
	sleepFactorGetOffInfo(serial1Data.byte);
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}
#endif

#if BOARD_USS1001
/* 0x01d2: updaterGetStatus's 8 bytes */
static unsigned char serial1Command01d2(struct SerialBuffer *request, struct SerialBuffer *response)
{
	updaterGetStatus(serial1Data.byte);
	response->length = 8;
	response->data = serial1Data.byte;
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x0108: a word, 0: task 4's operation 0x1a, answered when it is done; status 0x61 while
   one runs, 0x33 for any other word */
static unsigned char serial1Command0108(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	union Serial1Word value;

	value.byte[0] = request->data[0];
	value.byte[1] = request->data[1];
	if (value.value == 0) {
		if (serial1State == 0) {
			serial1State = 0x1a;
			serialSetAnswer(SERIAL_ANSWER_LATER);
			task5SetFlag(4, 0x4000);
		} else {
			response->length = 0;
			serialSetAnswer(SERIAL_ANSWER);
			result = 0x61;
		}
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x33;
	}
	return result;
}

/* 0x0109: the state record (mode 0) or the last two states' (mode 1), 0x20 bytes */
static unsigned char serial1Command0109(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char mode;

	memset(serial1Data.byte, 0, 0x20);
	response->length = 0x20;
	mode = request->data[0];
	if (mode == 0) {
		stateGetRecord(serial1Data.byte);
	} else if (mode == 1) {
		stateGetHistory(serial1Data.byte);
	} else {
		response->length = 0;
		result = 0x33;
	}
	response->data = serial1Data.byte;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif

#if FW_SINCE(0x00090009)
/* 0x0142: the offset, the size, the data: a write to the open copy (configWrite) */
static unsigned char serial1Command0142(struct SerialBuffer *request, struct SerialBuffer *response)
{
	union Serial1Word id;
	unsigned int size;
	unsigned char result;

#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	result = configWrite(id.value, (unsigned char)(size < 0xff ? size : 0xff), &request->data[3]);
	response->data = serial1Data.byte;
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0143 */
static unsigned char serial1Command0143(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	response->data = serial1Data.byte;
	response->length = 0;
	result = configOpen();
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0144 */
static unsigned char serial1Command0144(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	response->data = serial1Data.byte;
	response->length = 0;
	result = configClose();
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0145 */
static unsigned char serial1Command0145(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;

#if !FW_SINCE(0x00090804)
	SERIAL1_UNLOCKED_CHECK();
#endif
	response->data = serial1Data.byte;
	response->length = 0;
	result = configCommit();
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

#endif

#if !FW_SINCE(0x00090009)
#elif !FW_SINCE(0x00090804)
/* 0x00090206's records for the open copy (configWriteRecords): the size, the records; no entry
   of serialCommands1 runs it, and it sets no answer */
static unsigned char serial1Command0146(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	unsigned char size;
	unsigned char *data;

	SERIAL1_UNLOCKED_CHECK();
	size = request->data[0];
	data = &request->data[1];
	response->data = serial1Data.byte;
	response->length = 0;
	result = configWriteRecords(data, size);
	return SERIAL1_CONFIG_RESULT(result);	/* (no serialSetAnswer) */
}
#else
/* 0x0146: the position, the size, the data: records for the open copy (configWriteRecords) */
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
	result = configWriteRecords(id.value, size, data, 0);
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0147: as 0x0146, the records checked against the bank in use */
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
	result = configWriteRecords(id.value, size, data, 1);
	serialSetAnswer(SERIAL_ANSWER);
	return SERIAL1_CONFIG_RESULT(result);
}

/* 0x0171: getResetFlags's flags, the reset's cause, stateGetCause, the system mode, stateGetWakeMask */
static unsigned char serial1Command0171(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned long state;

	response->data = serial1Data.byte;
#if FW_SINCE(0x00090903)
	response->length = 9;
	serial1Data.byte[0] = getResetFlags();
	serial1Data.byte[1] = getResetCause();
	serial1Data.byte[2] = stateGetCause();
	memcpy(&serial1Data.byte[3], stateGetWakeupFactor(), 2);
	state = stateGetWakeMask();
	memcpy(&serial1Data.byte[5], &state, 4);
#else
	response->length = 5;		/* 0x00090804: the wakeup factor's first byte, the mask */
	serial1Data.byte[0] = *(unsigned char *)stateGetWakeupFactor();
	state = stateGetWakeMask();
	memcpy(&serial1Data.byte[1], &state, 4);
#endif
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

#if FW_SINCE(0x01000104)
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
#endif

#if FW_SINCE(0x01000104)
/* the status of 0x0150-0x0152's result; before 0x01000104 the result is the status */
#define SERIAL1_STATUS_015X(result)	(result = serial1Status015x(result))

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
#else
#define SERIAL1_STATUS_015X(result)
#endif

/* 0x0150: logGetInfo's eight bytes */
static unsigned char serial1Command0150(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->data = serial1Data.byte;
	result = (unsigned char)logGetInfo((struct LogInfo *)serial1Data.byte);
	SERIAL1_STATUS_015X(result);
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

#if BOARD_USS1001
	result = (unsigned char)logSetMode(LOG_MODE_STARTED, 2);
#else
	result = (unsigned char)logStart();
#endif
	SERIAL1_STATUS_015X(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0152 */
static unsigned char serial1Command0152(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

#if BOARD_USS1001
	result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 2);
#else
	result = (unsigned char)logStop();
#endif
	SERIAL1_STATUS_015X(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0153: the id: logReadData's read */
static unsigned char serial1Command0153(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int id;
	unsigned char length;

	memcpy(&id, request->data, 2);
	response->data = serial1Data.byte;
	result = (unsigned char)logReadData(id, response->data, &length);
	SERIAL1_STATUS_015X(result);
	if (result == 0)
		response->length = length;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0154: the id, the size: logReadOffset's read */
static unsigned char serial1Command0154(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	union Serial1Word id;
	unsigned char size;

	id.byte[0] = request->data[0];
	id.byte[1] = request->data[1];
	size = request->data[2];
	response->data = serial1Data.byte;
	result = (unsigned char)logReadOffset(id.value, size, response->data);
	SERIAL1_STATUS_015X(result);
	if (result == 0)
		response->length = size;
	else
		response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0155: the id: logClear */
static unsigned char serial1Command0155(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int id;

	memcpy(&id, request->data, 2);
	result = (unsigned char)logClear(id);
	SERIAL1_STATUS_015X(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0156 */
static unsigned char serial1Command0156(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	result = (unsigned char)logClearIfStopped();
	SERIAL1_STATUS_015X(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

#if BOARD_USS1001
/* 0x0157: the log halted (logSetMode) */
static unsigned char serial1Command0157(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	result = (unsigned char)logSetMode(LOG_MODE_HALTED, 2);
	SERIAL1_STATUS_015X(result);
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif

/* 0x0160: in state 4, clears (1) the NVS's syscon area (mask bit 0, then logClearIfStopped) and
   configRestoreDefaults's (bit 1; the slim also bstoreMigrate, bit 2), or (2) runs nvsFormat and
   logClearIfStopped */
static unsigned char serial1Command0160(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char what;
	unsigned char mask;
	unsigned int offset;
	unsigned char *buffer;
	unsigned int state;

	state = stateGet();
	what = request->data[0];
	mask = request->data[1];
	if (state != 0x0004) {
		result = 0x90;
		goto done;
	}
#if FW_SINCE(0x01000104)
	if (mask == 0)
		mask = 0xff;
#else
	/* before 0x01000104 no mask: data[1] must be 0, and 1 clears both */
	if (mask != 0) {
		result = 0x33;
		goto done;
	}
#endif
	if (what == 1) {
#if FW_SINCE(0x01000104)
		if (mask & 0x01) {
#else
		{
#endif
#if BOARD_USS1001
			logSetMode(LOG_MODE_STOPPED, 2);
#else
			logStop();
#endif
			buffer = serial1Data.byte;
			memset(buffer, 0xff, 0x20);
			for (offset = 0; offset < 0x40; offset += 0x20) {
				result = nvsWrite(offset, 0x20, buffer, 1);
				if (result != 0) {
					result = 0x91;
					goto done;
				}
			}
			result = (unsigned char)logClearIfStopped();
			if (result != 0) {
				result = 0x92;
				goto done;
			}
		}
#if FW_SINCE(0x01000104)
		if (mask & 0x02) {
#else
		{
#endif
			result = configRestoreDefaults();
			if (result != 0) {
				result = 0x93;
				goto done;
			}
		}
#if BOARD_USS1001
		if (mask & 0x04) {
			result = bstoreMigrate();
			if (result != 0) {
				result = 0x96;
				goto done;
			}
		}
#endif
	} else if (what == 2) {
		result = nvsFormat();
		if (result != 0) {
			result = 0x91;
			goto done;
		}
		result = (unsigned char)logClearIfStopped();
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

#if FW_SINCE(0x01000104)
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

#if FW_SINCE(0x0100060B)
/* 0x0163: 0x5a; the notification raises the battery's bootloader line and, but in the off
   state, runs transition 0x5c */
static unsigned char serial1Command0163(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char key;

	response->length = 0;
	key = request->data[0];
	if (key != 0x5a)
		result = 0x33;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

static unsigned char serial1Notify0163(void)
{
	unsigned int state;

	state = stateGet();
	batteryBLLineOn();
	if (state != 0x0004)
		STATE_TRANSITION(0x5c);
	else
		powerJobPostOffEnd();
	return 0;
}
#endif
#endif

#if FW_SINCE(0x00090903)
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
	task5SetFlag(4, 0x4000);
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
	task5SetFlag(4, 0x4000);
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
	task5SetFlag(4, 0x4000);
	return result;
}
#endif

#if FW_SINCE(0x01000104)
/* 0x0190 */
static unsigned char serial1Command0190(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int value;

	response->data = serial1Data.byte;
	response->length = 8;
	serial1Data.byte[0] = intp11Low();
	serial1Data.byte[1] = serial4GetP71();
	value = serial4GetCommand();
	memcpy(&serial1Data.byte[2], &value, 2);
	serial1Data.byte[4] = serial4GetStatus();
	serial1Data.byte[5] = (unsigned char)serial2TestAreaUsed();
	serial1Data.byte[6] = (unsigned char)serial2GetArmed();
	serial1Data.byte[7] = (unsigned char)serial2InTime();
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif

#if BOARD_USS1001
/* 0x0183-0x0189 (a byte) and 0x018a-0x018e: task 4's operations 0x0e-0x19, answered when they
   are done; status 0x61 while one runs */
static unsigned char serial1Command0183(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x0e;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0184(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x0f;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0185(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x10;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0186(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x11;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0187(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x12;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0188(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x13;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command0189(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x14;
		serial1Args.byte[0] = request->data[0];
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command018a(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x15;
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command018b(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x16;
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command018c(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x17;
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command018d(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x18;
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}

static unsigned char serial1Command018e(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x19;
		serialSetAnswer(SERIAL_ANSWER_LATER);
		task5SetFlag(4, 0x4000);
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x61;
	}
	return result;
}
#endif

#if FW_SINCE(0x0100060B)
/* 0x01a0: in system state 0x20, both touch panels' information (family, variant, version,
   build, configuration version, grip edges) */
static unsigned char serial1Command01a0(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int state;

	response->length = 0;
	state = stateGet();
	if (state != 0x0020) {
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x40;
	} else {
		response->data = serial1Data.byte;
		response->length = 0x14;
		memcpy(serial1Data.byte, &touch.panel[0], 10);
		memcpy(&serial1Data.byte[10], &touch.panel[1], 10);
		serialSetAnswer(SERIAL_ANSWER);
	}
	return result;
}

/* 0x01a1: in system state 0x20, the report IDs of the frame being filled */
static unsigned char serial1Command01a1(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int state;

	response->length = 0;
	state = stateGet();
	if (state != 0x0020) {
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x40;
	} else {
		response->data = serial1Data.byte;
		response->length = 0x0c;
		touchGetReportIds(serial1Data.byte);
		serialSetAnswer(SERIAL_ANSWER);
	}
	return result;
}

/* 0x01b0: the Elmo's registers 0xb0-0xb2 at the last power cut */
static unsigned char serial1Command01b0(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->data = serial1Data.byte;
#if BOARD_USS1001
	response->length = 8;			/* powerGetCutRegs gives 8 bytes on the slim */
#else
	response->length = 6;
#endif
	powerGetCutRegs(serial1Data.byte);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

#if BOARD_USS1001
/* 0x01c0: the event log's version, mode and count (eventLogGetInfo) */
static unsigned char serial1Command01c0(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char status;
	unsigned char version;
	unsigned char mode;
	unsigned int count;

	status = (unsigned char)eventLogGetInfo(&version, &mode, &count);
	if (status != 0) {
		response->length = 0;
		result = 0x33;
	} else {
		response->data = serial1Data.byte;
		response->length = 4;
		serial1Data.byte[0] = version;
		serial1Data.byte[1] = mode;
		memcpy(&serial1Data.byte[2], &count, 2);
	}
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x01c1: the event log cleared */
static unsigned char serial1Command01c1(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char status;

	status = (unsigned char)eventLogClear();
	response->length = 0;
	if (status != 0)
		result = 0xff;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x01c2: the event log started */
static unsigned char serial1Command01c2(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char status;

	status = (unsigned char)eventLogStart();
	response->length = 0;
	if (status != 0)
		result = 0xff;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x01c3: the event log stopped */
static unsigned char serial1Command01c3(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char status;

	status = (unsigned char)eventLogStop();
	response->length = 0;
	if (status != 0)
		result = 0xff;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x01c4: the index: an entry of the event log; status 0x33 for an index it does not hold */
static unsigned char serial1Command01c4(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char status;
	unsigned int index;
	struct EventLogEntry entry;

	memcpy(&index, request->data, 2);
	status = (unsigned char)eventLogRead(index, &entry);
	if (status == 0) {
		response->data = serial1Data.byte;
		response->length = sizeof(entry);
		memcpy(serial1Data.byte, &entry, sizeof(entry));
	} else {
		response->length = 0;
		if (status == 1 || status == 2)
			result = 0x33;
		else
			result = 0xff;
	}
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x01d0 and 0x01d1: not on USS-1001 */
static unsigned char serial1Command01d0(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return 0x98;
}

static unsigned char serial1Command01d1(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return 0x98;
}
#else
/* 0x01d0: bbmcGetInfo's */
static unsigned char serial1Command01d0(struct SerialBuffer *request, struct SerialBuffer *response)
{
	response->data = serial1Data.byte;
	response->length = sizeof(struct BbmcInfo);
	bbmcGetInfo(serial1Data.byte);
	serialSetAnswer(SERIAL_ANSWER);
	return 0;
}

/* 0x01d1: an off flag, a byte, the BBMC registers 4 and 5 (its mode): the mode written, then
   the modem's power off (flag 0) or, in system state 0x20, on */
static unsigned char serial1Command01d1(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result;
	unsigned char *data;

	response->length = 0;
	response->data = serial1Data.byte;
	if (request->length == 4) {
		data = request->data;
		bbmcWriteRegs(4, 2, &data[2], BBMC_WRITE_SET);
		result = 0;
		if (data[0] == 0)
			powerDeviceSetPending(0x0200);
		else if (stateGet() == 0x0020)
			powerDeviceSetPending(0x0001);
		else
			result = 0x50;
	} else {
		result = 0x32;
	}
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif	/* BOARD_USS1001 */
#endif
#endif

#if FW_SINCE(0x00090804)
int serial1KeyOpen(void)
{
	return serial1Key == SERIAL1_KEY_OPEN;
}
#endif

/* 0x0900: opens the commands marked SERIAL1_KEY when the request is serial1Password, closes
   them otherwise */
static unsigned char serial1Command0900(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
#if FW_SINCE(0x00090804)
	unsigned char *data;

	data = request->data;
	if (memcmp(data, serial1Password, sizeof(serial1Password)) == 0)
		serial1Key = SERIAL1_KEY_OPEN;
	else
		serial1Key = SERIAL1_KEY_CLOSED;
#endif
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0901: closes them */
static unsigned char serial1Command0901(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

#if FW_SINCE(0x00090804)
	serial1Key = SERIAL1_KEY_CLOSED;
#endif
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

#if BOARD_KERNEL_COMMANDS
/* 0x0910-0x0916 (0x00090804, 0x00411000): the kernel's objects (a status of 0x33 for an ID it has none
   of): tasks and event flags 1-6, memory pools 1-2, mailbox 1 and cyclic handler 7 */
#define SERIAL1_KERNEL_TASKS		6
#define SERIAL1_KERNEL_FLAGS		6
#define SERIAL1_KERNEL_POOLS		2
#define SERIAL1_KERNEL_MAILBOX		1
#define SERIAL1_KERNEL_HANDLER		7

/* 0x0910: ref_tsk, a task's state */
static unsigned char serial1Command0910(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	if (id == 0 || id > SERIAL1_KERNEL_TASKS) {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	packet = serial1Data.byte;
	result = ref_tsk(id, (T_RTSK *)packet);
	response->data = packet;
	response->length = sizeof(T_RTSK);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0911: ref_sem, a semaphore's state: the kernel has none */
static unsigned char serial1Command0911(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	response->length = 0;
	serialSetAnswer(SERIAL_ANSWER);
	return 0x33;
}

/* 0x0912: ref_flg, an event flag's state */
static unsigned char serial1Command0912(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	if (id == 0 || id > SERIAL1_KERNEL_FLAGS) {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	packet = serial1Data.byte;
	result = ref_flg(id, (T_RFLG *)packet);
	response->data = packet;
	response->length = sizeof(T_RFLG);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0913: ref_mbx, a mailbox's state */
static unsigned char serial1Command0913(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	if (id != SERIAL1_KERNEL_MAILBOX) {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	packet = serial1Data.byte;
	result = ref_mbx(id, (T_RMBX *)packet);
	response->data = packet;
	response->length = sizeof(T_RMBX);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0914: ref_mpf, a fixed-size memory pool's state */
static unsigned char serial1Command0914(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	if (id == 0 || id > SERIAL1_KERNEL_POOLS) {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	packet = serial1Data.byte;
	result = ref_mpf(id, (T_RMPF *)packet);
	response->data = packet;
	response->length = sizeof(T_RMPF);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0915: ref_cyc, a cyclic handler's state */
static unsigned char serial1Command0915(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char id;
	unsigned char *packet;

	id = request->data[0];
	if (id != SERIAL1_KERNEL_HANDLER) {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x33;
	}
	packet = serial1Data.byte;
	result = ref_cyc(id, (T_RCYC *)packet);
	response->data = packet;
	response->length = sizeof(T_RCYC);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0916: ref_ver, the kernel's version */
static unsigned char serial1Command0916(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char *packet;

	packet = serial1Data.byte;
	result = ref_ver((T_RVER *)packet);
	response->data = packet;
	response->length = sizeof(T_RVER);
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif

#if BOARD_USS1001
/* 0x0917: the chip ID (chipIdGet); status 0x97 when it cannot be read */
static unsigned char serial1Command0917(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	response->data = serial1Data.byte;
	response->length = 4;
	result = chipIdGet(response->data);
	if (result == 0)
		result = 0;
	else
		result = 0x97;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0930-0x0932: task 4's operations 0x0a-0x0c */
static unsigned char serial1Command0930(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		switch (serial.command) {
		case 0x0930:
			serial1State = 0x0a;
			break;
		case 0x0931:
			serial1State = 0x0b;
			break;
		case 0x0932:
			serial1State = 0x0c;
			break;
		}
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serialSetAnswer(SERIAL_ANSWER_LATER);
	task5SetFlag(4, 0x4000);
	return result;
}
#endif

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
	task5SetFlag(4, 0x4000);
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
	task5SetFlag(4, 0x4000);
	return result;
}

#if FW_SINCE(0x0100060B)
/* 0x0952: in the off state, transition 0x43 */
static unsigned char serial1Command0952(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned int state;

	state = stateGet();
	response->length = 0;
	if (state != 0x0004) {
		serialSetAnswer(SERIAL_ANSWER);
		result = 0x50;
	} else {
		result = STATE_TRANSITION(0x43);
#if BOARD_USS1001
		result = serial1StateResult(result);
#endif
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
	}
	return result;
}

#if BOARD_USS1001
/* 0x0953: task 4's operation 0x0d */
static unsigned char serial1Command0953(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	if (serial1State == 0) {
		serial1State = 0x0d;
	} else {
		response->length = 0;
		serialSetAnswer(SERIAL_ANSWER);
		return 0x61;
	}
	serialSetAnswer(SERIAL_ANSWER_LATER);
	task5SetFlag(4, 0x4000);
	return result;
}

#if FW_SINCE(0x01030511)	/* 0x01030505 has none of 0x0961-0x0965 */
/* 0x0961: both sets of the analog sticks */
static unsigned char serial1Command0961(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	dis_dsp();
	controlsReadAnalog0(serial1Data.byte);
	controlsReadAnalog1(&serial1Data.byte[4]);
	ena_dsp();
	response->data = serial1Data.byte;
	response->length = 8;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0962: each axis's last sample */
static unsigned char serial1Command0962(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	dis_dsp();
	analogGetSamples((unsigned int *)serial1Data.byte);
	ena_dsp();
	response->data = serial1Data.byte;
	response->length = 8;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0963: the calibration */
static unsigned char serial1Command0963(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	dis_dsp();
	analogCalibrationGet(serial1Data.byte);
	ena_dsp();
	response->data = serial1Data.byte;
	response->length = 0x10;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0964: each axis's samples (mode 0) or medians (other modes) outside its limits */
static unsigned char serial1Command0964(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;
	unsigned char mode;

	mode = request->data[0];
	if (mode == 0) {
		dis_dsp();
		analogGetSampleFaults(serial1Data.byte);
		ena_dsp();
	} else {
		dis_dsp();
		analogGetMedianFaults(serial1Data.byte);
		ena_dsp();
	}
	response->data = serial1Data.byte;
	response->length = 0x20;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}

/* 0x0965: each axis's limits (analogLimitsSet's) */
static unsigned char serial1Command0965(struct SerialBuffer *request, struct SerialBuffer *response)
{
	unsigned char result = 0;

	dis_dsp();
	analogLimitsGet((unsigned int *)serial1Data.byte);
	ena_dsp();
	response->data = serial1Data.byte;
	response->length = 0x10;
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif
#endif
#else
/* 0x0950: a channel's level, as 1400 less 10 for each step above paramGetBase's base, most
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
			result = paramRead(serial1Levels[i].setting - (unsigned char *)&param, 1, &level);
			break;
		}
	}
	paramGetBase(&base);
	step = level - base;
#if ERNIE_BOARD == 0x00406000 || !FW_SINCE(0x01000306)
	/* PCH-1000's build and the builds before 0x01000306 multiply in 16 bits; the others'
	   take the product's low byte */
	value.value = 1400 - (unsigned int)step * 10;
#else
	value.value = 1400 - step * 10;
#endif
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
	paramGetBase(&base);
	level = step + base;
	for (i = 0; i < SERIAL1_LEVELS; i++) {
		if (serial1Levels[i].channel == channel) {
			result = paramWrite(serial1Levels[i].setting - (unsigned char *)&param, 1, &level);
			break;
		}
	}
done:
	serialSetAnswer(SERIAL_ANSWER);
	return result;
}
#endif

/* the commands, by number */
#if BOARD_USS1001
const struct SerialCommand1 serialCommands1[] = {
	{ 0x0100, serial1Command0100, 0 },
	{ 0x0101, serial1Command0101, 0 },
	{ 0x0102, serial1Command0102, 0 },
	{ 0x0103, serial1Command0103, 0 },
	{ 0x0104, serial1Command0104, 0 },
	{ 0x0105, serial1Command0105, SERIAL1_UNLOCKED },
	{ 0x0106, serial1Command0106, 0 },
	{ 0x0107, serial1Command0107, 0 },
	{ 0x0108, serial1Command0108, 0 },
	{ 0x0109, serial1Command0109, 0 },
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
	{ 0x0157, serial1Command0157, SERIAL1_UNLOCKED },
	{ 0x0160, serial1Command0160, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0161, serial1Command0161, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0162, serial1Command0162, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0163, serial1Command0163, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0168, serial1Command0168, SERIAL1_UNLOCKED },
	{ 0x0170, serial1Command0170, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0171, serial1Command0171, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0172, serial1Command0172, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0180, serial1Command0180, SERIAL1_UNLOCKED },
	{ 0x0181, serial1Command0181, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0182, serial1Command0182, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0183, serial1Command0183, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0184, serial1Command0184, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0185, serial1Command0185, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0186, serial1Command0186, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0187, serial1Command0187, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0188, serial1Command0188, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0189, serial1Command0189, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x018a, serial1Command018a, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x018b, serial1Command018b, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x018c, serial1Command018c, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x018d, serial1Command018d, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x018e, serial1Command018e, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0190, serial1Command0190, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x01a0, serial1Command01a0, SERIAL1_UNLOCKED },
	{ 0x01a1, serial1Command01a1, SERIAL1_UNLOCKED },
	{ 0x01b0, serial1Command01b0, SERIAL1_UNLOCKED },
	{ 0x01c0, serial1Command01c0, SERIAL1_UNLOCKED },
	{ 0x01c1, serial1Command01c1, SERIAL1_UNLOCKED },
	{ 0x01c2, serial1Command01c2, SERIAL1_UNLOCKED },
	{ 0x01c3, serial1Command01c3, SERIAL1_UNLOCKED },
	{ 0x01c4, serial1Command01c4, SERIAL1_UNLOCKED },
	{ 0x01d0, serial1Command01d0, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x01d1, serial1Command01d1, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x01d2, serial1Command01d2, SERIAL1_UNLOCKED },
	{ 0x0300, serial1Command0300, 0 },
	{ 0x0301, serial1Command0301, 0 },
	{ 0x0900, serial1Command0900, SERIAL1_NVS_KEY3 },
	{ 0x0901, serial1Command0901, SERIAL1_NVS_KEY3 },
	{ 0x0910, serial1Command0910, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0911, serial1Command0911, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0912, serial1Command0912, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0913, serial1Command0913, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0914, serial1Command0914, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0915, serial1Command0915, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0916, serial1Command0916, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0917, serial1Command0917, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0930, serial1Command0930, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0931, serial1Command0930, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0932, serial1Command0930, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0940, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0941, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0942, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0943, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0944, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0945, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0952, serial1Command0952, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0953, serial1Command0953, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
#if FW_SINCE(0x01030511)
	{ 0x0961, serial1Command0961, SERIAL1_KEY },
	{ 0x0962, serial1Command0962, SERIAL1_KEY },
	{ 0x0963, serial1Command0963, SERIAL1_KEY },
	{ 0x0964, serial1Command0964, SERIAL1_KEY },
	{ 0x0965, serial1Command0965, SERIAL1_KEY },
#endif
};
#elif !FW_SINCE(0x00090804)
const struct SerialCommand serialCommands1[] = {
	{ 0x0100, serial1Command0100 },
	{ 0x0101, serial1Command0101 },
	{ 0x0102, serial1Command0102 },
	{ 0x0103, serial1Command0103 },
	{ 0x0104, serial1Command0104 },
	{ 0x0105, serial1Command0105 },
	{ 0x0106, serial1Command0106 },
	{ 0x0110, serial1Command0110 },
	{ 0x0120, serial1Command0120 },
	{ 0x0121, serial1Command0121 },
	{ 0x0131, serial1Command0131 },
	{ 0x0132, serial1Command0132 },
#if FW_SINCE(0x00090009)
	{ 0x0140, serial1Command0140 },
	{ 0x0141, serial1Command0141 },
	{ 0x0142, serial1Command0142 },
	{ 0x0143, serial1Command0143 },
	{ 0x0144, serial1Command0144 },
	{ 0x0145, serial1Command0145 },
#endif
	{ 0x0300, serial1Command0300 },
	{ 0x0301, serial1Command0301 },
	{ 0x0900, serial1Command0900 },
	{ 0x0901, serial1Command0901 },
	{ 0x0910, serial1Command0910 },
	{ 0x0911, serial1Command0911 },
	{ 0x0912, serial1Command0912 },
	{ 0x0913, serial1Command0913 },
	{ 0x0914, serial1Command0914 },
	{ 0x0915, serial1Command0915 },
	{ 0x0916, serial1Command0916 },
	{ 0x0940, serial1Command0940 },
	{ 0x0941, serial1Command0941 },
	{ 0x0942, serial1Command0940 },
	{ 0x0943, serial1Command0941 },
	{ 0x0944, serial1Command0940 },
	{ 0x0945, serial1Command0941 },
	{ 0x0950, serial1Command0950 },
	{ 0x0951, serial1Command0951 },
};
#else
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
#if FW_SINCE(0x01000104)
	{ 0x0161, serial1Command0161, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
#if FW_SINCE(0x01000104)
	{ 0x0162, serial1Command0162, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
#if FW_SINCE(0x0100060B)
	{ 0x0163, serial1Command0163, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0168, serial1Command0168, SERIAL1_UNLOCKED },
#endif
	{ 0x0170, serial1Command0170, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0171, serial1Command0171, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#if FW_SINCE(0x01000104)
	{ 0x0172, serial1Command0172, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
#if FW_SINCE(0x00090903)
	{ 0x0180, serial1Command0180, SERIAL1_UNLOCKED },
	{ 0x0181, serial1Command0181, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x0182, serial1Command0182, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
#if FW_SINCE(0x01000104)
	{ 0x0190, serial1Command0190, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
#if FW_SINCE(0x0100060B)
	{ 0x01a0, serial1Command01a0, SERIAL1_UNLOCKED },
	{ 0x01a1, serial1Command01a1, SERIAL1_UNLOCKED },
	{ 0x01b0, serial1Command01b0, SERIAL1_UNLOCKED },
	{ 0x01d0, serial1Command01d0, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
	{ 0x01d1, serial1Command01d1, SERIAL1_UNLOCKED | SERIAL1_NVS_KEY3 },
#endif
	{ 0x0300, serial1Command0300, 0 },
	{ 0x0301, serial1Command0301, 0 },
	{ 0x0900, serial1Command0900, SERIAL1_NVS_KEY3 },
	{ 0x0901, serial1Command0901, SERIAL1_NVS_KEY3 },
#if BOARD_KERNEL_COMMANDS
	{ 0x0910, serial1Command0910, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0911, serial1Command0911, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0912, serial1Command0912, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0913, serial1Command0913, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0914, serial1Command0914, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0915, serial1Command0915, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0916, serial1Command0916, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
#endif
	{ 0x0940, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0941, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0942, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0943, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0944, serial1Command0940, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0945, serial1Command0941, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
#if FW_SINCE(0x0100060B)
	{ 0x0952, serial1Command0952, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
#else
	{ 0x0950, serial1Command0950, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
	{ 0x0951, serial1Command0951, SERIAL1_KEY | SERIAL1_NVS_KEY3 },
#endif
};
#endif

const struct SerialNotify serialNotifies1[] = {
	{ 0x0102, serial1Notify0102 },
#if FW_SINCE(0x01000104)
	{ 0x0161, serial1Notify0161 },
	{ 0x0162, serial1Notify0162 },
#endif
#if FW_SINCE(0x0100060B)
	{ 0x0163, serial1Notify0163 },
#endif
};

#if !FW_SINCE(0x0100060B)
const struct Serial1Level serial1Levels[SERIAL1_LEVELS] = {
	{ 0x81, &param.levels[0] },
	{ 0x82, &param.levels[1] },
	{ 0x83, &param.levels[2] },
	{ 0x84, &param.levels[3] },
};
#endif
