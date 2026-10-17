/* SceDbgUsb: development USB byte transport, bus 1. */
#include <psp2kern/kernel/cpu.h>
#include <psp2kern/kernel/dipsw.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/udcd.h>
#include <psp2kern/usbserv.h>
#include <sysmem/debug.h>
#include <sysmem/barrier.h>

#define BUFFER_SIZE 0x12000
#define ERR_REGISTERED ((int)0x8024fff1)
#define ERR_DETACHED ((int)0x8024fff2)
#define ERR_UNREGISTERED ((int)0x8024fff3)

typedef struct DbgUsbBuffer {
	struct DbgUsbBuffer *next, *prev;
	void *data;
	unsigned int remaining, received;
	SceUdcdDeviceRequest request;
	unsigned int reserved[5];
	unsigned int capacity;
	SceUID memblock;
	unsigned int index;
} DbgUsbBuffer;

typedef struct DbgUsbQueue {
	DbgUsbBuffer *next, *prev;
	int lock;
	SceUID event;
} DbgUsbQueue;

static DbgUsbQueue s_free, s_done;
static DbgUsbBuffer s_read[2], s_write;
static DbgUsbBuffer *s_pending;
static SceUID s_notify = -1, s_status = -1;
static SceUID s_statusThread = -1, s_readThread = -1;
static SceUID s_readMutex = -1, s_writeMutex = -1;
static SceUdcdEndpoint s_endpoints[3] = {{0, 0}, {0x80, 1}, {0, 2}};
static SceUdcdInterface s_interface = {-1, 0, 1};

/* Callback names were not retained in the debug records. */
static int RejectRequest(int recipient, int arg, SceUdcdEP0DeviceRequest *req,
			 void *user) /* name unknown */
{
	return -1;
}

static int DriverStartStop(int size, void *args, void *user) /* name unknown */
{
	return 0;
}

static int Attach(int version, void *user) /* name unknown */
{
	return ksceKernelSetEventFlag(s_status, 0x10000010);
}

static void Detach(void *user) /* name unknown */
{
	ksceKernelClearEventFlag(s_status, ~0x10u);
	ksceKernelSetEventFlag(s_status, 0x10000000);
}

static void WriteComplete(SceUdcdDeviceRequest *req) /* name unknown */
{
	ksceKernelClearEventFlag(s_status, ~0x100u);
	ksceKernelSetEventFlag(s_status, 0x1000);
}

static SceUdcdEndpointDescriptor s_epHi[3] = {
	{7, 5, 0x81, 2, 0x200}, {7, 5, 2, 2, 0x200}, {0}
};
static SceUdcdEndpointDescriptor s_epFull[3] = {
	{7, 5, 0x81, 2, 0x40}, {7, 5, 2, 2, 0x40}, {0}
};
static SceUdcdInterfaceDescriptor s_ifHi[4] = {
	{9, 4, 0, 0, 2, 0xff, 0, 0, 1, s_epHi}
};
static SceUdcdInterfaceDescriptor s_ifFull[2] = {
	{9, 4, 0, 0, 2, 0xff, 0, 0, 1, s_epFull}
};
static SceUdcdInterfaceSettings s_settingsHi = {s_ifHi, 0, 1};
static SceUdcdInterfaceSettings s_settingsFull = {s_ifFull, 0, 1};
static SceUdcdConfigDescriptor s_configHi = {
	9, 2, 0x20, 1, 1, 0, 0xc0, 0, &s_settingsHi
};
static SceUdcdConfigDescriptor s_configFull = {
	9, 2, 0x20, 1, 1, 0, 0xc0, 0, &s_settingsFull
};
static SceUdcdDeviceDescriptor s_deviceHi = {
	0x12, 1, 0x200, 0xff, 0, 0, 0x40, 0, 0, 0x120, 0, 0, 0, 1
};
static SceUdcdDeviceDescriptor s_deviceFull = {
	0x12, 1, 0x200, 0xff, 0, 0, 0x40, 0, 0, 0x120, 0, 0, 0, 1
};
static SceUdcdConfiguration s_configurationHi = {
	&s_configHi, &s_settingsHi, s_ifHi, s_epHi
};
static SceUdcdConfiguration s_configurationFull = {
	&s_configFull, &s_settingsFull, s_ifFull, s_epFull
};
static SceUdcdStringDescriptor s_string[2] = {
	{0x28, 3, {'U', 'S', 'B', ' ', 'D', 'e', 'b', 'u', 'g', ' ',
		 'I', 'n', 't', 'e', 'r', 'f', 'a', 'c', 'e'}}
};
static SceUdcdDriver s_driver = {
	.driverName = "USB_DbgUsb_Driver",
	.numEndpoints = 3,
	.endpoints = s_endpoints,
	.interface = &s_interface,
	.descriptor_hi = &s_deviceHi,
	.configuration_hi = &s_configurationHi,
	.descriptor = &s_deviceFull,
	.configuration = &s_configurationFull,
	.stringDescriptors = s_string,
	.processRequest = RejectRequest,
	.attach = Attach,
	.detach = Detach,
	.start = DriverStartStop,
	.stop = DriverStartStop,
};

