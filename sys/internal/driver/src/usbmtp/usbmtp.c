/*
 * SceUsbMtp: the USB MTP class driver under SceUdcd. mtpif drives the
 * containers and data phases; this module owns the pipes and the class
 * requests on the control pipe.
 */
#include "usbmtp.h"

/* Error names unknown; the SDK error index does not name this range. */
#define MTP_PROTOCOL ((int)0x80543601)
#define MTP_BUSY ((int)0x80543602)
#define MTP_INVALID ((int)0x80543603)
#define MTP_RUNNING ((int)0x80543604)
#define MTP_TIMEOUT ((int)0x80543605)
#define MTP_CANCELLED ((int)0x80543606)
#define MTP_DISCONNECTED ((int)0x80543607)
#define MTP_MEMORY ((int)0x80543608)
#define MTP_UNSUPPORTED ((int)0x80543609)
#define MTP_RETRY_LIMIT ((int)0x8054360a)
#define MTP_UNAVAILABLE ((int)0x8054360c)
#define MTP_WAIT_ERROR ((int)0x8054360d)

/* busFlags: what the bus and the host asked for */
#define MTP_BUS_ATTACHED	0x0001
#define MTP_BUS_RESET		0x0002		/* Device Reset Request */
#define MTP_BUS_CANCEL		0x0004		/* Cancel Request */

/* transferFlags */
#define MTP_TRANSFER_SENDING	0x0001		/* a data-in phase is running */
#define MTP_TRANSFER_RECEIVING	0x0002		/* a data-out phase is running */
#define MTP_TRANSFER_IN_STALLED	0x0010
#define MTP_TRANSFER_OUT_STALLED 0x0020
#define MTP_TRANSFER_STARTED	0x0100

/* status: what mtpif sees */
#define MTP_STATUS_CONNECTED	0x0001
#define MTP_STATUS_CANCELLED	0x0002

/* phase of the current transaction */
#define MTP_PHASE_IDLE		0
#define MTP_PHASE_COMMAND	1
#define MTP_PHASE_DATA_IN	2
#define MTP_PHASE_DATA_OUT	4
#define MTP_PHASE_RESPONSE	8
#define MTP_PHASE_STALLED	16

/* the event flag */
#define MTP_EVENT_ATTACHED	0x0001
#define MTP_EVENT_INITIALIZED	0x0002
#define MTP_EVENT_SENT		0x0200
#define MTP_EVENT_RECEIVED	0x0400
#define MTP_EVENT_NOTIFIED	0x0800
#define MTP_EVENT_CONFIGURED	0x1000
#define MTP_EVENT_SEND_FAILED	0x2000
#define MTP_EVENT_RECV_FAILED	0x4000
#define MTP_EVENT_NOTIFY_FAILED	0x8000

/* class requests (Still Image Capture Device class) */
#define MTP_REQUEST_CANCEL		0x64
#define MTP_REQUEST_DEVICE_RESET	0x66
#define MTP_REQUEST_GET_DEVICE_STATUS	0x67

/* response codes of the device status */
#define MTP_RESPONSE_OK			0x2001
#define MTP_RESPONSE_DEVICE_BUSY	0x2019
#define MTP_RESPONSE_TRANSACTION_CANCELLED 0x201f

#define MTP_WAIT_TIMEOUT	4000000		/* microseconds */

#define MTP_CACHE_LINE		64
#define MTP_ALIGN(p)		((uint8_t *)(((uintptr_t)(p) + MTP_CACHE_LINE - 1) & ~(MTP_CACHE_LINE - 1)))

/* the device status the class request returns */
typedef struct MtpDeviceStatus {
	uint16_t wLength;
	uint16_t code;
	uint32_t params[6];		/* the stalled endpoints */
} MtpDeviceStatus;

/* the data of a Cancel Request: code 0x4001 and the transaction */
typedef struct __attribute__((packed)) MtpCancelRequest {
	uint8_t code[2];
	uint32_t transaction;
} MtpCancelRequest;

static struct UsbMtp {
	SceKernelSpinlock lock;
	SceUdcdDriver driver;
	unsigned port;
	unsigned product;
	unsigned speed;			/* 2: high speed, 1: full speed */
	unsigned packetSize;		/* of the bulk pipes */
	unsigned busFlags;		/* MTP_BUS_* */
	unsigned transferFlags;		/* MTP_TRANSFER_* */
	SceUID memblock;
	SceUID event;			/* MTP_EVENT_* */
	unsigned staged;		/* bytes in data */
	unsigned cursor;		/* the next of them */
	int64_t remaining;		/* of the data phase */
	int64_t received;
	void *controlBuffer;
	void *sendBuffer;		/* never set: sends name their buffer */
	void *receiveBuffer;
	void *reserved0;
	MtpContainer *data;
	MtpContainer *command;
	MtpContainer *response;
	MtpContainer *eventBuffer;
	unsigned transaction;
	unsigned cancelled;		/* the transaction the host cancelled */
	unsigned cancelPending;
	unsigned deferredResponse;
	unsigned deferredCancel;
	MtpRequest control;
	MtpRequest cancel;
	MtpRequest send;
	MtpRequest recv;
	MtpRequest notify;
	unsigned controlCode;		/* bRequest of the last control request */
	uint8_t serialNumber[128];
	SceUdcdInterface interface;
	MtpOsStringDescriptor osString;
	uint16_t reserved1;
	SceUdcdStringDescriptor serialString;
	MtpContainer cancelResponse;
	MtpCompatibleIdDescriptor compatibleId;
	unsigned phase;			/* MTP_PHASE_* */
	unsigned ready;			/* between Start and Stop */
	unsigned status;		/* MTP_STATUS_* */
	SceUdcdEndpoint endpoints[4];
	MtpContainer successResponse;
} s_mtp;

_Static_assert(offsetof(struct UsbMtp, control) == 0xc4, "control request");
_Static_assert(offsetof(struct UsbMtp, osString) == 0x294, "OS string");
_Static_assert(offsetof(struct UsbMtp, phase) == 0x330, "phase");
_Static_assert(sizeof(MtpIfTransfer) == 32, "shared transfer layout");

