/* Audio input capture implementation; original source filename unavailable. */
#include "audioin_internal.h"
#include <sysmem/cp15.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <psp2/audioin.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <syscon/syscon.h>
#include <sysmem/sblaimgr.h>

extern int SceCodecForDriver_41CFFE48(int *gain, int *alternateGain);
extern int initializeAudioInAecLibrary(void);
extern int startAudioInAec(AudioInAecContext *context);
extern int processAudioInAecBuffers(AudioInAecContext *context,
		int16_t *first, int16_t *second, int gain, unsigned int flags);
extern int SceCodecForDriver_C1DB7BAF(unsigned int input);
extern void initializeAudioInResampler(void);
extern int configureAudioInAec(AudioInAecContext *context, const void *control);
extern void initializeAudioInLegacyQueue(void);
extern void resetAudioInLegacyQueue(void);
extern int readAudioInLegacyQueue(int16_t *samples, unsigned int count);
extern void resampleAudioInCapture(unsigned int stream, const int16_t *input,
		int16_t *output, unsigned int count, int channel);

int audioInSysEventHandler(int resume, int eventId, void *unusedArgs, void *unusedOpt)
{
	if (resume == 0) {
		if (eventId == 0x102) {
			SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~4u);
			SceCpuForDriver_2A40BB93(&g_audioInRequestFlags, 2);
#define WAKE_INPUT_PORT(index) do { \
	if ((g_audioInPorts[index].flags & 6) == 6) \
		sceKernelSetEventFlag(g_audioInPorts[index].eventFlag, 1); \
} while (0)
			WAKE_INPUT_PORT(0);
			WAKE_INPUT_PORT(1);
			WAKE_INPUT_PORT(2);
			WAKE_INPUT_PORT(3);
#undef WAKE_INPUT_PORT
			g_audioInAppliedInput = -1;
		}
	} else if (resume == 1 && eventId == 0x1000f) {
		SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~2u);
		SceCpuForDriver_2A40BB93(&g_audioInRequestFlags, 4);
		sceKernelSetEventFlag(g_audioInRequestEventFlag, 256);
		return 0;
	}
	return 0;
}

int audioInProcessExitHandler(SceUID pid, ProcEventParam *unusedParam,
		int unusedArgument)
{
#define CLOSE_EXIT_PORT(index) do { \
	if (g_audioInPorts[index].eventFlag != 0) { \
		if (g_audioInPorts[index].flags & 0x10) { \
			g_audioInPorts[index].flags &= 0xffffff79; \
			sceKernelSetEventFlag(g_audioInPorts[index].eventFlag, 1024); \
		} else { \
			sceKernelDeleteEventFlag(g_audioInPorts[index].eventFlag); \
			g_audioInPorts[index].eventFlag = 0; \
		} \
	} \
	g_audioInPorts[index].flags = 0; \
	g_audioInPorts[index].pid = 0; \
	g_audioInOpenPortCount--; \
	g_audioInPorts[index].port = 0; \
} while (0)
	lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	if (pid != g_audioInPorts[0].pid)
		goto checkSecond;
	CLOSE_EXIT_PORT(0);
	if (g_audioInPortClosed != NULL) {
		g_audioInPortClosed(pid);
		if (pid != g_audioInPorts[1].pid)
			goto checkThird;
		goto closeSecond;
	}
checkSecond:
	if (pid != g_audioInPorts[1].pid)
		goto checkThird;
closeSecond:
	CLOSE_EXIT_PORT(1);
	if (g_audioInPortClosed != NULL) {
		g_audioInPortClosed(pid);
		if (pid != g_audioInPorts[2].pid)
			goto checkFourth;
		goto closeThird;
	}
checkThird:
	if (pid != g_audioInPorts[2].pid)
		goto checkFourth;
closeThird:
	CLOSE_EXIT_PORT(2);
	if (g_audioInPortClosed != NULL) {
		g_audioInPortClosed(pid);
		if (pid != g_audioInPorts[3].pid)
			goto complete;
		goto closeFourth;
	}
checkFourth:
	if (pid != g_audioInPorts[3].pid)
		goto complete;