static void QueuePut(DbgUsbQueue *queue, DbgUsbBuffer *buffer) /* name unknown */
{
	int state = ksceKernelSpinlockLowLockCpuSuspendIntr(&queue->lock);

	buffer->next = (DbgUsbBuffer *)queue;
	buffer->prev = queue->prev;
	queue->prev = buffer;
	buffer->prev->next = buffer;
	ksceKernelSetEventFlag(queue->event, 1);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&queue->lock, state);
}

static DbgUsbBuffer *QueueGet(DbgUsbQueue *queue) /* name unknown */
{
	int state = ksceKernelSpinlockLowLockCpuSuspendIntr(&queue->lock);
	DbgUsbBuffer *buffer = queue->next;

	if (buffer == (DbgUsbBuffer *)queue) {
		buffer = NULL;
	} else {
		buffer->next->prev = buffer->prev;
		buffer->prev->next = buffer->next;
		buffer->next = buffer;
		buffer->prev = buffer;
	}
	if (queue->next == (DbgUsbBuffer *)queue)
		ksceKernelClearEventFlag(queue->event, ~1u);
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&queue->lock, state);
	return buffer;
}

static int __attribute__((noreturn)) StatusWorker(SceSize size, void *args) /* name unknown */
{
	unsigned int previous = 0, bits, changed;

	for (;;) {
		bits = 0;
		if (ksceKernelWaitEventFlag(s_status, 0x10000000, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT,
					  &bits, NULL) >= 0) {
			changed = previous ^ bits;
			if ((changed & 0x10) && (bits & 1))
				ksceKernelSetEventFlag(s_notify, bits & 0x10 ? 4 : 8);
			if ((changed & 1) && (bits & 0x11) == 0x11)
				ksceKernelSetEventFlag(s_notify, 4);
		}
		previous = bits;
	}
}

static int __attribute__((noreturn)) _sceDbgUsbReadQueueWorker(SceSize size, void *args)
{
	DbgUsbBuffer *buffer;
	int ret;

	for (;;) {
		ksceKernelWaitEventFlag(s_free.event, 1, SCE_EVENT_WAITOR, NULL, NULL);
		buffer = QueueGet(&s_free);
		if (!buffer)
			continue;
		ret = ksceKernelWaitEventFlag(s_status, 0x10, SCE_EVENT_WAITOR, NULL, NULL);
		if (ret >= 0) {
			SCE_DBG_ASSERT(buffer->remaining == 0, 449);
			buffer->request.transmitted = 0;
			buffer->request.returnCode = 1;
			buffer->request.next = NULL;
			ret = ksceUdcdReqRecvInternal(&buffer->request, 1);
			if (ret >= 0)
				continue;
		}
		QueuePut(&s_free, buffer);
	}
}

