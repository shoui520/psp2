/* SceUsbSerial: boot USB CDC serial driver. Private names are unknown. */
#include "usbserial.h"
#include <intrmgr/syscall.h>
#include <stddef.h>

/* Error names are unknown: these values are absent from the SDK error index. */
#define SERIAL_NOT_STARTED ((int)0x80244401)
#define SERIAL_INVALID_ARGUMENT ((int)0x80244402)
#define SERIAL_BUSY ((int)0x80244403)
#define SERIAL_ALREADY_STARTED ((int)0x80244404)
#define SERIAL_NOT_OPEN ((int)0x80244405)
#define SERIAL_DISCONNECTED ((int)0x80244406)
#define SERIAL_TIMEOUT ((int)0x80244407)
#define SERIAL_CANCELLED ((int)0x80244408)

/* s_serial.flags */
#define SERIAL_STARTED		0x001	/* the driver is started */
#define SERIAL_ATTACHED		0x002	/* the cable is in */
#define SERIAL_OPEN		0x004	/* sceUsbSerialSetup was called */
#define SERIAL_CONNECTED	0x008	/* the host raised DTR */
#define SERIAL_NOTIFYING	0x010	/* a SERIAL_STATE notification is in flight */
#define SERIAL_RECEIVING	0x020
#define SERIAL_SENDING		0x040
#define SERIAL_DRAINING		0x080
#define SERIAL_NUMBER_READY	0x100	/* the serial number string is filled in */

/* s_serial.event bits */
#define EVENT_CLOSED		0x0001
#define EVENT_CONNECTED		0x0010
#define EVENT_DISCONNECTED	0x0020
#define EVENT_RX_DATA		0x0100	/* the receive ring holds data */
#define EVENT_TX_SPACE		0x0200	/* the transmit ring has room */
#define EVENT_TX_EMPTY		0x0400	/* nothing is left to send */
#define EVENT_RX_CANCEL		0x1000
#define EVENT_TX_CANCEL		0x2000
#define EVENT_DRAIN_CANCEL	0x4000
#define EVENT_IDLE		(EVENT_TX_EMPTY | EVENT_TX_SPACE)

/* cancel and reset masks */
#define SERIAL_MASK_RX		1
#define SERIAL_MASK_TX		2
#define SERIAL_MASK_DRAIN	4

/* CDC ACM class requests and notification */
#define CDC_SET_LINE_CODING		0x20
#define CDC_GET_LINE_CODING		0x21
#define CDC_SET_CONTROL_LINE_STATE	0x22
#define CDC_CONTROL_DTR			0x01
#define CDC_NOTIFY_SERIAL_STATE		0x20
#define CDC_STATE_DCD			0x01	/* bRxCarrier */
#define CDC_STATE_DSR			0x02	/* bTxCarrier */

#define USB_REQTYPE_TYPE_MASK		0x60
#define USB_REQTYPE_CLASS		0x20
#define USB_REQTYPE_CLASS_INTERFACE_IN	0xa1

/* product IDs the controller presents */
#define USB_PID_SERIAL		0x69b
#define USB_PID_MTP		0x4e4

#define SERIAL_RING_SIZE	0x10000
#define SERIAL_CONTROL_SIZE	0x40
#define SERIAL_PACKET_MAX	0x1000	/* bytes staged per bulk-in transfer */

/* CDC line coding, as GET/SET_LINE_CODING carry it */
typedef struct CdcLineCoding {
	uint32_t dwDTERate;
	uint8_t bCharFormat, bParityType, bDataBits;
	uint8_t padding;
} CdcLineCoding;

#define LINE_CODING_SIZE	7	/* on the wire: no padding */

/* Names unknown: ring and controller state, preserving the original layout. */
typedef struct SerialRing {
	uint8_t *base;
	unsigned size, write, read, empty;
	int timeout;
	unsigned reserved;
	unsigned overflow;		/* cleared on close, never set */
} SerialRing;

typedef struct SerialRequest {
	SceUdcdDeviceRequest request;
	uint8_t reserved[0x14];
} SerialRequest;

static struct {
	unsigned flags, mode, serialState, notificationPending;
	CdcLineCoding lineCoding;
	SerialRequest control, transmit, receive, notification;
	void *controlBuffer, *transmitBuffer, *receiveBuffer, *notificationBuffer;
	void *sendBuffer, *recvBuffer;
	SerialRing rx, tx;
	SceUID callback, event, memblock;
	unsigned notificationQueued;	/* cleared at start, never set */
	SceKernelSpinlock lock;
	uint8_t serialNumber[0x80];
	SceUdcdEndpoint endpoints[4];
	SceUdcdInterface interface;
	SceUdcdDriver driver;
	SceUdcdStringDescriptor serialString, productString;
} s_serial;

_Static_assert(offsetof(__typeof__(s_serial), driver) == 0x250, "driver");
_Static_assert(sizeof(s_serial) == 0x324, "serial state");


/* Name unknown: direct descriptor stores, without a library copy call. */
static void copyWords(void *dst, const void *src, unsigned words)
{
	volatile uint32_t *d = dst;
	const uint32_t *s = src;
	while (words--)
		*d++ = *s++;
}

/* Names unknown: full-capacity rings distinguish empty from full explicitly. */
static unsigned ringUsed(const SerialRing *ring)
{
	if (ring->empty)
		return 0;
	return ring->read < ring->write ? ring->write - ring->read :
		ring->write + ring->size - ring->read;
}

static unsigned ringFree(const SerialRing *ring)
{
	if (ring->empty)
		return ring->size;
	return ring->read < ring->write ? ring->read + ring->size - ring->write :
		ring->read - ring->write;
}

