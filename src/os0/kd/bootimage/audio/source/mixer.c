#include "audio_work.h"
/* Audio mixer and playback timing. */

#include <stdint.h>
#include <string.h>
#include <arm_neon.h>
#include <sce/threadmgr.h>
#include <sce/debug.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/sysmem/address_space.h>

#include "audio_private.h"

uint32_t ksceKernelGetProcessTimeLowCore(void);
int ksceKernelUserUnmap(SceUID uid, const void *address, int flags);
int audioSubmitDmaBuffer(uintptr_t sourceAddress, unsigned int wordCount,
			 unsigned int bank, unsigned int stream);
int audioSubmitSingleDmaBuffer(uintptr_t sourceAddress, unsigned int wordCount);
int audioSetHardwareTransferMode(unsigned int channel, unsigned int mode);
int audioSetSingleTransferMode(int mode);
int audioQueueHardwareBuffer(int channel, void *buffer, unsigned int wordCount);
int audioQueueSingleHardwareBuffer(void *buffer, unsigned int wordCount);
int audioInitializeI2s(void);
int audioInitializeSpdif(void);
int audioInitializeSrcMix(void);
int audioPrepareSuspendHook(void);
int audioSuspendI2s(void);
int ScePervasiveForDriver_146108D4(int index);
int SceAudioForDriver_28B3AF10(int channel, int inputRate, int outputRate,
			     int clockRate);


uint32_t audioWaitForPlaybackDeadline(uint32_t deadline, uint32_t freq,
				     uint32_t samples);
int audioDispatchDmaBuffer(uintptr_t sourceAddress, int channel,
			   unsigned int wordCount);

int _sceAudiodrvMixer(SceSize arglen, void *argp)
{
	uintptr_t physicalAddress[2];
	int32_t mixed[128];
	unsigned int state = 0;
	unsigned int buffer = 0;
	int result;

	(void)arglen;
	(void)argp;
	result = ksceKernelVAtoPA(audioMixerBuffers[0], &physicalAddress[0]);
	SCE_DBG_ASSERT(result >= 0, 253);
	result = ksceKernelVAtoPA(audioMixerBuffers[1], &physicalAddress[1]);
	SCE_DBG_ASSERT(result >= 0, 255);
	audioMixerDeadline = ksceKernelGetProcessTimeLowCore();
	while (ksceKernelWaitEventFlag(audioEventFlag, 32, 5, NULL, NULL) >= 0) {
		AudioProcessLocalStorage *process;
		unsigned int index;

		memset(mixed, 0, sizeof(mixed));
		process = audioProcessList;
		while (process != NULL) {
			int current;
			unsigned int notify = 0;
			unsigned int adopt = process->adopt;
			int direction = (unsigned char)process->ramp.direction;
			int target = process->ramp.target;
			int step = process->ramp.step;

			if (audioSuspended != 0) {
				current = 0;
				process->ramp.current = 0;
			} else {
				current = process->ramp.current;
			}
			for (index = 0; index < 9; ++index) {
				AudioMainPort *port = &process->main[index];
				int16_t *input = port->current.cursor;
				unsigned int remaining;
				unsigned int samples;
				unsigned int bytesPerFrame;

				if (input == NULL) {
					__builtin_prefetch(&process->main[index + 2].queued.mapping);
					if (port->queued.mapping != 0) {
						port->current = port->queued;
						input = port->current.cursor;
						notify |= 1U << index;
						port->queued.mapping = 0;
						port->queued.cursor = NULL;
						if (input == NULL)
							continue;
					} else {
						__builtin_prefetch(&process->main[index + 2].queued.cursor);
						port->queued.cursor = NULL;
						continue;
					}
				}
				remaining = port->current.remaining;
				bytesPerFrame = port->current.bytesPerFrame;
				direction = (unsigned char)process->ramp.direction;
				current = process->ramp.current;
				samples = remaining >= 64 ? 64 : remaining;
				if ((int8_t)(adopt | (unsigned char)direction) != 0) {
					int volume = process->ramp.volume;
					int leftPortVolume = port->volume[0];
					int rightPortVolume = port->volume[1];
					int leftVolume = (leftPortVolume * volume) >> 8;
					int rightVolume = (rightPortVolume * volume) >> 8;
					unsigned int sample;

					for (sample = 0; sample < samples; ++sample) {
						int left;
						int right;
						int32_t leftAccumulator;
						int32_t rightAccumulator;

						if (direction != 0) {
							int rampVolume = current / 16384;
							int crossed;

							current = (uint32_t)current + (uint32_t)step;
							leftVolume = (rampVolume * leftPortVolume) >> 8;
							rightVolume = (rampVolume * rightPortVolume) >> 8;
							crossed = (uint32_t)(int8_t)direction *
								((uint32_t)target - (uint32_t)current);
							if (crossed < 0) {
								current = target;
								direction = 0;
							}
						}
						left = input[0];
						input = (int16_t *)((char *)input + bytesPerFrame);
						right = input[-1];
						leftAccumulator = mixed[2 * sample];
						rightAccumulator = mixed[2 * sample + 1];
						__builtin_prefetch(mixed + 2 * (sample + 8));
						mixed[2 * sample] = (uint32_t)leftAccumulator +
							(uint32_t)((int32_t)((uint32_t)leftVolume *
							(uint32_t)left) >> 7);
						mixed[2 * sample + 1] =
							(uint32_t)rightAccumulator +
							(uint32_t)((int32_t)((uint32_t)rightVolume *
							(uint32_t)right) >> 7);
					}
					remaining -= samples;
					state |= 3;
					port->current.remaining = remaining;
				} else {
					direction = 0;
					input = (int16_t *)((char *)input + bytesPerFrame * samples);
					remaining -= samples;
					state |= 1;
					port->current.remaining = remaining;
				}
				if (remaining > 64) {
					if (audioLegacyTitleCompatibility != 0)
						state |= 4;
				} else {
					state |= 4;
					if (remaining == 0) {
						input = NULL;
						ksceKernelUserUnmap(port->current.mapping, NULL, 0);
						port->current.mapping = 0;
					}
				}
				port->current.cursor = input;
			}
			process->ramp.current = current;
			process->ramp.direction = direction;
			if (notify != 0)
				ksceKernelSetEventFlag(process->eventFlag, notify);
			process = (AudioProcessLocalStorage *)process->link.next;
		}
		if (state != 0) {
			int16_t *output = &audioMixerBuffers[buffer][state & ~63U][0];

			for (index = 0; index < 128; index += 8) {
				int32x4_t first = vld1q_s32(mixed + index);
				int32x4_t second = vld1q_s32(mixed + index + 4);
				int16x8_t samples = vcombine_s16(vqrshrn_n_s32(first, 8),
					vqrshrn_n_s32(second, 8));

				vst1q_s16(output, samples);
				output += 8;
			}
			state += 64;
			if (state != 0) {
				ksceKernelSetEventFlag(audioEventFlag, 32);
				if ((state & 4) != 0 || state >= 1024 || audioSuspended != 0) {
					unsigned int samples = state & ~63U;

					if ((state & 2) != 0) {
						uintptr_t address = physicalAddress[buffer];

						buffer ^= 1;
						audioDispatchDmaBuffer(address, 0, samples);
						state = 0;
						audioMixerDeadline = ksceKernelGetProcessTimeLowCore() +
							(1000U * samples) / 48;
					} else {
						state = 0;
						audioMixerDeadline = audioWaitForPlaybackDeadline(
							audioMixerDeadline, 48000, samples);
					}
				}
				continue;
			}
		}
		audioMixerIdle = 1;
		ksceKernelSetEventFlag(audioEventFlag, 32);
		state = 0;
		ksceKernelWaitEventFlag(audioEventFlag, 1, 5, NULL, NULL);
		audioMixerDeadline = ksceKernelGetProcessTimeLowCore();
	}
	ksceKernelExitThread(0);
	return 0;
}

