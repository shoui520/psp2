/*
 * SceUsbPspcm: PSP communication over USB. Internal Sony names unknown.
 * Both device paths and all four firmwares share this implementation.
 * Channel and descriptor layouts retain the original ABI and buffer ownership.
 */
#include <stddef.h>
#include <stdint.h>
#include <udcd/udcd.h>
#include <acmgr/acmgr.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include "usbpspcm.h"

/*
 * The SDK error index has no names for this range (0x802445xx); these are
 * named after where each is returned. 4 and 14 are not used here.
 */
#define PCM_ERROR_ALREADY		((int)0x80244501)	/* already opened or registered */
#define PCM_ERROR_ARGUMENT		((int)0x80244502)
#define PCM_ERROR_NO_UNIT		((int)0x80244503)	/* no free channel */
#define PCM_ERROR_NOT_FOUND		((int)0x80244505)	/* no such port or callback */
#define PCM_ERROR_BUSY			((int)0x80244506)
#define PCM_ERROR_DISCONNECTED		((int)0x80244507)
#define PCM_ERROR_TIMEOUT		((int)0x80244508)
#define PCM_ERROR_CANCELLED		((int)0x80244509)
#define PCM_ERROR_RESET			((int)0x8024450a)	/* a reset is in progress */
#define PCM_ERROR_RESET_RECEIVED	((int)0x8024450b)	/* the host reset the channel */
#define PCM_ERROR_STATE			((int)0x8024450c)	/* the channel is not connected */
#define PCM_ERROR_NOT_READY		((int)0x8024450d)	/* the driver is not started */

#define PCM_UNITS		8
#define PCM_PACKET_MAX		0x1000	/* the data of one packet */
#define PCM_CACHE_LINE		64
#define PCM_ROUND_CACHE(n)	(((unsigned int)(n) + PCM_CACHE_LINE - 1) & ~(PCM_CACHE_LINE - 1))
#define PCM_WAIT_TIMEOUT	3000000		/* microseconds */

/* the header in front of every packet on the bulk pipes */
typedef struct PcmHeader {
	uint16_t unit;
	uint16_t flags;			/* PCM_HEADER_* */
	uint32_t length;		/* of the data packet that follows */
} PcmHeader;

#define PCM_HEADER_DATA		0x0001
#define PCM_HEADER_RESET	0x0002
#define PCM_HEADER_LAST		0x0080	/* the last packet of a message */

/* a request to the host on the interrupt pipe */
typedef struct PcmCommand {
	uint32_t command;		/* PCM_COMMAND_* */
	uint32_t argument;		/* the unit, CONNECT: port << 8 | unit */
} PcmCommand;

#define PCM_COMMAND_CONNECT	4
#define PCM_COMMAND_CLOSE	5
#define PCM_COMMAND_CANCEL	6

/* the configuration the host sends with PCM_REQUEST_SET_CONFIG */
typedef struct PcmHostConfig {
	int ports;
	int units;
	int packetSize;			/* PCM_PACKET_MAX */
} PcmHostConfig;

/* vendor requests to the interface */
#define PCM_REQUEST_OUT			0x41
#define PCM_REQUEST_IN			0xc1
#define PCM_REQUEST_GET_TITLE		1	/* in */
#define PCM_REQUEST_SET_CONFIG		2	/* out */
#define PCM_REQUEST_SET_PORTS		3	/* out: the connected ports */
#define PCM_REQUEST_ACCEPT		4	/* in: the host accepts a CONNECT */
#define PCM_REQUEST_CLOSE		5	/* out */
#define PCM_REQUEST_CANCEL		6	/* out */
#define PCM_REQUEST_SET_VERSION		7	/* out */
#define PCM_REQUEST_GET_FEATURES	8	/* in */

/* sceUsbPspcmDevctl commands (names unknown) */
#define PCM_DEVCTL_GET_HOST_PORT	0x3407003
#define PCM_DEVCTL_REGISTER_CALLBACK	0x3415001
#define PCM_DEVCTL_UNREGISTER_CALLBACK	0x3415002
#define PCM_DEVCTL_GET_PORTS		0x3427004
#define PCM_DEVCTL_CONNECT		0x3435005
#define PCM_DEVCTL_CANCEL_CONNECT	0x3415006
#define PCM_DEVCTL_3416102		0x3416102	/* takes a word; no handler */
#define PCM_DEVCTL_3415104		0x3415104	/* takes a word; no handler */

/* the connection callback's event: (port, notification) */
#define PCM_NOTIFY_ARRIVED		0x01
#define PCM_NOTIFY_DEPARTED		0x02
#define PCM_NOTIFY_HOST			0x80	/* about the host port */

/* s.flags */
#define PCM_FLAG_ATTACHED		0x01
#define PCM_FLAG_BUFFER_WAIT		0x02	/* a data packet waits for a free buffer */
#define PCM_STATE_MASK			0x0c
#define PCM_STATE_READY			0x04
#define PCM_STATE_NOTIFYING		0x08	/* the connection callback runs */

/* the channel event flag */
#define PCM_EVENT_SENT			0x0001	/* a header or data packet went out */
#define PCM_EVENT_READ			0x0002	/* a waiting read has its data */
#define PCM_EVENT_COMMAND_DONE		0x0004
#define PCM_EVENT_DISCONNECTED		0x0008	/* a command failed */
#define PCM_EVENT_ACCEPTED		0x0010
#define PCM_EVENT_CANCELLED		0x0020
#define PCM_EVENT_CLOSED		0x0040
#define PCM_EVENT_RESET			0x0080	/* our reset is in progress */
#define PCM_EVENT_RESET_RECEIVED	0x0100
#define PCM_EVENT_DATA			0x0200	/* a packet arrived */
#define PCM_EVENT_WAIT_CANCEL		0x0400
#define PCM_EVENT_SEND_FAILED		0x1000
#define PCM_EVENT_RESERVED		0x2000	/* cleared before a read; never set */

/* s.event: the connection thread has notifications to deliver */
#define PCM_THREAD_EVENT_CHANGED	1

/* Channel.state */
#define PCM_CHANNEL_IDLE		0
#define PCM_CHANNEL_CONNECTING		1
#define PCM_CHANNEL_CANCELLING		2
#define PCM_CHANNEL_CONNECTED		3

/* Channel.reset */
#define PCM_RESET_NONE			0
#define PCM_RESET_SENT			1
#define PCM_RESET_RECEIVED		2

/* Channel.busy */
#define PCM_BUSY_READ			0x01
#define PCM_BUSY_WRITE			0x02
#define PCM_BUSY_RESET			0x04
#define PCM_BUSY_WAIT			0x08

/* Channel.mode, from sceUsbPspcmRead/Write */
#define PCM_MODE_MESSAGE		0x01	/* a read ends with its message */
#define PCM_MODE_CONTINUED		0x02	/* an exact 4 KiB write does not end the message */

struct Buffer {
	uint8_t *data;
	unsigned int size;
	struct Buffer *next;
};

struct BufferQueue {
	struct Buffer *head;
	struct Buffer *tail;
};

struct RequestSlot {
	SceUdcdDeviceRequest request;
	uint8_t reserved[20];		/* the bus driver's tail */
	PcmHeader *header;
};

struct Channel {
	unsigned int unit;
	int port;
	int state;			/* PCM_CHANNEL_* */
	int reset;			/* PCM_RESET_* */
	unsigned int mode;		/* PCM_MODE_* */
	int opened;
	unsigned int busy;		/* PCM_BUSY_* */
	unsigned int user;		/* the caller's buffers are user memory */
	struct RequestSlot resetRequest;
	struct RequestSlot headerRequest;
	SceUdcdDeviceRequest dataRequest;
	uint8_t dataReserved[20];
	uint8_t *write;
	unsigned int remaining;
	struct BufferQueue queue;
	uint8_t *read;
	unsigned int readRemaining;
	SceUdcdDeviceRequest command;
	uint8_t commandReserved[20];
	PcmCommand *commandBuffer;
	struct Buffer *partial;
	unsigned int offset;
	uint8_t *receiveBuffer;
	uint8_t *sendBuffer;
	struct Buffer *buffers;
	SceUID event;
};

_Static_assert(offsetof(struct Channel, write) == 0xe8, "channel write layout");
_Static_assert(sizeof(struct Channel) == 0x15c, "channel layout");

struct InterfaceDescriptors {
	SceUdcdInterfaceDescriptor descriptor[2];
};

struct EndpointDescriptors {
	SceUdcdEndpointDescriptor descriptor[4];
};

struct DescriptorSet {
	SceUdcdDeviceDescriptor device;
	SceUdcdConfiguration configuration;
	SceUdcdConfigDescriptor config;
	SceUdcdInterfaceSettings settings;
	struct InterfaceDescriptors interfaces;
	struct EndpointDescriptors endpoints;
};

_Static_assert(sizeof(struct DescriptorSet) == 0xb8, "descriptor layout");

/* the "SceUsbPspcm" memblock */
struct PcmMemory {
	uint8_t control[PCM_CACHE_LINE];
	uint8_t header[0x200];
	struct DescriptorSet high;
	struct DescriptorSet full;
	struct Channel channels[PCM_UNITS];
};

/* one unit's part of the "SceUsbPspcmUnit" memblock, cache-line aligned */
struct UnitMemory {
	uint8_t receive[2][PCM_PACKET_MAX];
	uint8_t send[PCM_PACKET_MAX];
	PcmHeader header __attribute__((aligned(PCM_CACHE_LINE)));
	PcmHeader resetHeader __attribute__((aligned(PCM_CACHE_LINE)));
	PcmCommand command __attribute__((aligned(PCM_CACHE_LINE)));
	struct Buffer buffers[2] __attribute__((aligned(PCM_CACHE_LINE)));
} __attribute__((aligned(PCM_CACHE_LINE)));

