/* SceDs3 USB transport; the original filename is not recorded. */
#include "ds3_private.h"
#include <ctrl/ctrl.h>
#include <regmgr/regmgr.h>
#include <bt/bt_registered.h>
#include <bt/bt_event.h>
#include <bt/bt_test.h>
#include <sysmem/debug.h>
#include <udcd/udcd_imports.h>
#include <threadmgr/threadmgr.h>
#include <motion/motion_dev.h>
#include <touch_dummy/touch_dummy.h>
#include <touch/touch.h>
#include <sysmem/sysmem.h>
#include <syscon/syscon.h>
#include <sysmem/cpu.h>
#include <sysmem/suspend.h>
#include <sysmem/procevent.h>
#include <sysmem/sysroot.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/callback.h>
#include <psp2kern/usbserv.h>
#include <psp2kern/registrymgr.h>
#include <display/display.h>
#include <string.h>

static const uint8_t ds3EmptyBluetoothAddress[6] = {0};

static const SceUsbdDriver ds3UsbDriver = {
	.name = "ds3",
	.probe = ds3UsbProbe,
	.attach = ds3UsbAttach,
	.detach = ds3UsbDetach,
};

static Ds3UsbState ds3Usb;

int ds3UsbDetach(int deviceId)
{
	Ds3UsbDevice *device;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	device = &ds3Usb.devices[index];
	if (ds3IsSafeMode() != 0) {
		uint64_t address = ((uint64_t)device->deviceAddress[0] << 40) |
			((uint64_t)device->deviceAddress[1] << 32) |
			((uint64_t)device->deviceAddress[2] << 24) |
			((uint64_t)device->deviceAddress[3] << 16) |
			((uint64_t)device->deviceAddress[4] << 8) |
			device->deviceAddress[5];

		ds3RemoveTrackedAddress(address);
	}
	ds3ReleaseDevice(2, deviceId, 0);
	ksceUsbdClosePipe(device->controlPipe);
	ksceUsbdClosePipe(device->inputPipe);
	ksceUsbdClosePipe(device->outputPipe);
	memset(device, 0, sizeof(*device));
	device->deviceId = DS3_USB_DEVICE_UNUSED;
	return 0;
}

int ds3UsbProbe(int deviceId)
{
	const SceUsbdDeviceDescriptor *descriptor;
	const uint8_t *version;
	Ds3UsbDevice *device;
	uint16_t productId;
	uint8_t versionLow, versionHigh;
	int index;

	descriptor = ksceUsbdScanStaticDescriptor(deviceId, NULL,
		SCE_USBD_DESCRIPTOR_DEVICE);
	if (descriptor == NULL || descriptor->idVendor != 0x054c)
		return -1;
	productId = descriptor->idProduct;
	if (productId != 0x042f && productId != 0x0268 && productId != 0x05c4)
		return -1;
	if (ds3Usb.devices[0].deviceId == DS3_USB_DEVICE_UNUSED)
		index = 0;
	else if (ds3Usb.devices[1].deviceId == DS3_USB_DEVICE_UNUSED)
		index = 1;
	else if (ds3Usb.devices[2].deviceId == DS3_USB_DEVICE_UNUSED)
		index = 2;
	else if (ds3Usb.devices[3].deviceId == DS3_USB_DEVICE_UNUSED)
		index = 3;
	else {
		SceCtrlForDriver_6E183B9B((int)0x80340101);
		return -1;
	}
	device = &ds3Usb.devices[index];
	device->deviceId = deviceId;
	device->idProduct = productId;
	device->idVendor = 0x054c;
	version = (const uint8_t *)&descriptor->bcdDevice;
	versionLow = version[0];
	versionHigh = version[1];
	device->controlPipe = -1;
	device->inputPipe = -1;
	device->outputPipe = -1;
	device->bcdDevice = versionLow | (versionHigh << 8);
	return 0;
}

