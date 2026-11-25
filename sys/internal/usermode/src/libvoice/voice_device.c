/*
 * AudioIn and AudioOut hardware device wrappers and worker thread for SceVoice.
 */

#include "voice_internal.h"

int voiceAudioInIoWrapper(int port, void *buf)
{
	return sceAudioInInput(port, buf);
}

int voiceAudioOutIoWrapper(int port, void *buf)
{
	return sceAudioOutOutput(port, buf);
}

int voiceDeviceThreadEntry(SceSize args, void *argp)
{
	VoiceDeviceIoBlock *io = *(VoiceDeviceIoBlock **)argp;
	unsigned int resultPattern = 0;
	int ret = 0;
	(void)args;

	for (;;) {
		ret = sceKernelWaitEvent(io->eventUid, 0x05000000u, &resultPattern, NULL, NULL);
		if ((resultPattern & 0x04000000u) != 0) {
			break;
		}
		io->ioFunc(io->audioPort, io->pendingBuf);
		sceKernelSetEvent(io->eventUid, 0x02000000u, 0);
	}
	return ret;
}

void voiceDeviceInit(VoiceAudioInDevice *dev, uint16_t slotIndex, int32_t freq, int32_t grain)
{
	dev->slotIndex = slotIndex;
	dev->freq = freq;
	dev->grain = grain;
	dev->portId = VOICE_INVALID_SLOT;
	dev->pingPong = 0;
	voiceMemset(&dev->io, 0, sizeof(dev->io) - sizeof(dev->io.workBuf));
}

int voiceDeviceBindPort(VoiceAudioInDevice *dev, const VoicePort *port)
{
	dev->portId = port->portId;
	return 0;
}

void voiceDeviceUnbindPort(VoiceAudioInDevice *dev)
{
	dev->portId = VOICE_INVALID_SLOT;
}

int voiceDeviceStopIo(VoiceAudioInDevice *dev)
{
	int ret = 0;
	if (dev->io.isOutput == 0) {
		sceKernelSetEvent(dev->io.eventUid, 0x04000000u, 0);
		sceKernelWaitThreadEnd(dev->io.threadId, NULL, NULL);
		sceKernelDeleteThread(dev->io.threadId);
		ret = sceKernelDeleteSimpleEvent(dev->io.eventUid);
	}
	sysVoiceFreeMem(&gSysVoice, dev->io.workBuf, 1);
	dev->io.workBuf = NULL;
	return ret;
}

int voiceDeviceStartIo(VoiceAudioInDevice *dev)
{
	dev->io.workBuf = sysVoiceAllocMem(&gSysVoice, 2048, "AudioObject working buffer", 1);
	voiceMemset(dev->io.workBuf, 0xC9, 2048);

	int ret = 0;
	if (dev->io.isOutput == 0) {
		dev->io.eventUid = sceKernelCreateSimpleEvent("audio_in_evt", 256, 0x01000000, NULL);
		dev->io.threadId = sceKernelCreateThread("SceVoiceDevice",
		                                         (SceKernelThreadEntry)voiceDeviceThreadEntry,
		                                         gSysVoice.threadPriority, 0x2000, 0,
		                                         gSysVoice.threadAffinityMask, NULL);
		VoiceDeviceIoBlock *ioPtr = &dev->io;
		ret = sceKernelStartThread(dev->io.threadId, sizeof(VoiceDeviceIoBlock) - 4, &ioPtr);
	}
	return ret;
}

int voiceDeviceTransfer(VoiceAudioInDevice *dev, int audioPort, void *buf)
{
	(void)audioPort;
	if (dev->io.isOutput != 0) {
		dev->io.pendingBuf = buf;
		return dev->io.ioFunc(dev->io.audioPort, buf);
	}

	unsigned int resultPattern = 0;
	unsigned int timeout = 4000000;
	sceKernelWaitEvent(dev->io.eventUid, 0x06000000u, &resultPattern, NULL, &timeout);
	dev->io.pendingBuf = buf;
	unsigned int nextPattern = ((resultPattern & 0x04000000u) != 0) ? 0x04000000u : 0x01000000u;
	return sceKernelSetEvent(dev->io.eventUid, nextPattern, 0);
}

