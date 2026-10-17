/* Audio input ports; original source filename unavailable. */
#include "audioin_internal.h"
#include <sysmem/cp15.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>
#include <psp2/audioin.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/procevent.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <psp2kern/kernel/threadmgr/thread.h>

extern const SceKernelProcEventHandler g_audioInProcEventHandler;

int sceAudioInReleasePort(int port)
{
	unsigned int userState = readTpidruro();
	int pid, portPid, index;
	AudioInPort *entry;
	unsigned int count;
	void (*closed)(int pid);

	writeTpidruro(userState << 16);
	pid = sceKernelGetProcessId();
	if (pid < 0) {
		writeTpidruro(userState);
		return pid;
	}
	ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	portPid = g_audioInPorts[0].pid;
	if (portPid == pid && g_audioInPorts[0].port == port) {
		index = 0;
		goto found;
	}
	portPid = g_audioInPorts[1].pid;
	if (portPid == pid && g_audioInPorts[1].port == port) {
		index = 1;
		goto found;
	}
	portPid = g_audioInPorts[2].pid;
	if (portPid == pid && g_audioInPorts[2].port == port) {
		index = 2;
		goto found;
	}
	portPid = g_audioInPorts[3].pid;
	if (portPid == pid && g_audioInPorts[3].port == port) {
		index = 3;
		goto found;
	}
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	writeTpidruro(userState);
	return SCE_AUDIO_IN_ERROR_NOT_OPENED;
found:
	entry = &g_audioInPorts[index];
	if (entry->eventFlag) {
		if (entry->flags & 0x10) {
			entry->flags &= ~0x86u;
			ksceKernelSetEventFlag(entry->eventFlag, 1024);
		} else {
			ksceKernelDeleteEventFlag(entry->eventFlag);
			entry->eventFlag = 0;
		}
	}
	count = g_audioInOpenPortCount;
	closed = g_audioInPortClosed;
	entry->flags = 0;
	entry->pid = 0;
	g_audioInOpenPortCount = count - 1;
	entry->port = 0;
	if (closed)
		closed(portPid);
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	writeTpidruro(userState);
	return 0;
}

int sceAudioInInput(int port, void *destPtr)
{
	unsigned int userState = readTpidruro();
	unsigned int bits;
	AudioInPort *entry;
	int result, index, eventFlag;

	writeTpidruro(userState << 16);
	if (!g_audioInInitialized) {
		result = SCE_AUDIO_IN_ERROR_FATAL;
		goto done;
	}
	result = sceKernelGetProcessId();
	if (result < 0)
		goto done;
	ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	if (result == g_audioInPorts[0].pid && port == g_audioInPorts[0].port)
		index = 0;
	else if (result == g_audioInPorts[1].pid && port == g_audioInPorts[1].port)
		index = 1;
	else if (result == g_audioInPorts[2].pid && port == g_audioInPorts[2].port)
		index = 2;
	else if (result == g_audioInPorts[3].pid && port == g_audioInPorts[3].port)
		index = 3;
	else
		goto notOpened;
	entry = &g_audioInPorts[index];
	if (!(entry->flags & 1))
		goto notOpened;
	if (!destPtr) {
		result = SCE_AUDIO_IN_ERROR_INVALID_POINTER;
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		goto done;
	}
	entry->flags |= 4;
	entry->destPtr = destPtr;
	entry->auxiliaryDestinations[0] = NULL;
	entry->auxiliaryDestinations[1] = NULL;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	eventFlag = entry->eventFlag;
	entry->flags |= 0x10;
	result = ksceKernelWaitEventFlag(eventFlag, 1807, 5, &bits, NULL);
	entry->flags &= ~0x10u;
	if (bits & 0x400) {
		ksceKernelDeleteEventFlag(entry->eventFlag);
		entry->eventFlag = 0;
	}
	if ((g_audioInRequestFlags & 2) || result >= 0) {
		result = entry->inputResult;
		result &= result >> 31;
	}
	goto done;
notOpened:
	result = SCE_AUDIO_IN_ERROR_NOT_OPENED;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
done:
	writeTpidruro(userState);
	return result;
}