#define PCM_UNIT_SIZE	(offsetof(struct UnitMemory, buffers) + sizeof(struct Buffer[2]))

_Static_assert(offsetof(struct PcmMemory, high) == 0x240, "descriptor offset");
_Static_assert(offsetof(struct PcmMemory, channels) == 0x3b0, "channel offset");
_Static_assert(offsetof(struct UnitMemory, header) == 0x3000, "unit header offset");
_Static_assert(offsetof(struct UnitMemory, buffers) == 0x30c0, "unit buffer offset");
_Static_assert(sizeof(struct UnitMemory) == 0x3100, "unit stride");

static struct {
	int ports;
	unsigned int count;
	struct Channel *channels[PCM_UNITS];
	SceUID events[PCM_UNITS];
	char title[64];
	unsigned int titleLength;
	unsigned int hostVersion;
	int hostPort;
	unsigned int connected;		/* a bit per connected port */
	unsigned int flags;		/* PCM_FLAG_*, PCM_STATE_* */
	unsigned int packetSize;
	SceUdcdEP0DeviceRequest setup;
	SceUdcdDeviceRequest control;
	uint8_t controlReserved[20];
	uint8_t *controlBuffer;
	SceUdcdDeviceRequest header;
	uint8_t headerReserved[20];
	PcmHeader *headerBuffer;
	SceUdcdDeviceRequest receive;
	uint8_t receiveReserved[20];
	struct BufferQueue free;
	struct Buffer marker;		/* queued for an empty message end */
	uint32_t reserved0;
	SceUID memblock;
	SceUID unitMemblock;
	SceUID sema;
	int lock;
	uint32_t reserved1[2];
	SceUdcdDriver driver;
	SceUdcdStringDescriptor strings;
	uint8_t reserved2[64];
	SceUdcdInterface interface;
	SceUdcdEndpoint endpoints[4];
	unsigned int notification;	/* PCM_NOTIFY_* for the host port */
	int oldPort;
	unsigned int changes;
	unsigned int arrivals;
	unsigned int departures;
	SceUID callbackThread;
	SceUID thread;
	SceUID event;
	SceUID notificationSema;
} s;

_Static_assert(offsetof(__typeof__(s), driver) == 0x1a0, "driver layout");
_Static_assert(sizeof(s) == 0x2e4, "module state layout");

static int startCommunication(int size, void *args, void *context);
static int stopCommunication(int size, void *args, void *context);
static int attachCommunication(int speed, void *context);
static void detachCommunication(void *context);
static int processRequest(int recipient, int endpoint, SceUdcdEP0DeviceRequest *request, void *context);
static int connectionThread(SceSize size, void *args);
static void controlComplete(SceUdcdDeviceRequest *request);
static void headerComplete(SceUdcdDeviceRequest *request);
static void receiveComplete(SceUdcdDeviceRequest *request);
static void commandComplete(SceUdcdDeviceRequest *request);
static void resetComplete(SceUdcdDeviceRequest *request);
static void dataComplete(SceUdcdDeviceRequest *request);
static unsigned lock(void)
{
	return SceCpuForDriver_D32ACE9E(&s.lock);
}

static void unlock(unsigned state)
{
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s.lock, state);
}

static void appendBuffer(struct BufferQueue *queue, struct Buffer *buffer)
{
	if (buffer == NULL)
		return;
	if (queue->tail != NULL)
		queue->tail->next = buffer;
	else
		queue->head = buffer;
	queue->tail = buffer;
	buffer->next = NULL;
}

static struct Buffer *takeBuffer(struct BufferQueue *queue)
{
	struct Buffer *buffer = queue->head;

	if (buffer != NULL) {
		queue->head = buffer->next;
		if (buffer->next == NULL)
			queue->tail = NULL;
		buffer->next = NULL;
	}
	return buffer;
}

static void receiveHeader(void)
{
	SceCpuForDriver_02796361(s.header.data, PCM_ROUND_CACHE(s.header.size));
	s.header.returnCode = 1;
	SceUdcdForDriver_C60A74B2(&s.header);
}

static void releaseBuffer(struct Buffer *buffer)
{
	if (!buffer || buffer == &s.marker)
		return;
	if (s.flags & PCM_FLAG_BUFFER_WAIT) {
		s.flags &= ~PCM_FLAG_BUFFER_WAIT;
		if (s.flags & PCM_FLAG_ATTACHED) {
			s.receive.data = buffer->data;
			s.receive.size = s.headerBuffer->length;
			s.receive.onComplete = receiveComplete;
			s.receive.returnCode = 1;
			s.receive.unused = buffer;
			SceCpuForDriver_02796361(buffer->data, PCM_ROUND_CACHE(s.receive.size));
			SceUdcdForDriver_C60A74B2(&s.receive);
			return;
		}
	}
	appendBuffer(&s.free, buffer);
}

static void releaseChannel(struct Channel *channel)
{
	struct Buffer *buffer;
	if (channel->partial) {
		releaseBuffer(channel->partial);
		channel->partial = NULL;
	}
	while ((buffer = takeBuffer(&channel->queue)))
		releaseBuffer(buffer);
}

static void resetChannelState(struct Channel *channel)
{
	channel->port = -1;
	channel->state = PCM_CHANNEL_IDLE;
	channel->reset = PCM_RESET_NONE;
	channel->mode = 0;
	if (channel->opened > 0)
		channel->opened = -1;
}

static void commandComplete(SceUdcdDeviceRequest *request)
{
	struct Channel *channel = request->unused;
	if (request->returnCode >= 0)
		sceKernelSetEventFlag(channel->event, PCM_EVENT_COMMAND_DONE);
	else if (channel->state == PCM_CHANNEL_CANCELLING)
		sceKernelSetEventFlag(channel->event, PCM_EVENT_CANCELLED);
	else
		sceKernelSetEventFlag(channel->event, PCM_EVENT_DISCONNECTED);
}

static void resetComplete(SceUdcdDeviceRequest *request)
{
	struct Channel *channel = request->unused;
	sceKernelSetEventFlag(channel->event, request->returnCode ? PCM_EVENT_SEND_FAILED : PCM_EVENT_SENT);
}

static void dataComplete(SceUdcdDeviceRequest *request)
{
	struct Channel *channel = request->unused;
	if (!request->returnCode && !channel->remaining)
		channel->write = NULL;
	sceKernelSetEventFlag(channel->event, request->returnCode ? PCM_EVENT_SEND_FAILED : PCM_EVENT_SENT);
}

static int disconnectChannel(struct Channel *channel, int command)
{
	unsigned int bits, timeout = PCM_WAIT_TIMEOUT;
	int ret;
	resetChannelState(channel);
	if (command == PCM_COMMAND_CANCEL)
		channel->state = PCM_CHANNEL_CANCELLING;
	ret = SceUdcdForDriver_AA5FD90E(&channel->command);
	if (ret == (int)SCE_UDCD_ERROR_DRIVER_IN_PROGRESS) {
		ret = sceKernelWaitEventFlag(channel->event,
					      PCM_EVENT_DISCONNECTED | PCM_EVENT_CANCELLED,
					      SCE_KERNEL_EVF_WAITMODE_OR, &bits, &timeout);
		if (ret < 0)
			return ret;
		if (bits & PCM_EVENT_DISCONNECTED) {
			if (command == PCM_COMMAND_CANCEL)
				channel->state = PCM_CHANNEL_IDLE;
			return PCM_ERROR_DISCONNECTED;
		}
	}
	sceKernelClearEventFlag(channel->event, ~PCM_EVENT_COMMAND_DONE);
	channel->commandBuffer->command = command;
	channel->commandBuffer->argument = channel->unit;
	SceCpuForDriver_364E68A4(channel->commandBuffer, PCM_CACHE_LINE);
	if (command == PCM_COMMAND_CLOSE) {
		unsigned state = lock();
		releaseChannel(channel);
		unlock(state);
	}
	SceUdcdForDriver_1ED0E89E(&channel->command);
	timeout = PCM_WAIT_TIMEOUT;
	ret = sceKernelWaitEventFlag(channel->event,
				      PCM_EVENT_COMMAND_DONE | PCM_EVENT_DISCONNECTED |
				      PCM_EVENT_CANCELLED, SCE_KERNEL_EVF_WAITMODE_OR, NULL,
				      &timeout);
	if (command == PCM_COMMAND_CANCEL && ret >= 0)
		channel->state = PCM_CHANNEL_IDLE;
	return ret;
}

static const SceUdcdDeviceDescriptor s_deviceTemplate = {
	.bLength = 18,
	.bDescriptorType = 1,
	.bcdUSB = 0x200,
	.bcdDevice = 0x100,
	.bNumConfigurations = 1,
};

static const SceUdcdConfigDescriptor s_configTemplate = {
	.bLength = 9,
	.bDescriptorType = 2,
	.wTotalLength = 0x27,
	.bNumInterfaces = 1,
	.bConfigurationValue = 1,
	.bmAttributes = 0xc0,
};

static const SceUdcdInterfaceSettings s_settingsTemplate = {
	.numDescriptors = 1,
};

static const struct InterfaceDescriptors s_interfaceTemplate = {
	.descriptor = {
		{	/* vendor specific */
			.bLength = 9,
			.bDescriptorType = 4,
			.bNumEndpoints = 3,
			.bInterfaceClass = 0xff,
			.bInterfaceSubClass = 1,
			.bInterfaceProtocol = 0xff,
			.iInterface = 1,
		},
	},
};

static const struct EndpointDescriptors s_endpointTemplate = {
	.descriptor = {
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x81, .bmAttributes = 2 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x02, .bmAttributes = 2 },
		{ .bLength = 7, .bDescriptorType = 5, .bEndpointAddress = 0x83, .bmAttributes = 3 },
	},
};

static const uint16_t s_productName[12] = {
	'U', 'S', 'B', ' ', 'P', 'S', 'P', ' ', 'C', 'o', 'm', 'm',
};

