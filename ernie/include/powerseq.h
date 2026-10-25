/*
 * The power sequences' board parts (app/powerseq.c): the devices read at the power-on (the
 * Elmo's and the Cookie's versions, the battery's identity), the Elmo mask steps of the
 * resume, off and suspend sequences, and the devices' power.
 */
#ifndef ERNIE_POWERSEQ_H
#define ERNIE_POWERSEQ_H

#include "elmo.h"

/* the battery's identity, which batteryReadInfo reads */
struct BatteryInfo {
	unsigned int	id;
	unsigned int	version[3];	/* 0xffff: no battery answered */
};

/* a step of a sequence: the Elmo masks it sets or clears, then a wait */
struct PowerStep {
	struct ElmoMasks	masks;
	unsigned int		delay;	/* sub_2A619's */
};

/* what the power-on reads (powerSeqOn) */
struct DeviceVersions {
	unsigned char		elmo[2];	/* the Elmo's version, low byte first; 0xff: none
						   answered */
	unsigned char		cookie[2];	/* the Cookie's */
	struct BatteryInfo	battery;	/* the battery's identity */
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
extern unsigned char powerNoneOn(void);
extern unsigned char powerNoneOff(void);
extern unsigned char powerAccOn(void);
extern unsigned char powerAccOff(void);
extern unsigned char powerWirelessOn(void);
extern unsigned char powerWirelessOff(void);
extern unsigned char powerSetVoltage(unsigned char regulator, unsigned char level);
extern unsigned char powerCookieSet(void);
extern unsigned char powerCookieClear(void);
extern unsigned char powerRmrOn(void);
extern unsigned char powerRmrOff(void);
extern unsigned char powerSdOn(void);
extern unsigned char powerSdOff(void);
extern unsigned char powerMultiCnOn(void);
extern unsigned char powerMultiCnOff(void);
extern void powerAccTimer(void);
extern void powerSdTimer(void);
extern void powerSeqInit(void);
extern void powerCookieHold(void);
extern void powerCookieRelease(void);
extern unsigned char powerBbmcCycle(void);
extern unsigned char powerBbmcOff(void);
extern unsigned char powerBbmcOn(void);
extern void powerBbmcPulse(void);
extern unsigned char powerElmoA0Set(unsigned char bit14, unsigned char bit13);
extern unsigned char powerElmoA0Clear(unsigned char bit14, unsigned char bit13);
extern unsigned char powerElmoA0Set2(unsigned char bit12, unsigned char unused1, unsigned char unused0);
extern unsigned char powerElmoA0Clear2(unsigned char bit12, unsigned char unused1, unsigned char unused0);
extern unsigned char powerHdmiCecOn(void);
extern unsigned char powerHdmiCecOff(void);
extern unsigned char powerMotionOn(void);
extern unsigned char powerMotionOff(void);
extern unsigned char powerNone2On(void);
extern unsigned char powerNone2Off(void);
extern unsigned char powerNone3On(void);
extern unsigned char powerNone3Off(void);
extern unsigned char powerHpremoteOn(void);
extern unsigned char powerHpremoteOff(void);
extern unsigned char powerBbmcStart(void);
extern unsigned char powerLedSet(unsigned char on);

#endif
