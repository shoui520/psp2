/*
 * SceUsbDevSerial: a USB CDC ACM serial function, with a transmit and a
 * receive ring between the syscalls and the bulk endpoints.
 */
#include <stdint.h>
#include <sysmem/sysmem.h>
#include <udcd/udcd.h>
#include <sysmem/cpu.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <excpmgr/excpmgr_cpu.h>
#include <kernel/error.h>
#include "usbdev_serial.h"

#define TX_SIZE		0x4000
#define RX_SIZE		0x1000
#define TX_CHUNK	0x2000		/* the most one bulk-in request carries */

#define SERIAL_MODE_ERROR 0x80243300	/* original error name unknown */

/* the event flag */
#define SERIAL_EVENT_RX_READY		0x0001	/* the receive ring holds data */
#define SERIAL_EVENT_TX_EMPTY		0x0002	/* the transmit ring is drained */
#define SERIAL_EVENT_NOTIFY_IDLE	0x0004	/* no SERIAL_STATE notification in flight */
#define SERIAL_EVENT_CANCEL		0x0008	/* a waiting read returns WAIT_CANCEL */
#define SERIAL_EVENT_DTR_ON		0x0010	/* SET_CONTROL_LINE_STATE: DTR set */
#define SERIAL_EVENT_DTR_OFF		0x0020	/* SET_CONTROL_LINE_STATE: DTR clear */
#define SERIAL_EVENT_RESERVED		0x0040	/* cleared on detach and close; never set */
#define SERIAL_EVENT_RETRY		0x0080	/* a waiter that sees it waits again */
#define SERIAL_EVENT_CLOSED		0x0100
#define SERIAL_EVENT_DETACHED		0x0200

/* CDC class requests and notifications */
#define CDC_SET_LINE_CODING		0x20
#define CDC_GET_LINE_CODING		0x21
#define CDC_SET_CONTROL_LINE_STATE	0x22
#define CDC_SEND_BREAK			0x23
#define CDC_NOTIFY_SERIAL_STATE		0x20

#define CDC_LINE_STATE_DTR		0x0001
#define CDC_SERIAL_STATE_DCD		0x0001
#define CDC_SERIAL_STATE_DSR		0x0002

#define USB_TYPE_MASK			0x60
#define USB_TYPE_CLASS			0x20

/* 918D359E commands (names unknown) */
#define SERIAL_COMMAND_SET_TIMEOUT	0x3517001
#define SERIAL_COMMAND_8		0x3517008

typedef struct CdcLineCoding {
	uint32_t dwDTERate;
	uint8_t bCharFormat;
	uint8_t bParityType;
	uint8_t bDataBits;
} __attribute__((packed)) CdcLineCoding;

typedef struct CdcNotificationHeader {
	uint8_t bmRequestType;
	uint8_t bNotification;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} CdcNotificationHeader;

typedef struct CdcNotification {
	CdcNotificationHeader header;
	uint16_t data;
} CdcNotification;

/* the bus driver uses the first 0x2c bytes; the rest pads it to 0x40 */
typedef struct SerialRequest {
	SceUdcdDeviceRequest request;
	uint8_t reserved[0x14];
} SerialRequest;

typedef struct SerialInterfaceSettings {
	SceUdcdInterfaceSettings settings[2];
} SerialInterfaceSettings;

typedef struct SerialInterfaceDescriptors {
	SceUdcdInterfaceDescriptor descriptor[3];
} SerialInterfaceDescriptors;

typedef struct SerialEndpointDescriptors {
	SceUdcdEndpointDescriptor descriptor[4];
} SerialEndpointDescriptors;

/* one speed's descriptor tree */
typedef struct SerialDescriptors {
	SceUdcdDeviceDescriptor device;
	SceUdcdConfiguration config;
	SceUdcdConfigDescriptor descriptor;
	SerialInterfaceSettings settings;
	SerialInterfaceDescriptors interfaces;
	SerialEndpointDescriptors endpoints;
} SerialDescriptors;

/* the memblock */
typedef struct SerialMemory {
	SerialDescriptors high;
	SerialDescriptors full;
	uint8_t txRing[TX_SIZE];
	uint8_t rxRing[RX_SIZE];
	uint8_t buffers[];		/* SerialBuffers, cache-line aligned */
} SerialMemory;

