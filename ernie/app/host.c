/*
 * The host link: the frames the host (the main processor) exchanges with the syscon over SPI
 * (CSI00, csi.c). Each frame starts on the host's interrupt (INTP5): the
 * first two bytes go both ways, then the host's command (its word, a length, the data and a
 * checksum) and the syscon's answer to the frame before (the link's status, a length, a
 * result, the data and a checksum). A command is handled at once from the command table, or,
 * with bit 7 of its first byte set, given to a request (hostRequests) that a task answers;
 * the host then polls the request for its answer. Task hostTask runs the frames from event
 * flag 1, and pauses the link when the host keeps it busy too long.
 */
#pragma rtos_task hostTask
#pragma rtos_interrupt INTP5 hostInterrupt
#pragma sfr
#pragma NOP
#pragma inline

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "chip.h"
#include "command.h"
#include "config.h"
#include "csi.h"
#include "handshake.h"
#include "host.h"
#include "inputreq.h"
#include "kernel.h"
#include "powerreq.h"
#include "sleepfactor.h"
#include "tau.h"
#include "updatereq.h"

#define MIN(a, b)		((a) < (b) ? (a) : (b))

/* a frame's command word */
#define FRAME_COMMAND(frame)	(*(unsigned int *)(frame))

/* hostLink.state: an odd state waits for the end of a transfer (event flag 1, bit 0x0002) */
#define HOST_IDLE		0x02
#define HOST_START		0x04
#define HOST_HEADER		0x06
#define HOST_DATA		0x08
#define HOST_END		0x0a
#define HOST_ABORT		0x80

/* a command handler: the answer's data to out, its length to *length; returns the result */
typedef unsigned char (*HostCommandHandler)(unsigned char *out, unsigned char *length);

/* a command: its handler (type 0) or its HostStream (type 1) */
struct HostCommand {
	unsigned int	command;
	HostCommandHandler handler;
	unsigned char	type;
};

extern void sub_1706C(void);
extern void sub_170DA(void);
extern unsigned char sub_2D685(void);

/* the commands' handlers and streams (their files not written yet) */
extern unsigned char sub_2D289(unsigned char *out, unsigned char *length);

struct HostLink hostLink;

/* the pause the link takes when the host keeps it busy too long */
static struct {
	unsigned int	frameEnd;	/* TCR02 at the end of the last frame */
	unsigned int	frameStart;	/* TCR02 at the start of this one */
	unsigned int	busyTime;	/* the frames' time since the link was last idle */
	unsigned int	busyLimit;	/* the busy time before a pause */
	unsigned int	idleLimit;	/* the idle time that clears the busy time */
	unsigned int	pause;		/* the pause, in ticks */
	unsigned char	streaming;	/* a stream's answer goes on in the next frame */
} hostThrottle;

unsigned char hostCommandNull(unsigned char *out, unsigned char *length);
unsigned char hostCommandSleepFactor(unsigned char *out, unsigned char *length);
unsigned char hostCommandRequests(unsigned char *out, unsigned char *length);

/* the commands, in order: all of them, and those taken before handshakeUnlocked11 allows the rest */
static const struct HostCommand hostCommands[] = {
	{ 0x0000, hostCommandNull, 0 },
	{ 0x0001, hostCommandErnieVersion, 0 },
	{ 0x0002, hostCommandTimeStamp, 0 },
	{ 0x0003, hostCommandSleepFactor, 0 },
	{ 0x0004, hostCommandRequests, 0 },
	{ 0x0005, hostCommandHardwareInfo, 0 },
	{ 0x0010, hostCommandWakeupFactor, 0 },
	{ 0x0011, hostCommandClock, 0 },
	{ 0x0012, hostCommandAlarm, 0 },
	{ 0x0013, hostCommand0013, 0 },
	{ 0x0014, hostCommandPins, 0 },
	{ 0x0100, hostCommandControlsInfo, 0 },
	{ 0x0101, hostCommandControlsAnalog, 0 },
	{ 0x0102, hostCommand0102, 0 },
	{ 0x0103, hostCommandMultiCnInfo, 0 },
	{ 0x0104, hostCommandControlsAnalogBoth, 0 },
	{ 0x0111, hostCommandAnalog, 0 },
	{ 0x0112, hostCommand0112, 0 },
	{ 0x0113, hostCommandAnalogBoth, 0 },
	{ 0x0300, (HostCommandHandler)&hostStream0300, 1 },
	{ 0x0301, (HostCommandHandler)&hostStream0301, 1 },
	{ 0x0302, (HostCommandHandler)&hostStream0302, 1 },
	{ 0x0400, (HostCommandHandler)&hostStream0400, 1 },
	{ 0x0800, hostCommandPowerStatus, 0 },
	{ 0x0801, hostCommand0801, 0 },
	{ 0x0802, hostCommandBatteryRemainCapOld, 0 },
	{ 0x0803, hostCommandBatteryVoltOld, 0 },
	{ 0x0804, hostCommand0804, 0 },
	{ 0x0900, hostCommandBatteryRemainCap, 0 },
	{ 0x0901, hostCommandBatteryVolt, 0 },
	{ 0x0902, hostCommandBatteryLifePercent, 0 },
	{ 0x1100, hostCommandErnieDlVersion, 0 },
	{ 0x1300, sub_2D289, 0 },
};

