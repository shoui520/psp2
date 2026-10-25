/*
 * The 3G modem (the hardware info's flag 0x02), on UART2: its power and wake inputs, the
 * packets it sends (each answered at once), the registers it and the host read and write, and
 * the host's request 0x0a (commands 0x0a80-0x0a82). P3.2 low at the start says the modem is
 * there; P3.4 and P3.6 are its lines while it is on, P3.7 the host's line to it, P12.5 its
 * power for mode 1.
 */
#pragma sfr

#include <stddef.h>
#include <string.h>

#include "ernie.h"
#include "bbmc.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "host.h"
#include "input.h"
#include "kernel.h"
#include "nvsreq.h"
#include "powerwait.h"
#include "sleepfactor.h"
#include "task5.h"
#include "timer.h"
#include "uart.h"
#include "version.h"

#define BBMC_PORT		2	/* the UART */
#define BBMC_START		0x5a	/* a packet's first byte */
#define BBMC_ANSWER_START	0xa5	/* an answer's */
#define BBMC_NVS_MODE		0x2e	/* bbmcRegs.r.mode in NVS area 1 */

/* bbmcNvsDone's commands */
#define BBMC_NVS_LOAD		1	/* the mode was read at the start */
#define BBMC_NVS_SAVE		2	/* the host wrote the mode */
#define BBMC_NVS_WRITTEN	3

/* what a register lets a mode do (bbmcRegModes) */
#define BBMC_REG_WRITE		0
#define BBMC_REG_OR		1
#define BBMC_REG_READ_ONLY	3

extern void sub_2C394(int, int);
extern unsigned int sub_2AB46(void);
extern unsigned char sub_2AB49(int transition);
extern unsigned char sub_2BB99(void);
extern unsigned char sub_2BBE9(void);
extern unsigned char sub_11D57(void);
extern unsigned char sub_11D61(void);
extern void sub_123DB(int);

struct Bbmc bbmc;			/* the link */
static unsigned int bbmcSavedMode;	/* the mode as NVS holds it */
union BbmcRegs bbmcRegs;
struct HostRequest bbmcRequest;
static unsigned char bbmcRequestAnswer[0x20];
static unsigned char bbmcRequestData[0x20];
struct InputHandler bbmcInputHandlerPower;
struct InputHandler bbmcInputHandlerWake;

/* a register's 2-bit right for each mode, at the shift bbmcModeShift gives */
static const unsigned char bbmcModeShift[5] = { 4, 0, 2, 6, 6 };
static const unsigned char bbmcRegModes[BBMC_REGS + 1] = {
	0x33, 0x03, 0x33, 0x30, 0x03, 0x03, 0x3c, 0x3c,
	0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31, 0x31,
	0x30, 0x30, 0x30, 0x30, 0x3c, 0x00, 0x00, 0x00,
	0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c, 0x3c,
	0x3c, 0x3c, 0x3c, 0x3c, 0x0f,
};
static const unsigned char bbmcZero12[12] = { 0 };
static const unsigned char bbmcZero16[16] = { 0 };
static const unsigned char bbmcZero2[2] = { 0 };
static const unsigned char bbmcZero4[4] = { 0 };

/* the link's timeouts and handlers, the registers, request 0x0a and the inputs; the hardware
   info's flag 0x02 when P3.2 stays low */
void bbmcInit(void)
{
	struct UartTimeout timeout;
	struct TimerHandler handler;
	unsigned char result;
	unsigned char level;

	P33 = 1;
	dly_tsk(0x39d);
	timeout.timer = 0;
	timeout.timeout = 1;
	timeout.flgid = 6;
	timeout.pattern = BBMC_EVENT_RECEIVE;
	if (uartSetTimeout(BBMC_PORT, &timeout))
		;
	handler.handler = bbmcPostRxTimeout;
	result = timerSetHandler(2, &handler);
	handler.handler = bbmcPostLinkTimeout;
	result = timerSetHandler(0x13, &handler);
	handler.handler = bbmcPostStart;
	result = timerSetHandler(0x15, &handler);
	handler.handler = bbmcPostMode;
	result = timerSetHandler(0x19, &handler);
	bbmc.lastSequence = 0;
	bbmc.repeats = 0;
	bbmc.rxLength = 0;
	bbmc.pending = 0;
	bbmc.transition = 0;
	bbmc.state = 1;
	bbmcRegsInit();
	bbmcRequest.id = 0x0a;
	bbmcRequest.data = bbmcRequestData;
	bbmcRequest.answer = bbmcRequestAnswer;
	bbmcRequest.size = sizeof(bbmcRequestData);
	bbmcRequest.notify = bbmcRequestNotify;
	bbmcRequest.state = HOST_REQUEST_RECEIVE;
	bbmcInputHandlerPower.type = 1;
	bbmcInputHandlerPower.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	bbmcInputHandlerPower.handler = bbmcPostPower;
	bbmcInputHandlerWake.type = 1;
	bbmcInputHandlerWake.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	bbmcInputHandlerWake.handler = bbmcPostWake;
	P33 = 0;
	if (sub_2C658()->bbmcDetect) {
		level = P3.2;
		dly_tsk(1);
		if (P3.2 == level && level == 0)
			setHardwareInfoFlags(0x02);
	}
}

