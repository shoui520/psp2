/* Audio input capture implementation; original source filename unavailable. */
#include "audioin_internal.h"
#include <sysmem/cp15.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <psp2/audioin.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem/data_transfers.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <syscon/syscon.h>
#include <psp2kern/kernel/aimgr.h>

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
extern unsigned int ksceKernelGetProcessTimeLowCore(void);

int audioInReturnZero(void)
{
	return 0;
}

static inline __attribute__((always_inline)) unsigned int
captureGrainGcd(unsigned int current, unsigned int grain)
{
	while (grain != 0) {
		unsigned int remainder = current % grain;

		current = grain;
		grain = remainder;
	}
	return current;
}

unsigned int configureAudioInCaptureBuffers(void)
{
	AudioInPort *port = g_audioInPorts;
	unsigned int grain8 = 128, grain16 = 256, grain48 = 768;
	unsigned int primary48 = 0, secondary48 = 0;
	unsigned int primary16 = 0, secondary16 = 0, combined16 = 0, voice8 = 0;
	unsigned int result, outputGrain;

	do {
		unsigned int flags = port->flags;
		unsigned int grain;

		if (!(flags & 1) || (flags & 0x40))
			continue;
		if (port->freq == 16000) {
			grain = port->grain;
			grain48 = captureGrainGcd(grain48, 3 * grain);
			grain16 = captureGrainGcd(grain16, grain);
			if (flags & 0x2000)
				primary16++;
			else if (flags & 0x4000)
				secondary16++;
			else if (flags & 0x1000) {
				combined16++;
				primary16++;
				secondary16++;
			}
		} else if (port->freq == 48000) {
			grain = port->grain;
			grain48 = captureGrainGcd(grain48, grain);
			if (flags & 0x2000)
				primary48++;
			else if (flags & 0x4000)
				secondary48++;
		} else if (port->freq == 8000) {
			if (flags & 0x1000) {
				grain = port->grain;
				grain48 = captureGrainGcd(grain48, 6 * grain);
				grain16 = captureGrainGcd(grain16, 2 * grain);
				combined16++;
				primary16++;
				secondary16++;
			} else {
				grain = port->grain;
				grain48 = captureGrainGcd(grain48, 6 * grain);
				grain16 = captureGrainGcd(grain16, 2 * grain);
				grain8 = captureGrainGcd(grain8, grain);
				voice8++;
			}
		}
	} while (++port != g_audioInPorts + 4);

	result = grain48;
	outputGrain = grain48;
	if (primary48 | secondary48) {
		g_audioInCaptureGrain = grain48;
		g_audioInCaptureFreq = 48000;
		g_audioInInputBuffer.capacity = 6144;
		g_audioInInputBuffer.inputGrain = grain48;
		g_audioInInputBuffer.writeCursor = 0;
		g_audioInInputBuffer.readCursor = 0;
		g_audioInInputBuffer.outputGrain = grain48;
		g_audioInInputBuffer.pendingSamples = 0;
		g_audioInInputBuffer.samples = g_audioInInputSamples;
		if (primary48) {
			g_audioInPrimary48Buffer.writeCursor = 0;
			g_audioInPrimary48Buffer.readCursor = 0;
			g_audioInPrimary48Buffer.samples = g_audioInPrimary48Samples;
			g_audioInPrimary48Buffer.inputGrain = grain48;
			g_audioInPrimary48Buffer.outputGrain = grain48;
			g_audioInPrimary48Buffer.pendingSamples = 0;
			g_audioInPrimary48Buffer.capacity = 1536;
		}
		if (secondary48) {
			g_audioInSecondary48Buffer.inputGrain = grain48;
			g_audioInSecondary48Buffer.outputGrain = grain48;
			g_audioInSecondary48Buffer.writeCursor = 0;
			g_audioInSecondary48Buffer.readCursor = 0;
			g_audioInSecondary48Buffer.pendingSamples = 0;
			g_audioInSecondary48Buffer.capacity = 1536;
			g_audioInSecondary48Buffer.samples = g_audioInSecondary48Samples;
		}
		if (primary16 | secondary16 | combined16) {
			if (grain48 == 0)
				goto configure16Input;
			goto configure16;
		}
	} else {
		if (!(primary16 | secondary16 | combined16)) {
			if (voice8)
				goto configure8Input;
			return result;
		}
configure16Input:
		g_audioInInputBuffer.samples = g_audioInInputSamples;
		g_audioInCaptureGrain = grain48;
		g_audioInInputBuffer.pendingSamples = 0;
		g_audioInInputBuffer.inputGrain = grain48;
		g_audioInInputBuffer.outputGrain = grain16;
		g_audioInInputBuffer.capacity = 6144;
		g_audioInCaptureFreq = 48000;
		g_audioInInputBuffer.writeCursor = 0;
		g_audioInInputBuffer.readCursor = 0;
configure16:
		if (primary16) {
			g_audioInPrimary16Buffer.inputGrain = grain48;
			g_audioInPrimary16Buffer.outputGrain = grain16;
			g_audioInPrimary16Buffer.pendingSamples = 0;
			g_audioInPrimary16Buffer.samples = g_audioInPrimary16Samples;
			g_audioInPrimary16Buffer.writeCursor = 0;
			g_audioInPrimary16Buffer.readCursor = 0;
			g_audioInPrimary16Buffer.capacity = 1024;
		}
		if (secondary16) {
			g_audioInSecondary16Buffer.inputGrain = grain48;
			g_audioInSecondary16Buffer.outputGrain = grain16;
			g_audioInSecondary16Buffer.samples = g_audioInSecondary16Samples;
			g_audioInSecondary16Buffer.capacity = 1024;
			g_audioInSecondary16Buffer.writeCursor = 0;
			g_audioInSecondary16Buffer.readCursor = 0;
			g_audioInSecondary16Buffer.pendingSamples = 0;
		}
		if (combined16) {
			g_audioInCombined16Buffer.inputGrain = grain48;
			g_audioInCombined16Buffer.outputGrain = grain16;
			g_audioInCombined16Buffer.capacity = 1024;
			g_audioInCombined16Buffer.samples = g_audioInCombined16Samples;
			g_audioInCombined16Buffer.writeCursor = 0;
			g_audioInCombined16Buffer.readCursor = 0;
			g_audioInCombined16Buffer.pendingSamples = 0;
		}
		outputGrain = grain16;
	}
	if (voice8) {
		if (result == 0) {
configure8Input:
			outputGrain = grain8;
			g_audioInInputBuffer.samples = g_audioInInputSamples;
			g_audioInCaptureGrain = result;
			g_audioInInputBuffer.pendingSamples = 0;
			g_audioInInputBuffer.inputGrain = result;
			g_audioInInputBuffer.outputGrain = grain8;
			g_audioInInputBuffer.capacity = 6144;
			g_audioInCaptureFreq = 48000;
			g_audioInInputBuffer.writeCursor = 0;
			g_audioInInputBuffer.readCursor = 0;
		}
		g_audioInVoice8Buffer.inputGrain = result;
		g_audioInVoice8Buffer.outputGrain = outputGrain;
		g_audioInVoice8Buffer.samples = g_audioInVoice8Samples;
		g_audioInVoice8Buffer.capacity = 256;
		g_audioInVoice8Buffer.writeCursor = 0;
		g_audioInVoice8Buffer.readCursor = 0;
		g_audioInVoice8Buffer.pendingSamples = 0;
	}
	return result;
}

int audioInProcessStopHandler(SceUID pid, int eventType,
		ProcEventParam *unusedParam, int unusedArgument)
{
	if (eventType == 512) {
#define NOTIFY_STOP(index) do { \
	if (pid == g_audioInPorts[index].pid && \
	    g_audioInPorts[index].eventFlag != 0) { \
		ksceKernelSetEventFlag(g_audioInPorts[index].eventFlag, 512); \
		g_audioInPorts[index].flags |= 0x40; \
	} \
} while (0)
		NOTIFY_STOP(0);
		NOTIFY_STOP(1);
		NOTIFY_STOP(2);
		NOTIFY_STOP(3);
#undef NOTIFY_STOP
	}
	return 0;
}
