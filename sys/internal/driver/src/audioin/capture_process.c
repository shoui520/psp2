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

int audioInProcessKillHandler(SceUID pid, ProcEventParam *unusedParam,
		int unusedArgument)
{
	ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	CLOSE_PROCESS_PORT(0);
	CLOSE_PROCESS_PORT(1);
	CLOSE_PROCESS_PORT(2);
	CLOSE_PROCESS_PORT(3);
	if (g_audioInOwnerPid == pid)
		g_audioInOwnerPid = -2;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	return 0;
}

int audioInProcessStartHandler(SceUID pid, int unusedEventType,
		ProcEventParam *unusedParam, int unusedArgument)
{
#define RESTART_PROCESS_PORT(index) do { \
	if (pid == g_audioInPorts[index].pid && \
	    (g_audioInPorts[index].flags & 0x40)) { \
		g_audioInPorts[index].flags &= ~0x40u; \
		if (g_audioInPorts[index].eventFlag != 0) \
			ksceKernelClearEventFlag(g_audioInPorts[index].eventFlag, ~1807u); \
	} \
} while (0)
	RESTART_PROCESS_PORT(0);
	RESTART_PROCESS_PORT(1);
	RESTART_PROCESS_PORT(2);
	RESTART_PROCESS_PORT(3);
#undef RESTART_PROCESS_PORT
	ksceKernelAtomicOrAndGet32(&g_audioInRequestFlags, 256);
	ksceKernelSetEventFlag(g_audioInRequestEventFlag, 256);
	return 0;
}

int audioInAecThread(SceSize unusedSize, void *unusedArgument)
{
	int needed = 0;
	int gain = 0, alternateGain = 0;

	initializeAudioInAecLibrary();
	startAudioInAec(&g_audioInAecContext);
	g_audioInAecEnabled = 1;
	for (;;) {
		unsigned int flags, mode, clearMode, normalClearMode, modeWithInputFlag;
		int codecFlags, selectedGain;
		unsigned int requestFlags;

		ksceKernelWaitEventFlag(g_audioInAecEventFlag, 1, 5, NULL, NULL);
		ksceKernelAtomicOrAndGet32(&g_audioInAecFlags, 1);
		if (g_audioInAecEnabled != 0 && g_audioInCaptureDevice != 1) {
#define REQUIRE_AEC(index) do { \
	if ((g_audioInPorts[index].flags & 2) && \
	    !(g_audioInPorts[index].flags & 0x2000)) \
		needed = 1; \
} while (0)
			REQUIRE_AEC(0);
			REQUIRE_AEC(1);
			REQUIRE_AEC(2);
#undef REQUIRE_AEC
			if ((g_audioInPorts[3].flags & 2) &&
			    !(g_audioInPorts[3].flags & 0x2000)) {
				needed = 1;
				if (ksceSblAimgrIsGenuineDolce())
					goto complete;
			} else {
				if (!needed || ksceSblAimgrIsGenuineDolce())
					goto complete;
			}
			codecFlags = SceCodecForDriver_41CFFE48(&gain, &alternateGain);
			flags = g_audioInAecFlags;
			if (codecFlags & 0x1000) {
				mode = 2;
				normalClearMode = 0x22;
				clearMode = 0x32;
				modeWithInputFlag = 0x12;
				selectedGain = gain;
			} else {
				mode = 0;
				normalClearMode = 0x20;
				clearMode = 0x30;
				modeWithInputFlag = 0x10;
				selectedGain = alternateGain;
			}
			if (flags & 0x1000)
				mode = modeWithInputFlag;
			else
				clearMode = normalClearMode;
			requestFlags = g_audioInRequestFlags;
			gain = (unsigned int)selectedGain << 8;
			if (g_audioInAecFlags & 0x2000)
				mode = clearMode;
			if (!(requestFlags & 2))
				processAudioInAecBuffers(&g_audioInAecContext,
					g_audioInAecSamples[0], g_audioInAecSamples[1], gain, mode);
			if (g_audioInAecFlags & 0x2000) {
				memset(g_audioInAecSamples[0], 0, 1024);
				ksceKernelAtomicAndAndGet32(&g_audioInAecFlags, ~0x2000u);
			}
		}
complete:
		ksceKernelAtomicAndAndGet32(&g_audioInAecFlags, ~1u);
	}
}
