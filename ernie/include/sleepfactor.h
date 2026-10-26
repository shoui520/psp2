/*
 * The sleep factor (app/sleepfactor.c): 32 flags that tell the host why the system sleeps or
 * wakes (command 0x0003, which Kermit's syscon driver calls SYSCON_CMD_GET_SLEEP_FACTOR).
 * Each setter changes one flag; a change sets bit 2 of the status byte the host reads first
 * in every frame (sleepFactorSnapshot), and the host then reads the flags.
 */
#ifndef ERNIE_SLEEPFACTOR_H
#define ERNIE_SLEEPFACTOR_H

#include "chip.h"

/* the flags (sleepfactor.c's sleepFactor, and sleepFactorRead as the host reads them) */
union SleepFactor {
	unsigned long	value;
	unsigned char	bytes[4];
	struct SfrBits	bits[4];
};

/* the status bits sleepFactorSnapshot returns; bits 0, 1 and 5 are the link's (HostFlags) */
union SleepFactorStatus {
	unsigned char	value;
	struct {
		unsigned char	reserved1:2;
		unsigned char	changed:1;	/* a flag changed since the host read them */
		unsigned char	status3:1;	/* set by the module at 0x28eeb */
		unsigned char	status4:1;	/* set by the module at 0x28eeb */
		unsigned char	reserved2:3;
	} bits;
};

extern void sleepFactorInit(void);
extern unsigned char sleepFactorSnapshot(void);
extern void sleepFactorAcknowledge(void);
extern unsigned long sleepFactorGet(void);
extern void sleepFactorSet5(unsigned char on);
extern void sleepFactorSet6(unsigned char on);
extern void sleepFactorSet4(unsigned char on);
extern void sleepFactorSet7(unsigned char on);
extern void sleepFactorSet3(unsigned char on);
extern void sleepFactorSet8(unsigned char on);
extern void sleepFactorSet25(unsigned char on);
#ifdef ERNIE_UPDATER
extern void sleepFactorSet24(unsigned char on);
#endif
extern void sleepFactorSetStatus3(unsigned char on);
extern void sleepFactorSet0(unsigned char on);
extern void sleepFactorSet1(unsigned char on);
extern void sleepFactorSet2(unsigned char on);
extern void sleepFactorSet9(unsigned char on);
extern void sleepFactorSet10(unsigned char on);
extern void sleepFactorSetStatus4(unsigned char on);
extern void sleepFactorSet16(unsigned char on);
extern void sleepFactorSet17(unsigned char on);
extern void sleepFactorSet18(unsigned char on);
extern void sleepFactorSet15(unsigned char on);
extern void sleepFactorSet19(unsigned char on);
extern void sleepFactorSet22(unsigned char on);
extern void sleepFactorSet23(unsigned char on);
extern void sleepFactorSet20(unsigned char on);

#endif
