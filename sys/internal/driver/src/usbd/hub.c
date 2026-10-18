/*
 * SceUsbd: the external hub class driver.
 *
 * A hub's control requests go through one queue per hub, one at a time:
 * first SET_FEATURE(PORT_POWER) on each port in turn, then, for each status
 * change the interrupt endpoint reports, GET_STATUS of the port and
 * CLEAR_FEATURE of each change bit until none is left. A settled connection
 * change attaches or detaches the device on that port.
 */

#include <psp2kern/kernel/sysclib.h>
#include "usbd_internal.h"

#define HUB_COUNT			8
#define HUB_PORT_COUNT			15
#define HUB_BUFFER_SIZE			64

/* hub class requests and features (USB 2.0, 11.24) */
#define HUB_REQUEST_TYPE_HUB_IN		0xa0
#define HUB_REQUEST_TYPE_PORT_IN	0xa3
#define HUB_REQUEST_TYPE_PORT_OUT	0x23
#define HUB_DESCRIPTOR_TYPE		0x29

#define HUB_FEATURE_PORT_RESET		4
#define HUB_FEATURE_PORT_POWER		8
#define HUB_FEATURE_C_PORT_CONNECTION	16
#define HUB_FEATURE_C_PORT_ENABLE	17
#define HUB_FEATURE_C_PORT_SUSPEND	18
#define HUB_FEATURE_C_PORT_OVER_CURRENT	19
#define HUB_FEATURE_C_PORT_RESET	20

/* HubPortStatus.status */
#define HUB_PORT_CONNECTION		0x0001
#define HUB_PORT_ENABLE			0x0002
#define HUB_PORT_LOW_SPEED		0x0200
#define HUB_PORT_HIGH_SPEED		0x0400

/* HubPortStatus.change */
#define HUB_PORT_C_CONNECTION		0x0001
#define HUB_PORT_C_ENABLE		0x0002
#define HUB_PORT_C_SUSPEND		0x0004
#define HUB_PORT_C_OVER_CURRENT		0x0008
#define HUB_PORT_C_RESET		0x0010

/* the hub descriptor, as far as it is read */
typedef struct HubDescriptor {
	SceUInt8 bDescLength;
	SceUInt8 bDescriptorType;
	SceUInt8 bNbrPorts;
	SceUInt8 wHubCharacteristics[2];
	SceUInt8 bPwrOn2PwrGood;
	SceUInt8 bHubContrCurrent;
} HubDescriptor;

/* GET_STATUS of a port */
typedef struct HubPortStatus {
	SceUInt16 status;
	SceUInt16 change;
} HubPortStatus;

typedef struct HubRequest {
	struct HubRequest *next;
	struct HubRequest *prev;
	SceUsbdDeviceRequest setup;
} HubRequest;

/* Hub.state: what the control requests are doing */
#define HUB_STATE_POWER			0	/* powering the ports one by one */
#define HUB_STATE_STATUS		1	/* reading a port's status */
#define HUB_STATE_CLEAR			2	/* clearing one of its change bits */

typedef struct Hub {
	struct Hub *next;
	struct Hub *prev;
	unsigned int deviceId;
	SceUID controlPipe;
	SceUID interruptPipe;
	SceSize interruptLength;
	int state;			/* HUB_STATE_* */
	int protocol;			/* bDeviceProtocol */
	int interruptPending;
	SceUInt16 clearFeature;		/* the change being cleared */
	SceUInt16 padding;
	SceUInt8 *statusChange;		/* the interrupt endpoint's bitmap */
	HubPortStatus *portStatusBuffer;
	HubPortStatus port[HUB_PORT_COUNT];	/* by port number */
	HubDescriptor *descriptor;
	HubRequest *request;		/* on the control pipe */
	HubRequest *requestHead;
	HubRequest *requestTail;
} Hub;

/* everything hubInit allocates, in one block */
typedef struct HubPool {
	SceUInt8 descriptor[HUB_COUNT][HUB_BUFFER_SIZE];
	SceUInt8 statusChange[HUB_COUNT][HUB_BUFFER_SIZE];
	HubPortStatus portStatus[HUB_COUNT][HUB_BUFFER_SIZE / sizeof(HubPortStatus)];
	Hub hub[HUB_COUNT];
} HubPool;

static int hubProbe(int deviceId);
static int hubAttach(int deviceId);
static int hubDetach(int deviceId);