static void initDescriptor(struct DescriptorSet *set, int highSpeed)
{
	set->device = s_deviceTemplate;
	set->config = s_configTemplate;
	set->settings = s_settingsTemplate;
	set->interfaces = s_interfaceTemplate;
	set->endpoints = s_endpointTemplate;
	set->configuration.configDescriptors = &set->config;
	set->configuration.settings = &set->settings;
	set->configuration.interfaceDescriptors = set->interfaces.descriptor;
	set->configuration.endpointDescriptors = set->endpoints.descriptor;
	set->settings.descriptors = set->interfaces.descriptor;
	set->config.settings = &set->settings;
	set->interfaces.descriptor[0].endpoints = set->endpoints.descriptor;
	set->device.bMaxPacketSize0 = 64;
	set->endpoints.descriptor[0].wMaxPacketSize = highSpeed ? 512 : 64;
	set->endpoints.descriptor[1].wMaxPacketSize = highSpeed ? 512 : 64;
	set->endpoints.descriptor[2].wMaxPacketSize = 8;
	set->endpoints.descriptor[2].bInterval = highSpeed ? 4 : 8;
}

static void initChannel(struct Channel *channel, unsigned int unit, struct UnitMemory *memory)
{
	channel->sendBuffer = memory->send;
	channel->commandBuffer = &memory->command;
	channel->receiveBuffer = memory->receive[0];
	channel->buffers = memory->buffers;
	channel->resetRequest.header = &memory->resetHeader;
	channel->headerRequest.header = &memory->header;
	channel->dataRequest.endpoint = &s.endpoints[1];
	channel->dataRequest.data = NULL;
	channel->buffers[1].data = memory->receive[1];
	channel->buffers[0].data = memory->receive[0];
	channel->port = -1;
	channel->state = PCM_CHANNEL_IDLE;
	channel->reset = PCM_RESET_NONE;
	channel->mode = 0;
	channel->opened = 0;
	channel->busy = 0;
	channel->unit = unit;
	memory->resetHeader.unit = unit;
	memory->resetHeader.flags = PCM_HEADER_RESET;
	memory->resetHeader.length = 0;
	memory->header.unit = unit;
	channel->resetRequest.request.data = &memory->resetHeader;
	channel->write = NULL;
	channel->remaining = 0;
	channel->read = NULL;
	channel->readRemaining = 0;
	channel->queue.head = channel->queue.tail = channel->partial = NULL;
	channel->offset = 0;
	channel->resetRequest.request.endpoint = &s.endpoints[1];
	channel->resetRequest.request.size = sizeof(PcmHeader);
	channel->resetRequest.request.isControlRequest = 0;
	channel->resetRequest.request.next = NULL;
	channel->resetRequest.request.unused = NULL;
	channel->resetRequest.request.transmitted = 0;
	channel->resetRequest.request.returnCode = 1;
	channel->headerRequest.request.endpoint = &s.endpoints[1];
	channel->headerRequest.request.data = &memory->header;
	channel->headerRequest.request.size = sizeof(PcmHeader);
	channel->headerRequest.request.isControlRequest = 0;
	channel->headerRequest.request.next = NULL;
	channel->headerRequest.request.unused = channel;
	channel->headerRequest.request.transmitted = 0;
	channel->headerRequest.request.returnCode = 1;
	channel->headerRequest.request.onComplete = resetComplete;
	channel->dataRequest.attributes = 0;
	channel->dataRequest.next = NULL;
	channel->dataRequest.unused = channel;
	channel->dataRequest.onComplete = dataComplete;
	channel->dataRequest.transmitted = 0;
	channel->dataRequest.returnCode = 1;
	channel->command.endpoint = &s.endpoints[3];
	channel->command.data = channel->commandBuffer;
	channel->command.size = sizeof(PcmCommand);
	channel->command.isControlRequest = 0;
	channel->command.next = NULL;
	channel->command.unused = channel;
	channel->command.onComplete = commandComplete;
	channel->resetRequest.request.onComplete = NULL;
	channel->resetRequest.request.attributes = 0;
	channel->headerRequest.request.attributes = 0;
	channel->dataRequest.isControlRequest = 0;
	channel->dataRequest.size = PCM_PACKET_MAX;
	channel->command.attributes = 0;
	channel->command.transmitted = 0;
	channel->command.returnCode = 1;
	SceCpuForDriver_364E68A4(&memory->resetHeader, PCM_CACHE_LINE);
	s.count++;
	channel->event = s.events[unit];
}

/* chain every unit's two receive buffers into the free queue */
static void initFreeBuffers(unsigned int count)
{
	unsigned int i;

	s.free.head = s.channels[0]->buffers;
	for (i = 0; (int)i < (int)count; i++) {
		struct Buffer *buffers = s.channels[i]->buffers;

		buffers[0].next = &buffers[1];
		buffers[1].next = i + 1 < count ? s.channels[i + 1]->buffers : NULL;
		if (i + 1 == count)
			s.free.tail = &buffers[1];
	}
}

static int allocUnits(unsigned int count)
{
	SceKernelAllocMemBlockKernelOpt options;
	struct UnitMemory *units;
	unsigned int i;
	int ret;

	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s.unitMemblock = SceSysmemForDriver_C94850C9("SceUsbPspcmUnit", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
						 (count * PCM_UNIT_SIZE + 0xfff) & ~0xfffu, &options);
	if (s.unitMemblock < 0)
		return s.unitMemblock;
	ret = SceSysmemForDriver_A841EDDA(s.unitMemblock, (void **)&units);
	if (ret < 0) {
		SceSysmemForDriver_009E1C61(s.unitMemblock);
		return ret;
	}
	for (i = 0; (int)i < (int)count; i++)
		initChannel(s.channels[i], i, &units[i]);
	initFreeBuffers(count);
	return ret;
}

static int resetNotifications(void)
{
	s.callbackThread = -1;
	s.changes = s.arrivals = s.departures = 0;
	return 0;
}

static int startCommunication(int size, void *args, void *context)
{
	SceKernelAllocMemBlockKernelOpt options;
	struct PcmMemory *memory = NULL;
	SceUInt32 param[22];
	unsigned int i, state;

	memset(&options, 0, sizeof(options));
	options.size = sizeof(options);
	options.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT;
	s.memblock = SceSysmemForDriver_C94850C9("SceUsbPspcm", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW,
					     0x1000, &options);
	if (s.memblock < 0)
		return -1;
	if (SceSysmemForDriver_A841EDDA(s.memblock, (void **)&memory) < 0)
		goto failBlock;
	s.headerBuffer = (PcmHeader *)memory->header;
	s.controlBuffer = memory->control;
	s.driver.descriptor_hi = &memory->high.device;
	s.driver.configuration_hi = &memory->high.configuration;
	initDescriptor(&memory->high, 1);
	s.driver.descriptor = &memory->full.device;
	s.driver.configuration = &memory->full.configuration;
	initDescriptor(&memory->full, 0);
	for (i = 0; i < PCM_UNITS; i++)
		s.channels[i] = &memory->channels[i];
	s.sema = sceKernelCreateSema("SceUsbPspcm", 0x2000, 1, 1, NULL);
	if (s.sema <= 0)
		goto failBlock;
	for (i = 0; i < 8; i++) {
		int event = sceKernelCreateEventFlag("SceUsbPspcmUnit", 0x9000, 0, NULL);
		if (event <= 0) {
			while (i)
				sceKernelDeleteEventFlag(s.events[--i]);
			goto failSema;
		}
		s.events[i] = event;
	}
	s.header.onComplete = headerComplete;
	s.control.onComplete = controlComplete;
	s.header.size = 512;
	for (i = 0; i < 4; i++)
		s.endpoints[i].transmittedBytes = 0;
	s.header.endpoint = &s.endpoints[2];
	s.control.data = s.controlBuffer;
	s.header.data = s.headerBuffer;
	s.header.unused = s.headerBuffer;
	s.control.size = 64;
	s.control.endpoint = &s.endpoints[0];
	s.flags = s.count = s.connected = 0;
	s.marker.data = NULL;
	s.marker.size = 0;
	s.marker.next = NULL;
	s.control.next = NULL;
	s.control.unused = NULL;
	s.control.attributes = 0;
	s.control.transmitted = 0;
	s.header.isControlRequest = 0;
	s.header.next = NULL;
	s.header.attributes = 0;
	s.header.transmitted = 0;
	s.unitMemblock = -1;
	s.hostPort = -1;
	s.control.isControlRequest = 1;
	s.control.returnCode = 1;
	s.header.returnCode = 1;
	s.receive.endpoint = &s.endpoints[2];
	s.receive.data = NULL;
	s.receive.onComplete = receiveComplete;
	s.receive.size = PCM_PACKET_MAX;
	s.receive.isControlRequest = 0;
	s.receive.next = NULL;
	s.receive.unused = NULL;
	s.receive.attributes = 0;
	s.receive.transmitted = 0;
	s.receive.returnCode = 1;
	s.ports = 2;
	if (allocUnits(2) < 0)
		goto failEvents;
	resetNotifications();
	param[1] = 5;
	SceUdcdForDriver_78BF2A28(param);
	state = lock();
	s.flags = (s.flags & ~PCM_STATE_MASK) | PCM_STATE_READY;
	unlock(state);
	return 0;
failEvents:
	for (i = 0; i < 8; i++)
		sceKernelDeleteEventFlag(s.events[i]);
failSema:
	sceKernelCancelSema(s.sema, -1, NULL);
	sceKernelDeleteSema(s.sema);
failBlock:
	SceSysmemForDriver_009E1C61(s.memblock);
	return -1;
}

