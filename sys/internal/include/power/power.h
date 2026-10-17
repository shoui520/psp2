/* ScePower kernel interfaces absent from the pinned public SDK. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

typedef void (*PowerWirelessCallback)(int enabled, void *argument);
typedef void (*PowerProcessFlagsCallback)(SceUID pid, uint32_t flags, void *argument);

int scePowerRegisterCallback(SceUID callbackId);
int kscePowerSetCameraBusClockFrequency(int mhz);
int kscePowerSetIdleTimer(uint32_t timerId, uint64_t interval);
uint32_t kscePowerGetWakeupFactor(void);
uint32_t kscePowerGetDisplayMaxBrightness(void);
int scePowerIsPowerOnline(void);
int ScePowerForDriver_FE35B73D(PowerWirelessCallback callback, void *argument);
int ScePowerForDriver_B24263F0(unsigned int index,
	PowerProcessFlagsCallback callback, void *argument);
SceUInt32 ScePowerForDriver_394DE492(void);

int ScePowerForDriver_06AF03DB(SceUID pid);
int ScePowerForDriver_711C68DF(void (*before)(uint32_t, uint32_t, int, void *), void *beforeArgument,
			      void (*after)(uint32_t, uint32_t, int, void *), void *afterArgument);
int ScePowerForDriver_8C0D2051(uint32_t *out);
int ScePowerForDriver_F535D928(uint32_t mask, int enabled);
int kscePowerBatteryDisableUsbCharging(void);
uint32_t kscePowerBatteryEnableUsbCharging(void);
int kscePowerBatterySetUsbStatus(uint32_t status);
int kscePowerBatteryStopUsbCharging(void);
int kscePowerGetGpuClockFrequencyInternal(uint32_t *first, uint32_t *second);
int kscePowerGetGpuClockFrequencyProc(SceUID pid);
uint32_t kscePowerGetVeneziaClockFrequency(void);
int kscePowerRequestHibernate(void);
int kscePowerRequestShutdown(uint32_t argument);
int kscePowerSetCompatClockFrequency(int frequency);
int kscePowerSetGpuClockFrequencyInternal(int first, int second);
int kscePowerSetGpuClockFrequencyProc(SceUID pid, int frequency);
int kscePowerSetPowerSwMode(uint32_t mode);
int kscePowerSetPowerSwMode2(uint32_t mode);
int kscePowerSetPsButtonPushTime(uint32_t time);
int kscePowerSetStandbyButtonPushTime(uint32_t time);
int kscePowerSetVeneziaClockFrequency(int frequency);
int kscePowerSetVipClockFrequency(int frequency);
int kscePowerTick(uint32_t mask);
uint32_t scePowerCancelRequest(void);
int scePowerGetBatteryLifePercent(void);
int scePowerGetGpuXbarClockFrequency(void);
int scePowerIsBatteryExist(void);
int scePowerRequestColdReset(void);
int scePowerRequestStandby(void);
int scePowerSetArmClockFrequency(int frequency);
int scePowerSetBusClockFrequency(int frequency);
int scePowerSetGpuXbarClockFrequency(int frequency);
