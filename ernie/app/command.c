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
#include "clockout.h"
#include "command.h"
#include "host.h"
#include "kernel.h"
#include "rtc.h"
#include "version.h"

extern struct SystemMode *sub_2AB39(void);
extern int sub_2D646(unsigned int *value);
extern unsigned char sub_2BB99(void);
extern unsigned char sub_2BBE9(void);
extern int sub_2ED36(void);
extern int sub_2D626(unsigned int *value);
extern unsigned char sub_2D662(void);
extern unsigned char sub_2D67C(void);
extern unsigned char sub_2D685(void);
extern void sub_2C2ED(int, int);
extern unsigned char sub_1D0E6(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char sub_1D124(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char sub_2C15B(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char sub_2C199(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char sub_27642(void);
extern void sub_275F0(int);
extern void sub_21F33(int);
extern unsigned char sub_1B538(void);
extern void sub_2AA60(unsigned long);
extern unsigned char sub_2AB49(int);
extern unsigned char sub_2B61F(unsigned char *buf);
extern unsigned char sub_2DB73(void);
extern unsigned char sub_2DAEA(unsigned char *buf);
extern unsigned char sub_2DC1F(unsigned int offset, unsigned char *data, unsigned char *length);
extern int sub_2ED3C(unsigned char *data);
extern int sub_2ED5A(unsigned char *data);
extern int sub_2ED75(unsigned char *data, unsigned char *answer);
extern int sub_2EE8D(unsigned char *data, unsigned char *answer);

/* a task-side command is done: the answer from its result */
typedef void (*CommandDone)(unsigned char command, unsigned char result);

int commandHandshake(unsigned char *data, unsigned char command, CommandDone done);
void commandHandshakeDone(unsigned char command, unsigned char result);
int commandLogStart(unsigned char *data, unsigned char command, CommandDone done);
int commandLogInfo(unsigned char *data, unsigned char command, CommandDone done);
int commandLogRead(unsigned char *data, unsigned char *answer, unsigned char command,
		   CommandDone done);
void commandLogDone(unsigned char command, unsigned char result);
int commandSnvsHandshake(unsigned char *data, unsigned char command, CommandDone done);
int commandSnvs00D1(unsigned char *data, unsigned char command, CommandDone done);
int commandSnvsRead(unsigned char *data, unsigned char *answer, unsigned char command,
		    CommandDone done);
int commandSnvsWrite(unsigned char *data, unsigned char *answer, unsigned char command,
		     CommandDone done);
void commandSnvsDone(unsigned char command, unsigned char result);

/* request 0: its answer and its command's data */
struct HostRequest commandRequest;
unsigned char commandRequestAnswer[0x40];
unsigned char commandRequestData[0x40];

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
		unsigned char	reserved:4;
	} bits;
};

union CommandPins commandPins;

/* a word the commands carry least significant byte first */
union CommandWord {
	unsigned int	value;
	unsigned char	byte[2];
	struct HostLinkConfig config;
};

/* the commands request 0 takes before the handshake */
static const unsigned int commandBaseRequests[6] = {
	0x0080, 0x0083, 0x0084, 0x0085, 0x00a0, 0x00c0,
};

/* the time stamp's digits in appTimeStamp: the year, month, day, hour and minute */
static const unsigned char timeStampDigits[12] = {
	8, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 23,
};

/* 0x0001 (sceSysconGetErnieVersion) */
unsigned char hostCommandErnieVersion(unsigned char *out, unsigned char *length)
{
	getErnieVersion(out);
	*length = 4;
	return 0;
}

/* 0x0002 (sceSysconGetTimeStamp): the build's date as 12 digits, YYYYMMDDhhmm */
unsigned char hostCommandTimeStamp(unsigned char *out, unsigned char *length)
{
	unsigned char i;

	for (i = 0; i < 12; i++)
		out[i] = appTimeStamp[timeStampDigits[i]];
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

/* 0x0010 (sceSysconGetWakeupFactor) */
unsigned char hostCommandWakeupFactor(unsigned char *out, unsigned char *length)
{
	union CommandWord factor;

	factor.value = *(unsigned int *)sub_2AB39();
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
	unsigned int level;

	hostGetLinkLevel(&level);
	memcpy(out, getAlarm().byte, 4);
	if (level != 0) {
		out[4] = getAlarmMode();
		*length = 5;
	} else
		*length = 4;
	return 0;
}

/* 0x0013: the word of the module at 0x2d646 */
unsigned char hostCommand0013(unsigned char *out, unsigned char *length)
{
	union CommandWord value;

	sub_2D646(&value.value);
	out[0] = value.byte[0];
	out[1] = value.byte[1];
	*length = 2;
	return 0;
}

/* 0x0014: the pins */
unsigned char hostCommandPins(unsigned char *out, unsigned char *length)
{
	commandPins.bits.p30 = P3.0;
	commandPins.bits.present = 1;
	commandPins.bits.p31 = P3.1;
	commandPins.bits.p76 = P7.6;
	*out = commandPins.value & 0x0f;
	*length = 1;
	return 0;
}

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

/* 0x00c0's parameter: the data's three bytes in the window at 0xfc000000 */
#define COMMAND_PARAMETER(data)	(((unsigned long)(data)[1] & 0xff) |			\
				 ((unsigned long)(data)[2] << 8 & 0xff00) |		\
				 ((unsigned long)(data)[3] << 16 & 0xff0000) |		\
				 0xfc000000)

/* request 0's command came in, or its answer went out */
void commandRequestNotify(void)
{
	unsigned char i;
	union RtcTick tick;
	unsigned int offset;
	unsigned char size;
	unsigned char transition;
	unsigned char result;
	unsigned char linkSize;
	unsigned long parameter;
	union CommandWord word;

	if (!sub_2BB99()) {
		for (i = 0; i < 6; i++) {
			if (commandBaseRequests[i] == commandRequest.command)
				goto allowed;
		}
		hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		sub_2BBE9();
		return;
	}
allowed:
	switch (commandRequest.event) {
	case HOST_EVENT_COMMAND:
		if (commandRequest.command >= 0x00d2 && commandRequest.command < 0x00d4 &&
		    sub_2ED36() == 0) {
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x11, 0);
			return;
		}
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
			if (commandRequest.length == 4) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setClock(tick.value);
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x0082:		/* the alarm, and its mode */
			if (commandRequest.length == 4) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setAlarm(tick.value);
				setAlarmMode(0);
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else if (commandRequest.length == 5 && commandRequestData[4] < 2) {
				tick.byte[0] = commandRequestData[0];
				tick.byte[1] = commandRequestData[1];
				tick.byte[2] = commandRequestData[2];
				tick.byte[3] = commandRequestData[3];
				setAlarm(tick.value);
				setAlarmMode(commandRequestData[4]);
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x0083:
			if (commandRequest.length == 2) {
				word.byte[0] = commandRequestData[0];
				word.byte[1] = commandRequestData[1];
				sub_2D626(&word.value);
				result = sub_2D662();
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x0084:
			result = sub_2D67C();
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
		case 0x0085:
			result = sub_2D685();
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, result, 0);
			break;
		case 0x0086:		/* the log: sceSysconGetLogInfo */
		case 0x0087:		/* sceSysconLogStart */
		case 0x0088:		/* sceSysconLogReadData */
			sub_2C2ED(3, 0x2000);
			hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			break;
		case 0x0090:		/* sceSysconReadScratchPad */
			if (commandRequest.length != 3) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
				break;
			}
			offset = commandRequestData[0] | (unsigned int)commandRequestData[1] << 8;
			size = commandRequestData[2];
			if (sub_1D0E6(offset, size, commandRequestAnswer) == 0)
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
			if (sub_1D124(offset, size, &commandRequestData[3]) == 0)
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
			if (sub_2C15B(offset, size, commandRequestAnswer) == 0)
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
			if (sub_2C199(offset, size, &commandRequestData[3]) == 0)
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			break;
		case 0x00a0:		/* the handshake */
			hostGetLinkSize(&linkSize);
			if (linkSize == 0) {
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x20, 0);
				break;
			}
			if (commandRequest.length == 0x28) {
				sub_2C2ED(3, 0x2000);
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
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			break;
		case 0x00b2:		/* sceSysconCtrlHostOutputViaDongle */
			if (commandRequest.length == 1) {
				if (sub_27642() == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 4, 0);
				else if (commandRequestData[0] == 0) {
					sub_275F0(0);
					sub_21F33(0x20);
					dly_tsk(25);
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				} else if (sub_1B538() != 0 && sub_1B538() != 1)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 5, 0);
				else {
					sub_275F0(1);
					sub_21F33(0x10);
					dly_tsk(10);
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				}
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x00b3:		/* P6.7 */
			commandRequestAnswer[0] = P6.7;
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 1);
			break;
		case 0x00c0:		/* a transition with its parameter */
			if (commandRequest.length == 4) {
				if (commandRequestData[0] == 0x00) {
					transition = 0x50;
					parameter = COMMAND_PARAMETER(commandRequestData);
					sub_2AA60(parameter);
				} else if (commandRequestData[0] == 0x01) {
					transition = 0x51;
					parameter = COMMAND_PARAMETER(commandRequestData);
					sub_2AA60(parameter);
				} else if (commandRequestData[0] == 0x10) {
					transition = 0x58;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					sub_2AA60(parameter);
				} else if (commandRequestData[0] == 0x11) {
					transition = 0x59;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00000400;
					sub_2AA60(parameter);
				} else if (commandRequestData[0] == 0x12) {
					transition = 0x5a;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x00800000;
					sub_2AA60(parameter);
				} else if (commandRequestData[0] == 0x13) {
					transition = 0x5b;
					parameter = COMMAND_PARAMETER(commandRequestData);
					parameter &= ~0x02000000;
					sub_2AA60(parameter);
				} else {
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
					break;
				}
				if (sub_2AB49(transition) == 0)
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
				else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x00c1:		/* sceSysconErnieShutdown */
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			break;
		case 0x00c2:
			if (commandRequest.length == 1) {
				if (commandRequestData[0] == 0x5a) {
					transition = 0x56;
					if (sub_2AB49(transition) == 0)
						hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
					else
						hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 6, 0);
				} else
					hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0, 0);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
			break;
		case 0x00d0:		/* the handshake before the SNVS */
			if (commandRequest.length == 0x28) {
				sub_2C2ED(3, 0x2000);
				hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			} else
				hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0x28);
			break;
		case 0x00d1:
			sub_2C2ED(3, 0x2000);
			hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
			return;
		case 0x00d2:		/* sceSysconSnvsReadData */
		case 0x00d3:		/* sceSysconSnvsWriteData */
			if ((commandRequest.length & 0x0f) == 0) {
				sub_2C2ED(3, 0x2000);
				hostRequestSetState(&commandRequest, HOST_REQUEST_BUSY);
				return;
			}
			hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, 2, 0);
			return;
		default:
			hostRequestAnswer(&commandRequest, 0x10, 0x3f, 0);
			return;
		}
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&commandRequest, HOST_REQUEST_RECEIVE);
		break;
	}
}

