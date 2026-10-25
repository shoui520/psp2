/*
 * The input devices' requests: the controls' (request 1, commands 0x0180-0x01a1, answered by
 * task 3 when event flag 2 has bit 0x0010), the touch panels' (request 3, 0x0380-0x03b9) and
 * the motion sensor's (request 4, 0x0480-0x0488), which task 3 runs from the scan's ticks
 * (devicesRequestRun). A touch or motion command may take several runs: the bootloader's
 * commands wait for the controller's CHG line, the motion sensor's for its CSI01 transfers.
 * Beside them, the streams the host reads the frames from (0x0300 both panels' parts, 0x0301
 * and 0x0302 one panel's, 0x0400 the motion sensor's), and the controls' reads 0x0100-0x0113
 * that the command table answers at once.
 *
 * The answers' status: 0 done, 0x02 failed (or not in that mode), 0x03 not ready, 0x04 the
 * motion sensor is not set up, 0x3b the data's length is wrong, 0x3c the data is wrong, 0x3e
 * not taken here before the handshake, 0x3f not known.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "analog.h"
#include "csi01.h"
#include "host.h"
#include "input.h"
#include "inputreq.h"
#include "kernel.h"
#include "motion.h"
#include "tau.h"
#include "touch.h"

/* touchPending: touchRequest's command waits for the run */
#define TOUCH_PENDING		0x80

/* motionFlags */
#define MOTION_PENDING		0x80	/* motionRequest's command waits for the run */
#define MOTION_TRANSFER		0x40	/* the run waits for the end of a CSI01 transfer */
#define MOTION_ANSWER		0x20	/* and then answers */
#define MOTION_BOOT		0x02	/* the sensor is in its bootloader (0x0484) */
#define MOTION_STARTED		0x01	/* the sensor's frames go to stream 0x0400 (0x0481) */

/* a request's command and its handler */
struct InputCommand {
	unsigned int	command;
	void		(*handler)(void);
};

/* the requests' data, and their commands' state */
struct InputRequests {
	unsigned char	touchPending;		/* TOUCH_PENDING */
	unsigned char	reserved;		/* cleared by inputRequestInit, otherwise unused */
	unsigned char	touchData[0x20];
	unsigned char	touchAnswer[0x10];
	unsigned char	touchPhase;		/* the step of a command that takes several runs */
	unsigned char	touchTimer;		/* the runs it waits at most */
	unsigned char	motionFlags;		/* MOTION_* */
	unsigned char	motionCommand;		/* the command's low bits: its motionCommands entry */
	unsigned char	motionData[0x20];
	unsigned char	motionAnswer[0x40];
	unsigned char	motionPhase;		/* the step of a command that takes several runs */
	unsigned char	motionDelay;		/* the runs to skip before the next step */
	unsigned char	controlsData[0x10];
	union {
		unsigned char	byte[0x10];
		unsigned int	word[8];
	} controlsAnswer;
};

extern unsigned char sub_11D1B(void);
extern unsigned char sub_275E0(void);
extern unsigned char sub_275F0(unsigned char value);
extern unsigned char sub_2BB99(void);
extern unsigned char sub_2BBE9(void);
extern void sub_2C342(ID flgid, FLGPTN setptn);
extern void sub_2DAE1(int);


void controlsRequestNotify(void);
void touchRequestNotify(void);
void motionRequestNotify(void);
void controlsReadAnalog(unsigned char *out);
void controlsReadAnalog0(unsigned char *out);
void controlsReadAnalog1(unsigned char *out);
void touchStream0301Prepare(void);
void touchStream0302Prepare(void);
void touchStream0300Prepare(void);
void motionStreamPrepare(void);
static void controlsRequest0180(void);
static void controlsRequest0181(void);
static void controlsRequest0182(void);
static void controlsRequest0183(void);
static void controlsRequest0183Done(unsigned char command, unsigned char result);
static void controlsRequest0184(void);
static void controlsRequest0184Done(unsigned char command, unsigned char result);
static void controlsRequest0185(void);
static void controlsRequest0190(void);
static void controlsRequest01A0(void);
static void controlsRequest01A1(void);
static void touchRequestDeviceInfo(void);
static void touchRequestStart(void);
static void touchRequestWrite0(void);
static void touchRequestRead0(void);
static void touchRequestWrite1(void);
static void touchRequestRead1(void);
static void touchRequestSetPower(void);
static void touchRequestStatus(void);
static void touchRequestSetGrip(void);
static void touchRequestEnterBootloader0(void);
static void touchRequestUnlock0(void);
static void touchRequestLoad0(void);
static void touchRequestFlash0(void);
static void touchRequestBootloaderStatus0(void);
static void touchRequestBackup0(void);
static void touchRequestReset0(void);
static void touchRequestConfigVersion0(void);
static void touchRequestUserData0(void);
static void touchRequestSelfTest0(void);
static void touchRequestEnterBootloader1(void);
static void touchRequestUnlock1(void);
static void touchRequestLoad1(void);
static void touchRequestFlash1(void);
static void touchRequestBootloaderStatus1(void);
static void touchRequestBackup1(void);
static void touchRequestReset1(void);
static void touchRequestConfigVersion1(void);
static void touchRequestUserData1(void);
static void touchRequestSelfTest1(void);
static void motionRequest0480(void);
static void motionRequest0481(void);
static void motionRequestUnknown(void);
static void motionRequest0483(void);
static void motionRequest0484(void);
static void motionRequest0485(void);
static void motionRequest0486(void);
static void motionRequest0487(void);
static void motionRequest0488(void);

struct HostRequest controlsRequest;		/* request 1 */
struct HostRequest touchRequest;		/* request 3 */
struct HostRequest motionRequest;		/* request 4 */
static struct InputRequests inputRequests;
struct HostStream hostStream0300;
struct HostStream hostStream0301;
struct HostStream hostStream0302;
struct HostStream hostStream0400;
static unsigned char touchFlashExtra;		/* added to a full buffer's length (0x03a3), never
						   set */

static const struct InputCommand controlsCommands[] = {
	{ 0x0180, controlsRequest0180 },
	{ 0x0181, controlsRequest0181 },
	{ 0x0182, controlsRequest0182 },
	{ 0x0183, controlsRequest0183 },
	{ 0x0184, controlsRequest0184 },
	{ 0x0185, controlsRequest0185 },
	{ 0x0190, controlsRequest0190 },
	{ 0x01a0, controlsRequest01A0 },
	{ 0x01a1, controlsRequest01A1 },
};