unsigned char bbmcPowerOn(void)
{
	uart2Open();
	switch (bbmc.state) {
	case 3:
		break;
	case 4:
		sub_2C394(6, BBMC_EVENT_MODE);
		break;
	default:
		timerStart(0x13, 5000);
		timerStart(0x15, 0xa0);
		break;
	}
	bbmc.repeats = 0;
	bbmc.lastSequence = 0xff;
	return 0;
}

/* the lines low, the link closed, the registers of the mode and the events cleared, and the
   status for the system state */
unsigned char bbmcPowerOff(void)
{
	P34 = 0;
	PM34 = 0;
	P36 = 0;
	PM36 = 0;
	uart2Close(0);
	bbmc.state = 1;
	bbmc.transition = 0;
	timerStop(0x13);
	timerStop(0x15);
	bbmcWriteRegs(3, 1, bbmcZero12, BBMC_WRITE_RESET);
	bbmcWriteRegs(6, 0x10, bbmcZero16, BBMC_WRITE_RESET);
	bbmcWriteRegs(0x18, 0x0c, bbmcZero12, BBMC_WRITE_RESET);
	switch (sub_2AB46()) {
	case 0x0004:
	case 0x8004:
		bbmcSetStatus(0);
		break;
	case 0x0008:
	case 0x8008:
		if (chargeBatteryLevel9())
			bbmcSetStatus(0xa0);
		else
			bbmcSetStatus(0x22);
		break;
	case 0x0020:
	case 0x8020:
	default:
		bbmcSetStatus(0x22);
		break;
	}
	return 0;
}

/* the lines as inputs, and the state for the mode set */
unsigned char bbmcStart(void)
{
	PM34 = 1;
	PM36 = 1;
	bbmcWriteRegs(6, 2, bbmcZero2, BBMC_WRITE_SET);
	bbmcWriteRegs(8, 4, bbmcZero4, BBMC_WRITE_RESET);
	switch (bbmcRegs.r.mode) {
	case 1:
		P125 = 1;
		bbmc.state = 3;
		powerSleep(100);
		timerStart(0x19, 0x14);
		break;
	case 6:
	case 7:
		bbmc.state = 4;
		break;
	default:
		bbmc.state = 2;
		break;
	}
	return 0;
}

unsigned char bbmcSetOff(void)
{
	bbmc.state = 1;
	return 0;
}

void bbmcStop(void)
{
	bbmcSetStatus(2);
	bbmcSetModeActive(0x0200);
	bbmc.state = 1;
}

void bbmcWake(void)
{
	bbmcSetModeActive(0x0200);
	if (sub_11D57())
		bbmc.state = 2;
}

void bbmcSignal4(void)
{
	bbmcSetEvents(4);
}

void bbmcSignal8(void)
{
	bbmcSetEvents(8);
}

/* the mode in effect as 1-4 (modes 1, 2, 6 and 7), else 0 */
unsigned char bbmcGetMode(void)
{
	switch (bbmcRegs.r.modeActive) {
	case 1:
		return 1;
	case 2:
		return 2;
	case 6:
		return 3;
	case 7:
		return 4;
	}
	return 0;
}

unsigned char bbmcIsOn(void)
{
	switch (bbmc.state) {
	case 2:
	case 3:
	case 4:
		return 1;
	}
	return 0;
}

unsigned char bbmcModeIsNone(void)
{
	return bbmcRegs.r.modeActive == 0;
}