static const SceUdcdDeviceDescriptor s_deviceDescriptor = {
	.bLength = 18,
	.bDescriptorType = 1,
	.bcdUSB = 0x200,
	.bcdDevice = 0x100,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

static const SceUdcdConfigDescriptor s_configDescriptor = {
	.bLength = 9,
	.bDescriptorType = 2,
	.wTotalLength = 0x27,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.bmAttributes = 0xc0,
	.bMaxPower = 1,
};

static const SceUdcdInterfaceSettings s_interfaceSettings = {
	.numDescriptors = 1,
};

static const MtpInterfaceDescriptors s_interfaceDescriptors = {{
	{ .bLength = 9, .bDescriptorType = 4, .bNumEndpoints = 3 },
}};

static const MtpEndpointDescriptors s_endpointDescriptors = {{
	{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x81, .bmAttributes = 2 },
	{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x02, .bmAttributes = 2 },
	{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x83, .bmAttributes = 3 },
}};

static const SceUdcdStringDescriptor s_productString = {
	.bLength = 34,
	.bDescriptorType = 3,
	.bString = { '"', 'P', 'S', ' ', 'V', 'i', 't', 'a', '"', ' ', 'T', 'y', 'p', 'e', ' ', 'B' },
};

static const MtpCompatibleIdDescriptor s_compatibleId = {
	.dwLength = sizeof(MtpCompatibleIdDescriptor),
	.bcdVersion = 0x0100,
	.wIndex = 4,
	.bCount = 1,
	.reserved1 = 1,
	.compatibleID = "MTP",
	.subCompatibleID = { 0, 0, '3' },
};

static int changeSetting(int interfaceNumber, int alternateSetting, int bus)
{
	return interfaceNumber;
}

static void configure(int usbVersion, int descCount, SceUdcdInterfaceSettings *settings,
		      void *userData)
{
	s_mtp.compatibleId.bFirstInterfaceNumber = s_mtp.interface.interfaceNumber;
}

static void completeControl(SceUdcdDeviceRequest *req)
{
}

/* fills one speed's descriptors at base, returns the end of them */
static uint8_t *initializeDescriptors(int speed, SceUdcdDeviceDescriptor *device,
				      SceUdcdConfiguration *configuration, uint8_t *base)
{
	MtpDescriptors *descriptors = (MtpDescriptors *)base;

	configuration->configDescriptors = &descriptors->config;
	configuration->settings = &descriptors->settings;
	configuration->interfaceDescriptors = descriptors->interfaces.descriptor;
	configuration->endpointDescriptors = descriptors->endpoints.descriptor;
	*device = s_deviceDescriptor;
	*configuration->configDescriptors = s_configDescriptor;
	*configuration->settings = s_interfaceSettings;
	descriptors->interfaces = s_interfaceDescriptors;
	descriptors->endpoints = s_endpointDescriptors;
	configuration->configDescriptors->settings = configuration->settings;
	configuration->settings->descriptors = configuration->interfaceDescriptors;
	configuration->interfaceDescriptors->endpoints = configuration->endpointDescriptors;
	if (speed == 2) {
		configuration->endpointDescriptors[0].wMaxPacketSize = 512;
		configuration->endpointDescriptors[1].wMaxPacketSize = 512;
		configuration->endpointDescriptors[2].wMaxPacketSize = 64;
		configuration->endpointDescriptors[2].bInterval = 4;
	} else {
		configuration->endpointDescriptors[0].wMaxPacketSize = 64;
		configuration->endpointDescriptors[1].wMaxPacketSize = 64;
		configuration->endpointDescriptors[2].wMaxPacketSize = 64;
		configuration->endpointDescriptors[2].bInterval = 1;
	}
	device->bMaxPacketSize0 = 64;
	s_mtp.serialString.bLength = 36;
	s_mtp.serialString.bDescriptorType = 3;
	s_mtp.osString.bLength = sizeof(s_mtp.osString);
	s_mtp.osString.bDescriptorType = 3;
	s_mtp.osString.bString[0] = 'M';
	s_mtp.osString.bString[1] = 'S';
	s_mtp.osString.bString[2] = 'F';
	s_mtp.osString.bString[3] = 'T';
	s_mtp.osString.bString[4] = '1';
	s_mtp.osString.bString[5] = '0';
	s_mtp.osString.bString[6] = '0';
	s_mtp.osString.bString[7] = 1;		/* vendor code */
	return base + sizeof(*descriptors);
}

static void detach(void *userData)
{
	unsigned int intr;

	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_ATTACHED);
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.busFlags &= ~(MTP_BUS_ATTACHED | MTP_BUS_RESET | MTP_BUS_CANCEL);
	s_mtp.transferFlags &= ~(MTP_TRANSFER_SENDING | MTP_TRANSFER_RECEIVING |
				 MTP_TRANSFER_IN_STALLED | MTP_TRANSFER_OUT_STALLED);
	s_mtp.status &= ~MTP_STATUS_CONNECTED;
	s_mtp.phase = MTP_PHASE_IDLE;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
}

static void completeCancel(SceUdcdDeviceRequest *req);
static void completeNotify(SceUdcdDeviceRequest *req);
static void completeRecv(SceUdcdDeviceRequest *req);
static void completeSend(SceUdcdDeviceRequest *req);

static inline void initializeRequest(SceUdcdDeviceRequest *req, SceUdcdEndpoint *endpoint,
				     void *data, int size, int isControlRequest,
				     void (*onComplete)(SceUdcdDeviceRequest *))
{
	req->endpoint = endpoint;
	req->data = data;
	req->attributes = 0;
	req->size = size;
	req->isControlRequest = isControlRequest;
	req->onComplete = onComplete;
	req->transmitted = 0;
	req->returnCode = 1;
	req->next = NULL;
	req->unused = NULL;
}

static inline void initializeRequests(void)
{
	initializeRequest(&s_mtp.control.request, &s_mtp.endpoints[0],
			  s_mtp.controlBuffer, 1, 1, completeControl);
	initializeRequest(&s_mtp.cancel.request, &s_mtp.endpoints[0],
			  s_mtp.controlBuffer, 1, 1, completeCancel);
	initializeRequest(&s_mtp.send.request, &s_mtp.endpoints[1],
			  s_mtp.sendBuffer, 0, 0, completeSend);
	initializeRequest(&s_mtp.recv.request, &s_mtp.endpoints[2],
			  s_mtp.receiveBuffer, 0, 0, completeRecv);
	initializeRequest(&s_mtp.notify.request, &s_mtp.endpoints[3],
			  &s_mtp.eventBuffer, 24, 0, completeNotify);
}

static int Start(int size, void *args, void *userData)
{
	SceUInt32 param[16];

	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_CONFIGURED);
	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_SEND_FAILED);
	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_RECV_FAILED);
	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_NOTIFY_FAILED);
	initializeRequests();
	s_mtp.ready = 1;
	s_mtp.transferFlags |= MTP_TRANSFER_STARTED;
	param[1] = 0xffff;
	SceUdcdForDriver_78BF2A28(param);
	return 0;
}

