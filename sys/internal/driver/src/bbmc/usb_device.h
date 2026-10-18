#ifndef BBMC_USB_DEVICE_H
#define BBMC_USB_DEVICE_H

#include <stddef.h>
#include <udcd/udcd.h>

typedef struct BbmcUsbDeviceRequestSlot {
	SceUdcdDeviceRequest request;
	unsigned char reserved[20];
} BbmcUsbDeviceRequestSlot;

typedef struct BbmcUsbSerialDevice {
	int bus;
	int connected;
	SceUID memoryBlock;
	SceUID event;
	SceUID thread;
	BbmcUsbDeviceRequestSlot controlIn;
	BbmcUsbDeviceRequestSlot controlOut;
	BbmcUsbDeviceRequestSlot qcomIn;
	BbmcUsbDeviceRequestSlot qcomOut;
	BbmcUsbDeviceRequestSlot atInterruptIn;
	BbmcUsbDeviceRequestSlot atOut;
	BbmcUsbDeviceRequestSlot atIn;
	void *controlInBuffer;
	void *controlOutBuffer;
	void *qcomOutBuffer;
	void *qcomInBuffer;
	void *atInterruptBuffer;
	void *atOutBuffer;
	void *atInBuffer;
	int qcomReadPending;
	int atInterruptPending;
	int atReadPending;
	SceUdcdEP0DeviceRequest controlInSetup;
	SceUdcdEP0DeviceRequest controlOutSetup;
} BbmcUsbSerialDevice;

_Static_assert(sizeof(SceUdcdDeviceRequest) == 44, "UDCD request");
_Static_assert(sizeof(BbmcUsbDeviceRequestSlot) == 64, "USB device request slot");
_Static_assert(sizeof(BbmcUsbSerialDevice) == 524, "USB serial device");
_Static_assert(offsetof(BbmcUsbSerialDevice, controlIn) == 20, "USB control IN");
_Static_assert(offsetof(BbmcUsbSerialDevice, controlOut) == 84, "USB control OUT");
_Static_assert(offsetof(BbmcUsbSerialDevice, qcomIn) == 148, "USB Qcom IN");
_Static_assert(offsetof(BbmcUsbSerialDevice, qcomOut) == 212, "USB Qcom OUT");
_Static_assert(offsetof(BbmcUsbSerialDevice, atInterruptIn) == 276, "USB AT interrupt");
_Static_assert(offsetof(BbmcUsbSerialDevice, atOut) == 340, "USB AT OUT");
_Static_assert(offsetof(BbmcUsbSerialDevice, atIn) == 404, "USB AT IN");
_Static_assert(offsetof(BbmcUsbSerialDevice, controlInBuffer) == 468, "USB buffers");
_Static_assert(offsetof(BbmcUsbSerialDevice, controlInSetup) == 508, "USB control setup");

extern BbmcUsbSerialDevice *g_usbSerialDevice;
extern SceUdcdEndpoint g_usbSerialEndpoints[6];

#endif