/* request 0's commands a task handles (event flag 3, 0x2000) */
void commandRequestRun(void)
{
	switch (commandRequest.command) {
	case 0x0086:
		commandLogInfo(commandRequestData, 0x86, commandLogDone);
		break;
	case 0x0087:
		commandLogStart(commandRequestData, 0x87, commandLogDone);
		break;
	case 0x0088:
		commandLogRead(commandRequestData, commandRequestAnswer, 0x88, commandLogDone);
		break;
	case 0x00a0:
		commandHandshake(commandRequestData, 0, commandHandshakeDone);
		break;
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
	default:
		hostRequestAnswer(&commandRequest, 0x10, 0, 0);
		break;
	}
}

/* 0x00a0: the host's 0x28 bytes go to the handshake, which answers in their place */
int commandHandshake(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

	memcpy(commandRequestAnswer, data, 0x28);
	result = sub_2B61F(commandRequestAnswer);
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

/* 0x0087 (sceSysconLogStart) */
int commandLogStart(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

	result = sub_2DB73();
	done(command, result);
	return 0;
}

/* 0x0086 (sceSysconGetLogInfo) */
int commandLogInfo(unsigned char *data, unsigned char command, CommandDone done)
{
	unsigned char result;

	result = sub_2DAEA(commandRequestAnswer);
	done(command, result);
	return 0;
}

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
		result = sub_2DC1F(offset.value, read->data, &read->length);
	done(command, result);
	return 0;
}

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
		length = 0;
		break;
	case 0x86:
		length = 8;
		break;
	case 0x88:
		length = ((struct LogRead *)commandRequestAnswer)->length + 3;
		break;
	}
	hostRequestAnswer(&commandRequest, HOST_REQUEST_ANSWER, status, length);
}

/* 0x00d0: the handshake before the SNVS; its 0x28 bytes are answered back */
int commandSnvsHandshake(unsigned char *data, unsigned char command, CommandDone done)
{
	int result;

	result = sub_2ED3C(data);
	memcpy(commandRequestAnswer, data, 0x28);
	if (result != 0)
		result = 0x12;
	done(command, result);
	return result;
}

int commandSnvs00D1(unsigned char *data, unsigned char command, CommandDone done)
{
	int result;

	result = sub_2ED5A(data);
	done(command, result);
	return result;
}

/* 0x00d2 (sceSysconSnvsReadData) */
int commandSnvsRead(unsigned char *data, unsigned char *answer, unsigned char command,
		    CommandDone done)
{
	int result;

	result = sub_2ED75(data, answer);
	done(command, result);
	return result;
}

/* 0x00d3 (sceSysconSnvsWriteData) */
int commandSnvsWrite(unsigned char *data, unsigned char *answer, unsigned char command,
		     CommandDone done)
{
	int result;

	result = sub_2EE8D(data, answer);
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
