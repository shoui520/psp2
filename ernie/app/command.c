/*
 * The base commands: the reads the command table answers at once (0x0001 to 0x0014: the
 * versions, the build's time stamp, the wakeup factor, the clock and the alarm), and request
 * 0, which takes the writes (0x0080 to 0x00d3). Its notify handler answers most of them at
 * once; the log (0x0086 to 0x0088), the handshakes (0x00a0, 0x00d0, 0x00d1) and the SNVS
 * (0x00d2, 0x00d3) go to a task, which answers later. Before the handshake, only the
 * commands of commandBaseRequests are taken.
 *
 * The answers' status: 0 done, 0x02 failed, 0x04 not ready, 0x05 not now, 0x06 refused,
 * 0x11 the SNVS is locked, 0x20 the link is not set up, 0x3b the data's length is wrong,
 * 0x3c the data is wrong, 0x3e not taken here (or not before the handshake).
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "board.h"
#include "clockout.h"
#include "config.h"
#include "command.h"
#include "eventlog.h"
#include "handshake.h"
#include "host.h"
#include "kernel.h"
#include "log.h"
#include "param.h"
#include "rtc.h"
#include "scratchpad.h"
#include "serial.h"
#include "snvs.h"
#include "sleepfactor.h"
#include "state.h"
#include "task.h"
#include "task5.h"
#include "version.h"
#include "watchdog.h"


/* a task-side command is done: the answer from its result */
typedef void (*CommandDone)(unsigned char command, unsigned char result);

int commandHandshake(unsigned char *data, unsigned char command, CommandDone done);
void commandHandshakeDone(unsigned char command, unsigned char result);
int commandLogStart(unsigned char *data, unsigned char command, CommandDone done);
int commandLogInfo(unsigned char *data, unsigned char command, CommandDone done);
int commandLogRead(unsigned char *data, unsigned char *answer, unsigned char command,
		   CommandDone done);
void commandLogDone(unsigned char command, unsigned char result);
#if SOURCE_0103
int commandLogHalt(unsigned char *data, unsigned char command, CommandDone done);
#endif
#if FW_SINCE(0x00090903)
int commandSnvsHandshake(unsigned char *data, unsigned char command, CommandDone done);
int commandSnvs00D1(unsigned char *data, unsigned char command, CommandDone done);
int commandSnvsRead(unsigned char *data, unsigned char *answer, unsigned char command,
		    CommandDone done);
int commandSnvsWrite(unsigned char *data, unsigned char *answer, unsigned char command,
		     CommandDone done);
void commandSnvsDone(unsigned char command, unsigned char result);
#endif

/* request 0: its answer and its command's data */
struct HostRequest commandRequest;
#if FW_SINCE(0x00090903)
unsigned char commandRequestAnswer[0x40];
unsigned char commandRequestData[0x40];
#else
unsigned char commandRequestAnswer[0x28];	/* 0x00090804: the handshake's 0x28 bytes at most */
unsigned char commandRequestData[0x28];
#endif

/* 0x0088's answer: the log's data from offset */
struct LogRead {
	unsigned int	offset;
	unsigned char	length;
	unsigned char	data[1];
};

/* the pins command 0x0014 reports */
union CommandPins {
	unsigned char	value;
	struct {
		unsigned char	p30:1;		/* P3.0 */
		unsigned char	present:1;	/* always 1 */
		unsigned char	p31:1;		/* P3.1 */
		unsigned char	p76:1;		/* P7.6 */
		unsigned char	p115:1;		/* P11.5 (DOL-1001) */
		unsigned char	reserved:3;
	} bits;
};

union CommandPins commandPins;

/* a word the commands carry least significant byte first */
#if FW_SINCE(0x0100060B)
/* 0x00b4's and 0x00b5's pins, in the word's high byte */
union CommandPinState {
	unsigned char	value;
	struct {
		unsigned char	p30:1;		/* P3.0 */
		unsigned char	p67:1;		/* P6.7 */
		unsigned char	reserved:5;
		unsigned char	p31:1;		/* P3.1 */
	} bits;
};
#endif

/* 0x00b3-0x00b5's pins (the slim's P12.0 and P1.7 for P6.7 and P3.1), and where 0x00b4 and
   0x00b5 keep them (the slim's: a local of their own; DOL-1001's P6.5 and P3.1) */