static const struct InputCommand touchCommands[] = {
	{ 0x0380, touchRequestDeviceInfo },
	{ 0x0381, touchRequestStart },
	{ 0x0383, touchRequestWrite0 },
	{ 0x0384, touchRequestRead0 },
	{ 0x0385, touchRequestWrite1 },
	{ 0x0386, touchRequestRead1 },
	{ 0x0387, touchRequestSetPower },
	{ 0x0388, touchRequestStatus },
	{ 0x0389, touchRequestSetGrip },
	{ 0x03a0, touchRequestEnterBootloader0 },
	{ 0x03a1, touchRequestUnlock0 },
	{ 0x03a2, touchRequestLoad0 },
	{ 0x03a3, touchRequestFlash0 },
	{ 0x03a4, touchRequestBootloaderStatus0 },
	{ 0x03a5, touchRequestBackup0 },
	{ 0x03a6, touchRequestReset0 },
	{ 0x03a7, touchRequestConfigVersion0 },
	{ 0x03a8, touchRequestUserData0 },
	{ 0x03a9, touchRequestSelfTest0 },
	{ 0x03b0, touchRequestEnterBootloader1 },
	{ 0x03b1, touchRequestUnlock1 },
	{ 0x03b2, touchRequestLoad1 },
	{ 0x03b3, touchRequestFlash1 },
	{ 0x03b4, touchRequestBootloaderStatus1 },
	{ 0x03b5, touchRequestBackup1 },
	{ 0x03b6, touchRequestReset1 },
	{ 0x03b7, touchRequestConfigVersion1 },
	{ 0x03b8, touchRequestUserData1 },
	{ 0x03b9, touchRequestSelfTest1 },
};

/* request 4's commands, 0x0480 to 0x0488 (bit 7 ignored) */
static void (* const motionCommands[])(void) = {
	motionRequest0480,
	motionRequest0481,
	motionRequestUnknown,
	motionRequest0483,
	motionRequest0484,
	motionRequest0485,
	motionRequest0486,
	motionRequest0487,
	motionRequest0488,
};

void inputRequestInit(void)
{
	inputRequests.touchPending = 0;
	inputRequests.reserved = 0;
	inputRequests.touchPhase = 0;
	memset(inputRequests.touchData, 0, sizeof(inputRequests.touchData));
	memset(inputRequests.touchAnswer, 0, sizeof(inputRequests.touchAnswer));
	inputRequests.motionFlags = 0;
	inputRequests.motionCommand = 0;
	inputRequests.motionPhase = 0;
	inputRequests.motionDelay = 0;
	memset(inputRequests.motionData, 0, sizeof(inputRequests.motionData));
	memset(inputRequests.motionAnswer, 0, sizeof(inputRequests.motionAnswer));

	controlsRequest.id = 1;
	controlsRequest.size = sizeof(inputRequests.controlsData);
	controlsRequest.data = inputRequests.controlsData;
	controlsRequest.answer = inputRequests.controlsAnswer.byte;
	controlsRequest.notify = controlsRequestNotify;
	controlsRequest.state = HOST_REQUEST_RECEIVE;
	touchRequest.id = 3;
	touchRequest.size = sizeof(inputRequests.touchData);
	touchRequest.data = inputRequests.touchData;
	touchRequest.answer = inputRequests.touchAnswer;
	touchRequest.notify = touchRequestNotify;
	touchRequest.state = HOST_REQUEST_RECEIVE;
	motionRequest.id = 4;
	motionRequest.size = 0x40;		/* more than motionData holds */
	motionRequest.data = inputRequests.motionData;
	motionRequest.answer = inputRequests.motionAnswer;
	motionRequest.notify = motionRequestNotify;
	motionRequest.state = HOST_REQUEST_RECEIVE;

	hostStream0400.state = 0;
	hostStream0400.block = 0xff;
	hostStream0400.first = 0;
	hostStream0400.length = MOTION_PACKET_SIZE;
	hostStream0400.blocks[0] = motionFrames[0];
	hostStream0400.blocks[1] = motionFrames[1];
	hostStream0400.prepare = motionStreamPrepare;
	hostStream0300.state = 0;
	hostStream0300.block = 0xff;
	hostStream0300.first = 0;
	hostStream0300.length = 2 * TOUCH_PANEL_SIZE;
	hostStream0300.blocks[0] = touchFrames[0].panel[0];
	hostStream0300.blocks[1] = touchFrames[1].panel[0];
	hostStream0300.prepare = touchStream0300Prepare;
	hostStream0301.state = 0;
	hostStream0301.block = 0xff;
	hostStream0301.first = 0;
	hostStream0301.length = TOUCH_PANEL_SIZE;
	hostStream0301.blocks[0] = touchFrames[0].panel[0];
	hostStream0301.blocks[1] = touchFrames[1].panel[0];
	hostStream0301.prepare = touchStream0301Prepare;
	hostStream0302.state = 0;
	hostStream0302.block = 0xff;
	hostStream0302.first = 0;
	hostStream0302.length = TOUCH_PANEL_SIZE;
	hostStream0302.blocks[0] = touchFrames[0].panel[1];
	hostStream0302.blocks[1] = touchFrames[1].panel[1];
	hostStream0302.prepare = touchStream0302Prepare;
}

/* request 3's command came in, or its answer went out */
void touchRequestNotify(void)
{
	switch (touchRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		touchRequest.state = HOST_REQUEST_BUSY;
		inputRequests.touchPending = TOUCH_PENDING;
		inputRequests.touchPhase = 0;
		break;
	case HOST_EVENT_ANSWERED:
		touchRequest.state = HOST_REQUEST_RECEIVE;
		break;
	}
}

