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
#include "chip.h"
#include "config.h"
#include "host.h"
#include "input.h"
#include "kernel.h"
#include "motion.h"
#include "powerreq.h"
#include "version.h"

/* request 9's data and answer: the battery's registers are words */
union BatteryBuffer {
	unsigned char	byte[0x20];
	unsigned int	word[0x10];
};

/* the battery's identity, which the battery module reads (sub_2894D) */
struct BatteryInfo {
	unsigned int	id;
	unsigned int	version[3];	/* 0xffff: no battery answered */
};

/* a command to the battery's bootloader (0x09b2-0x09b4) */
struct BatteryBLCommand {
	unsigned int	command;
	unsigned char	length;
	unsigned char	*buffer;
};

#define BATTERY_BL_SIZE		0x63	/* the most the buffer holds */

/* a request's command and its handler */
struct PowerCommand {
	unsigned int	command;
	void		(*handler)(void);
};

extern unsigned char sub_2BB99(void);
extern unsigned char sub_2BBE9(void);
extern void sub_2C315(ID flgid, FLGPTN setptn);
extern unsigned char sub_2AB49(int transition);
extern void sub_21F33(unsigned int);
extern unsigned char sub_28243(unsigned char, unsigned char);
extern unsigned char sub_282AA(unsigned char, unsigned char);
extern unsigned char sub_2831A(unsigned char, unsigned char, unsigned char);
extern unsigned char sub_28373(unsigned char, unsigned char, unsigned char);
extern unsigned char sub_283D5(void);
extern unsigned char sub_2843A(void);
extern unsigned char sub_2849F(void);
extern unsigned char sub_28516(void);
extern unsigned char sub_27F89(void);
extern unsigned char sub_27FF3(void);
extern unsigned char sub_27B80(void);
extern unsigned char sub_27BE8(void);
extern unsigned char sub_27C59(void);
extern unsigned char sub_27CF1(void);
extern unsigned char sub_285AB(void);
extern unsigned char sub_28616(void);
extern unsigned char sub_29DDF(void);
extern void sub_2A2EC(unsigned char *status);
extern unsigned char sub_29B88(void);
extern unsigned char sub_11D57(void);
extern unsigned char sub_11D61(void);
extern void sub_11D6B(void);
extern void sub_11D73(void);
extern void sub_11D7B(void);
extern void sub_28235(void);
extern void sub_2A6A0(unsigned int);
extern unsigned char sub_28681(void);
extern unsigned char sub_2818A(void);
extern unsigned char sub_27EBD(void);
extern unsigned char sub_27F23(void);
extern unsigned char sub_28068(void);
extern unsigned char sub_280C7(void);
extern void sub_2E165(unsigned int);
extern unsigned char sub_2E8B4(void);
extern void sub_2E0A6(void);
extern void sub_2E0B5(void);
extern void sub_2E0D8(unsigned int);
extern void sub_2E0C4(unsigned int);
extern void sub_2DFF2(unsigned int);
extern void sub_2E016(unsigned int);
extern unsigned char sub_25224(unsigned char, unsigned char);
extern unsigned char sub_27D9E(unsigned char, unsigned char);
extern unsigned char sub_2489D(unsigned char *data, unsigned int length);
extern unsigned char sub_2A379(unsigned char);
extern unsigned char sub_25041(unsigned int, unsigned char *buffer);
extern unsigned char sub_24933(unsigned char *data, unsigned char *value);
extern unsigned char sub_29F40(unsigned char *reg, unsigned char *value);
extern unsigned char sub_29E85(unsigned char *data, unsigned int length);
extern void sub_28DE5(void);
extern void sub_2894D(struct BatteryInfo *info);
extern unsigned char sub_28ACF(unsigned int *temp);
extern unsigned char sub_28AF3(unsigned int *temp);
extern unsigned char sub_287ED(unsigned char *reg, unsigned char *data, unsigned int length);
extern unsigned char sub_28733(unsigned char *data, unsigned int length);
extern unsigned char sub_288C9(unsigned int reg, unsigned char *data, unsigned int length);
extern unsigned char sub_28DBE(void);
extern unsigned char sub_28E1A(void);
extern unsigned char sub_28CE1(void);
extern unsigned char sub_28D42(void);
extern unsigned char sub_28C24(unsigned char *buffer, unsigned int length);
extern unsigned char sub_28B55(unsigned char *data, unsigned char *answer, unsigned int length);
extern unsigned char sub_2A0ED(unsigned char);
extern unsigned char sub_2A029(unsigned char);
extern unsigned char sub_2A1EF(unsigned char);
extern unsigned char sub_2A191(unsigned char);