closeFourth:
	CLOSE_EXIT_PORT(3);
	if (g_audioInPortClosed != NULL)
		g_audioInPortClosed(pid);
complete:
#undef CLOSE_EXIT_PORT
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	return 0;
}

static inline __attribute__((always_inline)) void
copyAudioInChannel(int16_t *dest, const int16_t *source,
		unsigned int count, int channel)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		dest[i] = source[2 * i + channel];
}

static inline __attribute__((always_inline)) int
copyAudioInToPort(AudioInPort *port, void *dest, const int16_t *samples,
		unsigned int bytes)
{
	if ((g_audioInRequestFlags & 1) || (port->flags & 0x80))
		samples = g_audioInSilenceSamples;
	if (port->pid == 0x10005) {
		memcpy(dest, samples, bytes);
		return 0;
	}
	return sceKernelCopyToUserProc(port->pid, dest, samples, bytes);
}

int __attribute__((noreturn)) audioInCaptureThread(SceSize unusedSize,
		void *unusedArgument)
{
	unsigned int i;

	initializeAudioInResampler();
	for (;;) {
		if ((!g_audioInPorts[0].flags && !g_audioInPorts[1].flags &&
		     !g_audioInPorts[2].flags && !g_audioInPorts[3].flags) ||
		    (g_audioInRequestFlags & 2)) {
			if (sceKernelWaitEventFlag(g_audioInRequestEventFlag, 256, 5,
					NULL, NULL) < 0)
				sceKernelDelayThread(100000);
		}
		if (g_audioInRequestFlags & 0x100) {
			configureAudioInCaptureBuffers();
			SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~0x100u);
		}
		if (g_audioInRequestFlags & 0x200) {
			unsigned int input = g_audioInRequestedInput;

			if (lockFastMutex((SceKernelFastMutex *)&g_audioInLock) >= 0) {
				if (input <= 2 && SceSblAIMgrForDriver_C6E83F34())
					input = 32;
				if (g_audioInAppliedInput != input) {
					int channel;

					g_audioInAppliedInput = input;
					switch (input) {
					case 0:
						channel = SceCodecForDriver_C1DB7BAF(0);
						if (channel >= 0) {
							unsigned char hardwareInfo[16];

							if (g_audioInCaptureDevice != 0) {
								g_audioInCaptureDevice = 0;
								SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
							}
							configureAudioInCaptureBuffers();
							ksceSysconGetHardwareInfo2(hardwareInfo);
							configureAudioInAec(&g_audioInAecContext,
								(hardwareInfo[0] & 4) ? g_audioInInternalAecControl :
								g_audioInAlternateAecControl);
							g_audioInChannelSelector = channel;
						}
						break;
					case 1:
					case 2:
						channel = SceCodecForDriver_C1DB7BAF(input);
						if (channel >= 0) {
							if (g_audioInCaptureDevice != 0) {
								g_audioInCaptureDevice = 0;
								SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
							}
							configureAudioInCaptureBuffers();
							configureAudioInAec(&g_audioInAecContext, g_audioInExternalAecControl);
							g_audioInChannelSelector = channel;
						}
						break;
					case 16:
						initializeAudioInLegacyQueue();
						if (g_audioInCaptureDevice != 1) {
							g_audioInCaptureDevice = 1;
							SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
						}
						break;
					case 32:
						if (g_audioInCaptureDevice != 2) {
							g_audioInCaptureDevice = 2;
							SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
						}
						configureAudioInCaptureBuffers();
						g_audioInChannelSelector = 0;
						break;
					}
					if (g_audioInDeviceStateEnabled) {
						g_audioInDeviceChangeCountdown = 2;
						if (!g_audioInOwnerUnmuteDelay)
							g_audioInOwnerUnmuteDelay = 16;
					}
				}
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
			}
			SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~0x200u);
		}
		if (g_audioInCaptureDevice == 0 || g_audioInCaptureDevice == 2) {
			AudioInCaptureBuffer *buffer;
			unsigned int oldCursor, newCursor, grain;

			g_audioInReadyViews = 0;
			if (g_audioInRequestFlags & 0x10000) {
				g_audioInStartupSilenceFrames = 2;
				SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~0x10000u);
			}
			if (g_audioInCaptureFreq == 48000) {
				oldCursor = g_audioInInputBuffer.writeCursor;
				grain = g_audioInCaptureGrain;
				newCursor = oldCursor + grain;
				g_audioInInputBuffer.writeCursor = newCursor;
				if (newCursor == 3072)
					newCursor = 0;
				if (SceAudioForDriver_75915003((uintptr_t)
						(g_audioInInputBuffer.samples + 2 * newCursor), grain) < 0)
					sceKernelDelayThread(32000);
				if (g_audioInStartupSilenceFrames) {
					memset(g_audioInInputBuffer.samples + 2 * oldCursor, 0,
						4 * g_audioInCaptureGrain);
					g_audioInStartupSilenceFrames--;
				}
				g_audioInSampleViews[0].samples = g_audioInInputBuffer.samples + 2 * newCursor;
				g_audioInSampleViews[0].sampleCount = g_audioInCaptureGrain;
				g_audioInSampleViews[0].channels = 2;
				g_audioInReadyViews |= 1;
			}