#if BOARD_DOL1001
#define COMMAND_PIN_P67		P6.5
#define COMMAND_PIN_P31		P3.1
#define COMMAND_PINS		pins
#elif SOURCE_0103
#define COMMAND_PIN_P67		P12.0
#define COMMAND_PIN_P31		P1.7
#define COMMAND_PINS		pins
#else
#define COMMAND_PIN_P67		P6.7
#define COMMAND_PIN_P31		P3.1
#define COMMAND_PINS		word.pins.state
#endif

union CommandWord {
	unsigned int	value;
	unsigned char	byte[2];
	struct HostLinkConfig config;
	union Watchdog	watchdog;
#if FW_SINCE(0x0100060B)
	struct {
		unsigned char	low;
		union CommandPinState state;
	} pins;
#endif
};

/* the time stamp's digits in appTimeStamp: the year, month, day, hour and minute */
const unsigned char timeStampDigits[12] = {
	8, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23,
};

/* the commands request 0 takes before the handshake */
#if HOST_WATCHDOG
#define COMMAND_BASE_REQUESTS	6
static const unsigned int commandBaseRequests[COMMAND_BASE_REQUESTS] = {
	0x0080, 0x0083, 0x0084, 0x0085, 0x00a0, 0x00c0,
};
#else
#define COMMAND_BASE_REQUESTS	4
static const unsigned int commandBaseRequests[COMMAND_BASE_REQUESTS] = {
	0x0080, 0x0083, 0x00a0, 0x00c0,
};
#endif

/* 0x0001 (sceSysconGetErnieVersion) */
unsigned char hostCommandErnieVersion(unsigned char *out, unsigned char *length)
{
	getErnieVersion(out);
	*length = 4;
	return 0;
}

#if BOARD_EVENT_LOG
/* the slim's log of a command (kind 0x10, code 0): its number's low byte, its length and its
   first two bytes */
static void commandLogRequest(void)
{
	eventLogCommand[0] = commandRequest.command & 0xff;
	eventLogCommand[1] = commandRequest.length;
	eventLogCommand[2] = commandRequest.data[0];
	eventLogCommand[3] = commandRequest.data[1];
	eventLogRecord(0x10, 0, eventLogCommand, 4);
}

/* and of its answer (code 0x80): the number, the answer's length, its status and first byte */
static void commandLogAnswer(void)
{
	eventLogCommand[0] = commandRequest.command & 0xff;
	eventLogCommand[1] = commandRequest.answerLength;
	eventLogCommand[2] = commandRequest.status;
	eventLogCommand[3] = commandRequest.answer[0];
	eventLogRecord(0x10, 0x80, eventLogCommand, 4);
}
#endif

/* 0x0002 (sceSysconGetTimeStamp): the build's date as 12 digits, YYYYMMDDhhmm */
unsigned char hostCommandTimeStamp(unsigned char *out, unsigned char *length)
{
#if SOURCE_0103
	getTimeStamp(out);
#else
	unsigned char i;

	for (i = 0; i < 12; i++)
		out[i] = appTimeStamp[timeStampDigits[i]];
#endif
	*length = 12;
	return 0;
}

/* 0x0005 (sceSysconGetHardwareInfo) */
unsigned char hostCommandHardwareInfo(unsigned char *out, unsigned char *length)
{
	getHardwareInfo(out);
	*length = 4;
	return 0;
}

#if SOURCE_0103
/* 0x0006: the slim's hardware info 2 */
unsigned char hostCommand0006(unsigned char *out, unsigned char *length)
{
	getHardwareInfo2(out);
	*length = 16;
	return 0;
}
#endif

/* 0x0010 (sceSysconGetWakeupFactor) */
unsigned char hostCommandWakeupFactor(unsigned char *out, unsigned char *length)
{
	union CommandWord factor;

	factor.value = stateGetWakeupFactor()->word;
	out[0] = factor.byte[0];
	out[1] = factor.byte[1];
	*length = 2;
	return 0;
}

/* 0x0011 (sceSysconGetClock) */
unsigned char hostCommandClock(unsigned char *out, unsigned char *length)
{
	memcpy(out, getClock().byte, 4);
	*length = 4;
	return 0;
}

