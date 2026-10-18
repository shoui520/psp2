/* Composite USB host channels used by the modem serial bridge. */
#include <sysmem/cpu.h>
#include <psp2kern/kernel/threadmgr.h>
#include <string.h>
#include "usb_serial.h"

BbmcUsbSerialHost *g_usbSerialHost;
BbmcUsbSerialHost g_usbSerialHostStorage;
BbmcUsbSerialChannel g_usbSerialChannels[2];

int bbmcProbeUsbQdl(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
int bbmcProbeUsbQcom(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
int bbmcProbeUsbQedl(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
int bbmcProbeUsbQmodem(int deviceId, SceUsbdInterfaceDescriptor **descriptor);
int bbmcAttachUsbQdl(int deviceId);
int bbmcDetachUsbQdl(int deviceId);
int bbmcAttachUsbQcom(int deviceId);
int bbmcDetachUsbQcom(int deviceId);
int bbmcAttachUsbQedl(int deviceId);
int bbmcDetachUsbQedl(int deviceId);
int bbmcAttachUsbQmodem(int deviceId);
int bbmcDetachUsbQmodem(int deviceId);

static const SceUsbdCompositeDriver g_usbSerialQdlDriver = {
	.name = "SceBbmcQdlIf",
	.probe = bbmcProbeUsbQdl,
	.attach = bbmcAttachUsbQdl,
	.detach = bbmcDetachUsbQdl,
};

static const SceUsbdCompositeDriver g_usbSerialQedlDriver = {
	.name = "SceBbmcQedlIf",
	.probe = bbmcProbeUsbQedl,
	.attach = bbmcAttachUsbQedl,
	.detach = bbmcDetachUsbQedl,
};

static const SceUsbdCompositeDriver g_usbSerialQcomDriver = {
	.name = "SceBbmcQcomIf",
	.probe = bbmcProbeUsbQcom,
	.attach = bbmcAttachUsbQcom,
	.detach = bbmcDetachUsbQcom,
};

static const SceUsbdCompositeDriver g_usbSerialQmodemDriver = {
	.name = "SceBbmcQmodemIf",
	.probe = bbmcProbeUsbQmodem,
	.attach = bbmcAttachUsbQmodem,
	.detach = bbmcDetachUsbQmodem,
};

static const SceUInt8 g_usbSerialInterfaceNumbers[4] = { 0, 1, 0, 0 };
static const SceUInt32 g_usbSerialDetachedEvents[4] = { 2, 8, 32, 128 };
static const SceUInt32 g_usbSerialAttachedEvents[4] = { 1, 4, 16, 64 };
static const SceUInt32 g_usbSerialAttachedNotifications[4] = { 1, 4, 16, 64 };

/* name unknown: discover endpoints and publish an attached composite interface. */
int bbmcAttachUsbSerialInterface(int deviceId, unsigned int index)
{
	SceUsbdInterfaceDescriptor *interface;
	BbmcUsbSerialEndpoints *endpoints;
	void *cursor = NULL;
	int matchedDevice = g_usbSerialHost->deviceId[index];
	unsigned int count, subscriber;

	if (matchedDevice != deviceId)
		return -1;
	for (;;) {
		interface = SceUsbdForDriver_BC3EF82B(matchedDevice, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		cursor = interface;
		if (interface == NULL)
			return -1;
		if (interface->bInterfaceNumber == g_usbSerialInterfaceNumbers[index])
			break;
	}
	endpoints = &g_usbSerialHost->endpoints[index];
	if (endpoints != NULL && interface->bNumEndpoints != 0) {
		count = 0;
		do {
			SceUsbdEndpointDescriptor *endpoint;
			unsigned int type;

			endpoint = SceUsbdForDriver_BC3EF82B(matchedDevice, cursor,
				SCE_USBD_DESCRIPTOR_ENDPOINT);
			cursor = endpoint;
			if (endpoint == NULL)
				return -1;
			type = endpoint->bmAttributes & 3;
			++count;
			if (type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK) {
				if ((SceInt8)endpoint->bEndpointAddress >= 0)
					endpoints->bulkOut = endpoint;
				else
					endpoints->bulkIn = endpoint;
			} else if (type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT) {
				endpoints->interruptEndpoint = endpoint;
			}
		} while (interface->bNumEndpoints > count);
	}
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	sceKernelClearEventFlag(g_usbSerialHost->event, ~g_usbSerialDetachedEvents[index]);
	sceKernelSetEventFlag(g_usbSerialHost->event, g_usbSerialAttachedEvents[index]);
	g_usbSerialHost->attached[index] = 1;
	subscriber = 0;
	do {
		if (g_usbSerialHost->stateChanged[subscriber] != NULL)
			g_usbSerialHost->stateChanged[subscriber](
				g_usbSerialAttachedNotifications[index],
				g_usbSerialHost->stateContext[subscriber]);
		++subscriber;
	} while (subscriber != 2);
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return 0;
}

/* name unknown: detach the Qcom interface and fail its queued transfers. */
int bbmcDetachUsbQcom(int deviceId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialPipe *pipe;

	if (deviceId != g_usbSerialHost->deviceId[0])
		return -1;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	g_usbSerialHost->attached[0] = 0;
	sceKernelClearEventFlag(g_usbSerialHost->event, ~1u);
	sceKernelSetEventFlag(g_usbSerialHost->event, 2);
	if (g_usbSerialHost->stateChanged[0] != NULL)
		g_usbSerialHost->stateChanged[0](2, g_usbSerialHost->stateContext[0]);
	if (g_usbSerialHost->stateChanged[1] != NULL)
		g_usbSerialHost->stateChanged[1](2, g_usbSerialHost->stateContext[1]);
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind == 1)
		channel = &g_usbSerialChannels[0];
	else if (g_usbSerialChannels[1].kind == 1)
		channel = &g_usbSerialChannels[1];
	else
		return 0;
	pipe = &channel->pipes[0];
	do {
		if (pipe->pipeId > 0) {
			BbmcUsbSerialRequest *request;

			SceCpuForDriver_BF82DEB2(&pipe->lock);
			request = pipe->head;
			pipe->pipeId = 0;
			while (request != NULL) {
				int (*completed)(int, BbmcUsbSerialRequest *, int) = request->completed;
				BbmcUsbSerialRequest *next = request->next;

				pipe->head = next;
				if (completed != NULL)
					completed(channel->kind, request, (int)0x80332206);
				request = next;
			}
			sceKernelSpinlockLowUnlock(&pipe->lock);
		}
		++pipe;
	} while (pipe != &channel->pipes[3]);
	channel->kind = 0;
	return 0;
}

/* name unknown: unlink a completed bulk OUT request and notify its owner. */
void bbmcUsbSerialOutCompleted(int result, int count, void *argument)
{
	BbmcUsbSerialRequest *request = argument;
	BbmcUsbSerialChannel *channel = request->queueOwner.channel;
	BbmcUsbSerialRequest *head, *node, *completed;
	BbmcUsbSerialPipe *pipe = &channel->pipes[2];
	int kind;

	if (result < 0)
		count = result;
	request->queueOwner.channel = NULL;
	kind = channel->kind;
	SceCpuForDriver_BF82DEB2(&pipe->lock);
	head = pipe->head;
	node = head;
	if (node == NULL)
		goto missing;
	if (request != node) {
		for (;;) {
			completed = node->next;
			if (completed == NULL)
				goto missing;
			if (request == completed) {
				node->next = completed->next;
				break;
			}
			node = completed;
		}
	} else {
		completed = request;
		head = request->next;
		pipe->head = head;
	}
	if (head == NULL)
		pipe->tail = NULL;
	sceKernelSpinlockLowUnlock(&pipe->lock);
	if (completed->completed != NULL)
		completed->completed(kind, completed, count);
	return;
missing:
	sceKernelSpinlockLowUnlock(&pipe->lock);
}

int bbmcAttachUsbQedl(int deviceId)
{
	return bbmcAttachUsbSerialInterface(deviceId, 2);
}

int bbmcAttachUsbQdl(int deviceId)
{
	return bbmcAttachUsbSerialInterface(deviceId, 3);
}

int bbmcAttachUsbQmodem(int deviceId)
{
	return bbmcAttachUsbSerialInterface(deviceId, 1);
}

int bbmcAttachUsbQcom(int deviceId)
{
	return bbmcAttachUsbSerialInterface(deviceId, 0);
}

/* name unknown: close one composite channel and fail its pending requests. */
int bbmcCloseUsbSerialInterface(unsigned int index, int channelId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialPipe *pipe;
	int ret;

	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (g_usbSerialHost->attached[index] == 0)
		return (int)0x80332203;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind == channelId)
		channel = &g_usbSerialChannels[0];
	else if (g_usbSerialChannels[1].kind == channelId)
		channel = &g_usbSerialChannels[1];
	else {
		ret = (int)0x80332204;
		goto out;
	}
	if ((unsigned int)(channel->kind - 1) <= 3) {
		pipe = &channel->pipes[0];
		do {
			if (pipe->pipeId > 0) {
				BbmcUsbSerialRequest *request;

				SceCpuForDriver_BF82DEB2(&pipe->lock);
				SceUsbdForDriver_F304DC5C(pipe->pipeId);
				pipe->pipeId = 0;
				request = pipe->head;
				while (request != NULL) {
					int (*completed)(int, BbmcUsbSerialRequest *, int) = request->completed;
					BbmcUsbSerialRequest *next = request->next;

					pipe->head = next;
					if (completed != NULL)
						completed(channel->kind, request, (int)0x80332206);
					request = next;
				}
				sceKernelSpinlockLowUnlock(&pipe->lock);
			}
			++pipe;
		} while (pipe != &channel->pipes[3]);
		channel->kind = 0;
		if (g_usbSerialHost->controlPipe.pipeId > 0) {
			SceUsbdForDriver_F304DC5C(g_usbSerialHost->controlPipe.pipeId);
			g_usbSerialHost->controlPipe.pipeId = 0;
		}
	}
	channel->kind = 0;
	ret = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

/* name unknown: complete a transfer queued on the shared control pipe. */
int bbmcProbeUsbQdl(int deviceId, SceUsbdInterfaceDescriptor **descriptor)
{
	SceUsbdDeviceDescriptor *device;
	SceUsbdInterfaceDescriptor *interface;
	void *cursor = NULL;
	SceUInt32 location;

	if (SceUsbdForDriver_0E73D253(deviceId, &location) < 0 || location != 1)
		return -1;
	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL || device->idVendor != 0x05c6 || device->idProduct != 0x920c)
		return -1;
	for (;;) {
		interface = SceUsbdForDriver_BC3EF82B(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		cursor = interface;
		if (interface == NULL)
			return -1;
		if (interface->bInterfaceNumber == 0)
			break;
	}
	*descriptor = interface;
	g_usbSerialHost->deviceId[3] = deviceId;
	return 0;
}

void bbmcUsbSerialControlCompleted(int result, int count, void *argument)
{
	BbmcUsbSerialRequest *request = argument;
	BbmcUsbSerialPipe *pipe = request->queueOwner.controlPipe;
	BbmcUsbSerialRequest *head, *node, *completed;

	if (result < 0)
		count = result;
	SceCpuForDriver_BF82DEB2(&pipe->lock);
	head = pipe->head;
	node = head;
	if (node == NULL)
		goto missing;
	if (request != node) {
		for (;;) {
			completed = node->next;
			if (completed == NULL)
				goto missing;
			if (request == completed) {
				node->next = completed->next;
				break;
			}
			node = completed;
		}
	} else {
		completed = request;
		head = request->next;
		pipe->head = head;
	}
	if (head == NULL)
		pipe->tail = NULL;
	sceKernelSpinlockLowUnlock(&pipe->lock);
	if (completed->completed != NULL)
		completed->completed(0, completed, count);
	return;
missing:
	sceKernelSpinlockLowUnlock(&pipe->lock);
}

/* name unknown: complete a bulk IN transfer on the channel's receive pipe. */
void bbmcUsbSerialInCompleted(int result, int count, void *argument)
{
	BbmcUsbSerialRequest *request = argument;
	BbmcUsbSerialChannel *channel = request->queueOwner.channel;
	BbmcUsbSerialRequest *head, *node, *completed;
	BbmcUsbSerialPipe *pipe = &channel->pipes[1];
	int kind;

	if (result < 0)
		count = result;
	request->queueOwner.channel = NULL;
	kind = channel->kind;
	SceCpuForDriver_BF82DEB2(&pipe->lock);
	head = pipe->head;
	node = head;
	if (node == NULL)
		goto missing;
	if (request != node) {
		for (;;) {
			completed = node->next;
			if (completed == NULL)
				goto missing;
			if (request == completed) {
				node->next = completed->next;
				break;
			}
			node = completed;
		}
	} else {
		completed = request;
		head = request->next;
		pipe->head = head;
	}
	if (head == NULL)
		pipe->tail = NULL;
	sceKernelSpinlockLowUnlock(&pipe->lock);
	if (completed->completed != NULL)
		completed->completed(kind, completed, count);
	return;
missing:
	sceKernelSpinlockLowUnlock(&pipe->lock);
}

/* name unknown: complete an interrupt transfer on the channel's first pipe. */
void bbmcUsbSerialInterruptCompleted(int result, int count, void *argument)
{
	BbmcUsbSerialRequest *request = argument;
	BbmcUsbSerialChannel *channel = request->queueOwner.channel;
	BbmcUsbSerialRequest *head, *node, *completed;
	BbmcUsbSerialPipe *pipe = &channel->pipes[0];
	int kind;

	if (result < 0)
		count = result;
	request->queueOwner.channel = NULL;
	kind = channel->kind;
	SceCpuForDriver_BF82DEB2(&pipe->lock);
	head = pipe->head;
	node = head;
	if (node == NULL)
		goto missing;
	if (request != node) {
		for (;;) {
			completed = node->next;
			if (completed == NULL)
				goto missing;
			if (request == completed) {
				node->next = completed->next;
				break;
			}
			node = completed;
		}
	} else {
		completed = request;
		head = request->next;
		pipe->head = head;
	}
	if (head == NULL)
		pipe->tail = NULL;
	sceKernelSpinlockLowUnlock(&pipe->lock);
	if (completed->completed != NULL)
		completed->completed(kind, completed, count);
	return;
missing:
	sceKernelSpinlockLowUnlock(&pipe->lock);
}

/* name unknown: detach the Qedl interface and fail its queued transfers. */
int bbmcProbeUsbQcom(int deviceId, SceUsbdInterfaceDescriptor **descriptor)
{
	SceUsbdDeviceDescriptor *device;
	SceUsbdInterfaceDescriptor *interface;
	void *cursor = NULL;
	SceUInt32 location;

	if (SceUsbdForDriver_0E73D253(deviceId, &location) < 0 || location != 1)
		return -1;
	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL || device->idVendor != 0x05c6 || device->idProduct != 0x9005)
		return -1;
	for (;;) {
		interface = SceUsbdForDriver_BC3EF82B(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		cursor = interface;
		if (interface == NULL)
			return -1;
		if (interface->bInterfaceNumber == 0)
			break;
	}
	*descriptor = interface;
	g_usbSerialHost->deviceId[0] = deviceId;
	return 0;
}

int bbmcProbeUsbQedl(int deviceId, SceUsbdInterfaceDescriptor **descriptor)
{
	SceUsbdDeviceDescriptor *device;
	SceUsbdInterfaceDescriptor *interface;
	void *cursor = NULL;
	SceUInt32 location;

	if (SceUsbdForDriver_0E73D253(deviceId, &location) < 0 || location != 1)
		return -1;
	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL || device->idVendor != 0x05c6 || device->idProduct != 0x9008)
		return -1;
	for (;;) {
		interface = SceUsbdForDriver_BC3EF82B(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		cursor = interface;
		if (interface == NULL)
			return -1;
		if (interface->bInterfaceNumber == 0)
			break;
	}
	*descriptor = interface;
	g_usbSerialHost->deviceId[2] = deviceId;
	return 0;
}

int bbmcProbeUsbQmodem(int deviceId, SceUsbdInterfaceDescriptor **descriptor)
{
	SceUsbdDeviceDescriptor *device;
	SceUsbdInterfaceDescriptor *interface;
	void *cursor = NULL;
	SceUInt32 location;

	if (SceUsbdForDriver_0E73D253(deviceId, &location) < 0 || location != 1)
		return -1;
	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL || device->idVendor != 0x05c6 || device->idProduct != 0x9005)
		return -1;
	for (;;) {
		interface = SceUsbdForDriver_BC3EF82B(deviceId, cursor,
			SCE_USBD_DESCRIPTOR_INTERFACE);
		cursor = interface;
		if (interface == NULL)
			return -1;
		if (interface->bInterfaceNumber == 1)
			break;
	}
	*descriptor = interface;
	g_usbSerialHost->deviceId[1] = deviceId;
	return 0;
}

int bbmcDetachUsbQedl(int deviceId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialPipe *pipe;

	if (deviceId != g_usbSerialHost->deviceId[2])
		return -1;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	g_usbSerialHost->attached[2] = 0;
	sceKernelClearEventFlag(g_usbSerialHost->event, ~16u);
	sceKernelSetEventFlag(g_usbSerialHost->event, 32);
	if (g_usbSerialHost->stateChanged[0] != NULL)
		g_usbSerialHost->stateChanged[0](32, g_usbSerialHost->stateContext[0]);
	if (g_usbSerialHost->stateChanged[1] != NULL)
		g_usbSerialHost->stateChanged[1](32, g_usbSerialHost->stateContext[1]);
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind == 3)
		channel = &g_usbSerialChannels[0];
	else if (g_usbSerialChannels[1].kind == 3)
		channel = &g_usbSerialChannels[1];
	else
		return 0;
	pipe = &channel->pipes[0];
	do {
		if (pipe->pipeId > 0) {
			BbmcUsbSerialRequest *request;

			SceCpuForDriver_BF82DEB2(&pipe->lock);
			request = pipe->head;
			pipe->pipeId = 0;
			while (request != NULL) {
				int (*completed)(int, BbmcUsbSerialRequest *, int) = request->completed;
				BbmcUsbSerialRequest *next = request->next;

				pipe->head = next;
				if (completed != NULL)
					completed(channel->kind, request, (int)0x80332206);
				request = next;
			}
			sceKernelSpinlockLowUnlock(&pipe->lock);
		}
		++pipe;
	} while (pipe != &channel->pipes[3]);
	channel->kind = 0;
	return 0;
}

/* name unknown: detach the Qdl interface and fail its queued transfers. */
int bbmcDetachUsbQdl(int deviceId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialPipe *pipe;

	if (deviceId != g_usbSerialHost->deviceId[3])
		return -1;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	g_usbSerialHost->attached[3] = 0;
	sceKernelClearEventFlag(g_usbSerialHost->event, ~64u);
	sceKernelSetEventFlag(g_usbSerialHost->event, 128);
	if (g_usbSerialHost->stateChanged[0] != NULL)
		g_usbSerialHost->stateChanged[0](128, g_usbSerialHost->stateContext[0]);
	if (g_usbSerialHost->stateChanged[1] != NULL)
		g_usbSerialHost->stateChanged[1](128, g_usbSerialHost->stateContext[1]);
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind == 4)
		channel = &g_usbSerialChannels[0];
	else if (g_usbSerialChannels[1].kind == 4)
		channel = &g_usbSerialChannels[1];
	else
		return 0;
	pipe = &channel->pipes[0];
	do {
		if (pipe->pipeId > 0) {
			BbmcUsbSerialRequest *request;

			SceCpuForDriver_BF82DEB2(&pipe->lock);
			request = pipe->head;
			pipe->pipeId = 0;
			while (request != NULL) {
				int (*completed)(int, BbmcUsbSerialRequest *, int) = request->completed;
				BbmcUsbSerialRequest *next = request->next;

				pipe->head = next;
				if (completed != NULL)
					completed(channel->kind, request, (int)0x80332206);
				request = next;
			}
			sceKernelSpinlockLowUnlock(&pipe->lock);
		}
		++pipe;
	} while (pipe != &channel->pipes[3]);
	channel->kind = 0;
	return 0;
}