uint32_t audioWaitForPlaybackDeadline(uint32_t deadline, uint32_t freq,
				     uint32_t samples)
{
	uint32_t current = ksceKernelGetProcessTimeLowCore();
	uint32_t duration = (1000000 * samples) / freq;
	uint32_t delay = deadline - current;

	if (delay >= 128000)
		return current + duration;
	deadline += duration;
	if (delay != 0)
		sceKernelDelayThread(delay);
	return deadline;
}

int audioDispatchDmaBuffer(uintptr_t sourceAddress, int channel,
			   unsigned int wordCount)
{
	if (channel == 1)
		return audioSubmitDmaBuffer(sourceAddress, wordCount, 1, 0);
	if (channel == 2)
		return audioSubmitDmaBuffer(sourceAddress, wordCount, 0, 1);
	if (channel != 0)
		return -1;
	return audioSubmitDmaBuffer(sourceAddress, wordCount, 0, 0);
}

int audioTransferDmaBuffer(uintptr_t sourceAddress, unsigned int wordCount)
{
	return audioSubmitSingleDmaBuffer(sourceAddress, wordCount);
}

int SceAudioForDriver_AC9383D0(unsigned int channel, unsigned int mode)
{
	return audioSetHardwareTransferMode(channel, mode);
}

int SceAudioForDriver_85987B88(int mode)
{
	return audioSetSingleTransferMode(mode);
}

int SceAudioForDriver_81EB0AE5(int channel, void *buffer, unsigned int wordCount)
{
	return audioQueueHardwareBuffer(channel, buffer, wordCount);
}

int SceAudioForDriver_6F613D36(void *buffer, unsigned int wordCount)
{
	return audioQueueSingleHardwareBuffer(buffer, wordCount);
}

int audioInitializeHardware(void)
{
	int result;

	result = audioInitializeI2s();
	if (result < 0)
		return result;
	result = audioInitializeSpdif();
	if (result < 0)
		return result;
	result = audioInitializeSrcMix();
	if (result < 0)
		return result;
	SceAudioForDriver_28B3AF10(0, 48000, -1, 48000);
	SceAudioForDriver_28B3AF10(1, -1, 48000, 48000);
	return 0;
}

int audioPrepareSuspend(void)
{
	audioPrepareSuspendHook();
	audioSuspendI2s();
	return 0;
}

int audioResumeHardware(void)
{
	if (audioInitializeI2s() >= 0 && audioInitializeSpdif() >= 0 &&
	    audioInitializeSrcMix() >= 0) {
		SceAudioForDriver_28B3AF10(0, 48000, -1, 48000);
		SceAudioForDriver_28B3AF10(1, -1, 48000, 48000);
	}
	audioSuspended = 0;
	return 0;
}

int audioSuspendI2s(void)
{
	ScePervasiveForDriver_146108D4(0);
	ScePervasiveForDriver_146108D4(1);
	ScePervasiveForDriver_146108D4(2);
	ScePervasiveForDriver_146108D4(3);
	ScePervasiveForDriver_146108D4(5);
	ScePervasiveForDriver_146108D4(4);
	ScePervasiveForDriver_146108D4(7);
	return 0;
}

int audioSetSuspended(void)
{
	audioSuspended = 1;
	return 0;
}