int voiceAudioInOpen(VoiceAudioInDevice *dev)
{
	int port = sceAudioInOpenPort(SCE_AUDIO_IN_PORT_TYPE_VOICE, dev->grain, dev->freq,
	                              SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO);
	dev->io.audioPort = port;
	if (port >= 0) {
		dev->io.isOutput = 0;
		dev->io.ioFunc = voiceAudioInIoWrapper;
		int ret = voiceDeviceStartIo(dev);
		voiceVadFilterInit(&dev->vad, (float)dev->freq);
		return ret;
	}
	return port;
}

int voiceAudioInClose(VoiceAudioInDevice *dev)
{
	if (dev->io.audioPort < 0) {
		return 0;
	}
	voiceVadFilterTerm(&dev->vad);
	voiceDeviceStopIo(dev);
	return sceAudioInReleasePort(dev->io.audioPort);
}

int voiceAudioInProcess(VoiceAudioInDevice *dev, VoicePort *port)
{
	if (voicePortCheckAudioInAdopt(port) == 0) {
		return 0;
	}

	uint8_t *slotBuf = (uint8_t *)dev->io.workBuf + (dev->pingPong << 10);
	if (voiceMemcheck(slotBuf, 0xC9, 512) != 0 &&
	    voicePortRunVad(port, (const int16_t *)slotBuf, &dev->vad) != 0) {
		int applyGain = 1;
		int16_t gainQ8 = 0;
		if (port->param.bMute == 0) {
			float vol = port->param.volume;
			if (vol >= 0.95f && vol <= 1.05f) {
				applyGain = 0;
			} else {
				gainQ8 = (int16_t)(int32_t)(vol * 256.0f);
			}
		}
		if (applyGain != 0) {
			int16_t *samples = (int16_t *)slotBuf;
			for (int i = 0; i < dev->grain; i++) {
				int32_t scaled = ((int32_t)samples[i] * (int32_t)gainQ8) >> 8;
				if (scaled < -32768) {
					scaled = -32768;
				}
				if (scaled > 32767) {
					scaled = 32767;
				}
				samples[i] = (int16_t)scaled;
			}
		}
		voiceRingWrite(&port->internalRing, slotBuf, 512);
	}

	voiceMemset(slotBuf, 0xC9, 512);
	int ret = voiceDeviceTransfer(dev, dev->io.audioPort, slotBuf);
	dev->pingPong ^= 1u;
	return ret;
}

int voiceAudioOutOpen(VoiceAudioOutDevice *dev)
{
	int port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_VOICE, dev->grain, dev->freq,
	                               SCE_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);
	dev->io.audioPort = port;
	if (port >= 0) {
		dev->io.ioFunc = voiceAudioOutIoWrapper;
		dev->io.isOutput = 1;
		dev->stereoBuf = sysVoiceAllocMem(&gSysVoice, 4096, "AudioOut working buffer", 1);
		return voiceDeviceStartIo((VoiceAudioInDevice *)dev);
	}
	return port;
}

int voiceAudioOutClose(VoiceAudioOutDevice *dev)
{
	if (dev->io.audioPort < 0) {
		return 0;
	}
	voiceDeviceStopIo((VoiceAudioInDevice *)dev);
	dev->io.ioFunc(dev->io.audioPort, NULL);
	int ret = sceAudioOutReleasePort(dev->io.audioPort);
	sysVoiceFreeMem(&gSysVoice, dev->stereoBuf, 1);
	dev->stereoBuf = NULL;
	return ret;
}

int voiceAudioOutProcess(VoiceAudioOutDevice *dev, VoicePort *port)
{
	uint32_t got = voiceRingRead(&port->internalRing, dev->io.workBuf, 512);
	int16_t *stereoOut = (int16_t *)((uint8_t *)dev->stereoBuf + (dev->pingPong << 11));

	if (got < 512u) {
		voiceMemset(stereoOut, 0, 1024);
	} else {
		const int16_t *monoIn = (const int16_t *)dev->io.workBuf;
		for (int i = 0; i < 256; i++) {
			int16_t sample = monoIn[i];
			stereoOut[2 * i] = sample;
			stereoOut[2 * i + 1] = sample;
		}
	}

	int ret = voiceDeviceTransfer((VoiceAudioInDevice *)dev, dev->io.audioPort, stereoOut);
	dev->pingPong ^= 1u;
	return ret;
}