static const struct HostCommand hostBaseCommands[] = {
	{ 0x0000, hostCommandNull, 0 },
	{ 0x0001, hostCommandErnieVersion, 0 },
	{ 0x0002, hostCommandTimeStamp, 0 },
	{ 0x0004, hostCommandRequests, 0 },
	{ 0x0005, hostCommandHardwareInfo, 0 },
	{ 0x0010, hostCommandWakeupFactor, 0 },
	{ 0x0013, hostCommand0013, 0 },
	{ 0x1100, hostCommandErnieDlVersion, 0 },
	{ 0x1101, hostCommandUpdaterProgress, 0 },
	{ 0x1300, sub_2D289, 0 },
};

static const char hostRevision[] = "$Rev: 2579 $";

int hostStep(FLGPTN flgptn);
int hostFrameBegin(void);
int hostFrameCommand(void);
unsigned char hostFrameData(void);
int hostFrameFinish(void);
int sub_141DF(void);
int sub_14215(void);
int hostDispatch(unsigned int command, unsigned char *out, unsigned char *length);
struct HostRequest *hostFindRequest(unsigned char id);
unsigned char hostChecksum(unsigned char *data, unsigned char length);
int hostAnswerPending(void);
unsigned char hostPollRequests(void);

unsigned int hostCommandCount(void)
{
	return sizeof(hostCommands) / sizeof(hostCommands[0]);
}

unsigned int hostBaseCommandCount(void)
{
	return sizeof(hostBaseCommands) / sizeof(hostBaseCommands[0]);
}

void hostIntInit(void)
{
	EGP0.5 = 1;
}

void hostIntEnable(void)
{
	PIF5 = 0;
	PMK5 = 0;
}

void hostIntDisable(void)
{
	PMK5 = 1;
}

void hostIntCancel(void)
{
	PMK5 = 1;
	clr_flg(1, ~0x0001);
}

/* the host starts a frame */
void hostInterrupt(void)
{
	iset_flg(1, 0x0001);
}

int hostInit(void)
{
	hostLink.frameSize = 0x20;
	hostLink.config.size = 0;
	hostLink.config.level = 0;
	hostLink.position = 0;
	hostLink.requestActive = 0;
	hostLink.tx = hostLink.txBuffer;
	hostLink.rx = hostLink.rxBuffer;
	hostLink.frameMax = 0x80;
	hostLink.state = HOST_IDLE;
	hostLink.flags.value = 0;
	hostThrottle.frameEnd = 0;
	hostThrottle.frameStart = 0;
	hostThrottle.busyTime = 0;
	hostThrottle.streaming = 0;
	hostThrottle.busyLimit = sub_2C658()->hostBusyLimit;
	hostThrottle.idleLimit = sub_2C658()->hostIdleLimit;
	hostThrottle.pause = sub_2C658()->hostPause;
	tau0PowerOn();
	TMR02 = 0x8000;
	TDR02 = 0xffff;
	TMMK02 = 1;
	TS0L = 0x04;
	return 0;
}

unsigned char hostSetLinkConfig(struct HostLinkConfig config)
{
	unsigned char ret;

	ret = 1;
	if (config.level >= 2) {
		ret = 1;
	} else {
		switch (config.size) {
		case 0:
			hostLink.config = config;
			hostLink.frameSize = 0x20;
			ret = 0;
			break;
		case 1:
			hostLink.config = config;
			hostLink.frameSize = 0x40;
			ret = 0;
			break;
		case 2:
			hostLink.config = config;
			hostLink.frameSize = 0x80;
			ret = 0;
			break;
		default:
			ret = 1;
			break;
		}
	}
	return ret;
}