#define CAPTURE_MONO(member, view, bit, channel, rate, stream) do { \
	buffer = &g_audioInBuffers.member; \
	if (buffer->inputGrain && buffer->inputGrain == \
	    g_audioInInputBuffer.writeCursor - buffer->readCursor) { \
		int16_t *destination = buffer->samples + buffer->writeCursor; \
		const int16_t *source = g_audioInInputBuffer.samples + 2 * buffer->readCursor; \
		if ((rate) == 48000 || g_audioInCaptureFreq == (rate)) \
			copyAudioInChannel(destination, source, buffer->outputGrain, (channel)); \
		else if ((rate) == 16000 && g_audioInCaptureFreq == 48000) \
			resampleAudioInCapture((stream), source, destination, 3 * buffer->outputGrain, (channel)); \
		buffer->readCursor += buffer->inputGrain; \
		buffer->writeCursor += buffer->outputGrain; \
		g_audioInSampleViews[view].samples = destination; \
		g_audioInSampleViews[view].sampleCount = buffer->outputGrain; \
		g_audioInSampleViews[view].channels = 1; \
		g_audioInReadyViews |= (bit); \
	} \
} while (0)
			CAPTURE_MONO(primary48, 1, 2, g_audioInChannelSelector, 48000, 0);
			CAPTURE_MONO(secondary48, 2, 4, 1 - g_audioInChannelSelector, 48000, 0);
			CAPTURE_MONO(primary16, 3, 8, g_audioInChannelSelector, 16000, 0);
			CAPTURE_MONO(secondary16, 4, 16, 1 - g_audioInChannelSelector, 16000, 1);
#undef CAPTURE_MONO
			buffer = &g_audioInVoice8Buffer;
			if (buffer->inputGrain && buffer->inputGrain ==
			    g_audioInInputBuffer.writeCursor - buffer->readCursor) {
				int16_t *destination = buffer->samples + buffer->writeCursor;

				if (g_audioInCaptureFreq == 8000)
					copyAudioInChannel(destination, g_audioInInputBuffer.samples +
						2 * buffer->readCursor, buffer->outputGrain, g_audioInChannelSelector);
				buffer->readCursor += buffer->inputGrain;
				buffer->writeCursor += buffer->outputGrain;
				/* Sony writes the secondary16 pointer/count here, then the8k channel count. */
				g_audioInSampleViews[4].samples = destination;
				g_audioInSampleViews[4].sampleCount = buffer->outputGrain;
				g_audioInSampleViews[5].channels = 1;
				g_audioInReadyViews |= 0x20;
			}
			oldCursor = g_audioInPrimary16Buffer.writeCursor;
			if (oldCursor && !(oldCursor & 511) && !(g_audioInAecFlags & 1)) {
				memcpy(g_audioInCombined16Samples + oldCursor - 512,
					g_audioInPrimary16Samples + oldCursor - 512, 1024);
				g_audioInAecSamples[1] = memcpy(g_audioInAecReferenceSamples + oldCursor - 512,
					g_audioInSecondary16Samples + oldCursor - 512, 1024);
				g_audioInAecSamples[0] = g_audioInCombined16Samples + oldCursor - 512;
				g_audioInCombined16Buffer.writeCursor =
					g_audioInPrimary16Buffer.writeCursor == 1024 ? 0 : g_audioInPrimary16Buffer.writeCursor;
				SceCpuForDriver_BC248C30(&g_audioInAecFlags, 256);
			}
			buffer = &g_audioInCombined16Buffer;
			buffer->pendingSamples += g_audioInCaptureGrain;
			if (buffer->pendingSamples == buffer->inputGrain) {
				oldCursor = buffer->writeCursor;
				g_audioInSampleViews[6].samples = buffer->samples + oldCursor;
				g_audioInSampleViews[7].samples = g_audioInPrimary16Buffer.samples + oldCursor;
				g_audioInSampleViews[8].samples = g_audioInSecondary16Buffer.samples + oldCursor;
				g_audioInSampleViews[6].sampleCount = buffer->outputGrain;
				g_audioInSampleViews[7].sampleCount = buffer->outputGrain;
				g_audioInSampleViews[8].sampleCount = buffer->outputGrain;
				buffer->writeCursor += buffer->outputGrain;
				if (buffer->writeCursor >= buffer->capacity)
					buffer->writeCursor = 0;
				buffer->pendingSamples = 0;
				g_audioInReadyViews |= 0x40;
			}
			if (g_audioInInputBuffer.writeCursor == 3072)
				g_audioInInputBuffer.writeCursor = 0;
