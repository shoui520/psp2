/*
 * SceUsbd: Sony's usbd.c, the bus and driver layer of the USB host stack.
 *
 * Two threads do the work. The event thread ("SceUsbdEvent") enumerates a device
 * when a hub reports an attach and tears it down on a detach; the callback thread
 * ("SceUsbdCallback") runs the transfer completion callbacks. All state is under
 * one fast mutex; the event queues are under a spinlock because the controllers'
 * interrupt handlers fill them.
 */

#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <fw.h>
#include "usbd_internal.h"

/* the module's data, one object in this order (0x81010000) */
static struct {
	SceUInt8 initialized;
	SceUID eventThreadId;
	SceUID callbackThreadId;
	SceUID deviceEventSemaId;
	SceUID callbackSemaId;
	SceUID enumerationSemaId;
	SceUID bufferMemBlockId;
	SceUID procEventHandlerId;
	int smallBufferCount;
	int largeBufferCount;
	UsbdBus *bus[USBD_BUS_COUNT];
	UsbdDriver *driver[USBD_DRIVER_COUNT];		/* registered, by index */
	UsbdRequest *request[USBD_REQUEST_COUNT];	/* in flight, by id */
	UsbdClient *clientHead;
	UsbdClient *clientTail;
	UsbdDriver *freeDriverHead;
	UsbdDriver *freeDriverTail;
	SceKernelFastMutex mutex;
	SceKernelSpinlock queueLock;
	UsbdRequest *freeRequestHead;
	UsbdRequest *freeRequestTail;
	UsbdEventQueue deviceEvents;		/* for the event thread */
	UsbdEventQueue callbackEvents;		/* for the callback thread */
	UsbdBufferPool smallBuffers;
	UsbdBufferPool largeBuffers;
	SceUID eventId;			/* the callback event */
	SceUID callbackId;
	SceUID callbackPid;
	unsigned int callbackFlags;
} s_usbd;

static int usbdProcessExit(SceUID pid, ProcEventParam *param, void *common);
static int usbdProcessKill(SceUID pid, ProcEventParam *param, void *common);
static int usbdProcessStop(SceUID pid, int type, ProcEventParam *param, void *common);
static int usbdProcessStart(SceUID pid, int type, ProcEventParam *param, void *common);

static const SceKernelProcEventHandler usbdProcEventHandler = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = NULL,
	.exit = usbdProcessExit,
	.kill = usbdProcessKill,
	.suspend = usbdProcessStop,
	.resume = usbdProcessStart,
	.notify = NULL,
};

/* doubly linked lists kept as head/tail pairs */
static __always_inline void usbdLock(void)
{
	int result = lockFastMutex(&s_usbd.mutex);

	if (result < 0) {
		SCE_DBG_PRINTF(15, 111, "result=%x\n", result);
		SCE_DBG_ASSERT(0, 112);
	}
}

static __always_inline void usbdUnlock(void)
{
	int result = sceKernelUnlockFastMutex(&s_usbd.mutex);

	if (result < 0) {
		SCE_DBG_PRINTF(15, 122, "result=%x\n", result);
		SCE_DBG_ASSERT(0, 123);
	}
}

static __always_inline int usbdWaitDeviceResponse(void)
{
	SceUInt32 timeout = USBD_RESPONSE_TIMEOUT;
	int result = sceKernelWaitSema(s_usbd.enumerationSemaId, 1, &timeout);

	if (result < 0)
		SCE_DBG_PRINTF(15, 135, "result=%x\n", result);
	return result;
}

static __always_inline void usbdSignalDeviceResponse(void)
{
#if FW_SINCE(0x04000000)
	int result = sceKernelSignalSema(s_usbd.enumerationSemaId, 1);

	if (result < 0) {
		SCE_DBG_PRINTF(15, 150, "result=%x\n", result);
		SCE_DBG_ASSERT(0, 151);
	}
#else
	sceKernelSignalSema(s_usbd.enumerationSemaId, 1);
#endif
}

static __always_inline UsbdDevice *usbdGetDevice(unsigned int deviceId)
{
	return s_usbd.bus[USBD_ID_BUS(deviceId)]->device[USBD_ID_INDEX(deviceId)];
}

static __always_inline UsbdPipe *usbdGetPipe(unsigned int pipeId)
{
	return s_usbd.bus[USBD_ID_BUS(pipeId)]->pipe[USBD_ID_INDEX(pipeId)];
}

static __always_inline int usbdIsDeviceId(unsigned int deviceId)
{
	return USBD_ID_BUS(deviceId) < USBD_BUS_COUNT &&
	       (unsigned char)(USBD_ID_INDEX(deviceId) - 1) < USBD_DEVICES_PER_BUS - 1;
}

static __always_inline int usbdIsPipeId(unsigned int pipeId)
{
	return USBD_ID_BUS(pipeId) < USBD_BUS_COUNT &&
	       USBD_ID_INDEX(pipeId) < USBD_PIPES_PER_BUS;
}

static __always_inline UsbdClient *usbdFindClient(SceUID uid)
{
	UsbdClient *client;

	for (client = s_usbd.clientHead; client != NULL; client = client->next) {
		if (client->pid == uid)
			break;
	}
	return client;
}

/* the first driver attached to one of the device's interfaces */
static __always_inline UsbdDriver *usbdGetAttachedDriver(UsbdDevice *device)
{
	int i;

	for (i = 0; i < device->interfaceCount; i++) {
		if (device->driver[i] != NULL)
			return device->driver[i];
	}
	return NULL;
}

static __always_inline int usbdHasNoDriver(UsbdDevice *device)
{
	int i;

	for (i = 0; i < USBD_INTERFACE_COUNT; i++) {
		if (device->driver[i] != NULL)
			return 0;
	}
	return 1;
}

static __always_inline UsbdRequest *usbdAllocRequest(void)
{
	UsbdRequest *request = s_usbd.freeRequestHead;

	if (request == NULL)
		return NULL;
	USBD_LIST_REMOVE(s_usbd.freeRequestHead, s_usbd.freeRequestTail, request);
	s_usbd.request[request->id] = request;
	request->state = 0;
	return request;
}

static __always_inline void *usbdAllocBuffer(UsbdBufferPool *pool)
{
	void **buffer = pool->head;

	if (buffer == NULL)
		return NULL;
	pool->head = *buffer;
	if (pool->head == NULL)
		pool->tail = NULL;
	return buffer;
}

static __always_inline void usbdFreeBuffer(void *buffer)
{
	UsbdBufferPool *pool;

	if (buffer >= s_usbd.largeBuffers.base && buffer <= s_usbd.largeBuffers.last)
		pool = &s_usbd.largeBuffers;
	else if (buffer >= s_usbd.smallBuffers.base && buffer <= s_usbd.smallBuffers.last)
		pool = &s_usbd.smallBuffers;
	else
		return;
	if (pool->tail != NULL)
		*pool->tail = buffer;
	pool->tail = buffer;
	*(void **)buffer = NULL;
}

static __always_inline void usbdQueueEvent(UsbdEventQueue *queue, UsbdEvent *event)
{
	SceKernelIntrStatus intr;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_usbd.queueLock);
	USBD_LIST_APPEND(queue->head, queue->tail, event);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_usbd.queueLock, intr);
}

static __always_inline UsbdEvent *usbdDequeueEvent(UsbdEventQueue *queue)
{
	SceKernelIntrStatus intr;
	UsbdEvent *event;

	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_usbd.queueLock);
	event = queue->head;
	if (event != NULL)
		USBD_LIST_REMOVE(queue->head, queue->tail, event);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_usbd.queueLock, intr);
	return event;
}

static int usbdProcessStop(SceUID pid, int type, ProcEventParam *param, void *common)
{
	return 0;
}

static int usbdProcessStart(SceUID pid, int type, ProcEventParam *param, void *common)
{
	return 0;
}

/* a controller finished one of the API's transfers: run its callback in the thread */
static void usbdCompleteRequest(UsbdRequest *request)
{
	UsbdEvent *event = usbdAlloc(sizeof(*event));

	if (event == NULL)
		return;
	event->arg0 = (unsigned int)request;
	event->type = USBD_EVENT_COMPLETE;
	usbdQueueEvent(&s_usbd.callbackEvents, event);
	sceKernelSignalSema(s_usbd.callbackSemaId, 1);
}

/* the enumeration's control transfers, each waited for by usbdWaitDeviceResponse */
static void usbdConfigurationDescriptorDone(UsbdRequest *request)
{
	usbdSignalDeviceResponse();
}

static void usbdDeviceDescriptorDone(UsbdRequest *request)
{
	usbdSignalDeviceResponse();
}

static void usbdSetConfigurationDone(UsbdRequest *request)
{
	usbdSignalDeviceResponse();
}

static void usbdSetAddressDone(UsbdRequest *request)
{
	usbdSignalDeviceResponse();
}

/* a standard request on the device's default pipe, with the device's own request */
static __always_inline int usbdSubmitDeviceRequest(UsbdPipe *pipe, void (*complete)(UsbdRequest *),
					  unsigned int type, unsigned int request,
					  unsigned int value, unsigned int length, void *buffer)
{
	UsbdRequest *deviceRequest = &pipe->device->request;
	SceUsbdDeviceRequest *setup = deviceRequest->setup;

	deviceRequest->complete = complete;
	setup->bmRequestType = type;
	setup->bRequest = request;
	setup->wValue = value;
	setup->wIndex = 0;
	setup->wLength = length;
	pipe->direction = type & SCE_USBD_REQTYPE_DIR_BITS;
	deviceRequest->length = length;
	deviceRequest->buffer = buffer;
	deviceRequest->done = NULL;
	deviceRequest->result = 0;
	deviceRequest->count = 0;
	deviceRequest->pipe = pipe;
	return pipe->device->hcd->submitRequest(pipe, deviceRequest);
}