/* the power status (0x0800) and the USB state (0x0804, 0x089a), the power module's */
union PowerStatus {
	unsigned char	byte[2];
	struct {
		unsigned char	flags;
		unsigned char	usb:2;		/* the USB status's bits 2-3 (0x089a) */
		unsigned char	other:6;
	} bits;
};

extern __sreg union PowerStatus saddr_FFE54;
extern __sreg unsigned char saddr_FFE60;
extern __sreg unsigned char saddr_FFE61;
extern __sreg unsigned char saddr_FFE62;
extern __sreg unsigned char saddr_FFE57;
extern __sreg unsigned char saddr_FFE63;
extern unsigned int wordFF690;			/* the battery's remaining capacity */
extern unsigned int wordFF692;			/* and voltage */
extern unsigned char elmoVersion[2];		/* low byte first */
extern unsigned char cookieVersion[2];
extern unsigned char byteFF232[3];		/* the LED's command to the Elmo */
extern unsigned int wordFF300;			/* the LEDs' state */
extern unsigned char byteFF2F6[10];		/* the Elmo's status registers 0xa0-0xa4, high byte
						   first */
extern unsigned int wordFF698;			/* the battery's life in percent */
extern unsigned char byteFF6D6[6][2];		/* the Cookie's registers 3-8 as written */
extern struct BatteryInfo batteryInfo;
extern struct BatteryBLCommand batteryBLCommand;

void powerInputChanged(void);
void powerRequestNotify(void);
void batteryRequestNotify(void);
void powerRequestUnknown(void);
void powerRequestDone(unsigned char status);
static void powerRequestGetElmoVersion(void);
static void powerRequestGetCookieVersion(void);
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
static void powerRequestCtrlRMRPower(void);
static void powerRequestCtrlMultiCnPower(void);
static void powerRequestCtrlLedPwmBlink(void);
static void powerRequestSetChargeLedCtrl(void);
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
static void powerRequestBatteryStartBLMode(void);
static void powerRequestCtrlManualChargeMode(void);
static void powerRequestCtrlBatteryCharge(void);
static void powerRequestCtrlChargingCurrent(void);
static void powerRequestCtrlSupplyCurrentAc(void);
static void powerRequestCtrlSupplyCurrentUsb(void);
void powerReadStatus(unsigned char *out);
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
static void batteryRequestGetFullCap2(void);
static void batteryRequestGetBicTemp(void);
static void batteryRequestGetCycleCount(void);
static void batteryRequest098D(void);
static void batteryRequestStartBLMode(void);
static void batteryRequestStopBLMode(void);
static void batteryRequestSetBLCommand(void);
static void batteryRequestExecBLCommand(void);
static void batteryRequestReadBLCommand(void);
static unsigned char batteryResult(unsigned char result);

struct HostRequest powerRequest;		/* request 8 */
struct HostRequest batteryRequest;		/* request 9 */
static unsigned int powerCommand;		/* request 8's command, while it runs */
static unsigned char powerRequestData[10];
static unsigned char powerRequestAnswer[10];
struct InputHandler powerInputHandler;
static union BatteryBuffer batteryRequestData;
static union BatteryBuffer batteryRequestAnswer;

