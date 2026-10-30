/* SceUsbAudio: the device table, descriptor parsing, volume requests and the driver API. */
#include "usbaudio.h"
#include <sysmem/sysmem.h>
#include <kernel/error.h>
#include <intrmgr/syscall.h>
#include <sceerror.h>

#define USBAUDIO_HEAP_SIZE	0x40000

struct AudioCallback {
	struct AudioCallback *next, *prev;
	SceUID callback;
};

static SceUID s_heap = -1;
static SceKernelFastMutex s_lock;
static SceKernelFastMutex s_deviceLock;
static AudioDevice *s_devices[8];
static struct AudioCallback s_callbacks;

static __attribute__((noinline)) void completeVolume(int result, int length, void *arg);
static __attribute__((noinline)) void completeInterface(int result, int length, void *arg);
static __attribute__((noinline)) void completeSetup(int result, int length, void *arg);
static __attribute__((noinline)) SceUsbdEndpointDescriptor *streamEndpoint(unsigned id, const SceUsbdInterfaceDescriptor *interface);
static __attribute__((noinline)) void parseDescriptors(unsigned id, AudioPort *port);
static __attribute__((noinline)) void notifyDevices(void);
static __attribute__((noinline)) void completeCapture(int result, UsbdIsochRequest *packet, void *arg);
static __attribute__((noinline)) void completeOutput(int result, UsbdIsochRequest *packet, void *arg);
static __attribute__((noinline)) AudioQueue *allocateInputQueue(void);
static __attribute__((noinline)) AudioQueue *allocateOutputQueue(void);
static __attribute__((noinline)) void releaseQueue(AudioQueue *queue);