static int usbdSetConfiguration(UsbdDevice *device, int configuration)
{
	UsbdRequest *request;
	UsbdPipe *pipe;
	int result, closed;

	pipe = device->hcd->allocPipe(device, NULL);
	if (pipe == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	request = &pipe->device->request;
	result = usbdSubmitDeviceRequest(pipe, usbdSetConfigurationDone,
					 SCE_USBD_REQTYPE_DIR_TO_DEVICE,
					 SCE_USBD_REQUEST_SET_CONFIGURATION, configuration, 0, NULL);
	if (result < 0)
		goto close;
	result = usbdWaitDeviceResponse();
	if (result < 0)
		goto close;
	closed = device->hcd->closePipe(pipe);
	result = request->result;
	if (result == 0)
		result = closed;
	return result;

close:
	device->hcd->closePipe(pipe);
	return result;
}

/* the client of a process that ends */
static __always_inline int usbdReleaseClient(SceUID pid)
{
	UsbdClient *client, *entry;
	int result;

	usbdLock();
	client = usbdFindClient(pid);
	if (client == NULL) {
		result = 0;
		goto out;
	}
	for (entry = s_usbd.clientHead; entry != NULL; entry = entry->next) {
		if (entry == client)
			break;
	}
	if (entry == NULL) {
		result = SCE_USBD_ERROR_NOT_INITIALIZED;
		goto out;
	}
	result = sceKernelDeleteSema(client->semaId);
	if (result >= 0) {
		USBD_LIST_REMOVE(s_usbd.clientHead, s_usbd.clientTail, client);
		result = 0;
		usbdFree(client);
	}

out:
	if (s_usbd.callbackPid == pid) {
		s_usbd.callbackId = -1;
		s_usbd.callbackPid = -1;
		s_usbd.callbackFlags = 0;
	}
	usbdUnlock();
	return result;
}

static int usbdProcessKill(SceUID pid, ProcEventParam *param, void *common)
{
	return usbdReleaseClient(pid);
}

static int usbdProcessExit(SceUID pid, ProcEventParam *param, void *common)
{
	return usbdReleaseClient(pid);
}

static void usbdNotifyClients(UsbdDevice *device, UsbdBus *bus, int type)
{
	UsbdClientEvent *event;
	UsbdClient *client;

	for (client = s_usbd.clientHead; client != NULL; client = client->next) {
		if (client->closing)
			continue;
		event = usbdAlloc(sizeof(*event));
		if (event == NULL)
			continue;
		event->type = type;
		event->address = device->address;
		event->bus = bus->index;
		USBD_LIST_APPEND(client->eventHead, client->eventTail, event);
		usbdUnlock();
		sceKernelSignalSema(client->semaId, 1);
		usbdLock();
	}
}

static __always_inline void usbdReleaseDevice(UsbdBus *bus, UsbdDevice *device)
{
	UsbdDevice *entry;
	UsbdDevice *parent;

	for (entry = bus->freeDeviceHead; entry != NULL; entry = entry->next) {
		if (entry == device)
			return;
	}
	parent = device->parent;
	if (parent != NULL) {
		USBD_LIST_REMOVE(parent->childHead, parent->childTail, device);
		device->parent = NULL;
	}
	usbdFree(device->descriptor);
	USBD_LIST_APPEND(bus->freeDeviceHead, bus->freeDeviceTail, device);
}

/* detach every device below a hub, deepest first; called locked */
static void usbdDetachChildren(UsbdDevice **children)
{
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	int i;

	while ((device = *children) != NULL) {
		usbdDetachChildren(&device->childHead);
		while (device->pipeHead != NULL)
			device->hcd->closePipe(device->pipeHead);
		bus = device->bus;
		for (i = 0; i < device->interfaceCount; i++) {
			driver = device->driver[i];
			if (driver == NULL)
				continue;
			if (driver->flags & USBD_DRIVER_KERNEL) {
				usbdUnlock();
				if (driver->flags & USBD_DRIVER_COMPOSITE)
					driver->compositeDriver.detach(USBD_ID(bus->index, device->address));
				else
					driver->driver.detach(USBD_ID(bus->index, device->address));
				usbdLock();
			} else {
				usbdNotifyClients(device, bus, USBD_CLIENT_EVENT_DETACH);
			}
		}
		bus->device[device->address] = NULL;
		usbdReleaseDevice(bus, device);
	}
}

/* sceUsbdHostStop, sceUsbdResume: every device on the bus goes; called locked */
static int usbdDetachBus(SceUInt8 busIndex)
{
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdClientEvent *event;
	UsbdClient *client;
	UsbdPipe *pipe;
	UsbdBus *bus;
	int address, i;

	bus = s_usbd.bus[busIndex];
	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	for (address = 1; address < USBD_DEVICES_PER_BUS; address++) {
		device = bus->device[address];
		if (device == NULL)
			continue;
		for (i = 0; i < device->interfaceCount; i++) {
			driver = device->driver[i];
			if (driver == NULL)
				continue;
			pipe = device->pipeHead;
			while (pipe != NULL) {
				if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL &&
				    device->defaultPipeRefs != 0) {
					device->defaultPipeRefs--;
					continue;
				}
				if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
					device->defaultPipe = NULL;
				else if ((driver->flags & USBD_DRIVER_COMPOSITE) &&
					 pipe->interfaceNumber != 0)
					continue;
				device->hcd->closePipe(pipe);
				pipe = device->pipeHead;
			}
			device->configuration = -1;
			device->driver[driver->interfaceNumber] = NULL;
			if (driver->flags & USBD_DRIVER_KERNEL) {
				usbdUnlock();
				if (driver->driver.detach != NULL)
					driver->driver.detach(USBD_ID(bus->index, device->address));
				else if (driver->compositeDriver.detach != NULL)
					driver->compositeDriver.detach(USBD_ID(bus->index, device->address));
				usbdLock();
				continue;
			}
			for (client = s_usbd.clientHead; client != NULL; client = client->next) {
				if (client->closing)
					continue;
				event = usbdAlloc(sizeof(*event));
				if (event == NULL)
					continue;
				event->type = USBD_CLIENT_EVENT_DETACH;
				event->address = device->address;
				event->bus = bus->index;
				USBD_LIST_APPEND(client->eventHead, client->eventTail, event);
				sceKernelSignalSema(client->semaId, 1);
			}
		}
		bus->device[device->address] = NULL;
		usbdReleaseDevice(bus, device);
	}
	return 0;
}

/*
 * Copy the descriptors read during enumeration (the device descriptor, then the
 * configuration) from the bus's buffer into the device's, repairing what broken
 * devices send. Returns the configuration's length.
 */
static __always_inline int usbdSanityCheck(UsbdDevice *device, UsbdBus *bus)
{
	SceUInt8 *deviceDescriptor = NULL, *configuration = NULL, *interface = NULL;
	SceUInt8 *copiedConfiguration = NULL, *copiedInterface = NULL;
	SceUInt8 *p = bus->enumerationBuffer;
	SceUInt8 *end = bus->enumerationCursor;
	SceUInt8 *out = device->descriptor;
	unsigned int num_interfaces = 0, num_endpoints = 0;
	unsigned int length;
	int total = 0;

	while (p < end) {
		length = p[0];
		if (length <= 1) {
			SCE_DBG_PRINTF(6, 1167, "### short bLength ###\n");
			*p++ = 1;
			continue;
		}
		switch (p[1]) {
		case SCE_USBD_DESCRIPTOR_DEVICE:
			if (deviceDescriptor != NULL) {
				SCE_DBG_PRINTF(6, 1176, "### device descriptor is already found ###\n");
				goto skip;
			}
			deviceDescriptor = p;
			break;
		case SCE_USBD_DESCRIPTOR_CONFIGURATION:
			if (deviceDescriptor == NULL) {
				SCE_DBG_PRINTF(6, 1186, "### device descriptor is not yet found ###\n");
				goto skip;
			}
			if (configuration != NULL) {
				SCE_DBG_PRINTF(6, 1192, "### configuration descriptor is already found ###\n");
				goto skip;
			}
			if (length != 9) {
				SCE_DBG_PRINTF(6, 1198, "### bLength of configuration descriptor must be 9 ###\n");
				length = 9;
				p[0] = 9;
			}
			copiedConfiguration = out;
			configuration = p;
			break;
		case SCE_USBD_DESCRIPTOR_INTERFACE:
			if (deviceDescriptor == NULL || configuration == NULL) {
				SCE_DBG_PRINTF(6, 1212, "### configuration descriptor is not yet found ###\n");
				goto skip;
			}
			if (length != 9) {
				SCE_DBG_PRINTF(6, 1218, "### bLength of interface descriptor must be 9 ###\n");
				p[0] = 9;
			}
			if (interface == NULL || interface[2] != p[2]) {
				if (configuration[4] <= num_interfaces) {
					SCE_DBG_PRINTF(6, 1231, "### num_interfaces is exceed bNumInterfaces ###\n");
					goto skip;
				}
				num_interfaces++;
			} else if (interface[3] == p[3]) {
				SCE_DBG_PRINTF(6, 1242, "### found same interface descriptor ###\n");
				goto skip;
			}
			if (interface != NULL && interface[4] != num_endpoints) {
				SCE_DBG_PRINTF(6, 1248, "### adjust the bNumEndpoints ###\n");
				if (copiedInterface != NULL)
					copiedInterface[4] = num_endpoints;
			}
			length = p[0];
			copiedInterface = out;
			interface = p;
			num_endpoints = 0;
			break;
		case SCE_USBD_DESCRIPTOR_ENDPOINT:
			if (configuration == NULL || deviceDescriptor == NULL || interface == NULL) {
				SCE_DBG_PRINTF(6, 1265, "### configuration or interface descriptor is not yet found ###\n");
				goto skip;
			}
			if (interface[4] <= num_endpoints) {
				SCE_DBG_PRINTF(6, 1276, "### num_endpooints is exceed bNumEndpoints ###\n");
				goto skip;
			}
			num_endpoints++;
			break;
		}
		if (p + length > end) {
			p += length;
			continue;
		}
		memcpy(out, p, length);
		out += p[0];
		if (p[1] != SCE_USBD_DESCRIPTOR_DEVICE)
			total += p[0];
		p += p[0];
		continue;
skip:
		p += p[0];
	}
	if (deviceDescriptor == NULL || configuration == NULL) {
		SCE_DBG_PRINTF(6, 1292, "### device or configuration descriptor was not found ###\n");
		return -1;
	}
	if (configuration[4] != num_interfaces) {
		SCE_DBG_PRINTF(6, 1298, "### adjust the bNumInterfaces of configuration descriptor ###\n");
		if (copiedConfiguration != NULL)
			copiedConfiguration[4] = num_interfaces;
	}
	if (interface != NULL && interface[4] != num_endpoints) {
		SCE_DBG_PRINTF(6, 1305, "### adjust the bNumInterfaces of configuration descriptor ###\n");
		if (copiedInterface != NULL)
			copiedInterface[4] = num_endpoints;
	}
	if (((SceUsbdConfigurationDescriptor *)configuration)->wTotalLength != total) {
		SCE_DBG_PRINTF(6, 1312, "### adjust the wTotalLength of configuration descriptor ###\n");
		if (copiedConfiguration != NULL)
			((SceUsbdConfigurationDescriptor *)copiedConfiguration)->wTotalLength = total;
	}
	return total;
}

static __always_inline int usbdIsSingleConfigurationQuirk(const SceUsbdDeviceDescriptor *descriptor)
{
	return (descriptor->idVendor == USBD_VENDOR_0BDA &&
		descriptor->idProduct == USBD_PRODUCT_0BDA_8152) ||
	       (descriptor->idVendor == USBD_VENDOR_SONY &&
		descriptor->idProduct == USBD_PRODUCT_SONY_08A5);
}

/* the interface descriptor a composite driver's probe chose, and its configuration */
static __always_inline int usbdFindInterface(UsbdDevice *device,
				    const SceUsbdInterfaceDescriptor *chosen,
				    int *configurationValue, int *interfaceNumber)
{
	const SceUInt8 *p = device->descriptor;
	const SceUInt8 *configuration = NULL;
	unsigned int alternates = 0;
	unsigned int length;

	if (p == NULL)
		return 0;
	for (length = p[0]; length > 1 && p < device->descriptorEnd; length = p[0]) {
		if (p[1] == SCE_USBD_DESCRIPTOR_CONFIGURATION) {
			configuration = p;
			alternates = 0;
		} else if (p[1] == SCE_USBD_DESCRIPTOR_INTERFACE && configuration != NULL) {
			if (p[3] == 0)
				alternates++;
			if (p == (const SceUInt8 *)chosen) {
				*configurationValue = configuration[0] > 5 ? configuration[5] : -1;
				*interfaceNumber = length != 2 ? p[2] : -1;
				return 1;
			}
			if (configuration[0] > 4 && configuration[4] <= alternates) {
				configuration = NULL;
				alternates = 0;
			}
		}
		p += length;
		if (p == NULL)
			return 0;
	}
	return 0;
}