static void ReadComplete(SceUdcdDeviceRequest *req) /* name unknown */
{
	SceKernelEventFlagInfo info;
	DbgUsbBuffer *buffer = req->unused;
	unsigned int bits = 0;

	info.size = sizeof(info);
	if (ksceKernelGetEventFlagInfo(s_status, &info) >= 0)
		bits = info.currentPattern;
	if (req->returnCode == 0 && (unsigned int)req->transmitted > 3 &&
	    ((unsigned char *)req->data)[3] == 0x40) {
		buffer->received = req->transmitted;
		buffer->remaining = req->transmitted;
		QueuePut(&s_done, buffer);
		if ((bits & 0x11) == 0x11)
			ksceKernelSetEventFlag(s_notify, 2);
	} else {
		buffer->remaining = 0;
		QueuePut(&s_free, buffer);
	}
}

int SceDbgUsbForDriver_A6BFCC0A(SceUID event)
{
	SceKernelEventFlagInfo info;

	info.size = sizeof(info);
	if (ksceKernelGetEventFlagInfo(s_status, &info) >= 0 &&
	    (info.currentPattern & 1))
		return ERR_REGISTERED;
	s_notify = event;
	ksceKernelSetEventFlag(s_status, 0x10000001);
	return 0;
}

int SceDbgUsbForDriver_08CF16D1(void)
{
	SceKernelEventFlagInfo info;

	info.size = sizeof(info);
	if (ksceKernelGetEventFlagInfo(s_status, &info) < 0 ||
	    !(info.currentPattern & 1))
		return ERR_UNREGISTERED;
	ksceKernelClearEventFlag(s_status, ~1u);
	s_notify = -1;
	return 0;
}

int _sceDbgUsbPullData(void *data, unsigned int size)
{
	SceKernelEventFlagInfo info;
	DbgUsbBuffer *buffer;
	unsigned int left = size, count;
	int ret = 0, state, ready;

	ksceKernelLockMutex(s_readMutex, 1, NULL);
	info.size = sizeof(info);
	if (ksceKernelGetEventFlagInfo(s_status, &info) < 0 ||
	    !(info.currentPattern & 1)) {
		ret = ERR_UNREGISTERED;
		goto out;
	}
	if (!(info.currentPattern & 0x10)) {
		ret = ERR_DETACHED;
		goto out;
	}
	SCE_DBG_ASSERT(size <= 0x11000, 672);
	while (left) {
		buffer = s_pending;
		while (!buffer) {
			ksceKernelWaitEventFlag(s_done.event, 1, SCE_EVENT_WAITOR, NULL, NULL);
			buffer = QueueGet(&s_done);
		}
		s_pending = buffer;
		/* Sony clamps to the original size and keeps the destination fixed. */
		count = buffer->remaining < size ? buffer->remaining : size;
		memcpy(data, (char *)buffer->data + buffer->received -
		       buffer->remaining, count);
		buffer = s_pending;
		left -= count;
		ret += count;
		buffer->remaining -= count;
		if (!buffer->remaining) {
			s_pending = NULL;
			QueuePut(&s_free, buffer);
		}
	}
	if (!s_pending || s_pending->remaining) {
		state = ksceKernelSpinlockLowLockCpuSuspendIntr(&s_done.lock);
		ready = s_done.next != (DbgUsbBuffer *)&s_done;
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&s_done.lock, state);
		if (ready)
			ksceKernelSetEventFlag(s_notify, 2);
	}
out:
	ksceKernelUnlockMutex(s_readMutex, 1);
	return ret;
}