static inline __attribute__((always_inline)) AudioDevice *findUnlocked(unsigned id)
{
	for (unsigned i = 0; i < 8; i++) {
		AudioDevice *dev = s_devices[i];

		if (dev && dev->id == id)
			return dev;
	}
	return NULL;
}
static inline __attribute__((always_inline)) uint8_t *controlBuffer(AudioDevice *dev)
{
	int at = (int)dev->controlBuffer;
	return (void *)(at + 64 - at % 64);
}
/* a class request on the control pipe, data moving through the aligned control buffer */
static inline __attribute__((always_inline)) int controlRequest(AudioDevice *dev, SceUsbdDeviceRequest *req,
								void *data)
{
	int ret = lockFastMutex(&dev->mutex);
	if (ret)
		return ret;
	uint8_t *buffer = controlBuffer(dev);
	if ((int8_t)req->bmRequestType >= 0)
		*(uint16_t *)buffer = *(uint16_t *)data;
	dev->controlResult = 0;
	ret = SceUsbdForDriver_2E05660F(dev->port.controlPipe, req, buffer, completeVolume, dev);
	unsigned timeout = 1000000;
	int wait = sceKernelWaitEventFlag(dev->event, 1,
					   SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL,
					   0, &timeout);
	if ((int8_t)req->bmRequestType < 0)
		*(uint16_t *)data = *(uint16_t *)buffer;
	sceKernelUnlockFastMutex(&dev->mutex);
	if (ret)
		return ret;
	if (wait)
		return wait;
	return dev->controlResult;
}
static inline __attribute__((always_inline)) int volumeRequest(unsigned id, unsigned request, unsigned value, int *out)
{
	if (lockDevices())
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	AudioDevice *dev = findUnlocked(id);
	unlockDevices();
	if (!dev)
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	unsigned index = dev->port.volumeUnit << 8 | dev->port.volumeInterface;
	if (dev->port.volumeUnit == UAC_NO_UNIT)
		return AUDIO_ERROR(NOT_SUPPORTED);
	SceUsbdDeviceRequest req = {
		.bmRequestType = (request & SCE_USBD_REQTYPE_DIR_TO_HOST ?
				  SCE_USBD_REQTYPE_DIR_TO_HOST : SCE_USBD_REQTYPE_DIR_TO_DEVICE) |
				 SCE_USBD_REQTYPE_TYPE_CLASS | SCE_USBD_REQTYPE_RECIP_INTERFACE,
		.bRequest = request,
		.wValue = UAC_VOLUME_CONTROL << 8,
		.wIndex = index,
		.wLength = 2,
	};
	int16_t volume = value;
	int ret = controlRequest(dev, &req, &volume);
	if (request & SCE_USBD_REQTYPE_DIR_TO_HOST)
		*out = volume;
	return ret;
}
static inline __attribute__((always_inline)) AudioPacket *freePacket(AudioQueue *queue)
{
	for (unsigned i = 0; i < 3; i++)
		if (!queue->packet[i].owner)
			return &queue->packet[i];
	return NULL;
}
static inline __attribute__((always_inline)) void flushQueue(AudioPort *port, AudioQueue *queue, int capture)
{
	if (queue->pending == 2) {
		for (unsigned i = 0; i < 2; i++) {
			AudioPacket *p = NULL;
			if (queue->pending > 0) {
				p = queue->queued[queue->read];
				queue->pending--;
				queue->read = (queue->read + 1) % 2;
			}
			submitIso(capture ? port->inputPipe : port->outputPipe,
				&p->request, capture ? completeCapture : completeOutput, p);
		}
	}
}
static inline __attribute__((always_inline)) void queuePacket(AudioQueue *queue, AudioPacket *packet)
{
	if (queue->pending < 2) {
		queue->queued[queue->write] = packet;
		queue->pending++;
		queue->write = (queue->write + 1) % 2;
	}
}
/* the next descriptor of a type after previous (type 0: any) */
static inline __attribute__((always_inline)) void *scan(unsigned id, const void *previous, int type)
{
	return SceUsbdForDriver_BC3EF82B(id, (void *)previous, type);
}
static inline __attribute__((always_inline)) int interfaceMatches(const SceUsbdInterfaceDescriptor *interface, unsigned subclass)
{
	return interface && interface->bLength >= sizeof(*interface) &&
	       interface->bDescriptorType == SCE_USBD_DESCRIPTOR_INTERFACE &&
	       interface->bInterfaceClass == SCE_USBD_CLASS_AUDIO &&
	       interface->bInterfaceSubclass == subclass;
}
static inline __attribute__((always_inline)) const UacAsGeneralDescriptor *streamGeneral(unsigned id, const SceUsbdInterfaceDescriptor *interface)
{
	const UacDescriptor *p = (const void *)interface;

	for (;;) {
		p = scan(id, p, 0);
		if (!p)
			return NULL;
		if (p->bLength < 2)
			continue;
		if (p->bDescriptorType == SCE_USBD_DESCRIPTOR_INTERFACE) {
			const SceUsbdInterfaceDescriptor *next = (const void *)p;

			if (!interfaceMatches(next, UAC_SUBCLASS_AUDIOSTREAMING) ||
			    next->bInterfaceNumber != interface->bInterfaceNumber)
				return NULL;
		} else if (p->bDescriptorType != UAC_CS_INTERFACE) {
			return NULL;
		} else if (p->bLength >= sizeof(UacAsGeneralDescriptor) &&
			   p->bDescriptorSubtype == UAC_AS_GENERAL) {
			return (const void *)p;
		}
	}
}
static inline __attribute__((always_inline)) const UacHeaderDescriptor *controlHeader(unsigned id, const SceUsbdInterfaceDescriptor *interface)
{
	const UacHeaderDescriptor *p = (const void *)interface;

	for (;;) {
		p = scan(id, p, 0);
		if (!p)
			return NULL;
		if (p->bLength < 2)
			continue;
		if (p->bDescriptorType != UAC_CS_INTERFACE)
			return NULL;
		if (p->bLength >= sizeof(*p) && p->bDescriptorSubtype == UAC_HEADER &&
		    p->bLength >= sizeof(*p) + p->bInCollection)
			return p;
	}
}
static inline __attribute__((always_inline)) unsigned sampleRate(const uint8_t *rate)
{
	return rate[0] | rate[1] << 8 | rate[2] << 16;
}
static inline __attribute__((always_inline)) unsigned entityIndex(const UacEntityDescriptor **entities, unsigned n, uint8_t id)
{
	for (unsigned i = 0; i < n && entities[i]; i++)
		if (entities[i]->bEntityID == id)
			return i;
	return 255;
}
/*
 * Find the feature unit with a master volume control: walk the unit graph
 * back from the output terminal that feeds the USB stream.
 */