int sceAudioInInputWithInputDeviceState(int port, void *destPtr,
		int *inputDeviceState)
{
	unsigned int userState = readTpidruro();
	unsigned int bits;
	AudioInPort *entry;
	int result, index, eventFlag, deleteEvent;

	writeTpidruro(userState << 16);
	if (!g_audioInInitialized) {
		result = SCE_AUDIO_IN_ERROR_FATAL;
		goto done;
	}
	result = sceKernelGetProcessId();
	if (result < 0)
		goto done;
	ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	if (result == g_audioInPorts[0].pid && port == g_audioInPorts[0].port)
		index = 0;
	else if (result == g_audioInPorts[1].pid && port == g_audioInPorts[1].port)
		index = 1;
	else if (result == g_audioInPorts[2].pid && port == g_audioInPorts[2].port)
		index = 2;
	else if (result == g_audioInPorts[3].pid && port == g_audioInPorts[3].port)
		index = 3;
	else
		goto notOpened;
	entry = &g_audioInPorts[index];
	if (!(entry->flags & 1))
		goto notOpened;
	if (!destPtr) {
		result = SCE_AUDIO_IN_ERROR_INVALID_POINTER;
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		goto done;
	}
	entry->flags |= 4;
	entry->auxiliaryDestinations[0] = NULL;
	entry->auxiliaryDestinations[1] = NULL;
	entry->destPtr = destPtr;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	eventFlag = entry->eventFlag;
	entry->flags |= 0x10;
	result = ksceKernelWaitEventFlag(eventFlag, 1807, 5, &bits, NULL);
	deleteEvent = (bits & 0x400) != 0;
	entry->flags &= ~0x10u;
	if (deleteEvent) {
		ksceKernelDeleteEventFlag(entry->eventFlag);
		entry->eventFlag = 0;
	}
	if ((g_audioInRequestFlags & 2) || result >= 0) {
		result = g_audioInPorts[index].inputResult;
		if (result >= 0) {
			bits = 0;
			result = g_audioInDeviceStateEnabled;
			if (result) {
				if (g_audioInDeviceChangeCountdown > 0) {
					g_audioInDeviceChangeCountdown--;
					bits = 1;
				}
				result = sceKernelGetProcessId();
				if (result >= 0)
					result = sceKernelCopyToUserProc(result, inputDeviceState, &bits, 4);
			}
		}
	}
	goto done;
notOpened:
	result = SCE_AUDIO_IN_ERROR_NOT_OPENED;
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
done:
	writeTpidruro(userState);
	return result;
}

int sceAudioInGetAdopt(SceAudioInPortType portType)
{
	unsigned int userState = readTpidruro();
	int pid, result;

	writeTpidruro(userState << 16);
	if ((unsigned int)portType & ~2u) {
		result = SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
	} else {
		pid = sceKernelGetProcessId();
		if (pid == g_audioInOwnerPid)
			result = g_audioInPorts[0].pid == pid || g_audioInPorts[1].pid == pid ||
				g_audioInPorts[2].pid == pid || g_audioInPorts[3].pid == pid;
		else
			result = 0;
	}
	writeTpidruro(userState);
	return result;
}

int sceAudioInGetStatus(int select)
{
	unsigned int userState = readTpidruro();
	unsigned int sdkVersion;
	int result;

	writeTpidruro(userState << 16);
	if (select == 1) {
		result = g_audioInRequestFlags & 1;
		if (g_audioInRequestFlags & 1)
			result = 1;
		else if (SceSblAIMgrForDriver_71608CA3())
			result = g_audioInCaptureDevice != 1;
	} else {
		SceProcessmgrForDriver_D141C076(sceKernelGetProcessId(), &sdkVersion);
		if (sdkVersion >= 0x1800000)
			result = SCE_AUDIO_IN_ERROR_INVALID_PARAMETER;
		else
			result = SCE_AUDIO_IN_ERROR_INVALID_PORT_PARAM;
	}
	writeTpidruro(userState);
	return result;
}

int SceAudioInForDriver_FB46FA10(int pid)
{
	int present = pid == g_audioInPorts[0].pid;

	if (pid == g_audioInPorts[1].pid)
		present = 1;
	if (pid == g_audioInPorts[2].pid)
		present = 1;
	return g_audioInPorts[3].pid == pid ? 1 : present;
}

int SceAudioInForDriver_005A2A31(void)
{
	int present = g_audioInPorts[0].pid;

	if (present)
		present = 1;
	if (g_audioInPorts[1].pid)
		present = 1;
	if (g_audioInPorts[2].pid)
		present = 1;
	if (g_audioInPorts[3].pid)
		return 1;
	return present;
}