#define WRAP_CAPTURE_BUFFER(member, size) do { \
	if (g_audioInBuffers.member.readCursor == 3072) \
		g_audioInBuffers.member.readCursor = 0; \
	if (g_audioInBuffers.member.writeCursor == (size)) \
		g_audioInBuffers.member.writeCursor = 0; \
} while (0)
			WRAP_CAPTURE_BUFFER(primary48, 1536);
			WRAP_CAPTURE_BUFFER(secondary48, 1536);
			WRAP_CAPTURE_BUFFER(primary16, 1024);
			WRAP_CAPTURE_BUFFER(secondary16, 1024);
			WRAP_CAPTURE_BUFFER(voice8, 256);
#undef WRAP_CAPTURE_BUFFER
		} else if (g_audioInCaptureDevice == 1) {
			unsigned int count = 0, timeout = 0, bits;
			unsigned int oldCursor = g_audioInPrimary16Buffer.writeCursor;
			int16_t *samples = g_audioInPrimary16Samples + oldCursor;
			int result = -1;

			if (g_audioInRequestFlags & 0x10000) {
				resetAudioInLegacyQueue();
				SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~0x10000u);
			}
			if (g_audioInCaptureFreq == 16000) {
				count = g_audioInCaptureGrain;
				timeout = 2000000 * count / 16000;
			} else if (g_audioInCaptureFreq == 48000) {
				count = g_audioInCaptureGrain / 3;
				timeout = 2000000 * count / 16000;
			} else if (g_audioInCaptureFreq == 8000) {
				count = 2 * g_audioInCaptureGrain;
				timeout = 4000000 * g_audioInCaptureGrain / 16000;
			}
			for (;;) {
				if (lockFastMutex((SceKernelFastMutex *)&g_audioInLock) < 0) {
					sceKernelDelayThread(timeout);
					break;
				}
				result = readAudioInLegacyQueue(samples, count / 2);
				sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
				if (result >= 0)
					break;
				if (sceKernelWaitEventFlag(g_audioInRequestEventFlag, 12, 5, &bits, &timeout) < 0)
					break;
			}
			if (result < 0)
				memset(samples, 0, 2 * count);
			g_audioInPrimary16Buffer.writeCursor += count;
			if (g_audioInPrimary16Buffer.readCursor == 3072)
				g_audioInPrimary16Buffer.readCursor = 0;
			if (g_audioInPrimary16Buffer.writeCursor == 1024)
				g_audioInPrimary16Buffer.writeCursor = 0;
			g_audioInReadyViews = 0;
			g_audioInSampleViews[3].samples = samples;
			g_audioInSampleViews[6].samples = samples;
			g_audioInSampleViews[7].samples = samples;
			g_audioInSampleViews[8].samples = (int16_t *)g_audioInSilenceSamples;
			if (g_audioInSecondary16Buffer.inputGrain)
				g_audioInSampleViews[4].samples = (int16_t *)g_audioInSilenceSamples;
			if (g_audioInPrimary48Buffer.inputGrain) {
				for (i = 0; (int)i < (int)count; i++) {
					g_audioInPrimary48Samples[3 * i] = samples[i];
					g_audioInPrimary48Samples[3 * i + 1] = samples[i];
					g_audioInPrimary48Samples[3 * i + 2] = samples[i];
				}
				g_audioInSampleViews[1].samples = g_audioInPrimary48Samples;
			}
			g_audioInLegacyCaptureControl = 0;