int hostGetLinkConfig(struct HostLinkConfig *config)
{
	memcpy(config, &hostLink.config, sizeof(hostLink.config));
	return 0;
}

int hostGetLinkSize(unsigned char *size)
{
	*size = hostLink.config.size;
	return 0;
}

int hostGetLinkLevel(unsigned int *level)
{
	*level = hostLink.config.level;
	return 0;
}

/* a step of the frames: flgptn's bits 0x0001 a frame starts, 0x0002 a transfer ended,
   0x0004 the host aborted the frame */
int hostStep(FLGPTN flgptn)
{
	unsigned int idle;

	if (flgptn & 0x0001) {
		if (hostLink.state == HOST_IDLE)
			hostLink.state = HOST_START;
		clr_flg(1, ~0x0001);
		hostThrottle.frameStart = TCR02;
		idle = hostThrottle.frameEnd - hostThrottle.frameStart;
		if (hostThrottle.idleLimit < idle)
			hostThrottle.busyTime = 0;
		if (hostThrottle.busyLimit < hostThrottle.busyTime) {
			if (hostThrottle.streaming == 0) {
				dly_tsk(hostThrottle.pause);
				hostThrottle.frameStart = TCR02;
				hostThrottle.busyTime = 0;
			}
		}
	} else if (flgptn & 0x0002) {
		if (hostLink.state & 1)
			hostLink.state++;
		clr_flg(1, ~0x0002);
	} else if (flgptn & 0x0004) {
		if (hostLink.state & 1)
			hostLink.state = HOST_ABORT;
		clr_flg(1, ~0x0004);
	}
	switch (hostLink.state) {
	case 0:
	case HOST_IDLE:
		break;
	case HOST_START:
		hostFrameBegin();
		hostLink.state = HOST_HEADER;
	case HOST_HEADER:
		hostFrameCommand();
		hostLink.state++;
	case HOST_DATA:
		if (hostFrameData())
			hostLink.state++;
	case HOST_END:
		hostFrameFinish();
		hostLink.state = HOST_IDLE;
		break;
	case 0x18:
		sub_141DF();
		hostLink.state++;
		break;
	case 0x1a:
		sub_14215();
		hostLink.state = HOST_IDLE;
		break;
	case HOST_ABORT:
	default:
		csiCancel();
		hostFrameFinish();
		hostLink.state = HOST_IDLE;
		break;
	}
	return 0;
}

void hostRequestSetState(struct HostRequest *request, unsigned char state)
{
	if (request != NULL) {
		switch (state) {
		case HOST_REQUEST_IDLE:
		case HOST_REQUEST_RECEIVE:
		case HOST_REQUEST_BUSY:
			request->state = state;
			break;
		case HOST_REQUEST_ERROR:
			request->answerLength = 0;
			request->status = 0x39;
			request->state = state;
			break;
		default:
			request->state = 0x80;
			break;
		}
	}
}

void sub_138DB(struct HostRequest *request, unsigned char state)
{
	if (request != NULL) {
		switch (state) {
		case HOST_REQUEST_IDLE:
		case HOST_REQUEST_RECEIVE:
		case 3:
			request->state = state;
			break;
		default:
			request->state = 0x80;
			break;
		}
	}
}

/* the answer to a request: HOST_REQUEST_ANSWER with its status and length,
   HOST_REQUEST_ERROR, or HOST_REQUEST_UNKNOWN */
void hostRequestAnswer(struct HostRequest *request, unsigned char state,
		       unsigned char status, unsigned char length)
{
	if (request != NULL) {
		switch (state) {
		case HOST_REQUEST_ANSWER:
			request->answerLength = length;
			request->status = status;
			break;
		case HOST_REQUEST_UNKNOWN:
			request->answerLength = 0;
			request->status = 0x3f;
			state = HOST_REQUEST_ANSWER;
			break;
		case HOST_REQUEST_ERROR:
			request->answerLength = 0;
			request->status = 0x39;
			break;
		default:
			request->answerLength = 0;
			state = HOST_REQUEST_ERROR;
			break;
		}
		request->state = state;
		hostLink.flags.bits.answer = 1;
	}
}

/* a request's answer is ready (state: the request's state) */
void hostRequestAnswerReady(unsigned char *state)
{
	*state = HOST_REQUEST_ANSWER;
	hostLink.flags.bits.answer = 1;
}