/* name unknown: detach the Qmodem interface and fail its queued transfers. */
int bbmcDetachUsbQmodem(int deviceId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialPipe *pipe;

	if (deviceId != g_usbSerialHost->deviceId[1])
		return -1;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	g_usbSerialHost->attached[1] = 0;
	sceKernelClearEventFlag(g_usbSerialHost->event, ~4u);
	sceKernelSetEventFlag(g_usbSerialHost->event, 8);
	if (g_usbSerialHost->stateChanged[0] != NULL)
		g_usbSerialHost->stateChanged[0](8, g_usbSerialHost->stateContext[0]);
	if (g_usbSerialHost->stateChanged[1] != NULL)
		g_usbSerialHost->stateChanged[1](8, g_usbSerialHost->stateContext[1]);
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind == 2)
		channel = &g_usbSerialChannels[0];
	else if (g_usbSerialChannels[1].kind == 2)
		channel = &g_usbSerialChannels[1];
	else
		return 0;
	pipe = &channel->pipes[0];
	do {
		if (pipe->pipeId > 0) {
			BbmcUsbSerialRequest *request;

			SceCpuForDriver_BF82DEB2(&pipe->lock);
			request = pipe->head;
			pipe->pipeId = 0;
			while (request != NULL) {
				int (*completed)(int, BbmcUsbSerialRequest *, int) = request->completed;
				BbmcUsbSerialRequest *next = request->next;

				pipe->head = next;
				if (completed != NULL)
					completed(channel->kind, request, (int)0x80332206);
				request = next;
			}
			sceKernelSpinlockLowUnlock(&pipe->lock);
		}
		++pipe;
	} while (pipe != &channel->pipes[3]);
	channel->kind = 0;
	return 0;
}

