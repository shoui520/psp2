/* Private imports of SceUsbEtherSmsc; unidentified APIs retain their NIDs. */
#pragma once
#include <psp2kern/types.h>
#include <syscon/syscon.h>
#include <idstorage/idstorage.h>

int ScePowerForDriver_B24263F0(int kind, void (*handler)(int event, int value));
int ksceSysconCtrlDolceUsbPower(int enable);
int ksceSysconIsDownLoaderMode(void);
int ksceSysconGetHardwareInfo(void);
int sceKernelDelayThread(unsigned int delay);
SceUInt64 sceKernelGetSystemTimeWide(void);
int sceKernelExitThread(int status);
unsigned int __aeabi_uidiv(unsigned int numerator, unsigned int denominator);
int ksceKernelChangeThreadCpuAffinityMask(SceUID thread, unsigned int mask);