#define LEGACY_READY(member, view, bit) do { \
	g_audioInBuffers.member.pendingSamples += g_audioInCaptureGrain; \
	if (g_audioInBuffers.member.pendingSamples == g_audioInBuffers.member.inputGrain) { \
		g_audioInBuffers.member.pendingSamples = 0; \
		g_audioInSampleViews[view].sampleCount = g_audioInBuffers.member.outputGrain; \
		g_audioInSampleViews[view].channels = 1; \
		g_audioInReadyViews |= (bit); \
	} \
} while (0)
			LEGACY_READY(primary16, 3, 8);
			LEGACY_READY(secondary16, 4, 16);
			LEGACY_READY(combined16, 6, 64);
			if (g_audioInReadyViews & 64) {
				g_audioInSampleViews[7].sampleCount = g_audioInCombined16Buffer.outputGrain;
				g_audioInSampleViews[8].sampleCount = g_audioInCombined16Buffer.outputGrain;
				g_audioInSampleViews[7].channels = 1;
				g_audioInSampleViews[8].channels = 1;
			}
			LEGACY_READY(primary48, 1, 2);
			LEGACY_READY(voice8, 5, 32);
#undef LEGACY_READY
		}
		lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		for (i = 0; i < 4; i++) {
			AudioInPort *port = &g_audioInPorts[i];
			unsigned int count, channels;
			int result;

			if (!(port->flags & 4))
				continue;
#define DELIVER_CAPTURE_VIEW(index, bit) do { \
	if (g_audioInReadyViews & (bit)) { \
		port->inputResult = 0; \
		count = g_audioInSampleViews[index].sampleCount; \
		channels = g_audioInSampleViews[index].channels; \
		result = copyAudioInToPort(port, port->destPtr, \
			g_audioInSampleViews[index].samples, 2 * count * channels); \
		if (result < 0) \
			port->inputResult = result; \
		goto delivered; \
	} \
} while (0)
			if (port->flags & 0x8000) {
				DELIVER_CAPTURE_VIEW(0, 1);
			} else if (port->flags & 0x2000) {
				if (port->freq == 16000)
					DELIVER_CAPTURE_VIEW(3, 8);
				else if (port->freq == 48000)
					DELIVER_CAPTURE_VIEW(1, 2);
			} else if (port->flags & 0x4000) {
				if (port->freq == 16000)
					DELIVER_CAPTURE_VIEW(4, 16);
				else if (port->freq == 48000)
					DELIVER_CAPTURE_VIEW(2, 4);
			} else if (port->flags & 0x1000) {
				if (port->freq == 8000) {
					port->inputResult = 0;
					count = g_audioInSampleViews[6].sampleCount >> 1;
					channels = 1;
					result = copyAudioInToPort(port, port->destPtr,
						g_audioInCombined8Samples, 2 * count);
					if (result < 0)
						port->inputResult = result;
					goto delivered;
				} else if (port->freq == 16000 && (g_audioInReadyViews & 64)) {
#define DELIVER_AUXILIARY(index) do { \
	if (port->auxiliaryDestinations[index]) { \
		copyAudioInToPort(port, port->auxiliaryDestinations[index], \
			g_audioInSampleViews[7 + (index)].samples, \
			2 * g_audioInSampleViews[7 + (index)].sampleCount); \
		port->auxiliaryDestinations[index] += g_audioInSampleViews[7 + (index)].sampleCount; \
	} \
} while (0)
					DELIVER_AUXILIARY(0);
					DELIVER_AUXILIARY(1);
#undef DELIVER_AUXILIARY
					DELIVER_CAPTURE_VIEW(6, 64);
				}
			}
