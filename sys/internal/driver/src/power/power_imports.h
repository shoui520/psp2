/*
 * Imports of ScePower (os0:kd/bootimage/power) that no shared header declares yet;
 * the arities are those of the providers' definitions (lowio, syscon, sysmem, hdmi).
 */

#pragma once

#include <scetypes.h>
#include <hdmi/hdmi.h>
#include <stdint.h>
#include <lowio/pervasive.h>
#include <syscon/syscon.h>
#include <display/display.h>
#include <sysmem/sysroot.h>

/*
 * SceLowio's SMC thunks: lowio's definitions take four words and hand the last two to the
 * monitor untouched; power passes only the words it means, so it keeps these declarations.
 */
int SceGrabForDriver_B22EE8BC(unsigned int mask, unsigned int enable);
int ScePervasiveForDriver_7D02215D(unsigned int selector);
int ScePervasiveForDriver_C0F7CC39(unsigned int selector);

/* SceSysmem (sysroot) */

/* SceDisplay */