int SceDbgUsbForDriver_40C65DB2(const void *data, unsigned int size)
{
	SceKernelEventFlagInfo info;
	int ret;

	ksceKernelLockMutex(s_writeMutex, 1, NULL);
	info.size = sizeof(info);
	ret = ERR_UNREGISTERED;
	if (ksceKernelGetEventFlagInfo(s_status, &info) < 0 ||
	    !(info.currentPattern & 1))
		goto out;
	ret = ERR_DETACHED;
	if (!(info.currentPattern & 0x10))
		goto out;
	ret = 0;
	if (!size)
		goto out;
	if (size >= s_write.capacity)
		size = s_write.capacity;
	memcpy(s_write.data, data, size);
	dmb();
	(void)*(volatile unsigned char *)((char *)s_write.data + size - 1);
	dsb();
	s_write.request.size = size;
	s_write.request.transmitted = 0;
	s_write.request.returnCode = 1;
	s_write.request.next = NULL;
	ksceKernelClearEventFlag(s_status, ~0x1000u);
	ksceKernelSetEventFlag(s_status, 0x100);
	ret = ksceUdcdReqSendInternal(&s_write.request, 1);
	if (ret >= 0) {
		ret = ksceKernelWaitEventFlag(s_status, 0x1000, SCE_EVENT_WAITOR, NULL, NULL);
		if (ret >= 0 && s_write.request.returnCode >= 0 &&
		    s_write.request.transmitted == size)
			ret = size;
	}
out:
	ksceKernelUnlockMutex(s_writeMutex, 1);
	return ret;
}

static void InitRequest(DbgUsbBuffer *buffer, SceUdcdEndpoint *endpoint,
			void (*complete)(SceUdcdDeviceRequest *)) /* name unknown */
{
	buffer->request.endpoint = endpoint;
	buffer->request.data = buffer->data;
	buffer->request.attributes = 1;
	buffer->request.size = buffer->capacity;
	buffer->request.isControlRequest = 0;
	buffer->request.onComplete = complete;
	buffer->request.transmitted = 0;
	buffer->request.returnCode = 1;
	buffer->request.next = NULL;
	buffer->request.unused = buffer;
}

