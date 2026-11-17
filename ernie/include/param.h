/*
 * The parameter block (app/param.c): 0x80 bytes of RAM loaded from the settings block and
 * defaults at the start, which the host reads and writes (commands 0x0092 and 0x0093) and the
 * charge, the buttons and the blink patterns read.
 */
#ifndef ERNIE_PARAM_H
#define ERNIE_PARAM_H

#include "ernie.h"

#include "charge.h"

#define PARAM_ACCESS_MAX	0x20	/* the most bytes one read or write moves */
#define PARAM_PATTERN_SIZE	0x22

struct Param {
#if FW_SINCE(0x00090206)
	struct ChargeLevel chargeLevels[10];	/* chargeReadFlags's levels */
#else
	struct ChargeLevel chargeLevels[9];	/* 0x00090009's nine */
	unsigned char	chargeLevelsEnd[4];	/* set to 0xff, where later versions have a tenth */
#endif
	unsigned char	levels[4];		/* the Elmo's regulators 0x81-0x84's levels
						   (serial1Command0950, 0951) */
	unsigned char	base;			/* bits 0-5 serial1Command0950's base step */
	unsigned char	adjust;			/* bits 4-7 chargeSetCurrent's adjust */
#if FW_SINCE(0x01000104)
	unsigned char	pressTime1;		/* the buttons' press times (input.c) */
	unsigned char	pressTime2;
#else
	unsigned char	setting1;		/* the settings block's, for the host */
	unsigned char	setting2;
#endif
	unsigned char	reserved2[2];
	unsigned char	pattern1[PARAM_PATTERN_SIZE];	/* blink pattern 1 (powerLedFadeStart) */
#if FW_SINCE(0x01000104)
	unsigned char	setting1;		/* the settings block's, for the host */
	unsigned char	setting2;
#else
	unsigned char	reserved4[2];
#endif
	unsigned char	pattern2[PARAM_PATTERN_SIZE];	/* blink pattern 2 */
#if FW_SINCE(0x0100060B)
	unsigned char	alarmLevel;		/* the charge alarms' adjust (0-2) without the
						   3G modem (paramGetAlarmLevel) */
	unsigned char	alarmLevel3G;		/* and with it */
	unsigned char	reserved3[6];
#else
	unsigned char	reserved3[8];
#endif
};

extern struct Param param;

extern int paramInit(void);
#if !FW_SINCE(0x01000104)
/* none: before 0x01000104 paramInit loads the block itself */
#elif FW_SINCE(0x0100010B)
extern int paramLoad(void);
#else
extern void paramLoad(void);	/* returns nothing before 0x0100010B */
#endif
extern unsigned char paramRead(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char paramWrite(unsigned int offset, unsigned int size, unsigned char *buf);
extern void paramGetChargeLevels(struct ChargeLevel *levels);
#if defined(ERNIE_UPDATER) || !FW_SINCE(0x01000306)
extern void paramGetLevel0(unsigned char *level);
extern void paramGetLevel1(unsigned char *level);
extern void paramGetLevel2(unsigned char *level);
extern void paramGetLevel3(unsigned char *level);
#endif
#if !FW_SINCE(0x0100060B)
extern void paramGetBase(unsigned char *base);
#endif
extern void paramGetAdjust(unsigned char *adjust);
extern void paramGetPattern1(unsigned char *pattern);
extern void paramGetPattern2(unsigned char *pattern);
#if FW_SINCE(0x0100060B)
extern void paramGetAlarmLevel3G(unsigned char *level);
extern void paramGetAlarmLevel(unsigned char *level);
#endif

#endif