/* request 4's: 0x0480 to 0x0489 are answered at once while the sensor is not set up */
void motionRequestNotify(void)
{
	switch (motionRequest.event) {
	case HOST_EVENT_COMMAND:
		if (motion.status == MOTION_ABSENT) {
			switch (motionRequest.command) {
			case 0x0480:
				inputRequests.motionAnswer[0] = motion.info[1].byte[0];
				inputRequests.motionAnswer[1] = motion.info[1].byte[1];
				inputRequests.motionAnswer[2] = motion.info[0].byte[0];
				inputRequests.motionAnswer[3] = motion.info[0].byte[1];
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0, 4);
				break;
			case 0x0481:
			case 0x0483:
			case 0x0484:
			case 0x0485:
			case 0x0486:
			case 0x0487:
			case 0x0488:
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x04, 0);
				return;
			default:
				hostRequestAnswer(&motionRequest, HOST_REQUEST_UNKNOWN, 0, 0);
				return;
			}
		} else {
			hostRequestSetState(&motionRequest, HOST_REQUEST_BUSY);
			inputRequests.motionCommand = motionRequest.command & 0x7f;
			if (inputRequests.motionCommand < 9) {
				inputRequests.motionFlags = (inputRequests.motionFlags & 0x0f) |
							    MOTION_PENDING;
				inputRequests.motionPhase = 0;
				inputRequests.motionDelay = 0;
			} else
				hostRequestAnswer(&motionRequest, HOST_REQUEST_UNKNOWN, 0, 0);
		}
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&motionRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_NONE:
		break;
	}
}

/* request 1's: task 3 runs the command (controlsRequestRun) */
void controlsRequestNotify(void)
{
	switch (controlsRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		controlsRequest.state = HOST_REQUEST_BUSY;
		sub_2C342(2, 0x0010);
		break;
	case HOST_EVENT_ANSWERED:
		controlsRequest.state = HOST_REQUEST_RECEIVE;
		break;
	}
}

/* request 3's command, from touchCommands */
static void touchRequestRun(void)
{
	unsigned char i;

	if (inputRequests.touchPending != 0) {
		if (!sub_2BB99()) {
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			sub_2BBE9();
		} else {
			for (i = 0; i < sizeof(touchCommands) / sizeof(touchCommands[0]); i++) {
				if (touchCommands[i].command == touchRequest.command) {
					touchCommands[i].handler();
					return;
				}
			}
			hostRequestAnswer(&touchRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
			inputRequests.touchPending = 0;
		}
	}
}

/* request 4's command, from motionCommands (after motionDelay runs), then the end of its
   CSI01 transfer */
static void motionRequestRun(void)
{
	unsigned char i;

	i = 0;
	if (inputRequests.motionFlags & MOTION_PENDING) {
		if (inputRequests.motionDelay != 0)
			inputRequests.motionDelay--;
		else if (!sub_2BB99()) {
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			sub_2BBE9();
			return;
		} else
			motionCommands[inputRequests.motionCommand]();
	}
	if (inputRequests.motionFlags & MOTION_TRANSFER) {
		for (;;) {
			dly_tsk(1);
			if (csi01GetState() == CSI01_OPEN) {
				inputRequests.motionFlags &= ~MOTION_TRANSFER;
				if (inputRequests.motionFlags & MOTION_ANSWER) {
					inputRequests.motionFlags &= ~(MOTION_PENDING | MOTION_ANSWER);
					hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0,
							  motionRequest.answerLength);
				}
				break;
			}
			if (i++ > 10)
				continue;
		}
	}
}

/* requests 3 and 4 */
void devicesRequestRun(void)
{
	touchRequestRun();
	motionRequestRun();
	return;
}

