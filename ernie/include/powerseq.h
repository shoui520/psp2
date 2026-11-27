/*
 * The power sequences' board parts (app/powerseq.c): the devices read at the power-on (the
 * Elmo's and the Cookie's versions, the battery's identity), the Elmo mask steps of the
 * resume, off and suspend sequences, and the devices' power.
 */
#ifndef ERNIE_POWERSEQ_H
#define ERNIE_POWERSEQ_H

#include "board.h"
#include "elmo.h"

/* the battery's identity, which batteryReadInfo reads */
struct BatteryInfo {
	unsigned int	id;
#if SOURCE_0103
	unsigned int	version[5];	/* the slim's: two words more (batteryReadInfo) */
#else
	unsigned int	version[3];	/* 0xffff: no battery answered */
#endif
};

/* a step of a sequence: the Elmo masks it sets or clears, then a wait */
struct PowerStep {
	struct ElmoMasks	masks;
	unsigned int		delay;	/* powerWaitMs's */
};

/* what the power-on reads (powerSeqOn) */
struct DeviceVersions {
	unsigned char		elmo[2];	/* the Elmo's version, low byte first; 0xff: none
						   answered */
	unsigned char		cookie[2];	/* the Cookie's */
	struct BatteryInfo	battery;	/* the battery's identity */
#if BOARD_DOL1001 && !SOURCE_SLIM
	unsigned char		thermal[2];	/* DOL-1001: the temperature sensor's (THERMAL_ID, 0xff
						   none) */
#endif
};

extern struct DeviceVersions deviceVersions;
extern unsigned char elmoCommand[12];		/* the Elmo commands the sequences and the
						   power requests send */

extern void powerSeqOn(void);
extern unsigned char powerSeqResume(void);
extern void powerSeqLevels(void);
extern void powerSeqWaitP67(void);
extern unsigned char powerSeqOff(void);
extern unsigned char powerSeqSuspend(void);
#if !FW_SINCE(0x0100060B)
extern unsigned char powerNoneOn(void);
extern unsigned char powerNoneOff(void);
#endif
extern unsigned char powerAccOn(void);
extern unsigned char powerAccOff(void);
extern unsigned char powerOnWlan(void);
extern unsigned char powerOffWlan(void);
#if BOARD_USS1001 && SOURCE_0104
extern unsigned char powerOnTouch(void);
extern unsigned char powerOffTouch(void);
#endif
extern unsigned char powerSetVoltage(unsigned char regulator, unsigned char level);
extern unsigned char powerCookieSet(void);
extern unsigned char powerCookieClear(void);
extern unsigned char powerRmrOn(void);
extern unsigned char powerRmrOff(void);
extern unsigned char powerSdOn(void);
extern unsigned char powerSdOff(void);
extern unsigned char powerMultiCnOn(void);
extern unsigned char powerMultiCnOff(void);
#if BOARD_USS1001 && SOURCE_0104	/* battery.h's BATTERY_GAUGE */
/* the slim's 0x0105: the Elmo's register 0xaa bit 15, which the battery's gauge switches */
extern unsigned char powerRegAA15On(void);
extern unsigned char powerRegAA15Off(void);
#endif
extern void powerAccTimer(void);
extern void powerSdTimer(void);
extern void powerSeqInit(void);
#if FW_SINCE(0x01000104)
extern void powerCookieHold(void);
extern void powerCookieRelease(void);
extern unsigned char powerBbmcCycle(void);
extern unsigned char powerBbmcOff(void);
extern unsigned char powerBbmcOn(void);
extern void powerBbmcPulse(void);
#else
extern unsigned char powerBbmcCycle(void);
extern unsigned char powerBbmcRestart(void);
extern void powerCookieCycle(void);
#endif
extern unsigned char powerElmoA0Set(unsigned char bit14, unsigned char bit13);
extern unsigned char powerElmoA0Clear(unsigned char bit14, unsigned char bit13);
#if SOURCE_0103
extern unsigned char powerElmoA0Set2(void);
extern unsigned char powerElmoA0Clear2(void);
#else
extern unsigned char powerElmoA0Set2(unsigned char bit12, unsigned char unused1, unsigned char unused0);
extern unsigned char powerElmoA0Clear2(unsigned char bit12, unsigned char unused1, unsigned char unused0);
#endif
extern unsigned char powerHdmiCecOn(void);
extern unsigned char powerHdmiCecOff(void);
extern unsigned char powerMotionOn(void);
extern unsigned char powerMotionOff(void);
#if !FW_SINCE(0x0100060B)
#if FW_0302
extern unsigned char power088BOn(void);
extern unsigned char power088BOff(void);
#endif
extern unsigned char powerNone2On(void);
extern unsigned char powerNone2Off(void);
extern unsigned char powerNone3On(void);
extern unsigned char powerNone3Off(void);
#endif
extern unsigned char powerHpremoteOn(void);
extern unsigned char powerHpremoteOff(void);
extern unsigned char powerBbmcStart(void);
#if SOURCE_0103
extern unsigned char powerBbmcStop(void);
#endif
extern unsigned char powerLedSet(unsigned char on);
#if BOARD_DOL1001
extern unsigned char powerUsbOn(void);
extern unsigned char powerUsbOff(void);
extern unsigned char powerP80On(void);
extern unsigned char powerP80Off(void);
extern void powerSleepFactor21(void);
#endif
#if FW_SINCE(0x0100060B)
extern void powerGetCutRegs(unsigned char *regs);
#if BOARD_DOL1002 && !(defined(ERNIE_UPDATER))	/* the updater's is its host command */
extern void powerCtrlDeviceReset(unsigned char hold, unsigned char devices);
#endif
#endif

#endif