/* event flag 6's events */
unsigned char bbmcEvent(FLGPTN ptn)
{
	if (ptn & BBMC_EVENT_POWER) {
		unsigned char level;

		level = sub_11D57();
		if (level != 0) {
			if (bbmc.state == 1 || sub_2AB46() == 0x0004)
				sub_21F33(0x0200);
			else
				bbmcPowerOn();
		} else {
			if (bbmc.state == 1)
				;
			bbmcPowerOff();
		}
	}
	if (ptn & BBMC_EVENT_MODE) {
		unsigned int mode;

		mode = bbmcRegs.r.mode;
		P125 = 0;
		if (mode == 7)
			mode = 1;
		bbmcWriteRegs(6, 2, (unsigned char *)&mode, BBMC_WRITE_RESET);
		bbmcSetStatus(0x41);
	}
	if (ptn & BBMC_EVENT_WAKE) {
		unsigned char level;

		level = sub_11D61();
		if (level == 0 && bbmc.state != 1)
			bbmcSetStatus(4);
	}
	if (ptn & BBMC_EVENT_RECEIVE)
		bbmcReceive();
	if (ptn & BBMC_EVENT_RX_TIMEOUT)
		bbmc.rxLength = 0;
	if (ptn & BBMC_EVENT_LINK_TIMEOUT) {
		if (sub_2AB46() == 0x0008) {
			uart2Close(1);
			sub_123DB(1);
		}
	}
	if (ptn & BBMC_EVENT_START)
		;
	if (ptn & BBMC_EVENT_RESUME) {
		sub_123DB(0);
		uart2Open();
		timerStart(0x13, 5000);
	}
	if (ptn & BBMC_EVENT_STATE) {
		unsigned int state;

		state = sub_2AB46();
		switch (state) {
		case 0x0020:
			break;
		case 0x8020:
			if (bbmcIsOn())
				uart2Open();
			bbmc.transition = 0;
			break;
		case 0x0008:
			if (bbmcRegs.r.events != 0) {
				sub_2AB49(8);
				bbmc.transition = 1;
			}
			break;
		case 0x8008:
			if (bbmcIsOn()) {
				uart2Close(1);
				sub_123DB(1);
				if (P7.4)
					sub_2C394(6, BBMC_EVENT_RESUME);
				bbmc.transition = 0;
			}
			break;
		case 0x0004:
			break;
		case 0x8004:
			bbmcPowerOff();
			break;
		}
	}
	if (ptn & BBMC_EVENT_REQUEST)
		bbmcRequestRun();
	return 0;
}

/* a packet in or out (prints nothing) */
void bbmcTrace(const char *name, unsigned char *packet)
{
	int i;
	int length;

	length = ((struct BbmcPacket *)packet)->length + 2;
	for (i = 0; i < length; i++)
		;
}