/* name unknown: poll host events and consume the shutdown marker. */
int bbmcInitializeUsbSerialHost(void)
{
	SceUID event;

	memset(g_usbSerialChannels, 0, sizeof(g_usbSerialChannels));
	memset(&g_usbSerialHostStorage, 0, sizeof(g_usbSerialHostStorage));
	g_usbSerialChannels[0].pipes[0].lock = 0;
	g_usbSerialChannels[0].pipes[1].lock = 0;
	g_usbSerialChannels[0].pipes[2].lock = 0;
	g_usbSerialChannels[1].pipes[0].lock = 0;
	g_usbSerialChannels[1].pipes[1].lock = 0;
	g_usbSerialChannels[1].pipes[2].lock = 0;
	g_usbSerialHostStorage.lock = 0;
	event = sceKernelCreateEventFlag("SceUsbBbmcComm", 0x1000, 10, NULL);
	if (event < 0)
		goto fail;
	g_usbSerialHostStorage.event = event;
	g_usbSerialHost = &g_usbSerialHostStorage;
	if (sceUsbdRegisterCompositeLdd(&g_usbSerialQcomDriver) < 0)
		goto fail;
	if (sceUsbdRegisterCompositeLdd(&g_usbSerialQmodemDriver) < 0)
		goto unregisterQcom;
	if (sceUsbdRegisterCompositeLdd(&g_usbSerialQdlDriver) < 0)
		goto unregisterQmodem;
	if (sceUsbdRegisterCompositeLdd(&g_usbSerialQedlDriver) >= 0)
		return 0;
	SceUsbdForDriver_7934BB4A(&g_usbSerialQdlDriver);
unregisterQmodem:
	SceUsbdForDriver_7934BB4A(&g_usbSerialQmodemDriver);
unregisterQcom:
	SceUsbdForDriver_7934BB4A(&g_usbSerialQcomDriver);
fail:
	if (g_usbSerialHostStorage.event > 0) {
		sceKernelDeleteEventFlag(g_usbSerialHostStorage.event);
		g_usbSerialHostStorage.event = 0;
	}
	g_usbSerialHost = NULL;
	return 2;
}

