/*
 * libsas's API: the one SAS system the library holds, set up by sceSasInit and taken down by
 * sceSasExit, and each call passed to it (sas_internal.c).
 */
#include "as/as.h"
#include "sas_internal.h"

/* the system's state: none, being set up, set up, being taken down */
#define SCE_SAS_STATE_NONE		0
#define SCE_SAS_STATE_INITIALIZING	1
#define SCE_SAS_STATE_READY		2
#define SCE_SAS_STATE_EXITING		3

static SceSasSystem *sceSasSystem;
static volatile SceUInt32 sceSasState;

SceSasResult sceSasGetNeededMemorySize(const char *config, SceSize *outSize)
{
	return sceSasGetNeededMemorySizeInternal(config, outSize);
}

__attribute__((noinline)) SceSasResult sceSasInit(const char *config, void *buffer,
						 SceSize bufferSize)
{
	SceUInt32 state;
	int result;

	state = sceSasAsCompareAndSwap(&sceSasState, SCE_SAS_STATE_NONE, SCE_SAS_STATE_INITIALIZING);
	if (state != SCE_SAS_STATE_NONE) {
		if (state == SCE_SAS_STATE_READY)
			return SCE_SAS_ERROR_ALRDYINIT;
		if (state == SCE_SAS_STATE_EXITING)
			return SCE_SAS_ERROR_BUSY;
		if (state == SCE_SAS_STATE_INITIALIZING)
			return SCE_SAS_ERROR_BUSY;
		return SCE_SAS_ERROR_FAIL;
	}
	result = sceSasInitInternal(config, buffer, bufferSize, &sceSasSystem);
	if (result >= 0)
		sceSasAsCompareAndSwap(&sceSasState, SCE_SAS_STATE_INITIALIZING,
				       SCE_SAS_STATE_READY);
	else
		sceSasAsCompareAndSwap(&sceSasState, SCE_SAS_STATE_INITIALIZING,
				       SCE_SAS_STATE_NONE);
	return result;
}

SceSasResult sceSasInitWithGrain(const char *config, SceUInt32 grain, void *buffer,
				 SceSize bufferSize)
{
	int result;

	result = sceSasCheckGrain(grain);
	if (result < 0)
		return result;
	result = sceSasInit(config, buffer, bufferSize);
	if (result < 0)
		return result;
	sceSasSetGrainInternal(sceSasSystem, grain);
	return 0;
}

SceSasResult sceSasExit(void **outBuffer, SceSize *outBufferSize)
{
	SceUInt32 state;

	state = sceSasAsCompareAndSwap(&sceSasState, SCE_SAS_STATE_READY, SCE_SAS_STATE_EXITING);
	if (state == SCE_SAS_STATE_READY) {
		sceSasExitInternal(sceSasSystem, outBuffer, outBufferSize);
		sceSasSystem = NULL;
		sceSasAsCompareAndSwap(&sceSasState, SCE_SAS_STATE_EXITING, SCE_SAS_STATE_NONE);
		return 0;
	}
	if (state == SCE_SAS_STATE_INITIALIZING)
		return SCE_SAS_ERROR_BUSY;
	if (state == SCE_SAS_STATE_NONE)
		return SCE_SAS_ERROR_NOTINIT;
	if (state == SCE_SAS_STATE_EXITING)
		return SCE_SAS_ERROR_BUSY;
	return SCE_SAS_ERROR_FAIL;
}

SceSasResult sceSasSetGrain(SceUInt32 grain)
{
	return sceSasSetGrainInternal(sceSasSystem, grain);
}

SceSasResult sceSasGetGrain(void)
{
	return sceSasGetGrainInternal(sceSasSystem);
}

SceSasResult sceSasSetOutputmode(SceUInt32 outputmode)
{
	return sceSasSetOutputmodeInternal(sceSasSystem, outputmode);
}

SceSasResult sceSasGetOutputmode(void)
{
	return sceSasGetOutputmodeInternal(sceSasSystem);
}

SceSasResult sceSasCore(SceInt16 *out)
{
	return sceSasCoreInternal(sceSasSystem, out, 0, 0);
}

SceSasResult sceSasCoreWithMix(SceInt16 *inOut, SceInt32 lvol, SceInt32 rvol)
{
	return sceSasCoreInternal(sceSasSystem, inOut, lvol, rvol);
}

SceSasResult sceSasSetVoice(SceInt32 iVoiceNum, const void *vagBuf, SceSize size,
			    SceUInt32 loopflag)
{
	return sceSasSetVoiceInternal(sceSasSystem, iVoiceNum, vagBuf, size, loopflag);
}

