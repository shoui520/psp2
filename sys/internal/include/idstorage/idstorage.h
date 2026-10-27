/* SceIdStorage kernel interfaces absent from the pinned public SDK. */
#pragma once
#include <scetypes.h>
#include <stdint.h>

/* Leaf 0x114: physical camera, accelerometer and gyro locations. */
typedef struct DeviceLocation {
	SceUInt32 type;
	SceInt32 x;
	SceInt32 y;
	SceInt32 z;
} DeviceLocation;

_Static_assert(sizeof(DeviceLocation) == 16, "DeviceLocation size");

int sceIdStorageLookup(SceUInt32 leaf, SceUInt32 offset, void *buffer, SceUInt32 size);

int sceIdStorageCreateLeaf(uint32_t leaf);
int sceIdStorageWriteLeaf(SceSize leaf, const void *buffer);
int sceIdStorageDeleteLeaf(uint32_t leaf);
int sceIdStorageIsReadOnly(void);
int SceIdStorageForDriver_31E08AFB(void);	/* vitasdk: ksceIdStorageEnumId */
int sceIdStorageGetFreeLeaves(void);
int sceIdStorageFlush(void);
int sceIdStorageRestart(int flush);
int sceIdStorageUpdate(uint32_t leaf, uint32_t offset, const void *buffer, uint32_t size);
int sceIdStorageFormat(void);
int sceIdStorageCreateAtomicLeaves(const uint16_t *leaves, int count);
int sceIdStorageIsDirty(void);
int sceIdStorageReadLeaf(SceSize leaf, void *buffer);
int sceIdStorageGetLeafSize(void);
int sceIdStorageUnformat(void);
int sceIdStorageIsFormatted(void);
