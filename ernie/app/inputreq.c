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
#include "board.h"
#include "analog.h"
#include "csi01.h"
#include "handshake.h"
#include "host.h"
#include "input.h"
#include "inputreq.h"
#include "kernel.h"
#include "log.h"
#include "motion.h"
#include "serial.h"
#include "task.h"
#include "tau.h"
#include "timer.h"
#include "touch.h"

/* touchPending: touchRequest's command waits for the run */
#define TOUCH_PENDING		0x80

/* motionFlags */
#define MOTION_PENDING		0x80	/* motionRequest's command waits for the run */
#define MOTION_TRANSFER		0x40	/* the run waits for the end of a CSI01 transfer */
#define MOTION_ANSWER		0x20	/* and then answers */
#define MOTION_BOOT		0x02	/* the sensor is in its bootloader (0x0484) */
#define MOTION_STARTED		0x01	/* the sensor's frames go to stream 0x0400 (0x0481) */

/* the slim's: 0x0180 sets the sticks' mode this timer's 3 ticks after P10.2 goes low */
#define CONTROLS_MODE_TIMER	0x15

/* the slim answers each request with hostRequestAnswer, the controls' status kept in a local
   until then, and sets the requests' states with hostRequestSetState; the others set the request's status and length and hostRequestAnswerReady */
#if SOURCE_0103
#define CONTROLS_STATUS		status
#define TOUCH_STATUS		status
#define REQUEST_SET_STATE(request, value)	hostRequestSetState(&(request), value)
#else
#define CONTROLS_STATUS		controlsRequest.status
#define TOUCH_STATUS		touchRequest.status
#define REQUEST_SET_STATE(request, value)	((request).state = (value))
#endif

/* the slim's 0x0105 (USS-1002): the notifiers have no case for no event */
#define INPUT_REQUEST_0105	(BOARD_USS1001 && SOURCE_0104)

/* the slim's 0x0105: a word the host's data gives byte by byte */
union TouchRequestWord {
	unsigned int	word;
	unsigned char	byte[2];
};

/* a request's command and its handler */
struct InputCommand {
	unsigned int	command;
	void		(*handler)(void);
};

/* the requests' data, and their commands' state */
struct InputRequests {
	unsigned char	touchPending;		/* TOUCH_PENDING */
	unsigned char	reserved;		/* cleared by inputRequestInit, otherwise unused */
#if BOARD_USS1001 && SOURCE_0104
	unsigned char	touchData[0x2a];	/* the slim's 0x0105 */
	unsigned char	touchAnswer[0x2a];
#elif FW_SINCE(0x0100060B)
	unsigned char	touchData[0x20];
	unsigned char	touchAnswer[0x18];
#else
	unsigned char	touchData[0x20];
	unsigned char	touchAnswer[0x10];
#endif
	unsigned char	touchPhase;		/* the step of a command that takes several runs */
	unsigned char	touchTimer;		/* the runs it waits at most */
	unsigned char	motionFlags;		/* MOTION_* */
	unsigned char	motionCommand;		/* the command's low bits: its motionCommands entry */
#if BOARD_MOTION
	unsigned char	motionData[0x20];
	unsigned char	motionAnswer[0x40];
#else
	unsigned char	motionData[4];
	unsigned char	motionAnswer[8];
#endif
	unsigned char	motionPhase;		/* the step of a command that takes several runs */
	unsigned char	motionDelay;		/* the runs to skip before the next step */
	unsigned char	controlsData[0x10];
	union {
		unsigned char	byte[0x10];
		unsigned int	word[8];
	} controlsAnswer;
};

extern unsigned char input1Bit5Active(void);


void controlsRequestNotify(void);
void touchRequestNotify(void);
void motionRequestNotify(void);
void controlsReadAnalog(unsigned char *out);
void touchStream0301Prepare(void);
void touchStream0302Prepare(void);
void touchStream0300Prepare(void);
#if BOARD_MOTION
void motionStreamPrepare(void);
#endif
static void controlsRequest0180(void);
#if CONTROLS_MODE_DELAYED
static void controlsRequest0180Expire(void);
#endif
static void controlsRequest0181(void);
static void controlsRequest0182(void);
static void controlsRequest0183(void);
static void controlsRequest0183Done(unsigned char command, unsigned char result);
static void controlsRequest0184(void);
static void controlsRequest0184Done(unsigned char command, unsigned char result);
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
static void controlsRequest0185(void);
#endif
static void controlsRequest0190(void);
static void controlsRequest01A0(void);
static void controlsRequest01A1(void);
#if INPUT_REQUEST_0105
static void controlsRequest0186(void);
#endif
#if SOURCE_0103
static void controlsRequest01B0(void);
#endif
static void touchRequestDeviceInfo(void);
static void touchRequestStart(void);
static void touchRequestWrite0(void);
static void touchRequestRead0(void);
static void touchRequestWrite1(void);
static void touchRequestRead1(void);
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
static void touchRequestSetPower(void);
#endif
#if SOURCE_0103
static void touchRequestTestModePin(void);
#elif FW_SINCE(0x00090903)
static void touchRequestStatus(void);
static void touchRequestSetGrip(void);
#endif
#if FW_SINCE(0x0100060B)
static void touchRequestPanelInfo(void);
static void touchRequestGripEdges(void);
#endif
static void touchRequestEnterBootloader0(void);
static void touchRequestUnlock0(void);
static void touchRequestLoad0(void);
static void touchRequestFlash0(void);
static void touchRequestBootloaderStatus0(void);
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
static void touchRequestBackup0(void);
static void touchRequestReset0(void);
static void touchRequestConfigVersion0(void);
#endif
#if FW_SINCE(0x00090502)
static void touchRequestUserData0(void);
static void touchRequestSelfTest0(void);
#endif
static void touchRequestEnterBootloader1(void);
static void touchRequestUnlock1(void);
static void touchRequestLoad1(void);
static void touchRequestFlash1(void);
static void touchRequestBootloaderStatus1(void);
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
static void touchRequestBackup1(void);
static void touchRequestReset1(void);
static void touchRequestConfigVersion1(void);
#endif
#if FW_SINCE(0x00090502)
static void touchRequestUserData1(void);
static void touchRequestSelfTest1(void);
#endif
#if SOURCE_0103
static void touchRequest03AA(void);
static void touchRequest03AB(void);
static void touchRequest03AC(void);
static void touchRequest03AD(void);
static void touchRequest03BA(void);
static void touchRequest03BB(void);
static void touchRequest03BC(void);
static void touchRequest03BD(void);
#endif
#if SOURCE_0104
static void touchRequest03AE(void);
static void touchRequest03BE(void);
static void touchRequest03AF(void);
static void touchRequest03BF(void);
#endif
#if INPUT_REQUEST_0105
static void touchRequest03C0(void);
static void touchRequest0392(void);
static void touchRequest0393(void);
static void touchRequest0394(void);
static void touchRequest0395(void);
static void touchRequest0396(void);
static void touchRequest0397(void);
static void touchRequest0398(void);
static void touchRequest0399(void);
static void touchRequest039A(void);
static void touchRequest039B(void);
static void touchRequest039C(void);
static void touchRequest039D(void);
static void touchRequest039E(void);
static void touchRequest039F(void);
static void touchRequest03C2(void);
static void touchRequest03C3(void);
static void touchRequest03C4(void);
static void touchRequest03C5(void);
static void touchRequest03C6(void);
static void touchRequest03C7(void);
static void touchRequest03C9(void);
static void touchRequest03CA(void);
static void touchRequest03CB(void);
static void touchRequest03CC(void);
#endif
#if BOARD_MOTION
static void motionRequest0480(void);
static void motionRequest0481(void);
static void motionRequestUnknown(void);
static void motionRequest0483(void);
static void motionRequest0484(void);
static void motionRequest0485(void);
static void motionRequest0486(void);
static void motionRequest0487(void);
static void motionRequest0488(void);
#endif

struct HostRequest controlsRequest;		/* request 1 */
struct HostRequest touchRequest;		/* request 3 */
struct HostRequest motionRequest;		/* request 4 */
static struct InputRequests inputRequests;
struct HostStream hostStream0300;
struct HostStream hostStream0301;
struct HostStream hostStream0302;
#if BOARD_MOTION
struct HostStream hostStream0400;
#endif
#if !SOURCE_0103
static unsigned char touchFlashExtra;		/* added to a full buffer's length (0x03a3), never
						   set */
#endif

static const struct InputCommand controlsCommands[] = {
	{ 0x0180, controlsRequest0180 },
	{ 0x0181, controlsRequest0181 },
	{ 0x0182, controlsRequest0182 },
	{ 0x0183, controlsRequest0183 },
	{ 0x0184, controlsRequest0184 },
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
	{ 0x0185, controlsRequest0185 },
#endif
#if INPUT_REQUEST_0105
	{ 0x0186, controlsRequest0186 },
#endif
	{ 0x0190, controlsRequest0190 },
	{ 0x01a0, controlsRequest01A0 },
	{ 0x01a1, controlsRequest01A1 },
#if SOURCE_0103
	{ 0x01b0, controlsRequest01B0 },
#endif
};

static const struct InputCommand touchCommands[] = {
	{ 0x0380, touchRequestDeviceInfo },
	{ 0x0381, touchRequestStart },
	{ 0x0383, touchRequestWrite0 },
	{ 0x0384, touchRequestRead0 },
	{ 0x0385, touchRequestWrite1 },
	{ 0x0386, touchRequestRead1 },
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
	{ 0x0387, touchRequestSetPower },
#endif
#if SOURCE_0103
	{ 0x038a, touchRequestTestModePin },
#elif FW_SINCE(0x00090903)
	{ 0x0388, touchRequestStatus },
	{ 0x0389, touchRequestSetGrip },
#endif
#if FW_SINCE(0x0100060B)
	{ 0x0390, touchRequestPanelInfo },
	{ 0x0391, touchRequestGripEdges },
#endif
#if INPUT_REQUEST_0105
	{ 0x0392, touchRequest0392 },
	{ 0x0393, touchRequest0393 },
	{ 0x0394, touchRequest0394 },
	{ 0x0395, touchRequest0395 },
	{ 0x0396, touchRequest0396 },
	{ 0x0397, touchRequest0397 },
	{ 0x0398, touchRequest0398 },
	{ 0x0399, touchRequest0399 },
	{ 0x039a, touchRequest039A },
	{ 0x039b, touchRequest039B },
	{ 0x039c, touchRequest039C },
	{ 0x039d, touchRequest039D },
	{ 0x039e, touchRequest039E },
	{ 0x039f, touchRequest039F },
#endif
	{ 0x03a0, touchRequestEnterBootloader0 },
	{ 0x03a1, touchRequestUnlock0 },
	{ 0x03a2, touchRequestLoad0 },
	{ 0x03a3, touchRequestFlash0 },
	{ 0x03a4, touchRequestBootloaderStatus0 },
#if FW_SINCE(0x00090006)	/* 0x00080504 ends at 0x03a4 and 0x03b4 */
	{ 0x03a5, touchRequestBackup0 },
#endif
#if FW_SINCE(0x00090502)
	{ 0x03a6, touchRequestReset0 },
	{ 0x03a7, touchRequestConfigVersion0 },
	{ 0x03a8, touchRequestUserData0 },
	{ 0x03a9, touchRequestSelfTest0 },
#if SOURCE_0103
	{ 0x03aa, touchRequest03AA },
	{ 0x03ab, touchRequest03AB },
	{ 0x03ac, touchRequest03AC },
	{ 0x03ad, touchRequest03AD },
#endif
#if SOURCE_0104
	{ 0x03ae, touchRequest03AE },
	{ 0x03af, touchRequest03AF },
#endif
#elif FW_SINCE(0x00090006)
	{ 0x03a7, touchRequestConfigVersion0 },
	{ 0x03a6, touchRequestReset0 },
#endif
	{ 0x03b0, touchRequestEnterBootloader1 },
	{ 0x03b1, touchRequestUnlock1 },
	{ 0x03b2, touchRequestLoad1 },
	{ 0x03b3, touchRequestFlash1 },
	{ 0x03b4, touchRequestBootloaderStatus1 },
#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
	{ 0x03b5, touchRequestBackup1 },
	{ 0x03b6, touchRequestReset1 },
	{ 0x03b7, touchRequestConfigVersion1 },
#endif
#if FW_SINCE(0x00090502)
	{ 0x03b8, touchRequestUserData1 },
	{ 0x03b9, touchRequestSelfTest1 },
#endif
#if SOURCE_0103
	{ 0x03ba, touchRequest03BA },
	{ 0x03bb, touchRequest03BB },
	{ 0x03bc, touchRequest03BC },
	{ 0x03bd, touchRequest03BD },
#endif
#if SOURCE_0104
	{ 0x03be, touchRequest03BE },
	{ 0x03bf, touchRequest03BF },
#endif
#if INPUT_REQUEST_0105
	{ 0x03c0, touchRequest03C0 },
	{ 0x03c1, touchRequest03C0 },
	{ 0x03c2, touchRequest03C2 },
	{ 0x03c3, touchRequest03C3 },
	{ 0x03c4, touchRequest03C4 },
	{ 0x03c5, touchRequest03C5 },
	{ 0x03c6, touchRequest03C6 },
	{ 0x03c7, touchRequest03C7 },
	{ 0x03c8, touchRequest03C4 },
	{ 0x03c9, touchRequest03C9 },
	{ 0x03ca, touchRequest03CA },
	{ 0x03cb, touchRequest03CB },
	{ 0x03cc, touchRequest03CC },
#endif
};

