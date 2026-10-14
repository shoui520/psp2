/* SceUsbAudio: composite USB audio host driver and input service. */
#include "usbaudio.h"
#include <psp2kern/kernel/sysmem/memtype.h>
#include "sce/error.h"
#include "sce/syscall.h"

#define USBAUDIO_HEAP_SIZE	0x40000

#define AUDIO_ERROR(name) ((int)SCE_USBAUDIO_IN_ERROR_##name)

/* Private global/state/helper names unknown. No Sony debug records exist. */
struct AudioCallback {
	struct AudioCallback *next, *prev;
	SceUID callback;
};
static struct {
	unsigned opened;
	SceKernelFastMutex lock, deviceLock;
	unsigned reserved;
	AudioDevice *device[8];
	struct AudioCallback callbacks;
} s_audio;
static SceUID s_heap = -1;

static int lockAudio(void)
{
	return ksceKernelLockFastMutex(&s_audio.lock);
}
static int unlockAudio(void)
{
	return ksceKernelUnlockFastMutex(&s_audio.lock);
}
static int lockDevices(void)
{
	return ksceKernelLockFastMutex(&s_audio.deviceLock);
}
static int unlockDevices(void)
{
	return ksceKernelUnlockFastMutex(&s_audio.deviceLock);
}
static void *allocate(unsigned size)
{
	return s_heap < 0 ? NULL : ksceKernelAllocHeapMemory(s_heap, size);
}
static void release(void *p)
{
	if (p)
		ksceKernelFreeHeapMemory(s_heap, p);
}
static AudioDevice *findUnlocked(unsigned id)
{
	for (unsigned i = 0; i < 8; i++)
		if (s_audio.device[i] && s_audio.device[i]->id == id)
			return s_audio.device[i];
	return NULL;
}
static AudioPort *findPort(unsigned id)
{
	if (lockDevices())
		return NULL;
	AudioDevice *dev = findUnlocked(id);
	unlockDevices();
	return dev ? &dev->port : NULL;
}
static uint8_t *controlBuffer(AudioDevice *dev)
{
	int at = (int)dev->controlBuffer;
	return (void *)(at + 64 - at % 64);
}
static void notifyDevices(void)
{
	struct AudioCallback *head = &s_audio.callbacks;
	if (head->next && head->prev)
		for (struct AudioCallback *p = head->next; p != head; p = p->next)
			ksceKernelNotifyCallback(p->callback, 0);
}

static int probeAudio(int id, SceUsbdInterfaceDescriptor **descriptor);
static int attachAudio(int id);
static int detachAudio(int id);
static void completeSetup(int result, int length, void *arg);
static void completeInterface(int result, int length, void *arg);
static void completeInput(int result, ksceUsbdIsochTransfer *packet, void *arg);
static void completeCapture(int result, ksceUsbdIsochTransfer *packet, void *arg);
static void completeOutput(int result, ksceUsbdIsochTransfer *packet, void *arg);
static void parseDescriptors(unsigned id, AudioPort *port);

static int cleanupProcess(int process)
{
	unsigned ids[127];
	int n = ksceUsbAudioGetDeviceIdList(ids, 127);
	int ret = 0;
	for (int i = 0; i < n; i++) {
		AudioPort *port = findPort(ids[i]);
		if (!port) {
			ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
			continue;
		}
		AudioStream *stream = port->stream;
		if (stream->process != process || !stream->opened)
			continue;
		stream->opened = 0;
		stream->process = -1;
		int error = ksceKernelClearEventFlag(stream->event, 0);
		if (error)
			ret = error;
		error = lockAudio();
		if (error)
			return error;
		if (s_audio.opened)
			s_audio.opened--;
		unlockAudio();
	}
	return ret;
}

static __attribute__((noipa)) int processExit(SceUID pid, SceProcEventInvokeParam1 *param, int arg)
{
	return cleanupProcess(pid);
}
static __attribute__((noipa)) int processKill(SceUID pid, SceProcEventInvokeParam1 *param, int arg)
{
	return cleanupProcess(pid);
}

