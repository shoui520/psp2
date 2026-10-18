/*
 * Imports of SceEnumWakeUp (os0:kd/enum_wakeup.skprx) that vitasdk does not
 * declare, with the signatures its call sites use. NID-named ones have no
 * known name.
 */
#pragma once
#include <display/display.h>
#include <ctrl/ctrl.h>

#include <psp2kern/types.h>
#include <idstorage/idstorage.h>
#include <lowio/pervasive.h>
#include <lowio/gpio.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysroot.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <power/led.h>
#include <power/power.h>
#include <modulemgr/modulemgr.h>
#include <clockgen/clockgen.h>
#include <motion/motion_dev.h>
#include <udcd/udcd.h>

int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

/* display brightness: the OLED or the LCD driver, whichever is present */
/* vitasdk: ksceOledSetBrightness */
int SceOledForDriver_F9624C47(int brightness) __attribute__((weak));
int SceOledForDriver_3C990F53(void) __attribute__((weak));	/* display on? */
/* vitasdk: ksceLcdSetBrightness */
int SceLcdForDriver_581D3A87(int brightness) __attribute__((weak));
int SceLcdForDriver_8C965153(void) __attribute__((weak));	/* display on? */

typedef struct EwCtrlData {
	SceUInt64 timeStamp;
	SceUInt32 buttons;
	SceUInt8 sticks[4];
	SceUInt8 reserved[16];
} EwCtrlData;

int SceHpremoteForDriver_7735C403(void);	/* headphone remote off */

int ScePervasiveForDriver_E0711D42(void);