/* takes in the bytes received; a packet complete and whole is answered */
void bbmcReceive(void)
{
	unsigned char count;
	unsigned char read;
	unsigned char status;

	count = uartReceived(BBMC_PORT) < 0x20 - bbmc.rxLength ? uartReceived(BBMC_PORT) :
								 0x20 - bbmc.rxLength;
	while (count != 0) {
		if (bbmc.rxLength == 0) {
			count -= uartRead(BBMC_PORT, &bbmc.rx.start, 1);
			if (bbmc.rx.start == BBMC_START) {
				bbmc.rxLength = 1;
				timerStart(2, 0x25);
			}
		} else {
			read = uartRead(BBMC_PORT, (unsigned char *)&bbmc.rx + bbmc.rxLength, count);
			bbmc.rxLength += read;
			count -= read;
			if (read != 0 && (bbmc.rx.length < 3 || bbmc.rx.length >= 0x1f)) {
				bbmc.rxLength = 0;
				timerStop(2);
			}
			if (count != 0) {
				uartFlush(BBMC_PORT);
				count = 0;
			}
		}
		if (bbmc.rxLength >= 5 && bbmc.rx.length + 2 == bbmc.rxLength) {
			timerStop(2);
			timerStart(0x13, 5000);
			bbmcTrace("RX", (unsigned char *)&bbmc.rx);
			if (bbmc.rx.data[bbmc.rx.length - 3] == bbmcChecksum((unsigned char *)&bbmc.rx)) {
				if (bbmc.rx.sequence == bbmc.lastSequence) {
					if (++bbmc.repeats >= 4) {
						bbmcAddStatus(0x0100);
						bbmc.repeats = 0;
					}
				} else {
					bbmc.lastSequence = bbmc.rx.sequence;
					bbmc.repeats = 0;
				}
				bbmc.tx.start = BBMC_ANSWER_START;
				bbmc.tx.command = bbmc.rx.command;
				bbmc.tx.sequence = bbmc.rx.sequence;
				bbmc.tx.length = 4;
				switch (bbmc.rx.command) {
				case 0x00:
					status = bbmcCommandVersion(&bbmc.rx, &bbmc.tx);
					break;
				case 0x01:
					status = bbmcCommandHardwareInfo(&bbmc.rx, &bbmc.tx);
					break;
				case 0x02:
					status = bbmcCommandNone(&bbmc.rx, &bbmc.tx);
					break;
				case 0x10:
					status = bbmcCommandWrite(&bbmc.rx, &bbmc.tx);
					break;
				case 0x11:
					status = bbmcCommandRead(&bbmc.rx, &bbmc.tx);
					break;
				case 0x20:
					status = bbmcCommandEcho20(&bbmc.rx, &bbmc.tx);
					break;
				case 0x21:
					status = bbmcCommandEcho21(&bbmc.rx, &bbmc.tx);
					break;
				default:
					status = 0xfe;
					break;
				}
				bbmc.tx.status = status;
				bbmc.tx.data[bbmc.tx.length - 4] = bbmcChecksum((unsigned char *)&bbmc.tx);
				uartWrite(BBMC_PORT, (unsigned char *)&bbmc.tx, bbmc.tx.length + 2);
				bbmcTrace("TX", (unsigned char *)&bbmc.tx);
			}
			bbmc.rxLength = 0;
			bbmc.lastSequence = bbmc.rx.sequence;
			memset(&bbmc.rx, 0, 5);
		}
	}
}

/* the XOR of a packet's bytes before its checksum */
unsigned char bbmcChecksum(unsigned char *packet)
{
	unsigned char sum;
	unsigned char count;

	sum = 0;
	count = ((struct BbmcPacket *)packet)->length + 2 - 1;
	while (count--) {
		sum ^= *packet;
		packet++;
	}
	return sum;
}

unsigned char bbmcCommandVersion(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	getErnieVersion(&answer->data[0]);
	answer->length += 4;
	return 0;
}

unsigned char bbmcCommandHardwareInfo(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	getHardwareInfo(&answer->data[0]);
	answer->length += 4;
	return 0;
}

unsigned char bbmcCommandNone(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	return 0;
}

/* data[0] the first register, data[1] the count, then the values */
unsigned char bbmcCommandWrite(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	unsigned char result;

	if (packet->length < 6)
		return 0xfd;
	result = bbmcWriteRegs(packet->data[0], packet->data[1], &packet->data[2],
			       BBMC_WRITE_MODEM);
	return result != 0 ? 0 : 0xfd;
}

/* data[0] the first register, data[1] the count; the status and the events read stay */
unsigned char bbmcCommandRead(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	unsigned char result;

	if (packet->length < 5)
		return 0xfd;
	result = bbmcReadRegs(packet->data[0], packet->data[1], &answer->data[0], 1);
	answer->length += result;
	return result != 0 ? 0 : 0xfd;
}

unsigned char bbmcCommandEcho20(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	answer->length++;
	answer->data[0] = packet->data[0];
	return 0;
}

unsigned char bbmcCommandEcho21(struct BbmcPacket *packet, struct BbmcAnswer *answer)
{
	answer->length++;
	answer->data[0] = packet->data[0];
	return 0;
}

/* the mode's reads and writes in NVS: 0xffff read becomes 0, and a mode the host set is
   saved */
void bbmcNvsDone(unsigned char command, unsigned char result)
{
	unsigned char ret;

	switch (command) {
	case BBMC_NVS_LOAD:
		if (result != 0)
			;
		if (bbmcRegs.r.mode == 0xffff) {
			bbmcRegs.r.mode = 0;
			ret = nvsJobWrite(BBMC_NVS_MODE, 2, (unsigned char *)&bbmcRegs.r.mode, 1,
					  BBMC_NVS_WRITTEN, bbmcNvsDone);
			if (ret != 0)
				;
		}
		break;
	case BBMC_NVS_SAVE:
		if (result != 0)
			;
		if (bbmcSavedMode != bbmcRegs.r.mode) {
			bbmcSavedMode = bbmcRegs.r.mode;
			ret = nvsJobWrite(BBMC_NVS_MODE, 2, (unsigned char *)&bbmcSavedMode, 1,
					  BBMC_NVS_WRITTEN, bbmcNvsDone);
			if (ret != 0)
				;
		}
		break;
	case BBMC_NVS_WRITTEN:
		if (result != 0)
			;
		break;
	}
}