static int initializeAudio(void)
{
	int ret = ksceKernelInitializeFastMutex(&s_audio.lock, "SceUsbAudio", 0x2000, 0);
	if (!ret)
		ret = ksceKernelInitializeFastMutex(&s_audio.deviceLock,
			"SceUsbAudioDeviceLock", 0x2000, 0);
	if (ret) {
		ksceKernelPrintf("USBAUDIO: sceKernelInitializeFastMutex() failed! => 0x%x", ret);
		ksceKernelPrintf("\n");
		return ret;
	}
	SceKernelHeapCreateOpt opt = {
		.size = sizeof(opt),
		.flags = HEAP_HAS_MAX_SIZE | HEAP_HAS_MEMBLOCK,
		.maxSize = USBAUDIO_HEAP_SIZE,
	};
	opt.memBlock = ksceKernelAllocMemBlock("SceUsbAudio", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
					       USBAUDIO_HEAP_SIZE, NULL);
	s_heap = ksceKernelCreateHeap("SceUsbAudio", 0x1000, &opt);
	if (s_heap < 0) {
		ksceKernelPrintf("USBAUDIO: sceKernelCreateHeap() failed! => 0x%x", s_heap);
		ksceKernelPrintf("\n");
		return s_heap;
	}
	return 0;
}
int module_start(unsigned arglen, const void *argp)
{
	static const SceUsbdCompositeDriver driver = {
		"SceUsbAudio", probeAudio, attachAudio, detachAudio
	};
	static const SceProcEventHandler events = {
		.size = sizeof(events),
		.exit = processExit,
		.kill = processKill,
	};
	s_audio.opened = 0;
	if (initializeAudio())
		return 2;
	int ret = ksceUsbdRegisterCompositeLdd(&driver);
	if (ret) {
		ksceKernelPrintf("USBAUDIO: sceUsbdRegisterCompositeLdd() failed! => 0x%x", ret);
		ksceKernelPrintf("\n");
		return 2;
	}
	return ksceKernelRegisterProcEventHandler("SceUsbAudio", &events, 0) < 0 ? 2 : 0;
}