typedef struct SerialBuffers {
	uint8_t control[0x40];
	uint8_t transmit[TX_CHUNK];
	uint8_t receive[0x200];
	CdcNotification notification;
} SerialBuffers;

#define SERIAL_CACHE_LINE	64
#define SERIAL_ALIGN(p)	((void *)(((uintptr_t)(p) + SERIAL_CACHE_LINE - 1) & ~(SERIAL_CACHE_LINE - 1)))

_Static_assert(sizeof(SerialDescriptors) == 0xdc, "descriptor tree layout");
_Static_assert(sizeof(SerialRequest) == 0x40, "request layout");

static int Start(int size, void *args, void *ctx);
static int Stop(int size, void *args, void *ctx);
static int DevReqHdlr(int recipient, int index, SceUdcdEP0DeviceRequest *req, void *ctx);
static int Configured(int version, void *ctx);
static void detachSerial(void *ctx);

static struct SerialDriver {
	SceUdcdInterface interface;
	SceUdcdDriver driver;
	uint8_t cdcExtra[20];
	int bus;
	SceUdcdStringDescriptor strings[2];
	SceUdcdEndpoint endpoints[4];
} s_config = {
	.interface = {
		.expectNumber = 0,
		.interfaceNumber = 0,
		.numInterfaces = 2,
	},
	.driver = {
		.driverName = "USBSerialDriver",
		.numEndpoints = 4,
		.endpoints = s_config.endpoints,
		.interface = &s_config.interface,
		.stringDescriptors = s_config.strings,
		.processRequest = DevReqHdlr,
		.attach = Configured,
		.detach = detachSerial,
		.start = Start,
		.stop = Stop,
	},
	/* CDC functional descriptors: header, call management, ACM, union */
	.cdcExtra = {
		0x05, 0x24, 0x00, 0x10, 0x01,
		0x04, 0x24, 0x02, 0x06,
		0x05, 0x24, 0x06, 0x00, 0x01,
		0x05, 0x24, 0x01, 0x00, 0x01,
	},
	.bus = 2,
	.strings = {
		{ 22, 3, { 'U', 'S', 'B', ' ', 'S', 'e', 'r', 'i', 'a', 'l' } },
	},
	.endpoints = {
		{ .direction = 0x00, .driverEndpointNumber = 0 },
		{ .direction = 0x80, .driverEndpointNumber = 1 },	/* interrupt in */
		{ .direction = 0x00, .driverEndpointNumber = 2 },	/* bulk out */
		{ .direction = 0x80, .driverEndpointNumber = 3 },	/* bulk in */
	},
};

static struct SerialState {
	int attached;
	int openCount;
	int requireCarrier;		/* reads and writes fail once DTR drops */
	CdcLineCoding incomingLineCoding;
	CdcLineCoding outgoingLineCoding;
	uint16_t serialState;		/* CDC_SERIAL_STATE_* */
	unsigned int controlCommand;	/* the class request in its data stage */
	int notificationPending;
	SerialRequest control;
	SerialRequest transmit;
	SerialRequest receive;
	SerialRequest notification;
	void *controlBuffer;
	void *transmitBuffer;
	void *receiveBuffer;
	CdcNotification *notificationBuffer;
	uint8_t *txRing;
	unsigned int txWrite;
	unsigned int txRead;
	uint8_t *rxRing;
	unsigned int rxWrite;
	unsigned int rxRead;
	int timeout;
	SceUID event;
	SceUID memblock;
	int rxActive;
	int txActive;
	SceKernelSpinlock lock;
} s_serial __attribute__((aligned(16)));

_Static_assert(sizeof(s_config) == 0x138, "driver table layout");
_Static_assert(sizeof(s_serial) == 0x164, "serial state layout");

/*
 * Names unknown: every syscall keeps TPIDRURO (the caller's context word)
 * shifted while it runs and restores it on the way out.
 */
static inline unsigned int serialEnter(void)
{
	unsigned int context = readTpidruro();

	writeTpidruro(context << 16);
	return context;
}

static inline void serialLeave(unsigned int context)
{
	writeTpidruro(context);
}

static inline unsigned int txPending(void)
{
	return (s_serial.txWrite - s_serial.txRead) & (TX_SIZE - 1);
}

static inline unsigned int rxPending(void)
{
	return (s_serial.rxWrite - s_serial.rxRead) & (RX_SIZE - 1);
}

