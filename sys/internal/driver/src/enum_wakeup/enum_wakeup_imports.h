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

int snprintf(char *buf, SceSize size, const char *fmt, ...);
int sceKernelExitThread(int status);
int sceKernelDelayThread(SceUInt32 usec);
int sceKernelCancelAlarm(SceUID id);
int ksceKernelRegisterTimer(const char *name, SceUInt32 usec, int (*handler)(void), void *arg);
int ksceKernelPowerTick(int type);
SceUInt32 ksceKernelSysrootGetSystemSwVersion(void);
int SceKernelSuspendForDriver_2BB92967(int unk);	/* power unlock? (placeholder) */
int ksceKernelSearchModuleByName(const char *name);
int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);
int SceQafMgrForDriver_52B4E164(void);		/* QA flag: stay on the screen */

int ksceSysconSendCommand(int cmd, const void *buf, SceSize size);
int SceSysconForDriver_CF5B2F2F(int *status);		/* wake-up factor */
int SceSysconForDriver_A2FE9BF9(int *status);		/* charger status */
int SceSysconForDriver_3FDD29D6(SceUInt32 mask, int enabled);

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
int ksceLedSetMode(int led, int mode, void *config);

int kscePowerSetPowerSwMode2(int mode);
int scePowerSetArmClockFrequency(int mhz);
int scePowerSetBusClockFrequency(int mhz);
int kscePowerSetGpuClockFrequencyInternal(int core, int mhz);
int scePowerSetGpuXbarClockFrequency(int mhz);
int kscePowerSetCameraBusClockFrequency(int mhz);
int scePowerIsBatteryExist(void);
int scePowerGetBatteryLifePercent(void);
int scePowerCancelRequest(void);
int scePowerRequestStandby(void);
int scePowerRequestColdReset(void);
int kscePowerBatterySetUsbStatus(int status);

int ScePervasiveForDriver_4F64DEF6(int unk);
int ScePervasiveForDriver_E0711D42(void);
int ScePervasiveForDriver_821AE6C7(int unk);
int ScePervasiveForDriver_F861FEE4(int unk);
int ScePervasiveForDriver_2FB5F88F(int unk);
int ScePervasiveForDriver_B8AF1249(void);
int ScePervasiveForDriver_51981FEE(void);
int ScePervasiveForDriver_146108D4(int unk);
int ScePervasiveForDriver_A3CF9844(int unk);
int kscePervasiveDsiSetPixelClock(int port, int clock);
int ScePervasiveForDriver_3D8A8712(int unk);
int ScePervasiveForDriver_4E43299D(int unk);
int SceClockgenForDriver_5130682D(void);
int SceClockgenForDriver_F0A0242D(void);
int SceClockgenForDriver_82F39378(void);
int ksceClockgenWlanBtClkDisable(void);
int ksceGpioSetPortMode(int bus, int port, int mode);
int ksceGpioPortSet(int bus, int port);
int SceUdcdForDriver_2D91F8C3(int chargeEnable);
int ksceUdcdGetDeviceState(void);
