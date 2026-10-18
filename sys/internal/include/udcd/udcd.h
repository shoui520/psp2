#pragma once

#include <psp2kern/types.h>
#include <psp2kern/udcd.h>	/* the types, until ours */
#include <stdint.h>

struct udcdDeviceInformation;

int SceUdcdForDriver_03781649(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_0DECE532(SceUdcdDriver *driver);
int SceUdcdForDriver_0F3595AE(int bus);
int SceUdcdForDriver_1494293B(const char *name, int args, void *argp);
int SceUdcdForDriver_150FD3BB(const char *name, int args, void *argp, int bus);
int SceUdcdForDriver_175E6179(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_1A5A3851(void);
int SceUdcdForDriver_1C684884(unsigned int timeout, int bus);
int SceUdcdForDriver_1ED0E89E(SceUdcdDeviceRequest *request);
int SceUdcdForDriver_28BFEC45(void);
int SceUdcdForDriver_2A234CAF(int bus);
int SceUdcdForDriver_2D91F8C3(unsigned int enabled);
int SceUdcdForDriver_2E3E622A(SceUdcdDeviceRequest *request, int bus);
int SceUdcdForDriver_34079250(SceUdcdEndpoint *endpoint);
int SceUdcdForDriver_360E95B9(int unused, int bus);
int SceUdcdForDriver_38787672(SceUdcdEndpoint *endpoint);
int SceUdcdForDriver_409DD196(int bus);
int SceUdcdForDriver_45EB0177(SceUdcdDeviceInfo *information, int bus);
int SceUdcdForDriver_4E55244D(SceUdcdDriver *driver);
int SceUdcdForDriver_512F77BC(const char *name, int bus);
int SceUdcdForDriver_64675918(SceUdcdDriver *driver, int bus);
int SceUdcdForDriver_6CCD152E(SceUdcdDriver *driver, int bus);
int SceUdcdForDriver_78BF2A28(const struct udcdDeviceInformation *information);
int SceUdcdForDriver_7AD0C8D1(SceUdcdWaitParam *parameter, unsigned int timeout, int bus);
int SceUdcdForDriver_8AE87657(void);
int SceUdcdForDriver_90F8BCAE(const char *name, int args, void *argp, int bus);
int SceUdcdForDriver_9119247B(unsigned int productId, unsigned int policy, int bus);
int SceUdcdForDriver_9B44DF29(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_9E7F7EFE(void);
int SceUdcdForDriver_9F53D64D(SceUdcdEndpoint *endpoint);
int SceUdcdForDriver_A35F3BAF(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_AA5FD90E(SceUdcdDeviceRequest *request);
void SceUdcdForDriver_B1454F11(const unsigned int *value);
int SceUdcdForDriver_B1C2EA5A(SceUdcdEndpoint *endpoint, int bus);
int SceUdcdForDriver_C0CA5DDB(const char *name);
int SceUdcdForDriver_C60A74B2(SceUdcdDeviceRequest *request);
int SceUdcdForDriver_D01CFD22(void);
int SceUdcdForDriver_D03017C0(SceUdcdWaitParam *parameter, unsigned int timeout);
int SceUdcdForDriver_DB7EF7AB(int bus);
int SceUdcdForDriver_E054B5E4(void);
int SceUdcdForDriver_E4F32EDA(const struct udcdDeviceInformation *information, int bus);
int SceUdcdForDriver_E9E75C49(int bus);
int SceUdcdForDriver_EBB1E86B(void);
int SceUdcdForDriver_FBEA3703(SceUdcdDeviceInfo *information);
int SceUdcdForDriver_FDC10F93(int bus);
int SceUdcd_F1A3690B(void);
int SceUdcd_FCD876FB(unsigned int enabled);
int sceUdcdActivate(unsigned int productId);
int sceUdcdGetDrvState(const char *driverName);
int sceUdcdRegisterCallback(SceUID callback, unsigned int mask);
int sceUdcdStart(const char *name, int args, void *argp);
int sceUdcdUnregisterCallback(SceUID callback);
