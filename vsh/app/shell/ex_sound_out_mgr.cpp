/*
 * ex_sound_out_mgr.cpp: SceShellExSoundOutMgr — arbitrates exclusive audio output
 * between the camera shutter sound server and the MusicCore BGM player.
 * 3.65-CEX: 0x813C96E4..0x813C9915
 */
#include <stddef.h>
#include <scetypes.h>
#include <thread/thread.h>

extern "C" {
extern int sceAppMgrAcquireSoundOutExclusive2(int priority, SceUInt32 displayEnabled,
					      SceUInt32 displayMode, int displayRequest);
extern int sceAppMgrAcquireSoundOutExclusive3(int mode, int reserved, int flags,
					      int extraParam, int auxParam);
extern int sceAppMgrReleaseSoundOutExclusive2(void);
}

struct ShellSoundOutExclusiveStatus {
	SceUInt32 isExclusive;
	SceUInt32 activeOwner;
};

/* 0x815CC3D8, 0x81549350..0x8154936C */
static paf::thread::RMutex s_exSoundOutMutex("SceShellExSoundOutMgr", 0);
static SceUInt8 s_shutterHolding = 0;
static SceUInt8 s_musicCoreHolding = 0;
static SceUInt32 s_activeOwner = 0;
static int s_musicMode = 0;
static int s_musicReserved = 0;
static int s_musicFlags = 0;
static int s_musicExtraParam = 0;
static int s_musicAuxParam = 0;

/* 0x813C96E4 */
int shellSoundOutExclusiveGetStatus(ShellSoundOutExclusiveStatus *outStatus)
{
	s_exSoundOutMutex.Lock();
	SceUInt32 owner = s_activeOwner;
	outStatus->isExclusive = (owner != 0) ? 1U : 0U;
	outStatus->activeOwner = owner;
	s_exSoundOutMutex.Unlock();
	return 0;
}

/* 0x813C9714 */
int shellSoundOutExclusiveBegin(int displayRequest)
{
	s_exSoundOutMutex.Lock();
	int ret = sceAppMgrAcquireSoundOutExclusive2(0, 0, 1, displayRequest);
	if (ret == 0)
		s_activeOwner = 1;
	s_shutterHolding = 1;
	s_exSoundOutMutex.Unlock();
	return ret;
}

/* 0x813C9758 */
int shellSoundOutExclusiveEnd(void)
{
	s_exSoundOutMutex.Lock();
	int ret = 0;
	if (s_musicCoreHolding != 0) {
		ret = sceAppMgrAcquireSoundOutExclusive3(s_musicMode, s_musicReserved,
							 s_musicFlags, s_musicExtraParam,
							 s_musicAuxParam);
		if (ret == 0)
			s_activeOwner = 2;
	} else {
		ret = sceAppMgrReleaseSoundOutExclusive2();
		if (ret == 0)
			s_activeOwner = 0;
	}
	s_shutterHolding = 0;
	s_exSoundOutMutex.Unlock();
	return ret;
}

extern "C" {

/* 0x813C97F2 */
int sceShellAcquireSoundOutExclusive(int mode, int reserved, int flags, int extraParam,
				     int auxParam)
{
	s_exSoundOutMutex.Lock();
	s_musicMode = mode;
	s_musicReserved = reserved;
	s_musicFlags = flags;
	s_musicExtraParam = extraParam;
	s_musicAuxParam = auxParam;

	int ret = 0;
	if (s_shutterHolding == 0) {
		ret = sceAppMgrAcquireSoundOutExclusive3(mode, reserved, flags, extraParam,
							 auxParam);
		if (ret == 0)
			s_activeOwner = 2;
	}
	s_musicCoreHolding = 1;
	s_exSoundOutMutex.Unlock();
	return ret;
}

/* 0x813C989A */
int sceShellReleaseSoundOutExclusive(void)
{
	s_exSoundOutMutex.Lock();
	int ret = 0;
	if (s_shutterHolding == 0) {
		ret = sceAppMgrReleaseSoundOutExclusive2();
		if (ret == 0)
			s_activeOwner = 0;
	}
	s_musicCoreHolding = 0;
	s_exSoundOutMutex.Unlock();
	return ret;
}

} /* extern "C" */