static int attachCommunication(int speed, void *context)
{
	SceUdcdDeviceInfo info;
	SceUdcdForDriver_FBEA3703(&info);
	if (!(s.flags & PCM_FLAG_ATTACHED)) {
		s.packetSize = speed == 2 ? 512 : 64;
		s.flags |= PCM_FLAG_ATTACHED;
		s.header.size = s.packetSize;
		s.hostVersion = 0;
		s.header.returnCode = 1;
	}
	s.control.returnCode = 1;
	return 1;
}

static int stopCommunication(int size, void *args, void *context)
{
	unsigned state = lock(), i;
	for (i = 0; i < s.count && i < 8; i++) {
		if (s.channels[i]->busy & PCM_BUSY_READ) {
			unlock(state);
			return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
		}
	}
	if ((s.flags & PCM_STATE_MASK) == PCM_STATE_NOTIFYING) {
		unlock(state);
		return SCE_UDCD_ERROR_DRIVER_IN_PROGRESS;
	}
	s.flags &= ~PCM_STATE_MASK;
	unlock(state);
	SceSysmemForDriver_009E1C61(s.unitMemblock);
	sceKernelCancelSema(s.sema, -1, NULL);
	sceKernelDeleteSema(s.sema);
	for (i = 0; i < 8; i++)
		sceKernelDeleteEventFlag(s.events[i]);
	SceSysmemForDriver_009E1C61(s.memblock);
	return 0;
}

static void disconnected(void)
{
	if ((int)s.changes < 0) {
		s.hostPort = -1;
		s.connected = 0;
		s.arrivals = 0;
		if (!s.departures) {
			sceKernelClearEventFlag(s.event, ~PCM_THREAD_EVENT_CHANGED);
			return;
		}
	} else {
		s.changes |= 0x80000000;
		s.oldPort = s.hostPort;
		s.departures = 0x80000000;
	}
	s.arrivals = 0;
	s.connected = 0;
	s.hostPort = -1;
	sceKernelSetEventFlag(s.event, PCM_THREAD_EVENT_CHANGED);
}

static void detachCommunication(void *context)
{
	unsigned i;
	if (!(s.flags & PCM_FLAG_ATTACHED))
		return;
	s.flags &= ~PCM_FLAG_ATTACHED;
	for (i = 0; i < 4; i++)
		s.endpoints[i].transmittedBytes = 0;
	disconnected();
	for (i = 0; i < s.count; i++) {
		struct Channel *channel = s.channels[i];
		unsigned state = lock();
		resetChannelState(channel);
		sceKernelSetEventFlag(channel->event, PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED);
		releaseChannel(channel);
		unlock(state);
	}
}

static void notifyConnection(const SceUdcdEP0DeviceRequest *request)
{
	unsigned value = request->wValue;
	if (value & 0x8000) {
		value &= 0x7fff;
		if (value == 4 || value == 8 || value == 16 || value == 32 || value == 64)
			s.notification = value | 0x80;
	} else {
		s.notification = 0x81;
		s.hostPort = value;
	}
	s.changes |= 0x80000000;
	s.arrivals |= 0x80000000;
	sceKernelSetEventFlag(s.event, PCM_THREAD_EVENT_CHANGED);
}

static void notifyPorts(const SceUdcdEP0DeviceRequest *request)
{
	unsigned changed = request->wValue ^ s.connected, i;
	for (i = 0; i < (unsigned)s.ports && s.ports > 0; i++) {
		unsigned bit = 1u << (i & 255);
		if (bit & changed) {
			if (!(bit & s.changes) && (bit & s.connected))
				s.departures |= bit;
			else
				s.arrivals ^= bit;
		}
	}
	s.connected = request->wValue;
	s.changes = changed;
	if (!s.arrivals && !s.departures)
		sceKernelClearEventFlag(s.event, ~PCM_THREAD_EVENT_CHANGED);
	else
		sceKernelSetEventFlag(s.event, PCM_THREAD_EVENT_CHANGED);
}

static void controlComplete(SceUdcdDeviceRequest *request)
{
	const PcmHostConfig *config = request->data;

	if (request->returnCode != 0)
		return;
	if (s.setup.bRequest == PCM_REQUEST_SET_CONFIG) {
		if (config->ports >= PCM_UNITS || config->units >= PCM_UNITS ||
		    config->packetSize != PCM_PACKET_MAX)
			return;
		s.ports = config->ports;
		if ((unsigned int)config->units != s.count) {
			if (s.unitMemblock >= 0) {
				SceSysmemForDriver_009E1C61(s.unitMemblock);
				s.unitMemblock = -1;
				s.count = 0;
			}
			if (allocUnits(config->units) < 0)
				return;
		}
		notifyConnection(&s.setup);
		receiveHeader();
	} else if (s.setup.bRequest == PCM_REQUEST_SET_VERSION) {
		s.hostVersion = *(const SceUInt32 *)request->data;
	}
}

static int processRequest(int recipient, int endpoint, SceUdcdEP0DeviceRequest *request, void *context)
{
	unsigned i, state;
	s.setup = *request;
	if (endpoint < 0 || (request->bmRequestType != PCM_REQUEST_OUT &&
			     request->bmRequestType != PCM_REQUEST_IN))
		return -1;
	if (request->bmRequestType == PCM_REQUEST_IN) {
		switch (request->bRequest) {
		case PCM_REQUEST_GET_TITLE:
			s.control.size = s.titleLength;
			memcpy(s.control.data, s.title, s.titleLength);
			SceCpuForDriver_103872A5(s.control.data, PCM_ROUND_CACHE(s.control.size));
			SceUdcdForDriver_1ED0E89E(&s.control);
			return 0;
		case PCM_REQUEST_ACCEPT:
			s.controlBuffer[0] = 0;
			i = request->wValue & 255;
			if (i < s.count && i < PCM_UNITS && s.channels[i]->state == PCM_CHANNEL_CONNECTING) {
				s.channels[i]->state = PCM_CHANNEL_CONNECTED;
				sceKernelSetEventFlag(s.channels[i]->event, PCM_EVENT_ACCEPTED);
				s.controlBuffer[0] = 1;
			}
			s.control.size = 1;
			break;
		case PCM_REQUEST_GET_FEATURES:
			if (s.hostVersion < 0x90000)
				return -1;
			*(SceUInt32 *)s.controlBuffer = 0x03000000;
			s.control.size = sizeof(SceUInt32);
			break;
		default:
			return -1;
		}
		SceCpuForDriver_103872A5(s.control.data, PCM_CACHE_LINE);
		SceUdcdForDriver_1ED0E89E(&s.control);
		return 0;
	}
	switch (request->bRequest) {
	case PCM_REQUEST_SET_CONFIG:
		if (request->wLength) {
			s.control.size = request->wLength < 64 ? request->wLength : 64;
			SceCpuForDriver_02796361(s.control.data, 64);
			SceUdcdForDriver_C60A74B2(&s.control);
			return 0;
		}
		if (s.hostPort >= 0) {
			for (i = 0; i < s.count; i++) {
				struct Channel *channel = s.channels[i];
				resetChannelState(channel);
				sceKernelSetEventFlag(channel->event, PCM_EVENT_CLOSED);
				state = lock();
				releaseChannel(channel);
				unlock(state);
			}
			SceUdcdForDriver_AA5FD90E(&s.header);
			SceUdcdForDriver_9F53D64D(&s.endpoints[2]);
		}
		notifyConnection(request);
		return 0;
	case PCM_REQUEST_SET_PORTS:
		if (s.hostPort >= 0)
			notifyPorts(request);
		return 0;
	case PCM_REQUEST_CLOSE:
	case PCM_REQUEST_CANCEL:
		i = request->wValue;
		if (i < s.count && i < PCM_UNITS) {
			struct Channel *channel = s.channels[i];

			resetChannelState(channel);
			if (request->bRequest == PCM_REQUEST_CLOSE) {
				channel->write = NULL;
				channel->remaining = 0;
			}
			sceKernelSetEventFlag(channel->event, request->bRequest == PCM_REQUEST_CLOSE ?
					       PCM_EVENT_CLOSED : PCM_EVENT_CLOSED | PCM_EVENT_CANCELLED);
			state = lock();
			releaseChannel(channel);
			unlock(state);
		}
		return 0;
	case PCM_REQUEST_SET_VERSION:
		s.control.size = request->wLength < 64 ? request->wLength : 64;
		SceCpuForDriver_02796361(s.control.data, 64);
		SceUdcdForDriver_C60A74B2(&s.control);
		return 0;
	default:
		return -1;
	}
}

static void processHeader(unsigned int unit, unsigned int flags)
{
	struct Channel *channel;
	struct Buffer *buffer;
	unsigned length = s.headerBuffer->length, state;
	if (unit >= s.count || unit > 7)
		return;
	channel = s.channels[unit];
	if (flags & PCM_HEADER_DATA) {
		if (length > PCM_PACKET_MAX)
			return;
		if (!length) {
			if (!channel->read) {
				if (channel->state == PCM_CHANNEL_CONNECTED && channel->reset != PCM_RESET_SENT)
					appendBuffer(&channel->queue, &s.marker);
			} else {
				channel->read = NULL;
				sceKernelSetEventFlag(channel->event, PCM_EVENT_READ);
			}
			receiveHeader();
			return;
		}
		buffer = takeBuffer(&s.free);
		if (!buffer) {
			s.flags |= PCM_FLAG_BUFFER_WAIT;
			return;
		}
		s.receive.data = buffer->data;
		s.receive.onComplete = receiveComplete;
		s.receive.unused = buffer;
		s.receive.returnCode = 1;
		s.receive.size = length;
		SceCpuForDriver_02796361(buffer->data, PCM_ROUND_CACHE(length));
		SceUdcdForDriver_C60A74B2(&s.receive);
		return;
	}
	if (flags & PCM_HEADER_RESET) {
		state = lock();
		releaseChannel(channel);
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s.lock, state);
		sceKernelSetEventFlag(channel->event, PCM_EVENT_RESET | PCM_EVENT_RESET_RECEIVED);
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_RESET);
		if (channel->reset != PCM_RESET_SENT)
			SceUdcdForDriver_1ED0E89E(&channel->resetRequest.request);
		channel->reset = PCM_RESET_RECEIVED;
		channel->remaining = 0;
		channel->write = NULL;
	}
	receiveHeader();
}