static inline unsigned int rxFree(void)
{
	return (s_serial.rxRead - s_serial.rxWrite - 1) & (RX_SIZE - 1);
}

/* the next bulk-in transfer: contiguous in the ring, at most TX_CHUNK */
static inline unsigned int transmitSize(void)
{
	unsigned int size = txPending();

	if ((int)s_serial.txWrite <= (int)s_serial.txRead && TX_SIZE - s_serial.txRead <= size)
		size = TX_SIZE - s_serial.txRead;
	return size < TX_CHUNK ? size : TX_CHUNK;
}

/* send the serial state now, or once the notification in flight completes */
static inline void SendSerialState(void)
{
	if (s_serial.notification.request.returnCode > 0) {
		s_serial.notificationPending = 1;
		return;
	}

	sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_NOTIFY_IDLE);
	s_serial.notificationPending = 0;
	s_serial.notificationBuffer->header = (CdcNotificationHeader){
		.bmRequestType = 0xa1,
		.bNotification = CDC_NOTIFY_SERIAL_STATE,
		.wValue = 0,
		.wIndex = s_config.interface.interfaceNumber,
		.wLength = sizeof(s_serial.notificationBuffer->data),
	};
	s_serial.notificationBuffer->data = s_serial.serialState;
	SceUdcdForDriver_2E3E622A(&s_serial.notification.request, s_config.bus);
}

static void completeControl(SceUdcdDeviceRequest *req)
{
	if (req->returnCode == 0 && s_serial.controlCommand == CDC_SET_LINE_CODING)
		s_serial.incomingLineCoding = *(CdcLineCoding *)s_serial.control.request.data;
	s_serial.controlCommand = 0;
}

static int Stop(int size, void *args, void *ctx)
{
	sceKernelDeleteEventFlag(s_serial.event);
	SceSysmemForDriver_009E1C61(s_serial.memblock);
	return 0;
}

static void detachSerial(void *ctx)
{
	s_serial.attached = 0;
	s_serial.openCount = 0;
	s_serial.controlCommand = 0;
	s_serial.notificationPending = 0;
	s_serial.txWrite = s_serial.txRead = 0;
	s_serial.rxWrite = s_serial.rxRead = 0;
	for (unsigned int i = 0; i < 4; i++)
		s_config.endpoints[i].transmittedBytes = 0;
	s_serial.serialState = 0;
	sceKernelClearEventFlag(s_serial.event,
				 ~(SERIAL_EVENT_RX_READY | SERIAL_EVENT_NOTIFY_IDLE | SERIAL_EVENT_CANCEL |
				   SERIAL_EVENT_DTR_ON | SERIAL_EVENT_RESERVED));
	sceKernelSetEventFlag(s_serial.event,
			       SERIAL_EVENT_DETACHED | SERIAL_EVENT_DTR_OFF | SERIAL_EVENT_TX_EMPTY);
}

static int DevReqHdlr(int recipient, int index, SceUdcdEP0DeviceRequest *req, void *ctx)
{
	if (index < 0 || (req->bmRequestType & USB_TYPE_MASK) != USB_TYPE_CLASS)
		return -1;

	s_serial.controlCommand = req->bRequest;
	switch (req->bRequest) {
	case CDC_SET_LINE_CODING:
		s_serial.control.request.size = 64;
		SceUdcdForDriver_175E6179(&s_serial.control.request, s_config.bus);
		return 0;
	case CDC_GET_LINE_CODING:
		*(CdcLineCoding *)s_serial.control.request.data = s_serial.outgoingLineCoding;
		s_serial.control.request.size = req->wLength < 64 ? req->wLength : 64;
		SceUdcdForDriver_2E3E622A(&s_serial.control.request, s_config.bus);
		return 0;
	case CDC_SET_CONTROL_LINE_STATE:
		if (req->wValue & CDC_LINE_STATE_DTR) {
			sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_DTR_OFF);
			sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_DTR_ON);
		} else {
			sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_DTR_ON);
			sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_DTR_OFF);
		}
		return 0;
	case CDC_SEND_BREAK:
		return 0;
	default:
		return -1;
	}
}