/* the frame's first two bytes: the link's status and flags out, the command's word in */
int hostFrameBegin(void)
{
	hostLink.rx[0] = sleepFactorSnapshot() | hostLink.flags.value;
	hostLink.flags.bits.overflow = 0;
	hostLink.flags.bits.checksum = 0;
	P5.6 = 1;
	P90 = 0;
	P5.7 = 0;
	csiTransferWait(hostLink.tx, 2, hostLink.rx, 2);
	hostLink.position = 2;
	return 0;
}

/* the command's word is in: the answer (to a command, at once; to a poll, the request's) */
int hostFrameCommand(void)
{
	char result;
	unsigned char length;
	struct HostRequest *request;

	length = 0;
	if ((FRAME_COMMAND(hostLink.tx) & 0x80) ? 0 : 1) {
		result = hostDispatch(FRAME_COMMAND(hostLink.tx), hostLink.rx + 4, &length);
		if (result >= 0)
			hostLink.rx[3] = result;
		else
			hostLink.rx[3] = 0x3f;
		hostLink.rx[2] = length + 2;
		sub_2D685();
	} else {
		request = hostFindRequest(hostLink.tx[1]);
		if (request != NULL) {
			hostLink.requestActive = 1;
			hostLink.request = request;
			switch (request->state) {
			case HOST_REQUEST_ANSWER:
				if (request->command == FRAME_COMMAND(hostLink.tx)) {
					length = MIN(hostLink.frameSize - 5,
						     request->answerLength - request->offset);
					memcpy(hostLink.rx + 4, request->answer + request->offset, length);
					request->offset += length;
					if (request->answerLength == request->offset)
						hostLink.rx[3] = request->status & 0x3f;
					else
						hostLink.rx[3] = request->status & 0x3f | 0x40;
				} else {
					hostLink.requestMismatch = 1;
					hostLink.rx[3] = 0x82;
				}
				hostLink.rx[2] = length + 2;
				sub_2D685();
				break;
			case HOST_REQUEST_IDLE:
				hostLink.rx[3] = 0x3e;
				hostLink.rx[2] = 2;
				break;
			case HOST_REQUEST_RECEIVE:
				hostLink.rx[3] = 0x80;
				hostLink.rx[2] = 2;
				sub_2D685();
				break;
			case HOST_REQUEST_BUSY:
				if (request->command == FRAME_COMMAND(hostLink.tx)) {
					hostLink.rx[3] = 0x81;
				} else {
					hostLink.requestMismatch = 1;
					hostLink.rx[3] = 0x82;
				}
				hostLink.rx[2] = 2;
				sub_2D685();
				break;
			case HOST_REQUEST_ERROR:
				hostLink.rx[3] = 0x83;
				hostLink.rx[2] = 2;
				break;
			default:
				hostLink.rx[3] = 0x84;
				hostLink.rx[2] = 2;
				break;
			}
		} else {
			hostLink.rx[3] = 0x3f;
			hostLink.flags.bits.overflow = 1;
			hostLink.rx[2] = 2;
		}
	}
	length += 2;
	csiTransferDmaWait(hostLink.tx + hostLink.position, hostLink.rx + hostLink.position, length);
	hostLink.rx[hostLink.rx[2] + 2] = hostChecksum(hostLink.rx, hostLink.rx[2] + 2);
	hostLink.position += length;
	while (DST0)
		NOP();
	csiRelease();
	sub_1706C();
	sub_170DA();
	length = 1;
	if ((hostLink.rx[2] & 1) == 0)
		length++;
	csiTransferWait(hostLink.tx + hostLink.position, length, hostLink.rx + hostLink.position, length);
	hostLink.position += length;
	return 0;
}

/* the rest of the command; 0 when the answer was the longer */
unsigned char hostFrameData(void)
{
	unsigned char length;

	if (hostLink.tx[2] <= hostLink.rx[2])
		return 0;
	length = hostLink.tx[2] + 3;
	if (hostLink.frameSize < length) {
		hostLink.flags.bits.overflow = 1;
		length = hostLink.frameSize;
	}
	length -= hostLink.position;
	if ((hostLink.tx[2] & 1) == 0)
		length++;
	csiTransferWait(hostLink.tx + hostLink.position, length, NULL, 0);
	hostLink.position += length;
	return 1;
}

