/*
 * Imports of ScePower (os0:kd/bootimage/power) that no shared header declares yet;
 * the arities are those of the providers' definitions (lowio, syscon, sysmem, hdmi).
 */

#pragma once

#include <psp2kern/types.h>
#include <sce/hdmi.h>
#include <stdint.h>

/* SceLowio (pervasive) */
uintptr_t ScePervasiveForDriver_47708C03(void);			/* voltage table */
int ScePervasiveForDriver_CA0ACFC5(unsigned int mask);
int ScePervasiveForDriver_725B4F59(unsigned int selector);
int ScePervasiveForDriver_2EEBE9AE(void);
int ScePervasiveForDriver_03E1FAA6(void);
int SceGrabForDriver_B22EE8BC(unsigned int mask, unsigned int enable);	/* SMC passthrough; power passes two words */

/* SceSyscon */
int SceSysconForDriver_9CA6EB70(uint32_t enabled);

/* SceSysmem (sysroot) */
int SceSysrootForDriver_7918D44E(void);
int SceSysrootForDriver_246AAAB3(int (*setLedMode)(uint32_t led, uint32_t mode,
						 const void *option));

/* SceDisplay */
int ksceDisplaySetBrightness(unsigned int head, uint32_t brightness);