int ds3UsbAttach(int deviceId)
{
	Ds3UsbDevice *device;
	Ds3Device *controller;
	SceUsbdConfigurationDescriptor *configuration = NULL;
	SceUsbdConfigurationDescriptor *descriptor = NULL;
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdDeviceRequest request;
	void *cursor;
	unsigned int packetSize;
	int index, endpointIndex;
	int result;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	device = &ds3Usb.devices[index];
	while ((descriptor = ksceUsbdScanStaticDescriptor(deviceId,
		descriptor, 0)) != NULL) {
		if (descriptor->bLength > 1 && descriptor->bDescriptorType ==
			SCE_USBD_DESCRIPTOR_CONFIGURATION && configuration == NULL)
			configuration = descriptor;
	}
	if (configuration == NULL)
		return -1;
	cursor = configuration;
	do {
		interface = ksceUsbdScanStaticDescriptor(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		if (interface == NULL)
			return -1;
		cursor = interface;
	} while (interface->bInterfaceClass != SCE_USBD_CLASS_HID);
	for (endpointIndex = 0; endpointIndex < interface->bNumEndpoints;
		endpointIndex++) {
		endpoint = ksceUsbdScanStaticDescriptor(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_ENDPOINT);
		if (endpoint == NULL)
			return -1;
		cursor = endpoint;
		if ((endpoint->bmAttributes & SCE_USBD_ENDPOINT_TRANSFER_TYPE_BITS)
			!= SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
			continue;
		if ((int8_t)endpoint->bEndpointAddress < 0) {
			if (device->inputPipe >= 0)
				continue;
			device->inputPipe = ksceUsbdOpenPipe(deviceId, endpoint);
			if (device->inputPipe < 0)
				return -1;
			packetSize = endpoint->wMaxPacketSize;
			if (packetSize > 64)
				packetSize = 64;
			device->inputPacketSize = packetSize;
		} else if (device->outputPipe < 0) {
			device->outputPipe = ksceUsbdOpenPipe(deviceId, endpoint);
			if (device->outputPipe < 0)
				return -1;
		}
	}
	if (device->inputPipe < 0)
		return -1;
	device->controlPipe = ksceUsbdOpenPipe(deviceId, NULL);
	if (device->controlPipe < 0)
		return -1;
	if (ds3AllocateDevice(1, deviceId, 0) < 0)
		return -1;
	controller = ds3FindUsbDevice(deviceId);
	if (controller == NULL)
		return -1;
	controller->usbState = 3;
	controller->idProduct = device->idProduct;
	request.bmRequestType = 0;
	request.wIndex = 0;
	request.wLength = 0;
	request.wValue = configuration->bConfigurationValue;
	request.bRequest = SCE_USBD_REQUEST_SET_CONFIGURATION;
	result = ksceUsbdControlTransfer(device->controlPipe, &request,
		NULL, ds3UsbSetupComplete, controller);
	if (result != 0) {
		ds3ReleaseDevice(2, deviceId, 0);
		return -1;
	}
	device->interfaceNumber = interface->bInterfaceNumber;
	device->alternateSetting = interface->bAlternateSetting;
	return result;
}

int ds3UsbSetHostAddress(Ds3Device *controller)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wLength = 8;
	request.wIndex = 0;
	request.wValue = 0x03f5;
	request.bRequest = 9; /* HID SET_REPORT. */
	report[1] = 0;
	request.bmRequestType = 0x21;
	report[0] = 1;
	report[2] = ds3Usb.localAddress[0];
	report[3] = ds3Usb.localAddress[1];
	report[4] = ds3Usb.localAddress[2];
	report[5] = ds3Usb.localAddress[3];
	report[6] = ds3Usb.localAddress[4];
	report[7] = ds3Usb.localAddress[5];
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

int ds3UsbGetFeatureReport(Ds3Device *controller, uint16_t reportId,
	uint16_t length)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wIndex = 0;
	request.wValue = reportId | 0x0300;
	request.wLength = length;
	request.bmRequestType = 0xa1;
	request.bRequest = 1; /* HID GET_REPORT. */
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

int ds3UsbReadInput(unsigned int port, Ds3UsbInputReport *report)
{
	Ds3Device *controller;
	Ds3UsbDevice *device;
	uint8_t *psButtons;
	int deviceId, index, intr;

	if (port > 6 || report == NULL)
		return -1;
	controller = ds3FindDeviceByPort(port);
	if (controller == NULL || controller->inputReady == 0)
		return 0;
	intr = SceCpuForDriver_D32ACE9E(&ds3Usb.inputLock);
	*report = *(Ds3UsbInputReport *)
		controller->inputRequests[controller->inputIndex]->buffer;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3Usb.inputLock, intr);
	if (controller->idProduct == 0x0268) {
		if (report->bytes[6] == 0 && report->bytes[7] == 0 && report->bytes[8] == 0 &&
			report->bytes[9] == 0) {
			report->bytes[9] = 0x80;
			report->bytes[8] = 0x80;
			report->bytes[7] = 0x80;
			report->bytes[6] = 0x80;
		}
		psButtons = &report->bytes[4];
	} else if (controller->idProduct == 0x05c4) {
		psButtons = &report->bytes[7];
	} else {
		return 64;
	}
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return 0;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return 0;
	device = &ds3Usb.devices[index];
	if (device->psSuppressionCount != 0) {
		device->psSuppressionCount--;
		if (device->psSuppressionCount == 0 && (*psButtons & 1) != 0)
			device->suppressPsUntilRelease = 1;
		else
			*psButtons &= ~1;
	}
	if (device->suppressPsUntilRelease != 0) {
		if ((*psButtons & 1) != 0)
			*psButtons &= ~1;
		else
			device->suppressPsUntilRelease = 0;
	}
	return 64;
}

int ds3UsbSetLeds(Ds3Device *controller, const Ds3LedState *state)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wValue = 0x0201;
	request.bmRequestType = 0x21;
	request.bRequest = 9; /* HID SET_REPORT. */
	request.wLength = 48;
	request.wIndex = 0;
	ds3BuildUsbOutputReport(controller, report);
	report[9] = state->mask;
	report[25] = state->patterns[0].period;
	report[26] = state->patterns[0].duration >> 8;
	report[27] = state->patterns[0].duration;
	report[28] = state->patterns[0].offTime;
	report[29] = state->patterns[0].onTime;
	report[20] = state->patterns[1].period;
	report[21] = state->patterns[1].duration >> 8;
	report[22] = state->patterns[1].duration;
	report[23] = state->patterns[1].offTime;
	report[24] = state->patterns[1].onTime;
	report[15] = state->patterns[2].period;
	report[16] = state->patterns[2].duration >> 8;
	report[17] = state->patterns[2].duration;
	report[18] = state->patterns[2].offTime;
	report[19] = state->patterns[2].onTime;
	report[10] = state->patterns[3].period;
	report[11] = state->patterns[3].duration >> 8;
	report[12] = state->patterns[3].duration;
	report[13] = state->patterns[3].offTime;
	report[14] = state->patterns[3].onTime;
	ds3UpdateUsbOutputState(controller, report);
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

void ds3UsbAdvanceSetup(Ds3Device *controller, int transferred)
{
	union {
		SceBtRegisteredInfo info;
		Ds3LedState leds;
	} scratch;
	SceUsbdDeviceRequest request;
	Ds3UsbDevice *usb;
	Ds3Device *bluetooth;
	uint8_t peerAddress[6];
	uint8_t previousHostAddress[6];
	uint8_t hostAddress[6];
	uint8_t *report;
	void *input;
	uint64_t address;
	int deviceId = controller->usbDeviceId;
	int index;
	int axis;
	int interruptState;
	uint8_t command;

	if (deviceId == DS3_USB_DEVICE_UNUSED ||
		(deviceId != ds3Usb.devices[0].deviceId &&
		 deviceId != ds3Usb.devices[1].deviceId &&
		 deviceId != ds3Usb.devices[2].deviceId &&
		 deviceId != ds3Usb.devices[3].deviceId))
		return;
	switch (controller->setupStage) {
	case 30:
		address = controller->deviceAddress;
		memset(&scratch.info, 0, sizeof(scratch.info));
		if (ksceBtGetRegisteredInfo(address, &scratch.info, 1) != 1) {
			request.bmRequestType = 8;
			request.bRequest = 5;
			if (ds3IsSafeMode() == 0 && SceSysrootForDriver_B0E1FC67() == 0 &&
				memcmp(ds3Usb.localAddress, ds3EmptyBluetoothAddress, 6) != 0) {
				deviceId = controller->usbDeviceId;
				if (deviceId == DS3_USB_DEVICE_UNUSED)
					return;
				if (deviceId == ds3Usb.devices[0].deviceId)
					index = 0;
				else if (deviceId == ds3Usb.devices[1].deviceId)
					index = 1;
				else if (deviceId == ds3Usb.devices[2].deviceId)
					index = 2;
				else if (deviceId == ds3Usb.devices[3].deviceId)
					index = 3;
				else
					return;
				usb = &ds3Usb.devices[index];
				for (axis = 0; axis < 6; axis++) {
					peerAddress[axis] = usb->deviceAddress[5 - axis];
					previousHostAddress[axis] = usb->hostAddress[5 - axis];
				}
				if (SceBtForDriver_10D68FAE(peerAddress, &request, previousHostAddress,
					usb->idVendor, usb->idProduct, usb->bcdDevice,
					"PLAYSTATION(R)3 Controller", hostAddress, usb->linkKey) < 0)
					return;
			}
		}
		if (controller->deviceSubtype != 0) {
			controller->setupStage = 31;
			ds3BuildLedState(30, 0, &scratch.leds);
			ds3UsbSetLeds(controller, &scratch.leds);
			return;
		}
		if (controller->deviceType == 2) {
			controller->setupStage = 36;
			goto startInput;
		}
		bluetooth = ds3FindBtDevice(controller->deviceAddress);
		if (bluetooth != NULL && bluetooth->bluetoothState == 1) {
			controller->setupStage = 34;
			command = 12;
		} else {
			controller->setupStage = 33;
			command = 11;
		}
		deviceId = controller->usbDeviceId;
		goto sendFeature;
	case 31:
		bluetooth = ds3FindBtDevice(controller->deviceAddress);
		if (bluetooth != NULL)
			bluetooth->bluetoothState = 3;
		controller->setupStage = 32;
		command = 9;
		goto sendFeature;
	case 32:
		address = controller->deviceAddress;
		bluetooth = ds3FindBtDevice(address);
		if (bluetooth != NULL)
			bluetooth->bluetoothState = 4;
		ksceBtStartDisconnect((uint32_t)address, address >> 32);
		ds3DetachBluetoothTransport(controller->deviceAddress);
		controller->setupStage = 34;
		deviceId = controller->usbDeviceId;
		command = 12;
		goto sendFeature;
	case 33:
	case 34:
		controller->setupStage = 36;
		goto startInput;
	case 35:
		report = controller->outputRequests[0]->buffer;
		if (report != NULL && transferred > 11) {
			controller->connectionStatus = report[0];
			controller->batteryInfo = report[6];
		}
		controller->setupStage = 36;
		return;
	default:
		return;
	}
sendFeature:
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wIndex = 0;
	request.bmRequestType = 0x21;
	request.bRequest = 9;
	request.wValue = 0x03f4;
	request.wLength = 4;
	report[0] = 0x42;
	report[1] = command;
	ksceUsbdControlTransfer(usb->controlPipe, &request, report,
		ds3UsbSetupComplete, controller);
	return;
startInput:
	if (ds3IsSuspending() != 0)
		return;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	if (usb->inputPipe < 0)
		return;
	interruptState = SceCpuForDriver_D32ACE9E(&ds3Usb.inputLock);
	input = controller->inputRequests[controller->inputIndex]->buffer;
	controller->inputIndex ^= 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3Usb.inputLock, interruptState);
	ksceUsbdInterruptTransfer(usb->inputPipe, input, usb->inputPacketSize,
		ds3UsbInputComplete, controller);
}

