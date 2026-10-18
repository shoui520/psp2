/* ScePower kernel interfaces absent from the pinned public SDK. */
#pragma once

#include <psp2kern/types.h>
#include <stdint.h>

typedef void (*PowerWirelessCallback)(int enabled, void *argument);
typedef void (*PowerProcessFlagsCallback)(SceUID pid, uint32_t flags, void *argument);

int scePowerRegisterCallback(SceUID callbackId);
int ScePowerForDriver_C743E392(int mhz);
int ScePowerForDriver_6BC26FC7(uint32_t timerId, uint64_t interval);
uint32_t ScePowerForDriver_9F26222A(void);
uint32_t ScePowerForDriver_D8759B55(void);
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
int ScePowerForDriver_90285886(void);
uint32_t ScePowerForDriver_733F973B(void);
int ScePowerForDriver_E4E3C316(uint32_t status);
int ScePowerForDriver_ACC857A4(void);
int scePowerGetGpuClockFrequencyInternal(uint32_t *first, uint32_t *second);
int ScePowerForDriver_642E0AF2(SceUID pid);
uint32_t ScePowerForDriver_64641E6A(void);
int ScePowerForDriver_08951418(void);
int ScePowerForDriver_8921A7A0(uint32_t argument);
int ScePowerForDriver_FFC84E69(int frequency);
int ScePowerForDriver_264C24FC(int first, int second);
int ScePowerForDriver_B104EFE2(SceUID pid, int frequency);
int ScePowerForDriver_C1853BA7(uint32_t mode);
int ScePowerForDriver_0CD21B1F(uint32_t mode);
int ScePowerForDriver_CF8F0529(uint32_t time);
int ScePowerForDriver_675A84ED(uint32_t time);
int ScePowerForDriver_621BD8FD(int frequency);
int ScePowerForDriver_E5573571(int frequency);
int ScePowerForDriver_EFD3C963(uint32_t mask);
uint32_t scePowerCancelRequest(void);
int scePowerGetBatteryLifePercent(void);
uint32_t scePowerGetGpuXbarClockFrequency(void);
int scePowerIsBatteryExist(void);
int scePowerRequestColdReset(void);
int scePowerRequestStandby(void);
int scePowerSetArmClockFrequency(int frequency);
int scePowerSetBusClockFrequency(int frequency);
int scePowerSetGpuXbarClockFrequency(int frequency);

uint32_t ScePowerForDriver_0074EF9B(void);
int ScePowerForDriver_02DB1035(void);
int ScePowerForDriver_0D56C601(void);
int ScePowerForDriver_0D80B917(SceUID pid);
int ScePowerForDriver_0E333BEC(int frequency);
uint32_t ScePowerForDriver_0E58FCDF(void);
uint32_t ScePowerForDriver_165CE085(void);
int ScePowerForDriver_166922EC(void);
int ScePowerForDriver_20A33D58(SceUID pid, int frequency);
int ScePowerForDriver_23BB0A60(void);
int ScePowerForDriver_2777A517(SceUID pid);
int ScePowerForDriver_2784A6BD(SceUID pid);
int ScePowerForDriver_2875994B(void);
int ScePowerForDriver_2B51FE2F(void);
int ScePowerForDriver_2E9000F7(SceUID pid);
int ScePowerForDriver_38415146(uint32_t value);
int ScePowerForDriver_3951AF53(void);
int ScePowerForDriver_4159E0D9(SceUID pid);
int ScePowerForDriver_43D5CE1D(uint32_t brightness);
int ScePowerForDriver_4A69163A(SceUID pid, uint32_t enabled);
int ScePowerForDriver_5210A6FE(SceUID pid);
int ScePowerForDriver_627A89C6(void);
int ScePowerForDriver_62C5406C(SceUID pid);
int ScePowerForDriver_660D5AB4(uint32_t timerId);
int ScePowerForDriver_661D670D(SceUID pid, uint32_t enabled);
uint32_t ScePowerForDriver_668F01D4(void);
int ScePowerForDriver_6D2CA84B(void);
int ScePowerForDriver_77027B6B(uint32_t limit);
int ScePowerForDriver_86CB5218(SceUID pid, int frequency);
int ScePowerForDriver_8D18F728(SceUID pid, int frequency);
int ScePowerForDriver_991BDEAF(uint32_t timerId, uint32_t mask);
int ScePowerForDriver_A6FF5997(uint32_t state);
int ScePowerForDriver_A902CDDF(int frequency);
int ScePowerForDriver_C62B6164(void (*callback)(uint32_t, void *), uint32_t threshold, void *argument);
uint32_t ScePowerForDriver_C63DACD5(void);
int ScePowerForDriver_C715F4F0(SceUID pid);
uint32_t ScePowerForDriver_CBCC11CC(void);
int ScePowerForDriver_D516D446(uint32_t *state);
int ScePowerForDriver_DD3D4DAC(void);
int ScePowerForDriver_EDC13FE5(uint32_t timerId, uint64_t *elapsedOut, uint64_t *remainingOut);
uint32_t ScePowerForDriver_F1E14EA9(void);
int ScePowerForDriver_FB5C3C75(SceUID pid, uint32_t unused, uint64_t interval);
int scePowerBatteryUpdateInfo(void);
int scePowerGetBatteryChargingStatus(void);
int scePowerGetBatteryCycleCount(void);
int scePowerGetBatteryElec(uint32_t *current);
int scePowerGetBatteryFullCapacity(void);
int scePowerGetBatteryLifeTime(void);
int scePowerGetBatteryRemainCapacity(void);
int scePowerGetBatteryRemainLevel(void);
int scePowerGetBatteryRemainMaxLevel(void);
int scePowerGetBatterySOH(void);
int scePowerGetBatteryTemp(void);
int scePowerGetBatteryVolt(void);
int scePowerGetCaseTemp(void);
int scePowerGetGpuClockFrequency(void);
int scePowerGetUsingWireless(void);
int scePowerIsBatteryCharging(void);
int scePowerIsLowBattery(void);
int scePowerIsLowBatteryInhibitUpdateDownload(void);
int scePowerIsLowBatteryInhibitUpdateReboot(void);
int scePowerIsRequest(void);
int scePowerIsSuspendRequired(void);
int scePowerRequestDisplayOff(void);
int scePowerRequestDisplayOn(void);
int scePowerRequestSuspend(void);
int scePowerSetGpuClockFrequency(int frequency);
int scePowerSetIdleTimerCount(uint64_t interval);
int scePowerUnregisterCallback(SceUID callback);

uint32_t scePowerGetArmClockFrequency(void);
uint32_t scePowerGetBusClockFrequency(void);