/* a device appeared on a hub port: enumerate it and offer it to the drivers */
static void usbdAttachDevice(unsigned int hubId, SceUInt8 port, SceUInt8 speed)
{
	unsigned int busIndex = USBD_ID_BUS(hubId);
	unsigned int hubIndex = USBD_ID_INDEX(hubId);
	UsbdBus *bus = s_usbd.bus[busIndex];
	SceUsbdDeviceDescriptor *descriptor;
	UsbdDevice *device, *parent;
	SceUsbdInterfaceDescriptor *chosen;
	UsbdRequest *request;
	UsbdDriver *driver;
	SceUInt8 *config, *limit, *p;
	SceUInt8 newSpeed = speed;
	UsbdHcd *hcd;
	UsbdPipe *pipe;
	unsigned int length, totalLength = 0, index, count, deviceId, kind;
	int configured, configurationValue, interfaceNumber, total, i;

	if (bus->rootHub->index == hubIndex) {
		bus->rootHub->speed = speed;
		hcd = speed == SCE_USBD_DEVICE_SPEED_HS ? &bus->ehci : &bus->ohci;
		parent = bus->rootHub;
	} else {
		hcd = bus->rootHub->speed == SCE_USBD_DEVICE_SPEED_HS ? &bus->ehci : &bus->ohci;
		parent = bus->device[hubIndex];
		if (parent != bus->rootHub) {
			hubId = USBD_ID(busIndex, hubIndex);
			if (hubResetPort(hubId, port) < 0)
				return;
			sceKernelDelayThread(500000);
			if (!hubGetPortStatus(hubId, port, &newSpeed))
				return;
		}
	}

	device = bus->freeDeviceHead;
	if (device == NULL)
		return;
	p = usbdAlloc(USBD_DESCRIPTOR_BUFFER_SIZE);
	if (p == NULL)
		return;
	USBD_LIST_REMOVE(bus->freeDeviceHead, bus->freeDeviceTail, device);
	device->descriptor = p;
	device->parent = parent;
	device->port = port;
	device->configuration = -1;
	device->address = 0;
	device->hcd = NULL;
	device->defaultPipe = NULL;
	device->interfaceCount = 0;
	device->defaultPipeRefs = 0;
	device->vendorId = 0;
	device->productId = 0;
	device->pipeHead = NULL;
	device->pipeTail = NULL;
	device->childHead = NULL;
	device->childTail = NULL;
	device->maxPacketSize0 = USBD_DEFAULT_MAX_PACKET_SIZE;
	if (parent != NULL) {
		device->depth = parent->depth + 1;
		USBD_LIST_APPEND(parent->childHead, parent->childTail, device);
		memcpy(&device->route, &parent->route, sizeof(device->route));
		device->route.port[device->depth] = port;
	}
	device->speed = newSpeed;
	device->hcd = hcd;

	pipe = hcd->allocPipe(device, NULL);
	if (pipe == NULL)
		goto release;
	sceKernelDelayThread(100000);

	/* the first 8 bytes of the device descriptor tell the default pipe's packet size */
	request = &pipe->device->request;
	bus->enumerationCursor = bus->enumerationBuffer;
	descriptor = (SceUsbdDeviceDescriptor *)pipe->device->bus->enumerationBuffer;
	if (usbdSubmitDeviceRequest(pipe, usbdDeviceDescriptorDone, SCE_USBD_REQTYPE_DIR_TO_HOST,
				    SCE_USBD_REQUEST_GET_DESCRIPTOR,
				    SCE_USBD_DESCRIPTOR_DEVICE << 8, 8, descriptor) < 0)
		goto close;
	if (usbdWaitDeviceResponse() < 0)
		goto close;
	if (request->result == 0) {
		if (descriptor->bMaxPacketSize0 - 8 > 56 || (descriptor->bMaxPacketSize0 & 7) != 0)
			goto close;
		if (pipe->hcd->setMaxPacketSize(pipe, descriptor->bMaxPacketSize0) < 0)
			goto close;
		request->pipe->device->maxPacketSize0 = descriptor->bMaxPacketSize0;

		if (usbdSubmitDeviceRequest(pipe, usbdDeviceDescriptorDone,
					    SCE_USBD_REQTYPE_DIR_TO_HOST,
					    SCE_USBD_REQUEST_GET_DESCRIPTOR,
					    SCE_USBD_DESCRIPTOR_DEVICE << 8,
					    descriptor->bLength, descriptor) < 0)
			goto close;
		if (usbdWaitDeviceResponse() < 0)
			goto close;
		if (descriptor->bMaxPacketSize0 - 8 > 56 || (descriptor->bMaxPacketSize0 & 7) != 0 ||
		    descriptor->bLength != sizeof(SceUsbdDeviceDescriptor))
			goto close;
		if (usbdIsSingleConfigurationQuirk(descriptor) &&
		    descriptor->bNumConfigurations > 1)
			descriptor->bNumConfigurations = 1;
		/* reported as the first DUALSHOCK 4 */
		if (descriptor->idVendor == USBD_VENDOR_SONY &&
		    descriptor->idProduct == USBD_PRODUCT_SONY_09CC)
			descriptor->idProduct = 0x05c4;
		if (descriptor->bNumConfigurations > 1)
			goto close;
	}
	if (request->result < 0)
		goto close;

	p = bus->enumerationBuffer;
	device->vendorId = p[8] | p[9] << 8;
	device->productId = p[10] | p[11] << 8;
	request = &pipe->device->request;
	if (usbdSubmitDeviceRequest(pipe, usbdSetAddressDone, SCE_USBD_REQTYPE_DIR_TO_DEVICE,
				    SCE_USBD_REQUEST_SET_ADDRESS, pipe->device->index, 0, NULL) < 0)
		goto close;
	if (usbdWaitDeviceResponse() < 0)
		goto close;
	if (request->result < 0)
		goto close;
	device->address = device->index;
	hcd->setAddress(pipe, device->index);
	sceKernelDelayThread(20000);

	/* each configuration: its first 4 bytes for wTotalLength, then all of it */
	bus->enumerationCursor += sizeof(SceUsbdDeviceDescriptor);
	device->configurationIndex = 0;
	request = &pipe->device->request;
	descriptor = (SceUsbdDeviceDescriptor *)pipe->device->bus->enumerationBuffer;
	config = pipe->device->bus->enumerationCursor;
	limit = (SceUInt8 *)descriptor + USBD_DESCRIPTOR_BUFFER_SIZE;
	for (;;) {
		index = pipe->device->configurationIndex++;
		if ((index >> 1) != 0 && (index & 1) == 0) {
			pipe->device->bus->enumerationCursor +=
				((SceUsbdConfigurationDescriptor *)config)->wTotalLength;
			if (usbdIsSingleConfigurationQuirk(descriptor))
				break;
			length = 4;
		} else if ((index & 1) == 0) {
			length = 4;
		} else {
			length = ((SceUsbdConfigurationDescriptor *)config)->wTotalLength;
		}
		if (config + length >= limit)
			goto close;
		if ((index >> 1) >= descriptor->bNumConfigurations)
			break;
		if (usbdSubmitDeviceRequest(pipe, usbdConfigurationDescriptorDone,
					    SCE_USBD_REQTYPE_DIR_TO_HOST,
					    SCE_USBD_REQUEST_GET_DESCRIPTOR,
					    SCE_USBD_DESCRIPTOR_CONFIGURATION << 8 | index >> 1,
					    length, config) < 0)
			goto close;
		if (usbdWaitDeviceResponse() < 0)
			goto close;
		if (request->result != 0) {
			if (request->result < 0)
				goto close;
			break;
		}
		if (request->count < length)
			goto close;
		if (length == 4)
			totalLength = ((SceUsbdConfigurationDescriptor *)config)->wTotalLength;
		else if (((SceUsbdConfigurationDescriptor *)config)->wTotalLength != totalLength)
			goto close;
	}

	total = usbdSanityCheck(device, bus);
	if (total < 0)
		goto close;
	p = device->descriptor;
	device->descriptorEnd = p + total + sizeof(SceUsbdDeviceDescriptor);
	length = p[0];
	if (length <= 1 || p >= device->descriptorEnd)
		goto close;
	count = 0;
	do {
		if (p[1] == SCE_USBD_DESCRIPTOR_CONFIGURATION && length > 4 && p[4] >= count)
			count = p[4];
		p += length;
		length = p[0];
	} while (length > 1 && p < device->descriptorEnd);
	if (count - 1 >= USBD_INTERFACE_COUNT)
		goto close;
	device->interfaceCount = count;
	for (i = 0; i < count; i++)
		device->driver[i] = NULL;
	bus->device[device->address] = device;
	hcd->closePipe(pipe);

	if (device->bus->suspended)
		return;
	usbdLock();
	configured = 0;
	deviceId = USBD_ID(busIndex, device->address);
	for (i = 0; i < USBD_DRIVER_COUNT; i++) {
		driver = s_usbd.driver[i];
		if (driver == NULL)
			continue;
		if (!(driver->flags & USBD_DRIVER_KERNEL)) {
			UsbdDriver *attached = usbdGetAttachedDriver(device);
			UsbdClientEvent *event;
			UsbdClient *client;

			if (attached == NULL)
				kind = 0;
			else if (attached->flags & USBD_DRIVER_COMPOSITE)
				kind = 2;
			else
				continue;
			for (client = s_usbd.clientHead; client != NULL; client = client->next) {
				if (client->closing)
					continue;
				event = usbdAlloc(sizeof(*event));
				if (event == NULL)
					continue;
				event->type = USBD_CLIENT_EVENT_ATTACH;
				event->bus = busIndex;
				event->driverKind = kind;
				event->address = device->address;
				USBD_LIST_APPEND(client->eventHead, client->eventTail, event);
				sceKernelSignalSema(client->semaId, 1);
			}
			continue;
		}
		if (!(driver->flags & USBD_DRIVER_COMPOSITE)) {
			if (driver->driver.probe == NULL)
				continue;
			usbdUnlock();
			if (driver->driver.probe(deviceId) != 0)
				goto relock;
			usbdLock();
			device->configuration = 0;
			driver->configuration = 0;
			driver->interfaceNumber = 0;
			device->driver[0] = driver;
			if (driver->driver.attach == NULL)
				continue;
			usbdUnlock();
			if (driver->driver.attach(deviceId) == 0) {
				usbdLock();
				goto attached;
			}
			goto relock;
		}
		if (driver->compositeDriver.probe == NULL)
			continue;
		chosen = NULL;
		usbdUnlock();
		if (driver->compositeDriver.probe(deviceId, &chosen) != 0)
			goto relock;
		usbdLock();
		if (!usbdFindInterface(device, chosen, &configurationValue, &interfaceNumber))
			continue;
		device->configuration = configurationValue;
		driver->configuration = configurationValue;
		driver->interfaceNumber = interfaceNumber;
		device->driver[interfaceNumber] = driver;
		if (!configured) {
			usbdUnlock();
			if (usbdSetConfiguration(device, configurationValue) < 0)
				goto relock;
			usbdLock();
			configured = 1;
		}
		if (driver->compositeDriver.attach == NULL)
			continue;
		usbdUnlock();
		driver->compositeDriver.attach(deviceId);
relock:
		usbdLock();
	}
attached:
	if (usbdHasNoDriver(device) && s_usbd.callbackId != -1 && (s_usbd.callbackFlags & 1))
		sceKernelSetEventWithNotifyCallback(s_usbd.eventId, 1, 0, 1);
	usbdUnlock();
	return;

close:
	hcd->closePipe(pipe);
release:
	usbdReleaseDevice(bus, device);
}

/* a device left a hub port */
static void usbdDetachDevice(unsigned int hubId, SceUInt8 port)
{
	unsigned int busIndex = USBD_ID_BUS(hubId);
	UsbdBus *bus = s_usbd.bus[busIndex];
	UsbdDevice *hub, *device;
	UsbdDriver *driver;
	int i;

	usbdLock();
	hub = USBD_ID_INDEX(hubId) != 0 ? bus->device[USBD_ID_INDEX(hubId)] : bus->rootHub;
	for (device = hub->childHead; device != NULL; device = device->next) {
		if (device->port == port)
			break;
	}
	if (device == NULL)
		goto out;
	usbdDetachChildren(&device->childHead);
	while (device->pipeHead != NULL)
		device->hcd->closePipe(device->pipeHead);
	for (i = 0; i < device->interfaceCount; i++) {
		driver = device->driver[i];
		if (driver == NULL)
			continue;
		if (!(driver->flags & USBD_DRIVER_KERNEL)) {
			UsbdClientEvent *event;
			UsbdClient *client;

			for (client = s_usbd.clientHead; client != NULL; client = client->next) {
				if (client->closing)
					continue;
				event = usbdAlloc(sizeof(*event));
				if (event == NULL)
					continue;
				event->type = USBD_CLIENT_EVENT_DETACH;
				event->bus = busIndex;
				event->address = device->address;
				USBD_LIST_APPEND(client->eventHead, client->eventTail, event);
				usbdUnlock();
				sceKernelSignalSema(client->semaId, 1);
				usbdLock();
			}
			continue;
		}
		usbdUnlock();
		if (driver->flags & USBD_DRIVER_COMPOSITE)
			driver->compositeDriver.detach(USBD_ID(busIndex, device->address));
		else
			driver->driver.detach(USBD_ID(busIndex, device->address));
		usbdLock();
	}
	bus->device[device->address] = NULL;
	usbdReleaseDevice(bus, device);
out:
	usbdUnlock();
}

/* "SceUsbdEvent" */
static int usbdEventThread(SceSize args, void *argp)
{
	UsbdEvent *queued, event;

	for (;;) {
		if (sceKernelWaitSema(s_usbd.deviceEventSemaId, 1, NULL) < 0)
			continue;
		queued = usbdDequeueEvent(&s_usbd.deviceEvents);
		if (queued == NULL)
			continue;
		event = *queued;
		usbdFree(queued);
		if (event.type == USBD_EVENT_ATTACH)
			usbdAttachDevice(event.arg0, event.arg1, event.arg2);
		else if (event.type == USBD_EVENT_DETACH)
			usbdDetachDevice(event.arg0, event.arg1);
		else if (event.type == USBD_EVENT_QUIT)
			break;
	}
	return sceKernelExitThread(1);
}

/* a pipe for the endpoint, NULL: the default pipe; called by the controllers, locked */
UsbdPipe *usbdAllocPipe(UsbdDevice *device, const SceUsbdEndpointDescriptor *endpoint)
{
	UsbdBus *bus = device->bus;
	UsbdPipe *pipe = bus->freePipeHead;

	if (pipe == NULL)
		return NULL;
	USBD_LIST_REMOVE(bus->freePipeHead, bus->freePipeTail, pipe);
	bus->pipe[pipe->index] = pipe;
	pipe->device = device;
	pipe->waiting = 0;
	pipe->busy = 0;
	pipe->requestHead = NULL;
	pipe->requestTail = NULL;
	pipe->descHead.ehci = NULL;
	pipe->descTail.ehci = NULL;
	pipe->frame = -1;
	if (endpoint != NULL) {
		pipe->type = endpoint->bmAttributes & SCE_USBD_ENDPOINT_TRANSFER_TYPE_BITS;
		pipe->direction = endpoint->bEndpointAddress & SCE_USBD_ENDPOINT_DIRECTION_BITS;
		pipe->maxPacketSize = endpoint->wMaxPacketSize & 0x7ff;
	} else {
		pipe->type = SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL;
	}
	USBD_LIST_APPEND(device->pipeHead, device->pipeTail, pipe);
	return pipe;
}

