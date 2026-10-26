/*
 * The update's commands in the updater, and the dispatch of the host's. The host sends the
 * application's image a segment (a 1 KiB flash block) at a time: 0x1180 the segment, 0x1181
 * its data in pieces, 0x1182 its programming (the data AES-128-CBC decrypted with the PS Dev
 * Wiki's "Ernie Update AES128CBC Key" for the PHAT boards, then written), then 0x1184 the
 * SHA-1 of every segment programmed, checked from the main loop, and 0x1183 the reboot. A
 * mode the application started (handshakeUnlocked11) takes every command; the others only those an
 * update needs.
 */
#pragma sfr
#pragma EI
#pragma NOP

#include <string.h>
#include "chip.h"
#include "ernie.h"
#include "battery.h"
#include "board.h"
#include "charge.h"
#include "config.h"
#include "elmo.h"
#include "keepram.h"
#include "nvs.h"
#include "powerseq.h"
#include "scratchpad.h"
#include "serial.h"
#include "serial1.h"
#include "sleepfactor.h"
#include "watchdog.h"
#include "aes.h"
#include "sha1.h"
#include "version.h"
#include "updater.h"

/* the flash self-programming library (ernie/lib) */
extern void FSL_Open(void);
extern unsigned char FSL_Init(unsigned char *descriptor);
extern unsigned char FSL_Init_cont(unsigned char *descriptor);
extern unsigned char FSL_ModeCheck(void);
extern unsigned char FSL_BlankCheck(unsigned char block);
extern unsigned char FSL_Erase(unsigned char block);
extern unsigned char FSL_IVerify(unsigned int block);
extern unsigned char FSL_Write(unsigned long address, unsigned char words);
extern unsigned char fslSetSecurityFlagSwap(void);
extern void FSL_Close(void);
extern unsigned char fslDataBuffer[];		/* the library's data buffer (main.c) */

/* 0x0090 and 0x0091's data: where in the scratch pad, and how many bytes */
struct ScratchPadRange {
	unsigned int	offset;
	unsigned char	size;
	unsigned char	data;		/* 0x0091's first byte */
};