static void BulkINDone(SceUdcdDeviceRequest *req)
{
	SceKernelIntrStatus state;

	if (req->returnCode < 0)
		return;

	state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	s_serial.txActive = 0;
	if (req->transmitted)
		s_serial.txRead = (s_serial.txRead + req->transmitted) & (TX_SIZE - 1);
	if (s_serial.attached && s_serial.openCount) {
		if (txPending() == 0) {
			sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_TX_EMPTY);
		} else {
			req->size = transmitSize();
			memcpy(req->data, s_serial.txRing + s_serial.txRead, req->size);
			if (SceUdcdForDriver_2E3E622A(req, s_config.bus) >= 0)
				s_serial.txActive = 1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
}

static void BulkOUTDone(SceUdcdDeviceRequest *req)
{
	SceKernelIntrStatus state;
	unsigned int received, tail;

	if (req->returnCode < 0)
		return;

	state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	s_serial.rxActive = 0;
	received = req->transmitted;
	if (received) {
		tail = RX_SIZE - s_serial.rxWrite;
		if (received < tail) {
			memcpy(s_serial.rxRing + s_serial.rxWrite, req->data, received);
		} else {
			memcpy(s_serial.rxRing + s_serial.rxWrite, req->data, tail);
			memcpy(s_serial.rxRing, (uint8_t *)req->data + tail, req->transmitted - tail);
		}
		s_serial.rxWrite = (s_serial.rxWrite + req->transmitted) & (RX_SIZE - 1);
		sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_RX_READY);
	}
	if (s_serial.attached && s_serial.openCount && (unsigned int)req->size <= rxFree() &&
	    SceUdcdForDriver_175E6179(req, s_config.bus) >= 0)
		s_serial.rxActive = 1;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
}

static const SceUdcdDeviceDescriptor s_deviceDescriptor = {
	.bLength = sizeof(SceUdcdDeviceDescriptor),
	.bDescriptorType = 1,
	.bcdUSB = 0x200,
	.bDeviceClass = 2,			/* communications */
	.bcdDevice = 0x100,
	.bNumConfigurations = 1,
};

static const SceUdcdConfigDescriptor s_configDescriptor = {
	.bLength = 9,
	.bDescriptorType = 2,
	.wTotalLength = 0x43,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.bmAttributes = 0xc0,			/* self powered */
};

static const SerialInterfaceSettings s_interfaceSettings = {
	.settings = {
		{ .numDescriptors = 1 },
		{ .numDescriptors = 1 },
	},
};

static const SerialInterfaceDescriptors s_interfaceDescriptors = {
	.descriptor = {
		{	/* communications class, abstract control model */
			.bLength = 9,
			.bDescriptorType = 4,
			.bInterfaceNumber = 0,
			.bNumEndpoints = 1,
			.bInterfaceClass = 2,
			.bInterfaceSubClass = 2,
			.bInterfaceProtocol = 1,
			.extra = s_config.cdcExtra,
			.extraLength = 19,
		},
		{	/* data class */
			.bLength = 9,
			.bDescriptorType = 4,
			.bInterfaceNumber = 1,
			.bNumEndpoints = 2,
			.bInterfaceClass = 10,
			.iInterface = 1,
		},
	},
};

static const SerialEndpointDescriptors s_endpointDescriptors = {
	.descriptor = {
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x81, .bmAttributes = 3 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x02, .bmAttributes = 2 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x83, .bmAttributes = 2 },
	},
};

static inline void initializeDescriptors(SerialDescriptors *d, unsigned int interval,
					 unsigned int packetSize)
{
	d->device = s_deviceDescriptor;
	d->config.configDescriptors = &d->descriptor;
	d->config.settings = d->settings.settings;
	d->config.interfaceDescriptors = d->interfaces.descriptor;
	d->config.endpointDescriptors = d->endpoints.descriptor;
	d->descriptor = s_configDescriptor;
	d->settings = s_interfaceSettings;
	d->interfaces = s_interfaceDescriptors;
	d->endpoints = s_endpointDescriptors;
	d->device.bMaxPacketSize0 = 64;
	d->descriptor.settings = d->settings.settings;
	d->settings.settings[0].descriptors = &d->interfaces.descriptor[0];
	d->settings.settings[1].descriptors = &d->interfaces.descriptor[1];
	d->interfaces.descriptor[0].endpoints = &d->endpoints.descriptor[0];
	d->interfaces.descriptor[1].endpoints = &d->endpoints.descriptor[1];
	d->endpoints.descriptor[0].wMaxPacketSize = 64;
	d->endpoints.descriptor[0].bInterval = interval;
	d->endpoints.descriptor[1].wMaxPacketSize = packetSize;
	d->endpoints.descriptor[2].wMaxPacketSize = packetSize;
}