static const SceUsbdDriver s_hubDriver = {
	.name = "SceUsbHub",
	.probe = hubProbe,
	.attach = hubAttach,
	.detach = hubDetach,
};

static Hub *s_hubHead;			/* attached */
static Hub *s_hubTail;
static Hub *s_hubFreeHead;
static Hub *s_hubFreeTail;

static void hubControlDone(int result, int count, void *arg);

/* the next queued request onto the control pipe */
static __always_inline void hubSubmitRequest(Hub *hub)
{
	HubRequest *request = hub->requestHead;

	if (request == NULL)
		return;
	USBD_LIST_REMOVE(hub->requestHead, hub->requestTail, request);
	hub->request = request;
	SceUsbdForDriver_2E05660F(hub->controlPipe, &request->setup,
				request->setup.wLength != 0 ? (unsigned char *)hub->portStatusBuffer : NULL,
				hubControlDone, hub);
}

static __always_inline void hubQueueRequest(Hub *hub, int requestType, int request, int value,
					    int index, int length)
{
	HubRequest *entry = usbdAlloc(sizeof(*entry));

	if (entry == NULL)
		return;
	entry->setup.bmRequestType = requestType;
	entry->setup.bRequest = request;
	entry->setup.wValue = value;
	entry->setup.wIndex = index;
	entry->setup.wLength = length;
	USBD_LIST_APPEND(hub->requestHead, hub->requestTail, entry);
	if (hub->request == NULL)
		hubSubmitRequest(hub);
}

static __always_inline Hub *hubFind(unsigned int deviceId)
{
	Hub *hub;

	for (hub = s_hubHead; hub != NULL; hub = hub->next) {
		if (hub->deviceId == deviceId)
			break;
	}
	return hub;
}

/* the hub descriptor arrived: power the first port */
static void hubGetDescriptorDone(int result, int count, void *arg)
{
	Hub *hub = arg;

	if (result != 0 || count < 3 || count > HUB_BUFFER_SIZE)
		return;
	if (hub->descriptor->bNbrPorts > HUB_PORT_COUNT)
		hub->descriptor->bNbrPorts = HUB_PORT_COUNT;
	hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_OUT, SCE_USBD_REQUEST_SET_FEATURE,
			HUB_FEATURE_PORT_POWER, 1, 0);
}

/* configured: read the hub descriptor */
static void hubSetConfigurationDone(int result, int count, void *arg)
{
	Hub *hub = arg;
	SceUsbdDeviceRequest setup;

	if (result != 0)
		return;
	setup.bmRequestType = HUB_REQUEST_TYPE_HUB_IN;
	setup.bRequest = SCE_USBD_REQUEST_GET_DESCRIPTOR;
	setup.wValue = HUB_DESCRIPTOR_TYPE << 8;
	setup.wIndex = 0;
	setup.wLength = 32;
	SceUsbdForDriver_2E05660F(hub->controlPipe, &setup, (unsigned char *)hub->descriptor,
				hubGetDescriptorDone, hub);
}

static int hubProbe(int deviceId)
{
	SceUsbdDeviceDescriptor *device;

	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL || device->bDeviceClass != SCE_USBD_CLASS_HUB)
		return -1;
	if (device->bNumConfigurations != 1)
		return -1;
	return 0;
}