#if BOARD_MOTION
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
#endif

void inputRequestInit(void)
{
#if CONTROLS_MODE_DELAYED
	struct TimerHandler handler;

#endif
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
#if BOARD_MOTION
	motionRequest.size = 0x40;		/* more than motionData holds */
#else
	motionRequest.size = sizeof(inputRequests.motionData);
#endif
	motionRequest.data = inputRequests.motionData;
	motionRequest.answer = inputRequests.motionAnswer;
	motionRequest.notify = motionRequestNotify;
	motionRequest.state = HOST_REQUEST_RECEIVE;

#if BOARD_MOTION
	hostStream0400.state = 0;
	hostStream0400.block = 0xff;
	hostStream0400.first = 0;
	hostStream0400.length = MOTION_PACKET_SIZE;
	hostStream0400.blocks[0] = motionFrames[0];
	hostStream0400.blocks[1] = motionFrames[1];
	hostStream0400.prepare = motionStreamPrepare;
#endif
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
#if CONTROLS_MODE_DELAYED
	handler.handler = controlsRequest0180Expire;
	timerSetHandler(CONTROLS_MODE_TIMER, &handler);
#endif
}

/* request 3's command came in, or its answer went out */
void touchRequestNotify(void)
{
	switch (touchRequest.event) {
#if !INPUT_REQUEST_0105
	case HOST_EVENT_NONE:
		break;
#endif
	case HOST_EVENT_COMMAND:
		REQUEST_SET_STATE(touchRequest, HOST_REQUEST_BUSY);
		inputRequests.touchPending = TOUCH_PENDING;
		inputRequests.touchPhase = 0;
		break;
	case HOST_EVENT_ANSWERED:
		REQUEST_SET_STATE(touchRequest, HOST_REQUEST_RECEIVE);
		break;
#if SOURCE_0103
	case HOST_EVENT_OPEN:
		hostRequestSetState(&touchRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&touchRequest, HOST_REQUEST_IDLE);
		break;
#endif
	}
}

#if BOARD_MOTION
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
#if !INPUT_REQUEST_0105
	case HOST_EVENT_NONE:
		break;
#endif
	}
}
#else
/* request 4's, with no motion sensor: 0x0480 answers no information, the others 0x04 */
void motionRequestNotify(void)
{
	switch (motionRequest.event) {
	case HOST_EVENT_COMMAND:
		switch (motionRequest.command) {
		case 0x0480:
			inputRequests.motionAnswer[0] = 0xff;
			inputRequests.motionAnswer[1] = 0xff;
			inputRequests.motionAnswer[2] = 0xff;
			inputRequests.motionAnswer[3] = 0xff;
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0, 4);
			return;
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
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&motionRequest, HOST_REQUEST_RECEIVE);
		break;
#if SOURCE_0103
	case HOST_EVENT_OPEN:
		hostRequestSetState(&motionRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&motionRequest, HOST_REQUEST_IDLE);
		break;
#endif
#if !INPUT_REQUEST_0105
	case HOST_EVENT_NONE:
		break;
#endif
	}
}
#endif

/* request 1's: task 3 runs the command (controlsRequestRun) */
void controlsRequestNotify(void)
{
	switch (controlsRequest.event) {
#if !INPUT_REQUEST_0105
	case HOST_EVENT_NONE:
		break;
#endif
	case HOST_EVENT_COMMAND:
		REQUEST_SET_STATE(controlsRequest, HOST_REQUEST_BUSY);
		task3SetFlag(2, 0x0010);
		break;
	case HOST_EVENT_ANSWERED:
		REQUEST_SET_STATE(controlsRequest, HOST_REQUEST_RECEIVE);
		break;
#if SOURCE_0103
	case HOST_EVENT_OPEN:
		hostRequestSetState(&controlsRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&controlsRequest, HOST_REQUEST_IDLE);
		break;
#endif
	}
}