int bbmcFinalizeUsbSerialHost(void)
{
	if (g_usbSerialHost != NULL) {
		sceKernelSetEventFlag(g_usbSerialHost->event, ~0u);
		SceUsbdForDriver_7934BB4A(&g_usbSerialQedlDriver);
		SceUsbdForDriver_7934BB4A(&g_usbSerialQdlDriver);
		SceUsbdForDriver_7934BB4A(&g_usbSerialQmodemDriver);
		SceUsbdForDriver_7934BB4A(&g_usbSerialQcomDriver);
		sceKernelDeleteEventFlag(g_usbSerialHost->event);
		g_usbSerialHost = NULL;
	}
	return 0;
}

int bbmcPollUsbSerialHost(SceUInt32 *events)
{
	int ret;

	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (events == NULL)
		return (int)0x80330002;
	ret = sceKernelPollEventFlag(g_usbSerialHost->event, ~0u, 1, events);
	if ((*events & 0x80000000u) != 0) {
		*events &= ~0x80000000u;
		return (int)0x80332202;
	}
	return ret;
}

int bbmcWaitUsbSerialHost(SceUInt32 events, SceUInt8 waitMode, SceUInt *timeout)
{
	SceUInt32 received;
	int ret;

	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if ((waitMode & 1) != 0)
		events |= 0x80000000u;
	ret = sceKernelWaitEventFlag(g_usbSerialHost->event, events, waitMode & 1,
		&received, timeout);
	ret &= ~((SceInt32)received >> 31);
	if ((SceInt32)received < 0)
		ret = (int)0x80332202;
	return ret;
}

