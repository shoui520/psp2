/*
 * The power and battery requests: request 8 takes the power commands 0x0880-0x08c4 and
 * request 9 the battery's 0x0980-0x09b4, each from its command table, run by the task that
 * waits on event flag 4 (bits 0x0001 and 0x0100). Beside them, the host commands 0x0800-0x0804
 * and 0x0900-0x0902, which the command table answers at once. The commands' names are the
 * SceSyscon exports that send them.
 *
 * The answers' status: 0 done, 0x01 failed, 0x03 the data is wrong, 0x04 not on this board,
 * 0x21 and 0x22 the device did not answer or refused, 0x3b the data's length is wrong, 0x3e
 * not taken here before the handshake, 0x3f not known.
 */
#pragma sfr

#include <string.h>

#include "ernie.h"
#include "board.h"
#include "battery.h"
#include "charge.h"
#include "chip.h"
#include "config.h"
#include "cookie.h"
#include "elmo.h"
#include "eventlog.h"
#include "handshake.h"
#include "host.h"
#include "input.h"
#include "kernel.h"
#include "led.h"
#include "motion.h"
#include "param.h"
#include "powerreq.h"
#include "powerseq.h"
#include "powerwait.h"
#include "serial2.h"
#include "state.h"
#include "task.h"
#include "task5.h"
#if BOARD_DOL1001
#include "thermal.h"
#endif
#include "version.h"

/* request 9's data and answer: the battery's registers are words */
union BatteryBuffer {
	unsigned char	byte[0x20];
	unsigned int	word[0x10];
};

/* a request's command and its handler */
struct PowerCommand {
	unsigned int	command;
	void		(*handler)(void);
};

extern unsigned char input2Bit0Get(void);
extern unsigned char input2Bit1Get(void);
extern void inputMotionInfoLater(void);
extern void inputFlag40Later(void);
extern void inputFlag80Later(void);

/* the power status (0x0800) and the USB state (0x0804, 0x089a), the power module's */

void powerRequestNotify(void);
void batteryRequestNotify(void);
#if SOURCE_0103
/* the slim's: every command is answered with its status and answer's length, and logged */
void powerRequestReply(unsigned char status, unsigned char length);
void batteryRequestReply(unsigned char status, unsigned char length);
#define powerRequestDone(status)	powerRequestReply(status, 0)
static void powerRequestLogRequest(void);
static void powerRequestLogAnswer(void);
static void batteryRequestLogRequest(void);
static void batteryRequestLogAnswer(void);
#define POWER_REQUEST_ANSWER(code, size)	powerRequestReply(code, size)
#define BATTERY_REQUEST_ANSWER(code, size)	batteryRequestReply(code, size)
#else
void powerRequestUnknown(void);
void powerRequestDone(unsigned char status);
/* the answer's length, the answer ready, then its status */
#define POWER_REQUEST_ANSWER(code, size)						\
	powerRequest.answerLength = (size);						\
	hostRequestAnswerReady(&powerRequest.state);					\
	powerRequest.status = (code)
#define BATTERY_REQUEST_ANSWER(code, size)						\
	batteryRequest.answerLength = (size);						\
	hostRequestAnswerReady(&batteryRequest.state);					\
	batteryRequest.status = (code)
#endif
static void powerRequestGetElmoVersion(void);
static void powerRequestGetCookieVersion(void);
#if !FW_SINCE(0x00090903)
static void powerRequest0882(void);
#endif
static void powerRequestGetPowerStatus(void);
static void powerRequest0884(void);
static void powerRequest0885(void);
static void powerRequestCtrlHdmiCecPower(void);
static void powerRequestCtrlMotionSensorPower(void);
static void powerRequestCtrlSdPower(void);
static void powerRequestCtrlAccPower(void);
static void powerRequestCtrlWirelessPower(void);
static void powerRequest088B(void);
static void powerRequest088C(void);
static void powerRequestCtrlHpremote(void);
static void powerRequestCtrlUsbStatus(void);
static void powerRequest088D(void);
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
static void powerRequestCtrlRMRPower(void);
static void powerRequestCtrlMultiCnPower(void);
#endif
#if FW_SINCE(0x00090804)
static void powerRequestCtrlLedPwmBlink(void);
#endif
#if LED_REQUESTS
static void powerRequestSetChargeLedCtrl(void);
#endif
#if SOURCE_0103
static void powerRequest089F(void);
#endif
static void powerRequestCtrlVoltage(void);
static void powerRequestCtrlDeviceReset(void);
static void powerRequest0890(void);
static void powerRequestCtrlLED(void);
static unsigned char powerResult(unsigned char result);
static void powerRequestCtrlChargeAC(void);
static void powerRequestCtrlChargeVBUS(void);
static void powerRequest0894(void);
static void powerRequestReadElmoReg(void);
static void powerRequestWriteElmoReg(void);
static void powerRequestReadCookieReg(void);
static void powerRequestWriteCookieReg(void);
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
static void powerRequestBatteryStartBLMode(void);
static void powerRequestCtrlManualChargeMode(void);
static void powerRequestCtrlBatteryCharge(void);
static void powerRequestCtrlChargingCurrent(void);
static void powerRequestCtrlSupplyCurrentAc(void);
static void powerRequestCtrlSupplyCurrentUsb(void);
#endif
void powerReadStatus(unsigned char *out);
#if !FW_SINCE(0x00090903)
static void powerRequest08A0(void);
static void powerRequest08A1(void);
static void powerRequest08A2(void);
static void powerRequest08A3(void);
static void powerRequest08A4(void);
static void powerRequest08A8(void);
static void powerRequest08A9(void);
static void powerRequest08AA(void);
#endif
static void batteryRequestGetVersion(void);
static void batteryRequestGetTemp(void);
static void batteryRequestGetFullCap(void);
static void batteryRequestGetElec(void);
static void batteryRequestGetTTE(void);
static void batteryRequestGetSOH(void);
static void batteryRequestReadReg(void);
static void batteryRequestWriteReg(void);
static void batteryRequest0988(void);
static void batteryRequestSWReset(void);
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
static void batteryRequestGetFullCap2(void);
#endif
#if FW_SINCE(0x00090206)
static void batteryRequestGetBicTemp(void);
#endif
#if FW_SINCE(0x00090903)
static void batteryRequestGetCycleCount(void);
static void batteryRequest098D(void);
#endif
static void batteryRequestStartBLMode(void);
static void batteryRequestStopBLMode(void);
static void batteryRequestSetBLCommand(void);
static void batteryRequestExecBLCommand(void);
static void batteryRequestReadBLCommand(void);
static unsigned char batteryResult(unsigned char result);
#if FW_SINCE(0x0100060B)
static void powerRequest08C5(void);
#endif
#if SOURCE_0103
static void powerRequest08C6(void);
static void powerRequest08C7(void);
static void powerRequest08C8(void);
static void powerRequest08C9(void);
static void powerRequest08CA(void);
static void powerRequest08CB(void);
static void powerRequest08CC(void);
static void powerRequest08CD(void);
#endif
#if BOARD_DOL1001
static void powerRequest08CE(void);
static void powerRequest08CF(void);
#endif

struct HostRequest powerRequest;		/* request 8 */
struct HostRequest batteryRequest;		/* request 9 */
static unsigned int powerCommand;		/* request 8's command, while it runs */
static unsigned char powerRequestData[10];
static unsigned char powerRequestAnswer[10];
#if !FW_SINCE(0x0100060B)
struct InputHandler powerInputHandler;	/* input.c's since 0x0100060B */
#endif
static union BatteryBuffer batteryRequestData;
static union BatteryBuffer batteryRequestAnswer;

/* request 8's commands */
static const struct PowerCommand powerCommands[] = {
	{ 0x0880, powerRequestGetElmoVersion },
	{ 0x0881, powerRequestGetCookieVersion },
#if !FW_SINCE(0x00090903)
	{ 0x0882, powerRequest0882 },
#endif
	{ 0x0883, powerRequestGetPowerStatus },
	{ 0x0884, powerRequest0884 },
	{ 0x0885, powerRequest0885 },
	{ 0x0886, powerRequestCtrlHdmiCecPower },
	{ 0x0887, powerRequestCtrlMotionSensorPower },
	{ 0x0888, powerRequestCtrlSdPower },
	{ 0x0889, powerRequestCtrlAccPower },
	{ 0x088a, powerRequestCtrlWirelessPower },
	{ 0x088b, powerRequest088B },
	{ 0x088c, powerRequest088C },
	{ 0x088d, powerRequest088D },
	{ 0x088e, powerRequestCtrlVoltage },
	{ 0x088f, powerRequestCtrlDeviceReset },
	{ 0x0890, powerRequest0890 },
	{ 0x0891, powerRequestCtrlLED },
	{ 0x0892, powerRequestCtrlChargeAC },
	{ 0x0893, powerRequestCtrlChargeVBUS },
	{ 0x0894, powerRequest0894 },
	{ 0x0895, powerRequestReadElmoReg },
	{ 0x0896, powerRequestWriteElmoReg },
	{ 0x0897, powerRequestReadCookieReg },
	{ 0x0898, powerRequestWriteCookieReg },
	{ 0x0899, powerRequestCtrlHpremote },
	{ 0x089a, powerRequestCtrlUsbStatus },
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
	{ 0x089b, powerRequestCtrlRMRPower },
	{ 0x089c, powerRequestCtrlMultiCnPower },
#endif
#if FW_SINCE(0x00090804)
	{ 0x089d, powerRequestCtrlLedPwmBlink },
#endif
#if LED_REQUESTS
	{ 0x089e, powerRequestSetChargeLedCtrl },
#endif
#if SOURCE_0103
	{ 0x089f, powerRequest089F },
#endif
#if !FW_SINCE(0x00090903)
	{ 0x08a0, powerRequest08A0 },
	{ 0x08a1, powerRequest08A1 },
	{ 0x08a2, powerRequest08A2 },
	{ 0x08a3, powerRequest08A3 },
	{ 0x08a4, powerRequest08A4 },
	{ 0x08a8, powerRequest08A8 },
	{ 0x08a9, powerRequest08A9 },
	{ 0x08aa, powerRequest08AA },
#endif
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
	{ 0x08b0, powerRequestBatteryStartBLMode },
	{ 0x08c0, powerRequestCtrlManualChargeMode },
	{ 0x08c1, powerRequestCtrlBatteryCharge },
	{ 0x08c2, powerRequestCtrlChargingCurrent },
	{ 0x08c3, powerRequestCtrlSupplyCurrentAc },
	{ 0x08c4, powerRequestCtrlSupplyCurrentUsb },
#endif
#if FW_SINCE(0x0100060B)
	{ 0x08c5, powerRequest08C5 },
#endif
#if SOURCE_0103
	{ 0x08c6, powerRequest08C6 },
	{ 0x08c7, powerRequest08C7 },
	{ 0x08c8, powerRequest08C8 },
	{ 0x08c9, powerRequest08C9 },
	{ 0x08ca, powerRequest08CA },
	{ 0x08cb, powerRequest08CB },
	{ 0x08cc, powerRequest08CC },
	{ 0x08cd, powerRequest08CD },
#endif
#if BOARD_DOL1001
	{ 0x08ce, powerRequest08CE },
	{ 0x08cf, powerRequest08CF },
#endif
};