static inline void initializeRequest(SerialRequest *req, SceUdcdEndpoint *endpoint, void *data,
				     int size, int isControlRequest,
				     void (*onComplete)(SceUdcdDeviceRequest *req))
{
	req->request.endpoint = endpoint;
	req->request.data = data;
	req->request.attributes = 0;
	req->request.size = size;
	req->request.isControlRequest = isControlRequest;
	req->request.onComplete = onComplete;
	req->request.transmitted = 0;
	req->request.returnCode = 0;
	req->request.next = NULL;
	req->request.unused = NULL;
	req->request.physicalAddress = NULL;
}

static void completeNotification(SceUdcdDeviceRequest *req);

static int Start(int size, void *args, void *ctx)
{
	SceKernelAllocMemBlockKernelOpt opt;
	SerialMemory *memory = NULL;
	SerialBuffers *buffers;
	int ret;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s_serial.memblock = SceSysmemForDriver_C94850C9("SceUsbDevSerial",
						    SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
						    0x8000, &opt);
	if (s_serial.memblock < 0)
		return s_serial.memblock;
	ret = SceSysmemForDriver_A841EDDA(s_serial.memblock, (void **)&memory);
	if (ret < 0)
		goto fail;

	s_config.driver.descriptor_hi = &memory->high.device;
	s_config.driver.configuration_hi = &memory->high.config;
	s_config.driver.descriptor = &memory->full.device;
	s_config.driver.configuration = &memory->full.config;
	initializeDescriptors(&memory->high, 7, 512);
	initializeDescriptors(&memory->full, 8, 64);

	buffers = SERIAL_ALIGN(memory->buffers);
	s_serial.controlBuffer = buffers->control;
	s_serial.transmitBuffer = buffers->transmit;
	s_serial.receiveBuffer = buffers->receive;
	s_serial.notificationBuffer = &buffers->notification;
	s_serial.txRing = memory->txRing;
	s_serial.rxRing = memory->rxRing;

	s_serial.event = sceKernelCreateEventFlag("SceUsbSerial", SCE_KERNEL_EVF_ATTR_MULTI,
						   SERIAL_EVENT_DETACHED | SERIAL_EVENT_CLOSED, NULL);
	if (s_serial.event <= 0)
		goto fail;

	initializeRequest(&s_serial.control, &s_config.endpoints[0], s_serial.controlBuffer,
			  64, 1, completeControl);
	initializeRequest(&s_serial.transmit, &s_config.endpoints[3], s_serial.transmitBuffer,
			  TX_CHUNK, 1, BulkINDone);
	initializeRequest(&s_serial.receive, &s_config.endpoints[2], s_serial.receiveBuffer,
			  512, 0, BulkOUTDone);
	initializeRequest(&s_serial.notification, &s_config.endpoints[1],
			  s_serial.notificationBuffer, sizeof(CdcNotification), 0,
			  completeNotification);
	for (unsigned int i = 0; i < 4; i++)
		s_config.endpoints[i].transmittedBytes = 0;

	s_serial.attached = 0;
	s_serial.openCount = 0;
	s_serial.serialState = 0;
	s_serial.controlCommand = 0;
	s_serial.notificationPending = 0;
	s_serial.outgoingLineCoding.dwDTERate = 115200;
	s_serial.outgoingLineCoding.bCharFormat = 1;
	s_serial.outgoingLineCoding.bParityType = 0;
	s_serial.outgoingLineCoding.bDataBits = 8;
	s_serial.txWrite = s_serial.txRead = 0;
	s_serial.rxWrite = s_serial.rxRead = 0;
	s_serial.requireCarrier = 0;
	s_serial.lock = 0;
	s_serial.timeout = -1;
	return 0;

fail:
	SceSysmemForDriver_009E1C61(s_serial.memblock);
	return -1;
}

static void completeNotification(SceUdcdDeviceRequest *req)
{
	if (req->returnCode != 0)
		return;

	if (s_serial.notificationPending)
		SendSerialState();
	else
		sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_NOTIFY_IDLE);
}