static unsigned isConnected(void)
{
	return (s_serial.flags & SERIAL_CONNECTED) / SERIAL_CONNECTED;
}

static void resetRings(void)
{
	s_serial.rx.write = s_serial.rx.read = 0;
	s_serial.tx.write = s_serial.tx.read = 0;
	s_serial.rx.empty = s_serial.tx.empty = 1;
}

static void completeControl(SceUdcdDeviceRequest *req)
{
	if (req->returnCode >= 0 && (unsigned)req->unused == CDC_SET_LINE_CODING) {
		const CdcLineCoding *coding = req->data;

		__builtin_memcpy(&s_serial.lineCoding, coding, LINE_CODING_SIZE);
	}
}

static int finalizeSerial(int size, void *args, void *ctx)
{
	s_serial.flags &= ~(SERIAL_STARTED | SERIAL_OPEN);
	ksceKernelDeleteEventFlag(s_serial.event);
	SceSysmemForDriver_009E1C61(s_serial.memblock);
	return 0;
}

static int initializeNumber(void *args)
{
	memset(s_serial.serialNumber, 0, sizeof(s_serial.serialNumber));
	if (ksceIdStorageLookup(0x112, 0, s_serial.serialNumber, 0x40) < 0) {
		for (unsigned i = 0; i < 0x40; i += 2) {
			s_serial.serialNumber[i] = 0;
			s_serial.serialNumber[i + 1] = '0';
		}
	}
	/* ID-storage stores this string at an odd byte offset. */
	copyWords(s_serial.serialString.bString, s_serial.serialNumber + 0x1f, 8);
	__builtin_memcpy(&s_serial.serialString.bString[16], s_serial.serialNumber + 0x3f,
			 sizeof(s_serial.serialString.bString[16]));
	s_serial.flags |= SERIAL_NUMBER_READY;
	return 1;
}

static void completeReceive(SceUdcdDeviceRequest *req)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (req->returnCode < 0) {
		ksceKernelSetEventFlag(s_serial.event, EVENT_RX_CANCEL);
		ksceKernelClearEventFlag(s_serial.event, ~EVENT_RX_CANCEL);
		req->returnCode = 0;
	}
	if ((s_serial.flags & (SERIAL_OPEN | SERIAL_CONNECTED)) == (SERIAL_OPEN | SERIAL_CONNECTED)) {
		unsigned n = req->transmitted;
		if (!n) {
			req->returnCode = 0;
		} else {
			unsigned free = ringFree(&s_serial.rx);
			unsigned tail = s_serial.rx.size - s_serial.rx.write;
			if (n > free)
				n = free;
			if (tail < n) {
				memcpy(s_serial.rx.base + s_serial.rx.write, req->data, tail);
				memcpy(s_serial.rx.base, (uint8_t *)req->data + tail, n - tail);
			} else {
				memcpy(s_serial.rx.base + s_serial.rx.write, req->data, n);
			}
			s_serial.rx.empty = 0;
			s_serial.rx.write += n;
			if (s_serial.rx.write >= s_serial.rx.size)
				s_serial.rx.write -= s_serial.rx.size;
			if (n)
				ksceKernelSetEventFlag(s_serial.event, EVENT_RX_DATA);
		}
	}
	if ((unsigned)req->size <= ringFree(&s_serial.rx) && req->returnCode == 0) {
		SceCpuForDriver_02796361(req->data, (req->size + 63) & ~63U);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		ksceUdcdReqRecv(req);
	} else {
		req->returnCode = 1;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	}
}

static int processControl(int recipient, int index, SceUdcdEP0DeviceRequest *req,
			  void *ctx)
{
	SceUdcdDeviceRequest *control = &s_serial.control.request;
	if (index < 0 || (req->bmRequestType & USB_REQTYPE_TYPE_MASK) != USB_REQTYPE_CLASS)
		return -1;
	switch (req->bRequest) {
	case CDC_SET_LINE_CODING:
	case CDC_GET_LINE_CODING:
		if (req->bRequest == CDC_GET_LINE_CODING) {
			CdcLineCoding *coding = control->data;

			__builtin_memcpy(coding, &s_serial.lineCoding, LINE_CODING_SIZE);
		}
		control->size = req->wLength < SERIAL_CONTROL_SIZE ? req->wLength : SERIAL_CONTROL_SIZE;
		control->unused = (void *)(unsigned)req->bRequest;
		if (req->bRequest == CDC_GET_LINE_CODING) {
			SceCpuForDriver_103872A5(control->data, SERIAL_CONTROL_SIZE);
			ksceUdcdReqSend(control);
		} else {
			SceCpuForDriver_02796361(control->data, SERIAL_CONTROL_SIZE);
			ksceUdcdReqRecv(control);
		}
		return 0;
	case CDC_SET_CONTROL_LINE_STATE:
		if ((req->wValue & CDC_CONTROL_DTR) != isConnected()) {
			unsigned clear, set;
			if (!(req->wValue & CDC_CONTROL_DTR)) {
				s_serial.flags &= ~SERIAL_CONNECTED;
				if (s_serial.flags & SERIAL_OPEN) {
					if (s_serial.flags & SERIAL_ATTACHED) {
						ksceUdcdReqCancelAll(&s_serial.endpoints[2]);
						ksceUdcdClearFIFO(&s_serial.endpoints[2]);
					}
					resetRings();
					clear = ~(EVENT_RX_DATA | EVENT_CONNECTED);
					set = EVENT_IDLE | EVENT_DISCONNECTED;
				} else {
					clear = ~EVENT_CONNECTED;
					set = EVENT_DISCONNECTED;
				}
			} else {
				s_serial.flags |= SERIAL_CONNECTED;
				clear = ~EVENT_DISCONNECTED;
				set = EVENT_CONNECTED;
			}
			ksceKernelClearEventFlag(s_serial.event, clear);
			ksceKernelSetEventFlag(s_serial.event, set);
			if (s_serial.callback >= 0)
				ksceKernelNotifyCallback(s_serial.callback, isConnected());
		}
		return 0;
	default:
		return -1;
	}
}