/* request 3's command, from touchCommands (the slim's task 3 runs it) */
#if SOURCE_0103
void touchRequestRun(void)
#else
static void touchRequestRun(void)
#endif
{
	unsigned char i;

	if (inputRequests.touchPending != 0) {
		if (!handshakeUnlocked11()) {
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			handshakeRefuse();
		} else {
			for (i = 0; i < sizeof(touchCommands) / sizeof(touchCommands[0]); i++) {
				if (touchCommands[i].command == touchRequest.command) {
					touchCommands[i].handler();
					return;
				}
			}
#if FW_SINCE(0x00090206)
			hostRequestAnswer(&touchRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
			inputRequests.touchPending = 0;
#else
			/* 0x00090009 fails request 1's, and leaves request 3 pending */
			controlsRequest.state = HOST_REQUEST_ERROR;
			controlsRequest.status = 0x3f;
#endif
		}
	}
}

#if BOARD_MOTION
/* request 4's command, from motionCommands (after motionDelay runs), then the end of its
   CSI01 transfer */
static void motionRequestRun(void)
{
	unsigned char i;

	i = 0;
	if (inputRequests.motionFlags & MOTION_PENDING) {
		if (inputRequests.motionDelay != 0)
			inputRequests.motionDelay--;
		else if (!handshakeUnlocked11()) {
			hostRequestAnswer(&motionRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
			handshakeRefuse();
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
#endif

#if !SOURCE_0103
/* requests 3 and 4 */
void devicesRequestRun(void)
{
	touchRequestRun();
#if BOARD_MOTION
	motionRequestRun();
#endif
	return;
}
#endif

/* request 1's command, from controlsCommands */
void controlsRequestRun(void)
{
	unsigned char i;

	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
	} else {
		for (i = 0; i < sizeof(controlsCommands) / sizeof(controlsCommands[0]); i++) {
			if (controlsCommands[i].command == controlsRequest.command) {
				controlsCommands[i].handler();
				return;
			}
		}
#if FW_SINCE(0x00090206)
		hostRequestAnswer(&controlsRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
#else
		controlsRequest.state = HOST_REQUEST_ERROR;
		controlsRequest.status = 0x3f;
#endif
	}
}

/* the buttons, active low: P8, P5's low half with P11.5 and P4.6, P11.2-3, and three bits of
   inputState1 */
void controlsReadButtons(unsigned char *out)
{
#if BOARD_DOL1001
	/* DOL-1001 has no buttons but the power button (P4.6, low when pressed) */
	out[0] = 0xff;
	out[1] = !P4.6 ? (unsigned char)0xbf : (unsigned char)0xff;
	out[2] = 0xff;
#else
	out[0] = P8;
#endif
#if BOARD_DOL1001
#elif SOURCE_0103
	out[1] = (P7 & 0x5f) | 0xa0;
	out[2] = (P5 & 3) | 0xfc;
#else
	out[1] = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
				  (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	out[2] = (P11 >> 2 & 3) | 0xfc;
#endif
#if FW_SINCE(0x00090006)
	out[3] = ((inputState1 ^ 0x0a) & 0x0e) | 0x74;
#else
	/* 0x00080504: bit 0 is the analog latch's */
	out[3] = (analogLatchIsSet() ? 1 : 0) | (((inputState1 ^ 0x0a) & 0x0f) | 0x74);
#endif
}

#if FW_SINCE(0x00090203)	/* 0x00090009 has one set */
/* the analog sticks, from the set analogFlags names */
void controlsReadAnalog(unsigned char *out)
{
	if (analogFlags & 1)
		controlsReadAnalog0(out);
	else
		controlsReadAnalog1(out);
}
#endif

#if !SOURCE_SLIM	/* the slim's (and DOL-1002's) are analog.c's */
void controlsReadAnalog0(unsigned char *out)
{
	out[0] = analogSticks0[0];
	out[1] = analogSticks0[1];
	out[2] = analogSticks0[2];
	out[3] = analogSticks0[3];
}

#if FW_SINCE(0x00090203)
void controlsReadAnalog1(unsigned char *out)
{
	out[0] = analogSticks1[0];
	out[1] = analogSticks1[1];
	out[2] = analogSticks1[2];
	out[3] = analogSticks1[3];
}
#endif
#endif

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
#if FW_SINCE(0x00090203)
	controlsReadAnalog(out);
#else
	controlsReadAnalog0(out);
#endif
	*length = 4;
	return 0;
}

#if FW_SINCE(0x00090203)	/* 0x00090009 has neither command for both sets of axes */
/* 0x0113 */
unsigned char hostCommandAnalogBoth(unsigned char *out, unsigned char *length)
{
	controlsReadAnalog0(out);
	controlsReadAnalog1(out + 4);
	*length = 8;
	return 0;
}
#endif

/* 0x0101 */
unsigned char hostCommandControlsAnalog(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
#if FW_SINCE(0x00090203)
	controlsReadAnalog(out + 4);
#else
	controlsReadAnalog0(out + 4);
#endif
	*length = 8;
	return 0;
}

#if FW_SINCE(0x00090203)
/* 0x0104 */
unsigned char hostCommandControlsAnalogBoth(unsigned char *out, unsigned char *length)
{
	controlsReadButtons(out);
	controlsReadAnalog0(out + 4);
	controlsReadAnalog1(out + 8);
	*length = 12;
	return 0;
}
#endif

/* 0x0103 (sceSysconGetMultiCnInfo) */
unsigned char hostCommandMultiCnInfo(unsigned char *out, unsigned char *length)
{
#if FW_SINCE(0x01000104)
	if (input1Bit5Active())
		out[0] = 0x80;
	else
		out[0] = 0;
#else
	out[0] = input1Bit5Active() ? 0x80 : 0;
#endif
	out[1] = analogGetMultiCn();
	out[2] = boardGetMultiCnOutput();
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

#if SOURCE_0103
static void controlsRequest0180(void)
{
	unsigned char status;

	if (controlsRequest.length == 1) {
#if CONTROLS_MODE_DELAYED
		if (controlsRequest.data[0] != 0) {
			/* answered by controlsRequest0180Done */
			P10.2 = 0;
			timerStart(CONTROLS_MODE_TIMER, 3);
			return;
		}
		analogStop();
#elif INPUT_REQUEST_0105
		/* P10.2 low first, the mode at once */
		if (controlsRequest.data[0] != 0) {
			P10.2 = 0;
			analogSetMode(controlsRequest.data[0]);
		} else
			analogStop();
#else
		if (controlsRequest.data[0] != 0)
			analogSetMode(controlsRequest.data[0]);
		else
			analogStop();
#endif
		status = 0;
	} else
		status = 0x3b;
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
}

#if CONTROLS_MODE_DELAYED

static void controlsRequest0180Expire(void)
{
	task3SetFlag(2, 0x0800);
}

/* task 3's, for event flag 2's 0x0800 */
void controlsRequest0180Done(void)
{
	analogSetMode(controlsRequest.data[0]);
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0, 0);
}
#endif
#else
static void controlsRequest0180(void)
{
	if (controlsRequest.length == 1) {
#if FW_SINCE(0x0100010B)
		if (controlsRequest.data[0] != 0)
			analogSetMode(controlsRequest.data[0]);
		else
			analogStop();
#else
		analogSetMode(controlsRequest.data[0]);
#endif
		CONTROLS_STATUS = 0;
	} else {
		CONTROLS_STATUS = 0x3b;
	}
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
}
#endif

static void controlsRequest0181(void)
{
#if SOURCE_0103
	unsigned char status;

#endif
	if (controlsRequest.length == 1) {
		if (controlsRequest.data[0] == 0)
			analogCalibrationReset();
		else if (controlsRequest.data[0] == 1) {
			analogCalibrationReset();
			analogCalibrationSave();
		}
#if FW_SINCE(0x00090203)	/* 0x00090009 has no default */
		else if (controlsRequest.data[0] == 2) {
			analogCalibrationDefault();
			analogCalibrationSave();
		}
#endif
#if FW_SINCE(0x0100060B)
		else if (controlsRequest.data[0] == 3)
			analogCalibrationDefault();	/* not saved */
#endif
		CONTROLS_STATUS = 0;
	} else
		CONTROLS_STATUS = 0x3b;
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
#else
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
#endif
	return;
}

static void controlsRequest0182(void)
{
	analogCalibrationGet(inputRequests.controlsAnswer.byte);
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0, sizeof(inputRequests.controlsAnswer));
#else
	controlsRequest.answerLength = sizeof(inputRequests.controlsAnswer);
	hostRequestAnswerReady(&controlsRequest.state);
#endif
	return;
}

/* answered by controlsRequest0183Done */
static void controlsRequest0183(void)
{
	analogStoreRead(inputRequests.controlsAnswer.byte, 0, controlsRequest0183Done);
}

static void controlsRequest0183Done(unsigned char command, unsigned char result)
{
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, result == 0 ? 0 : 0x02,
			  sizeof(inputRequests.controlsAnswer));
#else
	if (result == 0)
		controlsRequest.status = 0;
	else
		controlsRequest.status = 0x02;
	controlsRequest.answerLength = sizeof(inputRequests.controlsAnswer);
	hostRequestAnswerReady(&controlsRequest.state);
#endif
}

/* answered by controlsRequest0184Done */
static void controlsRequest0184(void)
{
	if (controlsRequest.length == sizeof(inputRequests.controlsData))
		analogStoreWrite(inputRequests.controlsData, 0, controlsRequest0184Done);
	else {
#if SOURCE_0103
		hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
#else
		controlsRequest.status = 0x3b;
		controlsRequest.answerLength = 0;
		hostRequestAnswerReady(&controlsRequest.state);
#endif
	}
}

static void controlsRequest0184Done(unsigned char command, unsigned char result)
{
#if SOURCE_0103
	if (result == 0)
		logEvent(3);
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, result == 0 ? 0 : 0x02, 0);
#else
	if (result == 0) {
		controlsRequest.status = 0;
#if FW_SINCE(0x00090502)
		logEvent(3);
#endif
	} else
		controlsRequest.status = 0x02;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
#endif
}

#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
/* six fixed words */
static void controlsRequest0185(void)
{
#if HW_IRT001
	inputRequests.controlsAnswer.word[0] = 0x00ff;
	inputRequests.controlsAnswer.word[1] = 0xff00;
	inputRequests.controlsAnswer.word[2] = 0x0001;
	inputRequests.controlsAnswer.word[3] = 0x0100;
#elif BOARD_IS(0x00411000)
	inputRequests.controlsAnswer.word[0] = 0x00ff;
	inputRequests.controlsAnswer.word[1] = 0xff00;
	inputRequests.controlsAnswer.word[2] = 0x00ff;
	inputRequests.controlsAnswer.word[3] = 0xff00;
#else
	inputRequests.controlsAnswer.word[0] = 0xff00;
	inputRequests.controlsAnswer.word[1] = 0x0001;
	inputRequests.controlsAnswer.word[2] = 0x0100;
	inputRequests.controlsAnswer.word[3] = 0x00ff;
#endif
#if BOARD_102603
	inputRequests.controlsAnswer.word[4] = 0x0001;
#elif SOURCE_0103 && !BOARD_DOL1001	/* DOL-1001's is the fat boards' */
	inputRequests.controlsAnswer.word[4] = 0x0003;
#else
	inputRequests.controlsAnswer.word[4] = 0x0002;
#endif
	inputRequests.controlsAnswer.word[5] = 0x0000;
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, 0, 12);
#else
	controlsRequest.status = 0;
	controlsRequest.answerLength = 12;
	hostRequestAnswerReady(&controlsRequest.state);
#endif
	return;
}
#endif

#if INPUT_REQUEST_0105
/* the sticks' records of faults, 0x20 bytes each: data[0] 0 the samples', 1 the medians' and
   their samples, 3 and 5 those of set 0's and set 1's positions */
static void controlsRequest0186(void)
{
	unsigned char mode;
	unsigned char status;
	unsigned char length;

	status = 0;
	length = 0;
	if (controlsRequest.length == 1) {
		mode = controlsRequest.data[0];
		switch (mode) {
		case 0:
			analogGetSampleFaults(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			break;
		case 1:
			analogGetMedianFaults(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			analogGetMedianSnapshots(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			break;
		case 3:
			analogGetSet0Faults(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			analogGetSet0Snapshots(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			break;
		case 5:
			analogGetSet1Faults(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			analogGetSet1Snapshots(&inputRequests.controlsAnswer.byte[length]);
			length += 0x20;
			break;
		default:
			status = 0x3c;
		}
	} else
		status = 0x3b;
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, length);
}
#endif

static void controlsRequest0190(void)
{
#if SOURCE_0103
	unsigned char status;

	if (controlsRequest.length == 2) {
		/* 0: the dongle's lines (3), 1: the host's (2) */
		switch (controlsRequest.data[0]) {
		case 0:
			status = boardSetMultiCnOutput(3);
			break;
		case 1:
			status = boardSetMultiCnOutput(2);
			break;
		case 4:
		default:
			status = boardSetMultiCnOutput(0xff);
			break;
		}
		if (status != 0)
			status = 0x3c;
		else
			status = 0;
	} else
		status = 0x3b;
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
#else
	if (controlsRequest.length == 2) {
		if (boardSetMultiCnOutput(controlsRequest.data[0]) != 0)
			controlsRequest.status = 0x3c;
		else
			controlsRequest.status = 0;
	} else
		controlsRequest.status = 0x3b;
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
	return;
#endif
}

static void controlsRequest01A0(void)
{
#if SOURCE_0103
	unsigned char status;

#endif
	if (controlsRequest.length == 1) {
#if BOARD_102603
		/* 0x00102603 drives P5.4 from the command's byte */
		if (controlsRequest.data[0] == 0)
			P5.4 = 0;
		else
			P5.4 = 1;
#endif
		CONTROLS_STATUS = 0;
	} else {
		CONTROLS_STATUS = 0x3b;
	}
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
#else
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
#endif
	return;
}

static void controlsRequest01A1(void)
{
#if SOURCE_0103
	unsigned char status;

#endif
	if (controlsRequest.length == 1) {
#if !SOURCE_0103	/* the slim has no latch */
		if (controlsRequest.data[0] == 0)
			analogLatchOff();
		else
			analogLatchOn();
#endif
		CONTROLS_STATUS = 0;
	} else
		CONTROLS_STATUS = 0x3b;
#if SOURCE_0103
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
#else
	controlsRequest.answerLength = 0;
	hostRequestAnswerReady(&controlsRequest.state);
#endif
	return;
}

#if SOURCE_0103
/* 0x0130: what is on the multi-connector (analogGetMultiCn) and its output */
unsigned char hostCommand0130(unsigned char *out, unsigned char *length)
{
#if BOARD_DOL1001
	/* DOL-1001 has no multi-connector: four zero bytes */
	out[0] = 0;
	out[1] = 0;
	out[2] = 0;
	out[3] = 0;
#else
	out[0] = 0;
	out[1] = analogGetMultiCn();
	switch (boardGetMultiCnOutput()) {
	case 0:
		out[2] = 0;
		break;
	case 1:
		out[2] = 1;
		break;
	case 2:
		out[2] = 2;
		break;
	case 3:
		out[2] = 3;
		break;
	default:
		out[2] = 0xff;
		break;
	}
	out[3] = 0;
#endif
	*length = 4;
	return 0;
}

/* the multi-connector's output: 0 and 1 at once, 2 (0x05 while the serial line is busy) and 3
   only with a dongle on it (0x04) */
static void controlsRequest01B0(void)
{
	unsigned char status;

#if BOARD_DOL1001	/* no multi-connector switch: every output taken */
	status = 0;
#else
	if (controlsRequest.length == 2) {
		switch (controlsRequest.data[0]) {
		case 0:
			boardSetMultiCnOutput(0);
			status = 0;
			break;
		case 1:
			boardSetMultiCnOutput(1);
			status = 0;
			break;
		case 2:
			if (boardMultiCnIsDongle()) {
				if (serialGetState() != 0 && serialGetState() != 1)
					status = 0x05;
				else {
					boardSetMultiCnOutput(2);
					status = 0;
				}
			} else
				status = 0x04;
			break;
		case 3:
			if (boardMultiCnIsDongle()) {
				boardSetMultiCnOutput(3);
				status = 0;
			} else
				status = 0x04;
			break;
		default:
			status = 0x3c;
			break;
		}
	} else
		status = 0x3b;
#endif
	hostRequestAnswer(&controlsRequest, HOST_REQUEST_ANSWER, status, 0);
}

#if SOURCE_0104
/* 0x0131: one zero byte */
unsigned char hostCommand0131(unsigned char *out, unsigned char *length)
{
	out[0] = 0;
	*length = 1;
	return 0;
}
#endif
#endif

#if !SOURCE_0103
/* request 3's command given up, unused */
void touchRequestCancel(void)
{
	inputRequests.touchPending = 0;
	touchRequest.state = HOST_REQUEST_ERROR;
	touchRequest.status = 0x3f;
}
#endif

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
#if FW_SINCE(0x00090804) && !FW_SINCE(0x00090903)
	touchFrames[block].panel[0][TOUCH_PANEL_STATE] = touchFrames[block].state0;
#endif
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
#if FW_SINCE(0x00090804) && !FW_SINCE(0x00090903)
	touchFrames[block].panel[1][TOUCH_PANEL_STATE] = touchFrames[block].state1;
#endif
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
#if FW_SINCE(0x00090804) && !FW_SINCE(0x00090903)
	touchFrames[block].panel[0][TOUCH_PANEL_STATE] = touchFrames[block].state0;
#endif
	age = touchFrames[block].time1 - now;
	touchFrames[block].panel[1][TOUCH_PANEL_AGE] = (unsigned char)(age & 0xff);
	touchFrames[block].panel[1][TOUCH_PANEL_AGE + 1] = (unsigned char)(age >> 8);
#if FW_SINCE(0x00090804) && !FW_SINCE(0x00090903)
	touchFrames[block].panel[1][TOUCH_PANEL_STATE] = touchFrames[block].state1;
#endif
}

#if INPUT_REQUEST_0105
/* 0x0303: each panel's controller 2's state (Touch.panel[n].controllerStatus), 3 for another
   controller */
unsigned char hostCommand0303(unsigned char *out, unsigned char *length)
{
	if (touch.panel[0].controller == 2)
		out[0] = touch.panel[0].controllerStatus;
	else
		out[0] = 3;
	if (touch.panel[1].controller == 2)
		out[1] = touch.panel[1].controllerStatus;
	else
		out[1] = 3;
	*length = 2;
	return 0;
}
#endif

/* 0x0380 (sceSysconGetTouchpanelDeviceInfo): each controller's family, variant, version and
   build, read again if the host asks; waits while the scan identifies the controllers (after a
   touch reset) */
static void touchRequestDeviceInfo(void)
{
#if SOURCE_0103
	unsigned char result;
	unsigned char status = 0;
	unsigned char status0 = 0;
	unsigned char status1 = 0;

	/* each panel's status: 3 and 4 not identified, 5 and 6 its object table too long; 7 both */
	if (touchRequest.length < 2) {
#if SOURCE_0104
		result = touchIdentify(0);
		if (result == 0)
			memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
		else if (result == 0x40)
			status0 = 5;
		else
			status0 = 3;
		result = touchIdentify(1);
		if (result == 0)
			memcpy(inputRequests.touchAnswer + 4, &touch.panel[1].family, 4);
		else if (result == 0x40)
			status1 = 6;
		else
			status1 = 4;
#else
		result = touchIdentify0();
		if (result == 0)
			memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
		else if (result == 4)
			status0 = 5;
		else
			status0 = 3;
		result = touchIdentify1();
		if (result == 0)
			memcpy(inputRequests.touchAnswer + 4, &touch.panel[1].family, 4);
		else if (result == 4)
			status1 = 6;
		else
			status1 = 4;
#endif
	}
	if (status0 != 0) {
		if (status1 != 0)
			status = 7;
		else
			status = status0;
	} else
		status = status1;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
#if BOARD_TOUCH_RESET
	if (inputDelays.flag40 != 0 || inputDelays.flag80 != 0)
		return;
#endif
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
#endif
}

/*
 * 0x0381: the panels on or off (each data[0] and data[2], bit 0) with their touches and the
 * frame rate (data[1] and data[3]: the touches in the low half, the rate in the high); a
 * panel turned on answers after the next frame.
 */
#if INPUT_REQUEST_0105
/* the slim's 0x0105: the streams' states set by hostRequestSetStateChecked, the rear's controller
   2 told to report (touchRearSetStatus) before its panel is turned on, one answer for all */
static void touchRequestStart(void)
{
	unsigned char panels;
	unsigned char setting;
	unsigned char rate;
	unsigned char result;
	unsigned char status;

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
					result = touchWriteVerified(touch.panel[0].objects.numTouch,
								    &touch.setting[0], 1, 0);
					if (result != 0) {
						result = 3;
						goto answer;
					}
				}
				touchEnable(0);
			}
			rate = inputRequests.touchData[1] >> 4;
		} else if (inputRequests.touchData[0] == 0) {
			hostRequestSetStateChecked((struct HostRequest *)&hostStream0301, 0);
			touchDisable(0);
		}
		if (inputRequests.touchData[2] & 1) {
			if (!(touch.flags & TOUCH_PANEL1)) {
				panels |= TOUCH_STATUS1;
				setting = inputRequests.touchData[3] & 0x0f;
				if (setting != 0 && setting < 9 && touch.setting[1] != setting) {
					touch.setting[1] = setting;
					result = touchWriteVerified(touch.panel[1].objects.numTouch,
								    &touch.setting[1], 1, 1);
					if (result != 0) {
						result = 4;
						goto answer;
					}
				}
				if (touch.panel[1].controller == 2 &&
				    touch.panel[1].controllerStatus != 0) {
					result = touchRearSetStatus(0, &status, 1);
					if (result != 0) {
						result = 4;
						goto answer;
					}
				}
				touchEnable(1);
			}
			rate = inputRequests.touchData[3] >> 4;
		} else if (inputRequests.touchData[2] == 0) {
			hostRequestSetStateChecked((struct HostRequest *)&hostStream0302, 0);
			touchDisable(1);
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
				touchClearMode();
				touchClearFlags(TOUCH_FRAME);
			}
			result = 0;
			goto answer;
		}
		break;
	case 1:
		if (touch.flags & TOUCH_FRAME) {
			inputRequests.touchAnswer[0] = 0;
			hostRequestSetStateChecked((struct HostRequest *)&hostStream0300, 3);
			if (touch.flags & TOUCH_PANEL0)
				hostRequestSetStateChecked((struct HostRequest *)&hostStream0301, 3);
			if (touch.flags & TOUCH_PANEL1)
				hostRequestSetStateChecked((struct HostRequest *)&hostStream0302, 3);
			result = 0;
			goto answer;
		}
		break;
	default:
		result = 0x21;
		goto answer;
	}
	return;
answer:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
}
#else
static void touchRequestStart(void)
{
	unsigned char panels;
	unsigned char setting;
	unsigned char rate;
#if SOURCE_0104
	unsigned char result;
#endif

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
#if SOURCE_0104
					result = touchWriteVerified(touch.panel[0].objects.numTouch,
								    &touch.setting[0], 1, 0);
					if (result != 0) {
						result = 3;
						goto error;
					}
#else
					touchWriteVerified(touch.panel[0].objects.numTouch,
							   &touch.setting[0], 1, 0);
#endif
				}