/* 0x0012: the alarm, and at link level 1 its mode */
unsigned char hostCommandAlarm(unsigned char *out, unsigned char *length)
{
#if FW_SINCE(0x01000104)
	unsigned int level;

	hostGetLinkLevel(&level);
	memcpy(out, getAlarm().byte, 4);
	if (level != 0) {
		out[4] = getAlarmMode();
		*length = 5;
	} else
		*length = 4;
#else
	/* before 0x01000104 the alarm has no mode */
	memcpy(out, getAlarm().byte, 4);
	*length = 4;
#endif
	return 0;
}

#if HOST_WATCHDOG
/* 0x0013: the host's watchdog's setting */
unsigned char hostCommand0013(unsigned char *out, unsigned char *length)
{
	union CommandWord value;

	watchdogGet(&value.watchdog);
	out[0] = value.byte[0];
	out[1] = value.byte[1];
	*length = 2;
	return 0;
}
#else
/* 0x0013: no host watchdog in 0x00080504; it answers 0xaa */
unsigned char hostCommand0013(unsigned char *out, unsigned char *length)
{
	out[0] = 0xaa;
	*length = 1;
	return 0;
}
#endif

/* 0x0014: the pins */
unsigned char hostCommandPins(unsigned char *out, unsigned char *length)
{
	commandPins.bits.p30 = P3.0;
#if BOARD_DOL1001
	/* DOL-1001's reports P3.0, three bits high (present, P3.1, P7.6) and P11.5 */
	commandPins.value |= 0x0e;
	commandPins.bits.p115 = P11.5;
	*out = commandPins.value & 0x1f;
#elif SOURCE_0103
	/* the slim's reports P3.0 and four bits high: present, P3.1, P7.6 and bit 4 */
	commandPins.value |= 0x1e;
	*out = commandPins.value & 0x1f;
#else
#if FW_SINCE(0x00090206)
	commandPins.bits.present = 1;
#else
	commandPins.bits.present = P14.1;	/* 0x00090009 reads it */
#endif
#if FW_SINCE(0x0100060B)
	commandPins.bits.p31 = 1;	/* P3.1 is driven low (inputInit): reported high */
#else
	commandPins.bits.p31 = P3.1;
#endif
#if FW_SINCE(0x00090804)
	commandPins.bits.p76 = P7.6;
	*out = commandPins.value & 0x0f;
#else
	*out = commandPins.value & 0x07;	/* 0x00090206 has not P7.6 */
#endif
#endif
	*length = 1;
	return 0;
}

#if FW_SINCE(0x0100060B)
/* 0x0015: sleepFactorGetOffInfo's */
unsigned char hostCommand0015(unsigned char *out, unsigned char *length)
{
	sleepFactorGetOffInfo(out);
	*length = 4;
	return 0;
}
#endif

void commandRequestInit(void)
{
	commandRequest.id = 0;
	commandRequest.data = commandRequestData;
	commandRequest.answer = commandRequestAnswer;
	commandRequest.size = sizeof(commandRequestData);
	commandRequest.notify = commandRequestNotify;
	commandRequest.state = HOST_REQUEST_RECEIVE;
}

void commandRequestReceive(void)
{
	commandRequest.state = HOST_REQUEST_RECEIVE;
}

/* the pending device bits 0x00b2 sets for the dongle's output off and on (0x00102603's
   multi-connector switch: bits 1 and 0) */
#if BOARD_102603
#define COMMAND_PENDING_DONGLE_OFF	0x02
#define COMMAND_PENDING_DONGLE_ON	0x01
#else
#define COMMAND_PENDING_DONGLE_OFF	0x20
#define COMMAND_PENDING_DONGLE_ON	0x10
#endif

/* every board's 0x010001xx: 0x00c0 without transition 0x5b, 0x00c0's parameter window
   0xff000000, the transitions' result read as a byte */
#define COMMAND_10X		(FW_SINCE(0x01000104) && !FW_SINCE(0x01000306))

/* 0x00c0's parameter: the data's three bytes in the window at 0xfc000000 (0xff000000 before
   0x01000104 and in the 0x010001xx; DOL-1001's: none, the bytes alone) */
