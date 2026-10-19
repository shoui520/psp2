#include <sysmem/sysmem.h>
#include <udcd/udcd.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/cpu.h>

#include "usb_serial.h"
#include "usb_device.h"
#include "bbmc_netif.h"

BbmcUsbSerialDevice *g_usbSerialDevice;
BbmcUsbSerialDevice g_usbSerialDeviceStorage;

SceUdcdEndpoint g_usbSerialEndpoints[6] = {
	{ 0, 0, 0, 0 },
	{ 0x80, 1, 0, 0 },
	{ 0, 2, 0, 0 },
	{ 0x80, 3, 0, 0 },
	{ 0, 4, 0, 0 },
	{ 0x80, 5, 0, 0 },
};

void bbmcUsbDeviceControlSent(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceControlOutCompleted(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceQcomInCompleted(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceQcomOutCompleted(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceAtInterruptCompleted(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceAtInCompleted(SceUdcdDeviceRequest *request);
void bbmcUsbDeviceAtOutCompleted(SceUdcdDeviceRequest *request);
int bbmcUsbDeviceControlForwarded(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceControlReadCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceQcomReadCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceQcomWriteCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceAtReadCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceAtWriteCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceAtInterruptReadCompleted(int channel, BbmcUsbSerialRequest *request, int result);
int bbmcUsbDeviceProcessRequest(int recipient, int endpoint,
			       SceUdcdEP0DeviceRequest *request, void *context);
int bbmcUsbDeviceAttached(int usbVersion, void *context);
void bbmcUsbDeviceDetached(void *context);

static SceUdcdEndpointDescriptor g_usbSerialFullEndpoints[6];
static SceUdcdInterfaceDescriptor g_usbSerialHighInterfaces[3];
static SceUdcdConfiguration g_usbSerialFullConfiguration;
static SceUdcdConfiguration g_usbSerialHighConfiguration;
static SceUdcdInterface g_usbSerialInterface;
static SceUdcdInterfaceSettings g_usbSerialFullSettings[2];

static SceUdcdEndpointDescriptor g_usbSerialHighEndpoints[6] = {
	{ 7, 5, 0x81, 2, 512, 32, NULL, 0 },
	{ 7, 5, 2, 2, 512, 32, NULL, 0 },
	{ 7, 5, 0x83, 3, 1024, 5, NULL, 0 },
	{ 7, 5, 4, 2, 512, 32, NULL, 0 },
	{ 7, 5, 0x85, 2, 512, 32, NULL, 0 },
	{ 0 },
};

static SceUdcdInterfaceDescriptor g_usbSerialFullInterfaces[3] = {
	{ 9, 4, 0, 0, 2, 0xff, 0xff, 0xff, 0,
	  &g_usbSerialFullEndpoints[0], NULL, 0 },
	{ 9, 4, 1, 0, 3, 0xff, 0xff, 0xff, 1,
	  &g_usbSerialFullEndpoints[2], NULL, 0 },
	{ 0 },
};

static SceUdcdDeviceDescriptor g_usbSerialFullDevice = {
	18, 1, 0x200, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 1,
};
static SceUdcdDeviceDescriptor g_usbSerialHighDevice = {
	18, 1, 0x200, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 1,
};
static SceUdcdInterfaceSettings g_usbSerialHighSettings[2] = {
	{ &g_usbSerialHighInterfaces[0], 0, 1 },
	{ &g_usbSerialHighInterfaces[1], 0, 1 },
};

static SceUdcdDriver g_usbSerialDriver = {
	.driverName = "SceUsbQcomSer",
	.numEndpoints = 6,
	.endpoints = g_usbSerialEndpoints,
	.interface = &g_usbSerialInterface,
	.descriptor_hi = &g_usbSerialHighDevice,
	.configuration_hi = &g_usbSerialHighConfiguration,
	.descriptor = &g_usbSerialFullDevice,
	.configuration = &g_usbSerialFullConfiguration,
	.processRequest = bbmcUsbDeviceProcessRequest,
	.attach = bbmcUsbDeviceAttached,
	.detach = bbmcUsbDeviceDetached,
	.start = bbmcStartUsbSerialDevice,
	.stop = bbmcStopUsbSerialDevice,
};

static SceUdcdConfigDescriptor g_usbSerialFullConfigDescriptor = {
	9, 2, 62, 2, 1, 0, 0xc0, 250, g_usbSerialFullSettings, NULL, 0,
};
static SceUdcdConfigDescriptor g_usbSerialHighConfigDescriptor = {
	9, 2, 62, 2, 1, 0, 0xc0, 250, g_usbSerialHighSettings, NULL, 0,
};
static SceUdcdEndpointDescriptor g_usbSerialFullEndpoints[6] = {
	{ 7, 5, 0x81, 2, 64, 0, NULL, 0 },
	{ 7, 5, 2, 2, 64, 0, NULL, 0 },
	{ 7, 5, 0x83, 3, 64, 5, NULL, 0 },
	{ 7, 5, 4, 2, 64, 0, NULL, 0 },
	{ 7, 5, 0x85, 2, 64, 0, NULL, 0 },
	{ 0 },
};
static SceUdcdInterfaceDescriptor g_usbSerialHighInterfaces[3] = {
	{ 9, 4, 0, 0, 2, 0xff, 0xff, 0xff, 0,
	  &g_usbSerialHighEndpoints[0], NULL, 0 },
	{ 9, 4, 1, 0, 3, 0xff, 0xff, 0xff, 0,
	  &g_usbSerialHighEndpoints[2], NULL, 0 },
	{ 0 },
};
static SceUdcdInterface g_usbSerialInterface = { -1, 0, 2 };
static SceUdcdConfiguration g_usbSerialFullConfiguration = {
	&g_usbSerialFullConfigDescriptor, g_usbSerialFullSettings,
	g_usbSerialFullInterfaces, g_usbSerialFullEndpoints,
};
static SceUdcdConfiguration g_usbSerialHighConfiguration = {
	&g_usbSerialHighConfigDescriptor, g_usbSerialHighSettings,
	g_usbSerialHighInterfaces, g_usbSerialHighEndpoints,
};
static SceUdcdInterfaceSettings g_usbSerialFullSettings[2] = {
	{ &g_usbSerialFullInterfaces[0], 0, 1 },
	{ &g_usbSerialFullInterfaces[1], 0, 1 },
};

int bbmcUsbSerialDeviceThread(SceSize argumentSize, void *arguments)
{
	BbmcUsbSerialRequest controlOut, controlIn, qcomWrite, qcomRead;
	BbmcUsbSerialRequest atWrite, atRead, atInterrupt;
	BbmcUsbSerialDevice *device;
	SceUInt32 events;
	int qcomChannel = 0;
	int atChannel = 0;
	int bus;

	if (!g_usbSerialDevice)
		return 0;
	device = g_usbSerialDevice;
	for (;;) {
		if (sceKernelWaitEventFlag(device->event, 0x800007ffu, 5,
					    &events, NULL) < 0 || (int)events < 0)
			goto finished;
		if (events & 512) {
			bbmcCloseUsbSerialQcom(qcomChannel);
			bbmcCloseUsbSerialAt(atChannel);
			device = g_usbSerialDevice;
			device->qcomReadPending = 0;
			device->atInterruptPending = 0;
			device->atReadPending = 0;
		} else {
			device = g_usbSerialDevice;
		}
		if (events & 1) {
			bus = device->bus;
			device->qcomOut.request.size = 1024;
			device->qcomOut.request.returnCode = 0;
			SceCpuForDriver_02796361(device->qcomOut.request.data, 1088);
			SceUdcdForDriver_175E6179(&device->qcomOut.request, bus);
			device = g_usbSerialDevice;
			bus = device->bus;
			device->atOut.request.size = 1536;
			device->atOut.request.returnCode = 0;
			SceCpuForDriver_02796361(device->atOut.request.data, 1600);
			SceUdcdForDriver_175E6179(&device->atOut.request, bus);
			if (bbmcWaitUsbSerialHost(5, 0, NULL) >= 0) {
				if (bbmcOpenUsbSerialQcom(&qcomChannel) < 0)
					goto finished;
				if (bbmcOpenUsbSerialAt(&atChannel) < 0) {
					bbmcCloseUsbSerialQcom(qcomChannel);
					goto finished;
				}
			}
			device = g_usbSerialDevice;
			if (!device->qcomReadPending) {
				device->qcomReadPending = 1;
				qcomRead.arguments.bulk.data = device->qcomInBuffer;
				qcomRead.transferType = 1;
				qcomRead.arguments.bulk.length = 1024;
				qcomRead.completed = bbmcUsbDeviceQcomReadCompleted;
				qcomRead.context = device;
				bbmcReadUsbSerial(qcomChannel, &qcomRead);
				device = g_usbSerialDevice;
			}
			if (!device->atInterruptPending) {
				device->atInterruptPending = 1;
				atInterrupt.completed = bbmcUsbDeviceAtInterruptReadCompleted;
				atInterrupt.context = device;
				atInterrupt.arguments.bulk.data = device->atInterruptBuffer;
				atInterrupt.transferType = 1;
				atInterrupt.arguments.bulk.length = 1024;
				bbmcInterruptUsbSerial(atChannel, &atInterrupt);
				device = g_usbSerialDevice;
			}
			if (!device->atReadPending) {
				device->atReadPending = 1;
				atRead.completed = bbmcUsbDeviceAtReadCompleted;
				atRead.context = device;
				atRead.transferType = 1;
				atRead.arguments.bulk.length = 1536;
				atRead.arguments.bulk.data = device->atInBuffer;
				bbmcReadUsbSerial(atChannel, &atRead);
				device = g_usbSerialDevice;
			}
		}
		if ((events & 4) && device->connected) {
			__builtin_memcpy(&controlIn.arguments, &device->controlInSetup, 8);
			controlIn.completed = bbmcUsbDeviceControlReadCompleted;
			controlIn.context = device;
			controlIn.transferType = 0;
			controlIn.controlData = device->controlOutBuffer;
			bbmcControlUsbSerial(qcomChannel, &controlIn);
			device = g_usbSerialDevice;
		}
		if ((events & 8) && device->connected) {
			__builtin_memcpy(&controlOut.arguments, &device->controlOutSetup, 8);
			controlOut.controlData = device->controlInBuffer;
			controlOut.transferType = 0;
			controlOut.completed = bbmcUsbDeviceControlForwarded;
			controlOut.context = device;
			bbmcControlUsbSerial(qcomChannel, &controlOut);
			device = g_usbSerialDevice;
		}
		if ((events & 32) && device->connected) {
			device->qcomReadPending = 1;
			qcomRead.arguments.bulk.data = device->qcomInBuffer;
			qcomRead.completed = bbmcUsbDeviceQcomReadCompleted;
			qcomRead.context = device;
			qcomRead.transferType = 1;
			qcomRead.arguments.bulk.length = 1024;
			bbmcReadUsbSerial(qcomChannel, &qcomRead);
			device = g_usbSerialDevice;
		}
		if (events & 16) {
			if (!device->qcomOut.request.returnCode) {
				qcomWrite.arguments.bulk.length = device->qcomOut.request.transmitted;
				qcomWrite.arguments.bulk.data = device->qcomOutBuffer;
				qcomWrite.context = device;
				qcomWrite.transferType = 1;
				qcomWrite.completed = bbmcUsbDeviceQcomWriteCompleted;
				bbmcWriteUsbSerial(qcomChannel, &qcomWrite);
				device = g_usbSerialDevice;
			} else if (device->qcomOut.request.returnCode < 0 && device->connected) {
				bus = device->bus;
				device->qcomOut.request.returnCode = 0;
				device->qcomOut.request.size = 1024;
				SceCpuForDriver_02796361(device->qcomOut.request.data, 1088);
				SceUdcdForDriver_175E6179(&device->qcomOut.request, bus);
				device = g_usbSerialDevice;
			}
		}
		if ((events & 256) && device->connected) {
			device->atInterruptPending = 1;
			atInterrupt.completed = bbmcUsbDeviceAtInterruptReadCompleted;
			atInterrupt.context = device;
			atInterrupt.transferType = 1;
			atInterrupt.arguments.bulk.length = 1024;
			atInterrupt.arguments.bulk.data = device->atInterruptBuffer;
			bbmcInterruptUsbSerial(atChannel, &atInterrupt);
			device = g_usbSerialDevice;
		}
		if ((events & 128) && device->connected) {
			device->atReadPending = 1;
			atRead.completed = bbmcUsbDeviceAtReadCompleted;
			atRead.context = device;
			atRead.transferType = 1;
			atRead.arguments.bulk.length = 1536;
			atRead.arguments.bulk.data = device->atInBuffer;
			bbmcReadUsbSerial(atChannel, &atRead);
			device = g_usbSerialDevice;
		}
		if (events & 64) {
			if (!device->atOut.request.returnCode) {
				atWrite.arguments.bulk.length = device->atOut.request.transmitted;
				atWrite.arguments.bulk.data = device->atOutBuffer;
				atWrite.completed = bbmcUsbDeviceAtWriteCompleted;
				atWrite.context = device;
				atWrite.transferType = 1;
				bbmcWriteUsbSerial(atChannel, &atWrite);
				device = g_usbSerialDevice;
			} else if (device->atOut.request.returnCode < 0 && device->connected) {
				bus = device->bus;
				device->atOut.request.returnCode = 0;
				device->atOut.request.size = 1536;
				SceCpuForDriver_02796361(device->atOut.request.data, 1600);
				SceUdcdForDriver_175E6179(&device->atOut.request, bus);
				device = g_usbSerialDevice;
			}
		}
	}
finished:
	bbmcCloseUsbSerialAt(atChannel);
	bbmcCloseUsbSerialQcom(qcomChannel);
	return 0;
}

void bbmcUsbDeviceControlSent(SceUdcdDeviceRequest *request)
{
}

int bbmcUsbDeviceControlForwarded(int channelId, BbmcUsbSerialRequest *request,
				 int result)
{
	return 0;
}

void bbmcUsbDeviceAtOutCompleted(SceUdcdDeviceRequest *request)
{
	BbmcUsbSerialDevice *device = request->unused;

	sceKernelSetEventFlag(device->event, 64);
}

void bbmcUsbDeviceAtInCompleted(SceUdcdDeviceRequest *request)
{
	BbmcUsbSerialDevice *device = request->unused;

	sceKernelSetEventFlag(device->event, 128);
}

void bbmcUsbDeviceAtInterruptCompleted(SceUdcdDeviceRequest *request)
{
	BbmcUsbSerialDevice *device = request->unused;

	sceKernelSetEventFlag(device->event, 256);
}

void bbmcUsbDeviceQcomOutCompleted(SceUdcdDeviceRequest *request)
{
	BbmcUsbSerialDevice *device = request->unused;

	sceKernelSetEventFlag(device->event, 16);
}

void bbmcUsbDeviceQcomInCompleted(SceUdcdDeviceRequest *request)
{
	BbmcUsbSerialDevice *device = request->unused;

	sceKernelSetEventFlag(device->event, 32);
}

void bbmcUsbDeviceControlOutCompleted(SceUdcdDeviceRequest *request)
{
	sceKernelSetEventFlag(g_usbSerialDevice->event, 8);
}

void bbmcUsbDeviceDetached(void *context)
{
	g_usbSerialDeviceStorage.connected = 0;
	sceKernelSetEventFlag(g_usbSerialDevice->event, 2);
}

int bbmcUsbDeviceAttached(int usbVersion, void *context)
{
	g_usbSerialDevice->connected = 1;
	return sceKernelSetEventFlag(g_usbSerialDevice->event, 1);
}

int bbmcUsbDeviceProcessRequest(int recipient, int endpoint,
			       SceUdcdEP0DeviceRequest *setup, void *context)
{
	BbmcUsbSerialDevice *device;
	int bus;

	if (endpoint < 0)
		return -1;
	if ((setup->bmRequestType & 0x60) != 0x20)
		return 0;
	device = g_usbSerialDevice;
	if (setup->bmRequestType & 0x80) {
		__builtin_memcpy(&device->controlInSetup, setup, sizeof(*setup));
		sceKernelSetEventFlag(device->event, 4);
		return 0;
	}
	__builtin_memcpy(&device->controlOutSetup, setup, sizeof(*setup));
	if (setup->wLength != 0) {
		bus = device->bus;
		device->controlOut.request.size = 64;
		device->controlOut.request.returnCode = 0;
		SceCpuForDriver_02796361(device->controlOut.request.data, 128);
		return SceUdcdForDriver_175E6179(&device->controlOut.request, bus);
	}
	sceKernelSetEventFlag(device->event, 8);
	return 0;
}

int bbmcUsbDeviceQcomReadCompleted(int channelId, BbmcUsbSerialRequest *request,
				   int result)
{
	BbmcUsbSerialDevice *device = request->context;
	void *buffer;
	int bus;

	device->qcomReadPending = 0;
	if (result > 0) {
		bus = device->bus;
		device->qcomIn.request.size = result;
		device->qcomIn.request.returnCode = 0;
		SceCpuForDriver_103872A5(device->qcomIn.request.data,
			((SceUInt32)result + 64) & ~63u);
		SceUdcdForDriver_2E3E622A(&device->qcomIn.request, bus);
		return 0;
	}
	if (device->connected == 0)
		return 0;
	if (result == 0 || result == (int)0x80240007 ||
	    (SceUInt32)result - 0x80240008u <= 1) {
		buffer = device->qcomInBuffer;
		g_usbSerialDevice->qcomReadPending = 1;
		request->arguments.bulk.data = buffer;
		request->transferType = 1;
		request->completed = bbmcUsbDeviceQcomReadCompleted;
		request->arguments.bulk.length = 1024;
		bbmcReadUsbSerial(channelId, request);
	}
	return 0;
}

int bbmcUsbDeviceQcomWriteCompleted(int channelId, BbmcUsbSerialRequest *request,
				    int result)
{
	BbmcUsbSerialDevice *device = request->context;
	int bus;

	if (device->connected != 0) {
		bus = device->bus;
		device->qcomOut.request.size = 1024;
		device->qcomOut.request.returnCode = 0;
		SceCpuForDriver_02796361(device->qcomOut.request.data, 1088);
		SceUdcdForDriver_175E6179(&device->qcomOut.request, bus);
	}
	return 0;
}

int bbmcUsbDeviceAtWriteCompleted(int channelId, BbmcUsbSerialRequest *request,
				    int result)
{
	BbmcUsbSerialDevice *device = request->context;
	int bus;

	if (device->connected != 0) {
		bus = device->bus;
		device->atOut.request.size = 1536;
		device->atOut.request.returnCode = 0;
		SceCpuForDriver_02796361(device->atOut.request.data, 1600);
		SceUdcdForDriver_175E6179(&device->atOut.request, bus);
	}
	return 0;
}

int bbmcUsbDeviceControlReadCompleted(int channelId, BbmcUsbSerialRequest *request,
				     int result)
{
	BbmcUsbSerialDevice *device = request->context;
	int bus;

	if (result <= 0 || request->arguments.control.wLength == 0)
		return 0;
	bus = device->bus;
	device->controlIn.request.size = result;
	device->controlIn.request.returnCode = 0;
	SceCpuForDriver_103872A5(device->controlIn.request.data,
		((SceUInt32)result + 64) & ~63u);
	SceUdcdForDriver_2E3E622A(&device->controlIn.request, bus);
	return 0;
}

int bbmcUsbDeviceAtInterruptReadCompleted(int channelId, BbmcUsbSerialRequest *request,
				   int result)
{
	BbmcUsbSerialDevice *device = request->context;
	void *buffer;
	int bus;

	device->atInterruptPending = 0;
	if (result > 0) {
		bus = device->bus;
		device->atInterruptIn.request.size = result;
		device->atInterruptIn.request.returnCode = 0;
		SceCpuForDriver_103872A5(device->atInterruptIn.request.data,
			((SceUInt32)result + 64) & ~63u);
		SceUdcdForDriver_2E3E622A(&device->atInterruptIn.request, bus);
		return 0;
	}
	if (device->connected == 0)
		return 0;
	if (result == 0 || result == (int)0x80240007 ||
	    (SceUInt32)result - 0x80240008u <= 1) {
		buffer = g_usbSerialDevice->atInterruptBuffer;
		g_usbSerialDevice->atInterruptPending = 1;
		request->transferType = 1;
		request->arguments.bulk.data = buffer;
		request->completed = bbmcUsbDeviceAtInterruptReadCompleted;
		request->arguments.bulk.length = 1024;
		bbmcInterruptUsbSerial(channelId, request);
	}
	return 0;
}

int bbmcUsbDeviceAtReadCompleted(int channelId, BbmcUsbSerialRequest *request,
				   int result)
{
	BbmcUsbSerialDevice *device = request->context;
	void *buffer;
	int bus;

	device->atReadPending = 0;
	if (result > 0) {
		bus = device->bus;
		device->atIn.request.size = result;
		device->atIn.request.returnCode = 0;
		SceCpuForDriver_103872A5(device->atIn.request.data,
			((SceUInt32)result + 64) & ~63u);
		SceUdcdForDriver_2E3E622A(&device->atIn.request, bus);
		return 0;
	}
	if (device->connected == 0)
		return 0;
	if (result == 0 || result == (int)0x80240007 ||
	    (SceUInt32)result - 0x80240008u <= 1) {
		buffer = device->atInBuffer;
		g_usbSerialDevice->atReadPending = 1;
		request->arguments.bulk.data = buffer;
		request->transferType = 1;
		request->completed = bbmcUsbDeviceAtReadCompleted;
		request->arguments.bulk.length = 1536;
		bbmcReadUsbSerial(channelId, request);
	}
	return 0;
}

int bbmcInitializeUsbSerial(SceSize argumentSize, const void *arguments)
{
	int result;

	__builtin_memset(&g_usbSerialDeviceStorage, 0, sizeof(g_usbSerialDeviceStorage));
	g_usbSerialDeviceStorage.bus = *(const int *)arguments;
	result = sceKernelCreateEventFlag("SceBbmcUsbSerial", 0, 0, NULL);
	if (result < 0)
		goto failed;
	g_usbSerialDeviceStorage.event = result;
	result = sceKernelCreateThread("SceBbmcUsbSerial", bbmcUsbSerialDeviceThread,
				       97, 8192, 0x10000000, 0x80000, NULL);
	if (result < 0)
		goto failed;
	g_usbSerialDeviceStorage.thread = result;
	if (SceUdcdForDriver_64675918(&g_usbSerialDriver, g_usbSerialDeviceStorage.bus) < 0)
		goto failed;
	g_usbSerialDevice = &g_usbSerialDeviceStorage;
	return 0;
failed:
	if (g_usbSerialDeviceStorage.thread > 0) {
		sceKernelDeleteThread(g_usbSerialDeviceStorage.thread);
		g_usbSerialDeviceStorage.thread = 0;
	}
	if (g_usbSerialDeviceStorage.event > 0) {
		sceKernelDeleteEventFlag(g_usbSerialDeviceStorage.event);
		g_usbSerialDeviceStorage.event = 0;
	}
	return 2;
}

int bbmcFinalizeUsbSerial(void)
{
	g_usbSerialDevice = NULL;
	if (g_usbSerialDeviceStorage.thread > 0) {
		sceKernelDeleteThread(g_usbSerialDeviceStorage.thread);
		g_usbSerialDeviceStorage.thread = 0;
	}
	if (g_usbSerialDeviceStorage.event > 0) {
		sceKernelDeleteEventFlag(g_usbSerialDeviceStorage.event);
		g_usbSerialDeviceStorage.event = 0;
	}
	SceUdcdForDriver_6CCD152E(&g_usbSerialDriver, g_usbSerialDeviceStorage.bus);
	return 0;
}

void bbmcCloseUsbSerialChannels(void)
{
	if (g_usbSerialDevice)
		sceKernelSetEventFlag(g_usbSerialDevice->event, 512);
}

void bbmcWakeUsbSerial(void)
{
	if (g_usbSerialDevice)
		sceKernelSetEventFlag(g_usbSerialDevice->event, 1024);
}