/* request 9's commands */
static const struct PowerCommand batteryCommands[] = {
	{ 0x0980, batteryRequestGetVersion },
	{ 0x0981, batteryRequestGetTemp },
	{ 0x0982, batteryRequestGetFullCap },
	{ 0x0983, batteryRequestGetElec },
	{ 0x0984, batteryRequestGetTTE },
	{ 0x0985, batteryRequestGetSOH },
	{ 0x0986, batteryRequestReadReg },
	{ 0x0987, batteryRequestWriteReg },
	{ 0x0988, batteryRequest0988 },
	{ 0x0989, batteryRequestSWReset },
#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
	{ 0x098a, batteryRequestGetFullCap2 },
#endif
#if FW_SINCE(0x00090206)
	{ 0x098b, batteryRequestGetBicTemp },
#endif
#if FW_SINCE(0x00090903)
	{ 0x098c, batteryRequestGetCycleCount },
	{ 0x098d, batteryRequest098D },
#endif
	{ 0x09b0, batteryRequestStartBLMode },
	{ 0x09b1, batteryRequestStopBLMode },
	{ 0x09b2, batteryRequestSetBLCommand },
	{ 0x09b3, batteryRequestExecBLCommand },
	{ 0x09b4, batteryRequestReadBLCommand },
};

void powerRequestInit(void)
{
	powerCommand = 0;
	memset(powerRequestData, 0, sizeof(powerRequestData));
	memset(powerRequestAnswer, 0, sizeof(powerRequestAnswer));
	powerRequest.id = 8;
	powerRequest.size = sizeof(powerRequestData);
	powerRequest.data = powerRequestData;
	powerRequest.answer = powerRequestAnswer;
	powerRequest.notify = powerRequestNotify;
	powerRequest.state = HOST_REQUEST_RECEIVE;
	batteryRequest.id = 9;
	batteryRequest.size = sizeof(batteryRequestData);
	batteryRequest.data = batteryRequestData.byte;
	batteryRequest.answer = batteryRequestAnswer.byte;
	batteryRequest.notify = batteryRequestNotify;
	batteryRequest.state = HOST_REQUEST_RECEIVE;
#if !FW_SINCE(0x0100060B)
	powerInputHandler.type = 0;
	powerInputHandler.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	powerInputHandler.handler = powerInputChanged;
#endif
}

/* the power input bit changed: event flag 4's 0x0040 */
void powerInputChanged(void)
{
	task5SetFlag(4, 0x0040);
}

#if SOURCE_0103
void powerRequestNotify(void)
{
	switch (powerRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		hostRequestSetState(&powerRequest, HOST_REQUEST_BUSY);
		powerCommand = powerRequest.command;
		task5SetFlag(4, 0x0001);
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&powerRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_OPEN:
		hostRequestSetState(&powerRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&powerRequest, HOST_REQUEST_IDLE);
		break;
	}
}

void batteryRequestNotify(void)
{
	switch (batteryRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		hostRequestSetState(&batteryRequest, HOST_REQUEST_BUSY);
		task5SetFlag(4, 0x0100);
		break;
	case HOST_EVENT_ANSWERED:
		hostRequestSetState(&batteryRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_OPEN:
		hostRequestSetState(&batteryRequest, HOST_REQUEST_RECEIVE);
		break;
	case HOST_EVENT_CLOSE:
		hostRequestSetState(&batteryRequest, HOST_REQUEST_IDLE);
		break;
	}
}

/* answers request 8 */
void powerRequestReply(unsigned char status, unsigned char length)
{
	hostRequestAnswer(&powerRequest, HOST_REQUEST_ANSWER, status, length);
}

/* answers request 9 */
void batteryRequestReply(unsigned char status, unsigned char length)
{
	hostRequestAnswer(&batteryRequest, HOST_REQUEST_ANSWER, status, length);
}

/* request 8's command, from powerCommands */
void powerRequestEvent(void)
{
	unsigned char i;

	powerRequestLogRequest();
	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&powerRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
		powerRequestLogAnswer();
	} else {
		for (i = 0; i < sizeof(powerCommands) / sizeof(powerCommands[0]); i++) {
			if (powerCommands[i].command == powerCommand) {
				powerCommands[i].handler();
				powerRequestLogAnswer();
				return;
			}
		}
		hostRequestAnswer(&powerRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
		powerRequestLogAnswer();
	}
}

/* request 9's command, from batteryCommands */
void batteryRequestEvent(void)
{
	unsigned char i;

	batteryRequestLogRequest();
	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
		batteryRequestLogAnswer();
	} else {
		for (i = 0; i < sizeof(batteryCommands) / sizeof(batteryCommands[0]); i++) {
			if (batteryCommands[i].command == batteryRequest.command) {
				batteryCommands[i].handler();
				batteryRequestLogAnswer();
				return;
			}
		}
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
		batteryRequestLogAnswer();
	}
}

/* the event log's record of request 8's command (code 0x08): the number, the length and the
   data's first two bytes */
static void powerRequestLogRequest(void)
{
#if BOARD_EVENT_LOG
	eventLogCharge.byte[0] = powerRequest.command & 0xff;
	eventLogCharge.byte[1] = powerRequest.length;
	eventLogCharge.byte[2] = powerRequest.data[0];
	eventLogCharge.byte[3] = powerRequest.data[1];
	eventLogRecord(0x10, 0x08, eventLogCharge.byte, 4);
#endif
}

/* and of its answer (code 0x88): the number, the answer's length, its status and first byte */
static void powerRequestLogAnswer(void)
{
#if BOARD_EVENT_LOG
	eventLogCharge.byte[0] = powerRequest.command & 0xff;
	eventLogCharge.byte[1] = powerRequest.answerLength;
	eventLogCharge.byte[2] = powerRequest.status;
	eventLogCharge.byte[3] = powerRequest.answer[0];
	eventLogRecord(0x10, 0x88, eventLogCharge.byte, 4);
#endif
}

/* request 9's (codes 0x09 and 0x89) */
static void batteryRequestLogRequest(void)
{
#if BOARD_EVENT_LOG
	eventLogCharge.byte[0] = batteryRequest.command & 0xff;
	eventLogCharge.byte[1] = batteryRequest.length;
	eventLogCharge.byte[2] = batteryRequest.data[0];
	eventLogCharge.byte[3] = batteryRequest.data[1];
	eventLogRecord(0x10, 0x09, eventLogCharge.byte, 4);
#endif
}

static void batteryRequestLogAnswer(void)
{
#if BOARD_EVENT_LOG
	eventLogCharge.byte[0] = batteryRequest.command & 0xff;
	eventLogCharge.byte[1] = batteryRequest.answerLength;
	eventLogCharge.byte[2] = batteryRequest.status;
	eventLogCharge.byte[3] = batteryRequest.answer[0];
	eventLogRecord(0x10, 0x89, eventLogCharge.byte, 4);
#endif
}
#else
void powerRequestNotify(void)
{
	switch (powerRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		powerRequest.state = HOST_REQUEST_BUSY;
		powerCommand = powerRequest.command;
		task5SetFlag(4, 0x0001);
		break;
	case HOST_EVENT_ANSWERED:
		powerRequest.state = HOST_REQUEST_RECEIVE;
		break;
	}
}

void batteryRequestNotify(void)
{
	switch (batteryRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		batteryRequest.state = HOST_REQUEST_BUSY;
		task5SetFlag(4, 0x0100);
		break;
	case HOST_EVENT_ANSWERED:
		batteryRequest.state = HOST_REQUEST_RECEIVE;
		break;
	}
}

/* request 8's command, from powerCommands */
void powerRequestEvent(void)
{
	unsigned char i;

	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&powerRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
	} else {
		for (i = 0; i < sizeof(powerCommands) / sizeof(powerCommands[0]); i++) {
			if (powerCommands[i].command == powerCommand) {
				powerCommands[i].handler();
				return;
			}
		}
#if FW_SINCE(0x00090206)
		hostRequestAnswer(&powerRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
#else
		powerRequest.state = HOST_REQUEST_ERROR;
		powerRequest.status = 0x3f;
#endif
	}
}

/* request 9's command, from batteryCommands */
void batteryRequestEvent(void)
{
	unsigned char i;

	if (!handshakeUnlocked11()) {
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		handshakeRefuse();
	} else {
		for (i = 0; i < sizeof(batteryCommands) / sizeof(batteryCommands[0]); i++) {
			if (batteryCommands[i].command == batteryRequest.command) {
				batteryCommands[i].handler();
				return;
			}
		}
#if FW_SINCE(0x00090206)
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
#else
		batteryRequest.state = HOST_REQUEST_ERROR;
		batteryRequest.status = 0x3f;
#endif
	}
}
#endif