int ksceUsbAudioRegisterCallback(int callback)
{
	int ret = lockAudio();
	if (ret)
		return ret;
	struct AudioCallback *head = &s_audio.callbacks;
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
	p->prev = head->prev;
	p->callback = callback;
	p->next = head;
	head->prev->next = p;
	head->prev = p;
	ksceKernelNotifyCallback(callback, 0);
out:
	unlockAudio();
	return ret;
}
int ksceUsbAudioUnregisterCallback(int callback)
{
	int ret = lockAudio();
	if (ret)
		return ret;
	struct AudioCallback *head = &s_audio.callbacks;
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
int ksceUsbAudioGetDeviceIdList(unsigned *list, int size)
{
	if (!list)
		return AUDIO_ERROR(INVALID_ARGUMENT);
	if (lockDevices())
		return 0;
	int n = 0;
	for (unsigned i = 0; i < 8; i++) {
		AudioDevice *dev = s_audio.device[i];
		if (dev && dev->state == 7) {
			list[n++] = dev->id;
			if (n >= (size > 7 ? 8 : size))
				break;
		}
	}
	unlockDevices();
	return n;
}

static void completeVolume(int result, int length, void *arg)
{
	AudioDevice *dev = arg;
	dev->controlResult = result;
	ksceKernelSetEventFlag(dev->event, 1);
}
static int volumeRequest(unsigned id, unsigned request, unsigned value, int *out)
{
	if (lockDevices())
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	AudioDevice *dev = findUnlocked(id);
	unlockDevices();
	if (!dev)
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	if (dev->port.volumeUnit == UAC_NO_UNIT)
		return AUDIO_ERROR(NOT_SUPPORTED);
	int ret = ksceKernelLockFastMutex(&dev->mutex);
	int volume = 0;
	if (!ret) {
		uint8_t *buffer = controlBuffer(dev);
		if (!(request & SCE_USBD_REQTYPE_DIR_TO_HOST))
			*(uint16_t *)buffer = value;
		SceUsbdDeviceRequest req = {
			.bmRequestType = (request & SCE_USBD_REQTYPE_DIR_TO_HOST ?
					  SCE_USBD_REQTYPE_DIR_TO_HOST : SCE_USBD_REQTYPE_DIR_TO_DEVICE) |
					 SCE_USBD_REQTYPE_TYPE_CLASS | SCE_USBD_REQTYPE_RECIP_INTERFACE,
			.bRequest = request,
			.wValue = UAC_VOLUME_CONTROL << 8,
			.wIndex = dev->port.volumeUnit << 8 | dev->port.volumeInterface,
			.wLength = 2,
		};
		dev->controlResult = 0;
		ret = ksceUsbdControlTransfer(dev->port.controlPipe, &req, buffer, completeVolume, dev);
		unsigned timeout = 1000000;
		int wait = ksceKernelWaitEventFlag(dev->event, 1, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR, 0, &timeout);
		if (request & SCE_USBD_REQTYPE_DIR_TO_HOST)
			volume = *(int16_t *)buffer;
		ksceKernelUnlockFastMutex(&dev->mutex);
		if (!ret)
			ret = wait ? wait : dev->controlResult;
	}
	if (out)
		*out = volume;
	return ret;
}
static int owned(unsigned id)
{
	AudioPort *port = findPort(id);
	if (!port)
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	int owner = port->stream->process;
	return owner == sceKernelGetProcessId() ? 0 : AUDIO_ERROR(PROCESS_HAS_NOT_A_DEVICE_OWNERSHIP);
}
static int getVolume(SceUInt32 id, SceUInt32 *out, unsigned req)
{
	unsigned domain = sceSyscallEnter();
	int value = 0, ret = owned(id);
	if (!ret) {
		if (!out)
			ret = AUDIO_ERROR(INVALID_ARGUMENT);
		else if (volumeRequest(id, req, 0, &value))
			ret = AUDIO_ERROR(NOT_SUPPORTED);
		else if (ksceKernelCopyToUser(out, &value, 4))
			ret = AUDIO_ERROR(FAILED_TO_COPY_BUFFER);
	}
	sceSyscallLeave(domain);
	return ret;
}
int SceUsbAudioIn_198A7C33(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, UAC_GET_CUR);
}
SceInt32 sceUsbAudioInGetMaxValueOfVolume(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, UAC_GET_MAX);
}
SceInt32 sceUsbAudioInGetMinValueOfVolume(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, UAC_GET_MIN);
}
SceInt32 sceUsbAudioInSetCurrentValueOfVolume(SceUInt32 id, SceUInt32 value)
{
	unsigned domain = sceSyscallEnter();
	int ret = owned(id);
	if (!ret && volumeRequest(id, UAC_SET_CUR, value, NULL))
		ret = AUDIO_ERROR(NOT_SUPPORTED);
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInGetDeviceInformation(SceUInt32 id, SceUsbAudioInDeviceInfo *out)
{
	unsigned domain = sceSyscallEnter();
	int ret;
	SceUsbAudioInDeviceInfo info;
	if (!out)
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
	else if (!findPort(id))
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
	else {
		uint8_t *desc = (void *)ksceUsbdScanStaticDescriptor(id, 0, 1);
		if (!desc)
			ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
		else {
			info.vendor = *(uint16_t *)(desc + 8);
			info.product = *(uint16_t *)(desc + 10);
			ret = ksceKernelCopyToUser(out, &info, sizeof(info)) ?
				AUDIO_ERROR(FAILED_TO_COPY_BUFFER) : 0;
		}
	}
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInGetDeviceIdList(SceUsbAudioInDeviceListItem *list, SceUInt32 *count, SceUInt32 size)
{
	unsigned domain = sceSyscallEnter();
	int ret;
	unsigned ids[127];
	if (!count || !list || (int)size < 1)
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
	else {
		int n = ksceUsbAudioGetDeviceIdList(ids, 127);
		int copied = n < (int)size ? n : size;
		ret = ksceKernelCopyToUser(list, ids, copied * 4);
		if (!ret)
			ret = ksceKernelCopyToUser(count, &copied, 4);
	}
	sceSyscallLeave(domain);
	return ret;
}

static int submitIso(int pipe, AudioIso *request, ksceUsbdIsochDoneCallback callback, void *arg)
{
	return ksceUsbdIsochronousTransfer(pipe, (void *)request, callback, arg);
}
static void completeInput(int result, ksceUsbdIsochTransfer *packet, void *arg)
{
	AudioDma *dma = arg;
	if (!dma || !dma->stream)
		return;
	AudioStream *stream = dma->stream;
	if (result == 8) {
		ksceKernelSetEventFlag(stream->event, 8);
		return;
	}
	unsigned *done = (void *)packet;
	unsigned bytes = done[1] * stream->frameBytes * stream->frames;
	stream->active--;
	AudioPort *port = findPort(stream->device);
	if (!stream->opened || port->halted) {
		if (!stream->active)
			ksceKernelSetEventFlag(stream->event, STREAM_STOPPED);
		return;
	}
	AudioRing *ring = &stream->ring[stream->write];
	memcpy(ring->data + ring->used, (void *)done[0], bytes);
	ring->used += bytes;
	if (!submitIso(port->inputPipe, &dma->request, completeInput, dma)) {
		stream->active++;
		if (ring->used >= ring->size) {
			ring->used = 0;
			stream->ready = stream->write;
			stream->write = (stream->write + 1) % 3;
			ksceKernelSetEventFlag(stream->event, STREAM_DATA_READY);
		}
	} else {
		ksceKernelSetEventFlag(stream->event, STREAM_SUBMIT_FAILED);
	}
}
SceInt32 sceUsbAudioInOpenDevice(SceUInt32 id, int bits, int rate)
{
	unsigned domain = sceSyscallEnter();
	int ret;
	if (rate != 48000 || bits != 16) {
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
		goto out;
	}
	ret = lockAudio();
	if (ret)
		goto out;
	if (s_audio.opened >= 2) {
		ret = AUDIO_ERROR(PROCESS_CANNOT_OPEN_MORE_DEVICE);
		goto unlock;
	}
	AudioPort *port = findPort(id);
	if (!port) {
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
		goto unlock;
	}
	AudioStream *stream = port->stream;
	if (ksceKernelTryLockMutex(stream->mutex, 1) < 0) {
		ret = AUDIO_ERROR(CANNOT_GET_PORT_OWNERSHIP);
		goto unlock;
	}
	if (stream->opened) {
		ret = AUDIO_ERROR(PORT_IS_ALREADY_OPENED);
		goto unlockPort;
	}
	port->halted = 0;
	stream->opened = 1;
	stream->process = sceKernelGetProcessId();
	stream->frameBytes = 2;
	stream->frames = 48;
	ksceKernelClearEventFlag(stream->event, 0);
	for (unsigned i = 0; i < 3; i++) {
		AudioDma *dma = stream->dma[i];
		dma->request.buffer = dma;
		dma->request.packets = 8;
		for (unsigned j = 0; j < 8; j++) {
			dma->request.packet[j].length = stream->frameBytes * stream->frames;
			dma->request.packet[j].status = 0;
		}
	}
	for (unsigned i = 0; i < 3; i++) {
		stream->ring[i].size = stream->frameBytes * stream->frames * 16;
		stream->ring[i].used = 0;
	}
	ksceUsbd_7938DAC7(port->inputPipe);
	stream->active = 0;
	for (unsigned i = 0; i < 3; i++) {
		if (submitIso(port->inputPipe, &stream->dma[i]->request, completeInput, stream->dma[i]) < 0) {
			ret = AUDIO_ERROR(FAILED_TO_REQUEST_ISOCHRONOUS);
			goto unlockPort;
		}
		stream->active++;
	}
	s_audio.opened++;
	unsigned flags[2] = { 0 }, timeout = 1000000;
	ret = ksceKernelWaitEventFlag(stream->event, STREAM_DATA_READY | STREAM_HALTED,
				      SCE_EVENT_WAITOR, flags, &timeout);
	ret = ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? AUDIO_ERROR(TIMEOUT) :
		ret ? AUDIO_ERROR(FAILED_TO_READ_STREAM) : 0;
 unlockPort:
	ksceKernelUnlockMutex(stream->mutex, 1);
 unlock:
	unlockAudio();
 out:
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInCloseDevice(SceUInt32 id)
{
	unsigned domain = sceSyscallEnter();
	AudioPort *port = findPort(id);
	int ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
	if (port) {
		AudioStream *stream = port->stream;
		ret = owned(id);
		if (!ret) {
			unsigned flags[2] = { 0 }, timeout = 1000000;
			stream->opened = 0;
			ksceKernelWaitEventFlag(stream->event, 4, SCE_EVENT_WAITAND | SCE_EVENT_WAITCLEAR_PAT, flags, &timeout);
			ksceKernelClearEventFlag(stream->event, 0);
			stream->process = -1;
			ret = lockAudio();
			if (!ret) {
				if (s_audio.opened)
					s_audio.opened--;
				unlockAudio();
			}
		}
	}
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInInput(SceUInt32 id, void *buffer)
{
	unsigned domain = sceSyscallEnter();
	AudioPort *port = findPort(id);
	int ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
	if (!port)
		goto out;
	if (!buffer) {
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
		goto out;
	}
	AudioStream *stream = port->stream;
	ret = owned(id);
	if (ret)
		goto out;
	ret = port->halted;
	if (ret)
		goto out;
	unsigned flags[2] = { 0 }, timeout = 64000;
	ret = ksceKernelWaitEventFlag(stream->event, 10, SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR_PAT, flags, &timeout);
	if (ret) {
		ret = ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? AUDIO_ERROR(TIMEOUT) : AUDIO_ERROR(FAILED_TO_READ_STREAM);
		goto out;
	}
	if (flags[0] & 8) {
		ret = port->halted = AUDIO_ERROR(DEVICE_WAS_HALTED);
		goto out;
	}
	if (flags[0] & 16) {
		ret = port->halted = AUDIO_ERROR(FAILED_TO_REQUEST_ISOCHRONOUS);
		goto out;
	}
	if (stream->ready < 3) {
		AudioRing *ring = &stream->ring[stream->ready];
		ret = ksceKernelCopyToUser(buffer, ring->data, ring->size) ?
			AUDIO_ERROR(FAILED_TO_COPY_BUFFER) : 0;
	} else {
		ret = AUDIO_ERROR(NO_DATA_TO_READ);
	}
 out:
	sceSyscallLeave(domain);
	return ret;
}

static void completeCapture(int result, ksceUsbdIsochTransfer *packet, void *arg)
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
static void completeOutput(int result, ksceUsbdIsochTransfer *packet, void *arg)
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
static AudioPacket *freePacket(AudioQueue *queue)
{
	for (unsigned i = 0; i < 3; i++)
		if (!queue->packet[i].owner)
			return &queue->packet[i];
	return NULL;
}
static void flushQueue(AudioPort *port, AudioQueue *queue, int capture)
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
static void queuePacket(AudioQueue *queue, AudioPacket *packet)
{
	if (queue->pending < 2) {
		queue->queued[queue->write] = packet;
		queue->pending++;
		queue->write = (queue->write + 1) % 2;
	}
}
int ksceUsbAudioInput(unsigned id, void *buffer, unsigned size)
{
	unsigned packets = size / 192;
	if (size != packets * 192 || packets - 1 > 7 || !buffer)
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
int ksceUsbAudioInput2(unsigned id, void *buffer, unsigned size)
{
	unsigned packets = size / 192;
	if (size != packets * 192 || packets - 1 > 7 || !buffer)
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

/* the next descriptor of a type after previous (type 0: any) */
static void *scan(unsigned id, const void *previous, int type)
{
	return ksceUsbdScanStaticDescriptor(id, (void *)previous, type);
}
static int interfaceMatches(const SceUsbdInterfaceDescriptor *interface, unsigned subclass)
{
	return interface && interface->bLength >= sizeof(*interface) &&
	       interface->bDescriptorType == SCE_USBD_DESCRIPTOR_INTERFACE &&
	       interface->bInterfaceClass == SCE_USBD_CLASS_AUDIO &&
	       interface->bInterfaceSubclass == subclass;
}
/* the endpoint of a streaming interface's alternate setting */
static SceUsbdEndpointDescriptor *streamEndpoint(unsigned id, const SceUsbdInterfaceDescriptor *interface)
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
static const UacAsGeneralDescriptor *streamGeneral(unsigned id, const SceUsbdInterfaceDescriptor *interface)
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
static const UacHeaderDescriptor *controlHeader(unsigned id, const SceUsbdInterfaceDescriptor *interface)
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
static unsigned sampleRate(const uint8_t *rate)
{
	return rate[0] | rate[1] << 8 | rate[2] << 16;
}
static unsigned entityIndex(const UacEntityDescriptor **entities, unsigned n, uint8_t id)
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
static void findVolumeUnit(unsigned id, AudioPort *port)
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
	if (root < n) {
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
	}
	port->volumeInterface = interface->bInterfaceNumber;
}
/* a 16-bit mono format with 48 kHz among its rates, after a streaming interface */
static const UacFormatTypeIDescriptor *findFormat(unsigned id, const SceUsbdInterfaceDescriptor *interface)
{
	const UacFormatTypeIDescriptor *format = (const void *)interface;

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
static void parseDescriptors(unsigned id, AudioPort *port)
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

			for (;;) {
				interface = scan(id, interface, SCE_USBD_DESCRIPTOR_INTERFACE);
				if (!interface)
					break;
				if (!interfaceMatches(interface, UAC_SUBCLASS_AUDIOSTREAMING) ||
				    interface->bInterfaceNumber != header->baInterfaceNr[c] ||
				    !interface->bNumEndpoints)
					continue;
				const UacAsGeneralDescriptor *general = streamGeneral(id, interface);
				const UacFormatTypeIDescriptor *format = findFormat(id, interface);
				if (!format)
					continue;
				AudioFormat info;
				info.supported = info.channels = 1;
				info.bits = 16;
				/* Sony leaves the reserved word uninitialized. */
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

static void decodeString(AudioDevice *dev, unsigned length, char *out)
{
	uint8_t *buffer = controlBuffer(dev);
	if (length - 2 > USB_STRING_MAX - 2 || length != buffer[0] ||
	    buffer[1] != SCE_USBD_DESCRIPTOR_STRING)
		return;
	buffer[length] = buffer[length + 1] = 0;
	char *limit = out + USB_STRING_MAX / 2;
	uint16_t *p = (void *)(buffer + 2);
	while (*p && out + 3 < limit) {
		unsigned code = *p++;
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
static int requestInterface(AudioDevice *dev, const SceUsbdInterfaceDescriptor *interface,
			    unsigned state)
{
	SceUsbdDeviceRequest req = {
		.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_DEVICE | SCE_USBD_REQTYPE_TYPE_STANDARD |
				 SCE_USBD_REQTYPE_RECIP_INTERFACE,
		.bRequest = SCE_USBD_REQUEST_SET_INTERFACE,
		.wValue = interface->bAlternateSetting,
		.wIndex = interface->bInterfaceNumber,
	};
	int ret = ksceUsbdControlTransfer(dev->port.controlPipe, &req, NULL, completeInterface,
		(void *)dev->id);
	if (!ret)
		dev->state = state;
	return ret;
}
static void requestStreamInterfaces(AudioDevice *dev)
{
	if (dev->port.inputInterface && dev->port.inputEndpoint)
		requestInterface(dev, dev->port.inputInterface, DEVICE_INPUT_INTERFACE);
	else if (dev->port.outputInterface && dev->port.outputEndpoint)
		requestInterface(dev, dev->port.outputInterface, DEVICE_OUTPUT_INTERFACE);
}
static void requestString(AudioDevice *dev, unsigned index, unsigned state)
{
	SceUsbdDeviceRequest req = {
		.bmRequestType = SCE_USBD_REQTYPE_DIR_TO_HOST | SCE_USBD_REQTYPE_TYPE_STANDARD |
				 SCE_USBD_REQTYPE_RECIP_DEVICE,
		.bRequest = SCE_USBD_REQUEST_GET_DESCRIPTOR,
		.wValue = SCE_USBD_DESCRIPTOR_STRING << 8 | index,
		.wIndex = USB_LANGID_EN_US,
		.wLength = USB_STRING_MAX,
	};
	if (!ksceUsbdControlTransfer(dev->port.controlPipe, &req, controlBuffer(dev), completeSetup,
	    (void *)dev->id))
		dev->state = state;
}
static void completeSetup(int result, int length, void *arg)
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
static void completeInterface(int result, int length, void *arg)
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
			if (!ksceUsbdControlTransfer(dev->port.controlPipe, &req, buffer,
			    completeSetup, (void *)dev->id))
				dev->state = DEVICE_SAMPLING_RATE;
		} else if (dev && dev->state == DEVICE_OUTPUT_INTERFACE) {
			notifyDevices();
			dev->state = DEVICE_READY;
		}
	}
	unlockAudio();
}
static int addDevice(unsigned id)
{
	if (lockDevices())
		return 0;
	unsigned index;
	for (index = 0; index < 8; index++)
		if (!s_audio.device[index])
			break;
	if (index == 8)
		return 0;
	AudioDevice *dev = allocate(sizeof(*dev));
	if (!dev)
		return 0;
	memset(dev, 0, sizeof(*dev));
	s_audio.device[index] = dev;
	unlockDevices();
	dev->event = -1;
	dev->id = id;
	dev->event = ksceKernelCreateEventFlag("SceUsbAudioIn", 0, 0, 0);
	if (ksceKernelInitializeFastMutex(&dev->mutex, "SceUsbAudioIn", 0x2000, 0))
		return 0;
	const SceUsbdDeviceDescriptor *descriptor = scan(id, NULL, SCE_USBD_DESCRIPTOR_DEVICE);
	if (descriptor) {
		dev->vendor = descriptor->idVendor;
		dev->product = descriptor->idProduct;
		dev->version = descriptor->bcdDevice;
		dev->manufacturerId = descriptor->iManufacturer;
		dev->productId = descriptor->iProduct;
		dev->serialId = descriptor->iSerialNumber;
	}
	parseDescriptors(id, &dev->port);
	if (dev->port.inputInterface && dev->port.inputEndpoint) {
		dev->port.inputQueue = allocate(sizeof(AudioQueue));
		if (dev->port.inputQueue)
			memset(dev->port.inputQueue, 0, sizeof(AudioQueue));
	}
	if (dev->port.outputInterface && dev->port.outputEndpoint) {
		/* playback has no completion ring */
		dev->port.outputQueue = allocate(offsetof(AudioQueue, completed));
		if (dev->port.outputQueue)
			memset(dev->port.outputQueue, 0, offsetof(AudioQueue, completed));
	}
	dev->port.controlPipe = ksceUsbdOpenPipe(id, NULL);
	if (dev->port.controlPipe >= 0) {
		if (dev->port.inputEndpoint) {
			dev->port.inputPipe = ksceUsbdOpenPipe(id, dev->port.inputEndpoint);
			if (dev->port.inputPipe < 0)
				goto setup;
		}
		if (dev->port.outputEndpoint)
			dev->port.outputPipe = ksceUsbdOpenPipe(id, dev->port.outputEndpoint);
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
static int attachAudio(int id)
{
	if (lockAudio())
		return -1;
	int ret = -1;
	if (addDevice(id)) {
		AudioPort *port = findPort(id);
		if (!port) {
			ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
			goto out;
		}
		AudioStream *stream = allocate(sizeof(*stream));
		port->stream = stream;
		if (!stream) {
			ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
			goto out;
		}
		stream->active = stream->write = stream->opened = 0;
		stream->ready = stream->device = stream->process = -1;
		for (unsigned i = 0; i < 3; i++) {
			stream->memblock[i] = ksceKernelAllocMemBlock("SceUsbAudio", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
								 0x1000, NULL);
			if (stream->memblock[i] < 0)
				stream->dma[i] = NULL;
			else {
				ksceKernelGetMemBlockBase(stream->memblock[i], (void **)&stream->dma[i]);
				memset(stream->dma[i], 0, sizeof(AudioDma));
			}
			memset(&stream->ring[i], 0, sizeof(AudioRing));
			stream->dma[i]->stream = stream;
		}
		stream->device = id;
		stream->event = ksceKernelCreateEventFlag("SceUsbAudio", 0, 0, 0);
		if (stream->event >= 0) {
			stream->mutex = ksceKernelCreateMutex("SceUsbAudio", 0, 0, 0);
			if (stream->mutex < 0)
				ret = 2;
			else {
				port->halted = 0;
				ret = 0;
			}
		}
	}
 out:
	unlockAudio();
	return ret;
}
static int removeDevice(unsigned id)
{
	if (lockDevices())
		return 0;
	AudioDevice *dev = findUnlocked(id);
	if (!dev) {
		unlockDevices();
		return 0;
	}
	release(dev->port.inputQueue);
	dev->port.inputQueue = NULL;
	release(dev->port.outputQueue);
	dev->port.outputQueue = NULL;
	if (dev->port.inputFormat.rates) {
		release(dev->port.inputFormat.rates);
		dev->port.inputFormat.rates = NULL;
	}
	if (dev->port.outputFormat.rates) {
		release(dev->port.outputFormat.rates);
		dev->port.outputFormat.rates = NULL;
	}
	ksceKernelDeleteEventFlag(dev->event);
	release(dev);
	for (unsigned i = 0; i < 8; i++)
		if (s_audio.device[i] == dev) {
			s_audio.device[i] = NULL;
			break;
		}
	notifyDevices();
	unlockDevices();
	return 1;
}
static int detachAudio(int id)
{
	if (lockAudio())
		return -1;
	AudioPort *port = findPort(id);
	int ret = -1;
	if (port) {
		AudioStream *stream = port->stream;
		if (stream->opened && s_audio.opened)
			s_audio.opened--;
		for (unsigned i = 0; i < 3; i++)
			ksceKernelFreeMemBlock(stream->memblock[i]);
		release(stream);
		ret = removeDevice(id) ? 0 : -1;
	}
	unlockAudio();
	return ret;
}
static uint8_t *audioInterface(unsigned id)
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
static int probeAudio(int id, SceUsbdInterfaceDescriptor **descriptor)
{
	if (lockAudio())
		return -1;
	uint8_t *p = audioInterface(id);
	unlockAudio();
	if (!p)
		return -1;
	*descriptor = (SceUsbdInterfaceDescriptor *)p;
	return 0;
}