static void headerComplete(SceUdcdDeviceRequest *request)
{
	PcmHeader *header = request->unused;

	if (request->returnCode == 0) {
		if (request->transmitted == sizeof(PcmHeader))
			processHeader(header->unit, header->flags);
		else
			receiveHeader();
	}
}

static void receiveComplete(SceUdcdDeviceRequest *request)
{
	unsigned state = lock(), unit = s.headerBuffer->unit;
	struct Buffer *buffer = request->unused;
	struct Channel *channel;
	if (unit >= s.count || unit > 7) {
		unlock(state);
		return;
	}
	channel = s.channels[unit];
	if (request->returnCode) {
		releaseBuffer(buffer);
		unlock(state);
		return;
	}
	if ((unsigned)request->transmitted == s.headerBuffer->length) {
		buffer->size = request->transmitted;
		if (channel->reset == PCM_RESET_SENT) {
			releaseBuffer(buffer);
			sceKernelSetEventFlag(channel->event, PCM_EVENT_DATA);
			unlock(state);
			receiveHeader();
			return;
		}
		if (!channel->read) {
			if (channel->state == PCM_CHANNEL_CONNECTED) {
				appendBuffer(&channel->queue, buffer);
				if ((s.headerBuffer->flags & PCM_HEADER_LAST) && request->transmitted == PCM_PACKET_MAX)
					appendBuffer(&channel->queue, &s.marker);
			} else {
				releaseBuffer(buffer);
			}
			sceKernelSetEventFlag(channel->event, PCM_EVENT_DATA);
			unlock(state);
			receiveHeader();
		} else {
			sceKernelSetEventFlag(channel->event, PCM_EVENT_READ);
			unlock(state);
		}
		return;
	}
	if (request->transmitted == 8) {
		*s.headerBuffer = *(PcmHeader *)request->data;
		unlock(state);
		processHeader(s.headerBuffer->unit, s.headerBuffer->flags);
	} else {
		unlock(state);
		receiveHeader();
	}
	state = lock();
	releaseBuffer(buffer);
	unlock(state);
}

static int dispatchNotification(const unsigned *args)
{
	unsigned state = lock();
	int ret;
	if (!(s.flags & PCM_STATE_MASK)) {
		unlock(state);
		return -1;
	}
	s.flags = (s.flags & ~PCM_STATE_MASK) | PCM_STATE_NOTIFYING;
	unlock(state);
	ret = -1;
	if (s.callbackThread >= 0 && sceKernelStartThread(s.callbackThread, 8, (void *)args) >= 0 &&
	    sceKernelWaitThreadEnd(s.callbackThread, NULL, NULL) >= 0)
		ret = 0;
	state = lock();
	s.flags = (s.flags & ~PCM_STATE_MASK) | PCM_STATE_READY;
	unlock(state);
	return ret;
}

static int connectionThread(SceSize size, void *args)
{
	unsigned state = lock(), active = s.connected | s.arrivals;
	unsigned arrivals, departures = 0, event[2], i, notification;
	int port = s.hostPort, oldPort;
	if (port >= 0)
		active |= 0x80000000;
	notification = s.notification;
	oldPort = s.oldPort;
	s.changes = s.arrivals = s.departures = 0;
	unlock(state);
	for (;;) {
		if ((int)departures < 0) {
			event[0] = oldPort;
			event[1] = 0x82;
			dispatchNotification(event);
		}
		if ((int)active < 0) {
			event[0] = port;
			event[1] = notification;
			dispatchNotification(event);
		}
		for (i = 0; (int)i < s.ports; i++) {
			if ((departures & (1u << (i & 255))) && oldPort != (int)i) {
				event[0] = i;
				event[1] = 2;
				dispatchNotification(event);
			}
			if ((active & (1u << (i & 255))) && port != (int)i) {
				event[0] = i;
				event[1] = 1;
				dispatchNotification(event);
			}
		}
		if (sceKernelWaitEventFlag(s.event, PCM_THREAD_EVENT_CHANGED,
					    SCE_KERNEL_EVF_WAITMODE_OR |
					    SCE_KERNEL_EVF_WAITMODE_CLEAR_ALL, NULL, NULL) < 0)
			continue;
		state = lock();
		arrivals = s.arrivals;
		departures = s.departures;
		active = arrivals;
		notification = s.notification;
		s.arrivals = s.departures = s.changes = 0;
		port = s.hostPort;
		oldPort = s.oldPort;
		unlock(state);
	}
	return 0;
}

static int initNotifications(void)
{
	s.event = sceKernelCreateEventFlag("SceUsbPspcmConnect", 0x8000, 0, NULL);
	if (s.event <= 0)
		return -1;
	s.notificationSema = sceKernelCreateSema("SceUsbPspcmConnect", 0x2000, 1, 1, NULL);
	if (s.notificationSema > 0) {
		s.thread = sceKernelCreateThread("SceUsbPspcm", connectionThread, 0x60, 0x1000, 0, 0x80000, NULL);
		if (s.thread >= 0) {
			resetNotifications();
			sceKernelStartThread(s.thread, 0, NULL);
			return 0;
		}
		sceKernelDeleteSema(s.notificationSema);
	}
	sceKernelDeleteEventFlag(s.event);
	s.event = -1;
	return -1;
}

int module_start(SceSize size, void *args)
{
	memcpy(s.strings.bString, s_productName, sizeof(s_productName));
	s.driver.driverName = "USBPSPCommunicationDriver";
	s.driver.numEndpoints = 4;
	s.driver.attach = attachCommunication;
	s.driver.detach = detachCommunication;
	s.driver.start = startCommunication;
	s.driver.processRequest = processRequest;
	s.driver.stop = stopCommunication;
	s.endpoints[3].driverEndpointNumber = 3;
	s.strings.bDescriptorType = 3;
	s.driver.stringDescriptors = &s.strings;
	s.driver.endpoints = s.endpoints;
	s.endpoints[2].driverEndpointNumber = 2;
	s.endpoints[0].driverEndpointNumber = 0;
	s.endpoints[2].direction = 0;
	s.driver.changeSetting = NULL;
	s.driver.configure = NULL;
	s.endpoints[1].direction = 0x80;
	s.endpoints[3].direction = 0x80;
	s.endpoints[1].driverEndpointNumber = 1;
	s.interface.numInterfaces = 1;
	s.strings.bLength = 0;
	s.flags = 0;
	s.driver.interface = &s.interface;
	s.interface.expectNumber = -1;
	s.lock = 0;
	if (initNotifications() >= 0 && SceUdcdForDriver_4E55244D(&s.driver) >= 0)
		return 0;
	return 2;
}

int module_stop(SceSize size, void *args)
{
	SceUdcdForDriver_0DECE532(&s.driver);
	sceKernelDeleteThread(s.thread);
	sceKernelDeleteSema(s.notificationSema);
	sceKernelDeleteEventFlag(s.event);
	s.event = -1;
	return 0;
}

static int connectChannel(const unsigned *args)
{
	struct Channel *channel = NULL;
	unsigned i, bits, timeout = PCM_WAIT_TIMEOUT, port = args[1];
	unsigned *wait = NULL;
	int ret;
	if (s.hostPort < 0)
		return PCM_ERROR_DISCONNECTED;
	if ((unsigned)s.hostPort == port)
		return PCM_ERROR_ARGUMENT;
	if (port != 255 && ((int)port < 0 || s.ports < (int)port || !(s.connected & (1u << (port & 255)))))
		return PCM_ERROR_NOT_FOUND;
	ret = sceKernelWaitSema(s.sema, 1, &timeout);
	if (ret < 0)
		return ret;
	for (i = 0; i < s.count; i++) {
		struct Channel *candidate = s.channels[i];
		if (candidate->state && candidate->state != PCM_CHANNEL_CONNECTED && (unsigned)candidate->port == port) {
			sceKernelSignalSema(s.sema, 1);
			return PCM_ERROR_BUSY;
		}
		if (!candidate->state && candidate->opened <= 0 && !channel)
			channel = candidate;
	}
	if (!channel) {
		sceKernelSignalSema(s.sema, 1);
		return PCM_ERROR_NO_UNIT;
	}
	channel->state = PCM_CHANNEL_CONNECTING;
	channel->port = port;
	sceKernelClearEventFlag(channel->event, 0);
	sceKernelSignalSema(s.sema, 1);
	channel->commandBuffer->command = PCM_COMMAND_CONNECT;
	channel->commandBuffer->argument = ((port & 255) << 8) | (channel->unit & 255);
	SceCpuForDriver_364E68A4(channel->commandBuffer, PCM_CACHE_LINE);
	SceUdcdForDriver_1ED0E89E(&channel->command);
	if (args[2]) {
		timeout = *(unsigned *)(uintptr_t)args[2];
		wait = &timeout;
	}
	ret = sceKernelWaitEventFlag(channel->event,
				      PCM_EVENT_COMMAND_DONE | PCM_EVENT_DISCONNECTED |
				      PCM_EVENT_CANCELLED, SCE_KERNEL_EVF_WAITMODE_OR, &bits, wait);
	if (ret >= 0) {
		if (bits & (PCM_EVENT_DISCONNECTED | PCM_EVENT_CANCELLED)) {
			channel->port = -1;
			channel->state = PCM_CHANNEL_IDLE;
			return PCM_ERROR_DISCONNECTED;
		}
		ret = sceKernelWaitEventFlag(channel->event,
					      PCM_EVENT_DISCONNECTED | PCM_EVENT_ACCEPTED |
					      PCM_EVENT_CANCELLED, SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, wait);
		if (ret >= 0) {
			if (bits & (PCM_EVENT_DISCONNECTED | PCM_EVENT_CANCELLED)) {
				channel->port = -1;
				channel->state = PCM_CHANNEL_IDLE;
				if (!(bits & PCM_EVENT_CANCELLED))
					return PCM_ERROR_DISCONNECTED;
			} else if (channel->state == PCM_CHANNEL_CONNECTED) {
				return channel->unit;
			}
			return PCM_ERROR_CANCELLED;
		}
	}
	if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
		ret = disconnectChannel(channel, PCM_COMMAND_CANCEL);
	return ret;
}