#if BOARD_DOL1001
#define COMMAND_PARAMETER_WINDOW	0
#elif SOURCE_0103
#define COMMAND_PARAMETER_WINDOW	0xf0000000	/* the slim's */
#elif FW_SINCE(0x01000104) && !COMMAND_10X
#define COMMAND_PARAMETER_WINDOW	0xfc000000
#else
#define COMMAND_PARAMETER_WINDOW	0xff000000
#endif
#define COMMAND_PARAMETER(data)	(((unsigned long)(data)[1] & 0xff) |			\
				 ((unsigned long)(data)[2] << 8 & 0xff00) |		\
				 ((unsigned long)(data)[3] << 16 & 0xff0000) |		\
				 COMMAND_PARAMETER_WINDOW)
/* the slim's 9-byte 0x00c0: four bytes of it in the window */
#define COMMAND_PARAMETER4(data)	(((unsigned long)(data)[1] & 0xff) |			\
				 ((unsigned long)(data)[2] << 8 & 0xff00) |		\
				 ((unsigned long)(data)[3] << 16 & 0xff0000) |		\
				 ((unsigned long)(data)[4] << 24 & 0xff000000) |	\
				 COMMAND_PARAMETER_WINDOW)

/* the slim logs 0x0081, 0x0082 and 0x00c0-0x00c2 and their answers (eventlog.c); DOL-1001
   keeps the calls, to functions that do nothing (at the file's end) */
#if BOARD_EVENT_LOG || BOARD_DOL1001
static void commandLogRequest(void);
static void commandLogAnswer(void);
#define COMMAND_LOG_REQUEST()		commandLogRequest()
#define COMMAND_LOG_ANSWER()		commandLogAnswer()
#else
#define COMMAND_LOG_REQUEST()
#define COMMAND_LOG_ANSWER()
#endif