void ds3UsbSetupComplete(int result, int transferred, void *argument)
{
	Ds3Device *controller = argument;
	Ds3UsbDevice *usb;
	SceUsbdDeviceRequest request;
	Ds3LedState leds;
	uint8_t firmwareInfo[48];
	uint8_t hostAddress[6];
	uint8_t *report;
	uint8_t *calibration;
	void *input;
	uint64_t address;
	unsigned int stage;
	unsigned int product;
	unsigned int length;
	unsigned int value;
	int deviceId;
	int index;
	int axis;
	int count;
	int interruptState;

	if (controller == NULL || result != 0)
		return;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	product = controller->idProduct;
	if (product != 0x0268 && product != 0x05c4)
		return;
	stage = controller->setupStage;
	if (product == 0x0268 && stage > 14 && stage <= 36) {
		ds3UsbAdvanceSetup(controller, transferred);
		return;
	}
	if (product == 0x0268 && stage > 14 && stage != 51 && stage != 52)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		index = -1;
	usb = index < 0 ? NULL : &ds3Usb.devices[index];
	report = NULL;
	if (product == 0x0268) {
		if (usb == NULL)
			return;
		if (stage <= 14)
			report = controller->outputRequests[0]->buffer;
		switch (stage) {
		case 0:
			if (usb->alternateSetting <= 0) {
				controller->setupStage = 2;
				ds3UsbGetFeatureReport(controller, 1, 50);
				return;
			}
			controller->setupStage = 1;
			request.wValue = usb->alternateSetting;
			request.bmRequestType = 1;
			request.wIndex = usb->interfaceNumber;
			request.wLength = 0;
			request.bRequest = 11;
			ksceUsbdControlTransfer(usb->controlPipe, &request, NULL,
				ds3UsbInputComplete, controller);
			return;
		case 1:
			controller->setupStage = 2;
			value = 0x0301;
			length = 50;
			goto getFeature;
		case 2:
			if (report != NULL && transferred > 49) {
				count = report[37];
				for (axis = 0; axis < count; axis++) {
					unsigned int auxiliary = report[38 + axis];

					if (auxiliary - 6 <= 1)
						controller->auxiliaryMotorReport = auxiliary;
				}
			}
			controller->setupStage = 3;
			value = 0x03f2;
			length = 18;
			goto getFeature;
		case 3:
			if (report != NULL && transferred > 17) {
				memcpy(usb->deviceAddress, report + 4, 6);
				controller->deviceAddress = ((uint64_t)usb->deviceAddress[0] << 40) |
					((uint64_t)usb->deviceAddress[1] << 32) |
					((uint64_t)usb->deviceAddress[2] << 24) |
					((uint64_t)usb->deviceAddress[3] << 16) |
					((uint64_t)usb->deviceAddress[4] << 8) | usb->deviceAddress[5];
			}
			if (ds3IsSafeMode() == 0 && SceSysrootForDriver_B0E1FC67() == 0 &&
				memcmp(ds3Usb.localAddress, ds3EmptyBluetoothAddress, 6) != 0) {
				controller->setupStage = 4;
				ds3UsbGetFeatureReport(controller, 0xf5, 8);
				return;
			}
			controller->setupStage = 11;
			value = 0x03f8;
			length = 4;
			goto getCurrentFeature;
		case 4:
		case 6:
			if (report != NULL && transferred > 7)
				memcpy(usb->hostAddress, report + 2, 6);
			if (memcmp(ds3Usb.localAddress, ds3EmptyBluetoothAddress, 6) == 0 ||
				memcmp(usb->hostAddress, ds3Usb.localAddress, 6) != 0) {
				controller->setupStage = 5;
				ds3UsbSetHostAddress(controller);
				return;
			}
			controller->setupStage = 7;
			deviceId = controller->usbDeviceId;
			value = 0xa0;
			goto sendCalibrationCommand;
		case 5:
			controller->setupStage = 6;
			ds3UsbSetHostAddress(controller);
			return;
		case 7:
			controller->setupStage = 8;
			value = 0x03ef;
			length = 48;
			goto getFeature;
		case 8:
			calibration = controller->outputRequests[controller->outputIndex]->buffer;
			controller->ds3Calibration[0] = (calibration[17] << 8) | calibration[18];
			controller->ds3Calibration[1] = (calibration[19] << 8) | calibration[20];
			controller->ds3Calibration[3] = (calibration[21] << 8) | calibration[22];
			controller->ds3Calibration[4] = (calibration[23] << 8) | calibration[24];
			controller->ds3Calibration[6] = (calibration[25] << 8) | calibration[26];
			controller->ds3Calibration[7] = (calibration[27] << 8) | calibration[28];
			controller->ds3Calibration[8] = (calibration[29] << 8) | calibration[30];
			controller->setupStage = 9;
			controller->auxiliaryMotorLevel = calibration[32];
			value = 0xb0;
			goto sendCalibrationCommand;
		case 9:
			controller->setupStage = 10;
			value = 0x03ef;
			length = 48;
			goto getFeature;
		case 10:
			calibration = controller->outputRequests[controller->outputIndex]->buffer;
			controller->ds3Calibration[2] = (calibration[17] << 8) | calibration[18];
			controller->ds3Calibration[5] = (calibration[19] << 8) | calibration[20];
			controller->setupStage = 11;
			value = 0x03f8;
			length = 4;
			goto getFeature;
		case 11:
			if (report != NULL && transferred > 3) {
				controller->deviceSubtype = report[0];
				controller->deviceType = report[1];
			}
			controller->setupStage = 12;
			ds3BuildLedState(30, 0, &leds);
			deviceId = controller->usbDeviceId;
			goto sendInitialLeds;
		case 12:
			controller->setupStage = 13;
			value = 0x03f7;
			length = 12;
			goto getFeature;
		case 13:
			if (report != NULL && transferred > 11) {
				controller->connectionStatus = report[0];
				controller->batteryInfo = report[6];
			}
			controller->setupStage = 30;
			ds3UsbAdvanceSetup(controller, transferred);
			return;
		case 51:
			controller->setupStage = 52;
			report = controller->outputRequests[0]->buffer;
			value = 0x03f4;
			length = 4;
			memset(report, 0, 64);
			report[0] = 0x42;
			report[1] = 11;
			goto setFeature;
		case 52:
			ds3RemoveTrackedAddress(controller->deviceAddress);
			ds3UnpairUsbDevice(controller->usbDeviceId);
			controller->setupStage = 36;
			controller->inputReady = 0;
			return;
		default:
			return;
		}
	}
	switch (stage) {
	case 0:
		controller->setupStage = 1;
		if (usb == NULL)
			return;
		value = 0x03a3;
		length = 49;
		goto getFeature;
	case 1:
		report = controller->outputRequests[0]->buffer;
		if (usb != NULL && report != NULL && transferred > 48) {
			memcpy(firmwareInfo, report + 1, sizeof(firmwareInfo));
			if ((firmwareInfo[34] | (firmwareInfo[35] << 8)) >= 0x3100) {
				controller->setupStage = 2;
				value = 0x0302;
				length = 37;
				goto getFeature;
			}
		}
		controller->setupStage = 50;
		return;
	case 2:
		report = controller->outputRequests[0]->buffer;
		if (usb != NULL && report != NULL && transferred > 36) {
			memset(controller->ds4Calibration, 0, sizeof(controller->ds4Calibration));
			calibration = controller->outputRequests[controller->outputIndex]->buffer;
			for (axis = 0; axis < 12; axis++)
				controller->ds4Calibration[axis] = calibration[1 + axis * 2] |
					(calibration[2 + axis * 2] << 8);
			deviceId = controller->usbDeviceId;
			controller->ds4Calibration[12] = calibration[27] | (calibration[28] << 8);
			controller->ds4Calibration[13] = calibration[31] | (calibration[32] << 8);
			controller->ds4Calibration[14] = calibration[25] | (calibration[26] << 8);
			controller->ds4Calibration[15] = calibration[29] | (calibration[30] << 8);
			controller->ds4Calibration[16] = calibration[33] | (calibration[34] << 8);
			controller->setupStage = 3;
			controller->ds4Calibration[17] = calibration[35] | (calibration[36] << 8);
		} else {
			controller->setupStage = 3;
		}
		value = 0x0312;
		length = 16;
		goto getFeature;
	case 3:
		report = controller->outputRequests[0]->buffer;
		if (usb != NULL && report != NULL && transferred > 15) {
			address = 0;
			for (axis = 0; axis < 6; axis++) {
				usb->deviceAddress[axis] = report[6 - axis];
				address |= (uint64_t)report[1 + axis] << (axis * 8);
			}
			controller->deviceAddress = address;
			for (axis = 0; axis < 6; axis++)
				usb->hostAddress[axis] = report[15 - axis];
		}
		if (ds3IsSafeMode() != 0 || SceSysrootForDriver_B0E1FC67() != 0 ||
			memcmp(ds3Usb.localAddress, ds3EmptyBluetoothAddress, 6) == 0)
			goto startInput;
		deviceId = controller->usbDeviceId;
		if (deviceId == DS3_USB_DEVICE_UNUSED)
			return;
		if (deviceId == ds3Usb.devices[0].deviceId)
			index = 0;
		else if (deviceId == ds3Usb.devices[1].deviceId)
			index = 1;
		else if (deviceId == ds3Usb.devices[2].deviceId)
			index = 2;
		else if (deviceId == ds3Usb.devices[3].deviceId)
			index = 3;
		else
			return;
		usb = &ds3Usb.devices[index];
		report = controller->outputRequests[0]->buffer;
		result = SceBtForDriver_10D68FAE(report + 1, report + 7, report + 10,
			usb->idVendor, usb->idProduct, usb->bcdDevice,
			"Wireless Controller", hostAddress, usb->linkKey);
		if (result < 0)
			return;
		if (result == 0)
			goto startInput;
		controller->setupStage = 4;
		deviceId = controller->usbDeviceId;
		goto sendPairing;
	case 4:
		controller->setupStage = 5;
		if (usb == NULL)
			return;
		report = controller->outputRequests[0]->buffer;
		value = 0x0314;
		length = 17;
		memset(report, 0, 64);
		report[0] = 20;
		report[1] = 2;
		goto setFeature;
	case 5:
		goto startInput;
	case 20:
		ds3RemoveTrackedAddress(controller->deviceAddress);
		ds3UnpairUsbDevice(controller->usbDeviceId);
		controller->inputReady = 0;
		controller->setupStage = 21;
		return;
	default:
		return;
	}
getCurrentFeature:
	deviceId = controller->usbDeviceId;
getFeature:
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wIndex = 0;
	request.bmRequestType = 0xa1;
	request.bRequest = 1;
	request.wValue = value;
	request.wLength = length;
	ksceUsbdControlTransfer(usb->controlPipe, &request, report,
		ds3UsbSetupComplete, controller);
	return;
sendCalibrationCommand:
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	report = controller->outputRequests[0]->buffer;
	length = 48;
	request.wIndex = 0;
	request.bmRequestType = 0x21;
	request.bRequest = 9;
	request.wValue = 0x03ef;
	request.wLength = length;
	memset(report, 0, 64);
	report[4] = 3;
	report[5] = 1;
	report[6] = value;
	goto transfer;
sendInitialLeds:
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wIndex = 0;
	request.bmRequestType = 0x21;
	request.bRequest = 9;
	request.wValue = 0x0201;
	request.wLength = 48;
	if (controller->auxiliaryMotorReport == 6) {
		report[3] = 0xff;
		report[4] = controller->auxiliaryMotorLevel;
	} else if (controller->auxiliaryMotorReport == 7) {
		report[5] = 0xff;
		report[6] = controller->auxiliaryMotorLevel;
	}
	report[9] = leds.mask;
	for (axis = 0; axis < 4; axis++) {
		int position = 25 - axis * 5;

		report[position] = leds.patterns[axis].period;
		report[position + 1] = leds.patterns[axis].duration >> 8;
		report[position + 2] = leds.patterns[axis].duration;
		report[position + 3] = leds.patterns[axis].offTime;
		report[position + 4] = leds.patterns[axis].onTime;
	}
	ds3UpdateUsbOutputState(controller, report);
	goto transfer;
sendPairing:
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	report[0] = 19;
	for (axis = 0; axis < 6; axis++)
		report[1 + axis] = ds3Usb.localAddress[5 - axis];
	memcpy(report + 7, usb->linkKey, sizeof(usb->linkKey));
	value = 0x0313;
	length = 23;
setFeature:
	request.wIndex = 0;
	request.bmRequestType = 0x21;
	request.bRequest = 9;
	request.wValue = value;
	request.wLength = length;
transfer:
	ksceUsbdControlTransfer(usb->controlPipe, &request, report,
		ds3UsbSetupComplete, controller);
	return;
startInput:
	controller->setupStage = 6;
	if (ds3IsSuspending() != 0)
		return;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	usb = &ds3Usb.devices[index];
	if (usb->inputPipe < 0)
		return;
	interruptState = SceCpuForDriver_D32ACE9E(&ds3Usb.inputLock);
	input = controller->inputRequests[controller->inputIndex]->buffer;
	controller->inputIndex ^= 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3Usb.inputLock, interruptState);
	ksceUsbdInterruptTransfer(usb->inputPipe, input, usb->inputPacketSize,
		ds3UsbInputComplete, controller);
}

