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

int ksceIdStorageLookup(SceUInt32 leaf, SceUInt32 offset, void *buffer, SceUInt32 size);