/* request 0's command came in, or its answer went out */
void commandRequestNotify(void)
{
#if SOURCE_0103
	union RtcTick tick;
	unsigned int offset;
	unsigned char size;
	unsigned char transition;
	unsigned char result;
	unsigned char linkSize;
	unsigned long parameter;
	unsigned char i;
	union CommandPinState pins;	/* 0x00b4's and 0x00b5's */
	union CommandWord word;
#elif FW_SINCE(0x01000104)
	STATE_TRANSITION_EVENT
	unsigned char i;
	union RtcTick tick;
	unsigned int offset;
	unsigned char size;
	unsigned char transition;
	unsigned char result;
	unsigned char linkSize;
	unsigned long parameter;
	union CommandWord word;
#else
	/* before 0x01000104 the event carries the transition and its parameter itself */
	unsigned char i;
	unsigned char reserved;		/* never used */
	union RtcTick tick;
	unsigned int offset;
	unsigned char size;
	unsigned char transition;	/* never used */
	STATE_TRANSITION_EVENT
	unsigned char result;
	union CommandWord word;
#endif

#if SOURCE_0103
	/* the slim's takes the link's events too, and checks the handshake on a command */
	switch (commandRequest.event) {
	case HOST_EVENT_COMMAND:
		if (!handshakeUnlocked11()) {
			for (i = 0; i < COMMAND_BASE_REQUESTS; i++) {
				if (commandBaseRequests[i] == commandRequest.command)
					break;
			}
			if (i >= COMMAND_BASE_REQUESTS) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
				handshakeRefuse();
				return;
			}
		}
#else
	if (!handshakeUnlocked11()) {
		for (i = 0; i < COMMAND_BASE_REQUESTS; i++) {
			if (commandBaseRequests[i] == commandRequest.command)
				goto allowed;
		}
		hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
		return;
	}
allowed:
	switch (commandRequest.event) {
	case HOST_EVENT_COMMAND:
#endif
#if FW_SINCE(0x00090903)
		if (commandRequest.command >= 0x00d2 && commandRequest.command < 0x00d4 &&
		    snvsUnlocked() == 0) {
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x11, 0);
			return;
		}
#endif
		switch (commandRequest.command) {
		case 0x0080:		/* the link's settings */
			if (commandRequest.length == 2) {
				word.value = commandRequestData[0] |
					     (unsigned int)commandRequestData[1] << 8;
				result = hostSetLinkConfig(word.config);
				if (result == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x20, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x0081:		/* sceSysconSetClock */
			COMMAND_LOG_REQUEST();
			if (commandRequest.length == 4) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setClock(tick.value);
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			COMMAND_LOG_ANSWER();
			break;
		case 0x0082:		/* the alarm, and its mode */
			COMMAND_LOG_REQUEST();
			if (commandRequest.length == 4) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setAlarm(tick.value);
#if FW_SINCE(0x01000104)
				setAlarmMode(0);
#endif
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
#if FW_SINCE(0x01000104)
			} else if (commandRequest.length == 5 && commandRequestData[4] < 2) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setAlarm(tick.value);
				setAlarmMode(commandRequestData[4]);
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
#endif
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			COMMAND_LOG_ANSWER();
			break;
#if HOST_WATCHDOG
		case 0x0083:
			if (commandRequest.length == 2) {
				word.byte[0] = commandRequestData[0];
				word.byte[1] = commandRequestData[1];
				watchdogSet(&word.watchdog);
				result = watchdogStart();
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x0084:
			result = watchdogStop();
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
		case 0x0085:
			result = (unsigned char)watchdogKick();
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
#else
		case 0x0083:		/* 0x00080504: no host watchdog */
			hostRequestAnswer(&commandRequest, 0x10, 0x3f, 0);
			break;
#endif
#if FW_SINCE(0x00090804)	/* 0x00090206 has no log */
		case 0x0086:		/* the log: sceSysconGetLogInfo */
		case 0x0087:		/* sceSysconLogStart */
#if FW_SINCE(0x00090903)
		case 0x0088:		/* sceSysconLogReadData */
#endif
#if SOURCE_0103
		case 0x0089:		/* the slim's */
#endif
			task2SetFlag(3, 0x2000);
			hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			break;
#endif
		case 0x0090:		/* sceSysconReadScratchPad */
			if (commandRequest.length != 3) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				break;
			}
			offset = commandRequestData[0] | (unsigned int)commandRequestData[1] << 8;
			size = commandRequestData[2];
			if (scratchPadRead(offset, size, commandRequestAnswer) == 0)
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, size);
			else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x0091:		/* sceSysconWriteScratchPad */
			if (commandRequest.length < 4) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				break;
			}
			offset = commandRequestData[0] | (unsigned int)commandRequestData[1] << 8;
			size = commandRequestData[2];
			if (scratchPadWrite(offset, size, &commandRequestData[3]) == 0)
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x0092:		/* the NVS's read */
			if (commandRequest.length != 3) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				break;
			}
			offset = commandRequestData[0] | (unsigned int)commandRequestData[1] << 8;
			size = commandRequestData[2];
			if (paramRead(offset, size, commandRequestAnswer) == 0)
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, size);
			else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x0093:		/* the NVS's write */
			if (commandRequest.length < 4) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				break;
			}
			offset = commandRequestData[0] | (unsigned int)commandRequestData[1] << 8;
			size = commandRequestData[2];
			if (paramWrite(offset, size, &commandRequestData[3]) == 0)
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x00a0:		/* the handshake */
#if FW_SINCE(0x01000104)
			hostGetLinkSize(&linkSize);
			if (linkSize == 0) {
#else
			if (hostLink.config.size == 0) {
#endif
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x20, 0);
				break;
			}
			if (commandRequest.length == 0x28) {
				task2SetFlag(3, 0x2000);
				hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0x28);
			break;
		case 0x00b0:		/* sceSysconOutputClock */
			if (commandRequest.length == 1) {
				if (commandRequestData[0] == 1)
					clockOutputStart();
				else
					clockOutputStop();
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x00b1:		/* sceSysconCtrlWirelessPowerDown */
#if HW_IS(0x00102603)
			/* the wireless power line, P6.7 */
			if (commandRequest.length == 1) {
				if (commandRequestData[0] == 1)
					P6.7 = 1;
				else
					P6.7 = 0;
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
#else
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
#endif
			break;
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
		case 0x00b2:		/* sceSysconCtrlHostOutputViaDongle */
			if (commandRequest.length == 1) {
				if (boardMultiCnIsDongle() == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 4, 0);
				else if (commandRequestData[0] == 0) {
#if SOURCE_0103
					boardSetMultiCnOutput(3);	/* the slim's: the dongle off */
#else
					boardSetMultiCnOutput(0);
					powerDeviceSetPending(COMMAND_PENDING_DONGLE_OFF);
					dly_tsk(25);
#endif
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				} else if (serialGetState() != 0 && serialGetState() != 1)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 5, 0);
				else {
#if SOURCE_0103
					boardSetMultiCnOutput(2);	/* and on */
#else
					boardSetMultiCnOutput(1);
					powerDeviceSetPending(COMMAND_PENDING_DONGLE_ON);
					dly_tsk(10);
#endif
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				}
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
#endif
#if FW_SINCE(0x00090804)	/* not in 0x00090206 */
		case 0x00b3:		/* P6.7 */
#if BOARD_102603 && FW_SINCE(0x01000104)
			commandRequestAnswer[0] = 0;	/* P6.7 is an output (0x00b1) or not wired */
#elif FW_SINCE(0x01000104)
			commandRequestAnswer[0] = COMMAND_PIN_P67;
#else
			if (configGet()->CONFIG_WLAN_LINE)
				commandRequestAnswer[0] = P6.7;
			else
				commandRequestAnswer[0] = 0;
#endif
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 1);
			break;
#endif
#if FW_SINCE(0x0100060B)
		case 0x00b4:		/* P3.1, P6.7 and P3.0 */
			COMMAND_PINS.value = 0;
			COMMAND_PINS.bits.p31 = COMMAND_PIN_P31;
			COMMAND_PINS.bits.p67 = COMMAND_PIN_P67;
			COMMAND_PINS.bits.p30 = P3.0;
			commandRequestAnswer[0] = COMMAND_PINS.value;
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 1);
			break;
		case 0x00b5:		/* P3.1, while sleep factor bit 0 is set */
			if (commandRequest.length == 1) {
				if (sleepFactorGet() & 1) {
					COMMAND_PINS.value = commandRequestData[0];
					COMMAND_PIN_P31 = COMMAND_PINS.bits.p31;
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				} else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
#endif
		case 0x00c0:		/* a transition with its parameter */
#if SOURCE_0103
			/* the slim's: transition 0x5d too, and a 9-byte form with four bytes of
			   parameter */
			COMMAND_LOG_REQUEST();
			if (commandRequest.length == 4) {
				if (commandRequestData[0] == 0x00) {
					transition = 0x50;
					parameter = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x01) {
					transition = 0x51;
					parameter = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x10) {
					transition = 0x58;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x11) {
					transition = 0x59;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x12) {
					transition = 0x5a;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00800000;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x13) {
					transition = 0x5b;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x02000000;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x20) {
					transition = 0x5d;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x0000001b;
					stateSetWakeMask(parameter);
				} else {
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
					break;
				}
				if ((unsigned char)STATE_TRANSITION_NOMASK(transition) == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
			} else if (commandRequest.length == 9) {
				if (commandRequestData[0] == 0x00) {
					transition = 0x50;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x01) {
					transition = 0x51;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x10) {
					transition = 0x58;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x11) {
					transition = 0x59;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x12) {
					transition = 0x5a;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					parameter &= ~0x00800000;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x13) {
					transition = 0x5b;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					parameter &= ~0x02000000;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x20) {
					transition = 0x5d;
					parameter = COMMAND_PARAMETER4(commandRequestData);
					parameter &= ~0x0000001b;
					stateSetWakeMask(parameter);
				} else {
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
					break;
				}
				if ((unsigned char)STATE_TRANSITION_NOMASK(transition) == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			COMMAND_LOG_ANSWER();
			break;
#else
			if (commandRequest.length == 4) {
#if !FW_SINCE(0x01000104)
				if (commandRequestData[0] == 0x00) {
					transitionEvent.transition = 0x50;
					transitionEvent.wakeMask = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(transitionEvent.wakeMask);
				} else if (commandRequestData[0] == 0x01) {
					transitionEvent.transition = 0x51;
					transitionEvent.wakeMask = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(transitionEvent.wakeMask);
				} else {
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
					break;
				}
				if ((unsigned char)stateTransition(&transitionEvent) == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
#else
				if (commandRequestData[0] == 0x00) {
					transition = 0x50;
					parameter = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x01) {
					transition = 0x51;
					parameter = COMMAND_PARAMETER(commandRequestData);
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x10) {
					transition = 0x58;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x11) {
					transition = 0x59;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					stateSetWakeMask(parameter);
				} else if (commandRequestData[0] == 0x12) {
					transition = 0x5a;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00800000;
					stateSetWakeMask(parameter);
#if !COMMAND_10X
				} else if (commandRequestData[0] == 0x13) {
					transition = 0x5b;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x02000000;
					stateSetWakeMask(parameter);
#endif
				} else {
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
					break;
				}
#if COMMAND_10X
				if ((unsigned char)STATE_TRANSITION_NOMASK(transition) == 0)
#else
				if (STATE_TRANSITION_NOMASK(transition) == 0)
#endif
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
#endif
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
#endif
		case 0x00c1:		/* sceSysconErnieShutdown */
			COMMAND_LOG_REQUEST();
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			COMMAND_LOG_ANSWER();
			break;
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
		case 0x00c2:
			COMMAND_LOG_REQUEST();
			if (commandRequest.length == 1) {
				if (commandRequestData[0] == 0x5a) {
#if FW_SINCE(0x01000104)
					transition = 0x56;
#if COMMAND_10X || SOURCE_0103
					if ((unsigned char)STATE_TRANSITION(transition) == 0)
#else
					if (STATE_TRANSITION(transition) == 0)
#endif
#else
					transitionEvent.transition = 0x56;
					transitionEvent.wakeMask = stateGetWakeMask();
					stateSetWakeMask(transitionEvent.wakeMask);
					if ((unsigned char)stateTransition(&transitionEvent) == 0)
#endif
						hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
					else
						hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
				}
#if FW_SINCE(0x00090206)	/* 0x00090009 leaves the other commands unanswered */
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
#else
			}
#endif
			COMMAND_LOG_ANSWER();
			break;
#endif
#if FW_SINCE(0x00090903)
		case 0x00d0:		/* the handshake before the SNVS */
			if (commandRequest.length == 0x28) {
				task2SetFlag(3, 0x2000);
				hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0x28);
			break;
		case 0x00d1:
			task2SetFlag(3, 0x2000);
			hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
#if SOURCE_0103
			break;
#else
			return;
#endif
		case 0x00d2:		/* sceSysconSnvsReadData */
		case 0x00d3:		/* sceSysconSnvsWriteData */
			if ((commandRequest.length & 0x0f) == 0) {
				task2SetFlag(3, 0x2000);
				hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
				return;
			}
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			return;
#endif
		default:
			hostRequestAnswer(&commandRequest, 0x10, 0x3f, 0);
			return;
		}
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&commandRequest, HOST_REQUEST_RECEIVE);
		break;
#if SOURCE_0103
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&commandRequest, HOST_REQUEST_IDLE);
		break;
	case HOST_EVENT_OPEN:
		hostRequestSetState(&commandRequest, HOST_REQUEST_RECEIVE);
		break;
#endif
	}
}

/* request 0's commands a task handles (event flag 3, 0x2000) */
void commandRequestRun(void)
{
	switch (commandRequest.command) {
#if FW_SINCE(0x00090804)
	case 0x0086:
		commandLogInfo(commandRequestData, 0x86, commandLogDone);
		break;
	case 0x0087:
		commandLogStart(commandRequestData, 0x87, commandLogDone);
		break;
#endif
#if FW_SINCE(0x00090903)
	case 0x0088:
		commandLogRead(commandRequestData, commandRequestAnswer, 0x88, commandLogDone);
		break;
#endif
#if SOURCE_0103
	case 0x0089:
		commandLogHalt(commandRequestData, 0x89, commandLogDone);
		break;
#endif
	case 0x00a0:
		commandHandshake(commandRequestData, 0, commandHandshakeDone);
		break;
#if FW_SINCE(0x00090903)
	case 0x00d0:
		commandSnvsHandshake(commandRequestData, 0xd0, commandSnvsDone);
		break;
	case 0x00d1:
		commandSnvs00D1(commandRequestData, 0xd1, commandSnvsDone);
		break;
	case 0x00d2:
		commandSnvsRead(commandRequestData, commandRequestAnswer, 0xd2, commandSnvsDone);
		break;
	case 0x00d3:
		commandSnvsWrite(commandRequestData, commandRequestAnswer, 0xd3, commandSnvsDone);
		break;
#endif
	default:
		hostRequestAnswer(&commandRequest, 0x10, 0, 0);
		break;
	}
}

/* 0x00a0: the host's 0x28 bytes go to the handshake, which answers in their place */
int commandHandshake(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

#if FW_SINCE(0x00090903)
	memcpy(commandRequestAnswer, data, 0x28);
	result = handshakeCommand(commandRequestAnswer);
#else
	/* 0x00090804: the handshake answers in the command's data, copied after */
	result = handshakeCommand(data);
	memcpy(commandRequestAnswer, data, 0x28);
#endif
	done(command, result);
	return 0;
}

void commandHandshakeDone(unsigned char command, unsigned char result)
{
	if (result == 0)
		hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0x28);
	else
		hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x10, 0);
}

