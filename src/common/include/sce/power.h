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