static int attach(int usbVersion, void *userData)
{
	SceUdcdDeviceInfo info;
	unsigned int intr;
	int ret;

	ksceUdcdGetDeviceInfo(&info);
	if (usbVersion == 2) {
		s_mtp.speed = 2;
		s_mtp.packetSize = 512;
	} else {
		s_mtp.speed = 1;
		s_mtp.packetSize = 64;
	}
	initializeRequests();
	ksceUdcdClearFIFO(&s_mtp.endpoints[1]);
	ksceUdcdClearFIFO(&s_mtp.endpoints[2]);
	ksceUdcdClearFIFO(&s_mtp.endpoints[3]);
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.status = MTP_STATUS_CONNECTED;
	s_mtp.phase = MTP_PHASE_IDLE;
	s_mtp.busFlags = MTP_BUS_ATTACHED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_ATTACHED);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_CONFIGURED);
	ret = ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_CONFIGURED);
	s_mtp.transaction = -1;
	s_mtp.cancelled = -1;
	s_mtp.cancelPending = 0;
	s_mtp.deferredResponse = -1;
	s_mtp.deferredCancel = -1;
	return ret;
}

static int Stop(int size, void *args, void *userData)
{
	if (s_mtp.busFlags & MTP_BUS_ATTACHED) {
		SCE_DBG_PRINTF(0, 2283, "[%s%-28s] Err(%4d) device is now running\n",
			       "usbmtp:", __func__, 2283);
		return MTP_RUNNING;
	}
	s_mtp.ready = 0;
	s_mtp.transferFlags &= ~MTP_TRANSFER_STARTED;
	return 0;
}

static void completeCancel(SceUdcdDeviceRequest *req)
{
	const MtpCancelRequest *cancel;
	unsigned int intr;

	if (s_mtp.controlCode != MTP_REQUEST_CANCEL)
		return;
	cancel = req->data;
	if (cancel->code[0] != 0x01 || cancel->code[1] != 0x40)
		return;
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.cancelled = ((const MtpCancelRequest *)req->data)->transaction;
	if ((s_mtp.transferFlags & MTP_TRANSFER_SENDING) &&
	    s_mtp.phase == MTP_PHASE_DATA_IN && s_mtp.remaining > 0)
		s_mtp.cancelPending = 1;
	s_mtp.busFlags |= MTP_BUS_CANCEL;
	s_mtp.status |= MTP_STATUS_CANCELLED;
	ksceUdcdClearFIFO(&s_mtp.endpoints[1]);
	ksceUdcdClearFIFO(&s_mtp.endpoints[3]);
	ksceUdcdClearFIFO(&s_mtp.endpoints[2]);
	s_mtp.transferFlags &= ~MTP_TRANSFER_RECEIVING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
}

static void completeNotify(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_NOTIFIED);
		return;
	}
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_NOTIFY_FAILED);
	req->returnCode = 0;
}

static void completeRecv(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_RECEIVED);
		return;
	}
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_RECV_FAILED);
	req->returnCode = 0;
}

static void completeSend(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0) {
		ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_SENT);
		return;
	}
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_SEND_FAILED);
	req->returnCode = 0;
}

static int _sceUsbMtpInitTask(void *args)
{
	SceKernelAllocMemBlockKernelOpt opt;
	uint8_t *base;
	unsigned int i;
	int ret;

	memset(s_mtp.serialNumber, 0, sizeof(s_mtp.serialNumber));
	ret = ksceIdStorageLookup(0x112, 0, s_mtp.serialNumber, 64);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2425, "[%s%-28s] Err(%4d) sceIdStorageLookup(), ret=%08x\n",
			       "usbmtp:", __func__, 2425, ret);
		for (i = 0; i < 64; i += 2) {
			s_mtp.serialNumber[i] = 0;
			s_mtp.serialNumber[i + 1] = '0';
		}
	}
	memcpy(s_mtp.serialString.bString, &s_mtp.serialNumber[31], 34);
	s_mtp.status = 0;
	s_mtp.phase = MTP_PHASE_IDLE;
	s_mtp.memblock = -1;
	s_mtp.busFlags = 0;
	s_mtp.transferFlags = 0;
	s_mtp.speed = 1;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s_mtp.memblock = SceSysmemForDriver_C94850C9("SceUsbMtp", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
						 0x11000, &opt);
	if (s_mtp.memblock < 0) {
		SCE_DBG_PRINTF(0, 2480, "[%s%-28s] Err(%4d) sceKernelAllocMemBlock. Size:0x%x\n",
			       "usbmtp:", __func__, 2480, 0x11000);
		return MTP_MEMORY;
	}
	ret = SceSysmemForDriver_A841EDDA(s_mtp.memblock, (void **)&base);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2486, "[%s%-28s] Err(%4d) sceKernelGetMemBlockBase\n",
			       "usbmtp:", __func__, 2486);
		SceSysmemForDriver_009E1C61(s_mtp.memblock);
		return MTP_MEMORY;
	}

	/* the high- and full-speed descriptors, then the containers and buffers */
	s_mtp.driver.descriptor_hi = (SceUdcdDeviceDescriptor *)base;
	s_mtp.driver.configuration_hi = (SceUdcdConfiguration *)(base + sizeof(SceUdcdDeviceDescriptor));
	base = MTP_ALIGN(initializeDescriptors(2, s_mtp.driver.descriptor_hi,
					       s_mtp.driver.configuration_hi,
					       base + sizeof(SceUdcdDeviceDescriptor) +
					       sizeof(SceUdcdConfiguration)));
	s_mtp.driver.descriptor = (SceUdcdDeviceDescriptor *)base;
	s_mtp.driver.configuration = (SceUdcdConfiguration *)(base + sizeof(SceUdcdDeviceDescriptor));
	base = MTP_ALIGN(initializeDescriptors(1, s_mtp.driver.descriptor,
					       s_mtp.driver.configuration,
					       base + sizeof(SceUdcdDeviceDescriptor) +
					       sizeof(SceUdcdConfiguration)));
	s_mtp.command = (MtpContainer *)base;
	s_mtp.response = (MtpContainer *)(base + 64);
	s_mtp.eventBuffer = (MtpContainer *)(base + 128);
	s_mtp.controlBuffer = base + 192;
	s_mtp.data = (MtpContainer *)(base + 320);
	s_mtp.receiveBuffer = base + 832;
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_INITIALIZED);
	return 1;
}

/*
 * Wait for a bulk or interrupt request to complete: 0 when it did, otherwise
 * MTP_CANCELLED/MTP_DISCONNECTED (the request failed), MTP_WAIT_ERROR, or
 * MTP_TIMEOUT once the endpoint has been cancelled.
 */
