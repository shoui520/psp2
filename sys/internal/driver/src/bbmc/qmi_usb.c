/* QMI USB transport request completion. */
#include "qmi_usb.h"
#include <psp2kern/kernel/threadmgr.h>

extern int bbmcLockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("lockFastMutex");
extern int bbmcUnlockFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelUnlockFastMutex");
extern int bbmcInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
	const char *name, int attr, void *opt)
	__asm__("sceKernelInitializeFastMutex");
extern int bbmcFinalizeFastMutex(struct ThreadmgrFastMutex *mutex)
	__asm__("sceKernelFinalizeFastMutex");

QmiUsbTransport *g_qmiUsbTransport;
QmiUsbTransport g_qmiUsbTransportStorage;

int qmiProbeUsbTransport(int deviceId, SceUsbdInterfaceDescriptor **interface);
int qmiAttachUsbTransport(int deviceId);
int qmiDetachUsbTransport(int deviceId);

static const SceUsbdCompositeDriver g_qmiUsbDriver = {
	"SceBbmcUsb", qmiProbeUsbTransport, qmiAttachUsbTransport,
	qmiDetachUsbTransport,
};

/* name unknown: forward a transfer error or completed byte count. */
void qmiUsbTransferCompleted(int result, int count, void *arg)
{
	QmiUsbRequest *request = arg;

	if (result < 0)
		count = result;
	request->callback(request, count);
}

int qmiDetachUsbTransport(int deviceId)
{
	QmiUsbTransport *control = g_qmiUsbTransport;
	int i;

	if (!control || control->deviceId != deviceId)
		return -1;
	control->interruptPipe = 0;
	control->controlPipe = 0;
	bbmcLockFastMutex(&control->mutex);
	control = g_qmiUsbTransport;
	for (i = 0; i != 2; ++i) {
		QmiUsbStateSubscriber *subscriber = &control->subscribers[i];

		if (subscriber->callback) {
			if (subscriber->callback(2, subscriber->context)) {
				control = g_qmiUsbTransport;
				control->subscribers[i].callback = NULL;
				control->subscribers[i].context = NULL;
			} else {
				control = g_qmiUsbTransport;
			}
		}
	}
	bbmcUnlockFastMutex(&control->mutex);
	sceKernelClearEventFlag(g_qmiUsbTransport->event, ~2u);
	sceKernelSetEventFlag(g_qmiUsbTransport->event, 1);
	g_qmiUsbTransport->deviceId = 0;
	return 0;
}

int qmiAttachUsbTransport(int deviceId)
{
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdEndpointDescriptor *interruptEndpoint = NULL;
	QmiUsbTransport *control;
	void *descriptor = NULL;
	int id;
	int pipe;
	int i;

	if (!g_qmiUsbTransport)
		return -1;
	id = g_qmiUsbTransport->deviceId;
	if (id != deviceId)
		return -1;
	for (;;) {
		interface = ksceUsbdScanStaticDescriptor(id, descriptor, 4);
		descriptor = interface;
		if (!interface)
			return -1;
		if (interface->bInterfaceNumber == 2)
			break;
	}
	for (i = 0; i < interface->bNumEndpoints; ++i) {
		endpoint = ksceUsbdScanStaticDescriptor(id, descriptor, 5);
		descriptor = endpoint;
		if (!endpoint)
			return -1;
		if ((endpoint->bmAttributes & 3) == 3) {
			interruptEndpoint = endpoint;
		} else if ((endpoint->bmAttributes & 3) == 2) {
			if ((SceInt8)endpoint->bEndpointAddress >= 0)
				g_qmiUsbTransport->bulkOut = endpoint;
			else
				g_qmiUsbTransport->bulkIn = endpoint;
		}
	}
	pipe = ksceUsbdOpenPipe(id, NULL);
	if (pipe < 0)
		return -1;
	g_qmiUsbTransport->controlPipe = pipe;
	pipe = ksceUsbdOpenPipe(id, interruptEndpoint);
	control = g_qmiUsbTransport;
	if (pipe < 0) {
		ksceUsbdClosePipe(g_qmiUsbTransport->controlPipe);
		return -1;
	}
	control->interruptPipe = pipe;
	bbmcLockFastMutex(&control->mutex);
	control = g_qmiUsbTransport;
	for (i = 0; i != 2; ++i) {
		QmiUsbStateSubscriber *subscriber = &control->subscribers[i];

		if (subscriber->callback) {
			if (subscriber->callback(1, subscriber->context)) {
				control = g_qmiUsbTransport;
				control->subscribers[i].callback = NULL;
				control->subscribers[i].context = NULL;
			} else {
				control = g_qmiUsbTransport;
			}
		}
	}
	bbmcUnlockFastMutex(&control->mutex);
	sceKernelClearEventFlag(g_qmiUsbTransport->event, ~1u);
	sceKernelSetEventFlag(g_qmiUsbTransport->event, 2);
	return 0;
}

