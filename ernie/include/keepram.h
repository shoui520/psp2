/*
 * The RAM kept over a reset (0xfd900, shared by the application and the updater): cstart
 * clears the rest of the RAM, and keeps this block while its magic and checksum hold.
 */
#ifndef KEEPRAM_H
#define KEEPRAM_H

#define KEEP_RAM_MAGIC	0xa55a

struct KeepRam {
	unsigned int	magic;		/* KEEP_RAM_MAGIC */
	unsigned int	checksum;	/* 0xffff - the sum of the bytes from resetCause on */
	unsigned char	resetCause;	/* why the last reset was made */
	unsigned long	clock;		/* the RTC's count of 0.5 s (rtc.h) */
	unsigned long	alarm;		/* the clock value the alarm wakes the system at */
	unsigned char	scratchPad[0x100];	/* the host's (scratchpad.h); the checksum covers
					   the first 0xf2 bytes */
};

extern __far struct KeepRam keepRam;

extern void setResetCause(unsigned char cause);
extern unsigned char getResetCause(void);
extern void resetSystem(void);

#endif