void usbdFreePipe(UsbdPipe *pipe)
{
	UsbdDevice *device;
	UsbdPipe *entry;
	UsbdBus *bus;

	if (pipe == NULL)
		return;
	device = pipe->device;
	bus = device->bus;
	for (entry = bus->freePipeHead; entry != NULL; entry = entry->next) {
		if (entry == pipe)
			return;
	}
	bus->pipe[pipe->index] = NULL;
	USBD_LIST_REMOVE(device->pipeHead, device->pipeTail, pipe);
	pipe->device = NULL;
	USBD_LIST_APPEND(bus->freePipeHead, bus->freePipeTail, pipe);
}

/* back to the free list; called locked */
void usbdFreeRequest(UsbdRequest *request)
{
	UsbdRequest *entry;

	if (request == NULL || request->isDeviceRequest)
		return;
	for (entry = s_usbd.freeRequestHead; entry != NULL; entry = entry->next) {
		if (entry == request)
			return;
	}
	if (request->poolBuffer != NULL) {
		usbdFreeBuffer(request->poolBuffer);
		request->poolBuffer = NULL;
	}
	s_usbd.request[request->id] = NULL;
	USBD_LIST_APPEND(s_usbd.freeRequestHead, s_usbd.freeRequestTail, request);
}

static __always_inline void usbdDiscardRequest(UsbdRequest *request)
{
	usbdLock();
	usbdFreeRequest(request);
	usbdUnlock();
}

/* "SceUsbdCallback": a finished transfer's callback, or its user client's event */
static int usbdCallbackThread(SceSize args, void *argp)
{
	UsbdIsochRequest isochStatus;
	UsbdEvent *queued, event;
	UsbdClientEvent *clientEvent;
	UsbdRequest *request;
	UsbdClient *client;
	UsbdPipe *pipe;

	for (;;) {
		if (sceKernelWaitSema(s_usbd.callbackSemaId, 1, NULL) < 0)
			continue;
		queued = usbdDequeueEvent(&s_usbd.callbackEvents);
		if (queued == NULL)
			continue;
		event = *queued;
		usbdFree(queued);
		if (event.type == USBD_EVENT_QUIT)
			break;
		if (event.type != USBD_EVENT_COMPLETE)
			continue;

		request = (UsbdRequest *)event.arg0;
		usbdLock();
		if (request == NULL)
			goto unlock;
		pipe = request->pipe;
		if (pipe == NULL || pipe->device == NULL) {
			usbdDiscardRequest(request);
			goto unlock;
		}
		client = request->client;
		if (client != NULL) {
			clientEvent = usbdAlloc(sizeof(*clientEvent));
			if (clientEvent == NULL) {
				usbdDiscardRequest(request);
				goto unlock;
			}
			if (client->closing) {
				usbdFree(clientEvent);
				usbdDiscardRequest(request);
				goto unlock;
			}
			clientEvent->type = USBD_CLIENT_EVENT_COMPLETE;
			clientEvent->address = pipe->device->address;
			clientEvent->bus = pipe->device->bus->index;
			clientEvent->requestId = request->id;
			if (pipe->direction == SCE_USBD_ENDPOINT_DIRECTION_IN &&
			    request->poolBuffer != NULL && request->length != 0 &&
			    request->userBuffer != NULL &&
			    sceKernelCopyToUserProc(request->client->pid, request->userBuffer,
						     request->poolBuffer, request->length) < 0) {
				usbdFree(clientEvent);
				usbdDiscardRequest(request);
				goto unlock;
			}
			USBD_LIST_APPEND(client->eventHead, client->eventTail, clientEvent);
			sceKernelSignalSema(client->semaId, 1);
			goto unlock;
		}
		if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS) {
			void (*isochDone)(int, UsbdIsochRequest *, void *) = request->isochDone;
			int result = request->result;
			void *arg = request->arg;

			isochStatus = request->isochStatus;
			usbdFreeRequest(request);
			usbdUnlock();
			if (isochDone != NULL)
				isochDone(result, &isochStatus, arg);
		} else {
			SceUsbdDoneCallback done = request->done;
			int result = request->result;
			int count = request->count;
			void *arg = request->arg;

			usbdFreeRequest(request);
			usbdUnlock();
			if (done != NULL)
				done(result, count, arg);
		}
		usbdLock();
unlock:
		usbdUnlock();
	}
	sceKernelExitThread(1);
	return 0;
}

/* the controllers' root ports: hubId is the root hub's, bus << 16 */
void usbdNotifyAttach(unsigned int hubId, unsigned int port, unsigned int speed)
{
	UsbdEvent *event = usbdAlloc(sizeof(*event));

	if (event == NULL)
		return;
	event->arg0 = hubId;
	event->arg1 = port;
	event->type = USBD_EVENT_ATTACH;
	event->arg2 = speed;
	usbdQueueEvent(&s_usbd.deviceEvents, event);
	sceKernelSignalSema(s_usbd.deviceEventSemaId, 1);
}

void usbdNotifyDetach(unsigned int hubId, unsigned int port)
{
	UsbdEvent *event = usbdAlloc(sizeof(*event));

	if (event == NULL)
		return;
	event->arg0 = hubId;
	event->arg1 = port;
	event->arg2 = 0;
	event->type = USBD_EVENT_DETACH;
	usbdQueueEvent(&s_usbd.deviceEvents, event);
	sceKernelSignalSema(s_usbd.deviceEventSemaId, 1);
}

/* the free list in index order, each entry's index set */
#define USBD_INIT_FREE_LIST(head, tail, array, count, table) do {	\
	(head) = NULL;							\
	(tail) = NULL;							\
	for (i = 0; i < (count); i++) {					\
		(array)[i].index = i;					\
		USBD_LIST_APPEND(head, tail, &(array)[i]);		\
		(table)[i] = NULL;					\
	}								\
} while (0)

static __always_inline void usbdQuitThread(UsbdEventQueue *queue, SceUID semaId, SceUID threadId)
{
	UsbdEvent *event = usbdAlloc(sizeof(*event));

	if (event == NULL)
		return;
	event->type = USBD_EVENT_QUIT;
	usbdQueueEvent(queue, event);
	sceKernelSignalSema(semaId, 1);
	sceKernelWaitThreadEnd(threadId, NULL, NULL);
}

int _sceUsbdInit(void)
{
	UsbdDevice *device, *rootHub;
	UsbdRequest *request;
	SceUInt8 *descriptor;
	UsbdPool *pool;
	UsbdBus *bus;
	SceUInt8 *buffer;
	void *base;
	int result, b, i;

	if (s_usbd.initialized)
		return SCE_USBD_ERROR_ALREADY_INITIALIZED;
	s_usbd.queueLock = 0;
	s_usbd.eventId = sceKernelCreateSimpleEvent("SceUsbd", 0, 0, NULL);
	s_usbd.callbackFlags = 0;
	s_usbd.callbackId = -1;
	s_usbd.callbackPid = -1;
	result = usbdCreateHeap();
	if (result < 0)
		return result;

	pool = usbdAlloc(sizeof(*pool));
	if (pool == NULL)
		goto no_memory;
	for (b = 0; b < USBD_BUS_COUNT; b++) {
		bus = &pool->bus[b];
		bus->freeDeviceHead = NULL;
		bus->freeDeviceTail = NULL;
		bus->index = b;
		bus->ehci.bus = bus;
		bus->ohci.bus = bus;
		bus->reserved.bus = bus;
		s_usbd.bus[b] = bus;
		for (i = 0; i < USBD_DEVICES_PER_BUS; i++) {
			device = &pool->device[b][i];
			device->index = i;
			device->bus = bus;
			device->request.setup = usbdAlloc(sizeof(SceUsbdDeviceRequest));
			if (device->request.setup == NULL)
				goto no_memory;
			device->request.isDeviceRequest = 1;
			USBD_LIST_APPEND(bus->freeDeviceHead, bus->freeDeviceTail, device);
			bus->device[i] = NULL;
		}
		USBD_INIT_FREE_LIST(bus->freePipeHead, bus->freePipeTail, pool->pipe[b],
				    USBD_PIPES_PER_BUS, bus->pipe);

		rootHub = bus->freeDeviceHead;
		if (rootHub != NULL && (descriptor = usbdAlloc(USBD_DESCRIPTOR_BUFFER_SIZE)) != NULL) {
			USBD_LIST_REMOVE(bus->freeDeviceHead, bus->freeDeviceTail, rootHub);
			rootHub->configuration = -1;
			rootHub->address = 0;
			rootHub->hcd = NULL;
			rootHub->defaultPipe = NULL;
			rootHub->descriptor = descriptor;
			rootHub->parent = NULL;
			rootHub->interfaceCount = 0;
			rootHub->port = 0;
			rootHub->defaultPipeRefs = 0;
			rootHub->maxPacketSize0 = USBD_DEFAULT_MAX_PACKET_SIZE;
			rootHub->vendorId = 0;
			rootHub->productId = 0;
			rootHub->pipeHead = NULL;
			rootHub->pipeTail = NULL;
			rootHub->childHead = NULL;
			rootHub->childTail = NULL;
		} else {
			rootHub = NULL;
		}
		bus->rootHub = rootHub;
		rootHub->depth = 0;
		bus->enumerationBuffer = sceKernelAllocUncacheHeapMemory(USBD_DESCRIPTOR_BUFFER_SIZE);
		if (bus->enumerationBuffer == NULL)
			goto no_memory;
	}

	s_usbd.freeRequestHead = NULL;
	s_usbd.freeRequestTail = NULL;
	for (i = 0; i < USBD_REQUEST_COUNT; i++) {
		request = &pool->request[0][i];
		request->id = i;
		request->state = 0;
		request->isDeviceRequest = 0;
		request->setup = usbdAlloc(sizeof(SceUsbdDeviceRequest));
		if (request->setup == NULL)
			goto no_memory;
		USBD_LIST_APPEND(s_usbd.freeRequestHead, s_usbd.freeRequestTail, request);
		s_usbd.request[i] = NULL;
	}
	USBD_INIT_FREE_LIST(s_usbd.freeDriverHead, s_usbd.freeDriverTail, pool->driver,
			    USBD_DRIVER_COUNT, s_usbd.driver);

	/* user transfers are staged in these */
	result = usbdAllocMemBlock(USBD_SMALL_BUFFER_COUNT * USBD_SMALL_BUFFER_SIZE +
				   USBD_LARGE_BUFFER_COUNT * USBD_LARGE_BUFFER_SIZE, &base);
	if (result < 0)
		goto delete_heap;
	s_usbd.bufferMemBlockId = result;
	buffer = base;
	s_usbd.smallBuffers.head = (void **)buffer;
	s_usbd.smallBuffers.base = buffer;
	for (i = 0; i < USBD_SMALL_BUFFER_COUNT - 1; i++, buffer += USBD_SMALL_BUFFER_SIZE)
		*(void **)buffer = buffer + USBD_SMALL_BUFFER_SIZE;
	*(void **)buffer = NULL;
	s_usbd.smallBuffers.tail = (void **)buffer;
	s_usbd.smallBuffers.last = buffer;
	buffer += USBD_SMALL_BUFFER_SIZE;
	s_usbd.largeBuffers.head = (void **)buffer;
	s_usbd.largeBuffers.base = buffer;
	s_usbd.smallBufferCount = USBD_SMALL_BUFFER_COUNT;
	for (i = 0; i < USBD_LARGE_BUFFER_COUNT - 1; i++, buffer += USBD_LARGE_BUFFER_SIZE)
		*(void **)buffer = buffer + USBD_LARGE_BUFFER_SIZE;
	*(void **)buffer = NULL;
	s_usbd.largeBuffers.tail = (void **)buffer;
	s_usbd.largeBuffers.last = buffer;
	s_usbd.largeBufferCount = USBD_LARGE_BUFFER_COUNT;

	s_usbd.deviceEvents.head = NULL;
	s_usbd.deviceEvents.tail = NULL;
	s_usbd.callbackEvents.head = NULL;
	s_usbd.callbackEvents.tail = NULL;
	s_usbd.clientHead = NULL;
	s_usbd.clientTail = NULL;
	result = sceKernelInitializeFastMutex(&s_usbd.mutex, "SceUsbd", 0x2002, NULL);
	if (result < 0)
		goto delete_heap;
	result = sceKernelCreateSema("SceUsbdDeviceEvent", 0, 0, 20, NULL);
	if (result < 0)
		goto finalize_mutex;
	s_usbd.deviceEventSemaId = result;
	result = sceKernelCreateSema("SceUsbdCallback", 0, 0, 20, NULL);
	if (result < 0)
		goto delete_device_event_sema;
	s_usbd.callbackSemaId = result;
	result = sceKernelCreateSema("SceUsbdEnumeration", 0, 0, 1, NULL);
	if (result < 0)
		goto delete_callback_sema;
	s_usbd.enumerationSemaId = result;
	result = sceKernelCreateThread("SceUsbdEvent", usbdEventThread, 60, 0x2000, 0,
					0x80000, NULL);
	if (result < 0)
		goto delete_enumeration_sema;
	s_usbd.eventThreadId = result;
	result = sceKernelCreateThread("SceUsbdCallback", usbdCallbackThread, 60, 0x2000, 0,
					0x80000, NULL);
	if (result < 0)
		goto delete_event_thread;
	s_usbd.callbackThreadId = result;
	result = sceKernelStartThread(s_usbd.eventThreadId, 0, NULL);
	if (result < 0)
		goto delete_callback_thread;
	result = sceKernelStartThread(s_usbd.callbackThreadId, 0, NULL);
	if (result < 0)
		goto quit_event_thread;

	result = ehciInit(s_usbd.bus[0]);
	if (result < 0)
		goto quit_callback_thread;
	result = ohciInit(s_usbd.bus[0]);
	if (result < 0)
		goto term_ehci0;
	result = ehciInit(s_usbd.bus[1]);
	if (result < 0)
		goto term_ohci0;
	result = ohciInit(s_usbd.bus[1]);
	if (result < 0)
		goto term_ehci1;
	result = ehciInit(s_usbd.bus[2]);
	if (result < 0)
		goto term_ohci1;
	result = ohciInit(s_usbd.bus[2]);
	if (result < 0)
		goto term_ehci2;
	result = sceKernelRegisterProcEventHandler("SceUsbd", &usbdProcEventHandler, NULL);
	if (result < 0)
		goto term_ohci2;
	s_usbd.procEventHandlerId = result;
	s_usbd.initialized = 1;
	result = hubInit();
	if (result >= 0)
		return 0;

	s_usbd.initialized = 0;
	if (sceKernelUnregisterProcEventHandler(s_usbd.procEventHandlerId) < 0)
		SCE_DBG_ASSERT(0, 2104);
term_ohci2:
	if (ohciTerm(s_usbd.bus[2]) < 0)
		SCE_DBG_ASSERT(0, 2108);
term_ehci2:
	if (ehciTerm(s_usbd.bus[2]) < 0)
		SCE_DBG_ASSERT(0, 2111);
term_ohci1:
	if (ohciTerm(s_usbd.bus[1]) < 0)
		SCE_DBG_ASSERT(0, 2114);
term_ehci1:
	if (ehciTerm(s_usbd.bus[1]) < 0)
		SCE_DBG_ASSERT(0, 2117);
term_ohci0:
	ohciTerm(s_usbd.bus[0]);
term_ehci0:
	ehciTerm(s_usbd.bus[0]);
quit_callback_thread:
	usbdQuitThread(&s_usbd.callbackEvents, s_usbd.callbackSemaId, s_usbd.callbackThreadId);
quit_event_thread:
	usbdQuitThread(&s_usbd.deviceEvents, s_usbd.deviceEventSemaId, s_usbd.eventThreadId);
delete_callback_thread:
	if (sceKernelDeleteThread(s_usbd.callbackThreadId) < 0)
		SCE_DBG_ASSERT(0, 2128);
delete_event_thread:
	if (sceKernelDeleteThread(s_usbd.eventThreadId) < 0)
		SCE_DBG_ASSERT(0, 2131);
delete_enumeration_sema:
	if (sceKernelDeleteSema(s_usbd.enumerationSemaId) < 0)
		SCE_DBG_ASSERT(0, 2134);
delete_callback_sema:
	if (sceKernelDeleteSema(s_usbd.callbackSemaId) < 0)
		SCE_DBG_ASSERT(0, 2137);
delete_device_event_sema:
	if (sceKernelDeleteSema(s_usbd.deviceEventSemaId) < 0)
		SCE_DBG_ASSERT(0, 2140);
finalize_mutex:
	if (sceKernelFinalizeFastMutex(&s_usbd.mutex) < 0)
		SCE_DBG_ASSERT(0, 2143);
	goto delete_heap;

no_memory:
	result = SCE_USBD_ERROR_NO_MEMORY;
delete_heap:
	if (usbdDeleteHeap() < 0)
		SCE_DBG_ASSERT(0, 2148);
	return result;
}

