/* SDK compatibility for the canonical composite-driver interface. */
#pragma once

#define SceUsbdCompositeDriver SceUsbdCompositeDriver_vitasdk
#define ksceUsbdGetDeviceLocation ksceUsbdGetDeviceLocation_vitasdk
#define ksceUsbdGetDeviceSpeed ksceUsbdGetDeviceSpeed_vitasdk
#define ksceUsbdRegisterCompositeLdd ksceUsbdRegisterCompositeLdd_vitasdk
#define ksceUsbdUnregisterCompositeLdd ksceUsbdUnregisterCompositeLdd_vitasdk
#include_next <psp2kern/usbd.h>
#undef SceUsbdCompositeDriver
#undef ksceUsbdGetDeviceLocation
#undef ksceUsbdGetDeviceSpeed
#undef ksceUsbdRegisterCompositeLdd
#undef ksceUsbdUnregisterCompositeLdd

#include <sce/usbd.h>