/* the frame is over: the command checked and given to the request it polled */
int hostFrameFinish(void)
{
	unsigned char sum;
	unsigned char notify;
	unsigned char length;
	struct HostRequest *request;

	notify = 0;
	P90 = 1;
	P5.7 = 1;
	sum = hostChecksum(hostLink.tx, hostLink.tx[2] + 2);
	if (hostLink.tx[hostLink.tx[2] + 2] != sum)
		hostLink.flags.bits.checksum = 1;
	if (hostLink.requestActive == 1) {
		request = hostLink.request;
		if (hostLink.requestMismatch == 1) {
			/* the command was not the request's */
		} else if (!hostLink.flags.bits.checksum) {
			switch (request->state) {
			case HOST_REQUEST_RECEIVE:
				length = MIN(request->size, hostLink.tx[2] - 1);
				request->command = FRAME_COMMAND(hostLink.tx);
				request->length = length;
				request->offset = 0;
				memcpy(request->data, hostLink.tx + 3, length);
				request->event = HOST_EVENT_COMMAND;
				notify = 1;
				break;
			case HOST_REQUEST_ANSWER:
				if (request->answerLength == request->offset) {
					request->event = HOST_EVENT_ANSWERED;
					request->offset = 0;
					notify = 1;
				}
				break;
			case HOST_REQUEST_BUSY:
				break;
			case HOST_REQUEST_IDLE:
				break;
			case HOST_REQUEST_ERROR:
				request->event = HOST_EVENT_ANSWERED;
				request->offset = 0;
				notify = 1;
				break;
			}
			if (notify && request->notify != NULL) {
				request->notify();
				if (hostAnswerPending() == 0)
					hostLink.flags.bits.answer = 0;
			}
		}
	}
	hostLink.position = 0;
	hostLink.requestActive = 0;
	hostLink.requestMismatch = 0;
	hostLink.frameCount++;
	hostThrottle.frameEnd = TCR02;
	hostThrottle.busyTime += hostThrottle.frameStart - hostThrottle.frameEnd;
	return 0;
}

int sub_141DF(void)
{
	struct HostRequest *request;

	request = hostLink.request;
	if (request != NULL)
		csiTransfer(NULL, 0, (unsigned char *)&request->command + 1, 1);
	return 0;
}

int sub_14215(void)
{
	P90 = 1;
	P5.7 = 1;
	hostThrottle.frameEnd = TCR02;
	hostThrottle.busyTime += hostThrottle.frameStart - hostThrottle.frameEnd;
	return 0;
}

/* a command from the table; returns the result, -1 if there is no such command */
int hostDispatch(unsigned int command, unsigned char *out, unsigned char *length)
{
	unsigned int low;
	unsigned int middle;
	unsigned int high;
	const struct HostCommand *entry;
	const struct HostCommand *table;
	unsigned char n;
	struct HostStream *stream;

	low = 0;
	if (handshakeUnlocked11()) {
		high = hostCommandCount();
		table = hostCommands;
	} else {
		high = hostBaseCommandCount();
		table = hostBaseCommands;
	}
	while (low < high) {
		middle = (low + high) / 2;
		entry = &table[middle];
		if (entry->command == command) {
			if (entry->type == 0)
				return entry->handler(out, length) & 0x3f;
			stream = (struct HostStream *)entry->handler;
			if (stream->offset == 0) {
				switch (stream->state) {
				case 1:
					return 0x3a;
				default:
					return 0x3e;
				case 3:
					stream->block = stream->first;
					stream->prepare();
					break;
				}
			}
			n = MIN(hostLink.frameSize - 5, stream->length - stream->offset);
			memcpy(hostLink.rx + 4, stream->blocks[stream->block] + stream->offset, n);
			*length = n;
			stream->offset += n;
			if (stream->length == stream->offset) {
				stream->block = 0xff;
				stream->offset = 0;
				hostThrottle.streaming = 0;
				return 0;
			}
			hostThrottle.streaming = 1;
			return 0x40;
		}
		if (command < entry->command)
			high = middle;
		else if (entry->command < command)
			low = middle + 1;
	}
	if (!handshakeUnlocked11())
		handshakeRefuse();
	return -1;
}

struct HostRequest *hostFindRequest(unsigned char id)
{
	unsigned int i;
	unsigned int count;
	struct HostRequest * const *p;

	p = hostRequests;
	count = hostRequestCount();
	for (i = 0; i < count; i++) {
		if ((*p)->id == id)
			return *p;
		p++;
	}
	return NULL;
}