static inline int WaitTransfer(SceUdcdDeviceRequest *req, SceUdcdEndpoint *endpoint,
			       unsigned int done, unsigned int failed, int bounded)
{
	SceUInt32 timeout = MTP_WAIT_TIMEOUT;
	unsigned int bits;
	int ret;

	ret = ksceKernelWaitEventFlag(s_mtp.event, done | failed, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR,
				      &bits, bounded ? &timeout : NULL);
	if ((bits & failed) || req->returnCode < 0)
		return s_mtp.busFlags & MTP_BUS_ATTACHED ? MTP_CANCELLED : MTP_DISCONNECTED;
	if (ret == 0)
		return 0;
	if (ret != (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		return MTP_WAIT_ERROR;
	timeout = MTP_WAIT_TIMEOUT;
	ksceUdcdClearFIFO(endpoint);
	if (ksceUdcdReqCancelAll(endpoint) == SCE_UDCD_ERROR_DRIVER_IN_PROGRESS)
		ksceKernelWaitEventFlag(s_mtp.event, failed, SCE_EVENT_WAITCLEAR, NULL, &timeout);
	return MTP_TIMEOUT;
}

static int sendBulk(void *buffer, unsigned int size)
{
	SceUdcdDeviceRequest *req = &s_mtp.send.request;
	uintptr_t start;
	int ret;

	/* 1: idle, 0: done; anything else is still queued */
	if ((unsigned int)req->returnCode > 1)
		return MTP_BUSY;
	if (buffer == NULL)
		return MTP_INVALID;
	req->data = buffer;
	req->size = size;
	req->isControlRequest = 0;
	start = (uintptr_t)buffer & ~(MTP_CACHE_LINE - 1);
	SceCpuForDriver_103872A5((void *)start, (uintptr_t)MTP_ALIGN((uint8_t *)buffer + size) - start);
	ret = ksceUdcdReqSend(req);
	if (ret < 0)
		return ret;
	ret = WaitTransfer(req, &s_mtp.endpoints[1], MTP_EVENT_SENT, MTP_EVENT_SEND_FAILED, 0);
	if (ret < 0)
		return ret;
	return req->transmitted;
}

static int receiveBulk(void *buffer, unsigned int size)
{
	SceUdcdDeviceRequest *req = &s_mtp.recv.request;
	unsigned int padding, intr, length;
	uintptr_t start;
	int ret;

	if ((unsigned int)req->returnCode > 1)
		return MTP_BUSY;
	if (buffer == NULL)
		return MTP_INVALID;
	req->data = buffer;
	req->isControlRequest = 0;
	/* receive whole packets */
	padding = size % s_mtp.packetSize;
	if (padding)
		padding = s_mtp.packetSize - padding;
	req->size = size + padding;
	start = (uintptr_t)buffer & ~(MTP_CACHE_LINE - 1);
	length = (uintptr_t)MTP_ALIGN((uint8_t *)buffer + req->size) - start;
	SceCpuForDriver_364E68A4((void *)start, length);
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.busFlags & MTP_BUS_CANCEL) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return MTP_CANCELLED;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	ret = ksceUdcdReqRecv(req);
	if (ret < 0)
		return ret;
	ret = WaitTransfer(req, &s_mtp.endpoints[2], MTP_EVENT_RECEIVED, MTP_EVENT_RECV_FAILED, 0);
	if (ret < 0)
		return ret;
	SceCpuForDriver_364E68A4((void *)start, length);
	return req->transmitted;
}

static int processRequest(int recipient, int arg, SceUdcdEP0DeviceRequest *req, void *userData)
{
	SceUdcdDeviceRequest *control = &s_mtp.control.request;
	MtpDeviceStatus status;
	unsigned int intr, length, count;

	s_mtp.controlCode = req->bRequest;
	if (arg < 0)
		return MTP_UNSUPPORTED;
	switch (req->bmRequestType & 0x60) {
	case 0x00:	/* standard: the Microsoft OS string */
		if (req->bRequest != 6 || req->wValue != 0x3ee)
			break;
		length = req->wLength;
		*(MtpOsStringDescriptor *)s_mtp.controlBuffer = s_mtp.osString;
		control->size = length < sizeof(s_mtp.osString) ? length : sizeof(s_mtp.osString);
		SceCpuForDriver_103872A5(control->data, 128);
		return ksceUdcdReqSend(control);
	case 0x40:	/* vendor: the extended compatible ID */
		if (req->bRequest != 1 || req->wIndex != 4)
			break;
		length = req->wLength;
		*(MtpCompatibleIdDescriptor *)s_mtp.controlBuffer = s_mtp.compatibleId;
		control->size = length < sizeof(s_mtp.compatibleId) ? length : sizeof(s_mtp.compatibleId);
		SceCpuForDriver_103872A5(control->data, 128);
		return ksceUdcdReqSend(control);
	case 0x20:	/* class */
		if (recipient != 1)
			break;
		switch (req->bRequest) {
		case MTP_REQUEST_CANCEL:
			length = req->wLength < sizeof(MtpCancelRequest) ?
				 req->wLength : sizeof(MtpCancelRequest);
			s_mtp.cancel.request.size = length;
			SceCpuForDriver_02796361(s_mtp.cancel.request.data,
							(length + MTP_CACHE_LINE - 1) & ~(MTP_CACHE_LINE - 1));
			return ksceUdcdReqRecv(&s_mtp.cancel.request);
		case MTP_REQUEST_DEVICE_RESET:
			ksceUdcdReqCancelAll(&s_mtp.endpoints[1]);
			ksceUdcdReqCancelAll(&s_mtp.endpoints[2]);
			ksceUdcdClearFIFO(&s_mtp.endpoints[1]);
			ksceUdcdClearFIFO(&s_mtp.endpoints[2]);
			s_mtp.busFlags |= MTP_BUS_RESET;
			s_mtp.status |= MTP_STATUS_CANCELLED;
			return 0;
		case MTP_REQUEST_GET_DEVICE_STATUS:
			status.wLength = 4;
			count = 0;
			intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
			if (s_mtp.transferFlags & MTP_TRANSFER_IN_STALLED) {
				s_mtp.phase = MTP_PHASE_STALLED;
				status.params[count++] = s_mtp.endpoints[1].endpointNumber;
				status.wLength += 4;
			}
			if (s_mtp.transferFlags & MTP_TRANSFER_OUT_STALLED) {
				status.params[count++] = s_mtp.endpoints[2].endpointNumber;
				status.wLength += 4;
				goto cancelled;
			}
			switch (s_mtp.phase) {
			case MTP_PHASE_IDLE:
				status.code = MTP_RESPONSE_OK;
				s_mtp.status &= ~MTP_STATUS_CANCELLED;
				break;
			case MTP_PHASE_STALLED:
cancelled:
				s_mtp.transferFlags &= ~(MTP_TRANSFER_IN_STALLED |
							 MTP_TRANSFER_OUT_STALLED);
				status.code = MTP_RESPONSE_TRANSACTION_CANCELLED;
				s_mtp.phase = MTP_PHASE_IDLE;
				break;
			case MTP_PHASE_COMMAND:
			case MTP_PHASE_DATA_IN:
			case MTP_PHASE_DATA_OUT:
			case MTP_PHASE_RESPONSE:
			default:
				status.code = MTP_RESPONSE_DEVICE_BUSY;
				break;
			}
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			length = req->wLength;
			memcpy(s_mtp.controlBuffer, &status, status.wLength);
			control->size = status.wLength < length ? status.wLength : length;
			SceCpuForDriver_103872A5(control->data, 128);
			return ksceUdcdReqSend(control);
		}
		break;
	}
	return MTP_UNSUPPORTED;
}

int SceUsbMtpForDriver_B958E5DE(void)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	SceUdcdForDriver_28BFEC45();
	return 0;
}