static int hubAttach(int deviceId)
{
	SceUsbdConfigurationDescriptor *configuration;
	SceUsbdInterfaceDescriptor *interface;
	SceUsbdEndpointDescriptor *endpoint;
	SceUsbdDeviceDescriptor *device;
	SceUsbdDeviceRequest setup;
	SceSize length;
	SceUID pipe;
	Hub *hub;
	int i;

	hub = s_hubFreeHead;
	if (hub == NULL)
		return -1;
	USBD_LIST_REMOVE(s_hubFreeHead, s_hubFreeTail, hub);
	hub->deviceId = deviceId;
	hub->state = HUB_STATE_POWER;
	hub->interruptPending = 0;
	for (i = 0; i < HUB_PORT_COUNT; i++) {
		hub->port[i].status = 0;
		hub->port[i].change = 0;
	}
	hub->requestHead = NULL;
	hub->requestTail = NULL;
	USBD_LIST_APPEND(s_hubHead, s_hubTail, hub);

	/* one interface with one interrupt IN endpoint */
	device = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (device == NULL)
		goto fail;
	hub->protocol = device->bDeviceProtocol;
	configuration = SceUsbdForDriver_BC3EF82B(deviceId, NULL, SCE_USBD_DESCRIPTOR_CONFIGURATION);
	if (configuration == NULL || configuration->bNumInterfaces != 1)
		goto fail;
	interface = SceUsbdForDriver_BC3EF82B(deviceId, configuration, SCE_USBD_DESCRIPTOR_INTERFACE);
	if (interface == NULL || interface->bNumEndpoints != 1)
		goto fail;
	endpoint = SceUsbdForDriver_BC3EF82B(deviceId, interface, SCE_USBD_DESCRIPTOR_ENDPOINT);
	if (endpoint == NULL || !(endpoint->bEndpointAddress & SCE_USBD_ENDPOINT_DIRECTION_IN) ||
	    (endpoint->bmAttributes & SCE_USBD_ENDPOINT_TRANSFER_TYPE_BITS) !=
	    SCE_USBD_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
		goto fail;

	pipe = SceUsbdForDriver_1CDBFF9F(deviceId, endpoint);
	if (pipe < 0)
		goto fail;
	hub->interruptPipe = pipe;
	length = endpoint->wMaxPacketSize & 0x7ff;
	if (length > HUB_BUFFER_SIZE)
		length = HUB_BUFFER_SIZE;
	hub->interruptLength = length;
	pipe = SceUsbdForDriver_1CDBFF9F(deviceId, NULL);
	if (pipe < 0)
		goto closeInterrupt;
	hub->controlPipe = pipe;

	setup.bmRequestType = 0;
	setup.bRequest = SCE_USBD_REQUEST_SET_CONFIGURATION;
	setup.wValue = configuration->bConfigurationValue;
	setup.wIndex = 0;
	setup.wLength = 0;
	if (SceUsbdForDriver_2E05660F(hub->controlPipe, &setup, NULL, hubSetConfigurationDone, hub) >= 0)
		return 0;

	SceUsbdForDriver_F304DC5C(hub->controlPipe);
closeInterrupt:
	SceUsbdForDriver_F304DC5C(hub->interruptPipe);
fail:
	USBD_LIST_REMOVE(s_hubHead, s_hubTail, hub);
	USBD_LIST_APPEND(s_hubFreeHead, s_hubFreeTail, hub);
	return -1;
}

static int hubDetach(int deviceId)
{
	Hub *hub = hubFind(deviceId);

	if (hub == NULL)
		return 0;
	USBD_LIST_REMOVE(s_hubHead, s_hubTail, hub);
	USBD_LIST_APPEND(s_hubFreeHead, s_hubFreeTail, hub);
	return 0;
}

/* the interrupt endpoint: which of the hub (bit 0) and its ports changed */
static void hubStatusChange(int result, int count, void *arg)
{
	Hub *hub = arg;
	SceUInt8 *bitmap;
	SceUInt16 port;

	hub->interruptPending = 0;
	if (result != 0)
		return;
	bitmap = hub->statusChange;
	if (bitmap[0] & 1) {
		hubQueueRequest(hub, HUB_REQUEST_TYPE_HUB_IN, SCE_USBD_REQUEST_GET_STATUS, 0, 0,
				sizeof(HubPortStatus));
		return;
	}
	for (port = 1; port <= hub->descriptor->bNbrPorts; port++) {
		if (bitmap[port >> 3] >> (port & 7) & 1)
			break;
	}
	if (port > hub->descriptor->bNbrPorts)
		return;
	hub->state = HUB_STATE_STATUS;
	hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_IN, SCE_USBD_REQUEST_GET_STATUS, 0, port,
			sizeof(HubPortStatus));
}

static __always_inline void hubWaitStatusChange(Hub *hub)
{
	if (hub->interruptPending)
		return;
	hub->interruptPending = 1;
	SceUsbdForDriver_A0BF85B8(hub->interruptPipe, hub->statusChange, hub->interruptLength,
				  hubStatusChange, hub);
}