int SceAudioInForDriver_E4042F6F(int pid, int unusedArgument1,
		int unusedArgument2, int delay)
{
	ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	g_audioInOwnerPid = pid;
#define ACTIVATE_OWNER_PORT(index) do { \
	if (g_audioInPorts[index].pid == pid) { \
		g_audioInDeviceStateEnabled = 0; \
		g_audioInPorts[index].flags &= ~0x80u; \
		SceCpuForDriver_DF899E4B(&g_audioInAecFlags, ~0x1000u); \
		if (g_audioInPorts[index].flags & 0x10000) \
			g_audioInDeviceStateEnabled = 1; \
		if (g_audioInPorts[index].flags & 0x20000) \
			SceCpuForDriver_BC248C30(&g_audioInAecFlags, 0x1000); \
	} \
} while (0)
	ACTIVATE_OWNER_PORT(0);
	ACTIVATE_OWNER_PORT(1);
	ACTIVATE_OWNER_PORT(2);
	ACTIVATE_OWNER_PORT(3);
#undef ACTIVATE_OWNER_PORT
#define MUTE_OTHER_PORT(index) do { \
	if (g_audioInPorts[index].pid != pid && g_audioInPorts[index].pid != 0) \
		g_audioInPorts[index].flags |= 0x80; \
} while (0)
	MUTE_OTHER_PORT(0);
	MUTE_OTHER_PORT(1);
	MUTE_OTHER_PORT(2);
	MUTE_OTHER_PORT(3);
#undef MUTE_OTHER_PORT
	g_audioInOwnerUnmuteDelay = delay;
	if (delay) {
#define MUTE_OWNER_PORT(index) do { \
	if (g_audioInPorts[index].pid == pid) \
		g_audioInPorts[index].flags |= 0x80; \
} while (0)
		MUTE_OWNER_PORT(0);
		MUTE_OWNER_PORT(1);
		MUTE_OWNER_PORT(2);
		MUTE_OWNER_PORT(3);
#undef MUTE_OWNER_PORT
	}
	SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 256);
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	return 0;
}

int sceAudioInSetMute(int select)
{
	unsigned int userState = readTpidruro();

	writeTpidruro(userState << 16);
	if (select == 1) {
		select = SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 1);
	} else if (select == 2) {
		select = SceCpuForDriver_DF899E4B(&g_audioInRequestFlags, ~1u);
		writeTpidruro(userState);
		return select;
	}
	writeTpidruro(userState);
	return select;
}

int SceAudioInForDriver_3EC79053(void (*callback)(int pid))
{
	g_audioInPortOpened = callback;
	return 0;
}

int SceAudioInForDriver_197CAA1B(void (*callback)(int pid))
{
	g_audioInPortClosed = callback;
	return 0;
}

int module_start(SceSize unusedSize, const void *unusedArgument)
{
	int event, result;

	memset(g_audioInPorts, 0, sizeof(g_audioInPorts));
	ksceKernelInitializeFastMutex((SceKernelFastMutex *)&g_audioInLock,
			g_audioInMutexName, 2, NULL);
	g_audioInCaptureDevice = 0;
	g_audioInRequestedInput = 2;
	event = ksceKernelCreateEventFlag(g_audioInRequestEventName, 0, 0, NULL);
	g_audioInRequestEventFlag = event;
	if (event < 0)
		goto failed;
	event = ksceKernelCreateEventFlag(g_audioInAecRequestEventName, 0, 0, NULL);
	g_audioInAecEventFlag = event;
	if (event < 0)
		goto failed;
	event = ksceKernelCreateThread(g_audioInInputThreadName, audioInCaptureThread,
			19, 4096, 0, 0x80000, NULL);
	g_audioInInputThreadId = event;
	if (event < 0)
		goto failed;
	result = ksceKernelStartThread(event, 0, NULL);
	if (result < 0)
		return result;
	event = ksceKernelCreateThread(g_audioInAecThreadName, audioInAecThread,
			19, 4096, 0, 0x80000, NULL);
	g_audioInAecThreadId = event;
	if (event < 0)
		goto failed;
	result = ksceKernelStartThread(event, 0, NULL);
	if (result < 0)
		return result;
	sceKernelRegisterSysEventHandler(g_audioInModuleName, audioInSysEventHandler, NULL);
	sceKernelRegisterProcEventHandler(g_audioInModuleName, &g_audioInProcEventHandler, 0);
	g_audioInOwnerPid = -2;
	g_audioInInitialized = 1;
	g_audioInRequestFlags = 0;
	g_audioInOpenPortCount = 0;
	g_audioInAppliedInput = -1;
	initializeAudioInCaptureBuffers();
	g_audioInSampleViews[5].samples = g_audioInVoice8Buffer.samples;
	g_audioInSampleViews[3].samples = g_audioInPrimary16Buffer.samples;
	g_audioInSampleViews[6].samples = g_audioInCombined16Buffer.samples;
	g_audioInSampleViews[7].samples = g_audioInPrimary16Buffer.samples;
	g_audioInSampleViews[0].channels = 2;
	g_audioInSampleViews[1].channels = 1;
	g_audioInSampleViews[2].channels = 1;
	g_audioInSampleViews[3].channels = 1;
	g_audioInSampleViews[4].channels = 1;
	g_audioInSampleViews[5].channels = 1;
	g_audioInSampleViews[6].channels = 1;
	g_audioInSampleViews[7].channels = 1;
	g_audioInSampleViews[8].channels = 1;
	g_audioInUnidentifiedState = 0;
	g_audioInSampleViews[0].samples = g_audioInInputBuffer.samples;
	g_audioInSampleViews[1].samples = g_audioInPrimary48Buffer.samples;
	g_audioInSampleViews[2].samples = g_audioInSecondary48Buffer.samples;
	g_audioInSampleViews[4].samples = g_audioInSecondary16Buffer.samples;
	g_audioInSampleViews[8].samples = g_audioInSecondary16Buffer.samples;
	return 0;
failed:
	return event;
}