#undef DELIVER_CAPTURE_VIEW
			continue;
delivered:
			port->transferredSamples += count;
			port->destPtr = (int16_t *)port->destPtr + count * channels;
			if (port->transferredSamples == port->grain) {
				port->flags &= ~4u;
				sceKernelSetEventFlag(port->eventFlag, 1);
				port->transferredSamples = 0;
			}
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		if (g_audioInAecFlags & 0x100) {
			sceKernelSetEventFlag(g_audioInAecEventFlag, 1);
			SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~0x100u);
		}
		lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		if (g_audioInOwnerUnmuteDelay) {
			g_audioInOwnerUnmuteDelay -= 16;
			if (g_audioInDeviceChangeCountdown && !g_audioInOwnerUnmuteDelay) {
				g_audioInOwnerUnmuteDelay = 16;
			} else if (g_audioInOwnerUnmuteDelay <= 0) {
				for (i = 0; i < 4; i++)
					if (g_audioInPorts[i].pid == g_audioInOwnerPid)
						g_audioInPorts[i].flags &= ~0x80u;
				g_audioInOwnerUnmuteDelay = 0;
			}
		}
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	}
}

int sceAudioInOpenPort(SceAudioInPortType portType, int grain, int freq,
		SceAudioInParam param)
{
	unsigned int userState = readTpidruro();
	unsigned int rawFlags, combinedFlags, stereoFlags;
	unsigned int flags;
	unsigned int index = 0, candidate, i;
	AudioInPort *port;
	char name[32];
	int pid, result;

	writeTpidruro(userState << 16);
	if (portType == 14)
		goto invalidPort;
	if (portType & 0x20) {
		rawFlags = 0x32000;
		combinedFlags = 0x31000;
		stereoFlags = 0x36000;
	} else {
		rawFlags = 0x2000;
		combinedFlags = 0x1000;
		stereoFlags = 0x6000;
	}
	flags = rawFlags;
	if ((unsigned int)portType >> 10)
		goto invalidPort;
	if (((unsigned int)portType & ~0x10u) == 2) {
		unsigned int valid16 = freq == 16000;
		unsigned int valid48 = freq == 48000;

		if (!((grain == 768 && freq == 48000) || (grain == 256 && freq == 16000))) {
			result = SCE_AUDIO_IN_ERROR_INVALID_SIZE;
			if (!(valid16 | valid48))
				result = SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ;
			goto done;
		}
	} else if (portType == 31) {
		if (freq != 48000)
			goto invalidFrequency;
		if (grain != 768)
			goto invalidSize;
		flags = stereoFlags;
	} else {
		if ((unsigned int)portType << 28)
			goto invalidPort;
		if (freq != 16000)
			goto invalidFrequency;
		if (grain != 512 && grain != 256)
			goto invalidSize;
		flags = combinedFlags;
	}
	if (param != SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO) {
		result = SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM;
		goto done;
	}
	pid = sceKernelGetProcessId();
	if (pid < 0) {
		result = pid;
		goto done;
	}
	lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	if (g_audioInPorts[0].pid == pid || g_audioInPorts[1].pid == pid ||
	    g_audioInPorts[2].pid == pid || g_audioInPorts[3].pid == pid) {
		result = SCE_AUDIO_IN_ERROR_PORT_FULL;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		goto done;
	}
	if (g_audioInPorts[0].flags & 2) {
		if (g_audioInPorts[1].flags & 2) {
			if (g_audioInPorts[2].flags & 2) {
				if (g_audioInPorts[3].flags & 2) {
					result = SCE_AUDIO_IN_ERROR_OUT_OF_MEMORY;
					sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
					goto done;
				}
				index = 3;
			} else {
				index = 2;
			}
		} else {
			index = 1;
		}
	}
	port = &g_audioInPorts[index];
	port->eventFlag = 0;
	port->flags = 1;
	port->unidentifiedInputControl = 0;
	port->unidentifiedCaptureControl = 0;
	port->pid = pid;
	port->grain = grain;
	port->freq = freq;
	candidate = (unsigned char)SceProcessmgrForDriver_02179E12() | 0x80;
	for (i = 0; i < 4; i++) {
		while (g_audioInPorts[i].port == candidate) {
			i = 0;
			candidate = (unsigned char)(candidate + 1) | 0x80;
		}
	}
	port->port = candidate;
	g_audioInOpenPortCount++;
	flags |= port->flags;
	port->gainControl = 4192;
	port->flags = flags;
	port->transferredSamples = 0;
	port->auxiliaryDestinations[0] = NULL;
	port->auxiliaryDestinations[1] = NULL;
	if (g_audioInOwnerPid == port->pid) {
		g_audioInDeviceStateEnabled = (flags & 0x10000) != 0;
		if (flags & 0x20000)
			SceCpuForDriver_BC248C30(&g_audioInAecFlags, 0x1000);
		else
			SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~0x1000u);
	}
	snprintf(name, sizeof(name), g_audioInProcessNameFormat, pid);
	port->eventFlag = sceKernelCreateEventFlag(name, 0x8000, 0, NULL);
	if (port->eventFlag < 0) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		result = port->eventFlag;
		goto done;
	}
	port->flags |= 2;
	if (pid != g_audioInOwnerPid && g_audioInOwnerPid != -2)
		port->flags |= 0x80;
	if (g_audioInPorts[0].flags & 2) {
		if (index != 0 || (g_audioInPorts[1].flags & 2))
			goto reconfigure;
	} else if (g_audioInPorts[1].flags & 2) {
		if (index != 1 || (g_audioInPorts[2].flags & 2))
			goto reconfigure;
		goto checkLastPort;
	}
	if (g_audioInPorts[2].flags & 2) {
		if (index != 2 || (g_audioInPorts[3].flags & 2))
			goto reconfigure;
		goto firstPort;
	}