static void hubControlDone(int result, int count, void *arg)
{
	Hub *hub = arg;
	HubRequest *request;
	HubPortStatus *status;
	unsigned int port, speed;
	int feature;

	if (result != 0 || count > HUB_BUFFER_SIZE)
		return;
	request = hub->request;
	hub->request = NULL;

	switch (hub->state) {
	case HUB_STATE_POWER:
		port = request->setup.wIndex;
		usbdFree(request);
		if (port < hub->descriptor->bNbrPorts) {
			hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_OUT, SCE_USBD_REQUEST_SET_FEATURE,
					HUB_FEATURE_PORT_POWER, port + 1, 0);
		} else {
			hub->state++;
			hubWaitStatusChange(hub);
		}
		break;
	case HUB_STATE_STATUS:
		port = request->setup.wIndex;
		usbdFree(request);
		status = &hub->port[port];
		feature = hub->clearFeature;
		*status = *hub->portStatusBuffer;
		hub->clearFeature = 0;
		if (status->change == 0) {
			/* settled: a connection change attaches or detaches */
			if (feature == HUB_FEATURE_C_PORT_CONNECTION) {
				if (status->status & HUB_PORT_CONNECTION) {
					if (status->status & HUB_PORT_LOW_SPEED)
						speed = SCE_USBD_DEVICE_SPEED_LS;
					else if (status->status & HUB_PORT_HIGH_SPEED)
						speed = SCE_USBD_DEVICE_SPEED_HS;
					else
						speed = SCE_USBD_DEVICE_SPEED_FS;
					usbdNotifyAttach(hub->deviceId, port, speed);
				} else {
					usbdNotifyDetach(hub->deviceId, port);
				}
			}
			hubWaitStatusChange(hub);
			break;
		}
		if (status->change & HUB_PORT_C_CONNECTION)
			feature = HUB_FEATURE_C_PORT_CONNECTION;
		else if (status->change & HUB_PORT_C_ENABLE)
			feature = HUB_FEATURE_C_PORT_ENABLE;
		else if (status->change & HUB_PORT_C_SUSPEND)
			feature = HUB_FEATURE_C_PORT_SUSPEND;
		else if (status->change & HUB_PORT_C_OVER_CURRENT)
			feature = HUB_FEATURE_C_PORT_OVER_CURRENT;
		else if (status->change & HUB_PORT_C_RESET)
			feature = HUB_FEATURE_C_PORT_RESET;
		else
			feature = 0;
		if (feature != 0) {
			hub->clearFeature = feature;
			hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_OUT, SCE_USBD_REQUEST_CLEAR_FEATURE,
					feature, port, 0);
		}
		hub->state++;
		break;
	case HUB_STATE_CLEAR:
		port = request->setup.wIndex;
		usbdFree(request);
		hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_IN, SCE_USBD_REQUEST_GET_STATUS, 0, port,
				sizeof(HubPortStatus));
		hub->state--;
		break;
	}
	if (hub->request == NULL)
		hubSubmitRequest(hub);
}

int hubResetPort(unsigned int hubId, unsigned int port)
{
	Hub *hub = hubFind(hubId);

	if (hub == NULL)
		return -1;
	hubQueueRequest(hub, HUB_REQUEST_TYPE_PORT_OUT, SCE_USBD_REQUEST_SET_FEATURE,
			HUB_FEATURE_PORT_RESET, port, 0);
	return 0;
}

/* nonzero when the port is enabled */
int hubGetPortStatus(unsigned int hubId, unsigned int port, SceUInt8 *speed)
{
	Hub *hub = hubFind(hubId);
	HubPortStatus *status;

	if (hub == NULL)
		return 0;
	status = &hub->port[port];
	if (status->status & HUB_PORT_LOW_SPEED)
		*speed = SCE_USBD_DEVICE_SPEED_LS;
	else if (status->status & HUB_PORT_HIGH_SPEED)
		*speed = SCE_USBD_DEVICE_SPEED_HS;
	else
		*speed = SCE_USBD_DEVICE_SPEED_FS;
	return status->status & HUB_PORT_ENABLE;
}

int hubInit(void)
{
	HubPool *pool;
	Hub *hub;
	int i, result;

	pool = usbdAlloc(sizeof(*pool));
	if (pool == NULL)
		return SCE_USBD_ERROR_NO_MEMORY;
	memset(pool, 0, sizeof(*pool));
	s_hubHead = NULL;
	s_hubTail = NULL;
	s_hubFreeHead = NULL;
	s_hubFreeTail = NULL;
	for (i = 0; i < HUB_COUNT; i++) {
		hub = &pool->hub[i];
		hub->descriptor = (HubDescriptor *)pool->descriptor[i];
		hub->statusChange = pool->statusChange[i];
		hub->portStatusBuffer = pool->portStatus[i];
		USBD_LIST_APPEND(s_hubFreeHead, s_hubFreeTail, hub);
	}
	result = SceUsbdForDriver_1EC94F18(&s_hubDriver);
	if (result < 0) {
		usbdFree(pool);
		return result;
	}
	return 0;
}
