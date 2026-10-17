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
#include <psp2kern/lowio/gpio.h>
#include <psp2kern/lowio/pervasive.h>
#include <syscon/syscon.h>
#include <threadmgr/threadmgr.h>
#include <psp2kern/kernel/suspend.h>
#include <psp2kern/kernel/sysroot.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <power/led.h>
#include <power/power.h>

int ksceKernelSearchModuleByName(const char *name);
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);

int ksceDisplayWaitVblankStart(void);
int ksceDisplayWaitVblankStartCB(void);

/* display brightness: the OLED or the LCD driver, whichever is present */
int ksceOledSetBrightness(int brightness) __attribute__((weak));
int SceOledForDriver_3C990F53(void) __attribute__((weak));	/* display on? */
int ksceLcdSetBrightness(int brightness) __attribute__((weak));
int SceLcdForDriver_8C965153(void) __attribute__((weak));	/* display on? */

typedef struct EwCtrlData {
	SceUInt64 timeStamp;
	SceUInt32 buttons;
	SceUInt8 sticks[4];
	SceUInt8 reserved[16];
} EwCtrlData;

int ksceCtrlReadBufferPositive(int port, EwCtrlData *data, int count);
int ksceMotionDevIsReady(void);
int SceMotionDevForDriver_031AF7A1(void);	/* stop the motion device */
int SceHpremoteForDriver_7735C403(void);	/* headphone remote off */

int ScePervasiveForDriver_E0711D42(void);
int SceClockgenForDriver_5130682D(void);
int SceClockgenForDriver_F0A0242D(void);
int SceClockgenForDriver_82F39378(void);
int ksceClockgenWlanBtClkDisable(void);
int SceUdcdForDriver_2D91F8C3(int chargeEnable);
int ksceUdcdGetDeviceState(void);