checkLastPort:
	if ((g_audioInPorts[3].flags & 2) && index != 3)
		goto reconfigure;
firstPort:
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
	initializeAudioInCaptureBuffers();
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
	SceCpuForDriver_BC248C30(&g_audioInAecFlags, 0x2000);
	sceKernelSetEventFlag(g_audioInRequestEventFlag, 256);
	goto unlocked;
reconfigure:
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
unlocked:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	result = port->port;
	if (g_audioInPortOpened)
		g_audioInPortOpened(pid);
	goto done;
invalidSize:
	result = SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	goto done;
invalidFrequency:
	result = SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ;
	goto done;
invalidPort:
	result = SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
done:
	writeTpidruro(userState);
	return result;
}

int sceAudioInOpenPortForDiag(unsigned int portType, unsigned int grain,
		unsigned int freq, int param)
{
	unsigned int userState = readTpidruro();
	unsigned int rawFlags, combinedFlags, stereoFlags, diagnosticFlags;
	unsigned int flags;
	unsigned int index = 0, candidate, i;
	AudioInPort *port;
	char name[32];
	int pid, result;

	writeTpidruro(userState << 16);
	if (portType & 0x20) {
		rawFlags = 0x32000;
		diagnosticFlags = 0x3a000;
		combinedFlags = 0x31000;
		stereoFlags = 0x36000;
	} else {
		rawFlags = 0x2000;
		diagnosticFlags = 0xa000;
		combinedFlags = 0x1000;
		stereoFlags = 0x6000;
	}
	flags = rawFlags;
	if ((unsigned int)portType >> 10)
		goto invalidPort;
	if (((unsigned int)portType & ~0x10u) == 2) {
		unsigned int valid16 = freq == 16000;
		unsigned int valid48 = freq == 48000;

		if (!((grain == 768 && freq == 48000) || (grain == 256 && freq == 16000))) {
			result = SCE_AUDIO_IN_ERROR_INVALID_SIZE;
			if (!(valid16 | valid48))
				result = SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ;
			goto done;
		}
	} else if (portType == 31) {
		if (freq != 48000)
			goto invalidFrequency;
		if (grain != 768)
			goto invalidSize;
		flags = stereoFlags;
	} else if (!(portType << 28)) {
		if (freq != 16000)
			goto invalidFrequency;
		if (grain != 512 && grain != 256)
			goto invalidSize;
		flags = combinedFlags;
	} else if (portType == 14) {
		flags = diagnosticFlags;
	} else {
		goto invalidPort;
	}
	if (param != SCE_AUDIO_IN_PARAM_FORMAT_S16_MONO) {
		result = SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM;
		goto done;
	}
	pid = sceKernelGetProcessId();
	if (pid < 0) {
		result = pid;
		goto done;
	}
	lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	if (g_audioInPorts[0].pid == pid || g_audioInPorts[1].pid == pid ||
	    g_audioInPorts[2].pid == pid || g_audioInPorts[3].pid == pid) {
		result = SCE_AUDIO_IN_ERROR_PORT_FULL;
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		goto done;
	}
	if (g_audioInPorts[0].flags & 2) {
		if (g_audioInPorts[1].flags & 2) {
			if (g_audioInPorts[2].flags & 2) {
				if (g_audioInPorts[3].flags & 2) {
					result = SCE_AUDIO_IN_ERROR_OUT_OF_MEMORY;
					sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
					goto done;
				}
				index = 3;
			} else {
				index = 2;
			}
		} else {
			index = 1;
		}
	}
	port = &g_audioInPorts[index];
	port->eventFlag = 0;
	port->flags = 1;
	port->unidentifiedInputControl = 0;
	port->unidentifiedCaptureControl = 0;
	port->pid = pid;
	port->grain = grain;
	port->freq = freq;
	candidate = (unsigned char)SceProcessmgrForDriver_02179E12() | 0x80;
	for (i = 0; i < 4; i++) {
		while (g_audioInPorts[i].port == candidate) {
			i = 0;
			candidate = (unsigned char)(candidate + 1) | 0x80;
		}
	}
	port->port = candidate;
	g_audioInOpenPortCount++;
	flags |= port->flags;
	port->gainControl = 4192;
	port->flags = flags;
	port->transferredSamples = 0;
	port->auxiliaryDestinations[0] = NULL;
	port->auxiliaryDestinations[1] = NULL;
	if (g_audioInOwnerPid == port->pid) {
		g_audioInDeviceStateEnabled = (flags & 0x10000) != 0;
		if (flags & 0x20000)
			SceCpuForDriver_BC248C30(&g_audioInAecFlags, 0x1000);
		else
			SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~0x1000u);
	}
	snprintf(name, sizeof(name), g_audioInProcessNameFormat, pid);
	port->eventFlag = sceKernelCreateEventFlag(name, 0x8000, 0, NULL);
	if (port->eventFlag < 0) {
		sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		result = port->eventFlag;
		goto done;
	}
	port->flags |= 2;
	if (pid != g_audioInOwnerPid && g_audioInOwnerPid != -2)
		port->flags |= 0x80;
	if (g_audioInPorts[0].flags & 2) {
		if (index != 0 || (g_audioInPorts[1].flags & 2))
			goto reconfigure;
	} else if (g_audioInPorts[1].flags & 2) {
		if (index != 1 || (g_audioInPorts[2].flags & 2))
			goto reconfigure;
		goto checkLastPort;
	}
	if (g_audioInPorts[2].flags & 2) {
		if (index != 2 || (g_audioInPorts[3].flags & 2))
			goto reconfigure;
		goto firstPort;
	}
checkLastPort:
	if ((g_audioInPorts[3].flags & 2) && index != 3)
		goto reconfigure;
firstPort:
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
	initializeAudioInCaptureBuffers();
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 0x10000);
	SceCpuForDriver_BC248C30(&g_audioInAecFlags, 0x2000);
	sceKernelSetEventFlag(g_audioInRequestEventFlag, 256);
	goto unlocked;
reconfigure:
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
unlocked:
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	result = port->port;
	if (g_audioInPortOpened)
		g_audioInPortOpened(pid);
	goto done;
invalidSize:
	result = SCE_AUDIO_IN_ERROR_INVALID_SIZE;
	goto done;
invalidFrequency:
	result = SCE_AUDIO_IN_ERROR_INVALID_SAMPLE_FREQ;
	goto done;
invalidPort:
	result = SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
done:
	writeTpidruro(userState);
	return result;
}