SceSasResult sceSasSetVoicePCM(SceInt32 iVoiceNum, const void *pcmBuf, SceSize size,
			       SceInt32 loopsize)
{
	return sceSasSetVoicePCMInternal(sceSasSystem, iVoiceNum, pcmBuf, size, loopsize);
}

SceSasResult sceSasSetNoise(SceInt32 iVoiceNum, SceUInt32 uClk)
{
	return sceSasSetNoiseInternal(sceSasSystem, iVoiceNum, uClk);
}

SceSasResult sceSasSetVolume(SceInt32 iVoiceNum, SceInt32 l, SceInt32 r, SceInt32 wl,
			     SceInt32 wr)
{
	return sceSasSetVolumeInternal(sceSasSystem, iVoiceNum, l, r, wl, wr);
}

SceSasResult sceSasSetPitch(SceInt32 iVoiceNum, SceInt32 pitch)
{
	return sceSasSetPitchInternal(sceSasSystem, iVoiceNum, pitch);
}

SceSasResult sceSasSetDistortion(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 drive,
				 SceUInt32 gain)
{
	return sceSasSetDistortionInternal(sceSasSystem, iVoiceNum, flag, drive, gain);
}

SceSasResult sceSasSetADSR(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 ar, SceUInt32 dr,
			   SceUInt32 sr, SceUInt32 rr)
{
	return sceSasSetADSRInternal(sceSasSystem, iVoiceNum, flag, ar, dr, sr, rr);
}

SceSasResult sceSasSetADSRmode(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 am, SceUInt32 dm,
			       SceUInt32 sm, SceUInt32 rm)
{
	return sceSasSetADSRmodeInternal(sceSasSystem, iVoiceNum, flag, am, dm, sm, rm);
}

SceSasResult sceSasSetSL(SceInt32 iVoiceNum, SceUInt32 sl)
{
	return sceSasSetSLInternal(sceSasSystem, iVoiceNum, sl);
}

SceSasResult sceSasSetSimpleADSR(SceInt32 iVoiceNum, SceUInt16 adsr1, SceUInt16 adsr2)
{
	return sceSasSetSimpleADSRInternal(sceSasSystem, iVoiceNum, adsr1, adsr2);
}

SceSasResult sceSasSetKeyOn(SceInt32 iVoiceNum)
{
	return sceSasSetKeyOnInternal(sceSasSystem, iVoiceNum);
}

SceSasResult sceSasSetKeyOff(SceInt32 iVoiceNum)
{
	return sceSasSetKeyOffInternal(sceSasSystem, iVoiceNum);
}

SceSasResult sceSasSetPause(SceInt32 iVoiceNum, SceUInt32 pauseFlag)
{
	return sceSasSetPauseInternal(sceSasSystem, iVoiceNum, pauseFlag);
}

SceSasResult sceSasGetEndState(SceInt32 iVoiceNum)
{
	return sceSasGetEndStateInternal(sceSasSystem, iVoiceNum);
}

SceSasResult sceSasGetPauseState(SceInt32 iVoiceNum)
{
	return sceSasGetPauseStateInternal(sceSasSystem, iVoiceNum);
}

SceSasResult sceSasGetEnvelope(SceInt32 iVoiceNum)
{
	return sceSasGetEnvelopeInternal(sceSasSystem, iVoiceNum);
}

SceSasResult sceSasSetEffect(SceInt32 drySwitch, SceInt32 wetSwitch)
{
	return sceSasSetEffectInternal(sceSasSystem, drySwitch, wetSwitch);
}

SceSasResult sceSasSetEffectType(SceInt32 type)
{
	return sceSasSetEffectTypeInternal(sceSasSystem, type);
}

SceSasResult sceSasSetEffectVolume(SceInt32 valL, SceInt32 valR)
{
	return sceSasSetEffectVolumeInternal(sceSasSystem, valL, valR);
}

SceSasResult sceSasSetEffectParam(SceUInt32 delayTime, SceUInt32 feedback)
{
	return sceSasSetEffectParamInternal(sceSasSystem, delayTime, feedback);
}

SceSasResult sceSasGetDryPeak(SceInt32 *outL, SceInt32 *outR)
{
	return sceSasGetDryPeakInternal(sceSasSystem, outL, outR);
}

SceSasResult sceSasGetWetPeak(SceInt32 *outL, SceInt32 *outR)
{
	return sceSasGetWetPeakInternal(sceSasSystem, outL, outR);
}

SceSasResult sceSasGetPreMasterPeak(SceInt32 *outL, SceInt32 *outR)
{
	return sceSasGetPreMasterPeakInternal(sceSasSystem, outL, outR);
}
