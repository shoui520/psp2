/* SceIdStorage kernel interfaces absent from the pinned public SDK. */
#pragma once
#include <psp2kern/types.h>

/* Leaf 0x114: physical camera, accelerometer and gyro locations. */
typedef struct DeviceLocation {
	SceUInt32 type;
	SceInt32 x;
	SceInt32 y;
	SceInt32 z;
} DeviceLocation;

_Static_assert(sizeof(DeviceLocation) == 16, "DeviceLocation size");

int sceIdStorageLookup(SceUInt32 leaf, SceUInt32 offset, void *buffer, SceUInt32 size);

int SceIdStorageForDriver_08A471A6(uint32_t leaf);
int SceIdStorageForDriver_1FA4D135(SceSize leaf, const void *buffer);
int SceIdStorageForDriver_2C97AB36(uint32_t leaf);
int SceIdStorageForDriver_2D633688(void);
int SceIdStorageForDriver_31E08AFB(void);
int SceIdStorageForDriver_37833CB8(void);
int SceIdStorageForDriver_3AD32523(void);
int SceIdStorageForDriver_60B647A5(int flush);
int SceIdStorageForDriver_683AAC10(uint32_t leaf, uint32_t offset, const void *buffer, uint32_t size);
int SceIdStorageForDriver_958089DB(void);
int SceIdStorageForDriver_99ACCB71(const uint16_t *leaves, int count);
int SceIdStorageForDriver_B9069BAD(void);
int SceIdStorageForDriver_EB00C509(SceSize leaf, void *buffer);
int SceIdStorageForDriver_EB830733(void);
int SceIdStorageForDriver_F4BCB3EE(void);
int SceIdStorageForDriver_FEFA40C2(void);