/* name unknown: install a state callback in the first available host slot. */
int bbmcRegisterUsbSerialState(void (*callback)(SceUInt32, void *), void *context)
{
	unsigned int index = 0;
	int ret;

	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialHost->stateChanged[0] != NULL) {
		ret = (int)0x80332207;
		if (g_usbSerialHost->stateChanged[1] != NULL)
			goto out;
		index = 1;
	}
	g_usbSerialHost->stateChanged[index] = callback;
	g_usbSerialHost->stateContext[index] = context;
	ret = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

/* name unknown: remove the matching host state callback and its context. */
int bbmcUnregisterUsbSerialState(void (*callback)(SceUInt32, void *))
{
	unsigned int index;

	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialHost->stateChanged[0] == callback)
		index = 0;
	else if (g_usbSerialHost->stateChanged[1] == callback)
		index = 1;
	else
		goto out;
	g_usbSerialHost->stateChanged[index] = NULL;
	g_usbSerialHost->stateContext[index] = NULL;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return 0;
}

int bbmcOpenUsbSerialQcom(int *channelId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialHost *host;
	BbmcUsbSerialEndpoints *endpoints;
	unsigned int slot = 0;
	int deviceId, ret;

	if (channelId == NULL)
		return (int)0x80330002;
	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (g_usbSerialHost->attached[0] == 0)
		return (int)0x80332203;
	*channelId = 0;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind != 0) {
		if (g_usbSerialChannels[1].kind != 0) {
			ret = (int)0x80332204;
			goto out;
		}
		slot = 1;
	}
	g_usbSerialChannels[slot].kind = 1;
	channel = &g_usbSerialChannels[slot];
	host = g_usbSerialHost;
	deviceId = host->deviceId[0];
	endpoints = &host->endpoints[0];
	if (endpoints == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION) == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (g_usbSerialHost->controlPipe.pipeId == 0) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, NULL);
		if (ret < 0)
			goto fail;
		g_usbSerialHost->controlPipe.pipeId = ret;
	}
	if (host->endpoints[0].interruptEndpoint != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, host->endpoints[0].interruptEndpoint);
		if (ret < 0)
			goto fail;
		channel->pipes[0].pipeId = ret;
	}
	if (endpoints->bulkIn != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkIn);
		if (ret < 0)
			goto fail;
		channel->pipes[1].pipeId = ret;
	}
	if (endpoints->bulkOut != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkOut);
		if (ret < 0)
			goto fail;
		channel->pipes[2].pipeId = ret;
	}
	*channelId = g_usbSerialChannels[slot].kind;
	ret = 0;
	goto out;
