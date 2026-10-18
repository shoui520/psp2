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

int SceIdStorageForDriver_08A471A6(uint32_t leaf);	/* vitasdk: ksceIdStorageCreateLeaf */
/* vitasdk: ksceIdStorageWriteLeaf */
int SceIdStorageForDriver_1FA4D135(SceSize leaf, const void *buffer);
int SceIdStorageForDriver_2C97AB36(uint32_t leaf);	/* vitasdk: ksceIdStorageDeleteLeaf */
int SceIdStorageForDriver_2D633688(void);	/* vitasdk: ksceIdStorageIsReadOnly */
int SceIdStorageForDriver_31E08AFB(void);	/* vitasdk: ksceIdStorageEnumId */
int SceIdStorageForDriver_37833CB8(void);	/* vitasdk: ksceIdStorageGetFreeLeaves */
int SceIdStorageForDriver_3AD32523(void);	/* vitasdk: ksceIdStorageFlush */
int SceIdStorageForDriver_60B647A5(int flush);	/* vitasdk: ksceIdStorageRestart */
/* vitasdk: ksceIdStorageUpdate */
int SceIdStorageForDriver_683AAC10(uint32_t leaf, uint32_t offset, const void *buffer, uint32_t size);
int SceIdStorageForDriver_958089DB(void);	/* vitasdk: ksceIdStorageFormat */
/* vitasdk: ksceIdStorageCreateAtomicLeaves */
int SceIdStorageForDriver_99ACCB71(const uint16_t *leaves, int count);
int SceIdStorageForDriver_B9069BAD(void);	/* vitasdk: ksceIdStorageIsDirty */
int SceIdStorageForDriver_EB00C509(SceSize leaf, void *buffer);	/* vitasdk: ksceIdStorageReadLeaf */
int SceIdStorageForDriver_EB830733(void);	/* vitasdk: ksceIdStorageGetLeafSize */
int SceIdStorageForDriver_F4BCB3EE(void);	/* vitasdk: ksceIdStorageUnformat */
int SceIdStorageForDriver_FEFA40C2(void);	/* vitasdk: ksceIdStorageIsFormatted */