/* request 8's commands */
static const struct PowerCommand powerCommands[] = {
	{ 0x0880, powerRequestGetElmoVersion },
	{ 0x0881, powerRequestGetCookieVersion },
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
	{ 0x089b, powerRequestCtrlRMRPower },
	{ 0x089c, powerRequestCtrlMultiCnPower },
	{ 0x089d, powerRequestCtrlLedPwmBlink },
	{ 0x089e, powerRequestSetChargeLedCtrl },
	{ 0x08b0, powerRequestBatteryStartBLMode },
	{ 0x08c0, powerRequestCtrlManualChargeMode },
	{ 0x08c1, powerRequestCtrlBatteryCharge },
	{ 0x08c2, powerRequestCtrlChargingCurrent },
	{ 0x08c3, powerRequestCtrlSupplyCurrentAc },
	{ 0x08c4, powerRequestCtrlSupplyCurrentUsb },
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
	{ 0x098a, batteryRequestGetFullCap2 },
	{ 0x098b, batteryRequestGetBicTemp },
	{ 0x098c, batteryRequestGetCycleCount },
	{ 0x098d, batteryRequest098D },
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
	powerInputHandler.type = 0;
	powerInputHandler.edges = INPUT_EDGE_LOW | INPUT_EDGE_HIGH;
	powerInputHandler.handler = powerInputChanged;
}

/* the power input bit changed: event flag 4's 0x0040 */
void powerInputChanged(void)
{
	sub_2C315(4, 0x0040);
}