#if FW_SINCE(0x00090804)
/* 0x0087 (sceSysconLogStart) */
int commandLogStart(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

#if SOURCE_0103
	result = (unsigned char)logSetMode(LOG_MODE_STARTED, 1);
#else
	result = (unsigned char)logStart();
#endif
	done(command, result);
	return 0;
}

/* 0x0086 (sceSysconGetLogInfo) */
int commandLogInfo(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

	result = (unsigned char)logGetInfo((struct LogInfo *)commandRequestAnswer);
	done(command, result);
	return 0;
}

#endif
#if FW_SINCE(0x00090903)
/* 0x0088 (sceSysconLogReadData): the log's data from the offset the host gives */
int commandLogRead(unsigned char *data, unsigned char *answer, unsigned char command,
		   CommandDone done)
{
	unsigned char result;
	union CommandWord offset;
	struct LogRead *read;

	read = (struct LogRead *)answer;
	offset.byte[0] = data[0];
	offset.byte[1] = data[1];
	read->offset = offset.value;
	if (offset.value == 0)
		result = 1;
	else
		result = (unsigned char)logReadData(offset.value, read->data, &read->length);
	done(command, result);
	return 0;
}

#if SOURCE_0103
/* 0x0089: the slim's, the log halted */
int commandLogHalt(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

	result = (unsigned char)logSetMode(LOG_MODE_HALTED, 1);
	done(command, result);
	return 0;
}
#endif
#endif