#if SOURCE_0104
				touchEnable(0);
#else
				touchEnable0();
#endif
			}
			rate = inputRequests.touchData[1] >> 4;
		} else if (inputRequests.touchData[0] == 0) {
			hostStream0301.state = 0;
#if SOURCE_0104
			touchDisable(0);
#else
			touchDisable0();
#endif
		}
		if (inputRequests.touchData[2] & 1) {
			if (!(touch.flags & TOUCH_PANEL1)) {
				panels |= TOUCH_STATUS1;
				setting = inputRequests.touchData[3] & 0x0f;
				if (setting != 0 && setting < 9 && touch.setting[1] != setting) {
					touch.setting[1] = setting;
#if SOURCE_0104
					result = touchWriteVerified(touch.panel[1].objects.numTouch,
								    &touch.setting[1], 1, 1);
					if (result != 0) {
						result = 4;
						goto error;
					}
#else
					touchWriteVerified(touch.panel[1].objects.numTouch,
							   &touch.setting[1], 1, 1);
#endif
				}
#if SOURCE_0104
				touchEnable(1);
#else
				touchEnable1();
#endif
			}
			rate = inputRequests.touchData[3] >> 4;
		} else if (inputRequests.touchData[2] == 0) {
			hostStream0302.state = 0;
#if SOURCE_0104
			touchDisable(1);
#else
			touchDisable1();
#endif
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
#if FW_SINCE(0x00090502)
			touchUpdateState();
#endif
			inputRequests.touchPhase = 1;
		} else {
			if (!(touch.flags & (TOUCH_PANEL0 | TOUCH_PANEL1))) {
				TT0L = 0x10;
				TMMK04 = 1;
				TMIF04 = 0;
#if SOURCE_0103
				touchClearMode();
#elif FW_SINCE(0x00090502)
				touchSetMode(0);
#endif
#if FW_SINCE(0x01000306)
				touchClearFlags(TOUCH_FRAME);
#endif
			}
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
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
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#if SOURCE_0104
	return;
error:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#endif
}
#endif

/* 0x0383: data[2] bytes (1-10) of data[3] to panel 0's address data[0-1]; the result */
static void touchRequestWrite0(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
#if SOURCE_0104
		result = 0x3c;
#else
		result = 1;
#endif
	else {
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchWrite(address, &inputRequests.touchData[3], length, 0);
	}
	inputRequests.touchAnswer[0] = result;
#if SOURCE_0103
	inputRequests.touchPending = 0;
#if SOURCE_0104
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#endif
#else
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

/* 0x0384: data[2] bytes (1-10) from panel 0's address data[0-1]; the result, then the bytes */
static void touchRequestRead0(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
#if SOURCE_0104
		result = 0x3c;
#else
		result = 1;
#endif
	else {
		memset(&inputRequests.touchAnswer[1], 0, 10);
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchRead(address, &inputRequests.touchAnswer[1], length, 0);
	}
	inputRequests.touchAnswer[0] = result;
#if SOURCE_0103
	inputRequests.touchPending = 0;
#if SOURCE_0104
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 11);
#else
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 11);
#endif
#else
	touchRequest.answerLength = 11;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

/* 0x0385: 0x0383 to panel 1 */
static void touchRequestWrite1(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
#if SOURCE_0104
		result = 0x3c;
#else
		result = 1;
#endif
	else {
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchWrite(address, &inputRequests.touchData[3], length, 1);
	}
	inputRequests.touchAnswer[0] = result;
#if SOURCE_0103
	inputRequests.touchPending = 0;
#if SOURCE_0104
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#endif
#else
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

/* 0x0386: 0x0384 from panel 1 */
static void touchRequestRead1(void)
{
	unsigned char result;
	unsigned char length;
	unsigned int address;

	length = inputRequests.touchData[2];
	if (length >= 11 || length == 0)
#if SOURCE_0104
		result = 0x3c;
#else
		result = 1;
#endif
	else {
		memset(&inputRequests.touchAnswer[1], 0, 10);
		address = inputRequests.touchData[0] | (unsigned int)inputRequests.touchData[1] << 8;
		result = touchRead(address, &inputRequests.touchAnswer[1], length, 1);
	}
	inputRequests.touchAnswer[0] = result;
#if SOURCE_0103
	inputRequests.touchPending = 0;
#if SOURCE_0104
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 11);
#else
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 11);
#endif
#else
	touchRequest.answerLength = 11;
	inputRequests.touchPending = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