int sceAudioInSelectInput(int input)
{
	unsigned int userState = readTpidruro();
	int result, event;

	writeTpidruro(userState << 16);
	if (g_audioInAppliedInput == input) {
		result = 0;
	} else {
		g_audioInRequestedInput = input;
		SceCpuForDriver_BC248C30(&g_audioInRequestFlags, 512);
		event = g_audioInAppliedInput == 16 ? 264 : 256;
		result = ksceKernelSetEventFlag(g_audioInRequestEventFlag, event);
	}
	writeTpidruro(userState);
	return result;
}

int sceAudioInGetInput(void)
{
	unsigned int userState = readTpidruro();

	writeTpidruro(userState << 16);
	writeTpidruro(userState);
	return g_audioInAppliedInput;
}

int SceAudioInForDriver_3D164387(const int16_t *samples, int count)
{
	int result = ksceKernelLockFastMutex((SceKernelFastMutex *)&g_audioInLock);

	if (result < 0)
		return result;
	if (g_audioInCaptureDevice == 1) {
		writeAudioInLegacyQueue(samples, count);
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
		return ksceKernelSetEventFlag(g_audioInRequestEventFlag, 4);
	}
	ksceKernelUnlockFastMutex((SceKernelFastMutex *)&g_audioInLock);
	return -1;
}

int sceAudioInSetMicGain(int port, int gain)
{
	unsigned int userState = readTpidruro();
	int result, index;

	writeTpidruro(userState << 16);
	if (!g_audioInInitialized) {
		result = SCE_AUDIO_IN_ERROR_FATAL;
		goto finished;
	}
	result = sceKernelGetProcessId();
	if (result < 0)
		goto finished;
#define FIND_GAIN_PORT(n) \
	if (g_audioInPorts[n].pid == result && g_audioInPorts[n].port == port) { \
		index = n; \
		goto found; \
	}
	FIND_GAIN_PORT(0);
	FIND_GAIN_PORT(1);
	FIND_GAIN_PORT(2);
	FIND_GAIN_PORT(3);
#undef FIND_GAIN_PORT
	result = SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
	goto finished;
found:
	result = 0;
	g_audioInPorts[index].gainControl = gain;
finished:
	writeTpidruro(userState);
	return result;
}

int sceAudioInGetMicGain(int port)
{
	unsigned int userState = readTpidruro();
	int result, index;

	writeTpidruro(userState << 16);
	if (!g_audioInInitialized) {
		result = SCE_AUDIO_IN_ERROR_FATAL;
		goto finished;
	}
	result = sceKernelGetProcessId();
	if (result < 0)
		goto finished;
#define FIND_GAIN_PORT(n) \
	if (g_audioInPorts[n].pid == result && g_audioInPorts[n].port == port) { \
		index = n; \
		goto found; \
	}
	FIND_GAIN_PORT(0);
	FIND_GAIN_PORT(1);
	FIND_GAIN_PORT(2);
	FIND_GAIN_PORT(3);
#undef FIND_GAIN_PORT
	result = SCE_AUDIO_IN_ERROR_INVALID_PORT_TYPE;
	goto finished;
found:
	result = (int16_t)g_audioInPorts[index].gainControl;
finished:
	writeTpidruro(userState);
	return result;
}