static int cancelConnect(unsigned port)
{
	unsigned i, timeout = PCM_WAIT_TIMEOUT;
	int ret;
	if (s.hostPort < 0)
		return PCM_ERROR_DISCONNECTED;
	if ((unsigned)s.hostPort == port)
		return PCM_ERROR_ARGUMENT;
	if (port != 255 && ((int)port < 0 || s.ports < (int)port || !(s.connected & (1u << (port & 255)))))
		return PCM_ERROR_NOT_FOUND;
	ret = sceKernelWaitSema(s.sema, 1, &timeout);
	if (ret < 0)
		return ret;
	for (i = 0; i < s.count; i++) {
		struct Channel *channel = s.channels[i];
		if (channel->state == PCM_CHANNEL_CONNECTING && (unsigned)channel->port == port) {
			disconnectChannel(channel, PCM_COMMAND_CANCEL);
			sceKernelSetEventFlag(channel->event, PCM_EVENT_CANCELLED);
			sceKernelSignalSema(s.sema, 1);
			return 0;
		}
	}
	sceKernelSignalSema(s.sema, 1);
	return PCM_ERROR_NOT_FOUND;
}

static int setCallback(unsigned command, int thread)
{
	int ret = sceKernelPollSema(s.notificationSema, 1);
	if (ret < 0)
		return ret == (int)SCE_KERNEL_ERROR_SEMA_ZERO ? PCM_ERROR_BUSY : ret;
	if (command == 0x3415001) {
		if (s.callbackThread > 0)
			ret = PCM_ERROR_ALREADY;
		else
			s.callbackThread = thread;
	} else if (s.callbackThread < 0) {
		ret = PCM_ERROR_NOT_FOUND;
	} else if (thread == s.callbackThread) {
		sceKernelDeleteThread(thread);
		s.callbackThread = -1;
	} else {
		ret = PCM_ERROR_ARGUMENT;
	}
	sceKernelSignalSema(s.notificationSema, 1);
	return ret;
}

static int devctl(unsigned command, const unsigned *input, unsigned inputSize, unsigned *output, unsigned outputSize)
{
	int ret, pid;
	switch (command) {
	case 0x3407003:
		return s.hostPort < 0 ? PCM_ERROR_DISCONNECTED : s.hostPort;
	case 0x3415001:
	case 0x3415002:
		if (!input || inputSize < 4)
			break;
		pid = sceKernelGetProcessId();
		if (pid < 0)
			break;
		ret = SceSysmemForDriver_45D22597(pid, *input);
		if (ret >= 0)
			return setCallback(command, ret);
		break;
	case 0x3427004:
		if (!output || outputSize < 4)
			break;
		if (s.hostPort < 0)
			return PCM_ERROR_DISCONNECTED;
		*output = s.connected;
		return 0;
	case 0x3435005:
		if (!input || inputSize < 12 || !output || outputSize != 4 || input[0] != 12)
			break;
		ret = connectChannel(input);
		if (ret < 0)
			return ret;
		*output = ret;
		return 0;
	case 0x3415006:
		if (input && inputSize >= 4)
			return cancelConnect(*input);
		break;
	}
	return PCM_ERROR_ARGUMENT;
}

static int openChannel(unsigned unit)
{
	struct Channel *channel;
	unsigned state;
	if (s.hostPort < 0)
		return PCM_ERROR_DISCONNECTED;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	state = lock();
	if (channel->state != PCM_CHANNEL_CONNECTED) {
		unlock(state);
		return PCM_ERROR_DISCONNECTED;
	}
	if (channel->opened > 0) {
		unlock(state);
		return PCM_ERROR_ALREADY;
	}
	channel->opened = 1;
	unlock(state);
	return unit;
}

static int closeChannel(unsigned unit)
{
	struct Channel *channel;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	sceKernelSetEventFlag(channel->event, PCM_EVENT_CLOSED);
	disconnectChannel(channel, PCM_COMMAND_CLOSE);
	channel->opened = 0;
	channel->mode = 0;
	return 0;
}

static int resetError(struct Channel *channel)
{
	if (channel->reset == PCM_RESET_SENT)
		return PCM_ERROR_RESET;
	channel->reset = PCM_RESET_NONE;
	return PCM_ERROR_RESET_RECEIVED;
}

static int waitData(struct Channel *channel, const unsigned *timeout)
{
	unsigned state = lock(), bits, time, *wait = NULL;
	int ret;
	if (channel->partial || channel->queue.head) {
		unlock(state);
		return 0;
	}
	if (channel->busy & PCM_BUSY_WAIT) {
		unlock(state);
		return PCM_ERROR_BUSY;
	}
	channel->busy |= PCM_BUSY_WAIT;
	sceKernelClearEventFlag(channel->event, ~(PCM_EVENT_DATA | PCM_EVENT_WAIT_CANCEL));
	unlock(state);
	if (timeout) {
		time = *timeout;
		wait = &time;
	}
	ret = sceKernelWaitEventFlag(channel->event,
				      PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED | PCM_EVENT_RESET |
				      PCM_EVENT_DATA | PCM_EVENT_WAIT_CANCEL,
				      SCE_KERNEL_EVF_WAITMODE_OR, &bits, wait);
	if (ret < 0) {
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			ret = PCM_ERROR_TIMEOUT;
	} else if (bits & PCM_EVENT_DISCONNECTED) {
		ret = PCM_ERROR_DISCONNECTED;
	} else if (bits & PCM_EVENT_CLOSED) {
		ret = PCM_ERROR_STATE;
	} else if (bits & PCM_EVENT_RESET) {
		ret = PCM_ERROR_RESET;
	} else {
		ret = bits & PCM_EVENT_WAIT_CANCEL ? PCM_ERROR_CANCELLED : 0;
	}
	state = lock();
	channel->busy &= ~PCM_BUSY_WAIT;
	unlock(state);
	return ret;
}

static int ioctlReset(unsigned unit)
{
	struct Channel *channel;
	unsigned state, bits;
	int ret;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	if (channel->state != PCM_CHANNEL_CONNECTED)
		return PCM_ERROR_STATE;
	state = lock();
	unlock(state);
	state = lock();
	if (channel->busy & PCM_BUSY_RESET) {
		unlock(state);
		return PCM_ERROR_BUSY;
	}
	channel->busy |= PCM_BUSY_RESET;
	if (!channel->reset) {
		channel->write = NULL;
		channel->remaining = 0;
		channel->reset = PCM_RESET_SENT;
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_RESET_RECEIVED);
		sceKernelSetEventFlag(channel->event, PCM_EVENT_RESET);
		unlock(state);
		channel->resetRequest.request.returnCode = 1;
		SceUdcdForDriver_1ED0E89E(&channel->resetRequest.request);
	} else {
		unlock(state);
	}
	ret = sceKernelWaitEventFlag(channel->event,
				      PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED |
				      PCM_EVENT_RESET_RECEIVED, SCE_KERNEL_EVF_WAITMODE_OR, &bits,
				      NULL);
	if (ret >= 0) {
		if (bits & PCM_EVENT_DISCONNECTED)
			ret = PCM_ERROR_DISCONNECTED;
		else if (bits & PCM_EVENT_CLOSED)
			ret = PCM_ERROR_STATE;
		else {
			state = lock();
			if (channel->reset == PCM_RESET_RECEIVED)
				channel->reset = PCM_RESET_NONE;
			ret = 0;
			goto done;
		}
	}
	state = lock();
done:
	channel->busy &= ~PCM_BUSY_RESET;
	unlock(state);
	return ret;
}

static int ioctlPoll(unsigned unit)
{
	struct Channel *channel;
	unsigned state;
	int ret;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	if (channel->state != PCM_CHANNEL_CONNECTED)
		return PCM_ERROR_STATE;
	state = lock();
	if (channel->reset) {
		ret = resetError(channel);
		unlock(state);
		return ret;
	}
	unlock(state);
	state = lock();
	ret = channel->partial ? 0 : !channel->queue.head;
	unlock(state);
	return ret;
}

static int ioctlWait(unsigned unit, const unsigned *timeout)
{
	struct Channel *channel;
	unsigned state;
	int ret;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	if (channel->state != PCM_CHANNEL_CONNECTED)
		return PCM_ERROR_STATE;
	state = lock();
	if (channel->reset) {
		ret = resetError(channel);
		unlock(state);
		return ret;
	}
	unlock(state);
	return waitData(channel, timeout);
}

static int ioctlCancel(unsigned unit)
{
	struct Channel *channel;
	unsigned state;
	int ret;
	if (unit >= s.count || unit > 7)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	if (channel->state != PCM_CHANNEL_CONNECTED)
		return PCM_ERROR_STATE;
	state = lock();
	if (channel->reset) {
		ret = resetError(channel);
		unlock(state);
		return ret;
	}
	unlock(state);
	sceKernelSetEventFlag(channel->event, PCM_EVENT_WAIT_CANCEL);
	return 0;
}

