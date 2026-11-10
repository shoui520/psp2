/*
 * libsas's SAS system on the AS audio system (sas_internal.c): a CORE with its BUSS, a SYNTH
 * of the voices, up to four PVERB reverbs, a MIXER of the dry and wet signals and the MASTER,
 * set up in one buffer; the API of sas.c works on the one the library holds.
 */
#ifndef SCE_SAS_INTERNAL_H
#define SCE_SAS_INTERNAL_H

#include <sas.h>

struct SceSasAsHandle;			/* as/as.h, which only libsas reads */

#define SCE_SAS_MAX_REVERBS		4

/* a voice as the API keeps it */
typedef struct SceSasVoice {
	SceUInt8 keyOn;
	SceUInt8 paused;
} SceSasVoice;

typedef struct SceSasSystem {
	char magic[4];				/* "SASH" */
	volatile SceUInt32 lock;		/* 1 while a call changes or renders it */
	void *buffer;				/* the buffer it was set up in */
	SceSize size;
	struct SceSasAsHandle *core;
	struct SceSasAsHandle *buss;
	struct SceSasAsHandle *synth;
	struct SceSasAsHandle *reverbs[SCE_SAS_MAX_REVERBS];
	struct SceSasAsHandle *mixer;
	struct SceSasAsHandle *master;
	SceSasVoice *voices;
	SceUInt16 numVoices;
	SceUInt16 grain;
	SceUInt16 numReverbs;
	SceUInt16 reserved;
	SceUInt32 outputmode;
	char endMagic[4];			/* "SASE" */
} SceSasSystem;

_Static_assert(sizeof(SceSasSystem) == 0x48, "SAS system size");

/* the exports the SDK's sas.h does not declare */
SceSasResult sceSasSetDistortion(SceInt32 iVoiceNum, SceUInt32 flag, SceUInt32 drive,
				 SceUInt32 gain);
SceSasResult sceSasGetDryPeak(SceInt32 *outL, SceInt32 *outR);
SceSasResult sceSasGetWetPeak(SceInt32 *outL, SceInt32 *outR);
SceSasResult sceSasGetPreMasterPeak(SceInt32 *outL, SceInt32 *outR);

SceSasResult sceSasCheckGrain(SceUInt32 grain);
SceSasResult sceSasGetNeededMemorySizeInternal(const char *config, SceSize *outSize);
SceSasResult sceSasInitInternal(const char *config, void *buffer, SceSize bufferSize,
				SceSasSystem **outSas);
SceSasResult sceSasExitInternal(SceSasSystem *sas, void **outBuffer, SceSize *outBufferSize);
SceSasResult sceSasSetGrainInternal(SceSasSystem *sas, SceUInt32 grain);
SceSasResult sceSasGetGrainInternal(SceSasSystem *sas);
SceSasResult sceSasSetOutputmodeInternal(SceSasSystem *sas, SceUInt32 outputmode);
SceSasResult sceSasGetOutputmodeInternal(SceSasSystem *sas);
SceSasResult sceSasCoreInternal(SceSasSystem *sas, SceInt16 *out, SceInt32 lvol, SceInt32 rvol);
SceSasResult sceSasSetVoiceInternal(SceSasSystem *sas, SceInt32 iVoiceNum, const void *vagBuf,
				    SceSize size, SceUInt32 loopflag);
SceSasResult sceSasSetVoicePCMInternal(SceSasSystem *sas, SceInt32 iVoiceNum, const void *pcmBuf,
				       SceSize size, SceInt32 loopsize);
SceSasResult sceSasSetNoiseInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 uClk);
SceSasResult sceSasSetVolumeInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceInt32 l,
				     SceInt32 r, SceInt32 wl, SceInt32 wr);
SceSasResult sceSasSetPitchInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceInt32 pitch);
SceSasResult sceSasSetDistortionInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
					 SceUInt32 drive, SceUInt32 gain);
SceSasResult sceSasSetADSRInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
				   SceUInt32 ar, SceUInt32 dr, SceUInt32 sr, SceUInt32 rr);
SceSasResult sceSasSetADSRmodeInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 flag,
				       SceUInt32 am, SceUInt32 dm, SceUInt32 sm, SceUInt32 rm);
SceSasResult sceSasSetSLInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 sl);
SceSasResult sceSasSetSimpleADSRInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt16 adsr1,
					 SceUInt16 adsr2);
SceSasResult sceSasSetKeyOnInternal(SceSasSystem *sas, SceInt32 iVoiceNum);
SceSasResult sceSasSetKeyOffInternal(SceSasSystem *sas, SceInt32 iVoiceNum);
SceSasResult sceSasSetPauseInternal(SceSasSystem *sas, SceInt32 iVoiceNum, SceUInt32 pauseFlag);
SceSasResult sceSasGetEndStateInternal(SceSasSystem *sas, SceInt32 iVoiceNum);
SceSasResult sceSasGetPauseStateInternal(SceSasSystem *sas, SceInt32 iVoiceNum);
SceSasResult sceSasGetEnvelopeInternal(SceSasSystem *sas, SceInt32 iVoiceNum);
SceSasResult sceSasSetEffectInternal(SceSasSystem *sas, SceInt32 drySwitch, SceInt32 wetSwitch);
SceSasResult sceSasSetEffectTypeInternal(SceSasSystem *sas, SceInt32 type);
SceSasResult sceSasSetEffectVolumeInternal(SceSasSystem *sas, SceInt32 valL, SceInt32 valR);
SceSasResult sceSasSetEffectParamInternal(SceSasSystem *sas, SceUInt32 delayTime,
					  SceUInt32 feedback);
SceSasResult sceSasGetDryPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR);
SceSasResult sceSasGetWetPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR);
SceSasResult sceSasGetPreMasterPeakInternal(SceSasSystem *sas, SceInt32 *outL, SceInt32 *outR);

#endif
