/*
 * The parameter block (app/param.c): 0x80 bytes of RAM loaded from the settings block and
 * defaults at the start, which the host reads and writes (commands 0x0092 and 0x0093) and the
 * charge, the buttons and the blink patterns read.
 */
#ifndef ERNIE_PARAM_H
#define ERNIE_PARAM_H

#include "charge.h"

#define PARAM_ACCESS_MAX	0x20	/* the most bytes one read or write moves */
#define PARAM_PATTERN_SIZE	0x22

struct Param {
	struct ChargeLevel chargeLevels[10];	/* chargeReadFlags's levels */
	unsigned char	levels[4];		/* the Elmo's regulators 0x81-0x84's levels
						   (serial1Command0950, 0951) */
	unsigned char	base;			/* bits 0-5 serial1Command0950's base step */
	unsigned char	adjust;			/* bits 4-7 chargeSetCurrent's adjust */
	unsigned char	pressTime1;		/* the buttons' press times (input.c) */
	unsigned char	pressTime2;
	unsigned char	reserved2[2];
	unsigned char	pattern1[PARAM_PATTERN_SIZE];	/* blink pattern 1 (powerLedFadeStart) */
	unsigned char	setting1;		/* the settings block's, for the host */
	unsigned char	setting2;
	unsigned char	pattern2[PARAM_PATTERN_SIZE];	/* blink pattern 2 */
	unsigned char	reserved3[8];
};

extern struct Param param;

extern int paramInit(void);
extern int paramLoad(void);
extern unsigned char paramRead(unsigned int offset, unsigned int size, unsigned char *buf);
extern unsigned char paramWrite(unsigned int offset, unsigned int size, unsigned char *buf);
extern void paramGetChargeLevels(struct ChargeLevel *levels);
#ifdef ERNIE_UPDATER
extern void paramGetLevel0(unsigned char *level);
extern void paramGetLevel1(unsigned char *level);
extern void paramGetLevel2(unsigned char *level);
extern void paramGetLevel3(unsigned char *level);
#endif
extern void paramGetBase(unsigned char *base);
extern void paramGetAdjust(unsigned char *adjust);
extern void paramGetPattern1(unsigned char *pattern);
extern void paramGetPattern2(unsigned char *pattern);

#endif