/* raise DCD/DSR, empty both rings and start receiving */
static inline int StartTransfer(void)
{
	int ret;

	s_serial.serialState |= CDC_SERIAL_STATE_DCD | CDC_SERIAL_STATE_DSR;
	SceUdcdForDriver_38787672(&s_config.endpoints[3]);
	SceUdcdForDriver_38787672(&s_config.endpoints[1]);
	sceKernelClearEventFlag(s_serial.event, ~(SERIAL_EVENT_CLOSED | SERIAL_EVENT_RX_READY));
	sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_TX_EMPTY);
	SendSerialState();
	s_serial.txWrite = s_serial.txRead = 0;
	s_serial.rxWrite = s_serial.rxRead = 0;
	ret = SceUdcdForDriver_175E6179(&s_serial.receive.request, s_config.bus);
	if (ret >= 0)
		s_serial.rxActive = 1;
	return ret;
}

static int Configured(int version, void *ctx)
{
	int ret;

	s_serial.attached = 1;
	s_serial.receive.request.size = version == 2 ? 512 : 64;
	ret = sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_DETACHED);
	if (s_serial.openCount > 0)
		ret = StartTransfer();
	return ret;
}

/* arguments: the bus, "1" or "2" */
int _sceUsbDevSerialModuleStart(SceSize arglen, const void *argp)
{
	const char *argv[8];
	SceSize offset = 0;
	int argc = 0;
	int ret;

	while (offset < arglen && argc < 7) {
		argv[argc++] = (const char *)argp + offset;
		offset += strnlen((const char *)argp + offset, 256) + 1;
	}
	argv[argc] = NULL;

	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "1", 32))
			s_config.bus = 1;
		else if (!strncmp(argv[i], "2", 32))
			s_config.bus = 2;
		else
			return SCE_KERNEL_START_FAILED;
	}

	ret = SceUdcdForDriver_64675918(&s_config.driver, s_config.bus);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	s_config.driver.user_data = NULL;
	return ret;
}

int _sceUsbDevSerialModuleStop(SceSize arglen, const void *argp)
{
	if (SceUdcdForDriver_6CCD152E(&s_config.driver, s_config.bus) < 0)
		return SCE_KERNEL_STOP_FAIL;
	return SCE_KERNEL_STOP_SUCCESS;
}

/* sceUsbDevSerialInit (0.990): start the bus and serial drivers and activate */
int SceUsbDevSerial_DAE5C362(unsigned int mode)
{
	unsigned int context = serialEnter();
	int ret;

	if (mode >= 2) {
		ret = SERIAL_MODE_ERROR;
	} else {
		ret = SceUdcdForDriver_90F8BCAE("USBDeviceControllerDriver", 0, NULL, s_config.bus);
		if (ret >= 0) {
			ret = SceUdcdForDriver_90F8BCAE("USBSerialDriver", 0, NULL, s_config.bus);
			if (ret >= 0) {
				ret = SceUdcdForDriver_9119247B(0xffff, mode, s_config.bus);
				if (ret < 0)
					SceUdcdForDriver_150FD3BB("USBSerialDriver", 0, NULL, s_config.bus);
			}
			if (ret < 0)
				SceUdcdForDriver_150FD3BB("USBDeviceControllerDriver", 0, NULL, s_config.bus);
		}
	}
	serialLeave(context);
	return ret;
}

/* sceUsbDevSerialFinalize (0.990) */
int SceUsbDevSerial_9F497D4E(void)
{
	unsigned int context = serialEnter();
	int serial, controller, ret;

	SceUdcdForDriver_FDC10F93(s_config.bus);
	serial = SceUdcdForDriver_150FD3BB("USBSerialDriver", 0, NULL, s_config.bus);
	controller = SceUdcdForDriver_150FD3BB("USBDeviceControllerDriver", 0, NULL, s_config.bus);
	serialLeave(context);
	if (controller < 0)
		return 1;
	return serial < 0;
}

/* the bus device state */
int SceUsbDevSerial_C0FDECAF(void)
{
	unsigned int context = serialEnter();
	int ret = SceUdcdForDriver_409DD196(s_config.bus);

	serialLeave(context);
	return ret;
}