int qmiProbeUsbTransport(int deviceId, SceUsbdInterfaceDescriptor **interface)
{
	SceUsbdDeviceDescriptor *device;
	SceUsbdInterfaceDescriptor *descriptor;
	SceUInt32 location;
	void *start = NULL;

	if (g_qmiUsbTransport &&
	    ksceUsbdGetDeviceLocation(deviceId, &location) >= 0 && location == 1) {
		device = ksceUsbdScanStaticDescriptor(deviceId, NULL, 1);
		if (device && device->idVendor == 0x5c6 && device->idProduct == 0x9005) {
			for (;;) {
				descriptor = ksceUsbdScanStaticDescriptor(deviceId, start, 4);
				start = descriptor;
				if (!descriptor)
					break;
				if (descriptor->bInterfaceNumber == 2) {
					QmiUsbTransport *control = g_qmiUsbTransport;

					*interface = descriptor;
					control->deviceId = deviceId;
					return 0;
				}
			}
		}
	}
	return -1;
}

int qmiInitializeUsbTransport(void)
{
	int result;

	result = sceKernelCreateEventFlag("SceBbmcProtoUsbCls", 0x1000, 1, NULL);
	if (result < 0)
		return result;
	g_qmiUsbTransportStorage.event = result;
	bbmcInitializeFastMutex(&g_qmiUsbTransportStorage.mutex,
			       "SceBbmcProtoUsbCls", 2, NULL);
	g_qmiUsbTransportStorage.subscribers[0].callback = NULL;
	g_qmiUsbTransportStorage.subscribers[0].context = NULL;
	g_qmiUsbTransportStorage.subscribers[1].callback = NULL;
	g_qmiUsbTransportStorage.subscribers[1].context = NULL;
	g_qmiUsbTransport = &g_qmiUsbTransportStorage;
	result = ksceUsbdRegisterCompositeLdd(&g_qmiUsbDriver);
	if (result < 0)
		g_qmiUsbTransport = NULL;
	return result;
}

int qmiFinalizeUsbTransport(void)
{
	int result;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (g_qmiUsbTransport->interruptPipe) {
		ksceUsbdClosePipe(g_qmiUsbTransport->interruptPipe);
		g_qmiUsbTransport->interruptPipe = 0;
	}
	if (g_qmiUsbTransport->controlPipe) {
		ksceUsbdClosePipe(g_qmiUsbTransport->controlPipe);
		g_qmiUsbTransport->controlPipe = 0;
	}
	result = ksceUsbdUnregisterCompositeLdd(&g_qmiUsbDriver);
	bbmcFinalizeFastMutex(&g_qmiUsbTransport->mutex);
	sceKernelDeleteEventFlag(g_qmiUsbTransport->event);
	g_qmiUsbTransport = NULL;
	return result;
}

int qmiGetUsbState(SceUInt32 *state)
{
	int result;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (!state)
		return 0x80330002;
	result = sceKernelPollEventFlag(g_qmiUsbTransport->event, ~0u, 1, state);
	if (result == (int)0x800280e3) {
		*state = 0;
		return 0;
	}
	return result;
}