int sceUsbMtpSendData(MtpIfTransfer *transfer)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	int64_t size = transfer->size, total = transfer->totalSize;
	uint8_t *buffer = transfer->buffer;
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	int ret;
	if (s_mtp.cancelled != (unsigned)-1) {
		if (!s_mtp.cancelPending) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			ret = MTP_CANCELLED;
			goto error;
		}
		s_mtp.cancelPending = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		if (s_mtp.remaining > 0)
			sendBulk(s_mtp.data, 0);
		intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
		if (s_mtp.deferredResponse == s_mtp.transaction) {
			s_mtp.deferredResponse = -1;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			MtpContainer *response = &s_mtp.cancelResponse;
			response->length = 12;
			response->type = 3;
			response->code = 0x2007;
			response->transaction = s_mtp.transaction;
			for (unsigned i = 0; i < 5; i++)
				response->params[i] = 0;
			sendBulk(response, 12);
		} else {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		}
		intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
		s_mtp.phase = MTP_PHASE_IDLE;
		s_mtp.status &= ~2U;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		ret = MTP_CANCELLED;
		goto error;
	}
	if (!buffer || ((unsigned)buffer & 3)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		ret = MTP_INVALID;
		goto error;
	}
	s_mtp.transferFlags |= MTP_TRANSFER_SENDING;
	s_mtp.phase = MTP_PHASE_DATA_IN;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	if (total < (int64_t)((uint64_t)size + s_mtp.remaining)) {
		if (s_mtp.remaining >= total) {
			intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
			s_mtp.transferFlags &= ~1U;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			return MTP_BUSY;
		}
		size = total - s_mtp.remaining;
	}
	unsigned first = 0;
	ret = MTP_PROTOCOL;
	if (!s_mtp.remaining) {
		s_mtp.data->length = total <= 0xfffffff3LL ? total + 12 : -1;
		s_mtp.data->type = 2;
		s_mtp.data->code = s_mtp.command->code;
		s_mtp.data->transaction = s_mtp.command->transaction;
		s_mtp.staged = 12;
	}
	if (s_mtp.staged) {
		int64_t n = s_mtp.packetSize - s_mtp.staged;
		if (size < n)
			n = size;
		first = n;
		memcpy((uint8_t *)s_mtp.data + s_mtp.staged, buffer, first);
		s_mtp.staged += first;
		s_mtp.remaining += n;
		if (s_mtp.staged < s_mtp.packetSize && s_mtp.remaining < total) {
			transfer->transferred = size;
			return 0;
		}
		ret = sendBulk(s_mtp.data, s_mtp.staged);
		if (ret < 0)
			goto error;
		s_mtp.staged = 0;
		size -= n;
	}
	int64_t bulk = (int64_t)((uint64_t)size + s_mtp.remaining) >= total ?
		total - s_mtp.remaining : (uint32_t)size & -s_mtp.packetSize;
	int64_t done = first;
	if (bulk) {
		ret = sendBulk(buffer + first, bulk);
		if (ret < 0)
			goto error;
		size -= bulk;
		done += bulk;
		s_mtp.remaining += bulk;
	}
	if (size) {
		if (size > s_mtp.packetSize) {
			SCE_DBG_PRINTF(0, 1524,
				"[%s%-28s] Err(%4d) Invalid data size(%lld bytes). Must be short than MaxPacket(%d)\n",
				"usbmtp:", __func__, 1524);
			goto error;
		}
		done += size;
		memcpy((uint8_t *)s_mtp.data + s_mtp.staged, buffer + first + (unsigned)bulk, size);
		s_mtp.remaining += size;
		s_mtp.staged += (unsigned)size;
	}
	if (s_mtp.remaining >= total) {
		/* a transfer that ends on a packet boundary ends with a zero-length packet */
		if (!(ret & (s_mtp.packetSize - 1))) {
			SceUdcdDeviceRequest *req = &s_mtp.send.request;

			if ((unsigned int)req->returnCode > 1) {
				ret = MTP_BUSY;
				goto error;
			}
			req->size = 0;
			ret = ksceUdcdReqSend(req);
			if (ret < 0)
				goto error;
			ret = WaitTransfer(req, &s_mtp.endpoints[1], MTP_EVENT_SENT,
					   MTP_EVENT_SEND_FAILED, 0);
			if (ret < 0)
				goto error;
		}
		intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
		s_mtp.transferFlags &= ~1U;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	}
	transfer->transferred = done;
	return 0;
error:
	s_mtp.staged = 0;
	s_mtp.remaining = 0;
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.transferFlags &= ~1U;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return ret;
}

