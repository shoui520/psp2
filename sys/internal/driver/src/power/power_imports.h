/*
 * Imports of ScePower (os0:kd/bootimage/power) that no shared header declares yet;
 * the arities are those of the providers' definitions (lowio, syscon, sysmem, hdmi).
 */

#pragma once

#include <psp2kern/types.h>
#include <hdmi/hdmi.h>
#include <stdint.h>
#include <lowio/pervasive.h>
#include <syscon/syscon.h>
#include <display/display.h>
#include <sysmem/sysroot.h>

/* SceLowio (pervasive) */
int SceGrabForDriver_B22EE8BC(unsigned int mask, unsigned int enable);	/* SMC passthrough; power passes two words */

/* SceSysmem (sysroot) */

/* SceDisplay */