int ds3UsbSetActuators(Ds3Device *controller, const Ds3ActuatorState *state)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	uint8_t smallMotor;
	int deviceId, index;

	if (controller->auxiliaryMotorReport == 6)
		return 0;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.bmRequestType = 0x21;
	request.bRequest = 9; /* HID SET_REPORT. */
	request.wValue = 0x0201;
	request.wLength = 48;
	request.wIndex = 0;
	ds3BuildUsbOutputReport(controller, report);
	report[1] = 0x96;
	smallMotor = state->smallMotor;
	report[3] = 0x96;
	report[2] = smallMotor;
	report[4] = state->largeMotor;
	ds3UpdateUsbOutputState(controller, report);
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

int ds3UsbSetAuxiliaryMotor(Ds3Device *controller)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.wLength = 48;
	request.wIndex = 0;
	request.bmRequestType = 0x21;
	request.bRequest = 9; /* HID SET_REPORT. */
	request.wValue = 0x0201;
	ds3BuildUsbOutputReport(controller, report);
	if (controller->auxiliaryMotorReport == 6) {
		report[3] = 0xff;
		report[4] = controller->actuatorState.auxiliaryMotor;
	} else if (controller->auxiliaryMotorReport == 7) {
		report[5] = 0xff;
		report[6] = controller->actuatorState.auxiliaryMotor;
	}
	ds3UpdateUsbOutputState(controller, report);
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