/* open */
int SceUsbDevSerial_6F2B3960(void)
{
	unsigned int context = serialEnter();
	SceKernelIntrStatus state = SceCpuForDriver_D32ACE9E(&s_serial.lock);

	if (s_serial.attached && s_serial.openCount == 0)
		StartTransfer();
	s_serial.openCount++;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
	serialLeave(context);
	return 0;
}

/* close */
int SceUsbDevSerial_F0C3F3D4(void)
{
	unsigned int context = serialEnter();
	SceKernelIntrStatus state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	int ret = 0;

	if (s_serial.openCount == 0) {
		ret = SCE_ERROR_ERRNO_EBADF;
	} else if (--s_serial.openCount < 1) {
		if (s_serial.attached) {
			s_serial.serialState &= ~(CDC_SERIAL_STATE_DCD | CDC_SERIAL_STATE_DSR);
			SendSerialState();
			sceKernelSetEventFlag(s_serial.event, SERIAL_EVENT_CLOSED);
			sceKernelClearEventFlag(s_serial.event,
						 ~(SERIAL_EVENT_RESERVED | SERIAL_EVENT_CANCEL));
			SceUdcdForDriver_38787672(&s_config.endpoints[2]);
		}
		s_serial.openCount = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
	serialLeave(context);
	return ret;
}

/* DevFileRead (0.990) */
int SceUsbDevSerial_57FBB08C(void *buffer, unsigned int size)
{
	unsigned int context = serialEnter();
	unsigned int bits, pending, tail, pattern;
	SceKernelIntrStatus state;
	SceUInt32 timeout, *timeoutp;
	int ret, requireCarrier;

	if (buffer == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	ret = size;
	if (size == 0)
		goto out;

	state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (!s_serial.attached || !s_serial.openCount) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto unlock;
	}
	pending = rxPending();
	if (pending == 0) {
		sceKernelPollEventFlag(s_serial.event, ~0U, SCE_KERNEL_EVF_WAITMODE_OR, &bits);
		if (s_serial.requireCarrier == 1 && (bits & SERIAL_EVENT_DTR_OFF)) {
			ret = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
			goto unlock;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);

		timeoutp = s_serial.timeout < 0 ? NULL : &timeout;
		if (timeoutp)
			timeout = s_serial.timeout;
		do {
			requireCarrier = s_serial.requireCarrier == 1;
			pattern = SERIAL_EVENT_DETACHED | SERIAL_EVENT_CLOSED | SERIAL_EVENT_RETRY |
				  SERIAL_EVENT_CANCEL | SERIAL_EVENT_RX_READY;
			if (requireCarrier)
				pattern |= SERIAL_EVENT_DTR_OFF;
			ret = SceThreadmgrForDriver_8A35F714(s_serial.event, pattern,
							SCE_KERNEL_EVF_WAITMODE_OR,
							&bits, timeoutp);
			if (ret < 0)
				goto out;
			if (bits & (SERIAL_EVENT_DETACHED | SERIAL_EVENT_CLOSED)) {
				ret = SCE_ERROR_ERRNO_ENODEV;
				goto out;
			}
			if (requireCarrier && (bits & SERIAL_EVENT_DTR_OFF)) {
				ret = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
				goto out;
			}
			if (bits & SERIAL_EVENT_CANCEL) {
				sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_CANCEL);
				ret = SCE_KERNEL_ERROR_WAIT_CANCEL;
				goto out;
			}
		} while (bits & SERIAL_EVENT_RETRY);

		state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
		pending = rxPending();
		sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_RX_READY);
	}

	if (pending < size)
		size = pending;
	tail = RX_SIZE - s_serial.rxRead;
	if (tail < size) {
		ret = SceSysmemForDriver_6D88EF8A(buffer, s_serial.rxRing + s_serial.rxRead, tail);
		if (ret < 0)
			goto unlock;
		ret = SceSysmemForDriver_6D88EF8A((uint8_t *)buffer + tail, s_serial.rxRing, size - tail);
	} else {
		ret = SceSysmemForDriver_6D88EF8A(buffer, s_serial.rxRing + s_serial.rxRead, size);
	}
	if (ret < 0)
		goto unlock;
	s_serial.rxRead = (s_serial.rxRead + size) & (RX_SIZE - 1);
	if (!s_serial.rxActive && (unsigned int)s_serial.receive.request.size <= rxFree() &&
	    SceUdcdForDriver_175E6179(&s_serial.receive.request, s_config.bus) >= 0)
		s_serial.rxActive = 1;
	ret = size;

unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
out:
	serialLeave(context);
	return ret;
}

/* DevFileWrite (0.990) */
int SceUsbDevSerial_4437FF2D(const void *buffer, unsigned int size)
{
	unsigned int context = serialEnter();
	unsigned int bits, space, tail, pattern;
	SceKernelIntrStatus state;
	int ret, requireCarrier;

	if (buffer == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	ret = size;
	if (size == 0)
		goto out;

	state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (!s_serial.attached || !s_serial.openCount) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto unlock;
	}
	sceKernelPollEventFlag(s_serial.event, ~0U, SCE_KERNEL_EVF_WAITMODE_OR, &bits);
	if (s_serial.requireCarrier == 1 && (bits & SERIAL_EVENT_DTR_OFF)) {
		ret = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
		goto unlock;
	}
	if (!(bits & SERIAL_EVENT_NOTIFY_IDLE)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
		do {
			requireCarrier = s_serial.requireCarrier == 1;
			pattern = SERIAL_EVENT_DETACHED | SERIAL_EVENT_CLOSED | SERIAL_EVENT_RETRY |
				  SERIAL_EVENT_NOTIFY_IDLE;
			if (requireCarrier)
				pattern |= SERIAL_EVENT_DTR_OFF;
			ret = SceThreadmgrForDriver_8A35F714(s_serial.event, pattern,
							SCE_KERNEL_EVF_WAITMODE_OR,
							&bits, NULL);
			if (ret < 0)
				goto out;
			if (bits & (SERIAL_EVENT_DETACHED | SERIAL_EVENT_CLOSED)) {
				ret = SCE_ERROR_ERRNO_ENODEV;
				goto out;
			}
			if (requireCarrier && (bits & SERIAL_EVENT_DTR_OFF)) {
				ret = SCE_UDCD_ERROR_BUS_DRIVER_NOT_STARTED;
				goto out;
			}
		} while (bits & SERIAL_EVENT_RETRY);
		state = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	}

	space = (s_serial.txRead - s_serial.txWrite - 1) & (TX_SIZE - 1);
	ret = space;
	if (space == 0)
		goto unlock;
	if (space < size)
		size = space;
	tail = TX_SIZE - s_serial.txWrite;
	if (tail < size) {
		ret = SceSysmemForDriver_BC996A7A(s_serial.txRing + s_serial.txWrite, buffer, tail);
		if (ret < 0)
			goto unlock;
		ret = SceSysmemForDriver_BC996A7A(s_serial.txRing, (const uint8_t *)buffer + tail,
					     size - tail);
	} else {
		ret = SceSysmemForDriver_BC996A7A(s_serial.txRing + s_serial.txWrite, buffer, size);
	}
	if (ret < 0)
		goto unlock;
	s_serial.txWrite = (s_serial.txWrite + size) & (TX_SIZE - 1);
	sceKernelClearEventFlag(s_serial.event, ~SERIAL_EVENT_TX_EMPTY);
	if (!s_serial.txActive) {
		s_serial.transmit.request.size = transmitSize();
		memcpy(s_serial.transmit.request.data, s_serial.txRing + s_serial.txRead,
		       s_serial.transmit.request.size);
		if (SceUdcdForDriver_2E3E622A(&s_serial.transmit.request, s_config.bus) >= 0)
			s_serial.txActive = 1;
	}
	ret = size;

unlock:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, state);
out:
	serialLeave(context);
	return ret;
}

/* DevFileIoctl (0.990) */
int SceUsbDevSerial_918D359E(unsigned int command, void *buffer, unsigned int size)
{
	unsigned int context = serialEnter();
	int ret = SCE_ERROR_ERRNO_EINVAL;

	if (command != SERIAL_COMMAND_8 && command != SERIAL_COMMAND_SET_TIMEOUT &&
	    (!s_serial.attached || !s_serial.openCount))
		ret = SCE_ERROR_ERRNO_ENODEV;
	else if (command == SERIAL_COMMAND_SET_TIMEOUT && buffer && size == sizeof(s_serial.timeout))
		ret = SceSysmemForDriver_BC996A7A(&s_serial.timeout, buffer, sizeof(s_serial.timeout));
	serialLeave(context);
	return ret;
}
