/* SceUsbAudio: the module entry, the composite driver callbacks and the SceUsbAudioIn syscalls. */
#include "usbaudio.h"
#include <sysmem/sysmem.h>
#include <kernel/error.h>
#include <intrmgr/syscall.h>

static int probeAudio(int id, SceUsbdInterfaceDescriptor **descriptor);
static int attachAudio(int id);
static int detachAudio(int id);

static SceUsbdCompositeDriver s_driver = {
	"SceUsbAudio", probeAudio, attachAudio, detachAudio
};
static unsigned s_opened;

static __attribute__((noinline)) int cleanupProcess(int process);
static __attribute__((noinline)) int processKill(SceUID pid, ProcEventParam *param, void *common);
static __attribute__((noinline)) int processExit(SceUID pid, ProcEventParam *param, void *common);
static __attribute__((noinline)) int detachAudio(int id);
static __attribute__((noinline)) int attachAudio(int id);
static __attribute__((noinline)) int probeAudio(int id, SceUsbdInterfaceDescriptor **descriptor);
static __attribute__((noinline)) void completeInput(int result, UsbdIsochRequest *packet, void *arg);

static inline __attribute__((always_inline)) int owned(unsigned id)
{
	AudioPort *port = findPort(id);
	if (!port)
		return AUDIO_ERROR(DEVICE_NOT_FOUND);
	int owner = port->stream->process;
	return owner == sceKernelGetProcessId() ? 0 : AUDIO_ERROR(PROCESS_HAS_NOT_A_DEVICE_OWNERSHIP);
}
static inline __attribute__((always_inline)) int getVolume(SceUInt32 id, SceUInt32 *out,
							  int (*request)(unsigned id, int *volume))
{
	unsigned domain = sceSyscallEnter();
	int value = 0, ret = owned(id);
	if (!ret) {
		if (!out)
			ret = AUDIO_ERROR(INVALID_ARGUMENT);
		else if (request(id, &value))
			ret = AUDIO_ERROR(NOT_SUPPORTED);
		else if (sceKernelCopyToUser(out, &value, 4))
			ret = AUDIO_ERROR(FAILED_TO_COPY_BUFFER);
	}
	sceSyscallLeave(domain);
	return ret;
}
static __attribute__((noinline)) int cleanupProcess(int process)
{
	unsigned ids[127];
	int n = SceUsbAudioForDriver_96EC3E18(ids, 127);
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
		int error = sceKernelClearEventFlag(stream->event, 0);
		if (error)
			ret = error;
		error = lockAudio();
		if (error)
			return error;
		if (s_opened)
			s_opened--;
		unlockAudio();
	}
	return ret;
}
static __attribute__((noinline)) int processKill(SceUID pid, ProcEventParam *param, void *common)
{
	return cleanupProcess(pid);
}
static __attribute__((noinline)) int processExit(SceUID pid, ProcEventParam *param, void *common)
{
	return cleanupProcess(pid);
}
static __attribute__((noinline)) int detachAudio(int id)
{
	if (lockAudio())
		return -1;
	AudioPort *port = findPort(id);
	int ret = -1;
	if (port) {
		AudioStream *stream = port->stream;
		if (stream->opened && s_opened)
			s_opened--;
		for (unsigned i = 0; i < 3; i++)
			sceKernelFreeMemBlock(stream->memblock[i]);
		release(port->stream);
		ret = removeDevice(id) ? 0 : -1;
	}
	unlockAudio();
	return ret;
}
static __attribute__((noinline)) int attachAudio(int id)
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
			stream->memblock[i] = sceKernelAllocMemBlockForKernel("SceUsbAudio", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
								 0x1000, NULL);
			if (stream->memblock[i] < 0)
				stream->dma[i] = NULL;
			else {
				sceKernelGetMemBlockBase(stream->memblock[i], (void **)&stream->dma[i]);
				memset(stream->dma[i], 0, sizeof(AudioDma));
			}
			memset(&stream->ring[i], 0, sizeof(AudioRing));
			stream->dma[i]->stream = stream;
		}
		stream = port->stream;
		stream->device = id;
		stream->event = sceKernelCreateEventFlag("SceUsbAudio", 0, 0, 0);
		if (stream->event >= 0) {
			stream->mutex = sceKernelCreateMutex("SceUsbAudio", 0, 0, 0);
			if (stream->mutex < 0) {
				debugHook();
				ret = 2;
			}
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
static __attribute__((noinline)) int probeAudio(int id, SceUsbdInterfaceDescriptor **descriptor)
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
static __attribute__((noinline)) void completeInput(int result, UsbdIsochRequest *packet, void *arg)
{
	AudioDma *dma = arg;
	if (!dma || !dma->stream)
		return;
	AudioStream *stream = dma->stream;
	if (result == 8) {
		sceKernelSetEventFlag(stream->event, 8);
		return;
	}
	unsigned *done = (void *)packet;
	unsigned bytes = done[1] * stream->frameBytes * stream->frames;
	stream->active--;
	AudioPort *port = findPort(stream->device);
	if (!stream->opened || port->halted) {
		if (!stream->active)
			sceKernelSetEventFlag(stream->event, STREAM_STOPPED);
		return;
	}
	memcpy(stream->ring[stream->write].data + stream->ring[stream->write].used, (void *)done[0], bytes);
	stream->ring[stream->write].used += bytes;
	if (!submitIso(port->inputPipe, &dma->request, completeInput, dma)) {
		AudioRing *ring = &stream->ring[stream->write];

		stream->active++;
		if (ring->used >= ring->size) {
			ring->used = 0;
			stream->ready = stream->write;
			stream->write = (stream->write + 1) % 3;
			sceKernelSetEventFlag(stream->event, STREAM_DATA_READY);
		}
	} else {
		sceKernelSetEventFlag(stream->event, STREAM_SUBMIT_FAILED);
	}
}
int module_start(unsigned arglen, const void *argp)
{
	static const SceKernelProcEventHandler events = {
		.size = sizeof(events),
		.exit = processExit,
		.kill = processKill,
	};
	s_opened = 0;
	if (initializeAudio())
		return 2;
	int ret = sceUsbdRegisterCompositeLdd(&s_driver);
	if (ret) {
		SceDebugForDriver_391B74B7("USBAUDIO: sceUsbdRegisterCompositeLdd() failed! => 0x%x", ret);
		SceDebugForDriver_391B74B7("\n");
		debugHook();
		return 2;
	}
	if (sceKernelRegisterProcEventHandler("SceUsbAudio", &events, NULL) < 0) {
		debugHook();
		return 2;
	}
	return 0;
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
	if (s_opened >= 2) {
		ret = AUDIO_ERROR(PROCESS_CANNOT_OPEN_MORE_DEVICE);
		goto unlock;
	}
	AudioPort *port = findPort(id);
	if (!port) {
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
		goto unlock;
	}
	AudioStream *stream = port->stream;
	if (sceKernelTryLockMutex(stream->mutex, 1) < 0) {
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
	sceKernelClearEventFlag(stream->event, 0);
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
	SceUsbdForDriver_7938DAC7(port->inputPipe);
	stream->active = 0;
	for (unsigned i = 0; i < 3; i++) {
		if (submitIso(port->inputPipe, &stream->dma[i]->request, completeInput, stream->dma[i]) < 0) {
			ret = AUDIO_ERROR(FAILED_TO_REQUEST_ISOCHRONOUS);
			goto unlockPort;
		}
		stream->active++;
	}
	s_opened++;
	unsigned flags = 0, timeout = 1000000;
	ret = sceKernelWaitEventFlag(stream->event, STREAM_DATA_READY | STREAM_HALTED,
				      SCE_KERNEL_EVF_WAITMODE_OR, &flags, &timeout);
	ret = ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? AUDIO_ERROR(TIMEOUT) :
		ret ? AUDIO_ERROR(FAILED_TO_READ_STREAM) : 0;
 unlockPort:
	sceKernelUnlockMutex(stream->mutex, 1);
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
			unsigned flags = 0, timeout = 1000000;
			stream->opened = 0;
			sceKernelWaitEventFlag(stream->event, 4,
						SCE_KERNEL_EVF_WAITMODE_AND |
						SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &flags, &timeout);
			sceKernelClearEventFlag(stream->event, 0);
			stream->process = -1;
			ret = lockAudio();
			if (!ret) {
				if (s_opened)
					s_opened--;
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
	unsigned flags = 0, timeout = 64000;
	ret = sceKernelWaitEventFlag(stream->event, 10,
				      SCE_KERNEL_EVF_WAITMODE_OR |
				      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &flags, &timeout);
	if (ret) {
		ret = ret == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT ? AUDIO_ERROR(TIMEOUT) : AUDIO_ERROR(FAILED_TO_READ_STREAM);
		goto out;
	}
	if (flags & 8) {
		port->halted = AUDIO_ERROR(DEVICE_WAS_HALTED);
		sceSyscallLeave(domain);
		return port->halted;
	}
	if (flags & 16) {
		port->halted = AUDIO_ERROR(FAILED_TO_REQUEST_ISOCHRONOUS);
		sceSyscallLeave(domain);
		return port->halted;
	}
	if (stream->ready < 3) {
		AudioRing *ring = &stream->ring[stream->ready];
		ret = sceKernelCopyToUser(buffer, ring->data, ring->size) ?
			AUDIO_ERROR(FAILED_TO_COPY_BUFFER) : 0;
	} else {
		ret = AUDIO_ERROR(NO_DATA_TO_READ);
	}
 out:
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInGetDeviceInformation(SceUInt32 id, AudioInDeviceInfo *out)
{
	unsigned domain = sceSyscallEnter();
	int ret;
	AudioInDeviceInfo info;
	if (!out)
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
	else if (!findPort(id))
		ret = AUDIO_ERROR(DEVICE_NOT_FOUND);
	else {
		ret = getDeviceIds(id, &info.vendor);
		if (!ret)
			ret = sceKernelCopyToUser(out, &info, sizeof(info)) ?
				AUDIO_ERROR(FAILED_TO_COPY_BUFFER) : 0;
	}
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInGetDeviceIdList(SceUInt32 *list, SceUInt32 *count, SceUInt32 size)
{
	unsigned domain = sceSyscallEnter();
	int ret;
	unsigned ids[127];
	if (!list || !count || (int)size < 1)
		ret = AUDIO_ERROR(INVALID_ARGUMENT);
	else {
		int n = SceUsbAudioForDriver_96EC3E18(ids, 127);
		int copied = n < (int)size ? n : size;
		ret = sceKernelCopyToUser(list, ids, copied * 4);
		if (!ret)
			ret = sceKernelCopyToUser(count, &copied, 4);
	}
	sceSyscallLeave(domain);
	return ret;
}
SceInt32 sceUsbAudioInSetCurrentValueOfVolume(SceUInt32 id, SceUInt32 value)
{
	unsigned domain = sceSyscallEnter();
	int ret = owned(id);
	if (!ret && setCurrentVolume(id, value))
		ret = AUDIO_ERROR(NOT_SUPPORTED);
	sceSyscallLeave(domain);
	return ret;
}
int SceUsbAudioIn_198A7C33(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, getCurrentVolume);
}
SceInt32 sceUsbAudioInGetMaxValueOfVolume(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, getMaxVolume);
}
SceInt32 sceUsbAudioInGetMinValueOfVolume(SceUInt32 id, SceUInt32 *volume)
{
	return getVolume(id, volume, getMinVolume);
}