int sceDbgUsbModuleStart(SceSize arglen, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	char name[36];
	unsigned int threadOpt[7];
	int i, ret;

	if (!ksceKernelCheckDipsw(0xc2) || ksceKernelCheckDipsw(0xc3)) {
		SCE_DBG_PRINTF(0xb, 1136, "disabled by dipsw\n");
		return 1;
	}
	for (i = 0; i < 2; i++) {
		snprintf(name, 0x21, "%s%d", "SceDbgUsbRead", i);
		memset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = 0x200004;
		opt.alignment = 0x1000;
		s_read[i].memblock = ksceKernelAllocMemBlock(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_NC_RW,
							 BUFFER_SIZE, &opt);
		if (s_read[i].memblock < 0)
			goto readFail;
		if (ksceKernelGetMemBlockBase(s_read[i].memblock,
					    &s_read[i].data) < 0) {
			ksceKernelFreeMemBlock(s_read[i].memblock);
			s_read[i].memblock = 0;
			goto readFail;
		}
		s_read[i].index = i;
		s_read[i].capacity = BUFFER_SIZE;
		s_read[i].request.size = BUFFER_SIZE;
	}
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200004;
	opt.alignment = 0x1000;
	s_write.memblock = ksceKernelAllocMemBlock("SceDbgUsbWrite", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_NC_RW,
						 BUFFER_SIZE, &opt);
	if (s_write.memblock < 0)
		goto writeFail;
	if (ksceKernelGetMemBlockBase(s_write.memblock, &s_write.data) < 0) {
		ksceKernelFreeMemBlock(s_write.memblock);
		s_write.memblock = 0;
		goto writeFail;
	}
	s_write.capacity = BUFFER_SIZE;
	s_write.request.size = BUFFER_SIZE;
	ret = ksceKernelCreateMutex("SceDbgUsbRead", 0, 0, NULL);
	if (ret < 0)
		goto readMutexFail;
	s_readMutex = ret;
	ret = ksceKernelCreateMutex("SceDbgUsbWrite", 0, 0, NULL);
	if (ret < 0)
		goto writeMutexFail;
	s_writeMutex = ret;
	ret = ksceKernelCreateEventFlag("SceDbgUsbStatus", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (ret < 0)
		goto statusFail;
	s_status = ret;
	s_free.next = s_free.prev = (DbgUsbBuffer *)&s_free;
	s_free.lock = 0;
	s_free.event = ksceKernelCreateEventFlag("SceDbgUsbFreeFifo", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (s_free.event < 0)
		goto freeFail;
	s_done.next = s_done.prev = (DbgUsbBuffer *)&s_done;
	s_done.lock = 0;
	s_done.event = ksceKernelCreateEventFlag("SceDbgUsbDoneFifo", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (s_done.event < 0)
		goto doneFail;
	/* Only the first three option words are replaced after the memblock opt. */
	threadOpt[0] = 0x1c;
	threadOpt[1] = 1;
	threadOpt[2] = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	threadOpt[3] = 0;
	threadOpt[4] = 0;
	threadOpt[5] = 0x1000;
	threadOpt[6] = 0;
	ret = ksceKernelCreateThread("SceDbgUsbStatus", StatusWorker, 0x1d,
				     0x4000, 0, 0xf, (void *)threadOpt);
	if (ret < 0)
		goto statusThreadFail;
	s_statusThread = ret;
	ret = ksceKernelCreateThread("SceDbgUsbReadQueue", _sceDbgUsbReadQueueWorker,
				     0x1d, 0x4000, 0, 0xf, (void *)threadOpt);
	if (ret < 0)
		goto readThreadFail;
	s_readThread = ret;
	for (i = 0; i < 2; i++)
		InitRequest(&s_read[i], &s_endpoints[2], ReadComplete);
	InitRequest(&s_write, &s_endpoints[1], WriteComplete);
	s_write.request.isControlRequest = 1;
	for (i = 0; i < 2; i++)
		QueuePut(&s_free, &s_read[i]);
	if (ksceKernelStartThread(s_statusThread, 0, NULL) < 0) {
		ksceKernelDeleteThread(s_readThread);
		goto readThreadFail;
	}
	ksceKernelStartThread(s_readThread, 0, NULL);
	if (ksceUsbServMacSelect(1, 1) < 0)
		return 2;
	if (ksceUdcdRegisterInternal(&s_driver, 1) < 0)
		return 2;
	if (ksceUdcdStartInternal("USBDeviceControllerDriver", 0, NULL, 1) >= 0) {
		if (ksceUdcdStartInternal("USB_DbgUsb_Driver", 0, NULL, 1) >= 0) {
			if (ksceUdcdActivateInternal(0x4cd, 0, 1) >= 0)
				return 0;
			ksceUdcdStopInternal("USB_DbgUsb_Driver", 0, NULL, 1);
		}
		ksceUdcdStopInternal("USBDeviceControllerDriver", 0, NULL, 1);
	}
	ksceUdcdUnregisterInternal(&s_driver, 1);
	return 2;
readThreadFail:
	ksceKernelDeleteThread(s_statusThread);
statusThreadFail:
	ksceKernelDeleteEventFlag(s_done.event);
doneFail:
	ksceKernelDeleteEventFlag(s_free.event);
freeFail:
	ksceKernelDeleteEventFlag(s_status);
statusFail:
	ksceKernelDeleteMutex(s_writeMutex);
writeMutexFail:
	ksceKernelDeleteMutex(s_readMutex);
readMutexFail:
	ksceKernelFreeMemBlock(s_write.memblock);
writeFail:
	ksceKernelFreeMemBlock(s_write.memblock);
	i = 1;
	goto readPrevious;
readFail:
	ksceKernelFreeMemBlock(s_read[i].memblock);
	i--;
readPrevious:
	if (i >= 0)
		ksceKernelFreeMemBlock(s_read[i].memblock);
	return 2;
}