#if FW_SINCE(0x00090804)
void commandLogDone(unsigned char command, unsigned char result)
{
	unsigned char status;
	unsigned char length;

	if (result == 0)
		status = 0;
	else if (result == 2)
		status = 7;
	else if (result == 3)
		status = 8;
	else
		status = 1;
	switch (command) {
	case 0x87:
#if SOURCE_0103
	case 0x89:
#endif
		length = 0;
		break;
	case 0x86:
		length = 8;
		break;
#if FW_SINCE(0x00090903)
	case 0x88:
		length = ((struct LogRead *)commandRequestAnswer)->length + 3;
		break;
#endif
	}
	hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, status, length);
}
#endif

#if FW_SINCE(0x00090903)
/* 0x00d0: the handshake before the SNVS; its 0x28 bytes are answered back */
int commandSnvsHandshake(unsigned char *data, unsigned char command, CommandDone done)
{
	int result;

	result = snvsHandshake(data);
	memcpy(commandRequestAnswer, data, 0x28);
	if (result != 0)
		result = 0x12;
	done(command, result);
	return result;
}

int commandSnvs00D1(unsigned char *data, unsigned char command, CommandDone done)
{
	int result;

	result = snvsLock(data);
	done(command, result);
	return result;
}

/* 0x00d2 (sceSysconSnvsReadData) */
int commandSnvsRead(unsigned char *data, unsigned char *answer, unsigned char command,
		    CommandDone done)
{
	int result;

	result = snvsRead(data, answer);
	done(command, result);
	return result;
}

/* 0x00d3 (sceSysconSnvsWriteData) */
int commandSnvsWrite(unsigned char *data, unsigned char *answer, unsigned char command,
		     CommandDone done)
{
	int result;

	result = snvsWrite(data, answer);
	done(command, result);
	return result;
}

void commandSnvsDone(unsigned char command, unsigned char result)
{
	unsigned char length;

	switch (result) {
	case 0x00:
		result = 0x00;
		break;
	case 0x13:
		result = 0x13;
		break;
	case 0x14:
		result = 0x14;
		break;
	case 0x15:
		result = 0x15;
		break;
	}
	switch (command) {
	case 0xd0:
		length = 0x28;
		break;
	case 0xd1:
		length = 0;
		break;
	case 0xd2:
		length = 0x30;
		break;
	case 0xd3:
		length = 0x10;
		break;
	default:
		length = 0;
		break;
	}
	hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, length);
}
#endif

#if BOARD_DOL1001
/* DOL-1001 has no event log: the hooks of the commands it would log */
static void commandLogRequest(void)
{
}

static void commandLogAnswer(void)
{
}
#endif