static inline __attribute__((always_inline)) void findVolumeUnit(unsigned id, AudioPort *port)
{
	const SceUsbdInterfaceDescriptor *interface = NULL;
	const UacEntityDescriptor *entities[32];
	const UacDescriptor *p;
	unsigned stack[33], depth = 0, root, n = 0;

	port->volumeUnit = port->volumeInterface = UAC_NO_UNIT;
	port->volumeState[0] = port->volumeState[1] = 0;
	do {
		interface = scan(id, interface, SCE_USBD_DESCRIPTOR_INTERFACE);
		if (!interface)
			return;
	} while (!interfaceMatches(interface, UAC_SUBCLASS_AUDIOCONTROL));
	p = (const void *)controlHeader(id, interface);
	if (!p) {
		port->volumeInterface = interface->bInterfaceNumber;
		return;
	}
	memset(entities, 0, sizeof(entities));
	while ((p = scan(id, p, 0))) {
		if (n >= 32)
			break;
		if (p->bLength < 3)
			continue;
		if (p->bDescriptorType != UAC_CS_INTERFACE)
			break;
		if ((unsigned)p->bDescriptorSubtype - UAC_INPUT_TERMINAL <= UAC_EXTENSION_UNIT - UAC_INPUT_TERMINAL)
			entities[n++] = (const void *)p;
	}
	for (root = 0; root < n; root++) {
		const UacOutputTerminalDescriptor *terminal = (const void *)entities[root];

		if (terminal->bDescriptorSubtype == UAC_OUTPUT_TERMINAL &&
		    terminal->wTerminalType == UAC_TERMINAL_STREAMING)
			break;
	}
	/* no streaming output terminal: Sony leaves the interface unset */
	if (root >= n)
		return;
	stack[0] = root;
	for (;;) {
		const UacEntityDescriptor *entity = entities[stack[depth]];
		const uint8_t *sources = NULL;
		unsigned count = 0;

		if (entity->bDescriptorSubtype == UAC_FEATURE_UNIT) {
			const UacFeatureUnitDescriptor *unit = (const void *)entity;

			if (unit->bmaControls[0] & UAC_FEATURE_VOLUME) {
				port->volumeUnit = unit->bUnitID;
				break;
			}
			if (depth < 32)
				stack[depth] = entityIndex(entities, n, unit->bSourceID);
			else
				depth--;
			continue;
		}
		if (entity->bDescriptorSubtype == UAC_OUTPUT_TERMINAL) {
			const UacOutputTerminalDescriptor *terminal = (const void *)entity;

			if (depth < 32)
				stack[depth] = entityIndex(entities, n, terminal->bSourceID);
			else
				depth--;
			continue;
		}
		if (entity->bDescriptorSubtype == UAC_MIXER_UNIT ||
		    entity->bDescriptorSubtype == UAC_SELECTOR_UNIT) {
			const UacPinsDescriptor *unit = (const void *)entity;

			count = unit->bNrInPins;
			sources = unit->baSourceID;
		} else if (entity->bDescriptorSubtype == UAC_PROCESSING_UNIT ||
			   entity->bDescriptorSubtype == UAC_EXTENSION_UNIT) {
			const UacProcessingUnitDescriptor *unit = (const void *)entity;

			count = unit->bNrInPins;
			sources = unit->baSourceID;
		}
		if (count) {
			for (unsigned i = 0; i < count; i++)
				if (depth < 32)
					stack[depth++] = entityIndex(entities, n, sources[i]);
			depth--;
		} else if (depth) {
			depth--;
		} else {
			break;
		}
	}
	port->volumeInterface = interface->bInterfaceNumber;
}
/* a 16-bit mono format with 48 kHz among its rates, after a streaming interface */
static inline __attribute__((always_inline)) const UacFormatTypeIDescriptor *findFormat(unsigned id, const SceUsbdInterfaceDescriptor *interface)
{
	const UacFormatTypeIDescriptor *format = (const void *)interface;

	if (!interfaceMatches(interface, UAC_SUBCLASS_AUDIOSTREAMING))
		return NULL;
	for (;;) {
		format = scan(id, format, 0);
		if (!format)
			return NULL;
		if (format->bLength < 2)
			continue;
		if (format->bDescriptorType != UAC_CS_INTERFACE)
			return NULL;
		if (format->bLength < sizeof(*format) || format->bDescriptorSubtype != UAC_FORMAT_TYPE ||
		    format->bBitResolution != 16 || format->bFormatType != UAC_FORMAT_TYPE_I)
			continue;
		unsigned count = format->bSamFreqType;
		if (!count)
			return NULL;
		if (format->bLength <= sizeof(*format) + count * 3 - 1 || format->bNrChannels != 1)
			continue;
		for (unsigned r = 0; r < count; r++)
			if (sampleRate(format->tSamFreq[r]) == 48000)
				return format;
	}
}
static inline __attribute__((always_inline)) void decodeString(AudioDevice *dev, unsigned length, char *out)
{
	uint8_t *buffer = controlBuffer(dev);
	if (length < 2 || length > USB_STRING_MAX - 2 || length != buffer[0] ||
	    buffer[1] != SCE_USBD_DESCRIPTOR_STRING)
		return;
	buffer[length] = buffer[length + 1] = 0;
	char *limit = out + USB_STRING_MAX / 2;
	const uint16_t *p = (const void *)(buffer + 2);
	unsigned code;
	while ((code = readLe16(p++)) && out + 3 < limit) {
		/* UTF-16 to UTF-8 */
		if (code < 0x80) {
			*out++ = code;
		} else if (code < 0x800) {
			*out++ = (code >> 6) | 0xc0;
			*out++ = (code & 63) | 0x80;
		} else {
			*out++ = (code >> 12) | 0xe0;
			*out++ = ((code >> 6) & 63) | 0x80;
			*out++ = (code & 63) | 0x80;
		}
	}
	*out = 0;
}
static inline __attribute__((always_inline)) int requestInterface(AudioDevice *dev, const SceUsbdInterfaceDescriptor *interface,
			    unsigned state)
{
	SceUsbdDeviceRequest req;

	req.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_DEVICE | SCE_USBD_REQTYPE_TYPE_STANDARD |
			    SCE_USBD_REQTYPE_RECIP_INTERFACE;
	req.bRequest = SCE_USBD_REQUEST_SET_INTERFACE;
	req.wValue = interface->bAlternateSetting;
	req.wIndex = interface->bInterfaceNumber;
	req.wLength = 0;
	int ret = SceUsbdForDriver_2E05660F(dev->port.controlPipe, &req, NULL, completeInterface,
		(void *)dev->id);
	if (!ret)
		dev->state = state;
	return ret;
}
static inline __attribute__((always_inline)) void requestStreamInterfaces(AudioDevice *dev)
{
	if (dev->port.inputInterface && dev->port.inputEndpoint)
		requestInterface(dev, dev->port.inputInterface, DEVICE_INPUT_INTERFACE);
	else if (dev->port.outputInterface && dev->port.outputEndpoint)
		requestInterface(dev, dev->port.outputInterface, DEVICE_OUTPUT_INTERFACE);
}
static inline __attribute__((always_inline)) void requestString(AudioDevice *dev, unsigned index, unsigned state)
{
	SceUsbdDeviceRequest req = {
		.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_HOST | SCE_USBD_REQTYPE_TYPE_STANDARD |
				 SCE_USBD_REQTYPE_RECIP_DEVICE,
		.bRequest = SCE_USBD_REQUEST_GET_DESCRIPTOR,
		.wValue = SCE_USBD_DESCRIPTOR_STRING << 8 | index,
		.wIndex = USB_LANGID_EN_US,
		.wLength = USB_STRING_MAX,
	};
	if (!SceUsbdForDriver_2E05660F(dev->port.controlPipe, &req, controlBuffer(dev), completeSetup,
	    (void *)dev->id))
		dev->state = state;
}
__attribute__((noinline)) int initializeAudio(void)
{
	int ret = sceKernelInitializeFastMutex(&s_lock, "SceUsbAudio", 0x2000, 0);
	if (!ret)
		ret = sceKernelInitializeFastMutex(&s_deviceLock,
			"SceUsbAudioDeviceLock", 0x2000, 0);
	if (ret) {
		SceDebugForDriver_391B74B7("USBAUDIO: sceKernelInitializeFastMutex() failed! => 0x%x", ret);
		SceDebugForDriver_391B74B7("\n");
		return ret;
	}
	SceKernelHeapCreateOpt opt = {
		.size = sizeof(opt),
		.flags = HEAP_HAS_MAX_SIZE | HEAP_HAS_MEMBLOCK,
		.maxSize = USBAUDIO_HEAP_SIZE,
	};
	opt.memBlock = sceKernelAllocMemBlockForKernel("SceUsbAudio", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
					       USBAUDIO_HEAP_SIZE, NULL);
	SceUID heap = sceKernelCreateHeap("SceUsbAudio", 0x1000, &opt);
	s_heap = heap;
	if (heap < 0) {
		SceDebugForDriver_391B74B7("USBAUDIO: sceKernelCreateHeap() failed! => 0x%x", heap);
		SceDebugForDriver_391B74B7("\n");
		return heap;
	}
	return 0;
}
/* an empty hook Sony calls after reporting a failure */
__attribute__((noinline)) void debugHook(void)
{
	__asm__ volatile("");
}
__attribute__((noinline)) int lockAudio(void)
{
	return lockFastMutex(&s_lock);
}
__attribute__((noinline)) int unlockAudio(void)
{
	return sceKernelUnlockFastMutex(&s_lock);
}
__attribute__((noinline)) int lockDevices(void)
{
	return lockFastMutex(&s_deviceLock);
}
__attribute__((noinline)) int unlockDevices(void)
{
	return sceKernelUnlockFastMutex(&s_deviceLock);
}
__attribute__((noinline)) void *allocate(unsigned size)
{
	return s_heap < 0 ? NULL : sceKernelAllocHeapMemory(s_heap, size);
}
__attribute__((noinline)) void release(void *p)
{
	if (p)
		sceKernelFreeHeapMemory(s_heap, p);
}
static __attribute__((noinline)) void completeVolume(int result, int length, void *arg)
{
	AudioDevice *dev = arg;
	dev->controlResult = result;
	sceKernelSetEventFlag(dev->event, 1);
}
static __attribute__((noinline)) void completeInterface(int result, int length, void *arg)
{
	if (result || lockAudio())
		return;
	if (!lockDevices()) {
		AudioDevice *dev = findUnlocked((unsigned)arg);
		unlockDevices();
		if (dev && dev->state == DEVICE_INPUT_INTERFACE) {
			/* 48 kHz, as a 24-bit sampling frequency */
			uint8_t *buffer = controlBuffer(dev);
			buffer[0] = (uint8_t)48000;
			buffer[1] = (uint8_t)(48000 >> 8);
			buffer[2] = 48000 >> 16;
			SceUsbdDeviceRequest req = {
				.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_DEVICE |
						 SCE_USBD_REQTYPE_TYPE_CLASS | SCE_USBD_REQTYPE_RECIP_ENDPOINT,
				.bRequest = UAC_SET_CUR,
				.wValue = UAC_SAMPLING_FREQ_CONTROL << 8,
				.wIndex = dev->port.inputEndpoint->bEndpointAddress,
				.wLength = 3,
			};
			if (!SceUsbdForDriver_2E05660F(dev->port.controlPipe, &req, buffer,
			    completeSetup, (void *)dev->id))
				dev->state = DEVICE_SAMPLING_RATE;
		} else if (dev && dev->state == DEVICE_OUTPUT_INTERFACE) {
			notifyDevices();
			dev->state = DEVICE_READY;
		}
	}
	unlockAudio();
}
static __attribute__((noinline)) void completeSetup(int result, int length, void *arg)
{
	if (result || lockAudio())
		return;
	if (!lockDevices()) {
		AudioDevice *dev = findUnlocked((unsigned)arg);
		unlockDevices();
		if (dev) {
			switch (dev->state) {
			case DEVICE_MANUFACTURER:
				decodeString(dev, length, dev->manufacturer);
				if (dev->productId) {
					requestString(dev, dev->productId, DEVICE_PRODUCT);
					break;
				}
				/* fall through */
			case DEVICE_PRODUCT:
				if (dev->state == DEVICE_PRODUCT)
					decodeString(dev, length, dev->productName);
				if (dev->serialId) {
					requestString(dev, dev->serialId, DEVICE_SERIAL);
					break;
				}
				requestStreamInterfaces(dev);
				break;
			case DEVICE_SERIAL:
				decodeString(dev, length, dev->serial);
				requestStreamInterfaces(dev);
				break;
			case DEVICE_SAMPLING_RATE:
				if (dev->port.outputInterface)
					requestInterface(dev, dev->port.outputInterface, DEVICE_OUTPUT_INTERFACE);
				else {
					notifyDevices();
					dev->state = DEVICE_READY;
				}
				break;
			}
		}
	}
	unlockAudio();
}
__attribute__((noinline)) int addDevice(unsigned id)
{
	if (lockDevices())
		return 0;
	unsigned index;
	for (index = 0; index < 8; index++)
		if (!s_devices[index])
			break;
	if (index == 8)
		return 0;
	AudioDevice *dev = allocate(sizeof(*dev));
	if (!dev)
		return 0;
	memset(dev, 0, sizeof(*dev));
	s_devices[index] = dev;
	unlockDevices();
	dev->event = -1;
	dev->id = id;
	dev->event = sceKernelCreateEventFlag("SceUsbAudioIn", 0, 0, 0);
	if (sceKernelInitializeFastMutex(&dev->mutex, "SceUsbAudioIn", 0x2000, 0))
		return 0;
	const SceUsbdDeviceDescriptor *descriptor = scan(dev->id, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (descriptor) {
		dev->vendor = readLe16(&descriptor->idVendor);
		dev->product = readLe16(&descriptor->idProduct);
		dev->version = readLe16(&descriptor->bcdDevice);
		dev->manufacturerId = descriptor->iManufacturer;
		dev->productId = descriptor->iProduct;
		dev->serialId = descriptor->iSerialNumber;
	}
	parseDescriptors(dev->id, &dev->port);
	if (dev->port.inputInterface && dev->port.inputEndpoint) {
		dev->port.inputQueue = allocateInputQueue();
	}
	if (dev->port.outputInterface && dev->port.outputEndpoint) {
		dev->port.outputQueue = allocateOutputQueue();
	}
	dev->port.controlPipe = SceUsbdForDriver_1CDBFF9F(dev->id, NULL);
	if (dev->port.controlPipe >= 0) {
		if (dev->port.inputEndpoint) {
			dev->port.inputPipe = SceUsbdForDriver_1CDBFF9F(dev->id, dev->port.inputEndpoint);
			if (dev->port.inputPipe < 0)
				goto setup;
		}
		if (dev->port.outputEndpoint)
			dev->port.outputPipe = SceUsbdForDriver_1CDBFF9F(dev->id, dev->port.outputEndpoint);
	}
 setup:
	if (!dev->state) {
		if (dev->manufacturerId)
			requestString(dev, dev->manufacturerId, DEVICE_MANUFACTURER);
		else if (dev->productId)
			requestString(dev, dev->productId, DEVICE_PRODUCT);
		else if (dev->serialId)
			requestString(dev, dev->serialId, DEVICE_SERIAL);
		else
			requestStreamInterfaces(dev);
	}
	return 1;
}
__attribute__((noinline)) int removeDevice(unsigned id)
{
	if (lockDevices())
		return 0;
	AudioDevice *dev;
	unsigned i;
	for (i = 0; i < 8; i++) {
		dev = s_devices[i];
		if (dev && dev->id == id)
			break;
	}
	if (i == 8) {
		unlockDevices();
		return 0;
	}
	releaseQueue(dev->port.inputQueue);
	dev->port.inputQueue = NULL;
	releaseQueue(dev->port.outputQueue);
	dev->port.outputQueue = NULL;
	if (dev->port.inputFormat.rates) {
		release(dev->port.inputFormat.rates);
		dev->port.inputFormat.rates = NULL;
	}
	if (dev->port.outputFormat.rates) {
		release(dev->port.outputFormat.rates);
		dev->port.outputFormat.rates = NULL;
	}
	sceKernelDeleteEventFlag(dev->event);
	release(dev);
	s_devices[i] = NULL;
	notifyDevices();
	unlockDevices();
	return 1;
}
__attribute__((noinline)) AudioPort *findPort(unsigned id)
{
	if (lockDevices())
		return NULL;
	AudioDevice *dev = findUnlocked(id);
	unlockDevices();
	return dev ? &dev->port : NULL;
}
__attribute__((noinline)) int SceUsbAudioForDriver_96EC3E18(unsigned *list, int size)
{
	if (!list)
		return AUDIO_ERROR(INVALID_ARGUMENT);
	if (lockDevices())
		return 0;
	int n = 0;
	for (unsigned i = 0; i < 8; i++) {
		AudioDevice *dev = s_devices[i];
		if (dev && dev->state == DEVICE_READY) {
			list[n++] = dev->id;
			if (n >= (size > 7 ? 8 : size))
				break;
		}
	}
	unlockDevices();
	return n;
}
__attribute__((noinline)) int getCurrentVolume(unsigned id, int *volume)
{
	return volumeRequest(id, UAC_GET_CUR, 0, volume);
}
__attribute__((noinline)) int setCurrentVolume(unsigned id, unsigned value)
{
	return volumeRequest(id, UAC_SET_CUR, value, NULL);
}
__attribute__((noinline)) int getMaxVolume(unsigned id, int *volume)
{
	return volumeRequest(id, UAC_GET_MAX, 0, volume);
}
__attribute__((noinline)) int getMinVolume(unsigned id, int *volume)
{
	return volumeRequest(id, UAC_GET_MIN, 0, volume);
}
/* the endpoint of a streaming interface's alternate setting */
static __attribute__((noinline)) SceUsbdEndpointDescriptor *streamEndpoint(unsigned id, const SceUsbdInterfaceDescriptor *interface)
{
	const UacDescriptor *p = (const void *)interface;

	if (!interfaceMatches(interface, UAC_SUBCLASS_AUDIOSTREAMING))
		return NULL;
	for (;;) {
		p = scan(id, p, 0);
		if (!p)
			return NULL;
		if (p->bLength < 2)
			continue;
		if (p->bDescriptorType == SCE_USBD_DESCRIPTOR_INTERFACE) {
			const SceUsbdInterfaceDescriptor *next = (const void *)p;

			if (!interfaceMatches(next, UAC_SUBCLASS_AUDIOSTREAMING) ||
			    next->bInterfaceNumber != interface->bInterfaceNumber)
				return NULL;
		} else if (p->bDescriptorType != UAC_CS_INTERFACE) {
			return p->bDescriptorType == SCE_USBD_DESCRIPTOR_ENDPOINT &&
			       p->bLength >= 7 ? (void *)p : NULL;
		}
	}
}
__attribute__((noinline)) uint8_t *audioInterface(unsigned id)
{
	uint8_t *p = NULL, *first = NULL;
	unsigned control = 0, stream = 0;
	for (;;) {
		p = scan(id, p, 4);
		if (!p)
			return NULL;
		if (p[0] < 9 || p[5] != 1)
			continue;
		if (p[6] == 1 || p[6] == 2) {
			if (!first)
				first = p;
			if (p[6] == 1)
				control = 1;
			else
				stream = 1;
		}
		if (control && stream)
			return first;
	}
}
static __attribute__((noinline)) void parseDescriptors(unsigned id, AudioPort *port)
{
	const SceUsbdInterfaceDescriptor *control = NULL;
	const UacHeaderDescriptor *header;

	do {
		control = scan(id, control, SCE_USBD_DESCRIPTOR_INTERFACE);
		if (!control)
			return;
	} while (!interfaceMatches(control, UAC_SUBCLASS_AUDIOCONTROL));
	header = controlHeader(id, control);
	if (header && header->bInCollection) {
		SceUsbdInterfaceDescriptor *input = NULL, *output = NULL;

		for (unsigned c = 0; c < header->bInCollection; c++) {
			SceUsbdInterfaceDescriptor *interface = NULL;
			unsigned number = header->baInterfaceNr[c];

			for (;;) {
				interface = scan(id, interface, SCE_USBD_DESCRIPTOR_INTERFACE);
				if (!interface)
					break;
				if (!interfaceMatches(interface, UAC_SUBCLASS_AUDIOSTREAMING) ||
				    interface->bInterfaceNumber != number ||
				    !interface->bNumEndpoints)
					continue;
				const UacAsGeneralDescriptor *general = streamGeneral(id, interface);
				const UacFormatTypeIDescriptor *format = findFormat(id, interface);
				if (!format)
					continue;
				AudioFormat info;
				info.supported = info.channels = 1;
				info.bits = 16;
				info.reserved = 0;
				info.rateCount = format->bSamFreqType;
				info.rates = allocate(info.rateCount * sizeof(*info.rates));
				for (unsigned i = 0; i < info.rateCount; i++)
					info.rates[i] = sampleRate(format->tSamFreq[i]);
				if (!general)
					continue;
				/* the terminal the stream links to says its direction */
				const UacEntityDescriptor *terminal = (const void *)control;
				for (;;) {
					terminal = scan(id, terminal, 0);
					if (!terminal)
						break;
					if (terminal->bLength < 2)
						continue;
					if (terminal->bDescriptorType != UAC_CS_INTERFACE) {
						terminal = NULL;
						break;
					}
					if (((terminal->bLength >= 12 &&
					      terminal->bDescriptorSubtype == UAC_INPUT_TERMINAL) ||
					     (terminal->bLength >= 9 &&
					      terminal->bDescriptorSubtype == UAC_OUTPUT_TERMINAL)) &&
					    terminal->bEntityID == general->bTerminalLink)
						break;
				}
				if (terminal) {
					if (terminal->bDescriptorSubtype == UAC_OUTPUT_TERMINAL) {
						port->outputFormat = info;
						if (!input)
							input = interface;
					} else {
						port->inputFormat = info;
						if (!output)
							output = interface;
					}
				}
			}
			if (input && output)
				break;
		}
		if (input || output) {
			if (input)
				port->inputInterface = input;
			if (output)
				port->outputInterface = output;
			port->channels = input ? 1 : 0;
		}
	}
	port->inputEndpoint = streamEndpoint(id, port->inputInterface);
	port->outputEndpoint = streamEndpoint(id, port->outputInterface);
	findVolumeUnit(id, port);
}
/* the device descriptor's vendor and product */
__attribute__((noinline)) int getDeviceIds(unsigned id, uint16_t *ids)
{
	if (!ids)
		return SCE_ERROR_ERRNO_EINVAL;
	const SceUsbdDeviceDescriptor *descriptor = scan(id, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (!descriptor)
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	ids[0] = readLe16(&descriptor->idVendor);
	ids[1] = readLe16(&descriptor->idProduct);
	return 0;
}
static __attribute__((noinline)) void notifyDevices(void)
{
	struct AudioCallback *head = &s_callbacks;
	if (head->next && head->prev)
		for (struct AudioCallback *p = head->next; p != head; p = p->next)
			sceKernelNotifyCallback(p->callback, 0);
}
int SceUsbAudioForDriver_974DBE03(int callback)
{
	int ret = lockAudio();
	if (ret)
		return ret;
	struct AudioCallback *head = &s_callbacks;
	if (!head->next || !head->prev) {
		head->next = head->prev = head;
	} else {
		for (struct AudioCallback *p = head->next; p != head; p = p->next)
			if (p->callback == callback) {
				ret = AUDIO_ERROR(DUPLICATE_ID);
				goto out;
			}
	}
	struct AudioCallback *p = allocate(sizeof(*p));
	if (!p) {
		ret = AUDIO_ERROR(NO_MEMORY);
		goto out;
	}
	for (unsigned i = 0; i < sizeof(*p); i++)
		((uint8_t *)p)[i] = 0;
	p->prev = head->prev;
	p->callback = callback;
	p->next = head;
	head->prev->next = p;
	head->prev = p;
	sceKernelNotifyCallback(callback, 0);
out:
	unlockAudio();
	return ret;
}
int SceUsbAudioForDriver_23A07D29(int callback)
{
	int ret = lockAudio();
	if (ret)
		return ret;
	struct AudioCallback *head = &s_callbacks;
	if (head->next && head->prev) {
		for (struct AudioCallback *p = head->next; p != head; p = p->next)
			if (p->callback == callback) {
				p->prev->next = p->next;
				p->next->prev = p->prev;
				release(p);
				break;
			}
	}
	unlockAudio();
	return ret;
}
static __attribute__((noinline)) void completeCapture(int result, UsbdIsochRequest *packet, void *arg)
{
	if (lockAudio())
		return;
	AudioPacket *done = arg;
	AudioPort *port = done->owner;
	AudioQueue *queue = port->inputQueue;
	if (queue->completed < 2) {
		queue->done[queue->completeWrite] = done;
		queue->completed++;
		queue->completeWrite = (queue->completeWrite + 1) % 2;
	}
	if (queue->pending > 0) {
		AudioPacket *next = queue->queued[queue->read];
		queue->pending--;
		queue->read = (queue->read + 1) % 2;
		if (next)
			submitIso(port->inputPipe, &next->request, completeCapture, next);
	}
	unlockAudio();
}
static __attribute__((noinline)) void completeOutput(int result, UsbdIsochRequest *packet, void *arg)
{
	if (lockAudio())
		return;
	AudioPacket *done = arg;
	AudioPort *port = done->owner;
	AudioQueue *queue = port->outputQueue;
	done->owner = NULL;
	if (queue->pending > 0) {
		AudioPacket *next = queue->queued[queue->read];
		queue->pending--;
		queue->read = (queue->read + 1) % 2;
		if (next)
			submitIso(port->outputPipe, &next->request, completeOutput, next);
	}
	unlockAudio();
}
static __attribute__((noinline)) AudioQueue *allocateInputQueue(void)
{
	AudioQueue *queue = allocate(sizeof(AudioQueue));
	if (queue)
		memset(queue, 0, sizeof(AudioQueue));
	return queue;
}
/* playback has no completion ring */
static __attribute__((noinline)) AudioQueue *allocateOutputQueue(void)
{
	AudioQueue *queue = allocate(offsetof(AudioQueue, completed));
	if (queue)
		memset(queue, 0, offsetof(AudioQueue, completed));
	return queue;
}
static __attribute__((noinline)) void releaseQueue(AudioQueue *queue)
{
	release(queue);
}
int SceUsbAudioForDriver_EFB4E3E4(unsigned id, void *buffer, unsigned size)
{
	unsigned packets = size / 192;
	if (size % 192 || packets - 1 > 7 || !buffer)
		return AUDIO_ERROR(INVALID_ARGUMENT);
	int ret = lockAudio();
	if (ret)
		return ret;
	AudioPort *port = findPort(id);
	if (!port) {
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
		goto out;
	}
	AudioQueue *queue = port->inputQueue;
	if (!queue) {
		ret = AUDIO_ERROR(NOT_SUPPORTED);
		goto out;
	}
	ret = 0;
	if (queue->completed > 0) {
		AudioPacket *done = queue->done[queue->completeRead];
		queue->completed--;
		queue->completeRead = (queue->completeRead + 1) % 2;
		if (done) {
			unsigned bytes = 0;
			for (int i = 0; i < done->request.packets && i < 8; i++) {
				if (done->request.packet[i].status) {
					bytes = 0;
					break;
				}
				bytes += done->request.packet[i].length;
			}
			if (port->channels == 1) {
				bytes *= 2;
				if ((int)bytes >= (int)size)
					bytes = size;
				int samples = (int)bytes / 2;
				uint8_t *out = buffer, *in = done->request.buffer;
				for (int i = 0; i < samples; i += 2) {
					out[0] = out[2] = in[i];
					out[1] = out[3] = in[i + 1];
					out += 4;
				}
				ret = samples * 2;
			} else {
				if ((int)bytes >= (int)size)
					bytes = size;
				memcpy(buffer, done->request.buffer, bytes);
				ret = bytes;
			}
			done->owner = NULL;
		}
	}
	AudioPacket *packet = freePacket(queue);
	if (packet) {
		flushQueue(port, queue, 1);
		memset(packet, 0, sizeof(*packet));
		packet->request.buffer = packet->data;
		packet->request.packets = packets;
		for (unsigned i = 0; i < packets; i++)
			packet->request.packet[i].length = port->channels == 1 ? 96 : 192;
		packet->owner = port;
		queuePacket(queue, packet);
	}
 out:
	unlockAudio();
	return ret;
}
int SceUsbAudioForDriver_863B0617(unsigned id, void *buffer, unsigned size)
{
	unsigned packets = size / 192;
	if (size % 192 || packets - 1 > 7 || !buffer)
		return AUDIO_ERROR(INVALID_ARGUMENT);
	int ret = lockAudio();
	if (ret)
		return ret;
	AudioPort *port = findPort(id);
	if (!port) {
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
		goto out;
	}
	AudioQueue *queue = port->outputQueue;
	if (!queue) {
		ret = AUDIO_ERROR(NOT_SUPPORTED);
		goto out;
	}
	AudioPacket *packet = freePacket(queue);
	if (packet) {
		flushQueue(port, queue, 0);
		memset(packet, 0, sizeof(*packet));
		packet->request.buffer = memcpy(packet->data, buffer, size);
		packet->request.packets = packets;
		for (unsigned i = 0; i < packets; i++)
			packet->request.packet[i].length = 192;
		packet->owner = port;
		queuePacket(queue, packet);
	}
 out:
	unlockAudio();
	return ret;
}
