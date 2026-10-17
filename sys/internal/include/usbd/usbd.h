/* USBD device and composite-driver interfaces shared with their clients. */
#pragma once

#include <psp2common/usbd.h>

typedef struct SceUsbdCompositeDriver {
	const char *name;
	int (*probe)(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
	int (*attach)(int deviceId);
	int (*detach)(int deviceId);
} SceUsbdCompositeDriver;

_Static_assert(sizeof(SceUsbdCompositeDriver) == 16, "USBD composite driver");

int ksceUsbdGetDeviceLocation(SceUID deviceId, SceUInt32 *location);
int ksceUsbdGetDeviceSpeed(int deviceId, SceUInt32 *speed);
int ksceUsbdRegisterCompositeLdd(const SceUsbdCompositeDriver *driver);
int ksceUsbdUnregisterCompositeLdd(const SceUsbdCompositeDriver *driver);