int kermitCmdNoImprement(struct HostSlowCommand *slow);
int kermitCmdNoService(struct HostSlowCommand *slow);
int kermitCmdChecksumError(struct HostSlowCommand *slow);
int updaterSetSegment(struct HostSlowCommand *slow);
int updaterWriteData(struct HostSlowCommand *slow);
int updaterExecProgramming(struct HostSlowCommand *slow);
int updaterExecFinalize(struct HostSlowCommand *slow);
int updaterReboot(struct HostSlowCommand *slow);
int hostCommandErnieDlVersion(void);
int hostCommandUpdaterProgress(void);
int hostCommandNull(void);
int hostCommandErnieVersion(void);
int hostCommandTimeStamp(void);
int hostCommandSleepFactor(void);
int hostCommandRequests(void);
int hostCommandHardwareInfo(void);
int hostCommandWakeupFactor(void);
int hostCommandClock(void);
int hostCommand0013(void);
int hostCommandControlsInfo(void);
int hostCommandSetLinkConfig(struct HostSlowCommand *slow);
int hostCommandReadScratchPad(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandWriteScratchPad(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandWatchdogStart(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandWatchdogStop(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandWatchdogKick(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandHandshake(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlHostOutputViaDongle(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandTransition(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandErnieShutdown(struct HostSlowCommand *slow);
int UiSetMultiCnPort(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlWirelessPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int powerCtrlDeviceReset(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int ctrlVoltage(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int powerCtrlLED(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlSdPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlAccPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlRMRPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandCtrlMultiCnPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandPowerStatus(void);
int hostCommandBatteryRemainCap(void);
int hostCommandBatteryVolt(void);
int hostCommandBatteryLifePercent(void);
int hostCommandBatteryGetFullCap(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandBatteryGetFullCap2(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandNvs1080(struct HostSlowCommand *slow);
int hostCommandNvsFormat(struct HostSlowCommand *slow);
int hostCommandNvsReadData(struct HostSlowCommand *slow);
int hostCommandNvsWriteData(struct HostSlowCommand *slow);
int hostCommandSerialUnlock(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandSerialLock(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandSerialRestart(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandSerialRead(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandSerialWrite(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
int hostCommandSerialWriteOrSend(struct HostCommandFrame *rx, struct HostSlowCommand *slow);
unsigned char powerResult(unsigned char result);
unsigned char batteryResult(unsigned char result);
int flSetSegment(unsigned char segment);
unsigned long flChecksum(void);
void flImageCheck(struct HostSlowCommand *slow);
int flHashCheck(unsigned char *digest);
int flDecrypt(void);
int flProg(void);
void flReboot(void);
int flBootClusterSwap(void);

/* the PS Dev Wiki's "Ernie Update AES128CBC Key 0x10/0x30/0x31/0x40/0x41 (PHAT)" */
const unsigned char updaterCryptoKey[16] = {
	0x12, 0xb5, 0x40, 0x8f, 0xd1, 0x89, 0xe2, 0x23,
	0xb6, 0x18, 0x90, 0xf4, 0x88, 0x53, 0x60, 0x08,
};

const unsigned char updaterCryptoIv[AES_BLOCK_SIZE] = {
	0x82, 0xd6, 0x52, 0x8a, 0x87, 0xbc, 0x55, 0xb3,
	0x8e, 0xf2, 0x9a, 0x45, 0x73, 0x0e, 0xf1, 0x30,
};

/* the updater's state, in one block as Sony's image has it (updater.lk) */
unsigned char updMode;		/* updModeChange's */
unsigned char updReset;		/* started from the reset vector (main), not by the application */
unsigned char updVersion[4];	/* the running application's version */
char updBuild[12];		/* and its build time, digits only */
unsigned char updImageChecked;		/* 0x1184 found the image's digest right */
unsigned char updProgrammed;
unsigned char updSegment;
unsigned char updPlain[0x400];		/* the segment decrypted */
unsigned char updCipher[0x400];
unsigned char updReceived[0x100];
SHA1Context updSha;
unsigned char updHash[SHA1HashSize];
unsigned long updShaW[SHA1WordsSize];
unsigned char updShaBlock[SHA1BlockSize];
unsigned long updRoundKeys[44];		/* AES-128: (nr + 1) * nb words */
unsigned char updIv[AES_BLOCK_SIZE];
unsigned long updLoops;		/* the main loop's rounds */
union UpdEvents updEvents;
unsigned int buttonPowerHeld;	/* the polls P11.5 has been held low */
unsigned int buttonPsHeld;	/* and P4.6 */
unsigned long buttonPolls;
unsigned int buttonShortTime;	/* buttonPoll's times, in polls */
unsigned int buttonHoldTime1;
unsigned int buttonHoldTime2;
struct AesKey updKey;
union UpdInput updInput;
unsigned char updProgress;
char logBuffer[0x80];		/* logPrintf's */

unsigned char updaterInit(void)
{
	updImageChecked = 0;
	buttonPowerHeld = 0;
	buttonPsHeld = 0;
	buttonPolls = 0;
	updInput.value = 0;
	updEvents.value = 0;
	updSegment = 4;
	updLoops = 0;
	return 0;
}

/* expands the update key, copies its IV and resets the image's digest */
int updaterDecryptInit(void)
{
	unsigned char result;

	memset(updHash, 0, sizeof(updHash));
	result = (unsigned char)SHA1Reset((SHA1Context __near *)&updSha,
					  (unsigned char __near *)updShaBlock,
					  (unsigned long __near *)updShaW);
	if (result != 0)
		return 1;
	updKey.w = updRoundKeys;
	updKey.nb = 4;
	updKey.nk = 4;
	result = (unsigned char)aesKeyExpansion((const unsigned long *)updaterCryptoKey,
						(struct AesKey __near *)&updKey);
	memcpy(updIv, updaterCryptoIv, AES_BLOCK_SIZE);
	memset(updReceived, 0, sizeof(updReceived));
	updProgrammed = 0;
	updProgress = 0;
}

/* the fast commands (bit 7 clear), answered in the same frame */
unsigned char hostCommandFast(unsigned int command)
{
	unsigned char result;

	if (!handshakeUnlocked11()) {
		switch (command) {
		case 0x0000:
			result = hostCommandNull();
			break;
		case 0x0001:
			result = hostCommandErnieVersion();
			break;
		case 0x0002:
			result = hostCommandTimeStamp();
			break;
		case 0x0004:
			result = hostCommandRequests();
			break;
		case 0x0005:
			result = hostCommandHardwareInfo();
			break;
		case 0x0010:
			result = hostCommandWakeupFactor();
			break;
		case 0x0013:
			result = hostCommand0013();
			break;
		case 0x1100:
			result = hostCommandErnieDlVersion();
			break;
		default:
			result = 0x3e;
			updEventSet(0x16);
			break;
		}
		return result;
	}
	switch (command) {
	case 0x0000:
		result = hostCommandNull();
		break;
	case 0x0001:
		result = hostCommandErnieVersion();
		break;
	case 0x0002:
		result = hostCommandTimeStamp();
		break;
	case 0x0003:
		result = hostCommandSleepFactor();
		break;
	case 0x0004:
		result = hostCommandRequests();
		break;
	case 0x0005:
		result = hostCommandHardwareInfo();
		break;
	case 0x0010:
		result = hostCommandWakeupFactor();
		break;
	case 0x0011:
		result = hostCommandClock();
		break;
	case 0x0013:
		result = hostCommand0013();
		break;
	case 0x0800:
		result = hostCommandPowerStatus();
		break;
	case 0x0900:
		result = hostCommandBatteryRemainCap();
		break;
	case 0x0901:
		result = hostCommandBatteryVolt();
		break;
	case 0x0902:
		result = hostCommandBatteryLifePercent();
		break;
	case 0x1100:
		result = hostCommandErnieDlVersion();
		break;
	case 0x1101:
		result = hostCommandUpdaterProgress();
		break;
	case 0x0100:
		result = hostCommandControlsInfo();
		break;
	default:
		result = 0x3f;
		break;
	}
	return result;
}

/* a queued command (bit 7 set), from its frame; 0x14 when its handler runs from the main loop */
unsigned char hostCommandSlow(unsigned int command, struct HostSlowCommand *slow)
{
	unsigned char result;
	struct HostCommandFrame *rx;

	if (slow == NULL)
		return 10;
	if (hostLink.statusBit1)
		return (unsigned char)kermitCmdChecksumError(slow);
	rx = &hostRx;
	if (!handshakeUnlocked11()) {
		switch (command) {
		case 0x80:
			result = hostCommandSetLinkConfig(slow);
			break;
		case 0x83:
			result = hostCommandWatchdogStart(rx, slow);
			break;
		case 0x84:
			result = hostCommandWatchdogStop(rx, slow);
			break;
		case 0x85:
			result = hostCommandWatchdogKick(rx, slow);
			break;
		case 0xa0:
			result = hostCommandHandshake(rx, slow);
			break;
		case 0xc0:
			result = hostCommandTransition(rx, slow);
			break;
		default:
			result = kermitCmdNoService(slow);
			updEventSet(0x16);
			break;
		}
		return result;
	}
	switch (command) {
	case 0x80:
		result = hostCommandSetLinkConfig(slow);
		break;
	case 0x83:
		result = hostCommandWatchdogStart(rx, slow);
		break;
	case 0x84:
		result = hostCommandWatchdogStop(rx, slow);
		break;
	case 0x85:
		result = hostCommandWatchdogKick(rx, slow);
		break;
	case 0xc1:
		result = hostCommandErnieShutdown(slow);
		break;
	case 0x90:
		result = hostCommandReadScratchPad(rx, slow);
		break;
	case 0x91:
		result = hostCommandWriteScratchPad(rx, slow);
		break;
	case 0xa0:
		result = hostCommandHandshake(rx, slow);
		break;
	case 0xb2:
		result = hostCommandCtrlHostOutputViaDongle(rx, slow);
		break;
	case 0xc0:
		result = hostCommandTransition(rx, slow);
		break;
	case 0x190:
		result = UiSetMultiCnPort(rx, slow);
		break;
	case 0x888:
		result = hostCommandCtrlSdPower(rx, slow);
		break;
	case 0x889:
		result = hostCommandCtrlAccPower(rx, slow);
		break;
	case 0x891:
		result = powerCtrlLED(rx, slow);
		break;
	case 0x88f:
		result = powerCtrlDeviceReset(rx, slow);
		break;
	case 0x88e:
		result = ctrlVoltage(rx, slow);
		break;
	case 0x89b:
		result = hostCommandCtrlRMRPower(rx, slow);
		break;
	case 0x89c:
		result = hostCommandCtrlMultiCnPower(rx, slow);
		break;
	case 0x982:
		result = hostCommandBatteryGetFullCap(rx, slow);
		break;
	case 0x98a:
		result = hostCommandBatteryGetFullCap2(rx, slow);
		break;
	case 0x1080:
		result = hostCommandNvs1080(slow);
		break;
	case 0x1081:
		result = hostCommandNvsFormat(slow);
		break;
	case 0x1082:
		result = hostCommandNvsReadData(slow);
		break;
	case 0x1083:
		result = hostCommandNvsWriteData(slow);
		break;
	case 0x1180:
		result = updaterSetSegment(slow);
		break;
	case 0x1181:
		result = updaterWriteData(slow);
		break;
	case 0x1182:
		result = updaterExecProgramming(slow);
		break;
	case 0x1183:
		result = updaterReboot(slow);
		break;
	case 0x1184:
		result = updaterExecFinalize(slow);
		break;
	case 0x2080:
		result = hostCommandSerialUnlock(rx, slow);
		break;
	case 0x2081:
		result = hostCommandSerialLock(rx, slow);
		break;
	case 0x2082:
		result = hostCommandSerialRestart(rx, slow);
		break;
	case 0x2083:
		result = hostCommandSerialRead(rx, slow);
		break;
	case 0x2084:
		result = hostCommandSerialWrite(rx, slow);
		break;
	case 0x2085:
		result = hostCommandSerialWriteOrSend(rx, slow);
		break;
	default:
		result = kermitCmdNoImprement(slow);
		break;
	}
	return result;
}

int kermitCmdNoImprement(struct HostSlowCommand *slow)
{
	slow->answer.length = 2;
	slow->answer.result = 0x3f;
	logPrintf("kermitCmdNoImprement: %04Xh\r\n", slow->command);
	return 1;
}

int kermitCmdNoService(struct HostSlowCommand *slow)
{
	slow->answer.length = 2;
	slow->answer.result = 0x3e;
	logPrintf("kermitCmdNoService: %04Xh\r\n", slow->command);
	return 5;
}

/* the frame's checksum was wrong */
int kermitCmdChecksumError(struct HostSlowCommand *slow)
{
	slow->answer.length = 2;
	slow->answer.result = 0x3d;
	return 2;
}

/* 0x1180: the segment whose data comes next */
int updaterSetSegment(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned char error;

	data = hostRx.data;
	error = flSetSegment(data[0]);
	slow->answer.length = 2;
	slow->answer.result = error == 0 ? 0 : 4;
	return 0;
}

/* 0x1181: a piece of the segment's data, at its offset */
int updaterWriteData(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned int offset;

	data = hostRx.data;
	((unsigned char __near *)&offset)[0] = data[0];
	((unsigned char __near *)&offset)[1] = data[1];
	if (hostLink.frameSize - 5 < data[2])
		slow->answer.result = 0x3c;
	else if (offset + data[2] > 0x400)
		slow->answer.result = 0x3c;
	else {
		memcpy(&updCipher[offset], &data[3], data[2]);
		slow->answer.result = 0;
	}
	slow->answer.length = 2;
	return 0;
}

/* 0x1182: programs the segment, when sum is the complement of its data's sum */
int updaterExecProgramming(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned long sum;
	unsigned char result;
	unsigned char error;

	data = hostRx.data;
	((unsigned char __near *)&sum)[0] = data[0];
	((unsigned char __near *)&sum)[1] = data[1];
	((unsigned char __near *)&sum)[2] = data[2];
	((unsigned char __near *)&sum)[3] = data[3];
	logPrintf("FlProg: block=%03d,sum=%08lX ", updSegment, sum);
	logPrintf("[S");
	if (sum == flChecksum()) {
		logPrintf("D");
		error = flDecrypt();
		logPrintf("P");
		if (error)
			result = 2;
		else {
			error = flProg();
			switch (error) {
			case 0x00:
				result = 0x00;
				break;
			case 0x01:
				result = 0x01;
				break;
			case 0x05:
				result = 0x05;
				break;
			case 0x10:
				result = 0x10;
				break;
			case 0x1a:
				result = 0x1a;
				break;
			case 0x1b:
				result = 0x1b;
				break;
			case 0x1c:
				result = 0x1c;
				break;
			case 0x1f:
			default:
				result = 0x20;
				break;
			}
		}
	} else {
		logPrintf("FlChksum: Error %08X Block=%02d\r\n", sum, updSegment);
		result = 3;
	}
	slow->answer.length = 2;
	slow->answer.result = result;
	logPrintf("]\r\n");
	return 0;
}

/* 0x1184: the image's digest, checked from the main loop (flImageCheck) */
int updaterExecFinalize(struct HostSlowCommand *slow)
{
	unsigned int i;
	unsigned int count;

	if (updImageChecked) {
		slow->answer.length = 2;
		slow->answer.result = 6;
		return 0;
	}
	if (slow->state == 0) {
		count = 0;
		for (i = 0; i < 0x100; i++)
			if (updReceived[i])
				count++;
		slow->hash.segment = 0;
		slow->hash.segmentDone = 0;
		slow->hash.segmentCount = count;
		slow->handler = flImageCheck;
		slow->state = 1;
		memcpy(&slow->frame, &hostRx, sizeof(slow->frame));
		updProgress = 0;
	}
	return 0x14;
}

/* 0x1183: run mode 0x72ba reboots, 0x9a54 is taken */
int updaterReboot(struct HostSlowCommand *slow)
{
	unsigned char *data;

	data = hostRx.data;
	slow->answer.result = 9;
	if (data[0] == 0xba && data[1] == 0x72) {
		powerOff();
		flReboot();
	} else if (data[0] == 0x54 && data[1] == 0x9a)
		slow->answer.result = 0;
	else
		slow->answer.result = 0x3c;
	slow->answer.length = 2;
	return 0;
}

/* 0x1100 (sceSysconGetErnieDLVersion) */
int hostCommandErnieDlVersion(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	memcpy(answer->data, APP_VERSION_INFO->dlVersion, 4);
	answer->result = 0;
	return 0;
}

/* 0x1101: the image check's progress in percent */
int hostCommandUpdaterProgress(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	answer->data[0] = updProgress;
	answer->data[1] = 0;
	answer->data[2] = 0;
	answer->data[3] = 0;
	answer->result = 0;
	return 0;
}

/* 0x0000 */
int hostCommandNull(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 2;
	answer->result = 0;
	return 0;
}

/* 0x0001 (sceSysconGetErnieVersion): the application's */
int hostCommandErnieVersion(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	memcpy(answer->data, updVersion, 4);
	answer->result = 0;
	return 0;
}

/* 0x0002 (sceSysconGetTimeStamp): the application's build, as 12 digits */
int hostCommandTimeStamp(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 14;
	memcpy(answer->data, updBuild, 12);
	answer->result = 0;
	return 0;
}

/* 0x0003 (sceSysconGetSleepFactor) */
int hostCommandSleepFactor(void)
{
	struct HostAnswerFrame *answer;
	unsigned long factor;

	answer = &hostTx;
	answer->length = 6;
	factor = sleepFactorGet();
	answer->data[0] = ((unsigned char __near *)&factor)[0];
	answer->data[1] = ((unsigned char __near *)&factor)[1];
	answer->data[2] = ((unsigned char __near *)&factor)[2];
	answer->data[3] = ((unsigned char __near *)&factor)[3];
	answer->result = 0;
	sleepFactorAcknowledge();
	return 0;
}

/* 0x0004: the sleep factor, then the slow commands queued (bit 15: not run yet) */
int hostCommandRequests(void)
{
	struct HostAnswerFrame *answer;
	int i;
	unsigned int n;
	unsigned int command;
	unsigned long factor;

	answer = &hostTx;
	factor = sleepFactorGet();
	answer->data[0] = ((unsigned char __near *)&factor)[0];
	answer->data[1] = ((unsigned char __near *)&factor)[1];
	answer->data[2] = ((unsigned char __near *)&factor)[2];
	answer->data[3] = ((unsigned char __near *)&factor)[3];
	answer->length = 6;
	answer->result = 0;
	sleepFactorAcknowledge();
	n = 4;
	for (i = 0; i < HOST_SLOW_COMMANDS; i++) {
		command = hostSlowCommands[i].command;
		if (command != 0) {
			if (hostSlowCommands[i].state == HOST_SLOW_QUEUED)
				command |= 0x8000;
			answer->data[n++] = (unsigned char)(command & 0xff);
			answer->data[n++] = (unsigned char)(command >> 8);
			answer->length += 2;
		}
	}
	return 0;
}

/* 0x0005 (sceSysconGetHardwareInfo): the application's */
int hostCommandHardwareInfo(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	answer->data[0] = APP_VERSION_INFO->hardwareInfo[0];
	answer->data[1] = APP_VERSION_INFO->hardwareInfo[1];
	answer->data[2] = APP_VERSION_INFO->hardwareInfo[2];
	answer->data[3] = APP_VERSION_INFO->hardwareInfo[3];
	answer->result = 0;
	return 0;
}

/* 0x0010 (sceSysconGetWakeupFactor): none */
int hostCommandWakeupFactor(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 4;
	answer->data[0] = 0;
	answer->data[1] = 0;
	answer->result = 0;
	return 0;
}

/* 0x0011 (sceSysconGetClock) */
int hostCommandClock(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	answer->data[0] = ((unsigned char __far *)&keepRam.clock)[0];
	answer->data[1] = ((unsigned char __far *)&keepRam.clock)[1];
	answer->data[2] = ((unsigned char __far *)&keepRam.clock)[2];
	answer->data[3] = ((unsigned char __far *)&keepRam.clock)[3];
	answer->result = 0;
	return 0;
}

/* 0x0013: the host's watchdog's setting */
int hostCommand0013(void)
{
	struct HostAnswerFrame *answer;
	union Watchdog setting;

	answer = &hostTx;
	watchdogGet(&setting);
	answer->length = 4;
	answer->data[0] = setting.bytes.timeout;
	answer->data[1] = setting.bytes.mode;
	answer->result = 0;
	return 0;
}

/* 0x0100 (sceSysconGetControlsInfo): the buttons, as controlsReadButtons in the application */
int hostCommandControlsInfo(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 6;
	answer->data[0] = P8;
	answer->data[1] = (0x0f & P5) | ((!P11.5 ? (unsigned char)0xef : (unsigned char)0xff) &
					 (!P4.6 ? (unsigned char)0xbf : (unsigned char)0xff) & 0xf0);
	answer->data[2] = (P11 >> 2 & 3) | 0xfc;
	answer->data[3] = 0x74;
	answer->result = 0;
	return 0;
}

/* 0x0080: the link's settings, the frame size by the first byte */
int hostCommandSetLinkConfig(struct HostSlowCommand *slow)
{
	unsigned int value;
	unsigned char *data;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	data = hostRx.data;
	value = (unsigned int)data[1] << 8 | data[0];
	answer->result = 0x20;
	answer->length = 2;
	if (hostRx.length == 3) {
		switch (value) {
		case 0:
			answer->result = 0;
			hostLink.frameSize = 0x20;
			break;
		case 1:
			answer->result = 0;
			hostLink.frameSize = 0x40;
			break;
		case 2:
			answer->result = 0;
			hostLink.frameSize = 0x80;
			break;
		}
	}
	return 0;
}

/* 0x0090 (sceSysconReadScratchPad) */
int hostCommandReadScratchPad(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	struct ScratchPadRange range;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	range = *(struct ScratchPadRange *)rx->data;
	answer->length = 2;
	answer->result = 2;
	if (rx->length == 4) {
		if (scratchPadRead(range.offset, range.size, answer->data) == 0) {
			answer->length += range.size;
			answer->result = 0;
		}
	} else
		answer->result = 0x3b;
	return 0;
}

/* 0x0091 (sceSysconWriteScratchPad) */
int hostCommandWriteScratchPad(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	struct ScratchPadRange range;
	struct HostAnswerFrame *answer;
	unsigned char *data;

	answer = &slow->answer;
	range = *(struct ScratchPadRange *)rx->data;
	data = &rx->data[3];
	answer->length = 2;
	answer->result = 2;
	if (rx->length >= 5) {
		if (scratchPadWrite(range.offset, range.size, data) == 0)
			answer->result = 0;
	} else
		answer->result = 0x3b;
	return 0;
}

/* 0x0083: the host's watchdog set and started */
int hostCommandWatchdogStart(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	union Watchdog *setting;
	unsigned char result;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	setting = (union Watchdog *)rx->data;
	watchdogSet(setting);
	result = watchdogStart();
	answer->length = 2;
	answer->result = result;
	return 0;
}

/* 0x0084: and stopped */
int hostCommandWatchdogStop(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char result;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	result = watchdogStop();
	answer->length = 2;
	answer->result = result;
	return 0;
}

/* 0x0085: and kicked */
int hostCommandWatchdogKick(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char result;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	result = watchdogKick();
	answer->length = 2;
	answer->result = result;
	return 0;
}

/* 0x00a0: the host's 0x28 bytes go to the handshake, which answers in their place */
int hostCommandHandshake(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	struct HostAnswerFrame *answer;
	unsigned char buf[0x28];
	unsigned char error;

	answer = &slow->answer;
	memcpy(buf, rx->data, sizeof(buf));
	error = handshakeCommand(buf);
	memcpy(answer->data, buf, sizeof(buf));
	answer->length = 0x2a;
	if (error == 0)
		answer->result = 0;
	else
		answer->result = 0x10;
	return 0;
}

/* 0x00b2 (sceSysconCtrlHostOutputViaDongle) */
int hostCommandCtrlHostOutputViaDongle(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	struct HostAnswerFrame *answer;
	unsigned char result;

	answer = &slow->answer;
	if (rx->length == 2) {
		if (!boardMultiCnIsDongle())
			answer->result = 4;
		else if (rx->data[0] == 0) {
			boardSetMultiCnOutput(BOARD_MULTICN_HOST);
			result = powerMultiCnOff();
			answer->result = result;
		} else if (serialGetState() != 0 && serialGetState() != 1)
			answer->result = 5;
		else {
			boardSetMultiCnOutput(BOARD_MULTICN_DONGLE);
			result = powerMultiCnOn();
			answer->result = result;
		}
	} else
		answer->result = 0x3b;
	answer->length = 2;
	return 0;
}

/* 0x00c0: a transition; only 0 (event 0x1e) */
int hostCommandTransition(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	data = rx->data;
	answer->length = 2;
	if (data[0] == 0) {
		updEventSet(0x1e);
		answer->result = 0;
	} else
		answer->result = 0x3e;
	return 0;
}

/* 0x00c1 (sceSysconErnieShutdown): 0 events 0x1f, 1 event 0x20 */
int hostCommandErnieShutdown(struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	data = hostRx.data;
	answer->result = 6;
	answer->length = 2;
	if (hostRx.length == 2) {
		if (data[0] == 0) {
			answer->result = 0;
			updEventSet(0x1f);
		} else if (data[0] == 1) {
			answer->result = 0;
			updEventSet(0x20);
		} else
			answer->result = 0x3c;
	}
	return 0;
}

/* 0x0190: the multi connector's output */
int UiSetMultiCnPort(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 3) {
		error = boardSetMultiCnOutput(rx->data[0]);
		if (error)
			slow->answer.result = 0x3c;
		else
			slow->answer.result = 0;
	} else
		slow->answer.result = 0x3b;
	logPrintf(" UiSetMultiCnPort(%02X): %x\r\n", rx->data[0], error);
	slow->answer.length = 2;
	return 0;
}

/* 0x088a (sceSysconCtrlWirelessPower), which the dispatch does not take */
int hostCommandCtrlWirelessPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0]) {
			error = powerOnWlan();
			logPrintf(" powerOnWlan:%d\r\n", error);
		} else {
			error = powerOffWlan();
			logPrintf(" powerOffWlan:%d\r\n", error);
		}
		slow->answer.result = error == 0 ? 0 : 1;
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x088f (sceSysconCtrlDeviceReset): data[1] the devices, data[0] their reset line */
int powerCtrlDeviceReset(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	if (rx->length == 3) {
		logPrintf(" powerCtrlDeviceReset(%02X,%02X)\r\n", rx->data[0], rx->data[1]);
		if (rx->data[1] & 0x02)
			P6.3 = rx->data[0];
		if (rx->data[1] & 0x04)
			P0.5 = rx->data[0];
		if (rx->data[1] & 0x08)
			P0.6 = rx->data[0];
		if (rx->data[1] & 0x10)
			P13.1 = rx->data[0];
		slow->answer.result = 0;
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x088e (sceSysconCtrlVoltage): regulator DD1-DD4 to a level below 0x40, trimmed */
int ctrlVoltage(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char result;
	unsigned char regulator;
	unsigned char level;
	unsigned char trim;

	if (rx->length == 3) {
		switch (rx->data[0]) {
		case 1:
			regulator = 0x81;
			logPrintf("[pwr] Kr->Er CtrlVoltage DD1:%02x\r\n", rx->data[1]);
			break;
		case 2:
			regulator = 0x82;
			logPrintf("[pwr] Kr->Er CtrlVoltage DD2:%02x\r\n", rx->data[1]);
			break;
		case 3:
			regulator = 0x83;
			logPrintf("[pwr] Kr->Er CtrlVoltage DD3:%02x\r\n", rx->data[1]);
			break;
		case 4:
			regulator = 0x84;
			logPrintf("[pwr] Kr->Er CtrlVoltage DD4:%02x\r\n", rx->data[1]);
			break;
		default:
			/* the result is not answered: the answer's stays */
			result = 3;
			goto done;
		}
		if (rx->data[1] >= 0x40) {
			logPrintf("[pwr] ctrlVoltage: error %02x,%02x\r\n", rx->data[0], rx->data[1]);
			slow->answer.result = 3;
		} else {
			trim = configGet()->voltageTrim;
			level = levelTrim(rx->data[1], trim);
			logPrintf("[pwr] Er->El %02x\r\n", level);
			result = powerSetVoltage(regulator, level);
			slow->answer.result = result == 0 ? 0 : 1;
		}
	} else
		slow->answer.result = 0x3b;
done:
	slow->answer.length = 2;
	return 0;
}

/* 0x0891 (sceSysconCtrlLED): data[0] 0x40-0x08 the LEDs, 0x80 on */
int powerCtrlLED(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0] & 0x40) {
			if (rx->data[0] & 0x80)
				elmoLed |= 0x8000;
			else
				elmoLed &= ~0x8000;
		}
		if (rx->data[0] & 0x20) {
			if (rx->data[0] & 0x80)
				elmoLed |= 0x4000;
			else
				elmoLed &= ~0x4000;
		}
		if (rx->data[0] & 0x10) {
			if (rx->data[0] & 0x80)
				elmoLed |= 0x2000;
			else
				elmoLed &= ~0x2000;
		}
		if (rx->data[0] & 0x08) {
			if (rx->data[0] & 0x80)
				elmoLed |= 0x1000;
			else
				elmoLed &= ~0x1000;
		}
		elmoCommand[0] = 0xa5;
		elmoCommand[1] = ((unsigned char *)&elmoLed)[1];
		elmoCommand[2] = (unsigned char)elmoLed;
		error = elmoWrite(elmoCommand, 3);
		slow->answer.result = powerResult(error);
		logPrintf(" powerCtrlLED(%02X) : %x\r\n", rx->data[0], error);
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x0888 (sceSysconCtrlSdPower): data[0] on or off */
int hostCommandCtrlSdPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0])
			error = powerSdOn();
		else
			error = powerSdOff();
		slow->answer.result = powerResult(error);
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x0889 (sceSysconCtrlAccPower): data[0] on or off */
int hostCommandCtrlAccPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0])
			error = powerAccOn();
		else
			error = powerAccOff();
		slow->answer.result = powerResult(error);
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x089b (sceSysconCtrlRMRPower): data[0] on or off */
int hostCommandCtrlRMRPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0])
			error = powerRmrOn();
		else
			error = powerRmrOff();
		slow->answer.result = powerResult(error);
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x089c (sceSysconCtrlMultiCnPower): data[0] on or off */
int hostCommandCtrlMultiCnPower(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char error;

	if (rx->length == 2) {
		if (rx->data[0])
			error = powerMultiCnOn();
		else
			error = powerMultiCnOff();
		slow->answer.result = powerResult(error);
	} else
		slow->answer.result = 0x3b;
	slow->answer.length = 2;
	return 0;
}

/* 0x0800 (sceSysconGetPowerStatus) */
int hostCommandPowerStatus(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 4;
	answer->data[0] = powerStatus.byte[0];
	answer->data[1] = powerStatus.byte[1];
	answer->result = 0;
	return 0;
}

/* 0x0900 (sceSysconGetBatteryRemainCap) */
int hostCommandBatteryRemainCap(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 4;
	answer->data[0] = (unsigned char)(charge.batteryRemainCap & 0xff);
	answer->data[1] = (unsigned char)(charge.batteryRemainCap >> 8);
	answer->result = 0;
	return 0;
}

/* 0x0901 (sceSysconGetBatteryVolt) */
int hostCommandBatteryVolt(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 4;
	answer->data[0] = (unsigned char)(charge.batteryVolt & 0xff);
	answer->data[1] = (unsigned char)(charge.batteryVolt >> 8);
	answer->result = 0;
	return 0;
}

/* 0x0902 (sceSysconGetBatteryLifePercent) */
int hostCommandBatteryLifePercent(void)
{
	struct HostAnswerFrame *answer;

	answer = &hostTx;
	answer->length = 4;
	answer->data[0] = (unsigned char)(charge.batteryLifePercent & 0xff);
	answer->data[1] = (unsigned char)(charge.batteryLifePercent >> 8);
	answer->result = 0;
	return 0;
}

/* 0x0982: the battery's register 0x3c */
int hostCommandBatteryGetFullCap(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	static const unsigned char reg = 0x3c;
	unsigned char error;

	slow->answer.length = 4;
	if (rx->length == 1) {
		error = batteryRead((unsigned char *)&reg, slow->answer.data, 2);
		slow->answer.result = batteryResult(error);
	} else
		slow->answer.result = 0x3b;
	return 0;
}

/* 0x098a: the battery's register 0x12 */
int hostCommandBatteryGetFullCap2(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	static const unsigned char reg = 0x12;
	unsigned char error;

	slow->answer.length = 4;
	if (rx->length == 1) {
		error = batteryRead((unsigned char *)&reg, slow->answer.data, 2);
		slow->answer.result = batteryResult(error);
	} else
		slow->answer.result = 0x3b;
	return 0;
}

/* 0x1080: the NVS's */
int hostCommandNvs1080(struct HostSlowCommand *slow)
{
	unsigned char *data;

	data = hostRx.data;
	slow->answer.length = 2;
	slow->answer.result = 0;
	return 0;
}

/* 0x1081: formats the NVS, data[0] 0x55 */
int hostCommandNvsFormat(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned char result;

	data = hostRx.data;
	if (data[0] == 0x55) {
		result = nvsFormat();
		slow->answer.result = result;
	} else
		slow->answer.result = 0x3c;
	slow->answer.length = 2;
	return 0;
}

/* 0x1082 (sceSysconNvsReadData): the offset, the size */
int hostCommandNvsReadData(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned int offset;
	unsigned char result;

	if (hostRx.length != 4) {
		slow->answer.length = 2;
		slow->answer.result = 0x3b;
		return 0;
	}
	data = hostRx.data;
	offset = (unsigned int)data[1] << 8 | data[0];
	result = nvsRead(offset, data[2], slow->answer.data, 0);
	if (result == 0) {
		slow->answer.length = data[2] + 2;
		slow->answer.result = 0;
	} else {
		slow->answer.length = 2;
		slow->answer.result = result;
	}
	return 0;
}

/* 0x1083 (sceSysconNvsWriteData): the offset, the size, the data */
int hostCommandNvsWriteData(struct HostSlowCommand *slow)
{
	unsigned char *data;
	unsigned int offset;
	unsigned char result;

	if (hostRx.length < 5) {
		slow->answer.length = 2;
		slow->answer.result = 0x3b;
		return 0;
	}
	data = hostRx.data;
	offset = (unsigned int)data[1] << 8 | data[0];
	result = nvsWrite(offset, data[2], &data[3], 0);
	slow->answer.length = 2;
	slow->answer.result = result;
	return 0;
}

/* 0x2080: unlocks the serial link */
int hostCommandSerialUnlock(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;		/* not used */
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	answer->length = 2;
	answer->result = serialUnlock(1);
	return 0;
}

/* 0x2081: locks it */
int hostCommandSerialLock(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	struct HostAnswerFrame *answer;

	answer = &slow->answer;
	answer->length = 2;
	answer->result = serialLock(1);
	return 0;
}

/* 0x2082: restarts it, text (0) or binary (1), at speed 2 or 3 */
int hostCommandSerialRestart(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;
	unsigned char state;

	answer = &slow->answer;
	data = rx->data;
	if (hostRx.length != 5) {
		slow->answer.length = 2;
		slow->answer.result = 2;
		return 0;
	}
	state = serialGetState();
	if (state != 0 && state != 1) {
		slow->answer.length = 2;
		slow->answer.result = 1;
		return 0;
	}
	if (data[0] == 0)
		serialStart(1);
	else if (data[0] == 1)
		serialStart(SERIAL_BINARY | 1);
	else {
		slow->answer.length = 2;
		slow->answer.result = 3;
		return 0;
	}
	if (data[1] == 0 || data[1] == 1) {
		answer->length = 2;
		slow->answer.result = serialReopen(data[1] += 2);
	} else {
		slow->answer.length = 2;
		slow->answer.result = 3;
	}
	return 0;
}

/* 0x2083: reads mode 1's buffer: the offset, the size */
int hostCommandSerialRead(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;
	unsigned char result;

	answer = &slow->answer;
	data = rx->data;
	if (hostRx.length != 3) {
		slow->answer.length = 2;
		slow->answer.result = 2;
		return 0;
	}
	result = serial1BufferRead(data[0], data[1], slow->answer.data);
	sleepFactorSet25(0);
	if (result != 0) {
		slow->answer.length = 2;
		slow->answer.result = result;
	} else {
		slow->answer.length = data[1] + 2;
		slow->answer.result = result;
	}
	return 0;
}

/* 0x2084: writes the buffer, and sends what was written */
int hostCommandSerialWrite(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;
	unsigned char result;

	answer = &slow->answer;
	data = rx->data;
	if (hostRx.length < 4) {
		slow->answer.length = 2;
		slow->answer.result = 2;
		return 0;
	}
	result = serial1BufferWrite(data[0], data[1], &data[2]);
	if (serialGetState() == SERIAL_RECEIVE) {
		memcpy(serial.buffer, &serial1Buffer[data[0]], data[1]);
		serial.command = SERIAL_REQUEST_FRAME;
		serial.status = 0;
		serial.length = data[1];
		updEventSet(0x35);
		slow->answer.length = 2;
		slow->answer.result = 0;
	} else {
		slow->answer.length = 2;
		slow->answer.result = 5;
	}
	return 0;
}

/* 0x2085: data[0] 0 writes the buffer, 1 sends its bytes */
int hostCommandSerialWriteOrSend(struct HostCommandFrame *rx, struct HostSlowCommand *slow)
{
	unsigned char *data;
	struct HostAnswerFrame *answer;
	unsigned char result;

	answer = &slow->answer;
	data = rx->data;
	if (hostRx.length < 4) {
		slow->answer.length = 2;
		slow->answer.result = 2;
		return 0;
	}
	if (data[0] == 0) {
		result = serial1BufferWrite(data[1], data[2], &data[3]);
		slow->answer.length = 2;
		slow->answer.result = result;
	} else if (data[0] == 1) {
		if (serialGetState() == SERIAL_RECEIVE) {
			memcpy(serial.buffer, &serial1Buffer[data[1]], data[2]);
			serial.command = SERIAL_REQUEST_FRAME;
			serial.status = 0;
			serial.length = data[2];
			updEventSet(0x35);
			slow->answer.length = 2;
			slow->answer.result = 0;
		} else {
			slow->answer.length = 2;
			slow->answer.result = 5;
		}
	} else {
		slow->answer.length = 2;
		slow->answer.result = 3;
	}
	return 0;
}

/* the Elmo's and the Cookie's errors as the host's results */
unsigned char powerResult(unsigned char result)
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

/* the battery's errors as the host's results */
unsigned char batteryResult(unsigned char result)
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

/*
 * The update's flash module: the segment's buffers, its checksum and decryption, its
 * programming, and the image's digest. Segments 0-3 (boot cluster 0) are written to 4-7
 * (boot cluster 1), which the boot cluster swap then puts in their place; 0xe0 is written
 * to 0xd4, and 0xe1-0xed (the updater's own) are refused.
 */
int flSetSegment(unsigned char segment)
{
	if (segment >= 0x100)
		return 1;
	if (segment == 0xe0)
		segment = 0xd4;
	else if (segment > 0xe0 && segment < 0xee)
		return 1;
	if (segment < 4)
		segment += 4;
	updSegment = segment;
	updReceived[segment] = 1;
	memset(updPlain, 0, sizeof(updPlain));
	memset(updCipher, 0, sizeof(updCipher));
	logPrintf("Segment: %d\r\n", segment);
	return 0;
}

/* the complement of the sum of the segment's ciphertext */
unsigned long flChecksum(void)
{
	unsigned long sum;
	unsigned char *data;
	unsigned int i;

	sum = 0;
	data = updCipher;
	for (i = 0; i < sizeof(updCipher); i++)
		sum += *data++;
	if (0)
		logPrintf("\r\nImgChksum=%08ld\r\n", sum);
	return ~sum;
}

/*
 * 0x1184's handler, from the main loop: hashes two programmed segments a call, then checks
 * the digest, takes the image's version and swaps the boot clusters
 */
void flImageCheck(struct HostSlowCommand *slow)
{
	unsigned char *digest;
	struct HostSlowHash *hash;
	unsigned int segment;
	unsigned char count;
	unsigned char result;

	count = 2;
	hash = &slow->hash;
	for (segment = hash->segment; segment < 0x100; segment++) {
		if (updReceived[segment]) {
			if (count-- == 0) {
				hash->segment = segment;
				return;
			}
			logPrintf("SHA1 digesting(%d)\r", segment);
			result = SHA1Input((SHA1Context __near *)&updSha,
					   (const unsigned char *)((unsigned long)segment << 10), 0x400);
			if (result != 0) {
				logPrintf("\nsha1update error%d\r\n", result);
				slow->answer.result = 8;
				goto done;
			}
			hash->segmentDone++;
			updProgress = (unsigned int)hash->segmentDone * 100 / (unsigned int)hash->segmentCount;
		}
	}
	digest = slow->frame.data;
	result = flHashCheck(digest);
	if (result == 0) {
		memcpy(updVersion, IMAGE_VERSION_INFO->version, 4);
		timeStampDigits(updBuild, (const char __far *)(IMAGE_VERSION_INFO + 1));
		if (flBootClusterSwap() == 0) {
			updImageChecked = 1;
			slow->answer.result = 0;
		} else {
			slow->answer.result = 7;
			EI();
		}
	} else
		slow->answer.result = 8;
done:
	slow->answer.length = 2;
	slow->state = HOST_SLOW_DONE;
}

/* the image's SHA-1 against digest's */
int flHashCheck(unsigned char *digest)
{
	unsigned char result;
	unsigned char hash[SHA1HashSize];

	result = 0;
	result = SHA1Result((SHA1Context __near *)&updSha, (unsigned char __near *)hash);
	if (result != 0) {
		logPrintf("\nsha1result error\r\n");
		return 2;
	}
	result = memcmp(hash, digest, SHA1HashSize);
	logPrintf("\nImage HashCheck %02X %02X %02X %02X... %s\n\r", digest[0], digest[1],
		  digest[2], digest[3], result == 0 ? "OK" : "NG");
	return result;
}

/* the segment AES-128-CBC decrypted, the IV chained from its last ciphertext block */
int flDecrypt(void)
{
	aesCbcDecrypt((const unsigned char __near *)updCipher, (unsigned char __near *)updPlain,
		      (struct AesKey __near *)&updKey, updIv, sizeof(updCipher));
	memcpy(updIv, &updCipher[sizeof(updCipher) - AES_BLOCK_SIZE], AES_BLOCK_SIZE);
	return 0;
}

/* programs the segment: erased if not blank, written 0x40 words at a time, verified */
int flProg(void)
{
	unsigned char result;
	unsigned int i;
	int tries;

	tries = 1;
	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	while (result == 0x1f)
		result = FSL_Init_cont(fslDataBuffer);
	result = FSL_ModeCheck();
	if (result == 0) {
		result = FSL_BlankCheck(updSegment);
		if (result == 0x1b) {
			do
				result = FSL_Erase(updSegment);
			while (result == 0x1f);
		}
		updProgrammed = 1;
		do {
			if (result == 0) {
				for (i = 0; i < 0x100; i += 0x40) {
					memcpy(fslDataBuffer, &updPlain[i << 2], 0x100);
					if (0)
						logPrintf("\r\nFP FADR=%06lX SADR=%06lX\r\n",
							  ((unsigned long)updSegment << 10) + (i << 2),
							  (unsigned long)&updPlain[i << 2]);
					do
						result = FSL_Write(((unsigned long)updSegment << 10) +
								   (i << 2), 0x40);
					while (result == 0x1f);
					if (result != 0)
						goto close;
				}
				do
					result = FSL_IVerify(updSegment);
				while (result == 0x1f);
				if (tries != 0 && result == 0x1b)
					result = FSL_Erase(updSegment);
			}
		} while (tries-- != 0 && result != 0);
	}
close:
	FSL_Close();
	if (result != 0)
		logPrintf("FlProg: Error=%02Xh Block=%02d\r\n", result, updSegment);
	return result;
}

/* 0x1183's reboot: an illegal instruction resets the chip */
void flReboot(void)
{
	logPrintf("ErnieRebooting\r\n");
#asm
	EXTRN	_cstart
	DB	0FFH
	BR	!!_cstart
#endasm
	while (1)
		NOP();
}

/* swaps the boot clusters, so the image's cluster 1 boots */
int flBootClusterSwap(void)
{
	unsigned char result;

	FSL_Open();
	result = FSL_Init(fslDataBuffer);
	if (result == 0) {
		result = FSL_ModeCheck();
		if (result == 0)
			result = fslSetSecurityFlagSwap();
	}
	FSL_Close();
	logPrintf("Boot Cluster Swap %d %s\r\n", result, result == 0 ? "OK" : "NG");
	return result;
}