int ds3UsbSetLightbar(Ds3Device *controller, const Ds3LedState *state)
{
	Ds3UsbDevice *device;
	uint8_t *report;
	uint8_t red;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	device = &ds3Usb.devices[index];
	if (device->outputPipe < 0)
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	report[0] = 5;
	report[1] = 3;
	report[2] = 0;
	report[3] = 0;
	report[4] = controller->actuatorState.smallMotor;
	report[5] = controller->actuatorState.largeMotor;
	red = state->lightbarColor[0];
	report[6] = red;
	report[7] = state->lightbarColor[1];
	report[8] = state->lightbarColor[2];
	controller->ledState.lightbarColor[0] = red;
	controller->ledState.lightbarColor[1] = report[7];
	controller->ledState.lightbarColor[2] = report[8];
	return ksceUsbdInterruptTransfer(device->outputPipe, report, 32,
		ds3UsbSetupComplete, controller);
}

int ds3UsbSetDs4Actuators(Ds3Device *controller, const Ds3ActuatorState *state)
{
	Ds3UsbDevice *device;
	uint8_t *report;
	uint8_t smallMotor;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	device = &ds3Usb.devices[index];
	if (device->outputPipe < 0)
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	report[0] = 5;
	report[1] = 3;
	report[2] = 0;
	report[3] = 0;
	smallMotor = state->smallMotor;
	report[4] = smallMotor;
	report[5] = state->largeMotor;
	report[6] = controller->ledState.lightbarColor[0];
	report[7] = controller->ledState.lightbarColor[1];
	report[8] = controller->ledState.lightbarColor[2];
	controller->actuatorState.smallMotor = smallMotor;
	controller->actuatorState.smallMotor = report[5];
	return ksceUsbdInterruptTransfer(device->outputPipe, report, 32,
		ds3UsbSetupComplete, controller);
}

