/* Audio input capture implementation; original source filename unavailable. */
#include "audioin_internal.h"
#include <sysmem/cp15.h>
#include <processmgr/processmgr.h>
#include <threadmgr/threadmgr.h>
#include <audioin.h>
#include <threadmgr/fast_mutex_object.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <syscon/syscon.h>
#include <sysmem/sblaimgr.h>

/* the kill callback's teardown of a port the process holds */
#define CLOSE_PROCESS_PORT(index) do { \
	if (pid == g_audioInPorts[index].pid) { \
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
		if (g_audioInPortClosed != 0) \
			g_audioInPortClosed(pid); \
	} \
} while (0)

int audioInProcessKillHandler(SceUID pid, ProcEventParam *unusedParam,
		void *common)
{
	lockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	CLOSE_PROCESS_PORT(0);
	CLOSE_PROCESS_PORT(1);
	CLOSE_PROCESS_PORT(2);
	CLOSE_PROCESS_PORT(3);
	if (g_audioInOwnerPid == pid)
		g_audioInOwnerPid = -2;
	sceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	return 0;
}

int audioInProcessStartHandler(SceUID pid, int unusedEventType,
		ProcEventParam *unusedParam, void *common)
{
#define RESTART_PROCESS_PORT(index) do { \
	if (pid == g_audioInPorts[index].pid && \
	    (g_audioInPorts[index].flags & 0x40)) { \
		g_audioInPorts[index].flags &= ~0x40u; \
		if (g_audioInPorts[index].eventFlag != 0) \
			sceKernelClearEventFlag(g_audioInPorts[index].eventFlag, ~1807u); \
	} \
} while (0)
	RESTART_PROCESS_PORT(0);
	RESTART_PROCESS_PORT(1);
	RESTART_PROCESS_PORT(2);
	RESTART_PROCESS_PORT(3);
#undef RESTART_PROCESS_PORT
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
	sceKernelSetEventFlag(g_audioInRequestEventFlag, 256);
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

		sceKernelWaitEventFlag(g_audioInAecEventFlag, 1, 5, NULL, NULL);
		SceCpuForDriver_BC248C30(&g_audioInAecFlags, 1);
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
				if (SceSblAIMgrForDriver_C6E83F34())
					goto complete;
			} else {
				if (!needed || SceSblAIMgrForDriver_C6E83F34())
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
				SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~0x2000u);
			}
		}
complete:
		SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~1u);
	}
}