int sceUsbMtpRecvData(MtpIfTransfer *transfer)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	int64_t size = transfer->size;
	uint8_t *buffer = transfer->buffer;
	if (size > 0x10000)
		return MTP_INVALID;
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	int ret;
	if (s_mtp.cancelled != (unsigned)-1) {
		ret = MTP_CANCELLED;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		goto error;
	}
	if (s_mtp.received > 0 && !s_mtp.remaining) {
		transfer->transferred = 0;
		transfer->totalSize = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return 0;
	}
	if (!buffer || ((unsigned)buffer & 3)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		ret = MTP_INVALID;
		goto error;
	}
	s_mtp.transferFlags |= MTP_TRANSFER_RECEIVING;
	s_mtp.phase = MTP_PHASE_DATA_OUT;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	ret = MTP_PROTOCOL;
	if (!s_mtp.remaining) {
		ret = receiveBulk(s_mtp.data, s_mtp.packetSize);
		if (ret < 0)
			goto error;
		if (s_mtp.data->type != 2 || s_mtp.data->code != s_mtp.command->code || ret < 12)
			return MTP_PROTOCOL;
		s_mtp.cursor = 12;
		if (s_mtp.data->length == (unsigned)-1) {
			s_mtp.remaining = -1;
			transfer->totalSize = -1;
		} else if (s_mtp.data->length < 12) {
			s_mtp.remaining = -1;
		} else {
			s_mtp.remaining = s_mtp.data->length - 12;
			transfer->totalSize = s_mtp.remaining;
		}
		s_mtp.staged = ret - 12;
	}
	int64_t done = 0;
	if (s_mtp.staged) {
		int64_t n = s_mtp.staged;
		if (size < n)
			n = size;
		memcpy(buffer, (uint8_t *)s_mtp.data + s_mtp.cursor, n);
		s_mtp.staged -= (unsigned)n;
		done = n;
		if (s_mtp.remaining != -1) {
			s_mtp.remaining -= n;
			if (!s_mtp.remaining) {
				s_mtp.transferFlags &= ~2U;
				goto finished;
			}
		}
		size -= n;
		if (size) {
			if (!s_mtp.staged && n + s_mtp.cursor < s_mtp.packetSize) {
				s_mtp.cursor = 0;
				goto output;
			}
			s_mtp.cursor = 0;
		} else {
			s_mtp.cursor += n;
			goto output;
		}
	}
	if (s_mtp.remaining != -1 && s_mtp.remaining < size)
		size = s_mtp.remaining;
	unsigned bulk = (unsigned)size & -s_mtp.packetSize;
	if (bulk) {
		ret = receiveBulk(buffer + (unsigned)done, bulk);
		if (ret < 0)
			goto error;
		size -= ret;
		done += ret;
		if ((unsigned)ret < bulk) {
			s_mtp.remaining = 0;
			goto finished;
		}
		if (s_mtp.remaining != -1)
			s_mtp.remaining -= ret;
	}
	if (size <= 0) {
		if (!s_mtp.remaining)
			goto finished;
		goto output;
	}
	if (size > s_mtp.packetSize) {
		SCE_DBG_PRINTF(0, 1802,
			"[%s%-28s] Err(%4d) Invalid data size(%lld bytes). Must be short than MaxPacket(%d)\n",
			"usbmtp:", __func__, 1802);
		ret = -1;
		goto error;
	}
	ret = receiveBulk(s_mtp.data, s_mtp.packetSize);
	if (ret < 0)
		goto error;
	if ((int64_t)ret < size || ((unsigned)ret < s_mtp.packetSize && ret == size)) {
		memcpy(buffer + (unsigned)done, s_mtp.data, ret);
		done += ret;
		s_mtp.remaining = 0;
		goto finished;
	}
	memcpy(buffer + (unsigned)done, s_mtp.data, size);
	s_mtp.staged = ret - (unsigned)size;
	if (s_mtp.remaining != -1)
		s_mtp.remaining -= size;
	done += size;
	s_mtp.cursor = size;
	if (s_mtp.remaining)
		goto output;
finished:
	if (ret > 0 && !(ret & (s_mtp.packetSize - 1)))
		receiveBulk(s_mtp.data, 0);
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.transferFlags &= ~2U;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
output:
	s_mtp.received += done;
	transfer->transferred = done;
	transfer->totalSize = s_mtp.remaining;
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.cancelled != (unsigned)-1) {
		if (s_mtp.received > 0 && (!s_mtp.remaining ||
		    (s_mtp.remaining > 0 && s_mtp.remaining <= s_mtp.staged + s_mtp.packetSize * 2))) {
			s_mtp.deferredResponse = s_mtp.transaction;
			s_mtp.cancelled = -1;
			s_mtp.status &= ~2U;
			s_mtp.deferredCancel = -1;
		} else {
			ret = MTP_CANCELLED;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			goto error;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return 0;
error:
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.cancelled != (unsigned)-1) {
		if (s_mtp.received || s_mtp.remaining) {
			if (s_mtp.transferFlags & MTP_TRANSFER_RECEIVING) {
				s_mtp.transferFlags |= MTP_TRANSFER_OUT_STALLED;
				ksceUdcdStall(&s_mtp.endpoints[2]);
			}
			s_mtp.deferredResponse = s_mtp.deferredCancel = -1;
		}
		s_mtp.transferFlags &= ~2U;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	s_mtp.received = s_mtp.remaining = 0;
	s_mtp.staged = 0;
	return ret;
}

int SceUsbMtpForDriver_576A266F(void *command, MtpIfTransfer *transfer)
{
	MtpCommand *out = command;
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	if (!(s_mtp.busFlags & MTP_BUS_ATTACHED))
		return MTP_DISCONNECTED;
	int ret;
	unsigned intr;
	if (!transfer || !out) {
		ret = MTP_INVALID;
		goto error;
	}
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.cancelPending) {
		s_mtp.cancelPending = 0;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		sendBulk(s_mtp.data, 0);
		intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
		s_mtp.status &= ~2U;
		s_mtp.phase = MTP_PHASE_IDLE;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.deferredResponse == s_mtp.transaction && s_mtp.deferredResponse != (unsigned)-1) {
		s_mtp.deferredResponse = -1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		MtpContainer *response = &s_mtp.successResponse;
		response->length = 12;
		response->type = 3;
		response->code = 0x2001;
		response->transaction = s_mtp.transaction;
		for (unsigned i = 0; i < 5; i++)
			response->params[i] = 0;
		sendBulk(response, 12);
	} else {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	}
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.busFlags & MTP_BUS_CANCEL) {
		s_mtp.busFlags &= ~4U;
		s_mtp.status &= ~2U;
		s_mtp.phase = MTP_PHASE_IDLE;
	}
	if ((s_mtp.status & 2) || s_mtp.phase) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return MTP_CANCELLED;
	}
	s_mtp.transferFlags |= MTP_TRANSFER_RECEIVING;
	s_mtp.busFlags &= ~6U;
	s_mtp.cancelled = -1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	unsigned retries = 0;
	for (;;) {
		ret = receiveBulk(s_mtp.data, s_mtp.packetSize);
		if (ret < 0) {
			if (ret == MTP_DISCONNECTED || ret == MTP_CANCELLED)
				goto error;
			if (ret != MTP_TIMEOUT) {
				ret = (int)0x8024300d;
				goto error;
			}
			if (++retries >= 6) {
				ret = MTP_RETRY_LIMIT;
				goto error;
			}
			continue;
		}
		if (!ret)
			continue;
		memset(s_mtp.command, 0, 32);
		memcpy(s_mtp.command, s_mtp.data, ret < 32 ? ret : 32);
		if (s_mtp.command->type == 1) {
			MtpContainer *in = s_mtp.command;
			if (s_mtp.cancelled == in->transaction)
				return MTP_CANCELLED;
			out->transaction = in->transaction;
			out->code = in->code;
			s_mtp.transaction = in->transaction;
			/* Sony copies parameters even when the container length is invalid. */
			unsigned words[5];
			for (unsigned i = 0; i < 5; i++)
				words[i] = in->params[i];
			for (unsigned i = 0; i < 5; i++)
				out->params[i] = words[i];
			unsigned n = in->length - 12;
			if (n >= 21 || (in->length & 3))
				return MTP_PROTOCOL;
			out->flags = 0;
			out->count = n >> 2;
			s_mtp.remaining = s_mtp.received = 0;
			intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
			s_mtp.phase = MTP_PHASE_COMMAND;
			s_mtp.transferFlags &= ~2U;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
			return ret;
		}
		if (s_mtp.command->type == 2) {
			s_mtp.cursor = 12;
			unsigned n = s_mtp.data->length;
			s_mtp.remaining = n >= 12 && n != (unsigned)-1 ? (int64_t)n - 12 : -1;
			s_mtp.staged = ret - 12;
			transfer->totalSize = s_mtp.remaining;
			transfer->size = 0x10000;
			while (transfer->totalSize) {
				ret = sceUsbMtpRecvData(transfer);
				if (ret < 0)
					goto error;
			}
		} else {
			intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
			s_mtp.transferFlags |= MTP_TRANSFER_OUT_STALLED;
			ksceUdcdStall(&s_mtp.endpoints[2]);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		}
	}