int ds3UsbSetDs3Control(Ds3Device *controller, uint8_t control)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.bRequest = 9; /* HID SET_REPORT. */
	request.wValue = 0x03f4;
	request.wLength = 4;
	request.wIndex = 0;
	report[1] = control;
	request.bmRequestType = 0x21;
	report[0] = 0x42;
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

int ds3UsbSetDs4Control(Ds3Device *controller, uint8_t control)
{
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId = controller->usbDeviceId;
	int index;

	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return -1;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return -1;
	report = controller->outputRequests[0]->buffer;
	memset(report, 0, 64);
	request.bRequest = 9; /* HID SET_REPORT. */
	request.wValue = 0x0314;
	request.wLength = 17;
	request.wIndex = 0;
	report[1] = control;
	request.bmRequestType = 0x21;
	report[0] = 0x14;
	return ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
		&request, report, ds3UsbSetupComplete, controller);
}

void ds3UsbInputComplete(int result, int transferred, void *argument)
{
	Ds3Device *controller = argument;
	Ds3UsbDevice *device;
	SceUsbdDeviceRequest request;
	uint8_t *report;
	void *input;
	int deviceId, index, intr;

	(void)transferred;
	if (ds3IsSuspending() != 0 || controller == NULL || result != 0)
		return;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	device = &ds3Usb.devices[index];
	if (controller->idProduct == 0x0268) {
		if ((unsigned int)controller->setupStage <= 29 ||
			controller->inputReady != 0)
			goto ds3Battery;
		if (controller->usbState == 4) {
			controller->usbState = 5;
			goto ds3Battery;
		}
		result = ds3AllocateDevice(2, deviceId, controller->deviceAddress);
		if (result >= 0) {
			controller->inputReady = 1;
			controller->usbState = 2;
			device->psSuppressionCount = 6;
			ds3AssignPort(controller->deviceAddress, &controller->port, 1);
			ds3SignalPortChange();
			SceKernelSuspendForDriver_E0489831(0);
			goto ds3Battery;
		}
		if (result == -100) {
			SceCtrlForDriver_6E183B9B((int)0x80340101);
			controller->usbState = 4;
		}
		deviceId = controller->usbDeviceId;
		controller->setupStage = 33;
		if (deviceId == DS3_USB_DEVICE_UNUSED)
			return;
		if (deviceId == ds3Usb.devices[0].deviceId)
			index = 0;
		else if (deviceId == ds3Usb.devices[1].deviceId)
			index = 1;
		else if (deviceId == ds3Usb.devices[2].deviceId)
			index = 2;
		else if (deviceId == ds3Usb.devices[3].deviceId)
			index = 3;
		else
			return;
		report = controller->outputRequests[0]->buffer;
		memset(report, 0, 64);
		request.bmRequestType = 0x21;
		request.bRequest = 9;
		request.wValue = 0x03f4;
		request.wIndex = 0;
		request.wLength = 4;
		report[0] = 0x42;
		report[1] = 11;
		ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
			&request, report, ds3UsbSetupComplete, controller);
		return;

ds3Battery:
		report = controller->inputRequests[controller->inputIndex]->buffer;
		controller->batteryInfo = report[30];
	} else if (controller->idProduct == 0x05c4) {
		if (controller->inputReady != 0 ||
			ds3FindBtDevice(controller->deviceAddress) != NULL)
			goto ds4Battery;
		report = controller->inputRequests[controller->inputIndex]->buffer;
		if (report[0] == 1 && (report[7] & 1) != 0) {
			if (ds3IsDs4UsbEnabled() != 0) {
				result = ds3AllocateDevice(2, controller->usbDeviceId,
					controller->deviceAddress);
				if (result < 0) {
					if (result == -100) {
						SceCtrlForDriver_6E183B9B((int)0x80340101);
						controller->usbState = 4;
					}
					return;
				}
				controller->usbState = 2;
				controller->inputReady = 1;
				device->psSuppressionCount = 6;
				ds3AssignPort(controller->deviceAddress, &controller->port, 2);
				ds3SignalPortChange();
				SceKernelSuspendForDriver_E0489831(0);
			} else {
				ds3UsbSetDs4Control(controller, 1);
			}
		}

ds4Battery:
		report = controller->inputRequests[controller->inputIndex]->buffer;
		ds3UpdateBatteryInfo(controller, report[30]);
	} else {
		return;
	}
	if (ds3IsSuspending() != 0)
		return;
	deviceId = controller->usbDeviceId;
	if (deviceId == DS3_USB_DEVICE_UNUSED)
		return;
	if (deviceId == ds3Usb.devices[0].deviceId)
		index = 0;
	else if (deviceId == ds3Usb.devices[1].deviceId)
		index = 1;
	else if (deviceId == ds3Usb.devices[2].deviceId)
		index = 2;
	else if (deviceId == ds3Usb.devices[3].deviceId)
		index = 3;
	else
		return;
	device = &ds3Usb.devices[index];
	if (device->inputPipe < 0)
		return;
	intr = SceCpuForDriver_D32ACE9E(&ds3Usb.inputLock);
	index = controller->inputIndex;
	input = controller->inputRequests[index]->buffer;
	controller->inputIndex = index ^ 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&ds3Usb.inputLock, intr);
	ksceUsbdInterruptTransfer(device->inputPipe, input, device->inputPacketSize,
		ds3UsbInputComplete, controller);
}