unsigned char hostChecksum(unsigned char *data, unsigned char length)
{
	unsigned char sum;

	sum = 0;
	while (length--)
		sum += *data++;
	return ~sum;
}

/* 1 if a request's answer is ready */
int hostAnswerPending(void)
{
	unsigned int i;
	unsigned int count;
	struct HostRequest * const *p;

	p = hostRequests;
	count = hostRequestCount();
	for (i = 0; i < count; i++) {
		if ((*p)->state == HOST_REQUEST_ANSWER)
			return 1;
		p++;
	}
	return 0;
}

/* command 0x0000: no data (the host's poll for the link's status) */
unsigned char hostCommandNull(unsigned char *out, unsigned char *length)
{
	out = out;		/* not used */
	*length = 0;
	return 0;
}

/* command 0x0003: the sleep factor */
unsigned char hostCommandSleepFactor(unsigned char *out, unsigned char *length)
{
	unsigned long factor;

	factor = sleepFactorGet();
	sleepFactorAcknowledge();
	memcpy(out, &factor, sizeof(factor));
	*length = 4;
	return 0;
}

/* command 0x0004: the sleep factor, then the command of every request taking or
   handling one (bit 15: handling) */
unsigned char hostCommandRequests(unsigned char *out, unsigned char *length)
{
	unsigned int i;
	unsigned int count;
	struct HostRequest *request;
	unsigned long factor;

	factor = sleepFactorGet();
	sleepFactorAcknowledge();
	memcpy(out, &factor, sizeof(factor));
	out += 4;
	*length = 4;
	count = hostRequestCount();
	for (i = 0; i < count; i++) {
		request = hostRequests[i];
		if (request->state & 0x06) {
			*(unsigned int *)out = request->command |
					       (request->state == HOST_REQUEST_BUSY ? 0x8000 : 0);
			out += 2;
			*length += 2;
		}
	}
	return 0;
}

/* the requests' answers taken while the link was paused; 1 if a request is busy */
unsigned char hostPollRequests(void)
{
	unsigned char busy;
	unsigned char notify;
	unsigned int i;
	unsigned int count;
	struct HostRequest * const *p;

	busy = 0;
	notify = 0;
	p = hostRequests;
	count = hostRequestCount();
	for (i = 0; i < count; i++) {
		switch ((*p)->state) {
		case HOST_REQUEST_RECEIVE:
			break;
		case HOST_REQUEST_ANSWER:
			(*p)->event = HOST_EVENT_ANSWERED;
			(*p)->offset = 0;
			notify = 1;
			break;
		case HOST_REQUEST_BUSY:
			busy = 1;
			break;
		case HOST_REQUEST_IDLE:
			break;
		case HOST_REQUEST_ERROR:
			(*p)->event = HOST_EVENT_ANSWERED;
			(*p)->offset = 0;
			notify = 1;
			break;
		}
		if (notify && (*p)->notify != NULL) {
			(*p)->notify();
			if (hostAnswerPending() == 0)
				hostLink.flags.bits.answer = 0;
		}
		p++;
	}
	return busy;
}

/* the link's task: event flag 1's bits 0x0007 run the frames (hostStep), 0x0080 pauses the
   link, 0x0100 polls the requests, 0x0040 resumes the link */
void hostTask(VP_INT exinf)
{
	FLGPTN flgptn;
	unsigned char paused;

	paused = 0;
	for (;;) {
		wai_flg(1, 0xffff, TWF_ORW, &flgptn);
		if (flgptn & 0x0080) {
			clr_flg(1, ~0x0080);
			paused = 1;
			hostIntDisable();
			csiClose();
			csiOpenMsb();
			P5.6 = 0;
		} else if (flgptn & 0x0100) {
			clr_flg(1, ~0x0100);
			dly_tsk(200);
			if (hostPollRequests())
				;
		} else if (flgptn & 0x0040) {
			clr_flg(1, ~0x0040);
			paused = 0;
			csiClose();
			csiOpenLsb();
			if (hostPollRequests())
				;
			hostIntEnable();
			P5.6 = 1;
		}
		switch (paused) {
		case 0:
			if (flgptn & 0x0007)
				hostStep(flgptn);
			break;
		case 1:
			if (flgptn & 0x0002)
				clr_flg(1, ~0x0002);
			if (flgptn & 0x0020)
				clr_flg(1, ~0x0020);
			break;
		}
	}
}
