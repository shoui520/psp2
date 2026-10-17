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

extern int ksceSblAimgrIsGenuineDolce(void);
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

void *initializeAudioInCaptureBuffers(void)
{
	g_audioInInputBuffer.writeCursor = 0;
	g_audioInInputBuffer.readCursor = 0;
	g_audioInInputBuffer.outputGrain = 0;
	g_audioInInputBuffer.pendingSamples = 0;
	g_audioInInputBuffer.capacity = 6144;
	g_audioInInputBuffer.inputGrain = 0;
	g_audioInInputBuffer.samples = g_audioInInputSamples;
	memset(g_audioInInputSamples, 0, sizeof(g_audioInInputSamples));
	g_audioInPrimary48Buffer.samples = g_audioInPrimary48Samples;
	g_audioInPrimary48Buffer.writeCursor = 0;
	g_audioInPrimary48Buffer.readCursor = 0;
	g_audioInPrimary48Buffer.inputGrain = 0;
	g_audioInPrimary48Buffer.outputGrain = 0;
	g_audioInPrimary48Buffer.pendingSamples = 0;
	g_audioInPrimary48Buffer.capacity = 1536;
	memset(g_audioInPrimary48Samples, 0, sizeof(g_audioInPrimary48Samples));
	g_audioInSecondary48Buffer.pendingSamples = 0;
	g_audioInSecondary48Buffer.samples = g_audioInSecondary48Samples;
	g_audioInSecondary48Buffer.capacity = 1536;
	g_audioInSecondary48Buffer.writeCursor = 0;
	g_audioInSecondary48Buffer.readCursor = 0;
	g_audioInSecondary48Buffer.inputGrain = 0;
	g_audioInSecondary48Buffer.outputGrain = 0;
	memset(g_audioInSecondary48Samples, 0, sizeof(g_audioInSecondary48Samples));
	g_audioInPrimary16Buffer.writeCursor = 0;
	g_audioInPrimary16Buffer.readCursor = 0;
	g_audioInPrimary16Buffer.capacity = 1024;
	g_audioInPrimary16Buffer.samples = g_audioInPrimary16Samples;
	g_audioInPrimary16Buffer.inputGrain = 0;
	g_audioInPrimary16Buffer.outputGrain = 0;
	g_audioInPrimary16Buffer.pendingSamples = 0;
	memset(g_audioInPrimary16Samples, 0, sizeof(g_audioInPrimary16Samples));
	g_audioInSecondary16Buffer.samples = g_audioInSecondary16Samples;
	g_audioInSecondary16Buffer.writeCursor = 0;
	g_audioInSecondary16Buffer.readCursor = 0;
	g_audioInSecondary16Buffer.capacity = 1024;
	g_audioInSecondary16Buffer.inputGrain = 0;
	g_audioInSecondary16Buffer.outputGrain = 0;
	g_audioInSecondary16Buffer.pendingSamples = 0;
	memset(g_audioInSecondary16Samples, 0, sizeof(g_audioInSecondary16Samples));
	g_audioInVoice8Buffer.writeCursor = 0;
	g_audioInVoice8Buffer.readCursor = 0;
	g_audioInVoice8Buffer.inputGrain = 0;
	g_audioInVoice8Buffer.outputGrain = 0;
	g_audioInVoice8Buffer.pendingSamples = 0;
	g_audioInVoice8Buffer.capacity = 256;
	g_audioInVoice8Buffer.samples = g_audioInVoice8Samples;
	memset(g_audioInVoice8Samples, 0, sizeof(g_audioInVoice8Samples));
	g_audioInCombined16Buffer.capacity = 1024;
	g_audioInCombined16Buffer.writeCursor = 0;
	g_audioInCombined16Buffer.readCursor = 0;
	g_audioInCombined16Buffer.inputGrain = 0;
	g_audioInCombined16Buffer.outputGrain = 0;
	g_audioInCombined16Buffer.pendingSamples = 0;
	g_audioInCombined16Buffer.samples = g_audioInCombined16Samples;
	return memset(g_audioInCombined16Samples, 0, sizeof(g_audioInCombined16Samples));
}