/* request 1's command, from controlsCommands */
void controlsRequestRun(void)
{
	unsigned char i;

	if (!sub_2BB99()) {
		hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		sub_2BBE9();
	} else {
		for (i = 0; i < sizeof(controlsCommands) / sizeof(controlsCommands[0]); i++) {
			if (controlsCommands[i].command == controlsRequest.command) {
				controlsCommands[i].handler();
				return;
			}
		}
		hostRequestAnswer(&controlsRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
	}
}

/* the buttons, active low: P8, P5's low half with P11.5 and P4.6, P11.2-3, and three bits of
   inputState1 */
void controlsReadButtons(unsigned char *out)
{
	out[0] = P8;
	out[1] = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
				  (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	out[2] = (P11 >> 2 & 3) | 0xfc;
	out[3] = ((inputState1 ^ 0x0a) & 0x0e) | 0x74;
}

/* the analog sticks, from the set analogFlags names */
void controlsReadAnalog(unsigned char *out)
{
	if (analogFlags & 1)
		controlsReadAnalog0(out);
	else
		controlsReadAnalog1(out);
}

void controlsReadAnalog0(unsigned char *out)
{
	out[0] = analogSticks0[0];
	out[1] = analogSticks0[1];
	out[2] = analogSticks0[2];
	out[3] = analogSticks0[3];
}

void controlsReadAnalog1(unsigned char *out)
{
	out[0] = analogSticks1[0];
	out[1] = analogSticks1[1];
	out[2] = analogSticks1[2];
	out[3] = analogSticks1[3];
}

/* 0x0100 (sceSysconGetControlsInfo) */
unsigned char hostCommandControlsInfo(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
	*length = 4;
	return 0;
}

/* 0x0111 */
unsigned char hostCommandAnalog(unsigned char *out, unsigned char *length)
{
	controlsReadAnalog(out);
	*length = 4;
	return 0;
}

/* 0x0113 */
unsigned char hostCommandAnalogBoth(unsigned char *out, unsigned char *length)
{
	controlsReadAnalog0(out);
	controlsReadAnalog1(out + 4);
	*length = 8;
	return 0;
}

/* 0x0101 */
unsigned char hostCommandControlsAnalog(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
	controlsReadAnalog(out + 4);
	*length = 8;
	return 0;
}

/* 0x0104 */
unsigned char hostCommandControlsAnalogBoth(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
	controlsReadAnalog0(out + 4);
	controlsReadAnalog1(out + 8);
	*length = 12;
	return 0;
}

/* 0x0103 (sceSysconGetMultiCnInfo) */
unsigned char hostCommandMultiCnInfo(unsigned char *out, unsigned char *length)
{
	if (sub_11D1B())
		out[0] = 0x80;
	else
		out[0] = 0;
	out[1] = analogGetMultiCn();
	out[2] = sub_275E0();
	out[3] = 0;
	*length = 4;
	return 0;
}

/* 0x0112: each axis's last sample */
unsigned char hostCommand0112(unsigned char *out, unsigned char *length)
{
	analogGetSamples((unsigned int *)out);
	*length = 8;
	return 0;
}

/* 0x0102: the buttons and each axis's last sample */
unsigned char hostCommand0102(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
	analogGetSamples((unsigned int *)(out + 4));
	*length = 12;
	return 0;
}

static void controlsRequest0180(void)
{
	if (controlsRequest.length == 1) {
		if (controlsRequest.data[0] != 0)
			analogSetMode(controlsRequest.data[0]);
		else
			analogStop();
		controlsRequest.status = 0;
	} else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

static void controlsRequest0181(void)
{
	if (controlsRequest.length == 1) {
		if (controlsRequest.data[0] == 0)
			analogCalibrationReset();
		else if (controlsRequest.data[0] == 1) {
			analogCalibrationReset();
			analogCalibrationSave();
		} else if (controlsRequest.data[0] == 2) {
			analogCalibrationDefault();
			analogCalibrationSave();
		}
		controlsRequest.status = 0;
	} else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

static void controlsRequest0182(void)
{
	analogCalibrationGet(inputRequests.controlsAnswer.byte);
	controlsRequest.answerLength = sizeof(inputRequests.controlsAnswer);
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

/* answered by controlsRequest0183Done */
static void controlsRequest0183(void)
{
	analogStoreRead(inputRequests.controlsAnswer.byte, 0, controlsRequest0183Done);
}

static void controlsRequest0183Done(unsigned char command, unsigned char result)
{
	if (result == 0)
		controlsRequest.status = 0;
	else
		controlsRequest.status = 0x02;
	controlsRequest.answerLength = sizeof(inputRequests.controlsAnswer);
	hostRequestAnswerReady(&controlsRequest.state);
}

/* answered by controlsRequest0184Done */
static void controlsRequest0184(void)
{
	if (controlsRequest.length == sizeof(inputRequests.controlsData))
		analogStoreWrite(inputRequests.controlsData, 0, controlsRequest0184Done);
	else {
		controlsRequest.status = 0x3b;
		controlsRequest.answerLength = 0;
		hostRequestAnswerReady(&controlsRequest.state);
	}
}

static void controlsRequest0184Done(unsigned char command, unsigned char result)
{
	if (result == 0) {
		controlsRequest.status = 0;
		sub_2DAE1(3);
	} else
		controlsRequest.status = 0x02;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
}

/* six fixed words */
static void controlsRequest0185(void)
{
	inputRequests.controlsAnswer.word[0] = 0xff00;
	inputRequests.controlsAnswer.word[1] = 0x0001;
	inputRequests.controlsAnswer.word[2] = 0x0100;
	inputRequests.controlsAnswer.word[3] = 0x00ff;
	inputRequests.controlsAnswer.word[4] = 0x0002;
	inputRequests.controlsAnswer.word[5] = 0x0000;
	controlsRequest.status = 0;
	controlsRequest.answerLength = 12;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

static void controlsRequest0190(void)
{
	if (controlsRequest.length == 2) {
		if (sub_275F0(controlsRequest.data[0]) != 0)
			controlsRequest.status = 0x3c;
		else
			controlsRequest.status = 0;
	} else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

static void controlsRequest01A0(void)
{
	if (controlsRequest.length == 1)
		controlsRequest.status = 0;
	else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

static void controlsRequest01A1(void)
{
	if (controlsRequest.length == 1) {
		if (controlsRequest.data[0] == 0)
			analogLatchOff();
		else
			analogLatchOn();
		controlsRequest.status = 0;
	} else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}

/* request 3's command given up, unused */
void touchRequestCancel(void)
{
	inputRequests.touchPending = 0;
	touchRequest.state = HOST_REQUEST_ERROR;
	touchRequest.status = 0x3f;
}

/* a stream starts a frame's answer: restart the frames' timer (TDR04's channel), and put each
   part's age in it */
void touchStream0301Prepare(void)
{
	unsigned int age;
	unsigned char block;

	if (touch.flags & TOUCH_PANEL0 && touch.rate != 0)
		TS0L = 0x10;
	age = TCR02;
	block = hostStream0301.block;
	age = touchFrames[block].time0 - age;
	touchFrames[block].panel[0][TOUCH_PANEL_AGE] = (unsigned char)(age & 0xff);
	touchFrames[block].panel[0][TOUCH_PANEL_AGE + 1] = (unsigned char)(age >> 8);
}

void touchStream0302Prepare(void)
{
	unsigned int age;
	unsigned char block;

	if (touch.flags & TOUCH_PANEL1 && touch.rate != 0)
		TS0L = 0x10;
	age = TCR02;
	block = hostStream0302.block;
	age = touchFrames[block].time1 - age;
	touchFrames[block].panel[1][TOUCH_PANEL_AGE] = (unsigned char)(age & 0xff);
	touchFrames[block].panel[1][TOUCH_PANEL_AGE + 1] = (unsigned char)(age >> 8);
}

void touchStream0300Prepare(void)
{
	unsigned int now;
	unsigned int age;
	unsigned char block;

	if (touch.flags & (TOUCH_PANEL0 | TOUCH_PANEL1) && touch.rate != 0)
		TS0L = 0x10;
	now = TCR02;
	block = hostStream0300.block;
	age = touchFrames[block].time0 - now;
	touchFrames[block].panel[0][TOUCH_PANEL_AGE] = (unsigned char)(age & 0xff);
	touchFrames[block].panel[0][TOUCH_PANEL_AGE + 1] = (unsigned char)(age >> 8);
	age = touchFrames[block].time1 - now;
	touchFrames[block].panel[1][TOUCH_PANEL_AGE] = (unsigned char)(age & 0xff);
	touchFrames[block].panel[1][TOUCH_PANEL_AGE + 1] = (unsigned char)(age >> 8);
}

/* 0x0380 (sceSysconGetTouchpanelDeviceInfo): each controller's family, variant, version and
   build, read again if the host asks; waits while the scan identifies the controllers */
static void touchRequestDeviceInfo(void)
{
	if (inputDelays.flag40 == 0 && inputDelays.flag80 == 0) {
		if (touchRequest.length < 2) {
			touchIdentify0();
			touchIdentify1();
		}
		memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
		memcpy(inputRequests.touchAnswer + 4, &touch.panel[1].family, 4);
		inputRequests.touchPending = 0;
		touchRequest.answerLength = 8;
		touchRequest.status = 0;
		hostRequestAnswerReady(&touchRequest.state);
	}
}

/*
 * 0x0381: the panels on or off (each data[0] and data[2], bit 0) with their touches and the
 * frame rate (data[1] and data[3]: the touches in the low half, the rate in the high); a
 * panel turned on answers after the next frame.
 */
static void touchRequestStart(void)
{
	unsigned char panels;
	unsigned char setting;
	unsigned char rate;

	panels = 0;
	rate = 0xff;
	switch (inputRequests.touchPhase) {
	case 0:
		if (inputRequests.touchData[0] & 1) {
			if (!(touch.flags & TOUCH_PANEL0)) {
				panels |= TOUCH_STATUS0;
				setting = inputRequests.touchData[1] & 0x0f;
				if (setting != 0 && setting < 9 && touch.setting[0] != setting) {
					touch.setting[0] = setting;
					touchWriteVerified(touch.panel[0].objects.numTouch,
							   &touch.setting[0], 1, 0);
				}
				touchEnable0();
			}
			rate = inputRequests.touchData[1] >> 4;
		} else if (inputRequests.touchData[0] == 0) {
			hostStream0301.state = 0;
			touchDisable0();
		}
		if (inputRequests.touchData[2] & 1) {
			if (!(touch.flags & TOUCH_PANEL1)) {
				panels |= TOUCH_STATUS1;
				setting = inputRequests.touchData[3] & 0x0f;
				if (setting != 0 && setting < 9 && touch.setting[1] != setting) {
					touch.setting[1] = setting;
					touchWriteVerified(touch.panel[1].objects.numTouch,
							   &touch.setting[1], 1, 1);
				}
				touchEnable1();
			}
			rate = inputRequests.touchData[3] >> 4;
		} else if (inputRequests.touchData[2] == 0) {
			hostStream0302.state = 0;
			touchDisable1();
		}
		if (rate != 0xff && touch.rate != rate) {
			touch.rate = rate;
			if (touch.rate != 0) {
				tau0PowerOn();
				TT0L = 0x10;
				TMR04 = 0x8000;
				TDR04 = touch.rate * 1000U / 32 - 1;
			}
		}
		if (panels != 0) {
			if (touch.rate != 0) {
				TMIF04 = 0;
				TMMK04 = 0;
				TS0L = 0x10;
			}
			touchUpdateState();
			inputRequests.touchPhase = 1;
		} else {
			if (!(touch.flags & (TOUCH_PANEL0 | TOUCH_PANEL1))) {
				TT0L = 0x10;
				TMMK04 = 1;
				TMIF04 = 0;
				touchSetMode(0);
				touchClearFlags(TOUCH_FRAME);
			}
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	case 1:
		if (touch.flags & TOUCH_FRAME) {
			inputRequests.touchAnswer[0] = 0;
			hostStream0300.state = 3;
			if (touch.flags & TOUCH_PANEL0)
				hostStream0301.state = 3;
			if (touch.flags & TOUCH_PANEL1)
				hostStream0302.state = 3;
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x0383: data[2] bytes (1-10) of data[3] to panel 0's address data[0-1]; the result */
static void touchRequestWrite0(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
		result = 1;
	else {
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchWrite(address, &inputRequests.touchData[3], length, 0);
	}
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0384: data[2] bytes (1-10) from panel 0's address data[0-1]; the result, then the bytes */
static void touchRequestRead0(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
		result = 1;
	else {
		memset(&inputRequests.touchAnswer[1], 0, 10);
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchRead(address, &inputRequests.touchAnswer[1], length, 0);
	}
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 11;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0385: 0x0383 to panel 1 */
static void touchRequestWrite1(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
		result = 1;
	else {
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchWrite(address, &inputRequests.touchData[3], length, 1);
	}
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0386: 0x0384 from panel 1 */
static void touchRequestRead1(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
		result = 1;
	else {
		memset(&inputRequests.touchAnswer[1], 0, 10);
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchRead(address, &inputRequests.touchAnswer[1], length, 1);
	}
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 11;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0387: each panel's power interval (data[1] if data[0], data[5] if data[4]) */
static void touchRequestSetPower(void)
{
	unsigned char result;

	result = 0;
	if (touchRequest.length == 8) {
		touchRequest.status = 0;
		if (inputRequests.touchData[0] != 0) {
			result = touchSetPower0(inputRequests.touchData[1]);
			if (result != 0)
				goto done;
		}
		if (inputRequests.touchData[4] != 0) {
			result = touchSetPower1(inputRequests.touchData[5]);
			if (result != 0)
				goto done;
		}
	} else
		touchRequest.status = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0388: each panel's status read with its frames (data[0], data[1]) on or off */
static void touchRequestStatus(void)
{
	unsigned char result;

	result = 0;
	if (touchRequest.length == 2) {
		touchRequest.status = 0;
		if (inputRequests.touchData[0] != 0) {
			if (!(touch.flags & TOUCH_STATUS0))
				result = touchStatusOn0();
		} else
			result = touchStatusOff0();
		if (result != 0)
			goto done;
		if (inputRequests.touchData[1] != 0) {
			if (!(touch.flags & TOUCH_STATUS1))
				result = touchStatusOn1();
		} else
			result = touchStatusOff1();
		if (result != 0)
			goto done;
	} else
		touchRequest.status = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0389: each panel's grip suppression (data[1-7] if data[0], data[9-15] if data[8]) */
static void touchRequestSetGrip(void)
{
	unsigned char result;

	result = 0;
	if (touchRequest.length == 0x10) {
		if (inputRequests.touchData[0] != 0) {
			result = touchSetGrip0(&inputRequests.touchData[1]);
			if (result != 0)
				goto done;
		}
		if (inputRequests.touchData[8] != 0)
			result = touchSetGrip1(&inputRequests.touchData[9]);
		touchRequest.status = 0;
	} else
		touchRequest.status = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x03a0: panel 0's controller to its bootloader; its status 8 runs later */
static void touchRequestEnterBootloader0(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchEnterBootloader0();
		inputRequests.touchTimer = 8;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			touchBootloaderStatus0(inputRequests.touchAnswer);
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x03b0: 0x03a0 for panel 1 */
static void touchRequestEnterBootloader1(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchEnterBootloader1();
		inputRequests.touchTimer = 8;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			touchBootloaderStatus1(inputRequests.touchAnswer);
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x03a1: panel 0's bootloader unlocked; its status once its CHG line (P7.5) falls, within
   10 runs */
static void touchRequestUnlock0(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchBootloaderUnlock0();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!P7.5) {
				touchBootloaderStatus0(inputRequests.touchAnswer);
				touchRequest.answerLength = 1;
				inputRequests.touchPending = 0;
				touchRequest.status = 0;
				hostRequestAnswerReady(&touchRequest.state);
			}
		} else {
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 1;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x03b1: 0x03a1 for panel 1 (CHG on P6.4) */
static void touchRequestUnlock1(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchBootloaderUnlock1();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!P6.4) {
				touchBootloaderStatus1(inputRequests.touchAnswer);
				touchRequest.answerLength = 1;
				inputRequests.touchPending = 0;
				touchRequest.status = 0;
				hostRequestAnswerReady(&touchRequest.state);
			}
		} else {
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 1;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/*
 * 0x03a2: a piece of a frame for panel 0's bootloader: data[0-1] the frame's number, data[2-3]
 * the piece's offset in it, data[4] its length, then the piece; the first clears the buffer.
 * The answer: the frame's number, then the bootloader's status.
 */
static void touchRequestLoad0(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int offset;

	length = inputRequests.touchData[4];
	offset = inputRequests.touchData[2] | (unsigned int)inputRequests.touchData[3] << 8;
	if (length + 5 == touchRequest.length) {
		touchBuffer.frame = inputRequests.touchData[0] |
				    (unsigned int)inputRequests.touchData[1] << 8;
		touchBuffer.end = offset + length;
		if (touchBuffer.end <= TOUCH_BUFFER_SIZE) {
			if (offset == 0)
				memset(touchBuffer.data, 0, TOUCH_BUFFER_SIZE);
			memcpy(touchBuffer.data + offset, &inputRequests.touchData[5], length);
			result = touchRead(TOUCH_BOOTLOADER, &inputRequests.touchAnswer[2], 1, 0);
			if (result != 0)
				result = 1;
			else
				result = 0;
		} else
			result = 0x3c;
	} else
		result = 0x3b;
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 3;
	touchRequest.status = result;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x03b2: 0x03a2 for panel 1 (the buffer is not cleared) */
static void touchRequestLoad1(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int offset;

	length = inputRequests.touchData[4];
	offset = inputRequests.touchData[2] | (unsigned int)inputRequests.touchData[3] << 8;
	if (length + 5 == touchRequest.length) {
		touchBuffer.frame = inputRequests.touchData[0] |
				    (unsigned int)inputRequests.touchData[1] << 8;
		touchBuffer.end = offset + length;
		if (touchBuffer.end <= TOUCH_BUFFER_SIZE) {
			memcpy(touchBuffer.data + offset, &inputRequests.touchData[5], length);
			result = touchRead(TOUCH_BOOTLOADER, &inputRequests.touchAnswer[2], 1, 1);
			if (result != 0)
				result = 1;
			else
				result = 0;
		} else
			result = 0x3c;
	} else
		result = 0x3b;
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 3;
	touchRequest.status = result;
	hostRequestAnswerReady(&touchRequest.state);
}

/*
 * 0x03a3: the frame gathered (its number in data[0-1]) to panel 0's bootloader, then its
 * status at each fall of the CHG line (P7.5), 50 runs at most each.
 */
static void touchRequestFlash0(void)
{
	unsigned char result;
	unsigned int frame;

	switch (inputRequests.touchPhase) {
	case 0:
		if (touchRequest.length == 2) {
			frame = inputRequests.touchData[0] |
				(unsigned int)inputRequests.touchData[1] << 8;
			if (touchBuffer.frame == frame) {
				if (touchBuffer.end == TOUCH_BUFFER_SIZE)
					touchBuffer.end += touchFlashExtra;
				result = touchWrite(TOUCH_BOOTLOADER, touchBuffer.data, touchBuffer.end,
						    0);
				inputRequests.touchPhase = 1;
				inputRequests.touchTimer = 50;
			} else {
				result = 0x3c;
				goto answer;
			}
		} else {
			result = 0x3b;
			goto answer;
		}
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!P7.5) {
				touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
				inputRequests.touchPhase = 2;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	case 2:
		if (--inputRequests.touchTimer != 0) {
			if (!P7.5) {
				touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
				inputRequests.touchPhase = 3;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
			touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
			goto answer;
		}
		break;
	case 3:
		if (inputRequests.touchTimer == 0 || !P7.5) {
			inputRequests.touchAnswer[0] = inputRequests.touchData[0];
			inputRequests.touchAnswer[1] = inputRequests.touchData[1];
			inputRequests.touchPending = 0;
			touchRequest.answerLength = 3;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		inputRequests.touchTimer--;
		break;
	default:
		break;
	}
	return;
answer:
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 3;
	hostRequestAnswerReady(&touchRequest.state);
	touchRequest.status = result;
}

/* 0x03b3: 0x03a3 for panel 1 (CHG on P6.4) */
static void touchRequestFlash1(void)
{
	unsigned char result;
	unsigned int frame;

	switch (inputRequests.touchPhase) {
	case 0:
		if (touchRequest.length == 2) {
			frame = inputRequests.touchData[0] |
				(unsigned int)inputRequests.touchData[1] << 8;
			if (touchBuffer.frame == frame) {
				result = touchWrite(TOUCH_BOOTLOADER, touchBuffer.data, touchBuffer.end,
						    1);
				inputRequests.touchPhase = 1;
				inputRequests.touchTimer = 50;
			} else {
				result = 0x3c;
				goto answer;
			}
		} else {
			result = 0x3b;
			goto answer;
		}
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!P6.4) {
				touchBootloaderStatus1(&inputRequests.touchAnswer[2]);
				inputRequests.touchPhase = 2;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	case 2:
		if (--inputRequests.touchTimer != 0) {
			if (!P6.4) {
				touchBootloaderStatus1(&inputRequests.touchAnswer[2]);
				inputRequests.touchPhase = 3;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	case 3:
		if (inputRequests.touchTimer == 0 || !P6.4) {
			inputRequests.touchAnswer[0] = inputRequests.touchData[0];
			inputRequests.touchAnswer[1] = inputRequests.touchData[1];
			inputRequests.touchPending = 0;
			touchRequest.answerLength = 3;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		inputRequests.touchTimer--;
		break;
	default:
		break;
	}
	return;
answer:
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 3;
	hostRequestAnswerReady(&touchRequest.state);
	touchRequest.status = result;
}

/* 0x03a4 */
static void touchRequestBootloaderStatus0(void)
{
	touchBootloaderStatus0(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03b4 */
static void touchRequestBootloaderStatus1(void)
{
	touchBootloaderStatus1(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03a5: panel 0's settings saved, answered 10 runs later */
static void touchRequestBackup0(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchBackup0();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			touchRequest.answerLength = 1;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x03b5 */
static void touchRequestBackup1(void)
{
	switch (inputRequests.touchPhase) {
	case 0:
		touchBackup1();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			touchRequest.answerLength = 1;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
		}
		break;
	}
}

/* 0x03a6 */
static void touchRequestReset0(void)
{
	touchReset0();
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03b6 */
static void touchRequestReset1(void)
{
	touchReset1();
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03a7 */
static void touchRequestConfigVersion0(void)
{
	touchReadConfigVersion0(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 2;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03b7 */
static void touchRequestConfigVersion1(void)
{
	touchReadConfigVersion1(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 2;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03a8 */
static void touchRequestUserData0(void)
{
	touchReadUserData0(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 8;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03b8 */
static void touchRequestUserData1(void)
{
	touchReadUserData1(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 8;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
}

/* 0x03a9: panel 0's self test 0x17 */
static void touchRequestSelfTest0(void)
{
	touchSelfTest0(0x17, inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x03b9 */
static void touchRequestSelfTest1(void)
{
	touchSelfTest1(0x17, inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
}

/* 0x0482 */
static void motionRequestUnknown(void)
{
	inputRequests.motionFlags &= ~MOTION_PENDING;
	hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3f, 0);
}

/* stream 0x0400 starts a frame's answer: nothing to do */
void motionStreamPrepare(void)
{
}

/* 0x0480: the sensor's two words (zeros if it is held in reset), once the scan has read them */
static void motionRequest0480(void)
{
	switch (inputRequests.motionPhase) {
	case 0:
		if (inputDelays.motionInfo != 0)
			return;
		if (P6.3) {
			motionInfoStart();
			inputRequests.motionPhase = 1;
		} else {
			inputRequests.motionAnswer[0] = 0;
			inputRequests.motionAnswer[1] = 0;
			inputRequests.motionAnswer[2] = 0;
			inputRequests.motionAnswer[3] = 0;
			goto answer;
		}
		break;
	case 1:
		if (inputKeys.state & 0x0400)
			break;
		if (motionInfoResult == 0) {
			inputRequests.motionAnswer[0] = motion.info[1].byte[0];
			inputRequests.motionAnswer[1] = motion.info[1].byte[1];
			inputRequests.motionAnswer[2] = motion.info[0].byte[0];
			inputRequests.motionAnswer[3] = motion.info[0].byte[1];
			goto answer;
		}
		inputRequests.motionFlags &= ~MOTION_PENDING;
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x03, 0);
		return;
	default:
		return;
	}
	return;
answer:
	inputRequests.motionFlags &= ~MOTION_PENDING;
	hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0, 4);
}

/* 0x0481: the sensor's frames off (data[0] 0) or on, at the period data[1] (at least 50) */
static void motionRequest0481(void)
{
	switch (inputRequests.motionPhase) {
	case 0:
		if (inputRequests.motionData[0] == 0) {
			inputRequests.motionFlags &= ~MOTION_STARTED;
			hostStream0400.state = 0;
			motionStop();
			inputRequests.motionAnswer[0] = 0;
			inputRequests.motionFlags &= ~MOTION_PENDING;
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0, 1);
			return;
		}
		if (inputRequests.motionData[1] >= 50)
			motionPeriod = inputRequests.motionData[1];
		motionStart();
		motionReadFrame();
		inputRequests.motionPhase = 1;
		break;
	case 1:
		if (motion.status & MOTION_FRAME) {
			inputRequests.motionFlags |= MOTION_STARTED;
			hostStream0400.state = 3;
			inputRequests.motionAnswer[0] = 0;
			inputRequests.motionFlags &= ~MOTION_PENDING;
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0, 1);
		}
		break;
	}
}

/*
 * 0x0483: a packet to the sensor, in pieces: data[0] the piece's offset, data[1] its length,
 * then the piece. The last piece sends the packet; its answer is the sensor's.
 */
static void motionRequest0483(void)
{
	unsigned char offset;
	unsigned char length;

	if (!(inputRequests.motionFlags & MOTION_STARTED)) {
		offset = inputRequests.motionData[0];
		length = inputRequests.motionData[1];
		if (offset + length >= MOTION_FRAME_SIZE) {
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
			inputRequests.motionFlags &= ~MOTION_PENDING;
		} else {
			memcpy(&motionSend[offset], &inputRequests.motionData[2], length);
			if (offset + length == MOTION_PACKET_SIZE) {
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				inputRequests.motionPhase = 0;
			} else {
				memset(inputRequests.motionAnswer, 0xff, MOTION_PACKET_SIZE);
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0,
						  MOTION_PACKET_SIZE);
				inputRequests.motionFlags &= ~MOTION_PENDING;
			}
			motionRequest.answerLength = MOTION_PACKET_SIZE;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* 0x0484: the sensor reset into its bootloader (P6.3 its reset, P4.2 low through it), then
   its command 0x80 */
static void motionRequest0484(void)
{
	if (!(inputRequests.motionFlags & MOTION_STARTED)) {
		switch (inputRequests.motionPhase) {
		case 0:
			inputRequests.motionFlags |= MOTION_BOOT;
			P6.3 = 0;
			P4.2 = 0;
			dly_tsk(1);
			P6.3 = 1;
			inputRequests.motionDelay = 1;
			inputRequests.motionPhase = 1;
			break;
		case 1:
			P4.2 = 1;
			inputRequests.motionDelay = 13;
			inputRequests.motionPhase = 2;
			break;
		case 2:
			memset(motionSend, 0, MOTION_FRAME_SIZE);
			motionPacketAdd(0x80, NULL, 0);
			motionPacketFinish();
			motionTransfer(motionSend, motionReceive);
			inputRequests.motionFlags |= MOTION_TRANSFER;
			inputRequests.motionPhase = 3;
			break;
		case 3:
			if (!P7.2) {
				memset(motionSend, 0, MOTION_FRAME_SIZE);
				motionPacketFinish();
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				motionRequest.answerLength = MOTION_PACKET_SIZE;
				inputRequests.motionPhase = 0;
			}
			break;
		default:
			break;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* 0x0485: the bootloader's command 0x84, which leaves it */
static void motionRequest0485(void)
{
	if (inputRequests.motionFlags & MOTION_BOOT) {
		switch (inputRequests.motionPhase) {
		case 0:
			memset(motionSend, 0, MOTION_FRAME_SIZE);
			motionPacketAdd(0x84, NULL, 0);
			motionPacketFinish();
			motionTransfer(motionSend, motionReceive);
			inputRequests.motionFlags |= MOTION_TRANSFER;
			inputRequests.motionPhase = 1;
			break;
		case 1:
			if (!P7.2) {
				memset(motionSend, 0, MOTION_FRAME_SIZE);
				motionPacketFinish();
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				motionRequest.answerLength = MOTION_PACKET_SIZE;
				inputRequests.motionPhase = 0;
				inputRequests.motionFlags &= ~MOTION_BOOT;
			}
			break;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* 0x0486: a packet to the bootloader, in pieces as 0x0483's; the sensor's answer once P7.2
   falls */
static void motionRequest0486(void)
{
	unsigned char offset;
	unsigned char length;

	if (inputRequests.motionFlags & MOTION_BOOT) {
		switch (inputRequests.motionPhase) {
		case 0:
			offset = inputRequests.motionData[0];
			length = inputRequests.motionData[1];
			if (offset + length >= MOTION_FRAME_SIZE) {
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
				inputRequests.motionFlags &= ~MOTION_PENDING;
			} else {
				memcpy(&motionSend[offset], &inputRequests.motionData[2], length);
				if (offset + length == MOTION_PACKET_SIZE) {
					motionTransfer(motionSend, motionReceive);
					motionRequest.answerLength = MOTION_PACKET_SIZE;
					inputRequests.motionFlags |= MOTION_TRANSFER;
					inputRequests.motionPhase = 1;
				} else {
					memset(inputRequests.motionAnswer, 0xff, MOTION_PACKET_SIZE);
					hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0,
							  MOTION_PACKET_SIZE);
					inputRequests.motionFlags &= ~MOTION_PENDING;
				}
			}
			break;
		case 1:
			if (!P7.2) {
				memset(motionSend, 0, MOTION_FRAME_SIZE);
				motionPacketFinish();
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				motionRequest.answerLength = MOTION_PACKET_SIZE;
				inputRequests.motionPhase = 0;
			}
			break;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* 0x0487: the same as 0x0486 */
static void motionRequest0487(void)
{
	unsigned char offset;
	unsigned char length;

	if (inputRequests.motionFlags & MOTION_BOOT) {
		switch (inputRequests.motionPhase) {
		case 0:
			offset = inputRequests.motionData[0];
			length = inputRequests.motionData[1];
			if (offset + length >= MOTION_FRAME_SIZE) {
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
				inputRequests.motionFlags &= ~MOTION_PENDING;
			} else {
				memcpy(&motionSend[offset], &inputRequests.motionData[2], length);
				if (offset + length == MOTION_PACKET_SIZE) {
					motionTransfer(motionSend, motionReceive);
					motionRequest.answerLength = MOTION_PACKET_SIZE;
					inputRequests.motionFlags |= MOTION_TRANSFER;
					inputRequests.motionPhase = 1;
				} else {
					memset(inputRequests.motionAnswer, 0xff, MOTION_PACKET_SIZE);
					hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0,
							  MOTION_PACKET_SIZE);
					inputRequests.motionFlags &= ~MOTION_PENDING;
				}
			}
			break;
		case 1:
			if (!P7.2) {
				memset(motionSend, 0, MOTION_FRAME_SIZE);
				motionPacketFinish();
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				motionRequest.answerLength = MOTION_PACKET_SIZE;
				inputRequests.motionPhase = 0;
			}
			break;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* 0x0488: the same as 0x0486 */
static void motionRequest0488(void)
{
	unsigned char offset;
	unsigned char length;

	if (inputRequests.motionFlags & MOTION_BOOT) {
		switch (inputRequests.motionPhase) {
		case 0:
			offset = inputRequests.motionData[0];
			length = inputRequests.motionData[1];
			if (offset + length >= MOTION_FRAME_SIZE) {
				hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
				inputRequests.motionFlags &= ~MOTION_PENDING;
			} else {
				memcpy(&motionSend[offset], &inputRequests.motionData[2], length);
				if (offset + length == MOTION_PACKET_SIZE) {
					motionTransfer(motionSend, motionReceive);
					motionRequest.answerLength = MOTION_PACKET_SIZE;
					inputRequests.motionFlags |= MOTION_TRANSFER;
					inputRequests.motionPhase = 1;
				} else {
					memset(inputRequests.motionAnswer, 0xff, MOTION_PACKET_SIZE);
					hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0,
							  MOTION_PACKET_SIZE);
					inputRequests.motionFlags &= ~MOTION_PENDING;
				}
			}
			break;
		case 1:
			if (!P7.2) {
				memset(motionSend, 0, MOTION_FRAME_SIZE);
				motionPacketFinish();
				motionTransfer(motionSend, inputRequests.motionAnswer);
				inputRequests.motionFlags |= MOTION_TRANSFER | MOTION_ANSWER;
				motionRequest.answerLength = MOTION_PACKET_SIZE;
				inputRequests.motionPhase = 0;
			}
			break;
		}
	} else {
		hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x02, 0);
		inputRequests.motionFlags &= ~MOTION_PENDING;
	}
}

/* the buttons, for a module that is not here (unused) */
void controlsGetButtons(unsigned char *out)
{
	controlsReadButtons(out);
}