fail:
	if (channel->pipes[0].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[0].pipeId);
		channel->pipes[0].pipeId = 0;
	}
	if (channel->pipes[1].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[1].pipeId);
		channel->pipes[1].pipeId = 0;
	}
	if (channel->pipes[2].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[2].pipeId);
		channel->pipes[2].pipeId = 0;
	}
	if (g_usbSerialHost->controlPipe.pipeId > 0) {
		SceUsbdForDriver_F304DC5C(g_usbSerialHost->controlPipe.pipeId);
		g_usbSerialHost->controlPipe.pipeId = 0;
	}
reset:
	g_usbSerialChannels[slot].kind = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

int bbmcCloseUsbSerialQcom(int channel)
{
	return bbmcCloseUsbSerialInterface(0, channel);
}

int bbmcOpenUsbSerialAt(int *channelId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialHost *host;
	BbmcUsbSerialEndpoints *endpoints;
	unsigned int slot = 0;
	int deviceId, ret;

	if (channelId == NULL)
		return (int)0x80330002;
	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (g_usbSerialHost->attached[1] == 0)
		return (int)0x80332203;
	*channelId = 0;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind != 0) {
		if (g_usbSerialChannels[1].kind != 0) {
			ret = (int)0x80332204;
			goto out;
		}
		slot = 1;
	}
	g_usbSerialChannels[slot].kind = 2;
	channel = &g_usbSerialChannels[slot];
	host = g_usbSerialHost;
	deviceId = host->deviceId[1];
	endpoints = &host->endpoints[1];
	if (endpoints == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION) == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (g_usbSerialHost->controlPipe.pipeId == 0) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, NULL);
		if (ret < 0)
			goto fail;
		g_usbSerialHost->controlPipe.pipeId = ret;
	}
	if (host->endpoints[1].interruptEndpoint != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, host->endpoints[1].interruptEndpoint);
		if (ret < 0)
			goto fail;
		channel->pipes[0].pipeId = ret;
	}
	if (endpoints->bulkIn != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkIn);
		if (ret < 0)
			goto fail;
		channel->pipes[1].pipeId = ret;
	}
	if (endpoints->bulkOut != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkOut);
		if (ret < 0)
			goto fail;
		channel->pipes[2].pipeId = ret;
	}
	*channelId = g_usbSerialChannels[slot].kind;
	ret = 0;
	goto out;
fail:
	if (channel->pipes[0].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[0].pipeId);
		channel->pipes[0].pipeId = 0;
	}
	if (channel->pipes[1].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[1].pipeId);
		channel->pipes[1].pipeId = 0;
	}
	if (channel->pipes[2].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[2].pipeId);
		channel->pipes[2].pipeId = 0;
	}
	if (g_usbSerialHost->controlPipe.pipeId > 0) {
		SceUsbdForDriver_F304DC5C(g_usbSerialHost->controlPipe.pipeId);
		g_usbSerialHost->controlPipe.pipeId = 0;
	}
reset:
	g_usbSerialChannels[slot].kind = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

int bbmcCloseUsbSerialAt(int channel)
{
	return bbmcCloseUsbSerialInterface(1, channel);
}

int bbmcOpenUsbSerialQedl(int *channelId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialHost *host;
	BbmcUsbSerialEndpoints *endpoints;
	unsigned int slot = 0;
	int deviceId, ret;

	if (channelId == NULL)
		return (int)0x80330002;
	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (g_usbSerialHost->attached[2] == 0)
		return (int)0x80332203;
	*channelId = 0;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind != 0) {
		if (g_usbSerialChannels[1].kind != 0) {
			ret = (int)0x80332204;
			goto out;
		}
		slot = 1;
	}
	g_usbSerialChannels[slot].kind = 3;
	channel = &g_usbSerialChannels[slot];
	host = g_usbSerialHost;
	deviceId = host->deviceId[2];
	endpoints = &host->endpoints[2];
	if (endpoints == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION) == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (g_usbSerialHost->controlPipe.pipeId == 0) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, NULL);
		if (ret < 0)
			goto fail;
		g_usbSerialHost->controlPipe.pipeId = ret;
	}
	if (host->endpoints[2].interruptEndpoint != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, host->endpoints[2].interruptEndpoint);
		if (ret < 0)
			goto fail;
		channel->pipes[0].pipeId = ret;
	}
	if (endpoints->bulkIn != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkIn);
		if (ret < 0)
			goto fail;
		channel->pipes[1].pipeId = ret;
	}
	if (endpoints->bulkOut != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkOut);
		if (ret < 0)
			goto fail;
		channel->pipes[2].pipeId = ret;
	}
	*channelId = g_usbSerialChannels[slot].kind;
	ret = 0;
	goto out;
fail:
	if (channel->pipes[0].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[0].pipeId);
		channel->pipes[0].pipeId = 0;
	}
	if (channel->pipes[1].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[1].pipeId);
		channel->pipes[1].pipeId = 0;
	}
	if (channel->pipes[2].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[2].pipeId);
		channel->pipes[2].pipeId = 0;
	}
	if (g_usbSerialHost->controlPipe.pipeId > 0) {
		SceUsbdForDriver_F304DC5C(g_usbSerialHost->controlPipe.pipeId);
		g_usbSerialHost->controlPipe.pipeId = 0;
	}
reset:
	g_usbSerialChannels[slot].kind = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

int bbmcCloseUsbSerialQedl(int channel)
{
	return bbmcCloseUsbSerialInterface(2, channel);
}