/* 0x0387: each panel's power interval (data[1] if data[0], data[5] if data[4]) */
static void touchRequestSetPower(void)
{
#if INPUT_REQUEST_0105
	/* the slim's 0x0105: panel 0 refused (0x3e) when it is controller 2; the rear's controller
	   2 told to stop reporting for 0, else to report, and the interval (0xff: 5; 5-0xfa)
	   written to its blocks 0x1b and 0x19 */
	unsigned char result;
	unsigned char interval[2];

	result = 0;
	if (touchRequest.length == 8) {
		if (inputRequests.touchData[0] != 0) {
			interval[0] = inputRequests.touchData[1];
			if (touch.panel[0].controller == 2)
				result = 0x3e;
			else if (touch.panel[0].controller == 0)
				result = touchSetPower(interval[0], 0);
			if (result != 0) {
				result = 3;
				goto done;
			}
		}
		if (inputRequests.touchData[4] != 0) {
			interval[0] = inputRequests.touchData[5];
			if (touch.panel[1].controller == 2) {
				if (interval[0] == 0)
					result = touchRearSetStatus(1, inputRequests.touchAnswer, 1);
				else {
					if (interval[0] == 0xff)
						interval[0] = 5;
					else if (interval[0] < 5 || interval[0] >= 0xfb) {
						result = 0x3c;
						goto done;
					}
					if (touch.panel[1].controllerStatus != 0)
						result = touchRearSetStatus(0, inputRequests.touchAnswer, 1);
					result = touchRearWriteBlock(0x1b, 1, interval,
								     inputRequests.touchAnswer, 1);
					if (result != 0) {
						result = 4;
						goto done;
					}
					interval[1] = 0;
					result = touchRearWriteBlock(0x19, 2, interval,
								     inputRequests.touchAnswer, 1);
				}
			} else if (touch.panel[1].controller == 0)
				result = touchSetPower(interval[0], 1);
			if (result != 0) {
				result = 4;
				goto done;
			}
		}
	} else
		result = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#elif SOURCE_0104
	unsigned char result;

	result = 0;
	if (touchRequest.length == 8) {
		if (inputRequests.touchData[0] != 0) {
			result = touchSetPower(inputRequests.touchData[1], 0);
			if (result != 0) {
				result = 3;
				goto done;
			}
		}
		if (inputRequests.touchData[4] != 0) {
			result = touchSetPower(inputRequests.touchData[5], 1);
			if (result != 0)
				result = 4;
		}
	} else
		result = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	unsigned char result;
#if SOURCE_0103
	unsigned char status;
#endif

#if FW_SINCE(0x01000104)
	result = 0;	/* never set before 104: the answer is then whatever was left in L */
#endif
	if (touchRequest.length == 8) {
#if FW_SINCE(0x00090903)
		TOUCH_STATUS = 0;
#endif
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
#if !FW_SINCE(0x00090903)
		TOUCH_STATUS = 0;
#endif
	} else
		TOUCH_STATUS = 0x3b;
done:
	inputRequests.touchAnswer[0] = result;
#if SOURCE_0103
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 1);
#else
	touchRequest.answerLength = 1;
	inputRequests.touchPending = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
#endif
}
#endif

#if SOURCE_0103
/* 0x038a: whether the mode pin switches the panels' touch thresholds (touchTestModePin) */
static void touchRequestTestModePin(void)
{
	unsigned char status = 0;

	status = touchTestModePin(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 1);
}
#elif FW_SINCE(0x00090903)
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

#if FW_SINCE(0x01000104)
	result = 0;	/* never set before 104: the answer is then whatever was left in L */
#endif
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
#endif

#if FW_SINCE(0x0100060B)
/* 0x0390: each panel identified again, then its information block's first bytes, its
   configuration version and its grip edges (10 bytes a panel) */
static void touchRequestPanelInfo(void)
{
#if SOURCE_0103
	unsigned char result;
	unsigned char status = 0;
	unsigned char status0 = 0;
	unsigned char status1 = 0;

	memset(inputRequests.touchAnswer, 0, 0x14);
#if SOURCE_0104
	result = touchIdentify(0);
	if (result == 0) {
		memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
		if (touchReadConfigVersion(&inputRequests.touchAnswer[4], 0) != 0)
			status0 = 3;
		if (touchReadGripEdges(&inputRequests.touchAnswer[6], 0) != 0)
			status0 = 3;
	} else if (result == 0x40)
		status0 = 5;
	else
		status0 = 3;
	result = touchIdentify(1);
	if (result == 0) {
		memcpy(&inputRequests.touchAnswer[10], &touch.panel[1].family, 4);
		if (touchReadConfigVersion(&inputRequests.touchAnswer[14], 1) != 0)
			status1 = 4;
		if (touchReadGripEdges(&inputRequests.touchAnswer[16], 1) != 0)
			status1 = 4;
	} else if (result == 0x40)
		status1 = 6;
	else
		status1 = 4;
#else
	result = touchIdentify0();
	if (result == 0) {
		memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
#if FW_SINCE(0x01030511) && !BOARD_DOL1001
		result = touchReadConfigVersion0(&inputRequests.touchAnswer[4]);
		if (result != 0)
			status0 = 3;
		result = touchReadGripEdges0(&inputRequests.touchAnswer[6]);
		if (result != 0)
			status0 = 3;
#else
		if (touchReadConfigVersion0(&inputRequests.touchAnswer[4]) != 0)
			status0 = 3;
		if (touchReadGripEdges0(&inputRequests.touchAnswer[6]) != 0)
			status0 = 3;
#endif
	} else if (result == 4)
		status0 = 5;
	else
		status0 = 3;
	result = touchIdentify1();
	if (result == 0) {
		memcpy(&inputRequests.touchAnswer[10], &touch.panel[1].family, 4);
#if FW_SINCE(0x01030511) && !BOARD_DOL1001
		result = touchReadConfigVersion1(&inputRequests.touchAnswer[14]);
		if (result != 0)
			status1 = 4;
		result = touchReadGripEdges1(&inputRequests.touchAnswer[16]);
		if (result != 0)
			status1 = 4;
#else
		if (touchReadConfigVersion1(&inputRequests.touchAnswer[14]) != 0)
			status1 = 4;
		if (touchReadGripEdges1(&inputRequests.touchAnswer[16]) != 0)
			status1 = 4;
#endif
	} else if (result == 4)
		status1 = 6;
	else
		status1 = 4;
#endif
	if (status0 != 0) {
		if (status1 != 0)
			status = 7;
		else
			status = status0;
	} else
		status = status1;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 0x14);
#else
	touchIdentify0();
	memcpy(inputRequests.touchAnswer, &touch.panel[0].family, 4);
	touchReadConfigVersion0(&inputRequests.touchAnswer[4]);
	touchReadGripEdges0(&inputRequests.touchAnswer[6]);
	touchIdentify1();
	memcpy(&inputRequests.touchAnswer[10], &touch.panel[1].family, 4);
	touchReadConfigVersion1(&inputRequests.touchAnswer[14]);
	touchReadGripEdges1(&inputRequests.touchAnswer[16]);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 0x14;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

/* 0x0391: each panel's grip edges */
static void touchRequestGripEdges(void)
{
#if SOURCE_0103
	unsigned char status;

	status = 0;
#if SOURCE_0104
	if (touchReadGripEdges(&inputRequests.touchAnswer[0], 0) != 0)
		status = 3;
	else if (touchReadGripEdges(&inputRequests.touchAnswer[4], 1) != 0)
		status = 4;
#else
	if (touchReadGripEdges0(&inputRequests.touchAnswer[0]) != 0)
		status = 3;
	else if (touchReadGripEdges1(&inputRequests.touchAnswer[4]) != 0)
		status = 4;
#endif
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
	touchReadGripEdges0(&inputRequests.touchAnswer[0]);
	touchReadGripEdges1(&inputRequests.touchAnswer[4]);
	inputRequests.touchPending = 0;
	touchRequest.answerLength = 8;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
	return;
#endif
}
#endif

/* 0x03a0: panel 0's controller to its bootloader; its status 8 runs later */
static void touchRequestEnterBootloader0(void)
{
#if INPUT_REQUEST_0105
	/* the slim's 0x0105: one answer, 0x21 for a phase not known */
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchEnterBootloader(0);
		if (result == 0) {
			inputRequests.touchTimer = 8;
			inputRequests.touchPhase = 1;
			return;
		}
		break;
	case 1:
		if (--inputRequests.touchTimer != 0)
			return;
		result = touchBootloaderStatus(inputRequests.touchAnswer, 0);
		break;
	default:
		result = 0x21;
		break;
	}
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchEnterBootloader(0);
		if (result == 0) {
			inputRequests.touchTimer = 8;
			inputRequests.touchPhase = 1;
			break;
		}
		goto answer;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			result = touchBootloaderStatus(inputRequests.touchAnswer, 0);
answer:
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		}
		break;
	}
#else
	switch (inputRequests.touchPhase) {
	case 0:
		touchEnterBootloader0();
		inputRequests.touchTimer = 8;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			touchBootloaderStatus0(inputRequests.touchAnswer);
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
}

/* 0x03b0: 0x03a0 for panel 1 */
static void touchRequestEnterBootloader1(void)
{
#if INPUT_REQUEST_0105
	/* the slim's 0x0105: one answer, 0x21 for a phase not known */
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchEnterBootloader(1);
		if (result == 0) {
			inputRequests.touchTimer = 8;
			inputRequests.touchPhase = 1;
			return;
		}
		break;
	case 1:
		if (--inputRequests.touchTimer != 0)
			return;
		result = touchBootloaderStatus(inputRequests.touchAnswer, 1);
		break;
	default:
		result = 0x21;
		break;
	}
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchEnterBootloader(1);
		if (result == 0) {
			inputRequests.touchTimer = 8;
			inputRequests.touchPhase = 1;
			break;
		}
		goto answer;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			result = touchBootloaderStatus(inputRequests.touchAnswer, 1);
answer:
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		}
		break;
	}
#else
	switch (inputRequests.touchPhase) {
	case 0:
		touchEnterBootloader1();
		inputRequests.touchTimer = 8;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			touchBootloaderStatus1(inputRequests.touchAnswer);
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
}

/* 0x03a1: panel 0's bootloader unlocked; its status once its CHG line (BOARD_TOUCH_CHG_PIN) falls, within
   10 runs */
static void touchRequestUnlock0(void)
{
#if INPUT_REQUEST_0105
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBootloaderUnlock(0);
		if (result != 0)
			goto answer;
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG_PIN) {
				result = touchBootloaderStatus(inputRequests.touchAnswer, 0);
				goto answer;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	default:
		result = 0x21;
answer:
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		break;
	}
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBootloaderUnlock(0);
		if (result == 0) {
			inputRequests.touchTimer = 10;
			inputRequests.touchPhase = 1;
			break;
		}
		goto answer;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG_PIN) {
				result = touchBootloaderStatus(inputRequests.touchAnswer, 0);
				goto answer;
			}
		} else {
			result = 1;
answer:
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		}
		break;
	}
#else
	switch (inputRequests.touchPhase) {
	case 0:
		touchBootloaderUnlock0();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG_PIN) {
				touchBootloaderStatus0(inputRequests.touchAnswer);
#if SOURCE_0103
				inputRequests.touchPending = 0;
				hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
				touchRequest.answerLength = 1;
				inputRequests.touchPending = 0;
				touchRequest.status = 0;
				hostRequestAnswerReady(&touchRequest.state);
#endif
			}
		} else {
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 1, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 1;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
}