void bbmcRegsInit(void)
{
	unsigned char result;

	memset(&bbmcRegs, 0, BBMC_REGS);
	bbmcRegs.r.id = 5;
	bbmcRegs.r.reserved1 = 0;
	bbmcRegs.r.size = BBMC_REGS;
	result = nvsJobRead(BBMC_NVS_MODE, 2, (unsigned char *)&bbmcRegs.r.mode, 1, BBMC_NVS_LOAD,
			    bbmcNvsDone);
	if (result != 0)
		;
}

/* writes count registers from reg as mode may; the count, or 0 if reg is past them */
unsigned char bbmcWriteRegs(unsigned char reg, unsigned char count, unsigned char *values,
			    unsigned char mode)
{
	unsigned char result;
	unsigned char changed;

	changed = 0;
	if (reg + count >= BBMC_REGS + 1) {
		if (reg >= BBMC_REGS + 1)
			return 0;
		count = BBMC_REGS - reg;
	}
	result = count;
	while (count--) {
		switch (bbmcRegModes[reg] >> bbmcModeShift[mode] & 3) {
		case BBMC_REG_WRITE:
			bbmcRegs.byte[reg] = *values;
			break;
		case BBMC_REG_OR:
			bbmcRegs.byte[reg] |= *values;
			break;
		case BBMC_REG_READ_ONLY:
			break;
		}
		if (reg >= 0x18 && reg < 0x1c && mode == BBMC_WRITE_MODEM)
			bbmcRegs.r.status |= 0x20;
		if (reg >= 4 && reg < 6 && mode == BBMC_WRITE_HOST)
			changed = 1;
		values++;
		reg++;
	}
	bbmc.pending = (bbmcRegs.r.status != 0 || bbmcRegs.r.events != 0) ? (unsigned char)1 :
									     (unsigned char)0;
	sleepFactorSet8(bbmc.pending);
	if (bbmcRegs.r.events & 8)
		sleepFactorSet20(1);
	if (bbmcRegs.r.events != 0 && !bbmc.transition && sub_2AB46() == 0x0008) {
		bbmc.transition = 1;
		sub_2AB49(8);
	}
	if (changed) {
		result = nvsJobRead(BBMC_NVS_MODE, 2, (unsigned char *)&bbmcSavedMode, 1,
				    BBMC_NVS_SAVE, bbmcNvsDone);
		if (result != 0)
			;
	}
	return result;
}

/* reads count registers from reg as the host (keep 0: the status and the events read are
   cleared) or the modem (keep 1) */
unsigned char bbmcReadRegs(unsigned char reg, unsigned char count, unsigned char *values,
			   unsigned char keep)
{
	unsigned char result;

	if (reg + count >= BBMC_REGS + 1) {
		if (reg >= BBMC_REGS + 1)
			return 0;
		count = BBMC_REGS - reg;
	}
	result = count;
	while (count--) {
		if (reg >= 8 && reg < 0x14) {
			*values = bbmcRegs.byte[reg];
			if (!keep)
				bbmcRegs.byte[reg] = 0;
		} else {
			*values = bbmcRegs.byte[reg];
		}
		values++;
		reg++;
	}
	bbmc.pending = (bbmcRegs.r.status != 0 || bbmcRegs.r.events != 0) ? (unsigned char)1 :
									     (unsigned char)0;
	sleepFactorSet8(bbmc.pending);
	return result;
}

void bbmcAddStatus(unsigned long bits)
{
	bits |= bbmcRegs.r.status;
	bbmcWriteRegs(8, 4, (unsigned char *)&bits, BBMC_WRITE_SET);
}

void bbmcSetStatus(unsigned long bits)
{
	bbmcWriteRegs(8, 4, (unsigned char *)&bits, BBMC_WRITE_SET);
}

void bbmcSetEvents(unsigned long bits)
{
	bbmcWriteRegs(0x0c, 4, (unsigned char *)&bits, BBMC_WRITE_SET);
}

