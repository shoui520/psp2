/* SceSysmem's debug LEDs and switches (SceDebugLed, SceDebugLedForDriver): the GPI and GPO words. */

#pragma once

#include <scetypes.h>
#include <stdint.h>

SceUInt32 sceKernelGetGPI(void);
int sceKernelSetGPO(SceUInt32 gpo);

/* vitasdk: ksceKernelSetGPOMask */
int SceDebugLedForDriver_098473B0(SceUInt32 clear, SceUInt32 set);
/* vitasdk: ksceDebugLedRegisterHandle0 */
void SceDebugLedForDriver_24173819(void (*handle)(int a1, int a2, int a3, int a4));
SceUInt32 sceKernelGetGPO(void);
void sceKernelSetGPI(SceUInt32 gpi);
/* vitasdk: ksceDebugLedRegisterHandle1 */
void SceDebugLedForDriver_F62154E7(void (*handle)(int a1, int a2, int a3, int a4));
void sceDebugLedInvokeHandle0(int a1, int a2, int a3, int a4);
void sceDebugLedInvokeHandle1(int a1, int a2, int a3, int a4);

/* SceDebugLed (the syscall library) has its own copies of the four calls above */
SceUInt32 sceKernelGetGPIForUser(void);
int sceKernelSetGPOForUser(SceUInt32 gpo);
void sceDebugLedInvokeHandle0ForUser(int a1, int a2, int a3, int a4);
void sceDebugLedInvokeHandle1ForUser(int a1, int a2, int a3, int a4);