int qmiRegisterUsbStateCallback(int (*callback)(int state, void *context),
				void *context)
{
	QmiUsbTransport *control;
	int index = 0;
	int result = 0;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	bbmcLockFastMutex(&g_qmiUsbTransport->mutex);
	control = g_qmiUsbTransport;
	if (control->subscribers[0].callback) {
		result = 0x80332101;
		if (control->subscribers[1].callback)
			goto out;
		index = 1;
	}
	control->subscribers[index].callback = callback;
	control->subscribers[index].context = context;
	result = 0;
out:
	bbmcUnlockFastMutex(&control->mutex);
	return result;
}

int qmiUnregisterUsbStateCallback(int (*callback)(int state, void *context))
{
	QmiUsbTransport *control;
	int index;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	bbmcLockFastMutex(&g_qmiUsbTransport->mutex);
	control = g_qmiUsbTransport;
	if (control->subscribers[0].callback == callback)
		index = 0;
	else if (control->subscribers[1].callback == callback)
		index = 1;
	else
		goto out;
	control->subscribers[index].callback = NULL;
	control->subscribers[index].context = NULL;
out:
	bbmcUnlockFastMutex(&control->mutex);
	return 0;
}

int qmiSubmitUsbControlIn(QmiUsbRequest *request)
{
	SceUsbdDeviceRequest setup;
	SceSize length;
	unsigned char *data;
	int pipe;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	length = request->length;
	pipe = g_qmiUsbTransport->controlPipe;
	data = request->data;
	request->transferType = 1;
	setup.wLength = length;
	setup.bRequest = 1;
	setup.bmRequestType = 0xa1;
	setup.wValue = 0;
	setup.wIndex = 2;
	return ksceUsbdControlTransfer(pipe, &setup, data,
				       qmiUsbTransferCompleted, request);
}

int qmiSubmitUsbControlOut(QmiUsbRequest *request)
{
	SceUsbdDeviceRequest setup;
	SceSize length;
	unsigned char *data;
	int pipe;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	length = request->length;
	pipe = g_qmiUsbTransport->controlPipe;
	data = request->data;
	request->transferType = 2;
	setup.wLength = length;
	setup.wIndex = 2;
	setup.bmRequestType = 0x21;
	setup.bRequest = 0;
	setup.wValue = 0;
	return ksceUsbdControlTransfer(pipe, &setup, data,
				       qmiUsbTransferCompleted, request);
}

int qmiSubmitUsbInterrupt(QmiUsbRequest *request)
{
	unsigned char *data;
	SceSize length;
	int pipe;

	if (!g_qmiUsbTransport)
		return 0x80330003;
	pipe = g_qmiUsbTransport->interruptPipe;
	data = request->data;
	length = request->length;
	request->transferType = 0;
	return ksceUsbdInterruptTransfer(pipe, data, length,
					 qmiUsbTransferCompleted, request);
}

int qmiOpenUsbBulkIn(void)
{
	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (g_qmiUsbTransport->deviceId)
		return ksceUsbdOpenPipe(g_qmiUsbTransport->deviceId,
					g_qmiUsbTransport->bulkIn);
	return 0x80330006;
}

int qmiOpenUsbBulkOut(void)
{
	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (g_qmiUsbTransport->deviceId)
		return ksceUsbdOpenPipe(g_qmiUsbTransport->deviceId,
					g_qmiUsbTransport->bulkOut);
	return 0x80330006;
}

int qmiCloseUsbPipe(int pipe)
{
	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (g_qmiUsbTransport->deviceId)
		return ksceUsbdClosePipe(pipe);
	return 0x80330006;
}

int qmiGetUsbDeviceSpeed(SceUInt32 *speed)
{
	if (!g_qmiUsbTransport)
		return 0x80330003;
	if (g_qmiUsbTransport->deviceId)
		return ksceUsbdGetDeviceSpeed(g_qmiUsbTransport->deviceId, speed);
	return 0x80330006;
}