/* 0x03b1: 0x03a1 for panel 1 (its CHG line BOARD_TOUCH_CHG1_PIN) */
static void touchRequestUnlock1(void)
{
#if INPUT_REQUEST_0105
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBootloaderUnlock(1);
		if (result != 0)
			goto answer;
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG1_PIN) {
				result = touchBootloaderStatus(inputRequests.touchAnswer, 1);
				goto answer;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	default:
		result = 0x21;
answer:
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		break;
	}
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBootloaderUnlock(1);
		if (result == 0) {
			inputRequests.touchTimer = 10;
			inputRequests.touchPhase = 1;
			break;
		}
		goto answer;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG1_PIN) {
				result = touchBootloaderStatus(inputRequests.touchAnswer, 1);
				goto answer;
			}
		} else {
			result = 1;
answer:
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		}
		break;
	}
#else
	switch (inputRequests.touchPhase) {
	case 0:
		touchBootloaderUnlock1();
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer != 0) {
			if (!BOARD_TOUCH_CHG1_PIN) {
				touchBootloaderStatus1(inputRequests.touchAnswer);
#if SOURCE_0103
				inputRequests.touchPending = 0;
				hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
				touchRequest.answerLength = 1;
				inputRequests.touchPending = 0;
				touchRequest.status = 0;
				hostRequestAnswerReady(&touchRequest.state);
#endif
			}
		} else {
#if SOURCE_0103
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 1, 1);
#else
			touchRequest.answerLength = 1;
			inputRequests.touchPending = 0;
			touchRequest.status = 1;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
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
#if !SOURCE_0104
			if (result != 0)
				result = 1;
			else
				result = 0;
#endif
		} else
			result = 0x3c;
	} else
		result = 0x3b;
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
#else
	touchRequest.answerLength = 3;
	touchRequest.status = result;
	hostRequestAnswerReady(&touchRequest.state);
#endif
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
#if SOURCE_0104
			result = touchBootloaderStatus(&inputRequests.touchAnswer[2], 1);
#else
#if SOURCE_0103
			result = touchBootloaderStatus1(&inputRequests.touchAnswer[2]);
#else
			result = touchRead(TOUCH_BOOTLOADER, &inputRequests.touchAnswer[2], 1, 1);
#endif
			if (result != 0)
				result = 1;
			else
				result = 0;
#endif
		} else
			result = 0x3c;
	} else
		result = 0x3b;
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
#else
	touchRequest.answerLength = 3;
	touchRequest.status = result;
	hostRequestAnswerReady(&touchRequest.state);
#endif
}

/*
 * 0x03a3: the frame gathered (its number in data[0-1]) to panel 0's bootloader, then its
 * status at each fall of the CHG line (BOARD_TOUCH_CHG_PIN), 50 runs at most each.
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
#if !SOURCE_0103
				if (touchBuffer.end == TOUCH_BUFFER_SIZE)
					touchBuffer.end += touchFlashExtra;
#endif
				result = touchWrite(TOUCH_BOOTLOADER, touchBuffer.data, touchBuffer.end,
						    0);
#if SOURCE_0104
				if (result != 0)
					goto answer;
#endif
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
			if (!BOARD_TOUCH_CHG_PIN) {
#if INPUT_REQUEST_0105
				result = touchBootloaderStatus(&inputRequests.touchAnswer[2], 0);
				if (result != 0)
					goto answer;
#elif SOURCE_0104
				touchBootloaderStatus(&inputRequests.touchAnswer[2], 0);
#else
				touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
#endif
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
			if (!BOARD_TOUCH_CHG_PIN) {
#if INPUT_REQUEST_0105
				result = touchBootloaderStatus(&inputRequests.touchAnswer[2], 0);
				if (result != 0)
					goto answer;
#elif SOURCE_0104
				touchBootloaderStatus(&inputRequests.touchAnswer[2], 0);
#else
				touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
#endif
				inputRequests.touchPhase = 3;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
#if SOURCE_0104
			touchBootloaderStatus(&inputRequests.touchAnswer[2], 0);
#else
			touchBootloaderStatus0(&inputRequests.touchAnswer[2]);
#endif
			goto answer;
		}
		break;
	case 3:
		if (inputRequests.touchTimer == 0 || !BOARD_TOUCH_CHG_PIN) {
			inputRequests.touchAnswer[0] = inputRequests.touchData[0];
			inputRequests.touchAnswer[1] = inputRequests.touchData[1];
			inputRequests.touchPending = 0;
#if SOURCE_0103
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 3);
#else
			touchRequest.answerLength = 3;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		inputRequests.touchTimer--;
		break;
	default:
#if INPUT_REQUEST_0105
		result = 0x21;
		goto answer;
#else
		break;
#endif
	}
	return;
answer:
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
#else
	touchRequest.answerLength = 3;
	hostRequestAnswerReady(&touchRequest.state);
	touchRequest.status = result;
#endif
}

/* 0x03b3: 0x03a3 for panel 1 (its CHG line BOARD_TOUCH_CHG1_PIN) */
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
#if TOUCH_PANEL1_IICA1 && !INPUT_REQUEST_0105	/* the slim's 0x0105: touchWrite again */
				result = touchBootloaderWrite(touchBuffer.data, touchBuffer.end, 1);
#else
				result = touchWrite(TOUCH_BOOTLOADER, touchBuffer.data, touchBuffer.end,
						    1);
#if SOURCE_0104
				if (result != 0)
					goto answer;
#endif
#endif
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
			if (!BOARD_TOUCH_CHG1_PIN) {
#if INPUT_REQUEST_0105
				result = touchBootloaderStatus(&inputRequests.touchAnswer[2], 1);
				if (result != 0)
					goto answer;
#elif SOURCE_0104
				touchBootloaderStatus(&inputRequests.touchAnswer[2], 1);
#else
				touchBootloaderStatus1(&inputRequests.touchAnswer[2]);
#endif
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
			if (!BOARD_TOUCH_CHG1_PIN) {
#if INPUT_REQUEST_0105
				result = touchBootloaderStatus(&inputRequests.touchAnswer[2], 1);
				if (result != 0)
					goto answer;
#elif SOURCE_0104
				touchBootloaderStatus(&inputRequests.touchAnswer[2], 1);
#else
				touchBootloaderStatus1(&inputRequests.touchAnswer[2]);
#endif
				inputRequests.touchPhase = 3;
				inputRequests.touchTimer = 50;
			}
		} else {
			result = 1;
			goto answer;
		}
		break;
	case 3:
		if (inputRequests.touchTimer == 0 || !BOARD_TOUCH_CHG1_PIN) {
			inputRequests.touchAnswer[0] = inputRequests.touchData[0];
			inputRequests.touchAnswer[1] = inputRequests.touchData[1];
			inputRequests.touchPending = 0;
#if SOURCE_0103
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 3);
#else
			touchRequest.answerLength = 3;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		inputRequests.touchTimer--;
		break;
	default:
#if INPUT_REQUEST_0105
		result = 0x21;
		goto answer;
#else
		break;
#endif
	}
	return;
answer:
	inputRequests.touchAnswer[0] = inputRequests.touchData[0];
	inputRequests.touchAnswer[1] = inputRequests.touchData[1];
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
#else
	touchRequest.answerLength = 3;
	hostRequestAnswerReady(&touchRequest.state);
	touchRequest.status = result;
#endif
}

/* 0x03a4 */
static void touchRequestBootloaderStatus0(void)
{
#if SOURCE_0104
	unsigned char result = 0;

	result = touchBootloaderStatus(inputRequests.touchAnswer, 0);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	touchBootloaderStatus0(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
}

/* 0x03b4 */
static void touchRequestBootloaderStatus1(void)
{
#if SOURCE_0104
	unsigned char result = 0;

	result = touchBootloaderStatus(inputRequests.touchAnswer, 1);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	touchBootloaderStatus1(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
}

#if FW_SINCE(0x00090006)	/* not in 0x00080504 */
/* 0x03a5: panel 0's settings saved, answered 10 runs later */
static void touchRequestBackup0(void)
{
#if INPUT_REQUEST_0105
	/* the slim's 0x0105: the wait by the controller (family and variant), 0x21 for a phase
	   not known */
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBackup(0);
		if (result != 0) {
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
			return;
		}
		switch (TOUCH_PANEL_ID(0)) {
		case 0x0a80:
			inputRequests.touchTimer = 0x19;
			break;
		case 0x2782:
			inputRequests.touchTimer = 0x20;
			break;
		case 0x2b82:
			inputRequests.touchTimer = 0x17;
			break;
		case 0x3a82:
			inputRequests.touchTimer = 0x17;
			break;
		case 0x3482:
		default:
			inputRequests.touchTimer = 0x26;
			break;
		}
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
		}
		break;
	default:
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x21, 0);
		break;
	}
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBackup(0);
		if (result != 0)
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
		}
		break;
	}
#else
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
#if SOURCE_0103
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
}

/* 0x03b5 */
static void touchRequestBackup1(void)
{
#if INPUT_REQUEST_0105
	/* the slim's 0x0105: the wait by the controller (family and variant), 0x21 for a phase
	   not known */
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBackup(1);
		if (result != 0) {
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
			return;
		}
		switch (TOUCH_PANEL_ID(1)) {
		case 0x0a80:
			inputRequests.touchTimer = 0x19;
			break;
		case 0x2782:
			inputRequests.touchTimer = 0x20;
			break;
		case 0x2b82:
			inputRequests.touchTimer = 0x17;
			break;
		case 0x3a82:
			inputRequests.touchTimer = 0x17;
			break;
		case 0x3482:
		default:
			inputRequests.touchTimer = 0x26;
			break;
		}
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
		}
		break;
	default:
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x21, 0);
		break;
	}
#elif SOURCE_0104
	unsigned char result = 0;

	switch (inputRequests.touchPhase) {
	case 0:
		result = touchBackup(1);
		if (result != 0)
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
		inputRequests.touchTimer = 10;
		inputRequests.touchPhase = 1;
		break;
	case 1:
		if (--inputRequests.touchTimer == 0) {
			inputRequests.touchAnswer[0] = 0;
			inputRequests.touchPending = 0;
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
		}
		break;
	}
#else
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
#if SOURCE_0103
			hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
			touchRequest.answerLength = 1;
			touchRequest.status = 0;
			hostRequestAnswerReady(&touchRequest.state);
#endif
		}
		break;
	}
#endif
}

/* 0x03a6 */
static void touchRequestReset0(void)
{
#if SOURCE_0104
	unsigned char result = 0;

	result = touchReset(0);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
#else
	touchReset0();
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 0);
#else
	touchRequest.answerLength = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
}

/* 0x03b6 */
static void touchRequestReset1(void)
{
#if SOURCE_0104
	unsigned char result = 0;

	result = touchReset(1);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
#else
	touchReset1();
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 0);
#else
	touchRequest.answerLength = 0;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
}

/* 0x03a7 */
static void touchRequestConfigVersion0(void)
{
#if SOURCE_0104
	unsigned char status;

	status = 0;
	if (touchReadConfigVersion(inputRequests.touchAnswer, 0) != 0)
		status = 3;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 2);
#else
#if SOURCE_0103
	unsigned char status;

	status = 0;
	if (touchReadConfigVersion0(inputRequests.touchAnswer) != 0)
		status = 3;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 2);
#else
#if FW_SINCE(0x00090502)
	touchReadConfigVersion0(inputRequests.touchAnswer);
#else
	inputRequests.touchAnswer[0] = 0;	/* no version read yet */
	inputRequests.touchAnswer[1] = 0;
#endif
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 2);
#else
	touchRequest.answerLength = 2;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
#endif
}

