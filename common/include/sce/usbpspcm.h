/* Private SceUsbPspcm imports; Sony names unknown. */
#pragma once
#include <psp2kern/udcd.h>
int SceUdcdForDriver_78BF2A28(void *info);
int SceUdcdForDriver_AA5FD90E(SceUdcdDeviceRequest *request);
int sceKernelCancelSema(SceUID uid, int count, int *waiters);