static int writeChannel(unsigned unit, const uint8_t *data, unsigned size, unsigned mode, unsigned user)
{
	struct Channel *channel;
	PcmHeader *header;
	unsigned int state, bits, length;
	int ret;
	if (unit >= s.count || unit > 7 || (!data && size) || ((uintptr_t)data & 3))
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	state = lock();
	channel->mode = mode;
	channel->user = user;
	if (channel->busy & PCM_BUSY_WRITE) {
		unlock(state);
		return PCM_ERROR_BUSY;
	}
	if (channel->reset) {
		ret = resetError(channel);
		unlock(state);
		return ret;
	}
	channel->busy |= PCM_BUSY_WRITE;
	unlock(state);
	if (channel->state != PCM_CHANNEL_CONNECTED) {
		ret = PCM_ERROR_STATE;
		goto done;
	}
	sceKernelClearEventFlag(channel->event, ~PCM_EVENT_SENT);
	sceKernelClearEventFlag(channel->event, ~PCM_EVENT_SEND_FAILED);
	header = channel->headerRequest.header;
	channel->write = (void *)data;
	channel->remaining = size;
	header->flags = PCM_HEADER_DATA;
	length = size;
	if (size > PCM_PACKET_MAX || (size == PCM_PACKET_MAX && (channel->mode & PCM_MODE_CONTINUED)))
		length = PCM_PACKET_MAX;
	else
		header->flags = PCM_HEADER_DATA | PCM_HEADER_LAST;
	header->length = length;
	channel->headerRequest.request.next = NULL;
	if (length) {
		if (!channel->user)
			memcpy(channel->sendBuffer, data, length);
		else {
			ret = SceSysmemForDriver_BC996A7A(channel->sendBuffer, data, length);
			if (ret < 0)
				goto done;
		}
		channel->dataRequest.data = channel->sendBuffer;
		channel->dataRequest.size = header->length;
	}
	if (channel->dataRequest.data)
		SceCpuForDriver_364E68A4(channel->sendBuffer, PCM_ROUND_CACHE(channel->dataRequest.size));
	channel->dataRequest.returnCode = 1;
	SceCpuForDriver_364E68A4(header, PCM_CACHE_LINE);
	channel->headerRequest.request.returnCode = 1;
	SceUdcdForDriver_1ED0E89E(&channel->headerRequest.request);
	for (;;) {
		ret = sceKernelWaitEventFlag(channel->event,
					      PCM_EVENT_SENT | PCM_EVENT_DISCONNECTED |
					      PCM_EVENT_CLOSED | PCM_EVENT_RESET |
					      PCM_EVENT_SEND_FAILED, SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, NULL);
		if (ret < 0)
			goto clear;
		if (bits & (PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED | PCM_EVENT_RESET | PCM_EVENT_SEND_FAILED))
			goto failedEvent;
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_SENT);
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_SEND_FAILED);
		SceUdcdForDriver_1ED0E89E(&channel->dataRequest);
		ret = sceKernelWaitEventFlag(channel->event,
					      PCM_EVENT_SENT | PCM_EVENT_DISCONNECTED |
					      PCM_EVENT_CLOSED | PCM_EVENT_RESET |
					      PCM_EVENT_SEND_FAILED, SCE_KERNEL_EVF_WAITMODE_OR,
					      &bits, NULL);
		if (ret < 0)
			goto clear;
		if (bits & (PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED | PCM_EVENT_RESET | PCM_EVENT_SEND_FAILED))
			goto failedEvent;
		if (channel->write) {
			channel->write += channel->dataRequest.transmitted;
			channel->remaining -= channel->dataRequest.transmitted;
		}
		length = channel->remaining;
		if (!length) {
			ret = size;
			goto clear;
		}
		header = channel->headerRequest.header;
		header->flags = PCM_HEADER_DATA;
		if (length <= PCM_PACKET_MAX) {
			if (length != PCM_PACKET_MAX || !(channel->mode & PCM_MODE_CONTINUED))
				header->flags = PCM_HEADER_DATA | PCM_HEADER_LAST;
		} else {
			length = PCM_PACKET_MAX;
		}
		header->length = length;
		SceCpuForDriver_364E68A4(header, PCM_CACHE_LINE);
		if (!channel->user)
			memcpy(channel->sendBuffer, channel->write, length);
		else {
			ret = SceSysmemForDriver_BC996A7A(channel->sendBuffer, channel->write, length);
			if (ret < 0)
				goto done;
		}
		channel->dataRequest.data = channel->sendBuffer;
		channel->dataRequest.size = header->length;
		SceCpuForDriver_364E68A4(channel->sendBuffer, PCM_ROUND_CACHE(channel->dataRequest.size));
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_SENT);
		channel->dataRequest.returnCode = 1;
		channel->headerRequest.request.returnCode = 1;
		SceUdcdForDriver_1ED0E89E(&channel->headerRequest.request);
	}
failedEvent:
	if (bits & PCM_EVENT_DISCONNECTED)
		ret = PCM_ERROR_DISCONNECTED;
	else if (bits & PCM_EVENT_CLOSED)
		ret = PCM_ERROR_STATE;
	else if (bits & PCM_EVENT_RESET)
		ret = PCM_ERROR_RESET;
	else if (bits & PCM_EVENT_SEND_FAILED)
		ret = PCM_ERROR_STATE;
clear:
	channel->write = NULL;
	channel->remaining = 0;
done:
	state = lock();
	channel->busy &= ~PCM_BUSY_WRITE;
	unlock(state);
	return ret;
}

static int copyRead(struct Channel *channel, void *target, const void *data, unsigned size)
{
	if (channel->user)
		return SceSysmemForDriver_6D88EF8A(target, data, size);
	memcpy(target, data, size);
	return 0;
}

static int readChannel(unsigned unit, uint8_t *data, unsigned size, unsigned mode, unsigned user)
{
	struct Channel *channel;
	struct Buffer *buffer = NULL;
	unsigned state, copied = 0, length, bits, total, actual;
	int ret;
	if (unit >= s.count || unit > 7 || !data)
		return PCM_ERROR_ARGUMENT;
	channel = s.channels[unit];
	state = lock();
	if (channel->busy & PCM_BUSY_READ) {
		unlock(state);
		return PCM_ERROR_BUSY;
	}
	channel = s.channels[unit];
	channel->mode = mode;
	channel->user = user;
	if (channel->reset) {
		ret = resetError(channel);
		unlock(state);
		return ret;
	}
	channel->busy |= PCM_BUSY_READ;
	unlock(state);
	if (channel->state != PCM_CHANNEL_CONNECTED) {
		ret = PCM_ERROR_STATE;
		goto done;
	}
	sceKernelClearEventFlag(channel->event, ~PCM_EVENT_READ);
	sceKernelClearEventFlag(channel->event, ~PCM_EVENT_RESERVED);
	state = lock();
	buffer = channel->partial;
	if (buffer) {
		length = buffer->size - channel->offset;
		if (length < size) {
			ret = copyRead(channel, data, buffer->data + channel->offset, length);
			if (ret < 0) {
				unlock(state);
				goto done;
			}
			actual = channel->partial->size;
			releaseBuffer(channel->partial);
			channel->partial = NULL;
			if (actual < PCM_PACKET_MAX) {
				unlock(state);
				ret = length;
				goto done;
			}
			data += length;
			size -= length;
			copied = length;
		} else {
			ret = copyRead(channel, data, buffer->data + channel->offset, size);
			if (ret < 0) {
				unlock(state);
				goto done;
			}
			channel->offset += size;
			if (size == length) {
				if (channel->mode & PCM_MODE_MESSAGE) {
					actual = channel->partial->size;
					releaseBuffer(channel->partial);
					channel->partial = NULL;
					if (actual == PCM_PACKET_MAX && channel->queue.head && !channel->queue.head->size)
						releaseBuffer(takeBuffer(&channel->queue));
				} else if (channel->partial->size == PCM_PACKET_MAX || !size) {
					releaseBuffer(channel->partial);
					channel->partial = NULL;
				}
			}
			unlock(state);
			ret = size;
			goto done;
		}
	}
	unlock(state);
	buffer = NULL;
	for (;;) {
		state = lock();
		if (buffer)
			releaseBuffer(buffer);
		buffer = takeBuffer(&channel->queue);
		if (!buffer)
			break;
		unlock(state);
		if (size <= buffer->size) {
			ret = copyRead(channel, data, buffer->data, size);
			if (ret < 0)
				goto done;
			channel->partial = buffer;
			channel->offset = size;
			total = copied + size;
			if (size == buffer->size) {
				if (channel->mode & PCM_MODE_MESSAGE) {
					state = lock();
					releaseBuffer(buffer);
					unlock(state);
					channel->partial = NULL;
					if (size == PCM_PACKET_MAX && channel->queue.head && !channel->queue.head->size) {
						state = lock();
						releaseBuffer(takeBuffer(&channel->queue));
						unlock(state);
					}
				} else if (!size || size == PCM_PACKET_MAX) {
					state = lock();
					releaseBuffer(buffer);
					unlock(state);
					channel->partial = NULL;
				}
			}
			ret = total;
			goto done;
		}
		ret = copyRead(channel, data, buffer->data, buffer->size);
		if (ret < 0)
			goto done;
		length = buffer->size;
		data += length;
		copied += length;
		size -= length;
		if (length < PCM_PACKET_MAX) {
			state = lock();
			releaseBuffer(buffer);
			unlock(state);
			ret = copied;
			goto done;
		}
	}
	channel->readRemaining = size;
	channel->read = data;
	unlock(state);
	for (;;) {
		ret = sceKernelWaitEventFlag(channel->event,
					      PCM_EVENT_READ | PCM_EVENT_DISCONNECTED |
					      PCM_EVENT_CLOSED | PCM_EVENT_RESET,
					      SCE_KERNEL_EVF_WAITMODE_OR, &bits, NULL);
		buffer = s.receive.unused;
		if (ret < 0)
			goto clearBusy;
		if (bits & (PCM_EVENT_DISCONNECTED | PCM_EVENT_CLOSED | PCM_EVENT_RESET)) {
			if (bits & PCM_EVENT_DISCONNECTED)
				ret = PCM_ERROR_DISCONNECTED;
			else if (bits & PCM_EVENT_CLOSED)
				ret = PCM_ERROR_STATE;
			else if (bits & PCM_EVENT_RESET)
				ret = PCM_ERROR_RESET;
			goto clearBusy;
		}
		if (s.receive.returnCode < 0) {
			ret = PCM_ERROR_CANCELLED;
			goto done;
		}
		if (!channel->read)
			break;
		length = buffer->size;
		if (channel->readRemaining <= length) {
			if (channel->user)
				SceCpuForDriver_02796361(buffer->data, PCM_ROUND_CACHE(length));
			copyRead(channel, channel->read, buffer->data, channel->readRemaining);
			channel->partial = buffer;
			channel->offset = channel->readRemaining;
			if (channel->mode & PCM_MODE_MESSAGE) {
				if (channel->offset == buffer->size) {
					releaseBuffer(buffer);
					channel->partial = NULL;
				} else if ((s.headerBuffer->flags & PCM_HEADER_LAST) && buffer->size == PCM_PACKET_MAX) {
					appendBuffer(&channel->queue, &s.marker);
				}
			} else {
				if ((s.headerBuffer->flags & PCM_HEADER_LAST) && buffer->size == PCM_PACKET_MAX)
					appendBuffer(&channel->queue, &s.marker);
				if (buffer->size == channel->readRemaining && channel->readRemaining == PCM_PACKET_MAX) {
					releaseBuffer(buffer);
					channel->partial = NULL;
				}
			}
			channel->read = NULL;
			channel->readRemaining = 0;
			sceKernelSetEventFlag(channel->event, PCM_EVENT_DATA);
			receiveHeader();
			break;
		}
		SceCpuForDriver_02796361(buffer->data, PCM_ROUND_CACHE(length));
		copyRead(channel, channel->read, buffer->data, length);
		channel->read += buffer->size;
		channel->readRemaining -= buffer->size;
		if (s.headerBuffer->flags & PCM_HEADER_LAST) {
			channel->read = NULL;
			releaseBuffer(buffer);
			sceKernelSetEventFlag(channel->event, PCM_EVENT_DATA);
			receiveHeader();
			break;
		}
		releaseBuffer(buffer);
		sceKernelSetEventFlag(channel->event, PCM_EVENT_DATA);
		receiveHeader();
		sceKernelClearEventFlag(channel->event, ~PCM_EVENT_READ);
	}
	ret = copied + size - channel->readRemaining;
	channel->readRemaining = 0;
	goto done;
clearBusy:
	channel->read = NULL;
	channel->readRemaining = 0;
	state = lock();
	channel->busy &= ~PCM_BUSY_READ;
	unlock(state);
done:
	state = lock();
	channel->busy &= ~PCM_BUSY_READ;
	unlock(state);
	return ret;
}