error:
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	s_mtp.phase = MTP_PHASE_IDLE;
	s_mtp.transferFlags &= ~2U;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return ret;
}

int SceUsbMtpForDriver_4463009B(const void *response)
{
	const MtpCommand *in = response;
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	if (!in || in->count > 5)
		return MTP_INVALID;
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (in->transaction == s_mtp.cancelled || (s_mtp.status & 2)) {
		if (s_mtp.received <= 0)
			goto ignored;
		if (s_mtp.remaining) {
			/* Emits the same signed 64-bit runtime division helper as Sony. */
			int64_t remainder = s_mtp.received % (int64_t)s_mtp.packetSize;
			if (!remainder || s_mtp.remaining >= s_mtp.packetSize * 2LL)
				goto ignored;
		}
	}
	if (s_mtp.phase != 8) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return MTP_CANCELLED;
	}
	s_mtp.transferFlags |= MTP_TRANSFER_SENDING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	MtpContainer *out = s_mtp.response;
	unsigned n = (in->count + 3) * 4;
	out->type = 3;
	out->length = n;
	out->code = in->code;
	out->transaction = in->transaction;
	for (unsigned i = 0; i < in->count; i++)
		out->params[i] = in->params[i];
	int ret = sendBulk(out, n);
	if (ret >= 0) {
		intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
		s_mtp.phase = MTP_PHASE_IDLE;
		s_mtp.transferFlags &= ~1U;
		s_mtp.deferredResponse = -1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	}
	return ret;
ignored:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return 0;
}

int SceUsbMtpForDriver_5FA86BA4(void *event)
{
	const MtpEvent *in = event;
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	if (!in || in->count > 3)
		return MTP_INVALID;
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (s_mtp.status & 2) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return MTP_CANCELLED;
	}
	if (in->code == 0x4001) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
		return 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	MtpContainer *out = s_mtp.eventBuffer;
	unsigned n = (in->count + 3) * 4;
	out->type = 4;
	out->length = n;
	out->code = in->code;
	out->transaction = in->transaction;
	for (unsigned i = 0; i < in->count; i++)
		out->params[i] = in->params[i];
	SceUdcdDeviceRequest *req = &s_mtp.notify.request;
	if (req->returnCode >= 2)
		return MTP_BUSY;
	req->data = out;
	req->size = n;
	SceCpuForDriver_103872A5(out, (n + 63) & ~63U);
	int ret = ksceUdcdReqSend(req);
	if (ret < 0)
		return ret;
	ret = WaitTransfer(req, &s_mtp.endpoints[3], MTP_EVENT_NOTIFIED, MTP_EVENT_NOTIFY_FAILED, 1);
	if (ret < 0)
		return ret;
	ret = req->transmitted;
	if (in->code != 0xc108)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if (in->transaction == s_mtp.transaction) {
		if (s_mtp.phase == MTP_PHASE_DATA_OUT) {
			if (s_mtp.transferFlags & MTP_TRANSFER_RECEIVING) {
				s_mtp.status |= 2;
				s_mtp.cancelled = in->transaction;
			}
		} else if (s_mtp.phase == MTP_PHASE_DATA_IN && (s_mtp.transferFlags & MTP_TRANSFER_SENDING)) {
			s_mtp.status |= 2;
			s_mtp.cancelPending = 1;
			s_mtp.cancelled = s_mtp.deferredResponse = in->transaction;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return ret;
}

int SceUsbMtpForDriver_151947D2(void)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	if (s_mtp.transferFlags & MTP_TRANSFER_SENDING) {
		s_mtp.transferFlags |= MTP_TRANSFER_IN_STALLED;
		ksceUdcdStall(&s_mtp.endpoints[1]);
	}
	if (s_mtp.transferFlags & MTP_TRANSFER_RECEIVING) {
		s_mtp.transferFlags |= MTP_TRANSFER_OUT_STALLED;
		ksceUdcdStall(&s_mtp.endpoints[2]);
	}
	return 0;
}

int SceUsbMtpForDriver_76D707B1(SceUInt32 timeout)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	uint32_t bits;
	int ret = ksceKernelWaitEventFlag(s_mtp.event, MTP_EVENT_ATTACHED | MTP_EVENT_CONFIGURED, SCE_EVENT_WAITOR, &bits, &timeout);
	if (bits & MTP_EVENT_CONFIGURED)
		return MTP_CANCELLED;
	return ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? MTP_TIMEOUT : ret;
}

int SceUsbMtpForDriver_6F7DF4B8(void)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	unsigned status = s_mtp.status;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return status;
}

