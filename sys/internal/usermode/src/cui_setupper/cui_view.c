/*
 * The progress view: each step's console line, the caller's callbacks, and SceSblSsUpdateMgr's
 * update status, where the system has it (its imports are weak). psp2swu links the same code.
 * No build names this file or its functions: the names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <std/string.h>
#include <update_mgr/update_mgr.h>
#include "cui_view.h"

#define CUI_VIEW_NAME_SIZE	64

/*
 * Each weak import's address, in a data word the loader leaves 0 where the system lacks the
 * import (Sony's compiler made the words; SNC 3.57 tests the address in the code, so they are
 * written out here).
 */
int (*cuiInformUpdateStarted)(int, int, const char *, SceSize) = _sceSblUsInformUpdateStarted;
int (*cuiInformUpdateOngoing)(int, int) = _sceSblUsInformUpdateOngoing;
int (*cuiInformUpdateFinished)(int, const char *, SceSize) = _sceSblUsInformUpdateFinished;
int (*cuiSetSwInfoInt)(const char *, SceSize, SceUInt32) = _sceSblUsSetSwInfoInt;
int (*cuiSetSwInfoStr)(const char *, SceSize, const char *, SceSize) = _sceSblUsSetSwInfoStr;
int (*cuiSetSwInfoBin)(const char *, SceSize, const void *, SceSize) = _sceSblUsSetSwInfoBin;

static SceInt32 s_progress;
static SceUInt8 s_finished;
static CuiStartedCallback s_startedCallback = CuiDefaultStartedCallback;
static CuiOngoingCallback s_ongoingCallback = CuiDefaultOngoingCallback;
static CuiFinishedCallback s_finishedCallback = CuiDefaultFinishedCallback;
char *cuiNameBuffer;

void CuiInformUpdateStarted(SceUInt32 phase, SceInt32 reserved, SceInt32 estimate,
			    const char *message, SceInt32 length)
{
	sceClibPrintf("[INFO]%s\n", message);
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	s_startedCallback(phase, reserved, message);
	s_progress = 0;
	if (cuiInformUpdateStarted != NULL) {
		if (length < 0)
			length = sce_paf_strlen(message);
		sceSblUsInformUpdateStarted(phase, estimate, message, length);
	}
}

void CuiInformUpdateOngoing(SceUInt32 phase, SceInt32 reserved, SceInt32 progress,
			    const char *message, SceInt32 length)
{
	if (message != NULL)
		sceClibPrintf("[INFO]%s\n", message);
	else
		sceClibPrintf(".");
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	s_ongoingCallback(phase, reserved, progress);
	if (cuiInformUpdateOngoing != NULL && s_progress < progress) {
		s_progress = progress;
		sceSblUsInformUpdateOngoing(phase, progress);
	}
}

void CuiInformUpdateFinished(SceInt32 result, const char *message, SceInt32 length)
{
	if (result != 0) {
		sceClibPrintf("[ERROR]%s: %08x\n", message, result);
		s_finishedCallback(result, message);
	} else {
		sceClibPrintf("[INFO]%s\n", message);
	}
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
	if (cuiInformUpdateFinished != NULL) {
		if (!s_finished) {
			if (length < 0)
				length = sce_paf_strlen(message);
			sceSblUsInformUpdateFinished(result, message, length);
			s_finished = 1;
		}
	} else {
		s_finished = 1;
	}
}

int CuiSetSwInfoInt(const char *name, SceSize nameLength, SceUInt32 value)
{
	SceSize length;

	if (cuiSetSwInfoInt == NULL)
		return 0;
	length = nameLength;
	if (cuiNameBuffer != NULL) {
		if (nameLength + 1 > CUI_VIEW_NAME_SIZE)
			length = CUI_VIEW_NAME_SIZE - 1;
		sce_paf_memcpy(cuiNameBuffer, name, length);
		cuiNameBuffer[length] = '\0';
		name = cuiNameBuffer;
	}
	return sceSblUsSetSwInfoInt(name, nameLength, value);
}

int CuiSetSwInfoStr(const char *name, SceSize nameLength, const char *value, SceSize valueLength)
{
	SceSize length;

	if (cuiSetSwInfoStr == NULL)
		return 0;
	length = nameLength;
	if (cuiNameBuffer != NULL) {
		if (nameLength + 1 > CUI_VIEW_NAME_SIZE)
			length = CUI_VIEW_NAME_SIZE - 1;
		sce_paf_memcpy(cuiNameBuffer, name, length);
		cuiNameBuffer[length] = '\0';
		name = cuiNameBuffer;
	}
	return sceSblUsSetSwInfoStr(name, nameLength, value, valueLength);
}

/* Sony's code checks for sceSblUsSetSwInfoBin, then sets the value with sceSblUsSetSwInfoStr */
int CuiSetSwInfoBin(const char *name, SceSize nameLength, const void *value, SceSize valueLength)
{
	SceSize length;

	if (cuiSetSwInfoBin == NULL)
		return 0;
	length = nameLength;
	if (cuiNameBuffer != NULL) {
		if (nameLength + 1 > CUI_VIEW_NAME_SIZE)
			length = CUI_VIEW_NAME_SIZE - 1;
		sce_paf_memcpy(cuiNameBuffer, name, length);
		cuiNameBuffer[length] = '\0';
		name = cuiNameBuffer;
	}
	return sceSblUsSetSwInfoStr(name, nameLength, value, valueLength);
}

void CuiSetCallbacks(CuiStartedCallback started, CuiOngoingCallback ongoing,
		     CuiFinishedCallback finished)
{
	s_startedCallback = started;
	s_ongoingCallback = ongoing;
	s_finishedCallback = finished;
}

int CuiDefaultStartedCallback(SceUInt32 phase, SceInt32 reserved, const char *message)
{
	return 0;
}

int CuiDefaultOngoingCallback(SceUInt32 phase, SceInt32 reserved, SceInt32 progress)
{
	return 0;
}

int CuiDefaultFinishedCallback(SceInt32 result, const char *message)
{
	return 0;
}
