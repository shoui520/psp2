/* Audio module implementation. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <arm_neon.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr.h>
#include <processmgr/processmgr.h>
#include <sysmem/uid_class.h>
#include <sysmem/debug.h>
#include <sysmem/procevent.h>
#include <acmgr/acmgr.h>
#include <audioout.h>

#include "audio_private.h"
#include "audio_work.h"
#include <sysmem/sysevent.h>
#include <sysmem/sysmem.h>
#include "alc.h"
#include "equalizer.h"
#include <sysmem/sysroot.h>
#include <clockgen/clockgen.h>
#include <audio/audio.h>
#include <codec/codec.h>

int sceAudioOutSetEffectType(int type);
int audioInitializeEffects(void);
int audioSetVolumeRamp(AudioVolumeRamp *ramp, int freq, int adopt, int duration);
int audioSetVolumeRampTarget(AudioVolumeRamp *ramp, int freq, int adopt,
			     int volume, int duration);
int audioProcessDirectPort(AudioDirectPort *port, int16_t *output,
			   const int16_t *input, unsigned int sampleCount);
int audioInitializeProcess(SceUID pid, ProcEventCreateParam *param, void *common);
int audioProcessExitCallback(SceUID pid, ProcEventParam *param, void *common);
int audioCleanupProcess(SceUID pid, ProcEventParam *param, void *common);
int audioProcessEventHandler(SceUID pid, int event, ProcEventParam *param,
			    void *common);
int audioProcessTitleEvent(SceUID pid, int event, ProcEventParam *param,
			  void *common);

const SceKernelProcEventHandler audioProcessEventHandlers = {
	.size = sizeof(SceKernelProcEventHandler),
	.create = audioInitializeProcess,
	.exit = audioProcessExitCallback,
	.kill = audioCleanupProcess,
	.suspend = audioProcessEventHandler,
	.resume = audioProcessTitleEvent,
	.notify = NULL,
};

int audioWaitForDrain(int samples)
{
	int result = (int32_t)((uint32_t)samples + 1024);
	int waits;

	for (waits = result / 480;
	     waits != 0 && audioSuspended == 0 && audioMixerIdle == 0;
	     --waits)
		result = sceKernelDelayThread(10000);
	return result;
}

int audioSetTitleCompatibility(SceUID pid, signed char enabled)
{
	char titleId[32];
	int result = SceSysrootForKernel_EC3124A3(pid, titleId, sizeof(titleId));

	if (result >= 0) {
		if (strncmp(titleId, "PCSG00135", 32) == 0 ||
		    strncmp(titleId, "PCSE00097", 32) == 0 ||
		    strncmp(titleId, "PCSB00232", 32) == 0 ||
		    strncmp(titleId, "PCSB00213", 32) == 0 ||
		    strncmp(titleId, "PCSF00140", 32) == 0 ||
		    strncmp(titleId, "PCSF00141", 32) == 0 ||
		    strncmp(titleId, "PCSF00003", 32) == 0 ||
		    strncmp(titleId, "PCSC00017", 32) == 0)
			audioLegacyTitleCompatibility = enabled;
		result = strncmp(titleId, "PCSG00122", 32);
		if (result == 0)
			audioInvalidPortDelayCompatibility = enabled;
	}
	return result;
}

int audioSystemEventHandler(int phase, int event, void *args, void *opt)
{
	(void)args;
	(void)opt;
	if (phase == 0) {
		switch (event) {
		case 4096:
			audioShutdownHardware();
			break;
		case 16399:
			audioPrepareSuspend();
			break;
		case 258:
			audioSetSuspended();
			break;
		default:
			break;
		}
	} else if (phase == 1 && event == 65551) {
		audioResumeHardware();
		SceAudioForDriver_28B3AF10(0, 48000, audioCaptureRate, 48000);
		SceAudioForDriver_28B3AF10(phase, audioPlaybackRate, 48000, 48000);
		return 0;
	}
	return 0;
}

int audioCleanupProcess(SceUID pid, ProcEventParam *param, void *common)
{
	AudioProcessLocalStorage *storage;
	AudioMainPort *port;
	int count;

	if (sceKernelGetRemoteKPLS(pid, audioStorageKey,
						   (void **)&storage, 1) >= 0) {
		AudioProcessLocalStorage *process = storage;

		sceKernelWaitEventFlag(audioEventFlag, 48, 4, NULL, NULL);
		if (audioProcessList != NULL && process->link.prev != NULL) {
			ListNode *previous = process->link.prev;
			ListNode *next = process->link.next;

			if (&process->link == previous) {
				audioProcessList = (AudioProcessLocalStorage *)next;
				if (next != NULL)
					next->prev = next;
			} else {
				previous->next = next;
				if (process->link.next != NULL)
					process->link.next->prev = previous;
			}
			process->link.next = NULL;
			process->link.prev = NULL;
		}
		sceKernelSetEventFlag(audioEventFlag, 48);
		if (process->eventFlag > 0)
			sceKernelDeleteEventFlag(process->eventFlag);
		port = process->main;
		for (count = 9; count != 0; --count, ++port) {
			if (port->current.mapping > 0)
				sceKernelUserUnmap(port->current.mapping, NULL, 0);
			if (port->queued.mapping > 0)
				sceKernelUserUnmap(port->queued.mapping, NULL, 0);
		}
		audioSetTitleCompatibility(pid, count);
	}
	return 0;
}

int audioProcessExitCallback(SceUID pid, ProcEventParam *param, void *common)
{
	return audioCleanupProcess(pid, NULL, common);
}

intptr_t audioUpdateAdoption(AudioDirectPort *port)
{
	int channel = port->channel;
	AudioDirectPort *adopted = audioAdoptedPorts[channel];
	int dmaActive;

	if ((port->adoptState & 4) == 0)
		return (intptr_t)port;
	if (channel == 0)
		dmaActive = audioDmaStates[1].transfers[0].state > 0;
	else
		dmaActive = audioDmaStates[0].transfers[1].state > 0;
	if ((port->adoptState & 2) != 0) {
		if (audioAdoptingPorts[channel] == port) {
			port->adoptState = 1;
			audioAdoptingPorts[channel] = NULL;
		}
		if (adopted == NULL ||
		    (adopted == port && (!dmaActive || port->ramp.direction == 0))) {
			port->adoptState = 1;
			audioAdoptedPorts[channel] = NULL;
			return SceCodecForDriver_CCDE1B04(512);
		}
		return (intptr_t)port;
	}
	if (adopted != port && adopted != NULL) {
		if (dmaActive && (adopted->ramp.direction != 0 || adopted->remaining != 0))
			return (intptr_t)port;
		adopted->adoptState = 1;
		audioAdoptedPorts[channel] = NULL;
	}
	adopted = audioAdoptingPorts[channel];
	if (adopted != NULL) {
		audioAdoptingPorts[channel] = NULL;
		audioAdoptedPorts[channel] = adopted;
	}
	if (port->ramp.direction != 0) {
		port->adoptState = 4;
		return (intptr_t)port;
	}
	port->adoptState = 0;
	adopted = audioAdoptedPorts[channel];
	if (adopted != NULL) {
		if (adopted != port)
			return (intptr_t)port;
		if (port->len != 0 && port->compress != 0)
			return SceCodecForDriver_CCDE1B04(256);
	}
	return SceCodecForDriver_CCDE1B04(512);
}

int audioSetAdoption(AudioDirectPort *port, int adopt, int duration)
{
	int channel = port->channel;
	AudioDirectPort *other;
	unsigned int remaining;

	if ((port->adoptState & 4) == 0 && (int8_t)port->adoptState != adopt)
		return 1;
	audioSetVolumeRamp(&port->ramp, port->freq, adopt, duration);
	port->adoptState &= 1;
	if (adopt != 0) {
		other = audioAdoptingPorts[channel];
		if (other != NULL)
			other->adoptState = 1;
		other = audioAdoptedPorts[channel];
		audioAdoptingPorts[channel] = port;
		if (other != port && other != NULL) {
			if ((other->adoptState & 6) != 6)
				audioSetVolumeRamp(&other->ramp, other->freq, 0, 10);
			other->adoptState = 6;
		}
	} else {
		port->adoptState |= 2;
		if (audioAdoptingPorts[channel] == port)
			audioAdoptingPorts[channel] = NULL;
	}
	other = audioAdoptedPorts[port->channel];
	if (other == NULL) {
		SceCodecForDriver_CCDE1B04(512);
	} else if (other == port) {
		if (port->len != 0 && port->compress != 0)
			SceCodecForDriver_CCDE1B04(256);
		else
			SceCodecForDriver_CCDE1B04(512);
	}
	remaining = port->remaining;
	port->adoptState |= 4;
	if (remaining == 0)
		audioUpdateAdoption(port);
	return 0;
}

int audioProcessTitleEvent(SceUID pid, int event, ProcEventParam *param,
			  void *common)
{
	char titleId[32];

	if (event == 131073 &&
	    SceSysrootForKernel_EC3124A3(pid, titleId, sizeof(titleId)) >= 0) {
		if (strncmp(titleId, "PCSG00135", 32) == 0 ||
		    strncmp(titleId, "PCSE00097", 32) == 0 ||
		    strncmp(titleId, "PCSB00232", 32) == 0 ||
		    strncmp(titleId, "PCSB00213", 32) == 0 ||
		    strncmp(titleId, "PCSF00140", 32) == 0 ||
		    strncmp(titleId, "PCSF00141", 32) == 0 ||
		    strncmp(titleId, "PCSF00003", 32) == 0 ||
		    strncmp(titleId, "PCSC00017", 32) == 0)
			audioLegacyTitleCompatibility = 1;
		if (strncmp(titleId, "PCSG00122", 32) == 0)
			audioInvalidPortDelayCompatibility = 1;
	}
	return 0;
}

int audioInitializeProcess(SceUID pid, ProcEventCreateParam *param, void *common)
{
	AudioProcessLocalStorage *storage;
	char titleId[32];

	if (sceKernelGetRemoteKPLS(pid, audioStorageKey,
						   (void **)&storage, 1) >= 0) {
		memset(storage, 0, sizeof(*storage));
		storage->direct[0].eventMask = 64;
		storage->direct[1].eventMask = 128;
		storage->adopt = 1;
		storage->direct[1].adoptState = 1;
		storage->direct[0].adoptState = 1;
		storage->direct[1].channel = 1;
		storage->ramp.volume = 256;
		storage->direct[1].ramp.volume = 256;
		storage->direct[0].ramp.volume = 256;
		storage->direct[0].channel = 0;
		storage->direct[0].alcMode = 0;
		if (SceSysrootForKernel_EC3124A3(pid, titleId, sizeof(titleId)) >= 0) {
			if (strncmp(titleId, "PCSG00135", 32) == 0 ||
			    strncmp(titleId, "PCSE00097", 32) == 0 ||
			    strncmp(titleId, "PCSB00232", 32) == 0 ||
			    strncmp(titleId, "PCSB00213", 32) == 0 ||
			    strncmp(titleId, "PCSF00140", 32) == 0 ||
			    strncmp(titleId, "PCSF00141", 32) == 0 ||
			    strncmp(titleId, "PCSF00003", 32) == 0 ||
			    strncmp(titleId, "PCSC00017", 32) == 0)
				audioLegacyTitleCompatibility = 1;
			if (strncmp(titleId, "PCSG00122", 32) == 0)
				audioInvalidPortDelayCompatibility = 1;
		}
	}
	return 0;
}

int audioProcessEventHandler(SceUID pid, int event, ProcEventParam *param,
			    void *common)
{
	AudioProcessLocalStorage *storage;
	AudioProcessLocalStorage *process;
	AudioDirectPort *port;
	AudioDirectPort *adopted;
	int result;
	int channel;
	unsigned int remaining;

	if (sceKernelGetRemoteKPLS(pid, audioStorageKey,
						   (void **)&storage, 1) < 0)
		return 0;
	switch (event) {
	case 513:
	case 256:
		break;
	case 514:
		audioSetTitleCompatibility(pid, 0);
		if ((storage->direct[0].adoptState & 1) == 0)
			return -1;
		return (storage->direct[1].adoptState & 1) != 0 ? 0 : -2;
	default:
		return 0;
	}
	process = storage;
	result = sceKernelWaitEventFlag(audioEventFlag, 240, 4, NULL, NULL);
	if (result < 0)
		return result;
	process->adopt = 0;
	port = &process->direct[0];
	if ((port->adoptState & 1) != 0 || (port->adoptState & 6) == 6 ||
	    port->ramp.step < 0) {
		if (port->remaining == 0)
			goto updateBgm;
		goto voice;
	}
	channel = port->channel;
	if ((port->adoptState & 4) == 0 && port->adoptState != 0)
		goto voice;
	audioSetVolumeRamp(&port->ramp, port->freq, 0, 1);
	port->adoptState = (port->adoptState & 1) | 2;
	if (audioAdoptingPorts[channel] == port)
		audioAdoptingPorts[channel] = NULL;
	adopted = audioAdoptedPorts[port->channel];
	if (adopted == NULL)
		SceCodecForDriver_CCDE1B04(512);
	else if (port == adopted) {
		if (port->len != 0 && port->compress != 0)
			SceCodecForDriver_CCDE1B04(256);
		else
			SceCodecForDriver_CCDE1B04(512);
	}
	remaining = port->remaining;
	port->adoptState |= 4;
	if (remaining != 0)
		goto voice;
updateBgm:
	audioUpdateAdoption(port);
voice:
	port = &process->direct[1];
	if ((port->adoptState & 1) != 0 || (port->adoptState & 6) == 6 ||
	    port->ramp.step < 0) {
		if (port->remaining != 0)
			goto done;
		goto updateVoice;
	}
	channel = port->channel;
	if ((port->adoptState & 4) == 0 && port->adoptState != 0)
		goto done;
	audioSetVolumeRamp(&port->ramp, port->freq, 0, 1);
	port->adoptState = (port->adoptState & 1) | 2;
	if (audioAdoptingPorts[channel] == port)
		audioAdoptingPorts[channel] = NULL;
	adopted = audioAdoptedPorts[port->channel];
	if (adopted == NULL)
		SceCodecForDriver_CCDE1B04(512);
	else if (port == adopted) {
		if (port->len != 0 && port->compress != 0)
			SceCodecForDriver_CCDE1B04(256);
		else
			SceCodecForDriver_CCDE1B04(512);
	}
	remaining = port->remaining;
	port->adoptState |= 4;
	if (remaining != 0)
		goto done;
updateVoice:
	audioUpdateAdoption(port);
done:
	sceKernelSetEventFlag(audioEventFlag, 240);
	return 0;
}

int sceAudioOutSetCompress(int port, unsigned int mode)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int index = (unsigned int)(port >> 8) - 1;
	AudioProcessLocalStorage *storage;
	AudioDirectPort *direct;
	AudioDirectPort *adopted;
	int result;

	if (index > 1) {
		result = (int)0xc026000a;
		goto out;
	}
	if (mode > 1) {
		result = (int)0x80260220;
		goto out;
	}
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	direct = &storage->direct[index];
	if (direct->len == 0) {
		result = (int)0xc0260001;
		goto unlock;
	}
	sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
	direct->compress = mode;
	adopted = audioAdoptedPorts[direct->channel];
	if (adopted == NULL) {
		SceCodecForDriver_CCDE1B04(512);
	} else if (direct == adopted) {
		if (direct->len != 0 && mode != 0)
			SceCodecForDriver_CCDE1B04(256);
		else
			SceCodecForDriver_CCDE1B04(512);
	}
	sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int SceAudioForDriver_3440C1CA(SceUID pid, int type, int adopt,
			    int duration, int wait)
{
	AudioProcessLocalStorage *storage;
	AudioProcessLocalStorage *process;
	AudioDirectPort *port;
	int result;
	int samples;
	int started;
	int tries;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		return result;
	result = sceGUIDReferObject(pid, NULL);
	if (result < 0)
		goto unlock;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							(void **)&storage, 1);
	if (result < 0)
		goto release;
	if (type != 0) {
		if (type < 0 || type > 2) {
			result = (int)0xc026000a;
			goto release;
		}
		port = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
		if (audioSetAdoption(port, adopt, duration) != 0) {
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
			goto release;
		}
		sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		if (adopt != 0 || wait == 0)
			goto release;
		started = 0;
		tries = 421;
		sceKernelSetEventFlag(audioEventFlag, 16);
		do {
			if (port->len == 0)
				break;
			for (;;) {
				if (audioSuspended != 0)
					goto waited;
				if (started != 0)
					break;
				if ((port->adoptState & 1) == 0)
					goto delay;
				tries = 420;
				sceKernelDelayThread(10000);
				started = 1;
				if (port->len == 0)
					goto waited;
			}
			if (port->remaining != 0)
				break;
			if (port->channel != 0 ? audioDmaStates[0].transfers[1].state <= 0 : audioDmaStates[1].transfers[0].state <= 0)
				break;
			started = 1;
delay:
			sceKernelDelayThread(10000);
		} while (--tries > 0);
	} else {
		process = storage;
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if ((int8_t)process->adopt == adopt ||
		    (adopt == 0 && audioMixerIdle != 0) ||
		    (process->main[8].current.cursor == NULL &&
		     process->main[7].current.cursor == NULL &&
		     process->main[6].current.cursor == NULL &&
		     process->main[5].current.cursor == NULL &&
		     process->main[4].current.cursor == NULL &&
		     process->main[3].current.cursor == NULL &&
		     process->main[2].current.cursor == NULL &&
		     process->main[1].current.cursor == NULL &&
		     process->main[0].current.cursor == NULL)) {
			process->adopt = adopt;
			sceKernelSetEventFlag(audioEventFlag, 32);
			goto release;
		}
		samples = audioSetVolumeRamp(&process->ramp, 48000, adopt, duration);
		process->adopt = adopt;
		sceKernelSetEventFlag(audioEventFlag, 32);
		if (adopt != 0 || wait == 0 || process->ramp.direction != -1)
			goto release;
		sceKernelSetEventFlag(audioEventFlag, 16);
		audioWaitForDrain(samples);
	}
waited:
	sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
release:
	sceGUIDReleaseObject(pid);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
	return result;
}

int sceAudioOutSetAdopt_forUser(int type, int adopt, int duration, int wait)
{
	unsigned int syscall = sceSyscallEnter();
	SceUID pid = sceKernelGetProcessId();
	AudioProcessLocalStorage *storage;
	AudioProcessLocalStorage *process;
	AudioDirectPort *port;
	int result;
	int samples;
	int started;
	int tries;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	result = sceGUIDReferObject(pid, NULL);
	if (result < 0)
		goto unlock;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							(void **)&storage, 1);
	if (result < 0)
		goto release;
	if (type != 0) {
		if (type < 0 || type > 2) {
			result = (int)0xc026000a;
			goto release;
		}
		port = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
		if (audioSetAdoption(port, adopt, duration) != 0) {
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
			goto release;
		}
		sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		if (adopt != 0 || wait == 0)
			goto release;
		started = 0;
		tries = 421;
		sceKernelSetEventFlag(audioEventFlag, 16);
		do {
			if (port->len == 0)
				break;
			for (;;) {
				if (audioSuspended != 0)
					goto waited;
				if (started != 0)
					break;
				if ((port->adoptState & 1) == 0)
					goto delay;
				tries = 420;
				sceKernelDelayThread(10000);
				started = 1;
				if (port->len == 0)
					goto waited;
			}
			if (port->remaining != 0)
				break;
			if (port->channel != 0 ? audioDmaStates[0].transfers[1].state <= 0 : audioDmaStates[1].transfers[0].state <= 0)
				break;
			started = 1;
delay:
			sceKernelDelayThread(10000);
		} while (--tries > 0);
	} else {
		process = storage;
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if ((int8_t)process->adopt == adopt ||
		    (adopt == 0 && audioMixerIdle != 0) ||
		    (process->main[8].current.cursor == NULL &&
		     process->main[7].current.cursor == NULL &&
		     process->main[6].current.cursor == NULL &&
		     process->main[5].current.cursor == NULL &&
		     process->main[4].current.cursor == NULL &&
		     process->main[3].current.cursor == NULL &&
		     process->main[2].current.cursor == NULL &&
		     process->main[1].current.cursor == NULL &&
		     process->main[0].current.cursor == NULL)) {
			process->adopt = adopt;
			sceKernelSetEventFlag(audioEventFlag, 32);
			goto release;
		}
		samples = audioSetVolumeRamp(&process->ramp, 48000, adopt, duration);
		process->adopt = adopt;
		sceKernelSetEventFlag(audioEventFlag, 32);
		if (adopt != 0 || wait == 0 || process->ramp.direction != -1)
			goto release;
		sceKernelSetEventFlag(audioEventFlag, 16);
		audioWaitForDrain(samples);
	}
waited:
	sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
release:
	sceGUIDReleaseObject(pid);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutOpenPort(int type, int len, int freq, int mode)
{
	unsigned int syscall;
	AudioProcessLocalStorage *storage;
	AudioMainPort *main;
	AudioDirectPort *direct;
	AudioDirectPort *adopted;
	void (*callback)(SceUID pid);
	unsigned char compress;
	unsigned char bytesPerFrame;
	unsigned char sampleMode = mode;
	int index;
	int result;

	if (((unsigned int)len << 26) != 0 || (unsigned int)len - 1 > 0xffbf)
		return SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
	syscall = sceSyscallEnter();
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	switch (type) {
	case SCE_AUDIO_OUT_PORT_TYPE_MAIN:
		if (freq != 48000) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
			goto unlock;
		}
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		main = &storage->main[7];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 7;
			goto selectedMain;
		}
		main = &storage->main[6];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 6;
			goto selectedMain;
		}
		main = &storage->main[5];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 5;
			goto selectedMain;
		}
		main = &storage->main[4];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 4;
			goto selectedMain;
		}
		main = &storage->main[3];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 3;
			goto selectedMain;
		}
		main = &storage->main[2];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 2;
			goto selectedMain;
		}
		main = &storage->main[1];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 1;
			goto selectedMain;
		}
		main = &storage->main[0];
		if (main->config.len == 0 && main->current.cursor == NULL) {
			index = 0;
			goto selectedMain;
		}
		result = SCE_AUDIO_OUT_ERROR_PORT_FULL;
		goto unlockMain;
selectedMain:
		if (sampleMode != 0) {
			if (sampleMode != 1) {
				result = SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
				goto unlockMain;
			}
			bytesPerFrame = 4;
		} else {
			bytesPerFrame = 2;
		}
		main->config.bytesPerFrame = bytesPerFrame;
		storage->ramp.direction = 0;
		main->current.mapping = 0;
		main->current.cursor = NULL;
		main->config.len = len;
		main->volume[1] = 0x8000;
		main->volume[0] = 0x8000;
		result = index;
		goto unlockMain;
	case SCE_AUDIO_OUT_PORT_TYPE_BGM:
		index = 0;
		callback = NULL;
		compress = 0;
		break;
	case SCE_AUDIO_OUT_PORT_TYPE_VOICE:
		index = 1;
		callback = audioPortOpenCallback;
		compress = 0;
		break;
	case 3:
		if (SceSblACMgrForDriver_6C5AB07F(0) == 0) {
			result = (int)0xc026000a;
			goto unlock;
		}
		index = 1;
		callback = audioPortOpenCallback;
		compress = 1;
		break;
	default:
		result = (int)0xc026000a;
		goto unlock;
	}
	direct = &storage->direct[index];
	sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
	if (freq != 48000 && freq != 44100 && freq != 32000 && freq != 24000 &&
	    freq != 22050 && freq != 16000 && freq != 12000 && freq != 11025 && freq != 8000) {
		if (SceSblACMgrForDriver_FD00C72A(0) != 1 || freq != 44184) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
			goto unlockDirect;
		}
	}
	if (sampleMode != 0) {
		if (sampleMode != 1) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
			goto unlockDirect;
		}
		bytesPerFrame = 4;
	} else {
		bytesPerFrame = 2;
	}
	if (direct->len != 0) {
		result = SCE_AUDIO_OUT_ERROR_PORT_FULL;
		goto unlockDirect;
	}
	direct->ramp.direction = 0;
	direct->bytesPerFrame = bytesPerFrame;
	direct->equalizer = 0x03333333;
	if (audioAdoptMode == 0 && (direct->adoptState & 4) == 0)
		audioSetAdoption(direct, 1, 10);
	direct->freq = freq;
	direct->volume[1] = 0x8000;
	direct->volume[0] = 0x8000;
	direct->len = len;
	direct->deadline = sceKernelGetProcessTimeLowCore();
	direct->compress = compress;
	adopted = audioAdoptedPorts[direct->channel];
	if (adopted == NULL) {
		SceCodecForDriver_CCDE1B04(512);
	} else if (direct == adopted) {
		if (direct->len != 0 && compress != 0)
			SceCodecForDriver_CCDE1B04(256);
		else
			SceCodecForDriver_CCDE1B04(512);
	}
	result = (int)((unsigned int)(direct->channel + 1) << 8);
	sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	if (result >= 0) {
		sceKernelSetEventFlag(audioEventFlag, 16);
		if (callback != NULL)
			callback(sceKernelGetProcessId());
		goto out;
	}
	goto unlock;
unlockDirect:
	sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	goto unlock;
unlockMain:
	sceKernelSetEventFlag(audioEventFlag, 32);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutOpenExtPort(int len, int mode)
{
	unsigned int syscall;
	AudioProcessLocalStorage *storage;
	AudioMainPort *port;
	unsigned char sampleMode = mode;
	unsigned char bytesPerFrame;
	int result;

	if (((unsigned int)len << 26) != 0 || (unsigned int)len - 1 > 0xffbf)
		return SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
	syscall = sceSyscallEnter();
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
	port = &storage->main[8];
	if (port->config.len != 0 || port->current.cursor != NULL) {
		result = SCE_AUDIO_OUT_ERROR_PORT_FULL;
		goto unlockMain;
	}
	if (sampleMode != 0) {
		if (sampleMode != 1) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
			goto unlockMain;
		}
		bytesPerFrame = 4;
	} else {
		bytesPerFrame = 2;
	}
	port->config.bytesPerFrame = bytesPerFrame;
	port->config.len = len;
	storage->ramp.direction = 0;
	port->current.mapping = 0;
	port->current.cursor = NULL;
	port->volume[1] = 0x8000;
	port->volume[0] = 0x8000;
	result = 8;
unlockMain:
	sceKernelSetEventFlag(audioEventFlag, 32);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutReleasePort(int port)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int type = ((unsigned int)port >> 8) & 0xff;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioMainPort *main;
	AudioDirectPort *direct;
	AudioDirectPort *adopted;
	void (*callback)(SceUID pid) = NULL;
	SceUInt32 sdkVersion;
	int result;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	if (type == 1 || type == 2) {
		if (type == 2)
			callback = audioPortCloseCallback;
		direct = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
		if (direct->len == 0) {
			result = (int)0xc0260001;
			goto unlockDirect;
		}
		if (direct->remaining != 0) {
			result = SCE_AUDIO_OUT_ERROR_BUSY;
			goto unlockDirect;
		}
		index = direct->channel;
		direct->compress = 0;
		direct->len = 0;
		adopted = audioAdoptedPorts[(int)index];
		if (adopted == NULL || adopted == direct)
			SceCodecForDriver_CCDE1B04(512);
		sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
		sceKernelSetEventFlag(audioEventFlag, 16);
		result = 0;
		if (callback != NULL)
			callback(sceKernelGetProcessId());
		goto out;
	}
	if (type != 0) {
		result = (int)0xc026000a;
		goto unlock;
	}
	if (index > 8) {
		result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
		goto unlock;
	}
	main = &storage->main[index];
	sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
	if (main->config.len == 0) {
		result = (int)0xc0260001;
		goto unlockMain;
	}
	if (main->current.mapping <= 0) {
		if (main->queued.mapping > 0) {
			result = SCE_AUDIO_OUT_ERROR_BUSY;
			goto unlockMain;
		}
	} else {
		sdkVersion = 0;
		if (main->queued.mapping > 0 ||
		    (SceProcessmgrForDriver_D141C076(0, &sdkVersion) >= 0 &&
		     sdkVersion >= 0x2000000)) {
			result = SCE_AUDIO_OUT_ERROR_BUSY;
			goto unlockMain;
		}
		while (main->current.mapping > 0 || main->queued.mapping > 0) {
			sceKernelSetEventFlag(audioEventFlag, 32);
			sceKernelDelayThread(1000);
			sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		}
	}
	result = 0;
	main->config.len = 0;
	if (storage->main[8].config.len != 0 || storage->main[8].current.cursor != NULL ||
	    storage->main[7].config.len != 0 || storage->main[7].current.cursor != NULL ||
	    storage->main[6].config.len != 0 || storage->main[6].current.cursor != NULL ||
	    storage->main[5].config.len != 0 || storage->main[5].current.cursor != NULL ||
	    storage->main[4].config.len != 0 || storage->main[4].current.cursor != NULL ||
	    storage->main[3].config.len != 0 || storage->main[3].current.cursor != NULL ||
	    storage->main[2].config.len != 0 || storage->main[2].current.cursor != NULL ||
	    storage->main[1].config.len != 0 || storage->main[1].current.cursor != NULL ||
	    storage->main[0].config.len != 0 || storage->main[0].current.cursor != NULL)
		goto unlockMain;
	if (audioProcessList != NULL && storage->link.prev != NULL) {
		ListNode *previous = storage->link.prev;
		ListNode *next = storage->link.next;

		if (&storage->link == previous) {
			audioProcessList = (AudioProcessLocalStorage *)next;
			if (next != NULL)
				next->prev = next;
		} else {
			previous->next = next;
			if (storage->link.next != NULL)
				storage->link.next->prev = previous;
		}
		storage->link.next = NULL;
		storage->link.prev = NULL;
	}
unlockMain:
	sceKernelSetEventFlag(audioEventFlag, 32);
	goto unlock;
unlockDirect:
	sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int _audioSetptrMain(int port, const void *buf)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int type = ((unsigned int)port >> 8) & 255;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioMainPort *main;
	AudioDirectPort *direct;
	void *kernelPage;
	SceSize kernelSize;
	SceUInt32 kernelOffset;
	char eventName[32];
	SceUID mapping;
	int result;
	int mask;
	int channel;
	int bytesPerFrame;
	unsigned int freq;
	unsigned int remaining;
	const void *userBuffer = buf;
	const char *mapped;
	uint16_t *rate;
	int transferResult;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	sceKernelSetEventFlag(audioEventFlag, 16);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto out;
	}
	if (type == 0) {
		if (index > 8) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
			goto out;
		}
		main = &storage->main[index];
		result = (int)0xc0260001;
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if (main->config.len == 0)
			goto unlockMain;
		result = SCE_AUDIO_OUT_ERROR_BUSY;
		if (main->queued.mapping != 0)
			goto unlockMain;
		if (storage->link.prev == NULL) {
			if (storage->eventFlag <= 0) {
				snprintf(eventName, sizeof(eventName), "SceAudioMixProc%08x",
					 sceKernelGetProcessId());
				result = sceKernelCreateEventFlag(eventName, 0x1000, 0, NULL);
				if (result < 0)
					goto unlockMain;
				storage->eventFlag = result;
			}
			storage->link.next = (ListNode *)audioProcessList;
			storage->link.prev = &storage->link;
			if (audioProcessList != NULL)
				audioProcessList->link.prev = &storage->link;
			audioProcessList = storage;
		}
		SCE_DBG_ASSERT(main->queued.mapping == 0, 88);
		if (buf != NULL) {
			int len;

			result = sceKernelUserMap("SceAudioUserMapMain", 1, buf,
				main->config.bytesPerFrame * main->config.len,
				&kernelPage, &kernelSize, &kernelOffset);
			if (result < 0)
				goto unlockMain;
			bytesPerFrame = main->config.bytesPerFrame;
			main->queued.mapping = result;
			len = main->config.len;
			main->queued.bytesPerFrame = bytesPerFrame;
			main->queued.remaining = len;
			main->queued.cursor = (int16_t *)((char *)kernelPage + kernelOffset);
			if (audioMixerIdle != 0) {
				sceKernelSetEventFlag(audioEventFlag, 1);
				audioMixerIdle = 0;
				mapping = main->queued.mapping;
				result = main->config.len;
			} else {
				mapping = result;
				result = len;
			}
		} else {
			if (main->current.cursor != NULL) {
				mapping = -1;
				main->queued.mapping = -1;
				main->queued.cursor = NULL;
			} else {
				mapping = main->queued.mapping;
			}
			result = 0;
		}
		if (mapping != 0) {
			int samples = result;
			unsigned int bits = 1U << index;

			for (;;) {
				int waitResult;

				sceKernelSetEventFlag(audioEventFlag, 32);
				waitResult = sceKernelWaitEventFlag(storage->eventFlag,
					bits, 4, NULL, NULL);
				sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
				if (waitResult < 0) {
					result = waitResult;
					break;
				}
				if (main->queued.mapping == 0) {
					result = samples;
					break;
				}
			}
		}
		goto unlockMain;
	}
	if (type > 2) {
		result = (int)0xc026000a;
		if (audioInvalidPortDelayCompatibility != 0)
			sceKernelDelayThread(100);
		goto out;
	}
	direct = &storage->direct[type - 1];
	mask = direct->eventMask;
	channel = direct->channel;
	result = (int)0xc0260001;
	if (direct->len == 0)
		goto out;
	result = SCE_AUDIO_OUT_ERROR_BUSY;
	if (direct->remaining != 0)
		goto out;
	sceKernelWaitEventFlag(audioEventFlag, mask, 5, NULL, NULL);
	bytesPerFrame = direct->bytesPerFrame;
	freq = direct->freq;
	if (buf != NULL) {
		result = sceKernelUserMap("SceAudioUserMapBgm", 1, buf,
			bytesPerFrame * direct->len, &kernelPage, &kernelSize,
			&kernelOffset);
		if (result < 0)
			goto unlockDirect;
		remaining = direct->len;
		direct->remaining = remaining;
		mapped = (const char *)kernelPage + kernelOffset;
	} else {
		result = 0;
		remaining = 0;
		mapped = NULL;
	}
	mapping = result;
	rate = channel == 0 ? &audioPlaybackRate : &audioCaptureRate;
	do {
		int direction = (unsigned char)direct->ramp.direction;
		int target = direct->ramp.target;
		int samples = remaining & 1023;
		int current = direct->ramp.current;
		int step = direct->ramp.step;

		if (samples == 0)
			samples = remaining >= 1024 ? 1024 : remaining;
		if ((direct->adoptState & 4) != 0) {
			if (audioSuspended != 0) {
				direction = 0;
				samples = 64;
				direct->ramp.direction = 0;
			} else if (step != 0) {
				int rampSamples = (int32_t)((uint32_t)target -
					(uint32_t)current) / step;

				if (rampSamples < 0 || (rampSamples != 0 && rampSamples < samples))
					samples = ((uint32_t)rampSamples + 63) & ~63U;
			}
		}
		audioUpdateAdoption(direct);
		if (mapped == NULL) {
			sceKernelSetEventFlag(audioEventFlag, mask);
			if ((direct->adoptState & 1) != 0) {
				result = 0;
				direct->deadline = audioWaitForPlaybackDeadline(direct->deadline,
					freq, 0);
			} else {
				result = audioDispatchDmaBuffer(0, channel + 1, 0);
				direct->deadline = sceKernelGetProcessTimeLowCore();
			}
			goto out;
		}
		if ((direct->adoptState & 1) == 0) {
			int volume;
			int leftVolume;
			int rightVolume;
			int16_t *output;
			const int16_t *input;

			if (freq != *rate) {
				*rate = freq;
				transferResult = channel != 0 ?
					SceAudioForDriver_28B3AF10(0, -1, freq, -1) :
					SceAudioForDriver_28B3AF10(1, freq, -1, -1);
				if (transferResult < 0)
					goto unmapDirect;
			}
			volume = direct->ramp.volume;
			leftVolume = (direct->volume[0] * volume) >> 8;
			rightVolume = (direct->volume[1] * volume) >> 8;
			output = &audioOutputBuffers[channel][audioOutputBufferIndex[channel]][0][0];
			input = (const int16_t *)(mapped +
				bytesPerFrame * (direct->len - remaining));
			if (leftVolume != 0x8000 || direction != 0 || channel != 0 ||
			    rightVolume != 0x8000 || bytesPerFrame != 4) {
				int16_t *destination = output;
				int16_t *end = output + 2 * samples;

				do {
					if (direction != 0) {
						int rampVolume = current / 16384;
						int crossed;

						current = (uint32_t)current + (uint32_t)step;
						crossed = (uint32_t)(int8_t)direction *
							((uint32_t)target - (uint32_t)current);
						leftVolume = (direct->volume[0] * rampVolume) >> 8;
						rightVolume = (direct->volume[1] * rampVolume) >> 8;
						if (crossed < 0) {
							current = target;
							direction = 0;
							direct->ramp.direction = 0;
						}
					}
					destination[0] = (int32_t)((uint32_t)leftVolume *
						(uint32_t)(int32_t)input[0]) >> 15;
					input = (const int16_t *)((const char *)input + bytesPerFrame);
					destination[1] = (int32_t)((uint32_t)rightVolume *
						(uint32_t)(int32_t)input[-1]) >> 15;
					destination += 2;
				} while (destination != end);
				direct->ramp.current = current;
				if (channel == 0)
					input = output;
			}
			if (channel == 0) {
				transferResult = audioProcessDirectPort(direct, output, input, samples);
				if (transferResult < 0)
					goto unmapDirect;
			}
			sceKernelSetEventFlag(audioEventFlag, mask);
			transferResult = audioDispatchDmaBuffer(
				audioOutputPhysicalAddress[channel][audioOutputBufferIndex[channel]],
				channel + 1, samples);
			{
				uint32_t time = sceKernelGetProcessTimeLowCore();

				direct->deadline = time + (1000000U * samples) / freq;
			}
			sceKernelWaitEventFlag(audioEventFlag, mask, 5, NULL, NULL);
			audioOutputBufferIndex[channel] ^= 1;
		} else {
			transferResult = 0;
			sceKernelSetEventFlag(audioEventFlag, mask);
			direct->deadline = audioWaitForPlaybackDeadline(direct->deadline,
				freq, samples);
			sceKernelWaitEventFlag(audioEventFlag, mask, 5, NULL, NULL);
		}
		remaining -= samples;
		direct->remaining = remaining;
	} while ((uint16_t)remaining != 0);
	audioUpdateAdoption(direct);
	result = sceKernelUserUnmap(mapping, userBuffer, 0);
	if (transferResult < 0) {
		result = transferResult;
		goto unlockDirect;
	}
	sceKernelSetEventFlag(audioEventFlag, mask);
	if (result >= 0)
		result = direct->len;
	goto out;
unmapDirect:
	sceKernelUserUnmap(mapping, userBuffer, 0);
	result = transferResult;
unlockDirect:
	sceKernelSetEventFlag(audioEventFlag, mask);
	goto out;
unlockMain:
	sceKernelSetEventFlag(audioEventFlag, 32);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutSetVolume(int port, int ch, int *vol)
{
	unsigned int syscall;
	unsigned int type = ((unsigned int)port >> 8) & 0xff;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioMainPort *main;
	AudioDirectPort *direct;
	unsigned int volume[2];
	unsigned int maximum;
	int result;

	if (vol == NULL)
		return SCE_AUDIO_OUT_ERROR_INVALID_POINTER;
	syscall = sceSyscallEnter();
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	result = sceKernelCopyFromUser(volume, vol, sizeof(volume));
	if (result < 0)
		goto unlock;
	if (type != 0) {
		if (type > 2) {
			result = (int)0xc026000a;
			goto unlock;
		}
		direct = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
		if (direct->len == 0) {
			result = (int)0xc0260001;
			goto unlockDirect;
		}
		if ((ch & 1) == 0)
			volume[0] = direct->volume[0];
		if ((ch & 2) == 0)
			volume[1] = direct->volume[1];
		if (SceSblACMgrForDriver_6C5AB07F(0) != 0)
			maximum = 0xffff;
		else if (sceSblACMgrIsSystemProgram(0) != 0)
			maximum = 0xffff;
		else
			maximum = 0x8000;
		if (volume[0] > maximum || volume[1] > maximum) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_VOLUME;
		} else {
			result = 0;
			direct->volume[0] = volume[0];
			direct->volume[1] = volume[1];
		}
unlockDirect:
		sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	} else {
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if (index > 8) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
			goto unlockMain;
		}
		main = &storage->main[index];
		if (main->config.len == 0) {
			result = (int)0xc0260001;
			goto unlockMain;
		}
		if ((ch & 1) == 0)
			volume[0] = main->volume[0];
		if ((ch & 2) == 0)
			volume[1] = main->volume[1];
		if (SceSblACMgrForDriver_6C5AB07F(0) != 0)
			maximum = 0xffff;
		else if (sceSblACMgrIsSystemProgram(0) != 0)
			maximum = 0xffff;
		else
			maximum = 0x8000;
		if (volume[0] > maximum || volume[1] > maximum) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_VOLUME;
		} else {
			result = 0;
			main->volume[0] = volume[0];
			main->volume[1] = volume[1];
		}
unlockMain:
		sceKernelSetEventFlag(audioEventFlag, 32);
	}
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutSetConfig(int port, int len, int freq, int mode)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int type = ((unsigned int)port >> 8) & 0xff;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioMainPort *main;
	AudioDirectPort *direct;
	unsigned char bytesPerFrame;
	int result;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto unlock;
	}
	if (type != 0) {
		if (type > 2) {
			result = (int)0xc026000a;
			goto unlock;
		}
		direct = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
		if (direct->len == 0) {
			result = (int)0xc0260001;
			goto unlockDirect;
		}
		if ((int)len < 0)
			len = direct->len;
		else if ((len & 63) != 0 || len - 1 > 0xffbf) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
			goto unlockDirect;
		}
		if (freq < 0)
			freq = direct->freq;
		else if (freq != 48000 && freq != 44100 && freq != 32000 &&
			 freq != 24000 && freq != 22050 && freq != 16000 &&
			 freq != 12000 && freq != 11025 && freq != 8000) {
			if (SceSblACMgrForDriver_FD00C72A(0) != 1 || freq != 44184) {
				result = SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
				goto unlockDirect;
			}
		}
		if (mode >= 0) {
			if ((unsigned char)mode == 0)
				bytesPerFrame = 2;
			else if ((unsigned char)mode == 1)
				bytesPerFrame = 4;
			else {
				result = SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
				goto unlockDirect;
			}
			if (type == 1) {
				result = sceAudioOutSetEffectType(((unsigned int)mode >> 8) & 0xff);
				if (result < 0)
					goto unlockDirect;
			}
			direct->bytesPerFrame = bytesPerFrame;
		}
		result = 0;
		direct->len = len;
		direct->freq = freq;
unlockDirect:
		sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	} else {
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if (index > 8) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
			goto unlockMain;
		}
		main = &storage->main[index];
		if (main->config.len == 0) {
			result = (int)0xc0260001;
			goto unlockMain;
		}
		if ((int)len < 0)
			len = main->config.len;
		else if ((len & 63) != 0 || len - 1 > 0xffbf) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_SIZE;
			goto unlockMain;
		}
		if (freq >= 0 && freq != 48000) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
			goto unlockMain;
		}
		if (mode >= 0) {
			if ((unsigned char)mode == 0)
				main->config.bytesPerFrame = 2;
			else if ((unsigned char)mode == 1)
				main->config.bytesPerFrame = 4;
			else {
				result = SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
				goto unlockMain;
			}
		}
		main->config.len = len;
		result = 0;
unlockMain:
		sceKernelSetEventFlag(audioEventFlag, 32);
	}
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutGetAdopt(int type)
{
	unsigned int syscall = sceSyscallEnter();
	AudioProcessLocalStorage *storage;
	AudioDirectPort *port;
	AudioDirectPort *adopted;
	int channel;
	int result;

	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto out;
	}
	switch (type) {
	case 0:
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		result = storage->adopt != 0 || storage->ramp.direction == -1;
		sceKernelSetEventFlag(audioEventFlag, 32);
		goto out;
	case 1:
	case 2:
		port = &storage->direct[type - 1];
		break;
	case 3:
		if (SceSblACMgrForDriver_6C5AB07F(0) == 0) {
			result = (int)0xc026000a;
			goto out;
		}
		port = &storage->direct[1];
		break;
	default:
		result = (int)0xc026000a;
		goto out;
	}
	sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
	channel = port->channel;
	if (port->remaining == 0)
		audioUpdateAdoption(port);
	adopted = audioAdoptedPorts[channel];
	if (adopted == NULL)
		adopted = audioAdoptingPorts[channel];
	result = port == adopted;
	sceKernelSetEventFlag(audioEventFlag, port->eventMask);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutGetConfig(int port, int type)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int portType = ((unsigned int)port >> 8) & 0xff;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioDirectPort *direct;
	AudioMainPort *main;
	unsigned int bytesPerFrame;
	int result;

	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto out;
	}
	if (portType != 0) {
		if (portType > 2) {
			result = (int)0xc026000a;
			goto out;
		}
		direct = &storage->direct[portType - 1];
		sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
		result = direct->len;
		bytesPerFrame = direct->bytesPerFrame;
		if (result == 0)
			result = (int)0xc0260001;
		else {
			switch (type) {
			case 1:
				result = direct->freq;
				break;
			case 2:
				result = bytesPerFrame != 2;
				break;
			case 0:
				break;
			default:
				result = SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE;
				break;
			}
		}
		sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	} else {
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if (index > 8) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
			goto unlockMain;
		}
		main = &storage->main[index];
		result = main->config.len;
		if (result == 0)
			result = (int)0xc0260001;
		else {
			switch (type) {
			case 1:
				result = 48000;
				break;
			case 2:
				result = main->config.bytesPerFrame != 2;
				break;
			case 0:
				break;
			default:
				result = SCE_AUDIO_OUT_ERROR_INVALID_CONF_TYPE;
				break;
			}
		}
unlockMain:
		sceKernelSetEventFlag(audioEventFlag, 32);
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutGetRestSample(int port)
{
	unsigned int syscall = sceSyscallEnter();
	unsigned int type = ((unsigned int)port >> 8) & 0xff;
	unsigned int index = (unsigned char)port;
	AudioProcessLocalStorage *storage;
	AudioDirectPort *direct;
	AudioMainPort *main;
	int result;

	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL) {
		result = (int)0x80260230;
		goto out;
	}
	if (type != 0) {
		if (type > 2) {
			result = (int)0xc026000a;
			goto out;
		}
		direct = &storage->direct[type - 1];
		sceKernelWaitEventFlag(audioEventFlag, direct->eventMask, 5, NULL, NULL);
		result = (int)0xc0260001;
		if (direct->len != 0)
			result = direct->remaining;
		sceKernelSetEventFlag(audioEventFlag, direct->eventMask);
	} else {
		sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
		if (index > 8) {
			result = SCE_AUDIO_OUT_ERROR_INVALID_PORT;
			goto unlockMain;
		}
		main = &storage->main[index];
		result = (int)0xc0260001;
		if (main->config.len != 0) {
			result = main->current.cursor != NULL ? main->current.remaining : 0;
			if (main->queued.mapping > 0)
				result += main->queued.remaining;
		}
unlockMain:
		sceKernelSetEventFlag(audioEventFlag, 32);
	}
out:
	sceSyscallLeave(syscall);
	return result;
}

int SceAudioForDriver_629261CD(SceUID pid, int mask, unsigned int volume,
			    int duration)
{
	AudioProcessLocalStorage *storage;
	AudioDirectPort *port;
	int result;
	int adopt;

	if (mask == 0)
		return 0;
	if (volume > 256)
		return -1;
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		return result;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							 (void **)&storage, 1);
	if (result >= 0) {
		if ((mask & 2) != 0) {
			sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
			audioSetVolumeRampTarget(&storage->ramp, 48000,
						 storage->adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, 32);
		}
		if ((mask & 4) != 0) {
			port = &storage->direct[0];
			sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
			adopt = (port->adoptState & 1) == 0 && (port->adoptState & 6) != 6;
			audioSetVolumeRampTarget(&port->ramp, port->freq, adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		}
		if ((mask & 8) != 0) {
			port = &storage->direct[1];
			sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
			adopt = (port->adoptState & 1) == 0 && (port->adoptState & 6) != 6;
			audioSetVolumeRampTarget(&port->ramp, port->freq, adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		}
	}
	sceKernelSetEventFlag(audioEventFlag, 16);
	return result;
}

int SceAudioForDriver_B05D5B28(SceUID pid, int type)
{
	AudioProcessLocalStorage *storage;
	AudioVolumeRamp *ramp;
	int result;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		return result;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							 (void **)&storage, 1);
	if (result >= 0) {
		if (type == 1)
			ramp = &storage->direct[0].ramp;
		else if (type == 2)
			ramp = &storage->direct[1].ramp;
		else if (type == 0)
			ramp = &storage->ramp;
		else {
			result = (int)0xc026000a;
			goto unlock;
		}
		result = ramp->volume;
	}
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
	return result;
}

int sceAudioOutSetPortVolume_forUser(int mask, unsigned int volume, int duration)
{
	unsigned int syscall = sceSyscallEnter();
	SceUID pid = sceKernelGetProcessId();
	AudioProcessLocalStorage *storage;
	AudioDirectPort *port;
	int result;
	int adopt;

	if (mask == 0) {
		result = 0;
		goto out;
	}
	if (volume > 256) {
		result = -1;
		goto out;
	}
	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							 (void **)&storage, 1);
	if (result >= 0) {
		if ((mask & 2) != 0) {
			sceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL);
			audioSetVolumeRampTarget(&storage->ramp, 48000,
						 storage->adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, 32);
		}
		if ((mask & 4) != 0) {
			port = &storage->direct[0];
			sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
			adopt = (port->adoptState & 1) == 0 && (port->adoptState & 6) != 6;
			audioSetVolumeRampTarget(&port->ramp, port->freq, adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		}
		if ((mask & 8) != 0) {
			port = &storage->direct[1];
			sceKernelWaitEventFlag(audioEventFlag, port->eventMask, 5, NULL, NULL);
			adopt = (port->adoptState & 1) == 0 && (port->adoptState & 6) != 6;
			audioSetVolumeRampTarget(&port->ramp, port->freq, adopt, volume, duration);
			sceKernelSetEventFlag(audioEventFlag, port->eventMask);
		}
	}
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutGetPortVolume_forUser(int type)
{
	unsigned int syscall = sceSyscallEnter();
	SceUID pid = sceKernelGetProcessId();
	AudioProcessLocalStorage *storage;
	AudioVolumeRamp *ramp;
	int result;

	result = sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	if (result < 0)
		goto out;
	result = sceKernelGetRemoteKPLS(pid, audioStorageKey,
							 (void **)&storage, 1);
	if (result >= 0) {
		if (type == 1)
			ramp = &storage->direct[0].ramp;
		else if (type == 2)
			ramp = &storage->direct[1].ramp;
		else if (type == 0)
			ramp = &storage->ramp;
		else {
			result = (int)0xc026000a;
			goto unlock;
		}
		result = ramp->volume;
	}
unlock:
	sceKernelSetEventFlag(audioEventFlag, 16);
out:
	sceSyscallLeave(syscall);
	return result;
}

int sceAudioOutSetAdoptMode(int mode)
{
	audioAdoptMode = mode;
	return 0;
}

int SceAudioForDriver_F07D34C2(void (*callback)(SceUID pid))
{
	audioPortOpenCallback = callback;
	return 0;
}

int SceAudioForDriver_E87902DF(void (*callback)(SceUID pid))
{
	audioPortCloseCallback = callback;
	return 0;
}

int SceAudioForDriver_75915003(uintptr_t sourceAddress, unsigned int wordCount)
{
	return audioTransferDmaBuffer(sourceAddress, wordCount);
}

int SceAudioForDriver_15FB30DF(unsigned int volume)
{
	int result;

	if (volume > 0x8000)
		return SCE_AUDIO_OUT_ERROR_INVALID_VOLUME;
	result = audioSetHardwareVolume(1, volume, volume);
	if (result >= 0)
		audioHardwareVolume = volume;
	return result;
}

int audioModuleStart(SceSize arglen, const void *argp)
{
	int result;
	unsigned int index;

	(void)arglen;
	(void)argp;
	memset(&audioWork, 0, sizeof(audioWork));
	audioHardwareVolume = 0x8000;
	result = sceKernelCreateEventFlag("SceAudio", 0x1000, 0xf0, NULL);
	audioEventFlag = result;
	if (result < 0)
		return result;
	result = audioInitializeHardware();
	if (result < 0)
		return result;
	result = sceKernelRegisterKPLS("SceAudio",
		sizeof(AudioProcessLocalStorage));
	audioStorageKey = result;
	if (result < 0)
		return result;
	result = sceKernelCreateThread("SceAudioMixer", _sceAudiodrvMixer,
		18, 4096, 0, 0x80000, NULL);
	if (result < 0)
		return result;
	result = sceKernelStartThread(result, 0, NULL);
	if (result < 0)
		return result;
	audioInitializeEffects();
	sceKernelRegisterSysEventHandler("SceAudio", audioSystemEventHandler, NULL);
	sceKernelRegisterProcEventHandler("SceAudio", &audioProcessEventHandlers, NULL);
	for (index = 0; index < 2; ++index) {
		result = sceKernelVAtoPA(audioOutputBuffers[0][index],
			&audioOutputPhysicalAddress[0][index]);
		SCE_DBG_ASSERT(result, 2602);
		result = sceKernelVAtoPA(audioOutputBuffers[1][index],
			&audioOutputPhysicalAddress[1][index]);
		SCE_DBG_ASSERT(result, 2602);
	}
	return 0;
}

int SceAudioForDriver_134C96C1(int freq)
{
	if (freq != 8000 && freq != 48000)
		return SCE_AUDIO_OUT_ERROR_INVALID_SAMPLE_FREQ;
	sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	audioEightKhzMode = freq == 8000;
	sceKernelSetEventFlag(audioEventFlag, 16);
	return 0;
}

int SceAudioForDriver_FC375BAC(int mode)
{
	if ((unsigned int)mode - 1 > 1)
		return SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
	sceKernelWaitEventFlag(audioEventFlag, 16, 5, NULL, NULL);
	audioModeOneEnabled = mode == 1;
	SceClockgenForDriver_820D3516(48000);
	sceKernelSetEventFlag(audioEventFlag, 16);
	return 0;
}

int audioProcessDirectPort(AudioDirectPort *port, int16_t *output,
			   const int16_t *input, unsigned int sampleCount)
{
	const int16_t *samples = input;
	unsigned int setting;
	int result;

	if (port->alcMode != 0) {
		result = audioProcessAlc(&audioAlc, input, output, sampleCount);
		if (result < 0)
			return (int)(0x80260211U - (unsigned int)result);
		samples = output;
	}
	setting = port->equalizer;
	if (audioEqualizerSetting != setting) {
		audioEqualizerSetting = setting;
		audioSetEqualizer(&audioEqualizer, setting);
		setting = port->equalizer;
	}
	if (setting == 0x03333333) {
		if (samples == output)
			return 0;
		memcpy(output, samples, 4 * sampleCount);
		return 0;
	}
	result = audioProcessEqualizer(&audioEqualizer, samples, output,
				       sampleCount);
	if (result >= 0)
		return 0;
	return (int)(0x80260219U - (unsigned int)result);
}

int sceAudioOutSetAlcMode(int mode)
{
	AudioProcessLocalStorage *storage;

	if ((unsigned int)mode > 1)
		return SCE_AUDIO_OUT_ERROR_INVALID_FX_TYPE;
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL)
		return (int)0x80260230;
	storage->direct[0].alcMode = mode;
	return 0;
}

int sceAudioOutSetEffectType(int type)
{
	AudioProcessLocalStorage *storage;
	uint32_t equalizer;

	switch (type) {
	case 0:
		equalizer = 0x03333333;
		break;
	case 1:
		equalizer = 0x03224322;
		break;
	case 2:
		equalizer = 0x04332234;
		break;
	case 3:
		equalizer = 0x02343313;
		break;
	case 4:
		equalizer = 0x01432331;
		break;
	default:
		return SCE_AUDIO_OUT_ERROR_INVALID_FX_TYPE;
	}
	storage = SceProcessmgrForDriver_EE694840(audioStorageKey);
	if (storage == NULL)
		return (int)0x80260230;
	storage->direct[0].equalizer = equalizer;
	return 0;
}

int audioInitializeEffects(void)
{
	audioInitializeAlc(&audioAlc, 16, 16);
	audioInitializeEqualizer(&audioEqualizer);
	audioEqualizerSetting = 0;
	return 0;
}

int audioSetVolumeRamp(AudioVolumeRamp *ramp, int freq, int adopt, int duration)
{
	int current;
	int target;
	int step;
	int32x2_t initial;

	if (ramp->direction != 0)
		current = ramp->current / 16384;
	else if (adopt == 0)
		current = ramp->volume;
	else
		current = 0;
	target = adopt != 0 ? ramp->volume : 0;
	if (current == target) {
		ramp->direction = 0;
		ramp->step = 0;
		return 0;
	}
	if (duration <= 0)
		duration = 1;
	else if (duration >= 4096)
		duration = 4096;
	if (freq == 0)
		freq = 48000;
	step = 167772160 / ((freq * duration) / 25);
	initial = vdup_n_s32(0);
	initial = vset_lane_s32(target, initial, 0);
	initial = vset_lane_s32(current, initial, 1);
	vst1_s32(&ramp->target, vshl_n_s32(initial, 14));
	ramp->direction = current < target ? 1 : -1;
	ramp->step = current < target ? step : -step;
	return 0x400000 / step;
}

int audioSetVolumeRampTarget(AudioVolumeRamp *ramp, int freq, int adopt,
			     int volume, int duration)
{
	int current;
	int step;
	int result = 0;

	if (adopt == 0)
		goto out;
	current = ramp->volume;
	if (current == volume) {
		ramp->direction = 0;
		ramp->step = 0;
		goto out;
	}
	if (duration <= 0)
		duration = 1;
	else if (duration >= 4096)
		duration = 4096;
	if (freq == 0)
		freq = 48000;
	step = 167772160 / ((freq * duration) / 25);
	ramp->target = (uint32_t)volume << 14;
	ramp->current = (uint32_t)current << 14;
	ramp->direction = current < volume ? 1 : -1;
	ramp->step = current < volume ? step : -step;
	result = 0x400000 / step;
out:
	ramp->volume = volume;
	return result;
}