/* 0x03b7 */
static void touchRequestConfigVersion1(void)
{
#if SOURCE_0104
	unsigned char status;

	status = 0;
	if (touchReadConfigVersion(inputRequests.touchAnswer, 1) != 0)
		status = 4;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 2);
#else
#if SOURCE_0103
	unsigned char status;

	status = 0;
	if (touchReadConfigVersion1(inputRequests.touchAnswer) != 0)
		status = 4;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 2);
#else
#if FW_SINCE(0x00090502)
	touchReadConfigVersion1(inputRequests.touchAnswer);
#else
	inputRequests.touchAnswer[0] = 0;	/* no version read yet */
	inputRequests.touchAnswer[1] = 0;
#endif
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 2);
#else
	touchRequest.answerLength = 2;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
#endif
}
#endif

#if FW_SINCE(0x00090502)
/* 0x03a8 */
static void touchRequestUserData0(void)
{
#if SOURCE_0104
	unsigned char status;

	status = 0;
	if (touchReadUserData(inputRequests.touchAnswer, 0) != 0)
		status = 3;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
#if SOURCE_0103
	unsigned char status;

	status = 0;
	if (touchReadUserData0(inputRequests.touchAnswer) != 0)
		status = 3;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
	touchReadUserData0(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 8);
#else
	touchRequest.answerLength = 8;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
#endif
}

/* 0x03b8 */
static void touchRequestUserData1(void)
{
#if SOURCE_0104
	unsigned char status;

	status = 0;
	if (touchReadUserData(inputRequests.touchAnswer, 1) != 0)
		status = 4;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
#if SOURCE_0103
	unsigned char status;

	status = 0;
	if (touchReadUserData1(inputRequests.touchAnswer) != 0)
		status = 4;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 8);
#else
	touchReadUserData1(inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 8);
#else
	touchRequest.answerLength = 8;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
	return;
#endif
#endif
}

/* 0x03a9: panel 0's self test 0x17 */
static void touchRequestSelfTest0(void)
{
#if SOURCE_0104
	unsigned char result = 0;

	result = touchSelfTest(0x17, inputRequests.touchAnswer, 0);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	touchSelfTest0(0x17, inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
#endif
}

/* 0x03b9 */
static void touchRequestSelfTest1(void)
{
#if SOURCE_0104
	unsigned char result = 0;

#if INPUT_REQUEST_0105
	/* the rear controller 2 answers its state for data[0] instead */
	if (touch.panel[1].controller == 0)
		result = touchSelfTest(0x17, inputRequests.touchAnswer, 1);
	else if (touch.panel[1].controller == 2)
		result = touchRearGetState(inputRequests.touchData[0], inputRequests.touchAnswer, 1);
#else
	result = touchSelfTest(0x17, inputRequests.touchAnswer, 1);
#endif
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
#else
	touchSelfTest1(0x17, inputRequests.touchAnswer);
	inputRequests.touchPending = 0;
#if SOURCE_0103
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
#else
	touchRequest.answerLength = 1;
	touchRequest.status = 0;
	hostRequestAnswerReady(&touchRequest.state);
#endif
#endif
}
#endif

#if SOURCE_0103
/* 0x03aa: panel 0's T51 bytes 5-6 (if data[1] bit 0: data[2-3]) and 9-10 (if bit 1:
   data[4-5]), then its byte 4 (data[0]); then T34's bytes, every run until the first is not
   0x80 (125 runs at most) */
static void touchRequest03AA(void)
{
#if SOURCE_0104
	unsigned char status;

	switch (inputRequests.touchPhase) {
	case 0:
		memset(inputRequests.touchAnswer, 0, 7);
		if (touchRequest.length == 6) {
			if (inputRequests.touchData[1] & 0x01) {
				status = touchSetT51Bytes5(&inputRequests.touchData[2], 0);
				if (status != 0)
					goto answer;
			}
			if (inputRequests.touchData[1] & 0x02) {
				status = touchSetT51Bytes9(&inputRequests.touchData[4], 0);
				if (status != 0)
					goto answer;
			}
			status = touchSetT51Byte4(inputRequests.touchData[0], 0);
			if (status != 0)
				goto answer;
		} else {
			status = 0x3b;
			goto answer;
		}
		inputRequests.touchPhase = 2;
		inputRequests.touchTimer = 125;
		break;
	case 1:
		if (--inputRequests.touchTimer < 0x59)
			inputRequests.touchPhase = 2;
		break;
	case 2:
		status = touchReadT34Bytes(inputRequests.touchAnswer, 0);
		if (status != 0)
			goto answer;
		if (inputRequests.touchAnswer[0] != 0x80) {
			status = 0;
			goto answer;
		}
		if (--inputRequests.touchTimer != 0)
			break;
		status = 1;
		goto answer;
	default:
#if INPUT_REQUEST_0105
		status = 0x21;
		goto answer;
#else
		break;
#endif
	}
	return;
answer:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 7);
#else
	unsigned char status;

	switch (inputRequests.touchPhase) {
	case 0:
		memset(inputRequests.touchAnswer, 0, 7);
		if (touchRequest.length == 6) {
			if (inputRequests.touchData[1] & 0x01) {
				status = touchSetT51Bytes5_0(&inputRequests.touchData[2]);
				if (status != 0)
					goto answer;
			}
			if (inputRequests.touchData[1] & 0x02) {
				status = touchSetT51Bytes9_0(&inputRequests.touchData[4]);
				if (status != 0)
					goto answer;
			}
			status = touchSetT51Byte4_0(inputRequests.touchData[0]);
			if (status != 0)
				goto answer;
		} else {
			status = 0x3b;
			goto answer;
		}
		inputRequests.touchPhase = 1;
		inputRequests.touchTimer = 125;
		break;
	case 1:
		status = touchReadT34Bytes0(inputRequests.touchAnswer);
		if (status != 0)
			goto answer;
		if (inputRequests.touchAnswer[0] != 0x80) {
			status = 0;
			goto answer;
		}
		if (--inputRequests.touchTimer != 0)
			break;
		status = 1;
		goto answer;
	default:
		break;
	}
	return;
answer:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 7);
#endif
}

/* 0x03ba: panel 1's T51 bytes 5-6 (if data[1] bit 0: data[2-3]) and 9-10 (if bit 1:
   data[4-5]), then its byte 4 (data[0]); then T34's bytes, every run until the first is not
   0x80 (125 runs at most) */
static void touchRequest03BA(void)
{
#if SOURCE_0104
	unsigned char status;

	switch (inputRequests.touchPhase) {
	case 0:
		memset(inputRequests.touchAnswer, 0, 7);
		if (touchRequest.length == 6) {
			if (inputRequests.touchData[1] & 0x01) {
				status = touchSetT51Bytes5(&inputRequests.touchData[2], 1);
				if (status != 0)
					goto answer;
			}
			if (inputRequests.touchData[1] & 0x02) {
				status = touchSetT51Bytes9(&inputRequests.touchData[4], 1);
				if (status != 0)
					goto answer;
			}
			status = touchSetT51Byte4(inputRequests.touchData[0], 1);
			if (status != 0)
				goto answer;
		} else {
			status = 0x3b;
			goto answer;
		}
		inputRequests.touchPhase = 2;
		inputRequests.touchTimer = 125;
		break;
	case 1:
		if (--inputRequests.touchTimer < 0x6c)
			inputRequests.touchPhase = 2;
		break;
	case 2:
		status = touchReadT34Bytes(inputRequests.touchAnswer, 1);
		if (status != 0)
			goto answer;
		if (inputRequests.touchAnswer[0] != 0x80) {
			status = 0;
			goto answer;
		}
		if (--inputRequests.touchTimer != 0)
			break;
		status = 1;
		goto answer;
	default:
#if INPUT_REQUEST_0105
		status = 0x21;
		goto answer;
#else
		break;
#endif
	}
	return;
answer:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 7);
#else
	unsigned char status;

	switch (inputRequests.touchPhase) {
	case 0:
		memset(inputRequests.touchAnswer, 0, 7);
		if (touchRequest.length == 6) {
			if (inputRequests.touchData[1] & 0x01) {
				status = touchSetT51Bytes5_1(&inputRequests.touchData[2]);
				if (status != 0)
					goto answer;
			}
			if (inputRequests.touchData[1] & 0x02) {
				status = touchSetT51Bytes9_1(&inputRequests.touchData[4]);
				if (status != 0)
					goto answer;
			}
			status = touchSetT51Byte4_1(inputRequests.touchData[0]);
			if (status != 0)
				goto answer;
		} else {
			status = 0x3b;
			goto answer;
		}
		inputRequests.touchPhase = 1;
		inputRequests.touchTimer = 125;
		break;
	case 1:
		status = touchReadT34Bytes1(inputRequests.touchAnswer);
		if (status != 0)
			goto answer;
		if (inputRequests.touchAnswer[0] != 0x80) {
			status = 0;
			goto answer;
		}
		if (--inputRequests.touchTimer != 0)
			break;
		status = 1;
		goto answer;
	default:
		break;
	}
	return;
answer:
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 7);
#endif
}

/* 0x03ab: panel 0's T18 byte 1 (touchSetComms): 1 if data[0] bit 7 is clear, else (if
   data[1] bit 7) 2 or 3 by data[0] bit 6 */
static void touchRequest03AB(void)
{
	unsigned char status = 0;

	if (touchRequest.length == 2) {
		if (inputRequests.touchData[0] & 0x80) {
			if (inputRequests.touchData[1] & 0x80) {
				if (inputRequests.touchData[0] & 0x40)
					status = touchSetComms(2, 0);
				else
					status = touchSetComms(3, 0);
			}
		} else
			status = touchSetComms(1, 0);
	} else
		status = 0x3b;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 0);
}

/* 0x03ac: panel 0's CHG line, in bit 7 */
static void touchRequest03AC(void)
{
	inputRequests.touchAnswer[0] = 0;
	if (BOARD_TOUCH_CHG_PIN)
		inputRequests.touchAnswer[0] |= 0x80;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
}

/* 0x03bb: panel 1's T18 byte 1 (touchSetComms): 1 if data[0] bit 7 is clear, else (if
   data[1] bit 7) 2 or 3 by data[0] bit 6 */
static void touchRequest03BB(void)
{
	unsigned char status = 0;

	if (touchRequest.length == 2) {
		if (inputRequests.touchData[0] & 0x80) {
			if (inputRequests.touchData[1] & 0x80) {
				if (inputRequests.touchData[0] & 0x40)
					status = touchSetComms(2, 1);
				else
					status = touchSetComms(3, 1);
			}
		} else
			status = touchSetComms(1, 1);
	} else
		status = 0x3b;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 0);
}

/* 0x03bc: panel 1's CHG line, in bit 7 */
static void touchRequest03BC(void)
{
	inputRequests.touchAnswer[0] = 0;
	if (BOARD_TOUCH_CHG1_PIN)
		inputRequests.touchAnswer[0] |= 0x80;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 1);
}

/* 0x03ad: panel 0's T34 bit (touchReadT34Bit3) */
static void touchRequest03AD(void)
{
	unsigned char status;

	status = touchReadT34Bit3(inputRequests.touchAnswer, 0);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 1);
}

/* 0x03bd: panel 1's T34 bit (touchReadT34Bit3) */
static void touchRequest03BD(void)
{
	unsigned char status;

	status = touchReadT34Bit3(inputRequests.touchAnswer, 1);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, status, 1);
}