/* 0x0800 (SceSysconForDriver_A2FE9BF9): the power status */
unsigned char hostCommandPowerStatus(unsigned char *out, unsigned char *length)
{
	dis_dsp();
#if SOURCE_0103
	*out++ = powerStatus.byte[0];
	*out++ = powerStatus.byte[1];
	*out++ = powerStatus.byte[2];
	*out = powerStatus.byte[3];
	ena_dsp();
	*length = 4;
#else
	*out++ = powerStatus.byte[0];
	*out = powerStatus.byte[1];
	ena_dsp();
	*length = 2;
#endif
	return 0;
}

/* 0x0801: the system mode's transition 0x54 */
unsigned char hostCommand0801(unsigned char *out, unsigned char *length)
{
	STATE_TRANSITION_EVENT
	STATE_TRANSITION(0x54);
	*length = 0;
	return 0;
}

#if !FW_SINCE(0x0100060B)
/* 0x0802 (sceSysconGetBatteryRemainCap, to version 0x70503) */
unsigned char hostCommandBatteryRemainCapOld(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(charge.batteryRemainCap & 0xff);
	out[1] = (unsigned char)(charge.batteryRemainCap >> 8);
	*length = 2;
	return 0;
}

/* 0x0803 (sceSysconGetBatteryVolt, to version 0x70503) */
unsigned char hostCommandBatteryVoltOld(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(charge.batteryVolt & 0xff);
	out[1] = (unsigned char)(charge.batteryVolt >> 8);
	*length = 2;
	return 0;
}
#endif

#if SOURCE_0103
/* 0x0804: the USB state 4, while the USB is in state 1 */
unsigned char hostCommand0804(unsigned char *out, unsigned char *length)
{
	if (chargeUsbIs1()) {
		if (chargeCurrent != 0xff) {
			chargeCurrent = 0xff;
			chargeUsbMode = 4;
#if BOARD_EVENT_LOG
			eventLogRecord(5, 0x22, 0, 0);
#endif
			powerDeviceSetPending(0x1000);
		}
	}
	*length = 0;
	return 0;
}

#if BOARD_DOL1001
/* 0x0805: P7.5 low */
unsigned char hostCommand0805(unsigned char *out, unsigned char *length)
{
	*length = 1;
	*out = !P7.5;
	return 0;
}

/* 0x0806: P7.4 low, as bit 0 */
unsigned char hostCommand0806(unsigned char *out, unsigned char *length)
{
	unsigned char value = 0;

	*length = 1;
	if (!P7.4)
		value |= 1;
	*out = value;
	return 0;
}

/* 0x0807: the temperature sensor's version */
unsigned char hostCommand0807(unsigned char *out, unsigned char *length)
{
	*length = 2;
	*out++ = deviceVersions.thermal[0];
	*out = deviceVersions.thermal[1];
	return 0;
}
#else
/* 0x0805-0x0807: one or two zero bytes, answered as unknown */
unsigned char hostCommand0805(unsigned char *out, unsigned char *length)
{
	*length = 1;
	*out = 0;
	return 0x3f;
}

unsigned char hostCommand0806(unsigned char *out, unsigned char *length)
{
	unsigned char value = 0;

	*length = 1;
	*out = value;
	return 0x3f;
}

unsigned char hostCommand0807(unsigned char *out, unsigned char *length)
{
	*length = 2;
	*out++ = 0;
	*out = 0;
	return 0x3f;
}
#endif

/* 0x0820: the power LED's host blink off */
unsigned char hostCommand0820(unsigned char *out, unsigned char *length)
{
#if BOARD_DOL1001
	powerLedEndMode(POWER_LED_MODE_HOST_BLINK);
#else
	powerLedClearFlags(0x0008);
#endif
	*length = 0;
	return 0;
}

/* 0x0821 and 0x0822: the power LED's host blink on, with the blink times 0 or 1 */
unsigned char hostCommand0821(unsigned char *out, unsigned char *length)
{
	powerLedLoadBlink(0);
#if BOARD_DOL1001
	powerLedStartMode(POWER_LED_MODE_HOST_BLINK);
#else
	powerLedSetFlags(0x0008);
#endif
	*length = 0;
	return 0;
}

unsigned char hostCommand0822(unsigned char *out, unsigned char *length)
{
	powerLedLoadBlink(1);
#if BOARD_DOL1001
	powerLedStartMode(POWER_LED_MODE_HOST_BLINK);
#else
	powerLedSetFlags(0x0008);
#endif
	*length = 0;
	return 0;
}

/* 0x0840: the manual charge mode */
unsigned char hostCommand0840(unsigned char *out, unsigned char *length)
{
	*out = chargeGetManualMode();
	*length = 1;
	return 0;
}

/* 0x0841: the charging current set */
unsigned char hostCommand0841(unsigned char *out, unsigned char *length)
{
	unsigned char result = 0;

	if (cookieChargingCurrentDecode(out, cookieGetChargingCurrent()) != 0) {
		*out = 0;
		result = 1;
	}
	*length = 1;
	return result;
}

/* 0x0842: the AC supply current set (DOL-1001's; the slim has none) */
unsigned char hostCommand0842(unsigned char *out, unsigned char *length)
{
	unsigned char result = 0;

#if BOARD_DOL1001
	if (cookieSupplyCurrentDecode(out, cookieGetSupplyCurrent()) != 0) {
		*out = 0;
		result = 1;
	}
#else
	*out = 0;
#endif
	*length = 1;
	return result;
}

/* 0x0843: the USB supply current set */
unsigned char hostCommand0843(unsigned char *out, unsigned char *length)
{
	unsigned char result = 0;

	if (cookieSupplyCurrentUsbDecode(out, cookieGetSupplyCurrentUsb()) != 0) {
		*out = 0;
		result = 1;
	}
	*length = 1;
	return result;
}
#else
/* 0x0804: the USB state 4 */
unsigned char hostCommand0804(unsigned char *out, unsigned char *length)
{
	if (chargeCurrent != 0xff) {
		chargeCurrent = 0xff;
		chargeUsbMode = 4;
		powerDeviceSetPending(0x1000);
	}
	*length = 0;
	return 0;
}

/* never called */
void powerRequestUnknown(void)
{
	powerRequest.state = HOST_REQUEST_ERROR;
	powerRequest.status = 0x3f;
}

/* answers request 8 with status and no data */
void powerRequestDone(unsigned char status)
{
	powerRequest.answerLength = 0;
	powerRequest.status = status;
	hostRequestAnswerReady(&powerRequest.state);
}
#endif

/* 0x0880 (sceSysconGetElmoVersion) */
#if SOURCE_0103
static void powerRequestGetElmoVersion(void)
{
	powerRequestAnswer[0] = deviceVersions.elmo[1];
	powerRequestAnswer[1] = deviceVersions.elmo[0];
	powerRequestReply(0, 2);
}
#else
static void powerRequestGetElmoVersion(void)
{
	powerRequestAnswer[0] = deviceVersions.elmo[1];
	powerRequestAnswer[1] = deviceVersions.elmo[0];
	POWER_REQUEST_ANSWER(0, 2);
}
#endif

/* 0x0881 (sceSysconGetCookieVersion) */
#if SOURCE_0103
static void powerRequestGetCookieVersion(void)
{
	powerRequestAnswer[0] = deviceVersions.cookie[1];
	powerRequestAnswer[1] = deviceVersions.cookie[0];
	powerRequestReply(0, 2);
}
#else
static void powerRequestGetCookieVersion(void)
{
	powerRequestAnswer[0] = deviceVersions.cookie[1];
	powerRequestAnswer[1] = deviceVersions.cookie[0];
	POWER_REQUEST_ANSWER(0, 2);
}
#endif

#if !FW_SINCE(0x00090903)
/* 0x0882 (0x00090804's): the battery's three version words */
static void powerRequest0882(void)
{
	unsigned int *answer;

	answer = (unsigned int *)powerRequestAnswer;
	*answer++ = deviceVersions.battery.version[0];
	*answer++ = deviceVersions.battery.version[1];
	*answer = deviceVersions.battery.version[2];
	POWER_REQUEST_ANSWER(0, 6);
}
#endif

/* 0x0883 (sceSysconGetPowerStatus) */
#if SOURCE_0103
static void powerRequestGetPowerStatus(void)
{
	powerReadStatus(powerRequestAnswer);
	powerRequestReply(0, 8);
}
#else
static void powerRequestGetPowerStatus(void)
{
	powerReadStatus(powerRequestAnswer);
	POWER_REQUEST_ANSWER(0, 8);
}
#endif

/* 0x0884 (SceSysconForDriver_3FDD29D6) */
static void powerRequest0884(void)
{
	unsigned char result;
	unsigned char on1 = 0;
	unsigned char on0 = 0;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x80) {
			if (powerRequestData[0] & 0x01)
				on0 = 1;
			if (powerRequestData[0] & 0x02)
				on1 = 1;
			result = powerElmoA0Set(on1, on0);
		} else {
			if (powerRequestData[0] & 0x01)
				on0 = 1;
			if (powerRequestData[0] & 0x02)
				on1 = 1;
			result = powerElmoA0Clear(on1, on0);
		}
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0885 (SceSysconForDriver_79E6DD8B) */
#if BOARD_102603 && !HW_IS(0x00315200)
/* Elmo register 0xa3's bits 15 and 14 and register 0xa0's bit 12: bit 14 (data bit 1 to set,
   bit 2 to clear) must be on, or already on, for bit 15 (data bit 2 to set, bit 1 to clear),
   0x3c otherwise */
static void powerRequest0885(void)
{
	unsigned char result;
	unsigned char on2 = 0;
	unsigned char on1 = 0;
	unsigned char on0 = 0;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x80) {
			if ((powerRequestData[0] & 0x02) || (elmoStatusMask.reg[3] & 0x4000))
				on1 = 1;
			else if (powerRequestData[0] & 0x04) {
				result = 0x3c;
				goto out;
			}
			if (powerRequestData[0] & 0x01)
				on0 = 1;
			if (powerRequestData[0] & 0x04)
				on2 = 1;
			result = powerElmoA0Set2(on2, on1, on0);
		} else {
			if ((powerRequestData[0] & 0x04) || (elmoStatusMask.reg[3] & 0x4000))
				on2 = 1;
			else if (powerRequestData[0] & 0x02) {
				result = 0x3c;
				goto out;
			}
			if (powerRequestData[0] & 0x01)
				on0 = 1;
			if (powerRequestData[0] & 0x02)
				on2 = 1;
			result = powerElmoA0Clear2(on2, on1, on0);
		}
		result = powerResult(result);
	} else
		result = 0x3b;