int module_start(SceSize args, void *argp)
{
	return _sceUsbdInit() < 0 ? SCE_KERNEL_START_NO_RESIDENT : SCE_KERNEL_START_SUCCESS;
}

int sceUsbdHostStart(int busIndex)
{
	UsbdBus *bus = s_usbd.bus[busIndex];
	int result;

	result = ehciStart(bus);
	if (result < 0)
		return result;
	bus->ehci.powerOnPort(bus, 0);
	result = ohciStart(bus);
	if (result < 0)
		return result;
	bus->ohci.powerOnPort(bus, 0);
	return 0;
}

int sceUsbdHostStop(int busIndex)
{
	UsbdBus *bus = s_usbd.bus[busIndex];
	int result;

	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	usbdLock();
	usbdDetachBus(bus->index);
	usbdUnlock();
	result = ehciStop(bus);
	if (result < 0)
		return result;
	result = ohciStop(bus);
	return result < 0 ? result : 0;
}

int sceUsbdSuspend(int busIndex)
{
	UsbdBus *bus = s_usbd.bus[busIndex];
	int result;

	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	result = bus->ehci.suspend(bus);
	if (result < 0)
		return result;
	result = bus->ohci.suspend(bus);
	return result < 0 ? result : 0;
}

int sceUsbdResume(int busIndex)
{
	UsbdBus *bus = s_usbd.bus[busIndex];
	int result;

	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	usbdLock();
	usbdDetachBus(busIndex);
	bus->suspended = 0;
	usbdUnlock();
	result = bus->ehci.resume(bus);
	if (result < 0)
		return result;
	bus->ehci.powerOnPort(bus, 0);
	result = bus->ohci.resume(bus);
	if (result < 0)
		return result;
	bus->ohci.powerOnPort(bus, 0);
	return 0;
}

/* before the suspend: reset the DUALSHOCK 4s, so they do not keep the bus awake */
int sceUsbdSuspendPhase2(int busIndex, int flag)
{
	UsbdBus *bus = s_usbd.bus[busIndex];
	UsbdDevice *device;
	int result, reset, address;

	if (bus == NULL)
		return SCE_USBD_ERROR_INVALID_PARAM;
	usbdLock();
	result = flag & 1;
	bus->suspended = 1;
	if (result) {
		if ((SceUInt8)busIndex < USBD_BUS_COUNT &&
		    (bus = s_usbd.bus[(SceUInt8)busIndex]) != NULL) {
			result = 0;
			for (address = 1; address < USBD_DEVICES_PER_BUS; address++) {
				device = bus->device[address];
				if (device == NULL || device->hcd->resetPort == NULL ||
				    device->vendorId != USBD_VENDOR_SONY || device->productId != 0x05c4)
					continue;
				if (device->depth == 1) {
					device->hcd->resetPort(bus, device->port);
				} else {
					reset = hubResetPort(USBD_ID(bus->index, device->parent->address),
							     device->port);
					if (result == 0)
						result = reset;
				}
				usbdDelay(10000);
			}
		} else {
			result = SCE_USBD_ERROR_INVALID_PARAM;
		}
	}
	usbdUnlock();
	return result;
}

/* every device on every bus that has no configuration yet */
#define USBD_FOR_EACH_DEVICE(bus, device, b, address)				\
	for (b = 0; b < USBD_BUS_COUNT; b++)					\
		if ((bus = s_usbd.bus[b]) != NULL && !bus->suspended)		\
			for (address = 1; address < USBD_DEVICES_PER_BUS; address++)	\
				if ((device = bus->device[address]) != NULL)

static __always_inline UsbdDriver *usbdFindDriver(const char *name)
{
	UsbdDriver *driver;
	int i;

	for (i = 0; i < USBD_DRIVER_COUNT; i++) {
		driver = s_usbd.driver[i];
		if (driver != NULL && strncmp(name, driver->name, 255) == 0)
			return driver;
	}
	return NULL;
}

static __always_inline UsbdDriver *usbdAllocDriver(void)
{
	UsbdDriver *driver = s_usbd.freeDriverHead;

	if (driver == NULL)
		return NULL;
	USBD_LIST_REMOVE(s_usbd.freeDriverHead, s_usbd.freeDriverTail, driver);
	s_usbd.driver[driver->index] = driver;
	return driver;
}

static __always_inline void usbdReleaseDriver(UsbdDriver *driver)
{
	UsbdDriver *entry;

	for (entry = s_usbd.freeDriverHead; entry != NULL; entry = entry->next) {
		if (entry == driver)
			return;
	}
	s_usbd.driver[driver->index] = NULL;
	USBD_LIST_APPEND(s_usbd.freeDriverHead, s_usbd.freeDriverTail, driver);
}