void bbmcSetModeActive(unsigned int mode)
{
	mode |= 0x8000;
	bbmcWriteRegs(6, 2, (unsigned char *)&mode, BBMC_WRITE_SET);
}

unsigned char bbmcGetPending(void)
{
	return bbmc.pending;
}

/* 0 without a version, 1 for versions 0-2, else 2 */
unsigned char bbmcGetVersion(void)
{
	if ((bbmcRegs.r.version & 0xf0) == 0)
		return 0;
	if ((bbmcRegs.r.version & 0x0f) < 3)
		return 1;
	return 2;
}

void bbmcDelay(void)
{
	dly_tsk(0x19);
	dly_tsk(0x19);
	dly_tsk(0x19);
}

void bbmcRequestNotify(void)
{
	switch (bbmcRequest.event) {
	case HOST_EVENT_COMMAND:
		bbmcRequest.state = HOST_REQUEST_BUSY;
		sub_2C394(6, BBMC_EVENT_REQUEST);
		break;
	case HOST_EVENT_ANSWERED:
		bbmcRequest.state = HOST_REQUEST_RECEIVE;
		break;
	}
}

/* request 0x0a's commands: 0x0a80 reads registers, 0x0a81 writes them, 0x0a82 sets P3.7 */
void bbmcRequestRun(void)
{
	unsigned char result;

	if (!sub_2BB99()) {
		hostRequestAnswer(&bbmcRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		sub_2BBE9();
		return;
	}
	if (!(getHardwareInfoFlags() & 0x02)) {
		hostRequestAnswer(&bbmcRequest, HOST_REQUEST_ANSWER, 2, 0);
		return;
	}
	switch (bbmcRequest.command) {
	case 0x0a80:
		if (bbmcRequest.length != 2) {
			bbmcRequest.answerLength = 0;
			bbmcRequest.status = 0x3b;
			hostRequestAnswerReady(&bbmcRequest.state);
			break;
		}
		result = bbmcReadRegs(bbmcRequestData[0], bbmcRequestData[1], bbmcRequestAnswer, 0);
		bbmcRequest.answerLength = result;
		bbmcRequest.status = result != 0 ? 0 : 4;
		hostRequestAnswerReady(&bbmcRequest.state);
		break;
	case 0x0a81:
		if (bbmcRequest.length < 3) {
			bbmcRequest.answerLength = 0;
			bbmcRequest.status = 0x3b;
			hostRequestAnswerReady(&bbmcRequest.state);
			break;
		}
		result = bbmcWriteRegs(bbmcRequestData[0], bbmcRequestData[1], &bbmcRequestData[2],
				       BBMC_WRITE_HOST);
		bbmcRequest.answerLength = 0;
		bbmcRequest.status = result != 0 ? 0 : 4;
		hostRequestAnswerReady(&bbmcRequest.state);
		break;
	case 0x0a82:
		if (bbmcRequest.length != 1) {
			bbmcRequest.status = 0x3b;
		} else {
			switch (bbmcRequestData[0]) {
			case 0:
				P37 = 0;
				bbmcRequest.status = 0;
				break;
			case 1:
				if (bbmcIsOn()) {
					P37 = 1;
					bbmcRequest.status = 0;
				} else {
					bbmcRequest.status = 5;
				}
				break;
			default:
				bbmcRequest.status = 0x3c;
				break;
			}
		}
		bbmcRequest.answerLength = 0;
		hostRequestAnswerReady(&bbmcRequest.state);
		break;
	default:
		bbmcRequest.state = HOST_REQUEST_UNKNOWN;
		break;
	}
}

void bbmcPostRxTimeout(void)
{
	sub_2C394(6, BBMC_EVENT_RX_TIMEOUT);
}

void bbmcPostLinkTimeout(void)
{
	sub_2C394(6, BBMC_EVENT_LINK_TIMEOUT);
}

void bbmcPostStart(void)
{
	sub_2C394(6, BBMC_EVENT_START);
}

void bbmcPostMode(void)
{
	sub_2C394(6, BBMC_EVENT_MODE);
}

void bbmcPostPower(void)
{
	sub_2C394(6, BBMC_EVENT_POWER);
}

void bbmcPostWake(void)
{
	sub_2C394(6, BBMC_EVENT_WAKE);
}