int bbmcOpenUsbSerialQdl(int *channelId)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialHost *host;
	BbmcUsbSerialEndpoints *endpoints;
	unsigned int slot = 0;
	int deviceId, ret;

	if (channelId == NULL)
		return (int)0x80330002;
	if (g_usbSerialHost == NULL)
		return (int)0x80332201;
	if (g_usbSerialHost->attached[3] == 0)
		return (int)0x80332203;
	*channelId = 0;
	SceCpuForDriver_BF82DEB2(&g_usbSerialHost->lock);
	if (g_usbSerialChannels[0].kind != 0) {
		if (g_usbSerialChannels[1].kind != 0) {
			ret = (int)0x80332204;
			goto out;
		}
		slot = 1;
	}
	g_usbSerialChannels[slot].kind = 4;
	channel = &g_usbSerialChannels[slot];
	host = g_usbSerialHost;
	deviceId = host->deviceId[3];
	endpoints = &host->endpoints[3];
	if (endpoints == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION) == NULL) {
		ret = (int)0x80332205;
		goto reset;
	}
	if (g_usbSerialHost->controlPipe.pipeId == 0) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, NULL);
		if (ret < 0)
			goto fail;
		g_usbSerialHost->controlPipe.pipeId = ret;
	}
	if (host->endpoints[3].interruptEndpoint != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, host->endpoints[3].interruptEndpoint);
		if (ret < 0)
			goto fail;
		channel->pipes[0].pipeId = ret;
	}
	if (endpoints->bulkIn != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkIn);
		if (ret < 0)
			goto fail;
		channel->pipes[1].pipeId = ret;
	}
	if (endpoints->bulkOut != NULL) {
		ret = SceUsbdForDriver_1CDBFF9F(deviceId, endpoints->bulkOut);
		if (ret < 0)
			goto fail;
		channel->pipes[2].pipeId = ret;
	}
	*channelId = g_usbSerialChannels[slot].kind;
	ret = 0;
	goto out;
fail:
	if (channel->pipes[0].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[0].pipeId);
		channel->pipes[0].pipeId = 0;
	}
	if (channel->pipes[1].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[1].pipeId);
		channel->pipes[1].pipeId = 0;
	}
	if (channel->pipes[2].pipeId > 0) {
		SceUsbdForDriver_F304DC5C(channel->pipes[2].pipeId);
		channel->pipes[2].pipeId = 0;
	}
	if (g_usbSerialHost->controlPipe.pipeId > 0) {
		SceUsbdForDriver_F304DC5C(g_usbSerialHost->controlPipe.pipeId);
		g_usbSerialHost->controlPipe.pipeId = 0;
	}
reset:
	g_usbSerialChannels[slot].kind = 0;
out:
	sceKernelSpinlockLowUnlock(&g_usbSerialHost->lock);
	return ret;
}

int bbmcCloseUsbSerialQdl(int channel)
{
	return bbmcCloseUsbSerialInterface(3, channel);
}

int bbmcControlUsbSerial(int channelId, BbmcUsbSerialRequest *request)
{
	BbmcUsbSerialHost *host;
	BbmcUsbSerialRequest *head, *current;
	__typeof__(&SceUsbdForDriver_235C094A) bulkTransfer = NULL;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	host = g_usbSerialHost;
	if (host->controlPipe.pipeId <= 0)
		return (int)0x80332206;
	if (g_usbSerialChannels[0].kind != channelId &&
	    g_usbSerialChannels[1].kind != channelId)
		return (int)0x80332204;
	request->queueOwner.controlPipe = &host->controlPipe;
	SceCpuForDriver_BF82DEB2(&host->controlPipe.lock);
	request->next = NULL;
	if (host->controlPipe.head != NULL) {
		host->controlPipe.tail->next = request;
		host->controlPipe.tail = request;
	} else {
		host->controlPipe.head = request;
		host->controlPipe.tail = request;
	}
	sceKernelSpinlockLowUnlock(&host->controlPipe.lock);
	if (request->transferType != 0)
		ret = bulkTransfer(host->controlPipe.pipeId,
				   request->arguments.bulk.data,
				   request->arguments.bulk.length,
				   bbmcUsbSerialControlCompleted, request);
	else
		ret = SceUsbdForDriver_2E05660F(host->controlPipe.pipeId,
					     &request->arguments.control,
					     request->controlData,
					     bbmcUsbSerialControlCompleted, request);
	if (ret >= 0)
		return ret;
	SceCpuForDriver_BF82DEB2(&host->controlPipe.lock);
	head = host->controlPipe.head;
	current = head;
	if (head == NULL)
		goto empty;
	if (request == head) {
		head = request->next;
		host->controlPipe.head = head;
	} else {
		while (current->next != NULL) {
			if (current->next == request) {
				current->next = request->next;
				goto removed;
			}
			current = current->next;
		}
		goto out;
	}
removed:
	if (head == NULL) {
empty:
		host->controlPipe.tail = NULL;
	}
out:
	sceKernelSpinlockLowUnlock(&host->controlPipe.lock);
	request->queueOwner.controlPipe = NULL;
	return ret;
}