int ds3UsbPollBattery(void)
{
	Ds3UsbDevice *device;
	Ds3Device *controller;
	SceUsbdDeviceRequest request;
	uint8_t *report;
	int deviceId, index;

	for (device = ds3Usb.devices; device != ds3Usb.devices + DS3_USB_DEVICES;
		device++) {
		if (device->deviceId == DS3_USB_DEVICE_UNUSED ||
			device->idProduct != 0x0268)
			continue;
		controller = ds3FindUsbDevice(device->deviceId);
		if (controller == NULL || controller->usbState <= 2 ||
			controller->setupStage != 36)
			continue;
		deviceId = controller->usbDeviceId;
		controller->setupStage = 35;
		if (deviceId == DS3_USB_DEVICE_UNUSED)
			continue;
		if (deviceId == ds3Usb.devices[0].deviceId)
			index = 0;
		else if (deviceId == ds3Usb.devices[1].deviceId)
			index = 1;
		else if (deviceId == ds3Usb.devices[2].deviceId)
			index = 2;
		else if (deviceId == ds3Usb.devices[3].deviceId)
			index = 3;
		else
			continue;
		report = controller->outputRequests[0]->buffer;
		memset(report, 0, 64);
		request.wValue = 0x03f7;
		request.wIndex = 0;
		request.wLength = 12;
		request.bmRequestType = 0xa1;
		request.bRequest = 1; /* HID GET_REPORT. */
		ksceUsbdControlTransfer(ds3Usb.devices[index].controlPipe,
			&request, report, ds3UsbSetupComplete, controller);
	}
	return 0;
}

int ds3UsbRegister(void)
{
	int result;

	memset(&ds3Usb, 0, sizeof(ds3Usb));
	ds3GetLocalAddress(ds3Usb.localAddress);
	ds3Usb.devices[0].deviceId = DS3_USB_DEVICE_UNUSED;
	ds3Usb.devices[1].deviceId = DS3_USB_DEVICE_UNUSED;
	ds3Usb.devices[2].deviceId = DS3_USB_DEVICE_UNUSED;
	ds3Usb.devices[3].deviceId = DS3_USB_DEVICE_UNUSED;
	ds3Usb.inputLock = 0;
	result = ksceUsbServAccessoryActivate();
	if (result < 0)
		return result;
	return ksceUsbdRegisterDriver(&ds3UsbDriver);
}