int SceUsbdForDriver_1EC94F18(const SceUsbdDriver *driver)
{
	UsbdDriver *slot;
	UsbdDevice *device;
	UsbdBus *bus;
	unsigned int deviceId;
	int result, b, address;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (driver == NULL || driver->name == NULL || driver->probe == NULL ||
	    driver->attach == NULL || driver->detach == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (usbdFindDriver(driver->name) != NULL) {
		result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
		goto out;
	}
	slot = usbdAllocDriver();
	if (slot == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	slot->flags = USBD_DRIVER_KERNEL;
	slot->name = driver->name;
	slot->driver = *driver;
	memset(&slot->compositeDriver, 0, sizeof(slot->compositeDriver));

	USBD_FOR_EACH_DEVICE(bus, device, b, address) {
		if (device->configuration >= 0)
			continue;
		deviceId = USBD_ID(bus->index, device->address);
		usbdUnlock();
		if (driver->probe(deviceId) == 0) {
			usbdLock();
			device->configuration = 0;
			slot->configuration = 0;
			slot->interfaceNumber = 0;
			device->driver[0] = slot;
			usbdUnlock();
			driver->attach(deviceId);
		}
		usbdLock();
	}
	result = 0;
out:
	usbdUnlock();
	return result;
}

int sceUsbdRegisterCompositeLdd(const SceUsbdCompositeDriver *driver)
{
	SceUsbdInterfaceDescriptor *chosen;
	UsbdDriver *slot;
	UsbdDevice *device;
	UsbdBus *bus;
	unsigned int deviceId;
	int configurationValue, interfaceNumber;
	int result, b, address, configured;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	chosen = NULL;
	if (driver == NULL || driver->name == NULL || driver->probe == NULL ||
	    driver->attach == NULL || driver->detach == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (usbdFindDriver(driver->name) != NULL) {
		result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
		goto out;
	}
	slot = usbdAllocDriver();
	if (slot == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	configured = 0;
	slot->flags = USBD_DRIVER_KERNEL | USBD_DRIVER_COMPOSITE;
	slot->name = driver->name;
	memset(&slot->driver, 0, sizeof(slot->driver));
	slot->compositeDriver = *driver;

	USBD_FOR_EACH_DEVICE(bus, device, b, address) {
		if (device->driver[0] != NULL && (device->driver[0]->flags & USBD_DRIVER_COMPOSITE))
			continue;
		deviceId = USBD_ID(bus->index, device->address);
		usbdUnlock();
		if (driver->probe(deviceId, &chosen) != 0) {
			usbdLock();
			continue;
		}
		usbdLock();
		if (!usbdFindInterface(device, chosen, &configurationValue, &interfaceNumber))
			continue;
		if (usbdGetAttachedDriver(device) != NULL)
			configured = 1;
		device->configuration = configurationValue;
		slot->configuration = configurationValue;
		slot->interfaceNumber = interfaceNumber;
		device->driver[interfaceNumber] = slot;
		usbdUnlock();
		if (!configured && usbdSetConfiguration(device, configurationValue) < 0) {
			usbdLock();
			device->configuration = -1;
			device->driver[slot->interfaceNumber] = NULL;
			continue;
		}
		driver->attach(deviceId);
		usbdLock();
	}
	result = 0;
out:
	usbdUnlock();
	return result;
}

/* close the pipes a driver opened on a device; called locked */
static __always_inline void usbdCloseDriverPipes(UsbdDevice *device, UsbdDriver *driver,
					unsigned int interfaceNumber)
{
	UsbdPipe *pipe = device->pipeHead;

	while (pipe != NULL) {
		if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL) {
			if (device->defaultPipeRefs != 0) {
				device->defaultPipeRefs--;
				continue;
			}
			device->defaultPipe = NULL;
		} else if ((driver->flags & USBD_DRIVER_COMPOSITE) &&
			   pipe->interfaceNumber != interfaceNumber) {
			continue;
		}
		device->hcd->closePipe(pipe);
		pipe = device->pipeHead;
	}
	device->configuration = -1;
	device->driver[driver->interfaceNumber] = NULL;
}

static __always_inline int usbdUnregisterDriver(const char *const *name, unsigned int flags)
{
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	int b, address, i;

	for (b = 0; b < USBD_BUS_COUNT; b++) {
		bus = s_usbd.bus[b];
		if (bus == NULL)
			continue;
		for (address = 1; address < USBD_DEVICES_PER_BUS; address++) {
			device = bus->device[address];
			if (device == NULL)
				continue;
			for (i = 0; i < device->interfaceCount; i++) {
				driver = device->driver[i];
				if (driver != NULL && (driver->flags & flags) == flags &&
				    strncmp(driver->name, *name, 255) == 0)
					usbdCloseDriverPipes(device, driver, 0);
			}
		}
	}
	for (i = 0; i < USBD_DRIVER_COUNT; i++) {
		driver = s_usbd.driver[i];
		if (driver != NULL && strncmp(driver->name, *name, 255) == 0) {
			usbdReleaseDriver(driver);
			break;
		}
	}
	return 0;
}

int SceUsbdForDriver_216F108D(const SceUsbdDriver *driver)
{
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (driver == NULL || driver->name == NULL || driver->probe == NULL ||
	    driver->attach == NULL || driver->detach == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else
		result = usbdUnregisterDriver(&driver->name, USBD_DRIVER_KERNEL);
	usbdUnlock();
	return result;
}

int SceUsbdForDriver_7934BB4A(const SceUsbdCompositeDriver *driver)
{
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (driver == NULL || driver->name == NULL || driver->probe == NULL ||
	    driver->attach == NULL || driver->detach == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else
		result = usbdUnregisterDriver(&driver->name,
					      USBD_DRIVER_KERNEL | USBD_DRIVER_COMPOSITE);
	usbdUnlock();
	return result;
}

SceUID SceUsbdForDriver_1CDBFF9F(int deviceId, SceUsbdEndpointDescriptor *endpoint)
{
	const SceUInt8 *ep = (const SceUInt8 *)endpoint;
	const SceUInt8 *configuration, *next, *interface, *p;
	UsbdDevice *device;
	UsbdPipe *pipe;
	unsigned int n;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	if (endpoint == NULL) {
		pipe = device->defaultPipe;
		if (pipe != NULL) {
			device->defaultPipeRefs++;
		} else {
			pipe = device->hcd->allocPipe(device, NULL);
			if (pipe == NULL)
				goto invalid;
			device->defaultPipe = pipe;
		}
		goto opened;
	}

	/* the configuration, then the interface, the endpoint descriptor lies in */
	p = device->descriptor;
	configuration = p + p[0];
	if (((SceUsbdDeviceDescriptor *)p)->bNumConfigurations == 0)
		goto invalid;
	next = configuration + ((SceUsbdConfigurationDescriptor *)configuration)->wTotalLength;
	for (n = 0; next <= ep; ) {
		if (++n == ((SceUsbdDeviceDescriptor *)p)->bNumConfigurations)
			goto invalid;
		if (next + ((SceUsbdConfigurationDescriptor *)next)->wTotalLength > ep) {
			configuration = next;
			break;
		}
		next += ((SceUsbdConfigurationDescriptor *)next)->wTotalLength;
	}
	interface = p = configuration + configuration[0];
	do {
		if (p[0] <= 1 || p >= device->descriptorEnd)
			break;
		if (p[1] == SCE_USBD_DESCRIPTOR_INTERFACE)
			interface = p;
		p += p[0];
	} while (p <= ep);
	if (interface >= device->descriptorEnd) {
		if (interface == NULL || interface >= ep)
			goto invalid;
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	pipe = device->hcd->allocPipe(device, endpoint);
	if (pipe == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	if (interface >= ep || interface == NULL)
		goto invalid;
opened:
	result = USBD_ID(device->bus->index, pipe->index);
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	return result;
}

/* the default pipe is shared: only its last close closes it */
static __always_inline int usbdClosePipe(UsbdPipe *pipe)
{
	UsbdDevice *device = pipe->device;

	if (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL) {
		if (device->defaultPipeRefs != 0) {
			device->defaultPipeRefs--;
			return 0;
		}
		device->defaultPipe = NULL;
	}
	return device->hcd->closePipe(pipe);
}

int SceUsbdForDriver_F304DC5C(SceUID pipeId)
{
	UsbdPipe *pipe;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (usbdIsPipeId(pipeId) && (pipe = usbdGetPipe(pipeId)) != NULL)
		result = usbdClosePipe(pipe);
	else
		result = SCE_USBD_ERROR_INVALID_PARAM;
	usbdUnlock();
	return result;
}

void *SceUsbdForDriver_BC3EF82B(SceUID deviceId, void *start, SceUsbdDescriptorType type)
{
	SceUInt8 *p = start, *end, *found = NULL;
	UsbdDevice *device;

	if (!s_usbd.initialized)
		return NULL;
	usbdLock();
	if (!usbdIsDeviceId(deviceId))
		goto out;
	device = usbdGetDevice(deviceId);
	if (device == NULL)
		goto out;
	if (p == NULL) {
		end = device->descriptorEnd;
		p = device->descriptor;
	} else {
		if (p < device->descriptor)
			goto out;
		end = device->descriptorEnd;
		if (p >= end)
			goto out;
		p += p[0];
	}
	for (; p[0] > 1 && p < end; p += p[0]) {
		if (type == 0 || p[1] == type) {
			found = p;
			break;
		}
	}
out:
	usbdUnlock();
	return found;
}

/* fill and submit one of the API's requests */
static __always_inline int usbdSubmitRequest(UsbdPipe *pipe, UsbdRequest *request, SceSize length,
				    void *buffer, SceUsbdDoneCallback done)
{
	request->length = length;
	request->buffer = buffer;
	request->result = 0;
	request->count = 0;
	request->done = done;
	request->pipe = pipe;
	return pipe->device->hcd->submitRequest(pipe, request);
}

int SceUsbdForDriver_2E05660F(SceUID pipeId, const SceUsbdDeviceRequest *req,
			    unsigned char *buffer, SceUsbdDoneCallback cb, void *user_data)
{
	SceUsbdDeviceRequest *setup;
	UsbdRequest *request;
	UsbdPipe *pipe;
	SceSize length;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (!usbdIsPipeId(pipeId) || (pipe = usbdGetPipe(pipeId)) == NULL || req == NULL ||
	    pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL)
		goto invalid;
	if (buffer != NULL) {
		if (req->wLength != 0 && (((SceUIntPtr)buffer & 0x3f) != 0 || req->wLength > 0x1000))
			goto invalid;
	} else if (req->wLength != 0) {
		goto invalid;
	}
	request = usbdAllocRequest();
	if (request == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	setup = request->setup;
	length = req->wLength;
	request->pipe = pipe;
	request->client = NULL;
	request->poolBuffer = NULL;
	request->complete = usbdCompleteRequest;
	if (length == 0)
		buffer = NULL;
	request->syncCache = 1;
	request->arg = user_data;
	request->buffer = buffer;
	setup->bmRequestType = req->bmRequestType;
	setup->bRequest = req->bRequest;
	setup->wIndex = req->wIndex;
	setup->wValue = req->wValue;
	setup->wLength = length;
	pipe->direction = req->bmRequestType & SCE_USBD_REQTYPE_DIR_BITS;
	result = usbdSubmitRequest(pipe, request, length, buffer, cb);
	if (result < 0)
		usbdFreeRequest(request);
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	return result;
}

static __always_inline int usbdTransfer(SceUID pipeId, unsigned int type, unsigned char *buffer,
			unsigned int length, SceUsbdDoneCallback cb, void *user_data,
			int syncCache)
{
	UsbdRequest *request;
	UsbdPipe *pipe;
	int result;

	usbdLock();
	if (!usbdIsPipeId(pipeId) || (pipe = usbdGetPipe(pipeId)) == NULL || pipe->type != type)
		goto invalid;
	if ((buffer == NULL && length != 0) ||
	    (length != 0 && (((SceUIntPtr)buffer & 0x3f) != 0 || length > 0x10000)))
		goto invalid;
	request = usbdAllocRequest();
	if (request == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	request->client = NULL;
	request->arg = user_data;
	request->poolBuffer = NULL;
	request->complete = usbdCompleteRequest;
	request->syncCache = syncCache;
	result = usbdSubmitRequest(pipe, request, length, buffer, cb);
	if (result < 0)
		usbdFreeRequest(request);
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	return result;
}

int SceUsbdForDriver_235C094A(SceUID pipeId, unsigned char *buffer, unsigned int length,
			 SceUsbdDoneCallback cb, void *user_data)
{
	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	return usbdTransfer(pipeId, SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK, buffer, length, cb,
			    user_data, 1);
}

int SceUsbdForDriver_470D5289(int pipeId, unsigned char *buffer, unsigned int length,
			  SceUsbdDoneCallback cb, void *user_data)
{
	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	return usbdTransfer(pipeId, SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK, buffer, length, cb,
			    user_data, 0);
}

int SceUsbdForDriver_A0BF85B8(SceUID pipeId, unsigned char *buffer, SceSize length,
			      SceUsbdDoneCallback cb, void *user_data)
{
	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	return usbdTransfer(pipeId, SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT, buffer, length,
			    cb, user_data, 1);
}

static __always_inline SceSize usbdIsochLength(const UsbdIsochRequest *transfer)
{
	SceSize length = 0;
	int i;

	for (i = 0; i < transfer->numPackets; i++)
		length += transfer->packets[i].length;
	return length;
}

int SceUsbdForDriver_2249BF99(SceUID pipeId, UsbdIsochRequest *transfer,
				SceUsbdIsochDoneCallback cb, void *user_data)
{
	UsbdIsochRequest *isoch = transfer;
	UsbdRequest *request;
	UsbdPipe *pipe;
	SceSize length = 0;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (!usbdIsPipeId(pipeId) || (pipe = usbdGetPipe(pipeId)) == NULL ||
	    cb == NULL || isoch == NULL || isoch->buffer == NULL ||
	    isoch->numPackets == 0 || isoch->numPackets > 8)
		goto invalid;
	if (isoch->numPackets > 0) {
		length = usbdIsochLength(isoch);
		if (pipe->device->speed == SCE_USBD_DEVICE_SPEED_HS) {
			if (length > 0x6000)
				goto invalid;
		} else if (length > 0x1ff8) {
			goto invalid;
		}
	}
	if (pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS)
		goto invalid;
	request = usbdAllocRequest();
	if (request == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	request->isochDone = (void (*)(int, UsbdIsochRequest *, void *))cb;
	request->complete = usbdCompleteRequest;
	request->pipe = pipe;
	request->client = NULL;
	request->buffer = isoch->buffer;
	request->poolBuffer = NULL;
	request->length = length;
	request->arg = user_data;
	request->syncCache = 1;
	request->isochRequest = *isoch;
	request->isochStatus = *isoch;
	request->pipe = pipe;
	request->count = 0;
	request->result = 0;
	result = pipe->device->hcd->submitRequest(pipe, request);
	if (result < 0)
		usbdFreeRequest(request);
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	return result;
}

int SceUsbdForDriver_C3BECF5F(int deviceId, SceUInt32 *speed)
{
	UsbdDevice *device;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (speed == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	switch (device->speed) {
	case SCE_USBD_DEVICE_SPEED_LS:
	case SCE_USBD_DEVICE_SPEED_FS:
	case SCE_USBD_DEVICE_SPEED_HS:
		*speed = device->speed;
		result = 0;
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		break;
	}
out:
	usbdUnlock();
	return result;
}

int SceUsbdForDriver_0E73D253(SceUID deviceId, SceUInt32 *location)
{
	UsbdDevice *device;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (location == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL)
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	else {
		*location = device->bus->index;
		result = 0;
	}
	usbdUnlock();
	return result;
}

/* the controller's frame number */
int SceUsbdForDriver_05073925(SceUID deviceId, int *frame, int *mask)
{
	UsbdDevice *device;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (mask == NULL || frame == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL)
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	else
		result = device->hcd->getFrameNumber(device->bus, (unsigned int *)frame,
						     (unsigned int *)mask);
	usbdUnlock();
	return result;
}

/* restart the pipe's isochronous schedule at the next frame */
int SceUsbdForDriver_7938DAC7(SceUID pipeId)
{
	int (*resetFrame)(UsbdPipe *);
	UsbdPipe *pipe;
	int result;

	if (!s_usbd.initialized)
		return SCE_USBD_ERROR_NOT_INITIALIZED;
	usbdLock();
	if (!usbdIsPipeId(pipeId) || (pipe = usbdGetPipe(pipeId)) == NULL) {
		usbdUnlock();
		return SCE_USBD_ERROR_INVALID_PARAM;
	}
	resetFrame = pipe->device->hcd->resetFrame;
	result = resetFrame != NULL ? resetFrame(pipe) : 0;
	usbdUnlock();
	return result;
}

/*
 * SceUsbdForUser: the user LDD API. Every call names its client by the uid
 * sceUsbdInit returned (the process id).
 */

/* a device by its id, on whichever bus carries that index */
static __always_inline UsbdDevice *usbdFindDevice(unsigned int deviceId)
{
	UsbdDevice *device;
	UsbdBus *bus;
	int b, address;

	for (b = 0; b < USBD_BUS_COUNT; b++) {
		bus = s_usbd.bus[b];
		if (bus == NULL || bus->index != USBD_ID_BUS(deviceId))
			continue;
		for (address = 1; address < USBD_DEVICES_PER_BUS; address++) {
			device = bus->device[address];
			if (device != NULL && device->address == USBD_ID_INDEX(deviceId))
				return device;
		}
	}
	return NULL;
}

int sceUsbdInit(SceUID *uid)
{
	UsbdClient *client, *entry;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	client = usbdAlloc(sizeof(*client));
	if (client == NULL) {
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto out;
	}
	memset(client, 0, sizeof(*client));
	result = sceKernelGetProcessId();
	if (result < 0)
		goto out;
	client->pid = result;
	for (entry = s_usbd.clientHead; entry != NULL; entry = entry->next) {
		if (entry == client) {
			result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
			goto out;
		}
	}
	result = sceKernelCreateSema("SceUsbdSemaUser", 0, 0, 10, NULL);
	if (result < 0)
		goto out;
	client->semaId = result;
	USBD_LIST_APPEND(s_usbd.clientHead, s_usbd.clientTail, client);
	result = sceKernelCopyToUser(uid, &client->pid, sizeof(client->pid));
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdEnd(SceUID uid)
{
	UsbdClient *client, *entry;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	client = usbdFindClient(uid);
	if (client == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	for (entry = s_usbd.clientHead; entry != NULL; entry = entry->next) {
		if (entry == client)
			break;
	}
	if (entry == NULL) {
		result = SCE_USBD_ERROR_NOT_INITIALIZED;
		goto out;
	}
	result = sceKernelDeleteSema(client->semaId);
	if (result >= 0) {
		USBD_LIST_REMOVE(s_usbd.clientHead, s_usbd.clientTail, client);
		result = 0;
		usbdFree(client);
	}
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetDeviceList(SceUID uid, SceSize num, UsbdDeviceInfo *info)
{
	UsbdDeviceInfo *list;
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	SceUInt32 state;
	int result, count, b, address;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || num == 0) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (num > USBD_DEVICES_PER_BUS)
		num = USBD_DEVICES_PER_BUS;
	list = usbdAlloc(num * sizeof(*list));
	if (list == NULL) {
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto out;
	}
	count = 0;
	for (b = 0; b < USBD_BUS_COUNT; b++) {
		bus = s_usbd.bus[b];
		if (bus == NULL)
			continue;
		for (address = 1; address < USBD_DEVICES_PER_BUS && count < num; address++) {
			device = bus->device[address];
			if (device == NULL)
				continue;
			driver = usbdGetAttachedDriver(device);
			list[count].bus = bus->index;
			list[count].address = device->index;
			if (driver == NULL)
				list[count].driverKind = 0;
			else if (driver->flags & USBD_DRIVER_COMPOSITE)
				list[count].driverKind = 2;
			else
				list[count].driverKind = 1;
			count++;
		}
	}
	if (count < num)
		num = count;
	result = sceKernelCopyToUser(info, list, num * sizeof(*list));
	if (result >= 0)
		result = num;
	usbdFree(list);
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetDescriptorSize(SceUID uid, unsigned int deviceId)
{
	UsbdDevice *device;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || (device = usbdFindDevice(deviceId)) == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else
		result = device->descriptorEnd - device->descriptor;
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetDescriptor(SceUID uid, unsigned int deviceId, void *descriptor, SceSize size)
{
	UsbdDevice *device;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || descriptor == NULL ||
	    size - 1 >= USBD_DESCRIPTOR_BUFFER_SIZE ||
	    (device = usbdFindDevice(deviceId)) == NULL || device->descriptor == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else
		result = sceKernelCopyToUser(descriptor, device->descriptor, size);
	usbdUnlock();
	sceSyscallLeave(state);
	return result < 0 ? result : (int)size;
}

/* the LDD's name from user memory, into a 256-byte heap block */
static __always_inline int usbdCopyName(char **name, const char *userName)
{
	char *buffer = usbdAlloc(256);
	int result;

	if (buffer == NULL)
		return SCE_USBD_ERROR_NO_MEMORY;
	result = sceKernelStrncpyFromUser(buffer, userName, 255);
	if (result < 0) {
		usbdFree(buffer);
		return result;
	}
	buffer[255] = '\0';
	*name = buffer;
	return 0;
}

static __always_inline int usbdRegisterLdd(SceUID uid, const char *userName,
					   unsigned int flags)
{
	UsbdDriver *driver;
	SceUInt32 state;
	char *name;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	result = usbdCopyName(&name, userName);
	if (result < 0)
		goto out;
	if (usbdFindDriver(name) != NULL) {
		result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
		goto free_name;
	}
	driver = usbdAllocDriver();
	if (driver == NULL) {
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto free_name;
	}
	driver->flags = flags;
	driver->name = name;
	if (flags & USBD_DRIVER_COMPOSITE) {
		memset(&driver->driver, 0, sizeof(driver->driver));
		driver->compositeDriver.name = name;
		driver->compositeDriver.probe = NULL;
		driver->compositeDriver.attach = NULL;
		driver->compositeDriver.detach = NULL;
	} else {
		driver->driver.name = name;
		driver->driver.probe = NULL;
		driver->driver.attach = NULL;
		driver->driver.detach = NULL;
		memset(&driver->compositeDriver, 0, sizeof(driver->compositeDriver));
	}
	result = driver->index;
	goto out;
free_name:
	usbdFree(name);
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

SceUID sceUsbdRegisterLdd(SceUID uid, const char *name)
{
	return usbdRegisterLdd(uid, name, 0);
}

SceUID sceUsbdRegisterCompositeLddForUser(SceUID uid, const char *name)
{
	return usbdRegisterLdd(uid, name, USBD_DRIVER_COMPOSITE);
}

int sceUsbdUnregisterLdd(SceUID uid, const char *userName)
{
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	SceUInt32 state;
	const char *registered;
	char *name;
	int result, b, address, i;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	result = usbdCopyName(&name, userName);
	if (result < 0)
		goto out;
	for (b = 0; b < USBD_BUS_COUNT; b++) {
		bus = s_usbd.bus[b];
		if (bus == NULL)
			continue;
		for (address = 1; address < USBD_DEVICES_PER_BUS; address++) {
			device = bus->device[address];
			if (device == NULL)
				continue;
			for (i = 0; i < device->interfaceCount; i++) {
				driver = device->driver[i];
				if (driver != NULL && !(driver->flags & USBD_DRIVER_KERNEL) &&
				    strncmp(name, driver->name, 255) == 0)
					usbdCloseDriverPipes(device, driver, i);
			}
		}
	}
	for (i = 0; i < USBD_DRIVER_COUNT; i++) {
		driver = s_usbd.driver[i];
		if (driver != NULL && strncmp(driver->name, name, 255) == 0)
			break;
	}
	if (i == USBD_DRIVER_COUNT) {
		result = SCE_USBD_ERROR_NOT_INITIALIZED;
		usbdFree(name);
		goto out;
	}
	registered = driver->name;
	usbdReleaseDriver(driver);
	result = 0;
	usbdFree(name);
	usbdFree((void *)registered);
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* sceUsbdOpenPipe: the endpoint the parameters name, in the device's descriptors */
SceUID sceUsbdOpenPipe(SceUID uid, const UsbdPipeParam *userParam)
{
	const SceUInt8 *p, *configuration, *interface, *endpoint;
	UsbdPipeParam param;
	UsbdDevice *device;
	UsbdPipe *pipe;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	result = sceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (result < 0)
		goto out;
	if (!usbdIsDeviceId(param.deviceId) || (device = usbdGetDevice(param.deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	p = device->descriptor;
	if (p[0] <= 1 || p >= device->descriptorEnd) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	configuration = interface = endpoint = NULL;
	do {
		switch (p[1]) {
		case SCE_USBD_DESCRIPTOR_CONFIGURATION:
			if (p[0] > 5 && param.configurationValue == p[5])
				configuration = p;
			break;
		case SCE_USBD_DESCRIPTOR_INTERFACE:
			if (configuration != NULL && p[0] > 3 && param.interfaceNumber == p[2] &&
			    param.alternateSetting == p[3])
				interface = p;
			break;
		case SCE_USBD_DESCRIPTOR_ENDPOINT:
			if (interface != NULL && configuration != NULL && p[0] != 2 &&
			    param.transferType == (p[3] & SCE_USBD_ENDPOINT_TRANSFER_TYPE_BITS) &&
			    param.endpointAddress == p[2])
				endpoint = p;
			break;
		}
		p += p[0];
	} while (p[0] > 1 && p < device->descriptorEnd);
	if (endpoint == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	pipe = device->hcd->allocPipe(device, (const SceUsbdEndpointDescriptor *)endpoint);
	if (pipe != NULL)
		result = USBD_ID(device->bus->index, pipe->index);
	else
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdClosePipe(SceUID uid, SceUID pipeId)
{
	UsbdPipe *pipe;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || !usbdIsPipeId(pipeId) ||
	    (pipe = usbdGetPipe(pipeId)) == NULL)
		result = SCE_USBD_ERROR_INVALID_PARAM;
	else
		result = usbdClosePipe(pipe);
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

SceUID sceUsbdOpenDefaultPipe(SceUID uid, SceUID deviceId)
{
	UsbdDevice *device;
	UsbdPipe *pipe;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
	} else if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	} else if (device->defaultPipe != NULL) {
		device->defaultPipeRefs++;
		result = device->defaultPipe->index;
	} else {
		pipe = device->hcd->allocPipe(device, NULL);
		if (pipe != NULL) {
			device->defaultPipe = pipe;
			result = USBD_ID(device->bus->index, pipe->index);
		} else {
			result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		}
	}
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* wait for the client's next event; a wakeup event only ends the wait */
int sceUsbdReceiveEvent(SceUID uid, UsbdClientEvent *userEvent)
{
	UsbdClientEvent *event;
	UsbdClient *client;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	client = usbdFindClient(uid);
	if (client == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	usbdUnlock();
	result = sceKernelWaitSema(client->semaId, 1, NULL);
	usbdLock();
	if (result < 0)
		goto out;
	event = client->eventHead;
	if (event == NULL) {
		result = SCE_USBD_ERROR_FATAL;
		goto out;
	}
	USBD_LIST_REMOVE(client->eventHead, client->eventTail, event);
	if (event->type != USBD_CLIENT_EVENT_WAKEUP) {
		result = sceKernelCopyToUser(userEvent, event, sizeof(*event));
		usbdFree(event);
	}
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* attach a user LDD to a whole device */
int sceUsbdAttach(SceUID uid, unsigned int driverId, SceUInt8 busIndex, SceUInt8 address)
{
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || driverId >= USBD_DRIVER_COUNT ||
	    busIndex >= USBD_BUS_COUNT || (unsigned int)(address - 1) >= USBD_DEVICES_PER_BUS - 1 ||
	    (bus = s_usbd.bus[busIndex]) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	device = bus->device[address];
	if (device == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	} else if (device->configuration >= 0) {
		result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
	} else {
		result = 0;
		driver = s_usbd.driver[driverId];
		device->configuration = 0;
		driver->configuration = 0;
		driver->interfaceNumber = 0;
		device->driver[0] = driver;
	}
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* attach a user composite LDD to one interface, configuring the device */
int sceUsbdAttachCompositeLdd(SceUID uid, const UsbdAttachCompositeParam *userParam)
{
	UsbdAttachCompositeParam param;
	UsbdDriver *driver;
	UsbdDevice *device;
	UsbdBus *bus;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	result = sceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (result < 0)
		goto out;
	if (param.driverId >= USBD_DRIVER_COUNT || param.bus >= USBD_BUS_COUNT ||
	    (unsigned int)(param.address - 1) >= USBD_DEVICES_PER_BUS - 1 || (bus = s_usbd.bus[param.bus]) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	device = bus->device[param.address];
	if (device == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	if (param.interfaceNumber < 0 || param.interfaceNumber >= device->interfaceCount) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (device->configuration >= 0) {
		result = SCE_USBD_ERROR_ALREADY_INITIALIZED;
		goto out;
	}
	result = usbdSetConfiguration(device, param.configuration);
	if (result >= 0) {
		driver = s_usbd.driver[param.driverId];
		result = 0;
		device->configuration = param.configuration;
		driver->configuration = param.configuration;
		driver->interfaceNumber = param.interfaceNumber;
		device->driver[param.interfaceNumber] = driver;
	}
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* a staging buffer for a user transfer: small up to 4 KiB, else large */
static __always_inline void *usbdAllocTransferBuffer(SceSize length)
{
	if (length <= USBD_SMALL_BUFFER_SIZE)
		return usbdAllocBuffer(&s_usbd.smallBuffers);
	return usbdAllocBuffer(&s_usbd.largeBuffers);
}

/* the user's data into the staging buffer, for an OUT transfer */
static __always_inline int usbdStageTransfer(UsbdPipe *pipe, void *poolBuffer,
					     const void *buffer, SceSize length)
{
	int result;

	if (pipe->direction != SCE_USBD_ENDPOINT_DIRECTION_OUT)
		return 0;
	result = sceKernelCopyFromUser(poolBuffer, buffer, length);
	if (result < 0)
		usbdFreeBuffer(poolBuffer);
	return result;
}

/* queue a user request; it completes through the callback thread */
static __always_inline int usbdSubmitUserRequest(UsbdPipe *pipe, UsbdRequest *request,
						 void *poolBuffer, void *buffer, SceSize length)
{
	int result;

	request->done = NULL;
	request->arg = NULL;
	request->result = 0;
	request->count = 0;
	request->poolBuffer = poolBuffer;
	request->userBuffer = buffer;
	request->length = length;
	request->buffer = poolBuffer;
	request->pipe = pipe;
	request->complete = usbdCompleteRequest;
	result = pipe->device->hcd->submitRequest(pipe, request);
	if (result < 0) {
		usbdFreeRequest(request);
		return result;
	}
	return request->id;
}

SceUID sceUsbdTransferData(SceUID uid, const UsbdTransferParam *userParam)
{
	SceUsbdDeviceRequest setup;
	UsbdTransferParam param;
	UsbdRequest *request;
	UsbdClient *client;
	UsbdPipe *pipe;
	SceUInt32 state;
	void *poolBuffer;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	client = usbdFindClient(uid);
	if (client == NULL)
		goto invalid;
	result = sceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (result < 0)
		goto out;
	if (param.setup != NULL && param.setupSize == sizeof(setup)) {
		result = sceKernelCopyFromUser(&setup, param.setup, sizeof(setup));
		if (result < 0)
			goto out;
	}
	if (!usbdIsPipeId(param.pipeId) || (pipe = usbdGetPipe(param.pipeId)) == NULL)
		goto invalid;

	switch (pipe->type) {
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_CONTROL:
		pipe->direction = setup.bmRequestType & SCE_USBD_REQTYPE_DIR_BITS;
		if (param.length > USBD_LARGE_BUFFER_SIZE)
			goto invalid;
		poolBuffer = NULL;
		if (param.length != 0 && param.buffer != NULL) {
			poolBuffer = usbdAllocTransferBuffer(param.length);
			if (poolBuffer == NULL)
				goto no_memory;
			result = usbdStageTransfer(pipe, poolBuffer, param.buffer, param.length);
			if (result < 0)
				goto out;
		}
		request = usbdAllocRequest();
		if (request == NULL)
			goto no_request;
		request->client = client;
		request->syncCache = 1;
		*request->setup = setup;
		break;
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK:
	case SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT:
		if (param.length > (pipe->type == SCE_USBD_ENDPOINT_TRANSFER_TYPE_BULK ?
				    USBD_LARGE_BUFFER_SIZE : USBD_SMALL_BUFFER_SIZE))
			goto invalid;
		if (param.buffer == NULL && param.length != 0)
			goto invalid;
		poolBuffer = NULL;
		if (param.buffer != NULL && param.length != 0) {
			poolBuffer = usbdAllocTransferBuffer(param.length);
			if (poolBuffer == NULL)
				goto no_memory;
			result = usbdStageTransfer(pipe, poolBuffer, param.buffer, param.length);
			if (result < 0)
				goto out;
		}
		request = usbdAllocRequest();
		if (request == NULL)
			goto no_request;
		request->client = client;
		request->syncCache = 1;
		break;
	default:
		goto invalid;
	}
	result = usbdSubmitUserRequest(pipe, request, poolBuffer, param.buffer, param.length);
	goto out;

no_request:
	if (poolBuffer != NULL)
		usbdFreeBuffer(poolBuffer);
	result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
	goto out;
no_memory:
	result = SCE_USBD_ERROR_NO_MEMORY;
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdIsochTransferData(SceUID uid, SceUID pipeId, const UsbdIsochRequest *userTransfer)
{
	UsbdIsochRequest transfer;
	UsbdRequest *request;
	UsbdClient *client;
	UsbdPipe *pipe;
	SceUInt32 state;
	void *poolBuffer;
	SceSize length;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	client = usbdFindClient(uid);
	if (client == NULL)
		goto invalid;
	result = sceKernelCopyFromUser(&transfer, userTransfer, sizeof(transfer));
	if (result < 0)
		goto out;
	if (!usbdIsPipeId(pipeId) || (pipe = usbdGetPipe(pipeId)) == NULL ||
	    pipe->type != SCE_USBD_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS ||
	    transfer.numPackets == 0 || transfer.numPackets > 8)
		goto invalid;
	length = usbdIsochLength(&transfer);
	if (transfer.buffer == NULL ? length != 0 : length > USBD_LARGE_BUFFER_SIZE)
		goto invalid;
	poolBuffer = usbdAllocTransferBuffer(length);
	if (poolBuffer == NULL) {
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto out;
	}
	result = usbdStageTransfer(pipe, poolBuffer, transfer.buffer, length);
	if (result < 0)
		goto out;
	request = usbdAllocRequest();
	if (request == NULL) {
		usbdFreeBuffer(poolBuffer);
		result = SCE_USBD_ERROR_PIPE_NOT_FOUND;
		goto out;
	}
	request->client = client;
	request->syncCache = 1;
	request->isochRequest = transfer;
	request->isochRequest.buffer = poolBuffer;
	result = usbdSubmitUserRequest(pipe, request, poolBuffer, transfer.buffer, length);
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetTransferStatus(SceUID uid, const UsbdTransferStatusParam *userParam)
{
	UsbdTransferStatusParam param;
	UsbdRequest *request;
	SceUInt32 state;
	int result, count;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL)
		goto invalid;
	result = sceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (result < 0)
		goto out;
	if (param.requestId >= USBD_REQUEST_COUNT || (request = s_usbd.request[param.requestId]) == NULL)
		goto invalid;
	result = request->result;
	count = request->count;
	usbdFreeRequest(request);
	result = sceKernelCopyToUser(param.result, &result, sizeof(result));
	if (result >= 0)
		result = sceKernelCopyToUser(param.count, &count, sizeof(count));
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetIsochTransferStatus(SceUID uid, const UsbdTransferStatusParam *userParam)
{
	UsbdTransferStatusParam param;
	UsbdIsochRequest status;
	UsbdRequest *request;
	SceUInt32 state;
	int result, requestResult;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL)
		goto invalid;
	result = sceKernelCopyFromUser(&param, userParam, sizeof(param));
	if (result < 0)
		goto out;
	if (param.requestId >= USBD_REQUEST_COUNT || (request = s_usbd.request[param.requestId]) == NULL)
		goto invalid;
	requestResult = request->result;
	status = request->isochStatus;
	usbdFreeRequest(request);
	result = sceKernelCopyToUser(param.result, &status, sizeof(status));
	if (result >= 0)
		result = sceKernelCopyToUser(param.count, &requestResult, sizeof(requestResult));
	goto out;
invalid:
	result = SCE_USBD_ERROR_INVALID_PARAM;
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetDeviceAddress(SceUID uid, SceUID deviceId, SceUsbdDeviceAddress *addr)
{
	SceUsbdDeviceAddress address;
	UsbdDevice *device;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || !usbdIsDeviceId(deviceId)) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
	} else if ((device = usbdGetDevice(deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	} else {
		address = device->route.packed;
		result = sceKernelCopyToUser(addr, &address, sizeof(address));
	}
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

int sceUsbdGetDeviceSpeed(SceUID uid, SceUID deviceId, unsigned int *speed)
{
	UsbdDevice *device;
	SceUInt32 state;
	unsigned int value;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	if (!usbdIsDeviceId(deviceId) || (device = usbdGetDevice(deviceId)) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
		goto out;
	}
	switch (device->speed) {
	case SCE_USBD_DEVICE_SPEED_LS:
	case SCE_USBD_DEVICE_SPEED_FS:
	case SCE_USBD_DEVICE_SPEED_HS:
		value = device->speed;
		result = sceKernelCopyToUser(speed, &value, sizeof(value));
		break;
	default:
		result = SCE_USBD_ERROR_FATAL;
		break;
	}
out:
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* reset the port the device is on: a root port, or its hub's */
int sceUsbdResetDevice(SceUID uid, SceUID deviceId)
{
	UsbdDevice *device;
	UsbdBus *bus;
	SceUInt32 state;
	int result;

	state = sceSyscallEnter();
	usbdLock();
	if (usbdFindClient(uid) == NULL || !usbdIsDeviceId(deviceId) ||
	    (bus = s_usbd.bus[USBD_ID_BUS(deviceId)]) == NULL) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
	} else if ((device = bus->device[USBD_ID_INDEX(deviceId)]) == NULL) {
		result = SCE_USBD_ERROR_DEVICE_NOT_FOUND;
	} else if (device->depth == 1) {
		result = device->hcd->resetPort(bus, device->port);
	} else {
		result = hubResetPort(deviceId, device->port);
	}
	usbdUnlock();
	sceSyscallLeave(state);
	return result;
}

/* one process may take the attach callback; it fires at once for a device no driver took */
int sceUsbdRegisterCallback(SceUID cbid, int flag)
{
	UsbdDevice *device;
	SceUInt32 state;
	SceUID pid, guid;
	int result, address;

	state = sceSyscallEnter();
	result = pid = sceKernelGetProcessId();
	if (result < 0)
		goto out;
	result = guid = scePUIDtoGUID(pid, cbid);
	if (result < 0)
		goto out;
	result = sceKernelGetThreadmgrUIDClass(guid);
	if (result < 0)
		goto out;
	if (result != SCE_KERNEL_THREADMGR_UID_CLASS_CALLBACK) {
		result = SCE_USBD_ERROR_INVALID_PARAM;
		goto out;
	}
	result = lockFastMutex(&s_usbd.mutex);
	if (result < 0)
		goto out;
	if (s_usbd.callbackId != -1) {
		sceKernelUnlockFastMutex(&s_usbd.mutex);
		result = SCE_USBD_ERROR_NO_MEMORY;
		goto out;
	}
	s_usbd.callbackId = guid;
	s_usbd.callbackPid = pid;
	s_usbd.callbackFlags = flag;
	result = sceKernelRegisterCallbackToEventForDriver(s_usbd.eventId, guid, guid);
	if (result >= 0) {
		if ((s_usbd.callbackFlags & 1) && s_usbd.bus[0] != NULL) {
			for (address = 0; address < USBD_DEVICES_PER_BUS; address++) {
				device = s_usbd.bus[0]->device[address];
				if (device != NULL && usbdHasNoDriver(device)) {
					sceKernelSetEventWithNotifyCallback(s_usbd.eventId, 1, 0, 1);
					break;
				}
			}
		}
		result = 0;
	}
	sceKernelUnlockFastMutex(&s_usbd.mutex);
out:
	sceSyscallLeave(state);
	return result;
}

int sceUsbdUnregisterCallback(SceUID cbid)
{
	SceUInt32 state;
	SceUID pid;
	int result;

	state = sceSyscallEnter();
	result = pid = sceKernelGetProcessId();
	if (result < 0)
		goto out;
	result = scePUIDtoGUID(pid, cbid);
	if (result < 0)
		goto out;
	result = lockFastMutex(&s_usbd.mutex);
	if (result < 0)
		goto out;
	if (pid == s_usbd.callbackPid && cbid == s_usbd.callbackId) {
		s_usbd.callbackId = -1;
		s_usbd.callbackPid = -1;
		s_usbd.callbackFlags = 0;
		sceKernelUnregisterCallbackFromEvent(s_usbd.eventId, -1);
	}
	sceKernelUnlockFastMutex(&s_usbd.mutex);
	result = 0;
out:
	sceSyscallLeave(state);
	return result;
}