static int allowed(void)
{
	return SceSblACMgrForDriver_FD00C72A(0) || sceSblACMgrIsSystemProgram(0);
}

static int checkReady(void)
{
	unsigned state = lock();
	int ready = (s.flags & PCM_STATE_MASK) != 0;
	unlock(state);
	return ready;
}

int sceUsbPspcmSetTitle(const char *title, unsigned size)
{
	unsigned syscall = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (!allowed())
		goto done;
	ret = PCM_ERROR_ARGUMENT;
	if (size <= 64) {
		s.titleLength = 0;
		if (!title) {
			ret = (int)0x8024300a;
			goto done;
		}
		ret = SceSysmemForDriver_DB3EC244(s.title, title, size);
		if (ret >= 0) {
			if (ret != 64) {
				s.titleLength = ret + 1;
				s.title[ret] = 0;
				ret = 0;
			} else {
				ret = PCM_ERROR_ARGUMENT;
			}
		}
	}
done:
	sceSyscallLeave(syscall);
	return ret;
}

int sceUsbPspcmStartDevice(void)
{
	unsigned syscall = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (!allowed())
		goto done;
	ret = PCM_ERROR_ARGUMENT;
	if (!s.titleLength)
		goto done;
	ret = SceUdcdForDriver_C0CA5DDB("USBPSPCommunicationDriver");
	if (ret != 2) {
		ret = SceUdcdForDriver_8AE87657();
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
		ret = SceUdcdForDriver_1494293B("USB_MTP_Driver", 0, NULL);
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
		ret = SceUdcdForDriver_1494293B("USBDeviceControllerDriver", 0, NULL);
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
	}
	ret = sceUdcdStart("USBDeviceControllerDriver", 0, NULL);
	if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
		goto done;
	ret = sceUdcdStart("USBPSPCommunicationDriver", 0, NULL);
	if (ret >= 0 || ret == SCE_UDCD_ERROR_INVALID_ARGUMENT) {
		ret = sceUdcdActivate(0x1cb);
		if (ret >= 0 || ret == SCE_UDCD_ERROR_INVALID_ARGUMENT) {
			ret = 0;
			goto done;
		}
		SceUdcdForDriver_1494293B("USBPSPCommunicationDriver", 0, NULL);
	}
	SceUdcdForDriver_1494293B("USBDeviceControllerDriver", 0, NULL);
done:
	sceSyscallLeave(syscall);
	return ret;
}

int sceUsbPspcmStopDevice(void)
{
	unsigned syscall = sceSyscallEnter(), time;
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (!allowed())
		goto done;
	ret = SceUdcdForDriver_C0CA5DDB("USBPSPCommunicationDriver");
	if (ret == 2) {
		ret = SceUdcdForDriver_8AE87657();
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
		time = SceThreadmgrForDriver_47F6DE49();
		do {
			sceKernelDelayThread(1000);
			ret = SceUdcdForDriver_1494293B("USBPSPCommunicationDriver", 0, NULL);
			if (ret >= 0)
				break;
		} while ((unsigned)(SceThreadmgrForDriver_47F6DE49() - time) < 5000000);
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
		ret = SceUdcdForDriver_1494293B("USBDeviceControllerDriver", 0, NULL);
		if (ret < 0 && ret != SCE_UDCD_ERROR_INVALID_ARGUMENT)
			goto done;
	}
	sceUdcdStart("USBDeviceControllerDriver", 0, NULL);
	sceUdcdStart("USB_MTP_Driver", 0, NULL);
	sceUdcdActivate(0x4e4);
	ret = 0;
done:
	sceSyscallLeave(syscall);
	return ret;
}

int sceUsbPspcmDevctl(unsigned command, const void *input, void *output, unsigned outputSize)
{
	unsigned syscall = sceSyscallEnter(), size = 0, timeout = 0, result;
	unsigned local[3], duplicate[3];
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (!allowed())
		goto done;
	ret = PCM_ERROR_NOT_READY;
	if (!checkReady())
		goto done;
	if (command == 0x3415001 || command == 0x3415002 || command == 0x3415006 ||
	    command == 0x3416102 || command == 0x3415104)
		size = 4;
	else if (command == 0x3435005)
		size = 12;
	if (size) {
		ret = PCM_ERROR_ARGUMENT;
		if (!input)
			goto done;
		ret = SceSysmemForDriver_BC996A7A(local, input, size);
		if (ret < 0)
			goto done;
		if (command == 0x3435005) {
			ret = SceSysmemForDriver_BC996A7A(duplicate, input, 12);
			if (ret < 0)
				goto done;
			if (duplicate[2]) {
				ret = SceSysmemForDriver_BC996A7A(&timeout, (void *)(uintptr_t)duplicate[2], 4);
				if (ret < 0)
					goto done;
				local[2] = (uintptr_t)&timeout;
			} else {
				local[2] = 0;
			}
		}
	}
	ret = PCM_ERROR_ARGUMENT;
	if (outputSize && (!output || outputSize > 4))
		goto done;
	ret = devctl(command, local, size, &result, outputSize);
	if (ret >= 0 && outputSize)
		ret = SceSysmemForDriver_6D88EF8A(output, &result, outputSize);
done:
	sceSyscallLeave(syscall);
	return ret;
}

#define CHANNEL_EXPORT(name, operation) \
int name(unsigned unit) \
{ \
	unsigned syscall = sceSyscallEnter(); \
	int ret = SCE_ERROR_ERRNO_ENOSYS; \
	if (allowed()) \
		ret = checkReady() ? operation(unit) : PCM_ERROR_NOT_READY; \
	sceSyscallLeave(syscall); \
	return ret; \
}
CHANNEL_EXPORT(sceUsbPspcmOpen, openChannel)
CHANNEL_EXPORT(sceUsbPspcmClose, closeChannel)
CHANNEL_EXPORT(sceUsbPspcmIoctlReset, ioctlReset)
CHANNEL_EXPORT(sceUsbPspcmIoctlPollData, ioctlPoll)
CHANNEL_EXPORT(sceUsbPspcmIoctlCancelWaitData, ioctlCancel)
int sceUsbPspcmIoctlWaitData(unsigned unit, const unsigned *timeout)
{
	unsigned syscall = sceSyscallEnter(), time;
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (allowed()) {
		ret = PCM_ERROR_NOT_READY;
		if (checkReady()) {
			ret = SceSysmemForDriver_BC996A7A(&time, timeout, 4);
			if (ret >= 0)
				ret = ioctlWait(unit, &time);
		}
	}
	sceSyscallLeave(syscall);
	return ret;
}

int sceUsbPspcmWrite(unsigned unit, const void *data, unsigned size, unsigned mode)
{
	unsigned syscall = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (allowed())
		ret = checkReady() ? writeChannel(unit, data, size, mode, 1) : PCM_ERROR_NOT_READY;
	sceSyscallLeave(syscall);
	return ret;
}

int sceUsbPspcmRead(unsigned unit, void *data, unsigned size, unsigned mode)
{
	unsigned syscall = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;
	if (allowed())
		ret = checkReady() ? readChannel(unit, data, size, mode, 1) : PCM_ERROR_NOT_READY;
	sceSyscallLeave(syscall);
	return ret;
}