int bbmcWriteUsbSerial(int channelId, BbmcUsbSerialRequest *request)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialRequest *head, *current;
	unsigned int slot;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	if (g_usbSerialChannels[0].kind == channelId)
		slot = 0;
	else if (g_usbSerialChannels[1].kind == channelId)
		slot = 1;
	else
		return (int)0x80332204;
	channel = &g_usbSerialChannels[slot];
	if (channel->pipes[2].pipeId == 0)
		return (int)0x80332206;
	request->queueOwner.channel = channel;
	SceCpuForDriver_BF82DEB2(&channel->pipes[2].lock);
	request->next = NULL;
	if (channel->pipes[2].head != NULL) {
		channel->pipes[2].tail->next = request;
		channel->pipes[2].tail = request;
	} else {
		channel->pipes[2].head = request;
		channel->pipes[2].tail = request;
	}
	sceKernelSpinlockLowUnlock(&channel->pipes[2].lock);
	if (request->transferType != 0)
		ret = SceUsbdForDriver_235C094A(channel->pipes[2].pipeId,
				   request->arguments.bulk.data,
				   request->arguments.bulk.length,
				   bbmcUsbSerialOutCompleted, request);
	else
		ret = SceUsbdForDriver_2E05660F(channel->pipes[2].pipeId,
					     &request->arguments.control,
					     request->controlData,
					     bbmcUsbSerialOutCompleted, request);
	if (ret >= 0)
		return ret;
	SceCpuForDriver_BF82DEB2(&channel->pipes[2].lock);
	head = channel->pipes[2].head;
	current = head;
	if (head == NULL)
		goto empty;
	if (request == head) {
		head = request->next;
		channel->pipes[2].head = head;
	} else {
		while (current->next != NULL) {
			if (current->next == request) {
				current->next = request->next;
				goto removed;
			}
			current = current->next;
		}
		goto out;
	}
removed:
	if (head == NULL) {
empty:
		channel->pipes[2].tail = NULL;
	}
out:
	sceKernelSpinlockLowUnlock(&channel->pipes[2].lock);
	request->queueOwner.channel = NULL;
	return ret;
}

int bbmcReadUsbSerial(int channelId, BbmcUsbSerialRequest *request)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialRequest *head, *current;
	unsigned int slot;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	if (g_usbSerialChannels[0].kind == channelId)
		slot = 0;
	else if (g_usbSerialChannels[1].kind == channelId)
		slot = 1;
	else
		return (int)0x80332204;
	channel = &g_usbSerialChannels[slot];
	if (channel->pipes[1].pipeId == 0)
		return (int)0x80332206;
	request->queueOwner.channel = channel;
	SceCpuForDriver_BF82DEB2(&channel->pipes[1].lock);
	request->next = NULL;
	if (channel->pipes[1].head != NULL) {
		channel->pipes[1].tail->next = request;
		channel->pipes[1].tail = request;
	} else {
		channel->pipes[1].head = request;
		channel->pipes[1].tail = request;
	}
	sceKernelSpinlockLowUnlock(&channel->pipes[1].lock);
	if (request->transferType != 0)
		ret = SceUsbdForDriver_235C094A(channel->pipes[1].pipeId,
				   request->arguments.bulk.data,
				   request->arguments.bulk.length,
				   bbmcUsbSerialInCompleted, request);
	else
		ret = SceUsbdForDriver_2E05660F(channel->pipes[1].pipeId,
					     &request->arguments.control,
					     request->controlData,
					     bbmcUsbSerialInCompleted, request);
	if (ret >= 0)
		return ret;
	SceCpuForDriver_BF82DEB2(&channel->pipes[1].lock);
	head = channel->pipes[1].head;
	current = head;
	if (head == NULL)
		goto empty;
	if (request == head) {
		head = request->next;
		channel->pipes[1].head = head;
	} else {
		while (current->next != NULL) {
			if (current->next == request) {
				current->next = request->next;
				goto removed;
			}
			current = current->next;
		}
		goto out;
	}
removed:
	if (head == NULL) {
empty:
		channel->pipes[1].tail = NULL;
	}
out:
	sceKernelSpinlockLowUnlock(&channel->pipes[1].lock);
	request->queueOwner.channel = NULL;
	return ret;
}

int bbmcInterruptUsbSerial(int channelId, BbmcUsbSerialRequest *request)
{
	BbmcUsbSerialChannel *channel;
	BbmcUsbSerialRequest *head, *current;
	unsigned int slot;
	int ret;

	if (request == NULL)
		return (int)0x80330002;
	if (g_usbSerialChannels[0].kind == channelId)
		slot = 0;
	else if (g_usbSerialChannels[1].kind == channelId)
		slot = 1;
	else
		return (int)0x80332204;
	channel = &g_usbSerialChannels[slot];
	if (channel->pipes[0].pipeId == 0)
		return (int)0x80332206;
	request->queueOwner.channel = channel;
	SceCpuForDriver_BF82DEB2(&channel->pipes[0].lock);
	request->next = NULL;
	if (channel->pipes[0].head != NULL) {
		channel->pipes[0].tail->next = request;
		channel->pipes[0].tail = request;
	} else {
		channel->pipes[0].head = request;
		channel->pipes[0].tail = request;
	}
	sceKernelSpinlockLowUnlock(&channel->pipes[0].lock);
	if (request->transferType != 0)
		ret = SceUsbdForDriver_A0BF85B8(channel->pipes[0].pipeId,
				   request->arguments.bulk.data,
				   request->arguments.bulk.length,
				   bbmcUsbSerialInterruptCompleted, request);
	else
		ret = SceUsbdForDriver_2E05660F(channel->pipes[0].pipeId,
					     &request->arguments.control,
					     request->controlData,
					     bbmcUsbSerialInterruptCompleted, request);
	if (ret >= 0)
		return ret;
	SceCpuForDriver_BF82DEB2(&channel->pipes[0].lock);
	head = channel->pipes[0].head;
	current = head;
	if (head == NULL)
		goto empty;
	if (request == head) {
		head = request->next;
		channel->pipes[0].head = head;
	} else {
		while (current->next != NULL) {
			if (current->next == request) {
				current->next = request->next;
				goto removed;
			}
			current = current->next;
		}
		goto out;
	}
removed:
	if (head == NULL) {
empty:
		channel->pipes[0].tail = NULL;
	}
out:
	sceKernelSpinlockLowUnlock(&channel->pipes[0].lock);
	request->queueOwner.channel = NULL;
	return ret;
}