out:
	powerRequestDone(result);
}
#else
#if SOURCE_0103
/* the slim's: Elmo register 0xa0's bits, data bits 0-2 any set */
static void powerRequest0885(void)
{
	unsigned char result = 0;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x80) {
			if (powerRequestData[0] & 0x07) {
				result = powerElmoA0Set2();
				result = powerResult(result);
			}
		} else {
			if (powerRequestData[0] & 0x07) {
				result = powerElmoA0Clear2();
				result = powerResult(result);
			}
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
static void powerRequest0885(void)
{
	unsigned char result;
	unsigned char on2 = 0;
	unsigned char on1 = 0;
	unsigned char on0 = 0;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x80) {
			if (powerRequestData[0] & 0x04)
				on2 = 1;
			result = powerElmoA0Set2(on2, on1, on0);
		} else {
			if (powerRequestData[0] & 0x02)
				on2 = 1;
			result = powerElmoA0Clear2(on2, on1, on0);
		}
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif
#endif

/* 0x0886 (sceSysconCtrlHdmiCecPower) */
static void powerRequestCtrlHdmiCecPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerHdmiCecOn();
		else
			result = powerHdmiCecOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0887 (sceSysconCtrlMotionSensorPower) */
static void powerRequestCtrlMotionSensorPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerMotionOn();
		else
			result = powerMotionOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0888 (sceSysconCtrlSdPower) */
static void powerRequestCtrlSdPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerSdOn();
		else
			result = powerSdOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0889 (sceSysconCtrlAccPower) */
static void powerRequestCtrlAccPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerAccOn();
		else
			result = powerAccOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x088a (sceSysconCtrlWirelessPower) */
static void powerRequestCtrlWirelessPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerOnWlan();
		else
			result = powerOffWlan();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x088b (SceSysconForDriver_5A614349): not on this syscon */
static void powerRequest088B(void)
{
	powerRequestDone(5);
	return;
}

/* 0x088c (SceSysconForDriver_B872E904): not on this syscon */
static void powerRequest088C(void)
{
	powerRequestDone(6);
	return;
}

/* 0x0899 (sceSysconCtrlHpremote, the wiki's guess) */
static void powerRequestCtrlHpremote(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerHpremoteOn();
		else
			result = powerHpremoteOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x089a (sceSysconCtrlUsbStatus) */
#if SOURCE_0103
static void powerRequestCtrlUsbStatus(void)
{
	unsigned char result = 0;
	unsigned char mode = chargeUsbMode;
	unsigned char status[2];

	if (powerRequest.length < 3) {
		chargeUsbMode = powerRequestData[0];
		if (chargeUsbMode != 0)
			;
		switch (chargeUsbMode) {
		case 3:
			chargeUsbHold = 0;
			break;
		case 0:
		case 1:
		case 2:
		case 4:
			break;
		case 5:
			if (chargeGetCharging() != 0)
				chargeUsbHold = 0xff;
			break;
		}
		if (!chargeUsbState.7) {
			cookieReadStatus2(status);
			dis_dsp();
#if BOARD_DOL1001
			powerStatus.bits.usb = (status[1] >> 2) & 3;
#else
			powerStatus.bits.usb = (status[1] >> 1) & 7;
#endif
			ena_dsp();
		}
		if (mode != chargeUsbMode) {
			result = powerResult(chargeSetCurrent());
			if (!chargeUsbState.7)
				task5SetFlag(4, 0x0020);
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlUsbStatus(void)
{
	unsigned char result;
#if FW_SINCE(0x01000104)
	unsigned char status[3];
#else
	unsigned char reg;
#endif

	if (powerRequest.length < 3) {
		chargeUsbMode = powerRequestData[0];
#if FW_SINCE(0x00090903)
		switch (chargeUsbMode) {
		case 3:
			chargeUsbHold = 0;
			break;
		case 0:
		case 1:
		case 2:
		case 4:
			break;
		case 5:
#if FW_SINCE(0x01000104)
			if (chargeGetCharging() != 0)
#endif
				chargeUsbHold = 0xff;
			break;
		}
#endif
#if FW_SINCE(0x01000104)
		cookieReadStatus2(status);
		dis_dsp();
		powerStatus.bits.usb = status[1] >> 2;
		ena_dsp();
#else
		reg = COOKIE_CTRL_REG(2);
		cookieRead(&reg, cookieStatus2.byte);
		dis_dsp();
		powerStatus.bits.usb = cookieStatus2.bits.usb;
		ena_dsp();
#endif
#if FW_SINCE(0x00090903)
		result = powerResult(chargeSetCurrent());
#else
		result = powerResult((unsigned char)chargeSetCurrent());
#endif
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* the 3G modem's P3.3 pulse: powerBbmcPulse since 0x01000104, written out before */
#if FW_SINCE(0x01000104)
#define POWER_BBMC_PULSE()	powerBbmcPulse()
#else
#define POWER_BBMC_PULSE()	(P33 = 1, powerSleep(0x0e74), P33 = 0)
#endif

/* 0x088d (SceSysconForDriver_DD16ABD9) */
#if !BOARD_BBMC
/* no 3G modem */
static void powerRequest088D(void)
{
	powerRequestDone(4);
	return;
}
#else
static void powerRequest088D(void)
{
	unsigned char result;

	if (!(getHardwareInfoFlags() & 0x02)) {
		powerRequestDone(4);
	} else {
		if (powerRequest.length == 1) {
			if (powerRequestData[0] & 0x01) {
				if (powerRequestData[0] & 0x80) {
					if (input2Bit0Get() == 1) {
						POWER_BBMC_PULSE();
						powerSleep(500);
					}
				}
				result = powerBbmcStart();
			} else {
				result = powerBbmcCycle();
				if (powerRequestData[0] & 0x80) {
					if (input2Bit0Get() == 1)
						POWER_BBMC_PULSE();
					if (result != 0 && input2Bit1Get() == 0)
						result = 0;
				}
			}
		} else
			result = 0x3b;
		powerRequestDone(result);
	}
}
#endif

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
/* 0x089b (sceSysconCtrlRMRPower) */
#if BOARD_102603
/* nothing to power on 0x00102603 */
static void powerRequestCtrlRMRPower(void)
{
	powerRequestDone(0);
	return;
}
#else
static void powerRequestCtrlRMRPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerRmrOn();
		else
			result = powerRmrOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x089c (sceSysconCtrlMultiCnPower) */
#if BOARD_102603
/* nothing to power on 0x00102603 */
static void powerRequestCtrlMultiCnPower(void)
{
	powerRequestDone(0);
	return;
}
#else
static void powerRequestCtrlMultiCnPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerMultiCnOn();
		else
			result = powerMultiCnOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif
#endif

/*
 * 0x089d (sceSysconCtrlLedPwmBlink). With data[1] 0x80 and the LED module busy, the LED
 * module answers when it is done (powerRequestLedDone).
 */
#if BOARD_DOL1001
/* the PS TV's: answered, nothing done */
static void powerRequestCtrlLedPwmBlink(void)
{
	unsigned char result = 0;

	powerRequestDone(result);
}

#elif LED_REQUESTS
static void powerRequestCtrlLedPwmBlink(void)
{
	unsigned char result = 0;

	if (powerRequest.length == 2) {
		switch (powerRequestData[0]) {
		case 1:
#if FW_SINCE(0x01000306) && !SOURCE_0103
			ledSetOff(0);
#endif
			switch (powerRequestData[1] & 0x0f) {
			case 0:
				if (powerRequestData[1] & 0x80) {
					if (powerLedGetState() & POWER_LED_REQUEST_PATTERNS) {
						if (powerRequestData[1] & 0x40)
							powerLedFadeOff();
						else
							powerLedFadeOn();
						return;
					}
				} else {
					if (powerRequestData[1] & 0x40)
						powerLedSetMode(POWER_LED_SET_OFF);
					else
						powerLedSetMode(POWER_LED_SET_ON);
				}
				break;
			case 1:
				powerLedSetMode(POWER_LED_SET_PATTERN1);
				break;
			case 2:
				powerLedSetMode(POWER_LED_SET_PATTERN2);
				break;
			case 3:
				powerLedSetMode(POWER_LED_SET_PATTERN3);
				break;
			default:
				result = 3;
				break;
			}
			if (powerRequestData[1] & 0x20)
				powerLedSetKeep(1);
			else
				powerLedSetKeep(0);
			break;
		default:
			result = 3;
			break;
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

#elif FW_SINCE(0x00090804)
/* before the LED requests, data[1] is powerLedFadeStart's pattern (no 0x089d on 0x00090206,
   which has no power LED fade) */
static void powerRequestCtrlLedPwmBlink(void)
{
	unsigned char result;

	if (powerRequest.length == 2) {
		if (powerRequestData[0] == 1) {
			if (powerRequestData[1] >= 4) {
				result = 3;
			} else {
				powerLedFadeStart(powerRequestData[1]);
				result = 0;
			}
		} else
			result = 3;
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

#if LED_REQUESTS
/* the LED module is done: answers 0x089d */
void powerRequestLedDone(void)
{
	if (powerCommand == 0x089d && powerRequest.state == HOST_REQUEST_BUSY)
		powerRequestDone(0);
}

#if BOARD_DOL1001
/* 0x089e (sceSysconSetChargeLedCtrl): no charge LED on the PS TV */
static void powerRequestSetChargeLedCtrl(void)
{
	powerRequestDone(0);
}
#else
/* 0x089e (sceSysconSetChargeLedCtrl) */
static void powerRequestSetChargeLedCtrl(void)
{
	unsigned char result = 0;

	if (powerRequest.length == 1) {
		switch (powerRequestData[0]) {
#if SOURCE_0103
		/* the slim's: no charge LED for the host to switch */
		case 0:
			break;
		case 1:
			break;
		case 2:
			if (configGet()->chargeLedHost)
				;
			break;
#else
		case 0:
			chargeLedSetHost(0);
			chargeLedSetHostOff(1);
			break;
		case 1:
			chargeLedSetHost(0);
			chargeLedSetHostOff(0);
			break;
		case 2:
			if (configGet()->chargeLedHost) {
				chargeLedSetHostOff(0);
				chargeLedSetHost(1);
			}
			break;
#endif
		default:
			result = 3;
			break;
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

#if BOARD_DOL1001
/* 0x089f: the power LED's host pattern (data[1] 0, data[2-3] and data[4-5] its times, low byte
   first) shown (data[0] 1) or ended (0) */
static void powerRequest089F(void)
{
	unsigned int on;
	unsigned int off;
	unsigned char result = 0;

	if (powerRequest.length == 6) {
		if (powerRequestData[0] == 0) {
			powerLedSetPattern(0xff, 0, 0);
			powerLedEndMode(POWER_LED_MODE_HOST);
		} else if (powerRequestData[0] == 1) {
			on = powerRequestData[2] | (unsigned int)powerRequestData[3] << 8;
			off = powerRequestData[4] | (unsigned int)powerRequestData[5] << 8;
			if (on != 0 || off != 0) {
				if (powerRequestData[1] == 0) {
					powerLedSetPattern(0, on, off);
					powerLedStartMode(POWER_LED_MODE_HOST);
				}
			}
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#elif SOURCE_0103
/* 0x089f: the power LED's blink (data[1] the pattern, data[2-3] and data[4-5] its times, low byte
   first) set (data[0] 1) or stopped (0) */
static void powerRequest089F(void)
{
	unsigned int on;
	unsigned int off;
	unsigned char result = 0;

	if (powerRequest.length == 6) {
		if (powerRequestData[0] == 0) {
			powerLedSetPattern(0xff, 0, 0);
			powerLedClearFlags(0x0004);
		} else if (powerRequestData[0] == 1) {
			on = powerRequestData[2] | (unsigned int)powerRequestData[3] << 8;
			off = powerRequestData[4] | (unsigned int)powerRequestData[5] << 8;
			if (on != 0 || off != 0) {
				switch (powerRequestData[1]) {
				case 0:
					powerLedSetPattern(0, on, off);
					powerLedSetFlags(0x0004);
					break;
				case 1:
					powerLedSetPattern(1, on, off);
					powerLedSetFlags(0x0004);
					break;
				case 2:
					powerLedSetPattern(2, on, off);
					powerLedSetFlags(0x0004);
					break;
				}
			}
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif
#endif

/* 0x088e (sceSysconCtrlVoltage): data[0] the regulator 1-4, data[1] its level */
static void powerRequestCtrlVoltage(void)
{
	unsigned char result;
	unsigned char regulator;
	unsigned char level;
	unsigned char trim;

	if (powerRequest.length == 2) {
		switch (powerRequestData[0]) {
		case 1:
			regulator = 0x81;
			break;
		case 2:
			regulator = 0x82;
			break;
		case 3:
			regulator = 0x83;
			break;
		case 4:
			regulator = 0x84;
			break;
		default:
			result = 3;
			goto done;
		}
#if BOARD_IRS1001
		/* IRS-1001's regulators take 0x48-0x87 */
		if (powerRequestData[1] < 0x48 || powerRequestData[1] >= 0x88) {
#elif BOARD_DOL1001
		/* DOL-1001's take 0-0x3f */
		if (powerRequestData[1] < 0 || powerRequestData[1] >= 0x40) {
#elif SOURCE_0103
		/* USS-1001's take 0x3c-0x7b */
		if (powerRequestData[1] < 0x3c || powerRequestData[1] >= 0x7c) {
#elif FW_SINCE(0x0100060B)
		/* Sony's tests the level against a lowest level of 0 too */
		if (powerRequestData[1] < 0 || powerRequestData[1] >= 0x40) {
#else
		if (powerRequestData[1] >= 0x40) {
#endif
			result = 3;
		} else {
#if BOARD_DOL1001
			/* DOL-1001's: powerSeqLevels' trims */
			if (regulator == 0x84)
				trim = configGet()->resumeTrim84;
			else
				trim = configGet()->resumeTrim;
#elif FW_SINCE(0x00090206)
			trim = configGet()->voltageTrim;
#else
			paramGetBase(&trim);	/* 0x00090009: the parameters' */
#endif
			level = levelTrim(powerRequestData[1], trim);
			result = powerResult(powerSetVoltage(regulator, level));
		}
	} else
		result = 0x3b;
done:
	powerRequestDone(result);
}

/* 0x088f (sceSysconCtrlDeviceReset): data[1] the devices, data[0] 1 to hold them in reset */
static void powerRequestCtrlDeviceReset(void)
{
	unsigned char result;

	if (powerRequest.length == 2) {
		if (powerRequestData[0] < 2) {
			if (powerRequestData[1] & 0x01)
				;
#if BOARD_DOL1001
			/* DOL-1001 has none of the slim's three: the dummy byte, then P13.1 */
			if (powerRequestData[1] & 0x02)
				boardDummyPin = powerRequestData[0];
			if (powerRequestData[1] & 0x04)
				boardDummyPin = powerRequestData[0];
			if (powerRequestData[1] & 0x08)
				boardDummyPin = powerRequestData[0];
			if (powerRequestData[1] & 0x10)
				P13.1 = powerRequestData[0];
#elif SOURCE_0103
			if (powerRequestData[1] & 0x02)
				P6.5 = powerRequestData[0];
			if (powerRequestData[1] & 0x04)
				P4.2 = powerRequestData[0];
			if (powerRequestData[1] & 0x08)
				P4.1 = powerRequestData[0];
			if (powerRequestData[1] & 0x10)
				P0.5 = powerRequestData[0];
#else
			if (powerRequestData[1] & 0x02) {
#if BOARD_MOTION
				if (powerRequestData[0] != 0 && motionGetStatus() != 0xff) {
					P4.2 = 1;
					inputMotionInfoLater();
				}
#endif
				P6.3 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x04) {
#if BOARD_TOUCH_RESET
				if (powerRequestData[0] != 0)
					inputFlag80Later();
#endif
				P0.5 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x08) {
#if BOARD_TOUCH_RESET
				if (powerRequestData[0] != 0)
					inputFlag40Later();
#endif
				P0.6 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x10)
				P13.1 = powerRequestData[0];
#endif
		}
		result = 0;
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0890 (SceSysconForDriver_285594F8) */
#if SOURCE_0103
/* the slim's: no P3.3 to switch */
static void powerRequest0890(void)
{
	powerRequestDone(4);
}
#elif HW_IS(0x00315200) && !FW_SINCE(0x01000104)
/* 0x00315200 before 0x01000104: no P3.3 to switch (Sony's keeps the result variable, unused:
   its register is saved) */
static void powerRequest0890(void)
{
	unsigned char result;

	powerRequestDone(4);
}
#else
static void powerRequest0890(void)
{
	unsigned char result;

	if (!(getHardwareInfoFlags() & 0x02)) {
		powerRequestDone(4);
	} else {
		if (powerRequest.length == 1) {
			if (powerRequestData[0] != 0)
				P33 = 0;
			else
				P33 = 1;
			result = 0;
		} else
			result = 0x3b;
		powerRequestDone(result);
	}
}
#endif

/* 0x0891 (sceSysconCtrlLED) */
#if SOURCE_0103
static void powerRequestCtrlLED(void)
{
	unsigned char result = 0;
	unsigned int led;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x40) {
#if BOARD_DOL1001
			/* DOL-1001's power LED is the power jobs' */
			if (powerRequestData[0] & 0x80)
				;
			else
				;
#else
			if (powerRequestData[0] & 0x80)
				powerLedSetMode(POWER_LED_SET_OFF);
			else
				powerLedSetMode(POWER_LED_SET_ON);
#endif
		}
		if (powerRequestData[0] & 0x38) {
			led = elmoLed;
			if (powerRequestData[0] & 0x20) {
				if (powerRequestData[0] & 0x80)
					led |= 0x4000;
				else
					led &= ~0x4000;
			}
#if BOARD_DOL1001
			/* and its two more LEDs on the Elmo */
			if (powerRequestData[0] & 0x10) {
				if (powerRequestData[0] & 0x80)
					led |= 0x2000;
				else
					led &= ~0x2000;
			}
			if (powerRequestData[0] & 0x08) {
				if (powerRequestData[0] & 0x80)
					led |= 0x1000;
				else
					led &= ~0x1000;
			}
#endif
			elmoCommand[0] = 0xa5;
			elmoCommand[1] = ((unsigned char *)&led)[1];
			elmoCommand[2] = (unsigned char)led;
			result = elmoWrite(elmoCommand, 3);
			if (result == 0)
				elmoLed = led;
			result = powerResult(result);
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlLED(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x40) {
#if FW_SINCE(0x01000306)
			ledSetOff(0);
#endif
#if BOARD_POWER_LED_ELMO
			if (powerRequestData[0] & 0x80)
				elmoLed |= 0x8000;
			else
				elmoLed &= ~0x8000;
		}
		{
#elif LED_REQUESTS
			if (powerRequestData[0] & 0x80) {
				powerLedSetMode(POWER_LED_SET_OFF);
				result = 0;
			} else {
				powerLedSetMode(POWER_LED_SET_ON);
				result = 0;
			}
#else
			if (powerRequestData[0] & 0x80) {
				powerLedSetBrightness(0);
				result = powerResult(powerLedSetPwm(1));
			} else {
				powerLedSetBrightness(0x0f);
				result = powerResult(powerLedSetPwm(0));
			}
#endif
#if !BOARD_POWER_LED_ELMO
		} else {
#endif
			if (powerRequestData[0] & 0x20) {
				if (powerRequestData[0] & 0x80)
					elmoLed |= 0x4000;
				else
					elmoLed &= ~0x4000;
			}
			if (powerRequestData[0] & 0x10) {
				if (powerRequestData[0] & 0x80)
					elmoLed |= 0x2000;
				else
					elmoLed &= ~0x2000;
			}
			if (powerRequestData[0] & 0x08) {
				if (powerRequestData[0] & 0x80)
					elmoLed |= 0x1000;
				else
					elmoLed &= ~0x1000;
			}
			elmoCommand[0] = 0xa5;
			elmoCommand[1] = ((unsigned char *)&elmoLed)[1];
			elmoCommand[2] = (unsigned char)elmoLed;
			result = powerResult(elmoWrite(elmoCommand, 3));
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

#if SOURCE_0103 && !BOARD_DOL1001
/* 0x0892 (sceSysconCtrlChargeAC): the slim has no AC charge control, and answers 0 */
static void powerRequestCtrlChargeAC(void)
{
	unsigned char result = 0;

	powerRequestDone(result);
}
#elif !FW_SINCE(0x00090009)
/* 0x0892 (sceSysconCtrlChargeAC): 0x00080504 has no AC charge control, and answers 0 */
static void powerRequestCtrlChargeAC(void)
{
	powerRequestDone(0);
}
#else
/* 0x0892 (sceSysconCtrlChargeAC) */
static void powerRequestCtrlChargeAC(void)
{
	unsigned char result;
	unsigned char on;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0) {
#if FW_SINCE(0x00090804)
			chargeControl.1 = 1;
#endif
			on = 1;
		} else {
#if FW_SINCE(0x00090804)
			chargeControl.1 = 0;
#endif
			on = 0;
		}
		result = powerResult(cookieCtrlChargeAc(on));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x0893 (sceSysconCtrlChargeVBUS): data[1] 1 the first bit, 0 the second */
#if SOURCE_0103
static void powerRequestCtrlChargeVBUS(void)
{
	unsigned char result = 0;
	unsigned char host = (unsigned char)chargeVbus.1;
	unsigned char device = (unsigned char)chargeVbus.0;

	if (powerRequest.length < 3) {
		if (powerRequestData[0] != 0) {
			if (powerRequestData[1] == 1)
				chargeVbus.1 = 1;
			else if (powerRequestData[1] == 0)
				chargeVbus.0 = 1;
		} else {
			if (powerRequestData[1] == 1)
				chargeVbus.1 = 0;
			else if (powerRequestData[1] == 0)
				chargeVbus.0 = 0;
		}
		if (chargeVbus.1 != host || chargeVbus.0 != device) {
			result = powerResult(chargeSetCurrent());
			if (!chargeUsbState.7)
				task5SetFlag(4, 0x0020);
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlChargeVBUS(void)
{
	unsigned char result;

	if (powerRequest.length < 3) {
#if FW_SINCE(0x00090903)
		if (powerRequestData[0] != 0) {
			if (powerRequestData[1] == 1)
				chargeVbus.1 = 1;
			else if (powerRequestData[1] == 0)
				chargeVbus.0 = 1;
		} else {
			if (powerRequestData[1] == 1)
				chargeVbus.1 = 0;
			else if (powerRequestData[1] == 0)
				chargeVbus.0 = 0;
		}
		result = powerResult(chargeSetCurrent());
#else
		/* 0x00090804's: on or off, the Cookie's charge set at once */
		if (powerRequestData[0] != 0) {
			chargeVbus = 1;
			result = powerCookieClear();
		} else {
			chargeVbus = 0;
			result = powerCookieSet();
		}
		result = powerResult(result);
#endif
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x0894: sets (data[0] not 0) or clears the bits of data[1-8] in the Elmo's status masks */
static void powerRequest0894(void)
{
	unsigned char result;

	if (powerRequestData[0] != 0) {
		elmoStatusMask.byte[1] |= powerRequestData[1];
		elmoStatusMask.byte[0] |= powerRequestData[2];
		elmoStatusMask.byte[3] |= powerRequestData[3];
		elmoStatusMask.byte[5] |= powerRequestData[5];
		elmoStatusMask.byte[4] |= powerRequestData[6];
	} else {
		elmoStatusMask.byte[1] &= ~powerRequestData[1];
		elmoStatusMask.byte[0] &= ~powerRequestData[2];
		elmoStatusMask.byte[3] &= ~powerRequestData[3];
		elmoStatusMask.byte[5] &= ~powerRequestData[5];
		elmoStatusMask.byte[4] &= ~powerRequestData[6];
		elmoStatusMask.byte[7] &= ~powerRequestData[7];
		elmoStatusMask.byte[9] &= ~powerRequestData[8];
	}
#if SOURCE_0103 && !BOARD_DOL1001
	result = elmoWrite(elmoCommand, elmoGetMasks(0x0f, elmoCommand));
#else
	result = elmoWrite(elmoCommand, elmoGetMasks(0x1f, elmoCommand));
#endif
	result = powerResult(result);
	powerRequestDone(result);
}

/* 0x0895 (sceSysconReadElmoReg) */
static void powerRequestReadElmoReg(void)
{
	unsigned char result;
	unsigned char value[2];

	if (powerRequest.length < 3) {
		result = powerResult(elmoRead(powerRequestData, value));
		if (result == 0) {
			powerRequestAnswer[0] = value[1];
			powerRequestAnswer[1] = value[0];
		}
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x0896 (sceSysconWriteElmoReg): data[0] the register, data[1-2] the value, low byte first */
static void powerRequestWriteElmoReg(void)
{
	unsigned char result;
	unsigned char write[3];

	if (powerRequest.length == 3) {
		write[0] = powerRequestData[0];
		write[1] = powerRequestData[2];
		write[2] = powerRequestData[1];
		result = powerResult(elmoWrite(write, 3));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0897 (sceSysconReadCookieReg) */
static void powerRequestReadCookieReg(void)
{
	unsigned char result;
	unsigned char value[2];
#if FW_SINCE(0x00090804)
	unsigned char reg;
#endif

	if (powerRequest.length < 3) {
#if !FW_SINCE(0x00090804)
		/* the host's register number as it is */
		result = powerResult(cookieRead(powerRequestData, value));
#else
#if BOARD_DOL1001
		reg = cookieReg(powerRequestData[0]);
#elif BOARD_COOKIE_REG_HIGH
		reg = configGet()->cookieRegHigh ? powerRequestData[0] + 0xa0 : powerRequestData[0];
#else
		reg = powerRequestData[0];
#endif
		result = powerResult(cookieRead(&reg, value));
#endif
		if (result == 0) {
			powerRequestAnswer[0] = value[1];
			powerRequestAnswer[1] = value[0];
		}
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x0898 (sceSysconWriteCookieReg): registers 3-8 are kept in cookieRegs */
static void powerRequestWriteCookieReg(void)
{
	unsigned char result;
	unsigned char write[3];
	unsigned char index;

	if (powerRequest.length == 3) {
#if BOARD_DOL1001
		write[0] = cookieReg(powerRequestData[0]);
#elif BOARD_COOKIE_REG_HIGH && FW_SINCE(0x00090804)
		write[0] = configGet()->cookieRegHigh ? powerRequestData[0] + 0xa0 :
			   powerRequestData[0];
#else
		write[0] = powerRequestData[0];
#endif
		write[1] = powerRequestData[2];
		write[2] = powerRequestData[1];
		result = powerResult(cookieWrite(write, 3));
		if (result == 0) {
#if BOARD_DOL1001
			if (write[0] >= cookieReg(3) && write[0] <= cookieReg(8)) {
#elif FW_SINCE(0x00090804)
			if (write[0] >= COOKIE_REG(3) && write[0] <= COOKIE_REG(8)) {
#else
			/* registers 3 to 0x11, past cookieRegs' seven */
			if (write[0] >= 3 && write[0] < 0x12) {
#endif
#if BOARD_DOL1001
				index = write[0] - cookieReg(3);
#else
				index = write[0] - COOKIE_CTRL_REG(3);
#endif
				cookieRegs[index].byte[0] = write[1];
				cookieRegs[index].byte[1] = write[2];
			}
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
/* 0x08b0 (sceSysconBatteryStartBLMode, to version 0x70503) */
static void powerRequestBatteryStartBLMode(void)
{
	batteryBLLineOn();
	powerRequestDone(0);
	return;
}

/* 0x08c0 (sceSysconCtrlManualChargeMode): nothing to do */
#if SOURCE_0103
/* the slim's: the manual charge mode on or off; 0x3e when it cannot be */
static void powerRequestCtrlManualChargeMode(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		result = chargeSetManualMode(powerRequestData[0] != 0 ? 1 : 0);
		if (result == 0xff)
			result = 0x3e;
		else
			result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlManualChargeMode(void)
{
	powerRequestDone(0);
	return;
}
#endif

/* 0x08c1 (sceSysconCtrlBatteryCharge) */
#if BOARD_DOL1001
/* DOL-1001's 0x08c1-0x08c4: the slim's, in any charge mode */
static void powerRequestCtrlBatteryCharge(void)
{
	unsigned char result;
	unsigned char on;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			on = 1;
		else
			on = 0;
		result = powerResult(cookieCtrlBatteryCharge(on));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

static void powerRequestCtrlChargingCurrent(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		result = cookieChargingCurrentEncode(&current, powerRequestData[0]);
		if (result != 0)
			result = 0x3c;
		else
			result = powerResult(cookieCtrlChargingCurrent(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

static void powerRequestCtrlSupplyCurrentAc(void)
{
	unsigned char result;
	unsigned char current = 0;

	if (powerRequest.length == 1) {
		result = cookieSupplyCurrentEncode(&current, powerRequestData[0]);
		if (result != 0)
			result = 0x3c;
		else
			result = powerResult(cookieCtrlSupplyCurrent(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

static void powerRequestCtrlSupplyCurrentUsb(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		result = cookieSupplyCurrentUsbEncode(&current, powerRequestData[0]);
		if (result != 0)
			result = 0x3c;
		else
			result = powerResult(cookieCtrlSupplyCurrentUsb(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#else
#if SOURCE_0103
/* the slim's 0x08c1-0x08c4 and 0x08c7-0x08c8 run only in the manual charge mode, 0x3e otherwise */
static void powerRequestCtrlBatteryCharge(void)
{
	unsigned char result;
	unsigned char on;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1) {
			if (powerRequestData[0] != 0)
				on = 1;
			else
				on = 0;
			result = powerResult(cookieCtrlBatteryCharge(on));
		} else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlBatteryCharge(void)
{
	unsigned char result;
	unsigned char on;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			on = 1;
		else
			on = 0;
		result = powerResult(cookieCtrlBatteryCharge(on));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x08c2 (sceSysconCtrlChargingCurrent) */
#if SOURCE_0103
static void powerRequestCtrlChargingCurrent(void)
{
	unsigned char result;
	unsigned char current;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1) {
			result = cookieChargingCurrentEncode(&current, powerRequestData[0]);
			if (result != 0)
				result = 0x3c;
			else
				result = powerResult(cookieCtrlChargingCurrent(current));
		} else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlChargingCurrent(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(cookieCtrlChargingCurrent(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x08c3 (sceSysconCtrlSupplyCurrentAc) */
#if SOURCE_0103
static void powerRequestCtrlSupplyCurrentAc(void)
{
	unsigned char result;
	unsigned char current = 0;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1)
			result = 0;
		else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlSupplyCurrentAc(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(cookieCtrlSupplyCurrentAc(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* 0x08c4 (sceSysconCtrlSupplyCurrentUsb) */
#if SOURCE_0103
static void powerRequestCtrlSupplyCurrentUsb(void)
{
	unsigned char result;
	unsigned char current;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1) {
			result = cookieSupplyCurrentUsbEncode(&current, powerRequestData[0]);
			if (result != 0)
				result = 0x3c;
			else
				result = powerResult(cookieCtrlSupplyCurrentUsb(current));
		} else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}
#else
static void powerRequestCtrlSupplyCurrentUsb(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(cookieCtrlSupplyCurrentUsb(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif
#endif
#endif

#if SOURCE_0103
/* 0x08c6: the Cookie's register 1 */
static void powerRequest08C6(void)
{
	unsigned char result;
	unsigned char unused;
	unsigned char reg[4];

	if (powerRequest.length == 0) {
		result = cookieReadReg1(reg);
		result = powerResult(result);
		if (result == 0) {
			powerRequestAnswer[0] = reg[0];
			powerRequestAnswer[1] = reg[1];
			powerRequestAnswer[2] = reg[2];
			powerRequestAnswer[3] = reg[3];
		}
	} else
		result = 0x3b;
	powerRequestReply(result, 4);
}

#if BOARD_DOL1001
/* DOL-1001's 0x08c7: the Cookie's AC charge (cookieSetChargeAc), data[0] set on */
static void powerRequest08C7(void)
{
	unsigned char result;

	if (powerRequest.length == 1)
		result = powerResult(cookieSetChargeAc(powerRequestData[0] != 0 ? 1 : 0));
	else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08c8: the Cookie's register 3 bit 7 cleared (data[0] set) or set */
static void powerRequest08C8(void)
{
	unsigned char result;

	if (powerRequest.length == 1)
		result = powerResult(cookieCtrlReg6Bit3(powerRequestData[0] != 0 ? 1 : 0));
	else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08c9: P8.0's power on (data[0] set) or off */
static void powerRequest08C9(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerP80On();
		else
			result = powerP80Off();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08ca: the temperature sensor's local temperature */
static void powerRequest08CA(void)
{
	unsigned char result;
	signed char temperature;

	if (powerRequest.length == 0) {
		result = powerResult(thermalReadTemp(&temperature));
		if (result == 0)
			powerRequestAnswer[0] = temperature;
	} else
		result = 0x3b;
	powerRequestReply(result, 1);
}

/* 0x08cb: one of its registers, data[0] */
static void powerRequest08CB(void)
{
	unsigned char result;
	unsigned char reg;
	unsigned char value;

	if (powerRequest.length == 1) {
		reg = powerRequestData[0];
		result = powerResult(thermalRead(&reg, &value, 1));
		if (result == 0)
			powerRequestAnswer[0] = value;
	} else
		result = 0x3b;
	powerRequestReply(result, 1);
}

/* 0x08cc: data[1] written to its register data[0] */
static void powerRequest08CC(void)
{
	unsigned char result;
	unsigned char write[2];

	if (powerRequest.length == 2) {
		write[0] = powerRequestData[0];
		write[1] = powerRequestData[1];
		result = powerResult(thermalWrite(write, 2));
	} else
		result = 0x3b;
	powerRequestReply(result, 0);
}

/* 0x08cd: its temperature in kelvin */
static void powerRequest08CD(void)
{
	unsigned char result;

	if (powerRequest.length == 0)
		result = powerResult(thermalReadKelvin((unsigned int *)powerRequestAnswer));
	else
		result = 0x3b;
	powerRequestReply(result, 2);
}
#else
static void powerRequest08C7(void)
{
	unsigned char result;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1)
			result = 0;
		else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}

/* 0x08c8: the Cookie's register 6 bit 3 cleared (data[0] set) or set */
static void powerRequest08C8(void)
{
	unsigned char result;

	if (chargeGetManualMode() == 1) {
		if (powerRequest.length == 1)
			result = powerResult(cookieCtrlReg6Bit3(powerRequestData[0] != 0 ? 1 : 0));
		else
			result = 0x3b;
	} else
		result = 0x3e;
	powerRequestDone(result);
}

/* 0x08c9-0x08cd: not known, answered with a zero byte or two */
static void powerRequest08C9(void)
{
	unsigned char result;

	result = 0x3f;
	powerRequestReply(result, 0);
}

static void powerRequest08CA(void)
{
	unsigned char result;

	result = 0x3f;
	powerRequestReply(result, 1);
}

static void powerRequest08CB(void)
{
	unsigned char result;

	result = 0x3f;
	powerRequestReply(result, 1);
}

static void powerRequest08CC(void)
{
	unsigned char result;

	result = 0x3f;
	powerRequestReply(result, 0);
}

static void powerRequest08CD(void)
{
	unsigned char result;

	result = 0x3f;
	powerRequestReply(result, 2);
}
#endif
#endif

#if BOARD_DOL1001
/* 0x08ce: the temperature statistics (struct ThermalReport, 16 bytes) */
static void powerRequest08CE(void)
{
	unsigned char result;

	if (powerRequest.length == 0) {
		thermalStatsGet((struct ThermalReport *)powerRequestAnswer);
		result = 0;
	} else
		result = 0x3b;
	powerRequestReply(result, 0x10);
}

/* 0x08cf: the statistics data[0]'s bits name cleared */
static void powerRequest08CF(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		thermalStatsClear(powerRequestData[0]);
		result = 0;
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#endif

/* the Elmo's status registers 0xa0-0xa3 and 0xa4's high byte, high byte first */
void powerReadStatus(unsigned char *out)
{
	*out++ = elmoStatusMask.byte[1];
	*out++ = elmoStatusMask.byte[0];
	*out++ = elmoStatusMask.byte[3];
	*out++ = elmoStatusMask.byte[2];
	*out++ = elmoStatusMask.byte[5];
	*out++ = elmoStatusMask.byte[4];
	*out++ = elmoStatusMask.byte[7];
	*out++ = elmoStatusMask.byte[9];
}

#if !FW_SINCE(0x00090903)
/* 0x00090804's battery gauge commands on the power request (the battery request's later) */

/* 0x08a0: the battery's temperature */
static void powerRequest08A0(void)
{
	unsigned char result;

	if (powerRequest.length == 0)
		result = powerResult(batteryReadTemp((unsigned int *)powerRequestAnswer));
	else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a1: the gauge's register 0x3c */
static void powerRequest08A1(void)
{
	unsigned char result;
	unsigned char reg;

	if (powerRequest.length == 0) {
		reg = 0x3c;
		result = powerResult(batteryRead(&reg, powerRequestAnswer, 2));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a2: register 0x14 */
static void powerRequest08A2(void)
{
	unsigned char result;
	unsigned char reg;

	if (powerRequest.length == 0) {
		reg = 0x14;
		result = powerResult(batteryRead(&reg, powerRequestAnswer, 2));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a3: register 0x16 */
static void powerRequest08A3(void)
{
	unsigned char result;
	unsigned char reg;

	if (powerRequest.length == 0) {
		reg = 0x16;
		result = powerResult(batteryRead(&reg, powerRequestAnswer, 2));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a4: register 0x28 */
static void powerRequest08A4(void)
{
	unsigned char result;
	unsigned char reg;

	if (powerRequest.length == 0) {
		reg = 0x28;
		result = powerResult(batteryRead(&reg, powerRequestAnswer, 2));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a8: data[0] the register to read */
static void powerRequest08A8(void)
{
	unsigned char result;

	if (powerRequest.length == 2)
		result = powerResult(batteryRead(powerRequestData, powerRequestAnswer, 2));
	else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}

/* 0x08a9: data[0] the register, data[1-2] its value */
static void powerRequest08A9(void)
{
	unsigned char result;

	if (powerRequest.length == 4) {
		powerRequestData[1] = powerRequestData[0];
		result = powerResult(batteryWrite(&powerRequestData[1], 3));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 0);
}

/* 0x08aa: data[0] the control command */
static void powerRequest08AA(void)
{
	unsigned char result;
	unsigned int command;

	if (powerRequest.length == 2) {
		command = powerRequestData[0];
		result = powerResult(batteryControl(command, powerRequestAnswer, 2));
	} else
		result = 0x3b;
	POWER_REQUEST_ANSWER(result, 2);
}
#endif

/* 0x0900 (sceSysconGetBatteryRemainCap) */
unsigned char hostCommandBatteryRemainCap(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(charge.batteryRemainCap & 0xff);
	out[1] = (unsigned char)(charge.batteryRemainCap >> 8);
	*length = 2;
	return 0;
}

/* 0x0901 (sceSysconGetBatteryVolt) */
unsigned char hostCommandBatteryVolt(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(charge.batteryVolt & 0xff);
	out[1] = (unsigned char)(charge.batteryVolt >> 8);
	*length = 2;
	return 0;
}

#if FW_SINCE(0x00090009)	/* 0x00080504 has no 0x0902 */
/* 0x0902 (sceSysconGetBatteryLifePercent) */
unsigned char hostCommandBatteryLifePercent(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(charge.batteryLifePercent & 0xff);
	out[1] = (unsigned char)(charge.batteryLifePercent >> 8);
	*length = 2;
	return 0;
}
#endif

/* the battery's word for the host, or the setting's when no battery answered */
#if BOARD_DOL1001
#define BATTERY_ABSENT()	(deviceVersions.battery.id == 0xffff)	/* DOL-1001 tests the id */
#elif BOARD_BATTERY_STANDIN
#define BATTERY_ABSENT()	(deviceVersions.battery.version[0] == 0xffff)
#else
#define BATTERY_ABSENT()	0
#endif

/* the battery read, or the setting standing in: DOL-1001's source asks the battery first, the
   others test for none first (their compilers lay the two out in source order) */
#if BOARD_DOL1001
#define BATTERY_ANSWER(read, standin) \
	if (!BATTERY_ABSENT()) { read; } else { standin; }
#else
#define BATTERY_ANSWER(read, standin) \
	if (BATTERY_ABSENT()) { standin; } else { read; }
#endif

/* 0x0980 (sceSysconGetBatteryVersion) */
#if SOURCE_0103
static void batteryRequestGetVersion(void)
{
	unsigned char result;

	result = batteryResult(batteryReadInfo(&deviceVersions.battery.id));
	if (result == 0) {
		batteryRequestAnswer.word[0] = deviceVersions.battery.version[0];
		batteryRequestAnswer.word[1] = deviceVersions.battery.version[1];
		batteryRequestAnswer.word[2] = deviceVersions.battery.version[2];
	}
	batteryRequestReply(result, 6);
}
#else
static void batteryRequestGetVersion(void)
{
	batteryReadInfo(&deviceVersions.battery.id);
	batteryRequestAnswer.word[0] = deviceVersions.battery.version[0];
	batteryRequestAnswer.word[1] = deviceVersions.battery.version[1];
	batteryRequestAnswer.word[2] = deviceVersions.battery.version[2];
	BATTERY_REQUEST_ANSWER(0, 6);
}
#endif

/* 0x0981 (sceSysconGetBatteryTemp) */
static void batteryRequestGetTemp(void)
{
	unsigned char result;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			result = batteryResult(batteryReadTemp(batteryRequestAnswer.word)),
			batteryRequestAnswer.word[0] = configGet()->batteryTemp; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

#if FW_SINCE(0x00090206)
/* 0x098b (sceSysconGetBicTemp) */
static void batteryRequestGetBicTemp(void)
{
	unsigned char result;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			result = batteryResult(batteryReadBicTemp(batteryRequestAnswer.word)),
			batteryRequestAnswer.word[0] = configGet()->batteryBicTemp; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}
#endif

/* 0x0982 (sceSysconGetBatteryFullCap): the battery's register 0x3c */
static void batteryRequestGetFullCap(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x3c; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = configGet()->batteryFullCap; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x0983 (sceSysconGetBatteryElec): register 0x14 */
static void batteryRequestGetElec(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x14; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = configGet()->batteryElec; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x0984 (sceSysconGetBatteryTTE): register 0x16 */
static void batteryRequestGetTTE(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x16; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = configGet()->batteryTte; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x0985 (sceSysconGetBatterySOH): register 0x28 */
static void batteryRequestGetSOH(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x28; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = configGet()->batterySoh; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

#if FW_SINCE(0x00090009)	/* not in 0x00080504 */
/* 0x098a (sceSysconGetBatteryFullCap, the second): register 0x12 */
static void batteryRequestGetFullCap2(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x12; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = configGet()->batteryFullCap2; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}
#endif

#if FW_SINCE(0x00090903)
/* 0x098c (sceSysconGetBatteryCycleCount): register 0x2a (0x00090804 has not it, nor 0x098d) */
static void batteryRequestGetCycleCount(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		BATTERY_ANSWER(
			reg = 0x2a; result = batteryResult(batteryRead(&reg, batteryRequestAnswer.byte, 2)),
			batteryRequestAnswer.word[0] = 0; result = batteryResult(0));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x098d */
static void batteryRequest098D(void)
{
	unsigned char result;

	result = batteryResult(batteryControl0015());
	BATTERY_REQUEST_ANSWER(result, 0);
}
#endif

/* 0x0986 (sceSysconReadBatteryReg): data[0] the register */
static void batteryRequestReadReg(void)
{
	unsigned char result;

	if (batteryRequest.length == 2)
		result = batteryResult(batteryRead(batteryRequestData.byte, batteryRequestAnswer.byte,
						 2));
	else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x0987 (sceSysconWriteBatteryReg): data[0] the register, data[2-3] the value */
static void batteryRequestWriteReg(void)
{
	unsigned char result;

	if (batteryRequest.length == 4) {
		batteryRequestData.byte[1] = batteryRequestData.byte[0];
		result = batteryResult(batteryWrite(&batteryRequestData.byte[1], 3));
	} else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 0);
}

/* 0x0988 (SceSysconForDriver_A5AB19B1) */
static void batteryRequest0988(void)
{
	unsigned char result;

	if (batteryRequest.length == 2)
		result = batteryResult(batteryControl(batteryRequestData.word[0],
						 batteryRequestAnswer.byte, 2));
	else
		result = 0x3b;
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x0989 (sceSysconBatterySWReset) */
static void batteryRequestSWReset(void)
{
	unsigned char result;

	result = batteryResult(batterySWReset());
	BATTERY_REQUEST_ANSWER(result, 0);
}

/* 0x09b0 (sceSysconBatteryStartBLMode) */
static void batteryRequestStartBLMode(void)
{
	unsigned char result;

	result = batteryResult(batteryStartBLMode());
	BATTERY_REQUEST_ANSWER(result, 0);
}

/* 0x09b1 (sceSysconBatteryStopBLMode) */
static void batteryRequestStopBLMode(void)
{
	unsigned char result;

	result = batteryResult(batteryStopBLMode());
	BATTERY_REQUEST_ANSWER(result, 0);
}

/*
 * 0x09b2 (sceSysconBatterySetBLCommand): data[0-1] the command, data[2] the length of the
 * data from data[4], data[3] its offset in the command's buffer
 */
static void batteryRequestSetBLCommand(void)
{
	unsigned char result;
	unsigned char length;
	unsigned char offset;

	length = batteryRequestData.byte[2];
	offset = batteryRequestData.byte[3];
	if (length + 4 == batteryRequest.length) {
		batteryBLCommand.command = batteryRequestData.word[0];
		batteryBLCommand.length = offset + length;
		if (batteryBLCommand.length < BATTERY_BL_SIZE) {
			memcpy(batteryBLCommand.buffer + offset, &batteryRequestData.byte[4], length);
			result = 0;
		} else
			result = 0x3c;
	} else
		result = 0x3b;
	batteryRequestAnswer.word[0] = batteryRequestData.word[0];
	BATTERY_REQUEST_ANSWER(result, 2);
}

/* 0x09b3 (sceSysconBatteryExecBLCommand): data[0-1] the command set by 0x09b2 */
static void batteryRequestExecBLCommand(void)
{
	unsigned char result;
	unsigned int command;

	if (batteryRequest.length == 2) {
		command = batteryRequestData.word[0];
		if (batteryBLCommand.command == command)
			result = batteryResult(batteryWriteAt(batteryBLCommand.buffer,
							 batteryBLCommand.length));
		else
			result = 0x3c;
	} else
		result = 0x3b;
	batteryRequestAnswer.word[0] = batteryRequestData.word[0];
	BATTERY_REQUEST_ANSWER(result, 2);
}

/*
 * 0x09b4 (sceSysconBatteryReadBLCommand): data[0-1] the command, data[2] the length read,
 * data[3-4] what the read sends
 */
static void batteryRequestReadBLCommand(void)
{
	unsigned char result;

	if (batteryRequest.length == 5) {
		batteryBLCommand.command = batteryRequestData.word[0];
		batteryBLCommand.length = batteryRequestData.byte[2];
		if (batteryBLCommand.length < BATTERY_BL_SIZE)
			result = batteryResult(batteryReadAt(&batteryRequestData.byte[3],
							 &batteryRequestAnswer.byte[2],
							 batteryBLCommand.length));
		else
			result = 0x3c;
	} else
		result = 0x3b;
	batteryRequestAnswer.word[0] = batteryRequestData.word[0];
	BATTERY_REQUEST_ANSWER(result, batteryBLCommand.length + 2);
}

/* a device's result as the host's status: 1, 2 and 4 give 0x22, 0x10 gives 0x21, the rest 1 */
static unsigned char powerResult(unsigned char result)
{
	unsigned char status;

	switch (result) {
	case 0:
		status = 0;
		break;
	case 0x10:
		status = 0x21;
		break;
	case 1:
	case 2:
	case 4:
		status = 0x22;
		break;
	default:
		status = 1;
		break;
	}
	return status;
}

static unsigned char batteryResult(unsigned char result)
{
	unsigned char status;

	switch (result) {
	case 0:
		status = 0;
		break;
	case 0x10:
		status = 0x21;
		break;
	case 1:
	case 2:
	case 4:
		status = 0x22;
		break;
	default:
		status = 1;
		break;
	}
	return status;
}

#if BOARD_DOL1001
/* 0x08c5: the PS TV's USB power on (not 0) or off */
static void powerRequest08C5(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = powerUsbOn();
		else
			result = powerUsbOff();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}
#elif FW_SINCE(0x0100060B)
/* 0x08c5: refused (0x3e) */
static void powerRequest08C5(void)
{
	unsigned char status;

	status = 0x3e;
	powerRequestDone(status);
}
#endif