int SceUsbMtpForDriver_E64513A9(int phase)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_mtp.lock);
	if ((s_mtp.transferFlags & MTP_TRANSFER_RECEIVING) && s_mtp.phase == MTP_PHASE_DATA_OUT) {
		if (s_mtp.cancelled == (unsigned)-1) {
			if ((s_mtp.received || s_mtp.remaining) && s_mtp.remaining != -1) {
				s_mtp.transferFlags |= MTP_TRANSFER_OUT_STALLED;
				ksceUdcdStall(&s_mtp.endpoints[2]);
				s_mtp.deferredResponse = -1;
				s_mtp.cancelled = s_mtp.transaction;
			}
		} else {
			s_mtp.deferredCancel = s_mtp.transaction;
		}
		s_mtp.transferFlags &= ~2U;
	}
	if (s_mtp.deferredCancel != (unsigned)-1 && !phase) {
		s_mtp.transferFlags |= MTP_TRANSFER_OUT_STALLED;
		ksceUdcdStall(&s_mtp.endpoints[2]);
		s_mtp.deferredResponse = -1;
		s_mtp.deferredCancel = -1;
	}
	s_mtp.phase = phase;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_mtp.lock, intr);
	return 0;
}

int sceUsbMtpStart(void)
{
	if (s_mtp.ready)
		return MTP_BUSY;
	if (ksceKernelWaitEventFlag(s_mtp.event, MTP_EVENT_INITIALIZED, SCE_EVENT_WAITOR, NULL, NULL) < 0)
		return MTP_TIMEOUT;
	const char *driver = "USBDeviceControllerDriver";
	int ret = ksceUdcdStart(driver, 0, 0);
	if (ret < 0 && ret != (int)0x80243002) {
		SCE_DBG_PRINTF(0, 2312,
			"[%s%-28s] Err(%4d) sceUdcdStart() failed. name=%s (0x%x)\n",
			"usbmtp:", __func__, 2312, driver, ret);
		return MTP_PROTOCOL;
	}
	driver = "USB_MTP_Driver";
	ret = ksceUdcdStart(driver, 0, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2318,
			"[%s%-28s] Err(%4d) sceUdcdStart() failed. name=%s (0x%x)\n",
			"usbmtp:", __func__, 2318, driver, ret);
		return MTP_PROTOCOL;
	}
	ret = ksceUdcdActivate(s_mtp.product);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 2324, "[%s%-28s] Err(%4d) sceUdcdActivate() failed (0x%x)\n",
			"usbmtp:", __func__, 2324, ret);
		ksceUdcdStop(driver, 0, 0);
		return MTP_UNAVAILABLE;
	}
	return 0;
}

int SceUsbMtpForDriver_08CD359F(void)
{
	if (!s_mtp.ready)
		return MTP_UNAVAILABLE;
	ksceUdcdDeactivate();
	if (ksceUdcdStop("USB_MTP_Driver", 0, 0) < 0)
		return MTP_PROTOCOL;
	ksceUdcdStop("USBDeviceControllerDriver", 0, 0);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_SEND_FAILED);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_RECV_FAILED);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_NOTIFY_FAILED);
	ksceKernelSetEventFlag(s_mtp.event, MTP_EVENT_CONFIGURED);
	s_mtp.transferFlags &= ~(MTP_TRANSFER_SENDING | MTP_TRANSFER_RECEIVING |
				 MTP_TRANSFER_IN_STALLED | MTP_TRANSFER_OUT_STALLED);
	s_mtp.busFlags &= ~7U;
	s_mtp.status &= ~1U;
	ksceKernelClearEventFlag(s_mtp.event, ~1U);
	ksceKernelClearEventFlag(s_mtp.event, ~MTP_EVENT_CONFIGURED);
	return 0;
}

int _sceUsbMtpModuleStart(unsigned arglen, const char *argp)
{
	memset(&s_mtp.port, 0, offsetof(struct UsbMtp, interface) - offsetof(struct UsbMtp, port));
	s_mtp.port = 2;
	s_mtp.product = 0x4e4;
	s_mtp.event = ksceKernelCreateEventFlag("SceUsbMtp", 0x9000, 0, 0);
	if (s_mtp.event < 0) {
		SCE_DBG_PRINTF(0, 2570, "[%s%-28s] Err(%4d) cannot create event flag\n",
			"usbmtp:", __func__, 2570);
		return 2;
	}
	s_mtp.compatibleId = s_compatibleId;
	s_mtp.driver.driverName = "USB_MTP_Driver";
	s_mtp.driver.numEndpoints = 4;
	s_mtp.driver.endpoints = s_mtp.endpoints;
	s_mtp.driver.interface = &s_mtp.interface;
	s_mtp.driver.stringDescriptors = 0;
	s_mtp.driver.stringDescriptorProduct = (SceUdcdStringDescriptor *)&s_productString;
	s_mtp.driver.stringDescriptorSerial = &s_mtp.serialString;
	s_mtp.driver.processRequest = processRequest;
	s_mtp.driver.changeSetting = changeSetting;
	s_mtp.driver.attach = attach;
	s_mtp.driver.detach = detach;
	s_mtp.driver.configure = configure;
	s_mtp.driver.start = Start;
	s_mtp.driver.stop = Stop;
	s_mtp.endpoints[0].driverEndpointNumber = 0;
	s_mtp.endpoints[1].direction = 0x80;
	s_mtp.endpoints[1].driverEndpointNumber = 1;
	s_mtp.endpoints[2].direction = 0;
	s_mtp.endpoints[2].driverEndpointNumber = 2;
	s_mtp.endpoints[3].direction = 0x80;
	s_mtp.endpoints[3].driverEndpointNumber = 3;
	s_mtp.interface.expectNumber = -1;
	s_mtp.interface.numInterfaces = 1;
	int offset = 0;
	const char *arg = argp;
	if ((int)arglen > 0) {
		for (;;) {
			offset += strnlen(arg, 256) + 1;
			if ((int)arglen <= offset)
				break;
			arg = argp + offset;
			if (!strncmp(arg, "port=0", 6))
				s_mtp.port = 0;
			if (!strncmp(arg, "port=1", 6))
				s_mtp.port = 1;
			if (!strncmp(arg, "port=2", 6))
				s_mtp.port = 2;
			if (!strncmp(arg, "debug", 5))
				s_mtp.port = 1;
		}
	}
	s_mtp.lock = 0;
	if (ksceUdcdRegister(&s_mtp.driver) < 0) {
		SCE_DBG_PRINTF(0, 2594, "[%s%-28s] Err(%4d) cannot register protocol driver\n",
			"usbmtp:", __func__, 2594);
		ksceKernelDeleteEventFlag(s_mtp.event);
		return 2;
	}
	ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceUsbMtpInit", _sceUsbMtpInitTask, 0);
	return 0;
}