#if SOURCE_0104
/* 0x03ae: panel 0's T34 bytes 8-10 (touchReadT34Bytes8) */
static void touchRequest03AE(void)
{
	unsigned char result;

	result = touchReadT34Bytes8(inputRequests.touchAnswer, 0);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
}

/* 0x03be: panel 1's T34 bytes 8-10 */
static void touchRequest03BE(void)
{
	unsigned char result;

	result = touchReadT34Bytes8(inputRequests.touchAnswer, 1);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
}

/* 0x03af: panel 0's T6 byte 4 to data[0] (touchSetT6Byte4) */
static void touchRequest03AF(void)
{
	unsigned char result;

	if (touchRequest.length == 1)
		result = touchSetT6Byte4(&inputRequests.touchData[0], 0);
	else
		result = 0x3b;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x03bf: panel 1's T6 byte 4 to data[0] */
static void touchRequest03BF(void)
{
	unsigned char result;

	if (touchRequest.length == 1)
		result = touchSetT6Byte4(&inputRequests.touchData[0], 1);
	else
		result = 0x3b;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}
#endif

#if INPUT_REQUEST_0105
/* the slim's 0x0105: the rear panel's controller commands. data[0] is the panel (0 refused,
   0x3e); the answers go to touchAnswer */

/* 0x03c0 and 0x03c1: data written to panel 1's controller and its 0x2a bytes read back (0x03c0
   names panel 0: refused) */
static void touchRequest03C0(void)
{
	unsigned char tx[0x20];
	unsigned char rx[0x2a];
	unsigned char result;
	unsigned char length;
	char panel;

	if (touchRequest.command == 0x03c0) {
		panel = 0;
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	panel = 1;
	length = touchRequest.length;
	if (length <= sizeof(tx)) {
		memcpy(tx, inputRequests.touchData, length);
		memset(rx, 0, sizeof(rx));
		result = touchWriteRead(tx, length, rx, sizeof(rx), panel, 3000);
		memcpy(inputRequests.touchAnswer, rx, sizeof(rx));
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, sizeof(rx));
	} else {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3b, 0);
	}
}

/* 0x0392: touchRearSetActive */
static void touchRequest0392(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearSetActive(panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x0393: touchRearSetIdle */
static void touchRequest0393(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearSetIdle(panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x0394: command 0x29 with data[1] (0: 7) */
static void touchRequest0394(void)
{
	unsigned char result;
	char panel;
	unsigned char index;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	index = inputRequests.touchData[1];
	if (index == 0)
		index = 7;
	result = touchRearCommand29(index, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x0395: command 0x2a */
static void touchRequest0395(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand2a(panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x0396: data[4] words (16 at most) from data[2] of block data[1] (touchRearReadWords) */
static void touchRequest0396(void)
{
	unsigned char result;
	char panel;
	unsigned int address;
	unsigned int count;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	address = inputRequests.touchData[2];
	count = inputRequests.touchData[4];
	if (count > 0x10) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
		return;
	}
	result = touchRearReadWords(inputRequests.touchData[1], address, count,
				    inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, (unsigned char)(count * 2) + 2);
}

/* 0x0397: command 0x28 with data[1] */
static void touchRequest0397(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand28(inputRequests.touchData[1], panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x0398: data[4] bytes (32 at most) from data[2] of block data[1] (touchRearReadRange) */
static void touchRequest0398(void)
{
	unsigned char result;
	char panel;
	unsigned int address;
	unsigned int count;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	address = inputRequests.touchData[2];
	count = inputRequests.touchData[4];
	if (count > 0x20) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
		return;
	}
	result = touchRearReadRange(inputRequests.touchData[1], address, count,
				    inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 2 + (unsigned char)count);
}

/* 0x0399: the same with command 0x27 (touchRearReadRange2) */
static void touchRequest0399(void)
{
	unsigned char result;
	char panel;
	unsigned int address;
	unsigned int count;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	address = inputRequests.touchData[2];
	count = inputRequests.touchData[4];
	if (count > 0x20) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
		return;
	}
	result = touchRearReadRange2(inputRequests.touchData[1], address, count,
				     inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 2 + (unsigned char)count);
}

/* 0x039a: the controller's identity (touchRearIdentify), bytes 4 and 5 swapped */
static void touchRequest039A(void)
{
	unsigned char result;
	char panel;
	unsigned char byte;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearIdentify(inputRequests.touchAnswer, TOUCH_REAR_ALL, panel);
	byte = inputRequests.touchAnswer[4];
	inputRequests.touchAnswer[4] = inputRequests.touchAnswer[5];
	inputRequests.touchAnswer[5] = byte;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0x12);
}

/* 0x039b: the short report on (data[1]) or off (touchRearSetReportShort) */
static void touchRequest039B(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearSetReportShort(inputRequests.touchData[1], panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x039c: command 0x72 with data[1] on */
static void touchRequest039C(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand72(&inputRequests.touchData[1], panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x039d: which controller the panel has (touchGetController); panel 0 too */
static void touchRequest039D(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	result = touchGetController(inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
}

/* 0x039e: block data[1] (touchRearReadBlock) */
static void touchRequest039E(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearReadBlock(inputRequests.touchData[1], inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 6);
}

/* 0x039f: data[2] bytes from data[3] to block data[1] (touchRearWriteBlock) */
static void touchRequest039F(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearWriteBlock(inputRequests.touchData[1], inputRequests.touchData[2],
				     &inputRequests.touchData[3], inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 2);
}

/* 0x03c2: the controller to its bootloader (touchRearEnterBootloader) */
static void touchRequest03C2(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearEnterBootloader(panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 0);
}

/* 0x03c3: data[5] bytes from data[6] into touchBuffer at offset data[3..4] of the write for
   address data[1..2] (0x01ff: the checked command's), at most 0x80 */
static void touchRequest03C3(void)
{
	char panel;
	union TouchRequestWord address;
	union TouchRequestWord offset;
	unsigned char length;
	union TouchRearBufferCommand *buffer;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	address.byte[0] = inputRequests.touchData[1];
	address.byte[1] = inputRequests.touchData[2];
	offset.byte[0] = inputRequests.touchData[3];
	offset.byte[1] = inputRequests.touchData[4];
	length = inputRequests.touchData[5];
	if (offset.word + length > 0x80) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3c, 0);
		return;
	}
	buffer = (union TouchRearBufferCommand *)touchBuffer.data;
	if (address.word == 0x01ff)
		memcpy(&buffer->key.data[offset.word], &inputRequests.touchData[6], length);
	else {
		memcpy(&buffer->page.data[offset.word], &inputRequests.touchData[6], length);
		buffer->page.pageZero = 0;
		buffer->page.pageLow = address.byte[0];
		buffer->page.pageHigh = address.byte[1];
	}
	inputRequests.touchAnswer[0] = address.byte[0];
	inputRequests.touchAnswer[1] = address.byte[1];
	inputRequests.touchAnswer[2] = 0;
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0, 3);
}

/* 0x03c4: touchBuffer's write for address data[1..2] sent (command 0x39); 0x03c8: the checked
   command 0x48 for address 0x01ff */
static void touchRequest03C4(void)
{
	unsigned char result;
	char panel;
	union TouchRequestWord address;
	union TouchRearBufferCommand *buffer;

	result = 0;
	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	address.byte[0] = inputRequests.touchData[1];
	address.byte[1] = inputRequests.touchData[2];
	buffer = (union TouchRearBufferCommand *)touchBuffer.data;
	if (touchRequest.command == 0x03c8) {
		if (address.word == 0x01ff)
			result = touchRearCommand48(address.word, inputRequests.touchAnswer, panel);
		else
			result = 0x3c;
	} else {
		if (memcmp(&address, &buffer->page.pageLow, 2) == 0) {
			buffer->page.pageZero = 0;
			result = touchRearCommand39(inputRequests.touchAnswer, panel);
		} else
			result = 0x3c;
	}
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 3);
}

/* 0x03c5: command 0x38 */
static void touchRequest03C5(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand38(inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 8);
}

/* 0x03c6: command 0x31 */
static void touchRequest03C6(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand31(inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
}

/* 0x03c7: command 0x3b */
static void touchRequest03C7(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearCommand3b(inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
}

/* 0x03c9: block data[1] (touchRearRead) */
static void touchRequest03C9(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearRead(inputRequests.touchData[1], inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 5);
}

/* 0x03ca: data[4] bytes (32 at most) of memory data[2..3] of block data[1] (below 3) into
   touchBuffer (touchRearReadMemory), and to the host */
static void touchRequest03CA(void)
{
	unsigned char result;
	char panel;
	unsigned char index;
	unsigned char length;
	union TouchRequestWord address;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	index = inputRequests.touchData[1];
	address.byte[0] = inputRequests.touchData[2];
	address.byte[1] = inputRequests.touchData[3];
	length = inputRequests.touchData[4];
	if (index >= 3 || length > 0x20)
		result = 0x3c;
	else {
		result = touchRearReadMemory(index, address.word, length, touchBuffer.data, panel);
		if (result == 0)
			memcpy(inputRequests.touchAnswer, touchBuffer.data, 2 + length);
	}
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 2 + length);
}

/* 0x03cb: data[4] bytes (32 at most) from data[5] to memory data[2..3] of block data[1] (below
   3; touchRearWriteMemory) */
static void touchRequest03CB(void)
{
	unsigned char result;
	char panel;
	unsigned char index;
	unsigned char length;
	union TouchRequestWord address;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	index = inputRequests.touchData[1];
	address.byte[0] = inputRequests.touchData[2];
	address.byte[1] = inputRequests.touchData[3];
	length = inputRequests.touchData[4];
	if (index >= 3 || length > 0x20)
		result = 0x3c;
	else
		result = touchRearWriteMemory(index, address.word, length, &inputRequests.touchData[5],
					      inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 2);
}

/* 0x03cc: the controller's status set to data[1] (touchRearSetStatus) */
static void touchRequest03CC(void)
{
	unsigned char result;
	char panel;

	panel = inputRequests.touchData[0];
	if (panel == 0) {
		inputRequests.touchPending = 0;
		hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		return;
	}
	result = touchRearSetStatus(inputRequests.touchData[1], inputRequests.touchAnswer, panel);
	inputRequests.touchPending = 0;
	hostRequestAnswer(&touchRequest, HOST_REQUEST_ANSWER, result, 1);
}
#endif
#endif

#if BOARD_MOTION
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
#if HW_IRT001 && !FW_SINCE(0x01000104)
	unsigned char result;	/* 0x00315200's before 0x01000104: unused, its register saved */
#elif (HW_102603 && !FW_SINCE(0x00090203)) || HW_IS(0x00404400)
	unsigned char result;	/* and 0x00102603's 0x00090009, 0x00404400's 0x00090502 */

#endif
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
			if (!BOARD_MOTION_READY) {
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
			if (!BOARD_MOTION_READY) {
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
			if (!BOARD_MOTION_READY) {
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
			if (!BOARD_MOTION_READY) {
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
			if (!BOARD_MOTION_READY) {
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
#endif

#if FW_SINCE(0x01000104)
/* the buttons, for a module that is not here (unused) */
void controlsGetButtons(unsigned char *out)
{
	controlsReadButtons(out);
}
#endif

#if FW_SINCE(0x0100060B)
/* host command 0x0120: not taken (0x3e) */
unsigned char hostCommand0120(unsigned char *out, unsigned char *length)
{
	return 0x3e;
}
#endif