void powerRequestNotify(void)
{
	switch (powerRequest.event) {
	case HOST_EVENT_NONE:
		break;
	case HOST_EVENT_COMMAND:
		powerRequest.state = HOST_REQUEST_BUSY;
		powerCommand = powerRequest.command;
		sub_2C315(4, 0x0001);
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
		sub_2C315(4, 0x0100);
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

	if (!sub_2BB99()) {
		hostRequestAnswer(&powerRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		sub_2BBE9();
	} else {
		for (i = 0; i < sizeof(powerCommands) / sizeof(powerCommands[0]); i++) {
			if (powerCommands[i].command == powerCommand) {
				powerCommands[i].handler();
				return;
			}
		}
		hostRequestAnswer(&powerRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
	}
}

/* request 9's command, from batteryCommands */
void batteryRequestEvent(void)
{
	unsigned char i;

	if (!sub_2BB99()) {
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_ANSWER, 0x3e, 0);
		sub_2BBE9();
	} else {
		for (i = 0; i < sizeof(batteryCommands) / sizeof(batteryCommands[0]); i++) {
			if (batteryCommands[i].command == batteryRequest.command) {
				batteryCommands[i].handler();
				return;
			}
		}
		hostRequestAnswer(&batteryRequest, HOST_REQUEST_UNKNOWN, 0x3f, 0);
	}
}

/* 0x0800 (SceSysconForDriver_A2FE9BF9): the power status */
unsigned char hostCommandPowerStatus(unsigned char *out, unsigned char *length)
{
	dis_dsp();
	*out++ = saddr_FFE54.byte[0];
	*out = saddr_FFE54.byte[1];
	ena_dsp();
	*length = 2;
	return 0;
}

/* 0x0801: the system mode's transition 0x54 */
unsigned char hostCommand0801(unsigned char *out, unsigned char *length)
{
	sub_2AB49(0x54);
	*length = 0;
	return 0;
}

/* 0x0802 (sceSysconGetBatteryRemainCap, to version 0x70503) */
unsigned char hostCommandBatteryRemainCapOld(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(wordFF690 & 0xff);
	out[1] = (unsigned char)(wordFF690 >> 8);
	*length = 2;
	return 0;
}

/* 0x0803 (sceSysconGetBatteryVolt, to version 0x70503) */
unsigned char hostCommandBatteryVoltOld(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(wordFF692 & 0xff);
	out[1] = (unsigned char)(wordFF692 >> 8);
	*length = 2;
	return 0;
}

/* 0x0804: the USB state 4 */
unsigned char hostCommand0804(unsigned char *out, unsigned char *length)
{
	if (saddr_FFE60 != 0xff) {
		saddr_FFE60 = 0xff;
		saddr_FFE61 = 4;
		sub_21F33(0x1000);
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

/* 0x0880 (sceSysconGetElmoVersion) */
static void powerRequestGetElmoVersion(void)
{
	powerRequestAnswer[0] = elmoVersion[1];
	powerRequestAnswer[1] = elmoVersion[0];
	powerRequest.answerLength = 2;
	hostRequestAnswerReady(&powerRequest.state);
	powerRequest.status = 0;
}

/* 0x0881 (sceSysconGetCookieVersion) */
static void powerRequestGetCookieVersion(void)
{
	powerRequestAnswer[0] = cookieVersion[1];
	powerRequestAnswer[1] = cookieVersion[0];
	powerRequest.answerLength = 2;
	hostRequestAnswerReady(&powerRequest.state);
	powerRequest.status = 0;
}

/* 0x0883 (sceSysconGetPowerStatus) */
static void powerRequestGetPowerStatus(void)
{
	powerReadStatus(powerRequestAnswer);
	powerRequest.answerLength = 8;
	hostRequestAnswerReady(&powerRequest.state);
	powerRequest.status = 0;
}

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
			result = sub_28243(on1, on0);
		} else {
			if (powerRequestData[0] & 0x01)
				on0 = 1;
			if (powerRequestData[0] & 0x02)
				on1 = 1;
			result = sub_282AA(on1, on0);
		}
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0885 (SceSysconForDriver_79E6DD8B) */
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
			result = sub_2831A(on2, on1, on0);
		} else {
			if (powerRequestData[0] & 0x02)
				on2 = 1;
			result = sub_28373(on2, on1, on0);
		}
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0886 (sceSysconCtrlHdmiCecPower) */
static void powerRequestCtrlHdmiCecPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = sub_283D5();
		else
			result = sub_2843A();
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
			result = sub_2849F();
		else
			result = sub_28516();
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
			result = sub_27F89();
		else
			result = sub_27FF3();
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
			result = sub_27B80();
		else
			result = sub_27BE8();
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
			result = sub_27C59();
		else
			result = sub_27CF1();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x088b (SceSysconForDriver_5A614349): not on this syscon */
static void powerRequest088B(void)
{
	powerRequestDone(5);
}

/* 0x088c (SceSysconForDriver_B872E904): not on this syscon */
static void powerRequest088C(void)
{
	powerRequestDone(6);
}

/* 0x0899 (sceSysconCtrlHpremote, the wiki's guess) */
static void powerRequestCtrlHpremote(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = sub_285AB();
		else
			result = sub_28616();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x089a (sceSysconCtrlUsbStatus) */
static void powerRequestCtrlUsbStatus(void)
{
	unsigned char result;
	unsigned char status[3];

	if (powerRequest.length < 3) {
		saddr_FFE61 = powerRequestData[0];
		switch (saddr_FFE61) {
		case 3:
			saddr_FFE62 = 0;
			break;
		case 0:
		case 1:
		case 2:
		case 4:
			break;
		case 5:
			if (sub_29DDF() != 0)
				saddr_FFE62 = 0xff;
			break;
		}
		sub_2A2EC(status);
		dis_dsp();
		saddr_FFE54.bits.usb = status[1] >> 2;
		ena_dsp();
		result = powerResult(sub_29B88());
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x088d (SceSysconForDriver_DD16ABD9) */
static void powerRequest088D(void)
{
	unsigned char result;

	if (!(getHardwareInfoFlags() & 0x02)) {
		powerRequestDone(4);
	} else {
		if (powerRequest.length == 1) {
			if (powerRequestData[0] & 0x01) {
				if (powerRequestData[0] & 0x80) {
					if (sub_11D57() == 1) {
						sub_28235();
						sub_2A6A0(500);
					}
				}
				result = sub_28681();
			} else {
				result = sub_2818A();
				if (powerRequestData[0] & 0x80) {
					if (sub_11D57() == 1)
						sub_28235();
					if (result != 0 && sub_11D61() == 0)
						result = 0;
				}
			}
		} else
			result = 0x3b;
		powerRequestDone(result);
	}
}

/* 0x089b (sceSysconCtrlRMRPower) */
static void powerRequestCtrlRMRPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = sub_27EBD();
		else
			result = sub_27F23();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x089c (sceSysconCtrlMultiCnPower) */
static void powerRequestCtrlMultiCnPower(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			result = sub_28068();
		else
			result = sub_280C7();
		result = powerResult(result);
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/*
 * 0x089d (sceSysconCtrlLedPwmBlink). With data[1] 0x80 and the LED module busy, the LED
 * module answers when it is done (powerRequestLedDone).
 */
static void powerRequestCtrlLedPwmBlink(void)
{
	unsigned char result = 0;

	if (powerRequest.length == 2) {
		switch (powerRequestData[0]) {
		case 1:
			sub_2E165(0);
			switch (powerRequestData[1] & 0x0f) {
			case 0:
				if (powerRequestData[1] & 0x80) {
					if (sub_2E8B4() & 0x1c) {
						if (powerRequestData[1] & 0x40)
							sub_2E0A6();
						else
							sub_2E0B5();
						return;
					}
				} else {
					if (powerRequestData[1] & 0x40)
						sub_2E0D8(1);
					else
						sub_2E0D8(2);
				}
				break;
			case 1:
				sub_2E0D8(4);
				break;
			case 2:
				sub_2E0D8(8);
				break;
			case 3:
				sub_2E0D8(0x10);
				break;
			default:
				result = 3;
				break;
			}
			if (powerRequestData[1] & 0x20)
				sub_2E0C4(1);
			else
				sub_2E0C4(0);
			break;
		default:
			result = 3;
			break;
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* the LED module is done: answers 0x089d */
void powerRequestLedDone(void)
{
	if (powerCommand == 0x089d && powerRequest.state == HOST_REQUEST_BUSY)
		powerRequestDone(0);
}

/* 0x089e (sceSysconSetChargeLedCtrl) */
static void powerRequestSetChargeLedCtrl(void)
{
	unsigned char result = 0;

	if (powerRequest.length == 1) {
		switch (powerRequestData[0]) {
		case 0:
			sub_2DFF2(0);
			sub_2E016(1);
			break;
		case 1:
			sub_2DFF2(0);
			sub_2E016(0);
			break;
		case 2:
			if (sub_2C658()->chargeLedHost) {
				sub_2E016(0);
				sub_2DFF2(1);
			}
			break;
		default:
			result = 3;
			break;
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

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
		if (powerRequestData[1] >= 0x40) {
			result = 3;
		} else {
			trim = sub_2C658()->voltageTrim;
			level = sub_25224(powerRequestData[1], trim);
			result = powerResult(sub_27D9E(regulator, level));
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
			if (powerRequestData[1] & 0x02) {
				if (powerRequestData[0] != 0 && motionGetStatus() != 0xff) {
					P4.2 = 1;
					sub_11D6B();
				}
				P6.3 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x04) {
				if (powerRequestData[0] != 0)
					sub_11D7B();
				P0.5 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x08) {
				if (powerRequestData[0] != 0)
					sub_11D73();
				P0.6 = powerRequestData[0];
			}
			if (powerRequestData[1] & 0x10)
				P13.1 = powerRequestData[0];
		}
		result = 0;
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0890 (SceSysconForDriver_285594F8) */
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

/* 0x0891 (sceSysconCtrlLED) */
static void powerRequestCtrlLED(void)
{
	unsigned char result;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] & 0x40) {
			sub_2E165(0);
			if (powerRequestData[0] & 0x80) {
				sub_2E0D8(1);
				result = 0;
			} else {
				sub_2E0D8(2);
				result = 0;
			}
		} else {
			if (powerRequestData[0] & 0x20) {
				if (powerRequestData[0] & 0x80)
					wordFF300 |= 0x4000;
				else
					wordFF300 &= ~0x4000;
			}
			if (powerRequestData[0] & 0x10) {
				if (powerRequestData[0] & 0x80)
					wordFF300 |= 0x2000;
				else
					wordFF300 &= ~0x2000;
			}
			if (powerRequestData[0] & 0x08) {
				if (powerRequestData[0] & 0x80)
					wordFF300 |= 0x1000;
				else
					wordFF300 &= ~0x1000;
			}
			byteFF232[0] = 0xa5;
			byteFF232[1] = ((unsigned char *)&wordFF300)[1];
			byteFF232[2] = (unsigned char)wordFF300;
			result = powerResult(sub_2489D(byteFF232, 3));
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0892 (sceSysconCtrlChargeAC) */
static void powerRequestCtrlChargeAC(void)
{
	unsigned char result;
	unsigned char on;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0) {
			saddr_FFE57.1 = 1;
			on = 1;
		} else {
			saddr_FFE57.1 = 0;
			on = 0;
		}
		result = powerResult(sub_2A379(on));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0893 (sceSysconCtrlChargeVBUS): data[1] 1 the first bit, 0 the second */
static void powerRequestCtrlChargeVBUS(void)
{
	unsigned char result;

	if (powerRequest.length < 3) {
		if (powerRequestData[0] != 0) {
			if (powerRequestData[1] == 1)
				saddr_FFE63.1 = 1;
			else if (powerRequestData[1] == 0)
				saddr_FFE63.0 = 1;
		} else {
			if (powerRequestData[1] == 1)
				saddr_FFE63.1 = 0;
			else if (powerRequestData[1] == 0)
				saddr_FFE63.0 = 0;
		}
		result = powerResult(sub_29B88());
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x0894: sets (data[0] not 0) or clears the bits of data[1-8] in the Elmo's status masks */
static void powerRequest0894(void)
{
	unsigned char result;

	if (powerRequestData[0] != 0) {
		byteFF2F6[1] |= powerRequestData[1];
		byteFF2F6[0] |= powerRequestData[2];
		byteFF2F6[3] |= powerRequestData[3];
		byteFF2F6[5] |= powerRequestData[5];
		byteFF2F6[4] |= powerRequestData[6];
	} else {
		byteFF2F6[1] &= ~powerRequestData[1];
		byteFF2F6[0] &= ~powerRequestData[2];
		byteFF2F6[3] &= ~powerRequestData[3];
		byteFF2F6[5] &= ~powerRequestData[5];
		byteFF2F6[4] &= ~powerRequestData[6];
		byteFF2F6[7] &= ~powerRequestData[7];
		byteFF2F6[9] &= ~powerRequestData[8];
	}
	result = sub_2489D(byteFF232, sub_25041(0x1f, byteFF232));
	result = powerResult(result);
	powerRequestDone(result);
}

/* 0x0895 (sceSysconReadElmoReg) */
static void powerRequestReadElmoReg(void)
{
	unsigned char result;
	unsigned char value[2];

	if (powerRequest.length < 3) {
		result = powerResult(sub_24933(powerRequestData, value));
		if (result == 0) {
			powerRequestAnswer[0] = value[1];
			powerRequestAnswer[1] = value[0];
		}
	} else
		result = 0x3b;
	powerRequest.answerLength = 2;
	hostRequestAnswerReady(&powerRequest.state);
	powerRequest.status = result;
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
		result = powerResult(sub_2489D(write, 3));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* the Cookie's register numbers: 0xa0 on with the setting 0xd4 */
#define COOKIE_REG(reg)		((unsigned char)(sub_2C658()->cookieRegHigh ? (reg) + 0xa0 : (reg)))

/* 0x0897 (sceSysconReadCookieReg) */
static void powerRequestReadCookieReg(void)
{
	unsigned char result;
	unsigned char value[2];
	unsigned char reg;

	if (powerRequest.length < 3) {
		reg = sub_2C658()->cookieRegHigh ? powerRequestData[0] + 0xa0 : powerRequestData[0];
		result = powerResult(sub_29F40(&reg, value));
		if (result == 0) {
			powerRequestAnswer[0] = value[1];
			powerRequestAnswer[1] = value[0];
		}
	} else
		result = 0x3b;
	powerRequest.answerLength = 2;
	hostRequestAnswerReady(&powerRequest.state);
	powerRequest.status = result;
}

/* 0x0898 (sceSysconWriteCookieReg): registers 3-8 are kept in byteFF6D6 */
static void powerRequestWriteCookieReg(void)
{
	unsigned char result;
	unsigned char write[3];
	unsigned char index;

	if (powerRequest.length == 3) {
		write[0] = sub_2C658()->cookieRegHigh ? powerRequestData[0] + 0xa0 :
			   powerRequestData[0];
		write[1] = powerRequestData[2];
		write[2] = powerRequestData[1];
		result = powerResult(sub_29E85(write, 3));
		if (result == 0) {
			if (write[0] >= COOKIE_REG(3) && write[0] <= COOKIE_REG(8)) {
				index = write[0] - COOKIE_REG(3);
				byteFF6D6[index][0] = write[1];
				byteFF6D6[index][1] = write[2];
			}
		}
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08b0 (sceSysconBatteryStartBLMode, to version 0x70503) */
static void powerRequestBatteryStartBLMode(void)
{
	sub_28DE5();
	powerRequestDone(0);
}

/* 0x08c0 (sceSysconCtrlManualChargeMode): nothing to do */
static void powerRequestCtrlManualChargeMode(void)
{
	powerRequestDone(0);
}

/* 0x08c1 (sceSysconCtrlBatteryCharge) */
static void powerRequestCtrlBatteryCharge(void)
{
	unsigned char result;
	unsigned char on;

	if (powerRequest.length == 1) {
		if (powerRequestData[0] != 0)
			on = 1;
		else
			on = 0;
		result = powerResult(sub_2A0ED(on));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08c2 (sceSysconCtrlChargingCurrent) */
static void powerRequestCtrlChargingCurrent(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(sub_2A029(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08c3 (sceSysconCtrlSupplyCurrentAc) */
static void powerRequestCtrlSupplyCurrentAc(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(sub_2A1EF(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* 0x08c4 (sceSysconCtrlSupplyCurrentUsb) */
static void powerRequestCtrlSupplyCurrentUsb(void)
{
	unsigned char result;
	unsigned char current;

	if (powerRequest.length == 1) {
		current = powerRequestData[0] << 4;
		result = powerResult(sub_2A191(current));
	} else
		result = 0x3b;
	powerRequestDone(result);
}

/* the Elmo's status registers 0xa0-0xa3 and 0xa4's high byte, high byte first */
void powerReadStatus(unsigned char *out)
{
	*out++ = byteFF2F6[1];
	*out++ = byteFF2F6[0];
	*out++ = byteFF2F6[3];
	*out++ = byteFF2F6[2];
	*out++ = byteFF2F6[5];
	*out++ = byteFF2F6[4];
	*out++ = byteFF2F6[7];
	*out++ = byteFF2F6[9];
}

/* 0x0900 (sceSysconGetBatteryRemainCap) */
unsigned char hostCommandBatteryRemainCap(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(wordFF690 & 0xff);
	out[1] = (unsigned char)(wordFF690 >> 8);
	*length = 2;
	return 0;
}

/* 0x0901 (sceSysconGetBatteryVolt) */
unsigned char hostCommandBatteryVolt(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(wordFF692 & 0xff);
	out[1] = (unsigned char)(wordFF692 >> 8);
	*length = 2;
	return 0;
}

/* 0x0902 (sceSysconGetBatteryLifePercent) */
unsigned char hostCommandBatteryLifePercent(unsigned char *out, unsigned char *length)
{
	out[0] = (unsigned char)(wordFF698 & 0xff);
	out[1] = (unsigned char)(wordFF698 >> 8);
	*length = 2;
	return 0;
}

/* the battery's word for the host, or the setting's when no battery answered */
#define BATTERY_ABSENT()	(batteryInfo.version[0] == 0xffff)

/* 0x0980 (sceSysconGetBatteryVersion) */
static void batteryRequestGetVersion(void)
{
	sub_2894D(&batteryInfo);
	batteryRequestAnswer.word[0] = batteryInfo.version[0];
	batteryRequestAnswer.word[1] = batteryInfo.version[1];
	batteryRequestAnswer.word[2] = batteryInfo.version[2];
	batteryRequest.answerLength = 6;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = 0;
}

/* 0x0981 (sceSysconGetBatteryTemp) */
static void batteryRequestGetTemp(void)
{
	unsigned char result;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryTemp;
			result = batteryResult(0);
		} else
			result = batteryResult(sub_28ACF(batteryRequestAnswer.word));
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x098b (sceSysconGetBicTemp) */
static void batteryRequestGetBicTemp(void)
{
	unsigned char result;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryBicTemp;
			result = batteryResult(0);
		} else
			result = batteryResult(sub_28AF3(batteryRequestAnswer.word));
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0982 (sceSysconGetBatteryFullCap): the battery's register 0x3c */
static void batteryRequestGetFullCap(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryFullCap;
			result = batteryResult(0);
		} else {
			reg = 0x3c;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0983 (sceSysconGetBatteryElec): register 0x14 */
static void batteryRequestGetElec(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryElec;
			result = batteryResult(0);
		} else {
			reg = 0x14;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0984 (sceSysconGetBatteryTTE): register 0x16 */
static void batteryRequestGetTTE(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryTte;
			result = batteryResult(0);
		} else {
			reg = 0x16;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0985 (sceSysconGetBatterySOH): register 0x28 */
static void batteryRequestGetSOH(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batterySoh;
			result = batteryResult(0);
		} else {
			reg = 0x28;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x098a (sceSysconGetBatteryFullCap, the second): register 0x12 */
static void batteryRequestGetFullCap2(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = sub_2C658()->batteryFullCap2;
			result = batteryResult(0);
		} else {
			reg = 0x12;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x098c (sceSysconGetBatteryCycleCount): register 0x2a */
static void batteryRequestGetCycleCount(void)
{
	unsigned char result;
	unsigned char reg;

	if (batteryRequest.length == 0) {
		if (BATTERY_ABSENT()) {
			batteryRequestAnswer.word[0] = 0;
			result = batteryResult(0);
		} else {
			reg = 0x2a;
			result = batteryResult(sub_287ED(&reg, batteryRequestAnswer.byte, 2));
		}
	} else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x098d */
static void batteryRequest098D(void)
{
	unsigned char result;

	result = batteryResult(sub_28E1A());
	batteryRequest.answerLength = 0;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0986 (sceSysconReadBatteryReg): data[0] the register */
static void batteryRequestReadReg(void)
{
	unsigned char result;

	if (batteryRequest.length == 2)
		result = batteryResult(sub_287ED(batteryRequestData.byte, batteryRequestAnswer.byte,
						 2));
	else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0987 (sceSysconWriteBatteryReg): data[0] the register, data[2-3] the value */
static void batteryRequestWriteReg(void)
{
	unsigned char result;

	if (batteryRequest.length == 4) {
		batteryRequestData.byte[1] = batteryRequestData.byte[0];
		result = batteryResult(sub_28733(&batteryRequestData.byte[1], 3));
	} else
		result = 0x3b;
	batteryRequest.answerLength = 0;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0988 (SceSysconForDriver_A5AB19B1) */
static void batteryRequest0988(void)
{
	unsigned char result;

	if (batteryRequest.length == 2)
		result = batteryResult(sub_288C9(batteryRequestData.word[0],
						 batteryRequestAnswer.byte, 2));
	else
		result = 0x3b;
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x0989 (sceSysconBatterySWReset) */
static void batteryRequestSWReset(void)
{
	unsigned char result;

	result = batteryResult(sub_28DBE());
	batteryRequest.answerLength = 0;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x09b0 (sceSysconBatteryStartBLMode) */
static void batteryRequestStartBLMode(void)
{
	unsigned char result;

	result = batteryResult(sub_28CE1());
	batteryRequest.answerLength = 0;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x09b1 (sceSysconBatteryStopBLMode) */
static void batteryRequestStopBLMode(void)
{
	unsigned char result;

	result = batteryResult(sub_28D42());
	batteryRequest.answerLength = 0;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
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
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
}

/* 0x09b3 (sceSysconBatteryExecBLCommand): data[0-1] the command set by 0x09b2 */
static void batteryRequestExecBLCommand(void)
{
	unsigned char result;
	unsigned int command;

	if (batteryRequest.length == 2) {
		command = batteryRequestData.word[0];
		if (batteryBLCommand.command == command)
			result = batteryResult(sub_28C24(batteryBLCommand.buffer,
							 batteryBLCommand.length));
		else
			result = 0x3c;
	} else
		result = 0x3b;
	batteryRequestAnswer.word[0] = batteryRequestData.word[0];
	batteryRequest.answerLength = 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
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
			result = batteryResult(sub_28B55(&batteryRequestData.byte[3],
							 &batteryRequestAnswer.byte[2],
							 batteryBLCommand.length));
		else
			result = 0x3c;
	} else
		result = 0x3b;
	batteryRequestAnswer.word[0] = batteryRequestData.word[0];
	batteryRequest.answerLength = batteryBLCommand.length + 2;
	hostRequestAnswerReady(&batteryRequest.state);
	batteryRequest.status = result;
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