/* Name unknown: stage at most 4096 bytes of the transmit ring. */
static void stageTransmit(SceUdcdDeviceRequest *req)
{
	unsigned used = ringUsed(&s_serial.tx), n = used;
	unsigned tail = s_serial.tx.size - s_serial.tx.read;
	if (n > SERIAL_PACKET_MAX)
		n = SERIAL_PACKET_MAX;
	if (tail < n) {
		void *dst = memcpy(req->data, s_serial.tx.base + s_serial.tx.read, tail);
		memcpy((uint8_t *)dst + tail, s_serial.tx.base, n - tail);
	} else {
		memcpy(req->data, s_serial.tx.base + s_serial.tx.read, n);
	}
	s_serial.tx.read += n;
	if (s_serial.tx.read >= s_serial.tx.size)
		s_serial.tx.read -= s_serial.tx.size;
	if (n == used)
		s_serial.tx.empty = 1;
	req->size = n;
	SceCpuForDriver_103872A5(req->data, (n + 63) & ~63U);
}

static void completeTransmit(SceUdcdDeviceRequest *req)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (req->returnCode < 0) {
		ksceKernelSetEventFlag(s_serial.event, EVENT_TX_CANCEL);
		ksceKernelClearEventFlag(s_serial.event, ~EVENT_TX_CANCEL);
		req->returnCode = 0;
	}
	if ((s_serial.flags & (SERIAL_OPEN | SERIAL_CONNECTED)) == (SERIAL_OPEN | SERIAL_CONNECTED)) {
		if (ringUsed(&s_serial.tx)) {
			stageTransmit(req);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ksceUdcdReqSend(req);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
			ksceKernelSetEventFlag(s_serial.event, EVENT_TX_SPACE);
		} else {
			req->returnCode = 1;
			ksceKernelSetEventFlag(s_serial.event, EVENT_TX_EMPTY);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
}

static void notifySerial(void)
{
	/* bmRequestType, bNotification, wValue 0, wIndex, wLength 2, the state */
	uint32_t *p = s_serial.notification.request.data;
	p[0] = CDC_NOTIFY_SERIAL_STATE << 8 | USB_REQTYPE_CLASS_INTERFACE_IN;
	p[1] = (uint16_t)s_serial.interface.interfaceNumber | sizeof(uint16_t) << 16;
	*(uint16_t *)(p + 2) = s_serial.serialState;
	s_serial.notification.request.size = 10;
	SceCpuForDriver_103872A5(p, 64);
	ksceUdcdReqSend(&s_serial.notification.request);
}

static int attachSerial(int version, void *ctx)
{
	SceUdcdDeviceInfo info;
	ksceUdcdGetDeviceInfo(&info);
	ksceKernelClearEventFlag(s_serial.event, 0);
	ksceKernelSetEventFlag(s_serial.event, EVENT_IDLE | EVENT_DISCONNECTED);
	s_serial.notificationPending = s_serial.flags & SERIAL_NOTIFYING;
	s_serial.flags |= SERIAL_ATTACHED;
	if (!s_serial.notificationPending) {
		s_serial.flags |= SERIAL_NOTIFYING;
		notifySerial();
	} else {
		s_serial.notificationPending = 1;
	}
	s_serial.receive.request.returnCode = 0;
	s_serial.transmit.request.returnCode = 0;
	s_serial.notification.request.returnCode = 0;
	SceCpuForDriver_02796361(s_serial.receive.request.data,
		(s_serial.receive.request.size + 63) & ~63U);
	int ret = ksceUdcdReqRecv(&s_serial.receive.request);
	s_serial.transmit.request.returnCode = 1;
	s_serial.notification.request.returnCode = 1;
	return ret;
}

static void completeNotification(SceUdcdDeviceRequest *req)
{
	unsigned intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	s_serial.flags &= ~SERIAL_NOTIFYING;
	if (req->returnCode < 0) {
		s_serial.notificationPending = 0;
		req->returnCode = 0;
	} else if (s_serial.notificationPending) {
		s_serial.flags |= SERIAL_NOTIFYING;
		s_serial.notificationPending = 0;
		notifySerial();
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return;
	}
	if ((s_serial.flags & (SERIAL_OPEN | SERIAL_CONNECTED)) == (SERIAL_OPEN | SERIAL_CONNECTED)) {
		if (ringUsed(&s_serial.tx)) {
			stageTransmit(&s_serial.transmit.request);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ksceUdcdReqSend(&s_serial.transmit.request);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
			ksceKernelSetEventFlag(s_serial.event, EVENT_TX_SPACE);
		} else {
			req->returnCode = 1;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
}

static void detachSerial(void *ctx)
{
	if (s_serial.flags & SERIAL_CONNECTED) {
		unsigned clear = ~EVENT_CONNECTED, set = EVENT_DISCONNECTED;
		if (s_serial.flags & SERIAL_OPEN) {
			resetRings();
			clear = ~(EVENT_RX_DATA | EVENT_CONNECTED);
			set = EVENT_IDLE | EVENT_DISCONNECTED;
		}
		s_serial.flags &= ~(SERIAL_ATTACHED | SERIAL_CONNECTED);
		ksceKernelClearEventFlag(s_serial.event, clear);
		ksceKernelSetEventFlag(s_serial.event, set);
		if (s_serial.callback >= 0)
			ksceKernelNotifyCallback(s_serial.callback, isConnected());
	} else {
		s_serial.flags &= ~SERIAL_ATTACHED;
	}
	ksceKernelSetEventFlag(s_serial.event, EVENT_RX_CANCEL | EVENT_TX_CANCEL | EVENT_DRAIN_CANCEL);
}

/* Name unknown: controller descriptor trees. */
typedef struct SerialDescriptors {
	SceUdcdDeviceDescriptor device;
	SceUdcdConfiguration config;
	SceUdcdConfigDescriptor descriptor;
	SceUdcdInterfaceSettings settings[2];
	SceUdcdInterfaceDescriptor interfaces[3];
	SceUdcdEndpointDescriptor endpoints[4];
} SerialDescriptors;

static int initializeSerial(int size, void *args, void *ctx)
{
	static const SceUdcdDeviceDescriptor device = {
		.bLength = 18, .bDescriptorType = 1, .bcdUSB = 0x200,
		.bDeviceClass = 2, .bcdDevice = 0x100, .iProduct = 2,
		.iSerialNumber = 3, .bNumConfigurations = 1,
	};
	static const SceUdcdConfigDescriptor config = {
		.bLength = 9, .bDescriptorType = 2, .wTotalLength = 0x30,
		.bNumInterfaces = 2, .bConfigurationValue = 1, .bmAttributes = 0xc0,
	};
	static const SceUdcdInterfaceSettings settings[2] = {
		{ 0, 0, 1 }, { 0, 0, 1 },
	};
	static const SceUdcdInterfaceDescriptor interfaces[3] = {
		{ .bLength = 9, .bDescriptorType = 4, .bNumEndpoints = 1,
		  .bInterfaceClass = 2, .bInterfaceSubClass = 2, .bInterfaceProtocol = 1 },
		{ .bLength = 9, .bDescriptorType = 4, .bInterfaceNumber = 1,
		  .bNumEndpoints = 2, .bInterfaceClass = 10 },
	};
	static const SceUdcdEndpointDescriptor endpoints[4] = {
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x81, .bmAttributes = 3 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x82, .bmAttributes = 2 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 3, .bmAttributes = 2 },
	};
	SceKernelAllocMemBlockKernelOpt opt;
	uint8_t *base = 0;
	int ret;
	s_serial.rx.base = s_serial.tx.base = 0;
	s_serial.rx.size = s_serial.tx.size = SERIAL_RING_SIZE;
	if (args) {
		unsigned intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
		if (size != 20 || *(unsigned *)args != 20) {
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			return SERIAL_INVALID_ARGUMENT;
		}
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s_serial.memblock = SceSysmemForDriver_C94850C9("SceUsbSerial", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, 0x61000, &opt);
	if (s_serial.memblock < 0)
		return -1;
	ret = SceSysmemForDriver_A841EDDA(s_serial.memblock, (void **)&base);
	if (ret < 0)
		goto fail;
	if (!s_serial.rx.base) {
		s_serial.rx.base = base;
		base += SERIAL_RING_SIZE;
	}
	if (!s_serial.tx.base) {
		s_serial.tx.base = base;
		base += SERIAL_RING_SIZE;
	}
	/* the syscall bounce buffers, then the transfer buffers, cache-line aligned */
	s_serial.sendBuffer = base;
	s_serial.recvBuffer = base + SERIAL_RING_SIZE;
	base = (uint8_t *)(((unsigned)base + 2 * SERIAL_RING_SIZE + 63) & ~63U);
	s_serial.controlBuffer = base;
	s_serial.transmitBuffer = base + SERIAL_CONTROL_SIZE;
	s_serial.receiveBuffer = base + SERIAL_CONTROL_SIZE + SERIAL_RING_SIZE;
	s_serial.notificationBuffer = base + SERIAL_CONTROL_SIZE + 2 * SERIAL_RING_SIZE;
	SerialDescriptors *descriptors = (void *)(base + 2 * SERIAL_CONTROL_SIZE +
						  2 * SERIAL_RING_SIZE);
	for (unsigned i = 0; i < 2; i++) {
		SerialDescriptors *d = &descriptors[i];
		if (i) {
			s_serial.driver.descriptor = &d->device;
			s_serial.driver.configuration = &d->config;
		} else {
			s_serial.driver.descriptor_hi = &d->device;
			s_serial.driver.configuration_hi = &d->config;
		}
		d->config.configDescriptors = &d->descriptor;
		d->config.settings = d->settings;
		d->config.interfaceDescriptors = d->interfaces;
		d->config.endpointDescriptors = d->endpoints;
		copyWords(&d->device, &device, 5);
		copyWords(&d->descriptor, &config, 6);
		copyWords(d->settings, settings, 6);
		memcpy(d->interfaces, interfaces, sizeof(interfaces));
		copyWords(d->endpoints, endpoints, 16);
		d->descriptor.settings = d->settings;
		d->settings[0].descriptors = &d->interfaces[0];
		d->settings[1].descriptors = &d->interfaces[1];
		d->interfaces[0].endpoints = &d->endpoints[0];
		d->interfaces[1].endpoints = &d->endpoints[1];
		d->device.bMaxPacketSize0 = 0x40;
		d->endpoints[0].wMaxPacketSize = 0x40;
		d->endpoints[0].bInterval = i ? 1 : 4;
		d->endpoints[1].wMaxPacketSize = i ? 0x40 : 0x200;
		d->endpoints[2].wMaxPacketSize = i ? 0x40 : 0x200;
	}
	s_serial.event = ksceKernelCreateEventFlag("SceUsbSerial", 0x9000,
						  EVENT_IDLE | EVENT_DISCONNECTED | EVENT_CLOSED, 0);
	if (s_serial.event < 0)
		goto fail;
	SceUdcdDeviceRequest *requests[] = { &s_serial.control.request,
		&s_serial.transmit.request, &s_serial.receive.request, &s_serial.notification.request };
	void *buffers[] = { s_serial.controlBuffer, s_serial.transmitBuffer,
		s_serial.receiveBuffer, s_serial.notificationBuffer };
	void (*callbacks[])(SceUdcdDeviceRequest *) = { completeControl,
		completeTransmit, completeReceive, completeNotification };
	for (unsigned i = 0; i < 4; i++) {
		SceUdcdDeviceRequest *req = requests[i];
		req->endpoint = &s_serial.endpoints[i == 1 ? 2 : i == 2 ? 3 : i == 3 ? 1 : 0];
		req->data = buffers[i];
		req->attributes = 0;
		req->size = i == 0 || i == 3 ? SERIAL_CONTROL_SIZE : i == 1 ? SERIAL_PACKET_MAX : 0x200;
		req->isControlRequest = i < 2;
		req->onComplete = callbacks[i];
		req->transmitted = 0;
		req->returnCode = 1;
		req->next = req->unused = 0;
	}
	s_serial.flags |= SERIAL_STARTED;
	s_serial.mode = s_serial.serialState = 0;
	resetRings();
	s_serial.notificationPending = s_serial.notificationQueued = 0;
	/* 115200 baud, 1.5 stop bits, no parity, 8 data bits */
	s_serial.lineCoding.dwDTERate = 115200;
	s_serial.lineCoding.bCharFormat = 1;
	s_serial.lineCoding.bParityType = 0;
	s_serial.lineCoding.bDataBits = 8;
	s_serial.rx.timeout = s_serial.tx.timeout = s_serial.callback = -1;
	/* Name unknown: device-information operation, selector 5. */
	uint32_t info[16];
	info[1] = 5;
	SceUdcdForDriver_78BF2A28(info);
	return 0;
fail:
	SceSysmemForDriver_009E1C61(s_serial.memblock);
	return ret;
}

int module_start(unsigned arglen, void *argp)
{
	static const uint16_t product[16] = {
		'"', 'P', 'S', ' ', 'V', 'i', 't', 'a', '"', ' ', 'T', 'y', 'p', 'e', ' ', 'D'
	};
	copyWords(s_serial.productString.bString, product, 8);
	s_serial.productString.bLength = 34;
	s_serial.productString.bDescriptorType = 3;
	s_serial.serialString.bLength = 36;
	s_serial.serialString.bDescriptorType = 3;
	s_serial.endpoints[0].driverEndpointNumber = 0;
	s_serial.endpoints[1].direction = 0x80;
	s_serial.endpoints[1].driverEndpointNumber = 1;
	s_serial.endpoints[2].direction = 0x80;
	s_serial.endpoints[2].driverEndpointNumber = 2;
	s_serial.endpoints[3].direction = 0;
	s_serial.endpoints[3].driverEndpointNumber = 3;
	s_serial.interface.expectNumber = 0;
	s_serial.interface.numInterfaces = 2;
	s_serial.driver.driverName = "USBSerDriver";
	s_serial.driver.numEndpoints = 4;
	s_serial.driver.endpoints = s_serial.endpoints;
	s_serial.driver.interface = &s_serial.interface;
	s_serial.driver.stringDescriptors = 0;
	s_serial.driver.stringDescriptorProduct = &s_serial.productString;
	s_serial.driver.stringDescriptorSerial = &s_serial.serialString;
	s_serial.driver.processRequest = processControl;
	s_serial.driver.changeSetting = 0;
	s_serial.driver.attach = attachSerial;
	s_serial.driver.detach = detachSerial;
	s_serial.driver.configure = 0;
	s_serial.driver.start = initializeSerial;
	s_serial.driver.stop = finalizeSerial;
	s_serial.flags = 0;
	s_serial.lock = 0;
	if (ksceUdcdRegister(&s_serial.driver) < 0)
		return 2;
	ksceKernelEnqueueWorkQueue(SCE_KERNEL_GLOBAL_WORK_QUEUE_ID, "SceUsbSerInit", initializeNumber, 0);
	return 0;
}

/* Names unknown: kernel-side serial API behind the syscall wrappers. */
static int serialStatus(void)
{
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	return (s_serial.flags & (SERIAL_ATTACHED | SERIAL_OPEN)) == (SERIAL_ATTACHED | SERIAL_OPEN) ?
	       0 : SERIAL_NOT_OPEN;
}

static int waitError(int ret, unsigned bits, unsigned disconnected, unsigned cancel)
{
	if (ret < 0)
		return ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? SERIAL_TIMEOUT : ret;
	if (bits & EVENT_CLOSED)
		return SERIAL_NOT_OPEN;
	if (bits & disconnected)
		return SERIAL_DISCONNECTED;
	if (bits & cancel)
		return SERIAL_CANCELLED;
	return ret;
}

static int receiveSerial(void *buffer, unsigned size, unsigned mode, int timeout)
{
	int ret = serialStatus();
	unsigned intr, total = 0, disconnected;
	uint32_t bits[2], usec;
	if (ret < 0)
		return ret;
	if (!buffer || mode > 2)
		return SERIAL_INVALID_ARGUMENT;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (s_serial.flags & SERIAL_RECEIVING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return SERIAL_BUSY;
	}
	s_serial.flags |= SERIAL_RECEIVING;
	disconnected = s_serial.mode ? 0 : EVENT_DISCONNECTED;
	ksceKernelClearEventFlag(s_serial.event, ~EVENT_RX_CANCEL);
	do {
		if (mode != 2) {
			if (timeout >= 0)
				usec = timeout;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ret = ksceKernelWaitEventFlag(s_serial.event, disconnected | EVENT_RX_CANCEL | EVENT_RX_DATA | EVENT_CLOSED,
				SCE_EVENT_WAITOR, bits, timeout < 0 ? 0 : &usec);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
			ret = waitError(ret, bits[0], disconnected, EVENT_RX_CANCEL);
			if (ret < 0)
				goto done;
		} else {
			if (disconnected && !(s_serial.flags & SERIAL_CONNECTED)) {
				ret = SERIAL_DISCONNECTED;
				goto done;
			}
			if (!ringUsed(&s_serial.rx)) {
				total = 0;
				break;
			}
		}
		unsigned n = size - total;
		if (n) {
			unsigned used = ringUsed(&s_serial.rx);
			unsigned tail = s_serial.rx.size - s_serial.rx.read;
			if (n > used)
				n = used;
			if (tail < n) {
				memcpy(buffer, s_serial.rx.base + s_serial.rx.read, tail);
				memcpy((uint8_t *)buffer + tail, s_serial.rx.base, n - tail);
			} else {
				memcpy(buffer, s_serial.rx.base + s_serial.rx.read, n);
			}
			s_serial.rx.read += n;
			if (s_serial.rx.read >= s_serial.rx.size)
				s_serial.rx.read -= s_serial.rx.size;
			if (n == used)
				s_serial.rx.empty = 1;
		}
		total += n;
		buffer = (uint8_t *)buffer + n;
		if (!ringUsed(&s_serial.rx))
			ksceKernelClearEventFlag(s_serial.event, ~EVENT_RX_DATA);
		if (s_serial.receive.request.returnCode == 1 &&
		    (unsigned)s_serial.receive.request.size <= ringFree(&s_serial.rx)) {
			SceCpuForDriver_02796361(s_serial.receive.request.data,
				(s_serial.receive.request.size + 63) & ~63U);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ksceUdcdReqRecv(&s_serial.receive.request);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
		}
	} while (mode == 0 && total < size);
	ret = total;
done:
	s_serial.flags &= ~SERIAL_RECEIVING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return ret;
}

static int sendSerial(const void *buffer, unsigned size, unsigned mode, int timeout)
{
	int ret = serialStatus();
	unsigned intr, total = 0, disconnected;
	uint32_t bits[2], usec;
	if (ret < 0)
		return ret;
	if (!buffer || mode > 2)
		return SERIAL_INVALID_ARGUMENT;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (s_serial.flags & SERIAL_SENDING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return SERIAL_BUSY;
	}
	s_serial.flags |= SERIAL_SENDING;
	disconnected = s_serial.mode ? 0 : EVENT_DISCONNECTED;
	ksceKernelClearEventFlag(s_serial.event, ~EVENT_TX_CANCEL);
	do {
		if (mode != 2) {
			if (timeout >= 0)
				usec = timeout;
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ret = ksceKernelWaitEventFlag(s_serial.event, disconnected | EVENT_TX_CANCEL | EVENT_TX_SPACE | EVENT_CLOSED,
				SCE_EVENT_WAITOR, bits, timeout < 0 ? 0 : &usec);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
			ret = waitError(ret, bits[0], disconnected, EVENT_TX_CANCEL);
			if (ret < 0)
				goto done;
		} else {
			if (disconnected && !(s_serial.flags & SERIAL_CONNECTED)) {
				ret = SERIAL_DISCONNECTED;
				goto done;
			}
			if (!ringFree(&s_serial.tx)) {
				total = 0;
				break;
			}
		}
		if (!(s_serial.flags & SERIAL_CONNECTED)) {
			ret = size;
			goto done;
		}
		unsigned n = size - total;
		if (n) {
			unsigned free = ringFree(&s_serial.tx);
			unsigned tail = s_serial.tx.size - s_serial.tx.write;
			if (n > free)
				n = free;
			if (tail < n) {
				memcpy(s_serial.tx.base + s_serial.tx.write, buffer, tail);
				memcpy(s_serial.tx.base, (const uint8_t *)buffer + tail, n - tail);
			} else {
				memcpy(s_serial.tx.base + s_serial.tx.write, buffer, n);
			}
			total += n;
			s_serial.tx.write += n;
			s_serial.tx.empty = 0;
			if (s_serial.tx.write >= s_serial.tx.size)
				s_serial.tx.write -= s_serial.tx.size;
			buffer = (const uint8_t *)buffer + n;
		}
		if (!ringFree(&s_serial.tx))
			ksceKernelClearEventFlag(s_serial.event, ~EVENT_TX_SPACE);
		if (!s_serial.tx.empty && ringUsed(&s_serial.tx)) {
			ksceKernelClearEventFlag(s_serial.event, ~EVENT_TX_EMPTY);
			if (!(s_serial.flags & SERIAL_NOTIFYING) && s_serial.transmit.request.returnCode == 1) {
				stageTransmit(&s_serial.transmit.request);
				sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
				ksceUdcdReqSend(&s_serial.transmit.request);
				intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
				ksceKernelSetEventFlag(s_serial.event, EVENT_TX_SPACE);
			}
		}
	} while (mode == 0 && total < size);
	ret = total;
done:
	s_serial.flags &= ~SERIAL_SENDING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return ret;
}

static int startSerial(void)
{
	int ret;
	if (!(s_serial.flags & SERIAL_NUMBER_READY))
		return SERIAL_NOT_STARTED;
	if (s_serial.flags & 1)
		return SERIAL_ALREADY_STARTED;
	ksceUdcdDeactivate();
	ksceUdcdStop("USB_MTP_Driver", 0, 0);
	ksceUdcdStop("USBDeviceControllerDriver", 0, 0);
	ret = ksceUdcdStart("USBDeviceControllerDriver", 0, 0);
	if (ret < 0)
		return ret;
	ret = ksceUdcdStart("USBSerDriver", 0, 0);
	if (ret >= 0) {
		ret = ksceUdcdActivate(USB_PID_SERIAL);
		if (ret >= 0)
			return 0;
		ksceUdcdStop("USBSerDriver", 0, 0);
	}
	ksceUdcdStop("USBDeviceControllerDriver", 0, 0);
	return ret;
}

static int closeSerial(void)
{
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	ksceUdcdDeactivate();
	ksceUdcdStop("USBSerDriver", 0, 0);
	ksceUdcdStop("USBDeviceControllerDriver", 0, 0);
	ksceUdcdStart("USBDeviceControllerDriver", 0, 0);
	ksceUdcdStart("USB_MTP_Driver", 0, 0);
	return ksceUdcdActivate(USB_PID_MTP);
}

static int setupSerial(unsigned mode)
{
	unsigned intr;
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	if (mode > 1)
		return SERIAL_INVALID_ARGUMENT;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (s_serial.flags & SERIAL_OPEN) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return SERIAL_BUSY;
	}
	s_serial.flags |= SERIAL_OPEN;
	s_serial.serialState |= CDC_STATE_DCD | CDC_STATE_DSR;
	s_serial.mode = mode;
	ksceKernelClearEventFlag(s_serial.event, ~EVENT_CLOSED);
	if (s_serial.flags & SERIAL_ATTACHED) {
		s_serial.notificationPending = s_serial.flags & SERIAL_NOTIFYING;
		if (!s_serial.notificationPending) {
			s_serial.flags |= SERIAL_NOTIFYING;
			notifySerial();
		} else {
			s_serial.notificationPending = 1;
		}
		SceCpuForDriver_02796361(s_serial.receive.request.data,
			(s_serial.receive.request.size + 63) & ~63U);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		ksceUdcdReqRecv(&s_serial.receive.request);
		intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return 0;
}

static int unsetupSerial(void)
{
	unsigned intr;
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (!(s_serial.flags & SERIAL_OPEN)) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return SERIAL_ALREADY_STARTED;
	}
	s_serial.flags &= ~SERIAL_OPEN;
	s_serial.serialState &= ~(CDC_STATE_DCD | CDC_STATE_DSR);
	if (s_serial.flags & SERIAL_ATTACHED) {
		s_serial.notificationPending = s_serial.flags & SERIAL_NOTIFYING;
		if (!s_serial.notificationPending) {
			s_serial.flags |= SERIAL_NOTIFYING;
			notifySerial();
		} else {
			s_serial.notificationPending = 1;
		}
		if (s_serial.flags & SERIAL_CONNECTED) {
			resetRings();
			ksceKernelClearEventFlag(s_serial.event, ~EVENT_RX_DATA);
			s_serial.rx.overflow = s_serial.tx.overflow = 0;
			ksceKernelSetEventFlag(s_serial.event, EVENT_IDLE | EVENT_CLOSED);
			goto done;
		}
	}
	ksceKernelSetEventFlag(s_serial.event, EVENT_CLOSED);
done:
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return 0;
}

static int cancelSerial(unsigned mask)
{
	int ret = serialStatus();
	if (ret < 0)
		return ret;
	if (!mask || (mask & ~(SERIAL_MASK_RX | SERIAL_MASK_TX | SERIAL_MASK_DRAIN)))
		return SERIAL_INVALID_ARGUMENT;
	if ((mask & SERIAL_MASK_RX) && (s_serial.flags & SERIAL_RECEIVING))
		ksceKernelSetEventFlag(s_serial.event, EVENT_RX_CANCEL);
	if ((mask & SERIAL_MASK_TX) && (s_serial.flags & SERIAL_SENDING))
		ksceKernelSetEventFlag(s_serial.event, EVENT_TX_CANCEL);
	if ((mask & SERIAL_MASK_DRAIN) && (s_serial.flags & SERIAL_DRAINING))
		ksceKernelSetEventFlag(s_serial.event, EVENT_DRAIN_CANCEL);
	return 0;
}

static int resetSerial(unsigned mask)
{
	int ret = serialStatus();
	unsigned intr;
	if (ret < 0)
		return ret;
	if (!mask || (mask & ~(SERIAL_MASK_RX | SERIAL_MASK_TX)))
		return SERIAL_INVALID_ARGUMENT;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (mask & SERIAL_MASK_RX) {
		s_serial.rx.write = s_serial.rx.read = 0;
		s_serial.rx.empty = 1;
		ksceKernelClearEventFlag(s_serial.event, ~EVENT_RX_DATA);
		if ((s_serial.flags & SERIAL_ATTACHED) && s_serial.receive.request.returnCode == 1) {
			SceCpuForDriver_02796361(s_serial.receive.request.data,
				(s_serial.receive.request.size + 63) & ~63U);
			sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
			ksceUdcdReqRecv(&s_serial.receive.request);
			intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
		}
	}
	if (mask & SERIAL_MASK_TX) {
		s_serial.tx.write = s_serial.tx.read = 0;
		s_serial.tx.empty = 1;
		ksceKernelSetEventFlag(s_serial.event, EVENT_IDLE);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return 0;
}

static int drainSerial(int timeout)
{
	int ret = serialStatus();
	unsigned intr, disconnected;
	uint32_t bits[2], usec;
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	if (s_serial.flags & SERIAL_DRAINING) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
		return SERIAL_BUSY;
	}
	s_serial.flags |= SERIAL_DRAINING;
	disconnected = s_serial.mode ? 0 : EVENT_DISCONNECTED;
	if (timeout >= 0)
		usec = timeout;
	ksceKernelClearEventFlag(s_serial.event, ~EVENT_DRAIN_CANCEL);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	ret = ksceKernelWaitEventFlag(s_serial.event, disconnected | EVENT_DRAIN_CANCEL | EVENT_TX_EMPTY | EVENT_CLOSED,
		SCE_EVENT_WAITOR, bits, timeout < 0 ? 0 : &usec);
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	ret = waitError(ret, bits[0], disconnected, EVENT_DRAIN_CANCEL);
	s_serial.flags &= ~SERIAL_DRAINING;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return ret;
}

static int queryRing(const SerialRing *ring, int free)
{
	unsigned intr;
	int n;
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	intr = SceCpuForDriver_D32ACE9E(&s_serial.lock);
	n = free ? ringFree(ring) : ringUsed(ring);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_serial.lock, intr);
	return n;
}

static int queryStatus(void)
{
	if (!(s_serial.flags & SERIAL_STARTED))
		return SERIAL_NOT_STARTED;
	if (!(s_serial.flags & SERIAL_OPEN))
		return SERIAL_NOT_OPEN;
	return isConnected();
}

/* Names unknown: exports without official symbol names retain their NIDs. */
#define SERIAL_EXPORT(name, decl, expression) \
int name decl \
{ \
	unsigned context = sceSyscallEnter(); \
	int ret = SceSblACMgrForDriver_062CAEB2(0) ? (expression) : (int)SCE_ERROR_ERRNO_ENOSYS; \
	sceSyscallLeave(context); \
	return ret; \
}

SERIAL_EXPORT(sceUsbSerialStart, (void), startSerial())
SERIAL_EXPORT(sceUsbSerialClose, (void), closeSerial())
SERIAL_EXPORT(sceUsbSerialSetup, (unsigned mode), setupSerial(mode))
SERIAL_EXPORT(SceUsbSerial_2C08272C, (void), unsetupSerial())
SERIAL_EXPORT(SceUsbSerial_60D93778, (unsigned mask), cancelSerial(mask))
SERIAL_EXPORT(SceUsbSerial_0ACC8A92, (unsigned mask), resetSerial(mask))
SERIAL_EXPORT(sceUsbSerialGetRecvBufferSize, (void), queryRing(&s_serial.rx, 0))
SERIAL_EXPORT(SceUsbSerial_923535FA, (void), queryRing(&s_serial.rx, 1))
SERIAL_EXPORT(SceUsbSerial_A281DF87, (void), queryRing(&s_serial.tx, 0))
SERIAL_EXPORT(SceUsbSerial_642F8F19, (void), queryRing(&s_serial.tx, 1))
SERIAL_EXPORT(sceUsbSerialStatus, (void), queryStatus())

int sceUsbSerialRecv(void *buffer, unsigned size, unsigned mode, int timeout)
{
	unsigned context = sceSyscallEnter();
	int ret;
	if (SceThreadmgrForDriver_20C228E4()) {
		ret = SERIAL_NOT_STARTED;
	} else if (!SceSblACMgrForDriver_062CAEB2(0)) {
		ret = (int)SCE_ERROR_ERRNO_ENOSYS;
	} else {
		ret = serialStatus();
		if (ret >= 0) {
			if (size > SERIAL_RING_SIZE) {
				ret = SERIAL_INVALID_ARGUMENT;
			} else {
				ret = receiveSerial(s_serial.recvBuffer, size, mode, timeout);
				/* Sony copies the requested size, including on an error. */
				if (SceSysmemForDriver_6D88EF8A(buffer, s_serial.recvBuffer, size) < 0)
					ret = 1;
			}
		}
	}
	sceSyscallLeave(context);
	return ret;
}

int sceUsbSerialSend(const void *buffer, unsigned size, unsigned mode, int timeout)
{
	unsigned context = sceSyscallEnter();
	int ret;
	if (!SceSblACMgrForDriver_062CAEB2(0)) {
		ret = (int)SCE_ERROR_ERRNO_ENOSYS;
	} else if (SceThreadmgrForDriver_20C228E4()) {
		ret = SERIAL_NOT_STARTED;
	} else {
		ret = serialStatus();
		if (ret >= 0) {
			if (size > SERIAL_RING_SIZE) {
				ret = SERIAL_INVALID_ARGUMENT;
			} else {
				ret = SceSysmemForDriver_BC996A7A(s_serial.sendBuffer, buffer, size);
				if (ret >= 0)
					ret = sendSerial(s_serial.sendBuffer, size, mode, timeout);
			}
		}
	}
	sceSyscallLeave(context);
	return ret;
}

int SceUsbSerial_405DB744(int timeout)
{
	unsigned context = sceSyscallEnter();
	int ret;
	if (SceThreadmgrForDriver_20C228E4())
		ret = SERIAL_NOT_STARTED;
	else if (!SceSblACMgrForDriver_062CAEB2(0))
		ret = (int)SCE_ERROR_ERRNO_ENOSYS;
	else
		ret = drainSerial(timeout);
	sceSyscallLeave(context);
	return ret;
}
