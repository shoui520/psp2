/*
 * ngs/ngs_top.h: the NGS system, rack, voice and patch types and functions, under Sony's names and
 * with Sony's definitions (SDK Release 03.570.011, target/include/ngs/ngs_top.h).
 */
#ifndef _SCE_NGS_NGS_TOP_H
#define _SCE_NGS_NGS_TOP_H

#include <scetypes.h>
#include <ngs/error.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_NGS_MAKE_PARAMS_ID(module_id, index, ver) \
	(((module_id) & 0x0000ffff) | (((ver) & 0x000000ff) << 16) | (((index) & 0x000000ff) << 24))

/* Define for block parameter setting */
#define SCE_NGS_MODULE_ALL_CHANNELS		(-1)
#define SCE_NGS_FAILED(r)			((r) != SCE_NGS_OK)
#define SCE_NGS_SUCCESS(r)			((r) == SCE_NGS_OK)
#define SCE_NGS_INVALID_HANDLE			(0)
#define SCE_NGS_MEMORY_ALIGN_SIZE		(16)

/* Maximum number of audio channels available to the system */
#define SCE_NGS_MAX_SYSTEM_CHANNELS		(2)
#define SCE_NGS_SYSTEM_FLAG_NO_MODULE_PARAM_CHECKING	(0)
/*
 * Each sceNgsVoiceUnlockParams and sceNgsVoiceSetParamsBlock call will validate the parameter
 * ranges.
 */
#define SCE_NGS_SYSTEM_FLAG_MODULE_PARAM_CHECKING	(1)
#define SCE_NGS_SYSTEM_FLAG_DEFAULT		(SCE_NGS_SYSTEM_FLAG_MODULE_PARAM_CHECKING)
/* Initialize Basic, resets play and pause flags. */
#define SCE_NGS_VOICE_INIT_BASE			(0)
/* Initialize routing to default settings (no active patches). */
#define SCE_NGS_VOICE_INIT_ROUTING		(1)
/* Initialize using Preset values, this will use the supplied voice preset (see sceNgsVoiceInit). */
#define SCE_NGS_VOICE_INIT_PRESET		(2)
#define SCE_NGS_VOICE_INIT_CALLBACKS		(4)	/* Initialize the callbacks per module */
/* Initialize everything */
#define SCE_NGS_VOICE_INIT_ALL \
	(SCE_NGS_VOICE_INIT_BASE | SCE_NGS_VOICE_INIT_CALLBACKS | SCE_NGS_VOICE_INIT_ROUTING | \
	 SCE_NGS_VOICE_INIT_PRESET)
#define SCE_NGS_VOICE_STATE_AVAILABLE		(0)	/* Voice available */
#define SCE_NGS_VOICE_STATE_ACTIVE		(1)	/* Voice currently active */
/* Voice is in key-off mode, but is still processing */
#define SCE_NGS_VOICE_STATE_FINALIZING		(4)
/* Voice is unloading (rack is being removed from system and therefore voice is unavailable) */
#define SCE_NGS_VOICE_STATE_UNLOADING		(8)

/* note, voice return flag may be bitwise OR'd with the following */
/* Voice has been requested to play, but has not yet started processing */
#define SCE_NGS_VOICE_STATE_PENDING		(16)
#define SCE_NGS_VOICE_STATE_PAUSED		(32)	/* Voice has been set into a pause state */
/* Voice has been requested to key-off but not yet started processing  */
#define SCE_NGS_VOICE_STATE_KEY_OFF		(64)
/* setup flag for patching, allocates patch on any free sub-patch for the given output */
#define SCE_NGS_VOICE_PATCH_AUTO_SUBINDEX	(-1)
#define SCE_NGS_MODULE_FLAG_BYPASSED		(2)
#define SCE_NGS_MODULE_FLAG_NOT_BYPASSED	(0)
#define SCE_NGS_NO_CALLBACK			(0)
#define SCE_NGS_SAMPLE_OFFSET_FROM_AT9_HEADER	(0x80000000)

struct SceNgsVoiceDefinition;

typedef SceUInt32 SceNgsModuleID;

typedef SceUInt32 SceNgsParamsID;

typedef SceUInt32 SceNgsHVoice;

typedef SceUInt32 SceNgsHPatch;

typedef SceUInt32 SceNgsHSynSystem;

typedef SceUInt32 SceNgsHRack;

typedef struct SceNgsModuleParamHeader {
	SceInt32 moduleId;	/* Module to set the parameters on */
	SceInt32 chan;	/* Channel index to set the parameters on or SCE_NGS_MODULE_ALL_CHANNELS */
} SceNgsModuleParamHeader;

typedef struct SceNgsParamsDescriptor {
	SceNgsParamsID id;	/* The unique identifier for this interface */
	SceUInt32 size;	/* Size of entire params data including this header */
} SceNgsParamsDescriptor;

typedef struct SceNgsBufferInfo {
	void *data;	/* Pointer to the data */
	SceUInt32 size;	/* Size of buffer */
} SceNgsBufferInfo;

typedef struct SceNgsVoicePreset {
	/*
	 * byte offset (from this struct) to char *null-terminated string for the preset name or 0
	 * (none)
	 */
	SceInt32 nNameOffset;
	SceUInt32 uNameLength;	/* length of name array */
	/*
	 * byte offset (from this struct) to SceNgsModuleParamHeader *header for module data in
	 * block data format or 0 (none)
	 */
	SceInt32 nPresetDataOffset;
	SceUInt32 uSizePresetData;	/* size of preset data */
	/* byte offset (from this struct) to SceUInt32 *array of module IDs to bypass or 0 (none) */
	SceInt32 nBypassFlagsOffset;
	SceUInt32 uNumBypassFlags;	/* number of elements in puBypassFlags array */
} SceNgsVoicePreset;

typedef struct SceNgsSystemInitParams {
	SceInt32 nMaxRacks;	/* Maximum number of Racks within the Stage */
	SceInt32 nMaxVoices;	/* Maximum number of active voices */
	/* PCM sample granularity (NGS will process and output PCM sample packets of this size) */
	SceInt32 nGranularity;
	SceInt32 nSampleRate;	/* Base sample rate */
	/* Maximum number of module types which are available for the whole system. */
	SceInt32 nMaxModules;
} SceNgsSystemInitParams;

typedef struct SceNgsRackDescription {
	/* Pointer to Voice Definition information */
	const struct SceNgsVoiceDefinition *pVoiceDefn;
	SceInt32 nVoices;	/* Maximum number of Voices within the Rack */
	/* Maximum number of audio channels for each voice (1=Mono, 2=Stereo) */
	SceInt32 nChannelsPerVoice;
	/*
	 * Maximum number of Voices (processed from other Racks) which can be patched to each input
	 */
	SceInt32 nMaxPatchesPerInput;
	SceInt32 nPatchesPerOutput;	/* Maximum patches from each Voice output */
	/* Pointer to user release data (when callback release method used) */
	void *pUserReleaseData;
} SceNgsRackDescription;

typedef struct SceNgsPatchSetupInfo {
	SceNgsHVoice hVoiceSource;	/* Source Voice handle */
	/* Source output index (0 to n-1, where n = max outputs of the Source) */
	SceInt32 nSourceOutputIndex;
	/*
	 * Source output sub index (SCE_NGS_VOICE_PATCH_AUTO_SUBINDEX or 0 to nPatchesPerOutput,
	 * see SceNgsRackDescription)
	 */
	SceInt32 nSourceOutputSubIndex;
	SceNgsHVoice hVoiceDestination;	/* Destination Voice handle */
	/* Destination input index (0 to n-1, where n=max inputs of the Target voice) */
	SceInt32 nTargetInputIndex;
} SceNgsPatchSetupInfo;

typedef struct SceNgsVolumeMatrix {
	/* matrix, [out channels][in channels] */
	SceFloat32 m[SCE_NGS_MAX_SYSTEM_CHANNELS][SCE_NGS_MAX_SYSTEM_CHANNELS];
} SceNgsVolumeMatrix;

typedef struct SceNgsPatchRouteInfo {
	SceInt32 nOutputChannels;	/* Number of output channels from Source Voice(0-n) */
	SceInt32 nInputChannels;	/* Number of input channels to Destination Voice(0-n) */
	SceNgsVolumeMatrix vols;	/* Volume Matrix for Patch */
} SceNgsPatchRouteInfo;

typedef struct SceNgsVoiceInfo {
	SceUInt32 uVoiceState;	/* see SCE_NGS_VOICE_STATE_... */
	SceUInt32 uNumModules;	/* number of modules in voice definition */
	SceUInt32 uNumInputs;	/* number of inputs in voice definition */
	SceUInt32 uNumOutputs;	/* number of outputs in voice definition */
	SceUInt32 uNumPatchesPerOutput;	/* number of patch routes per output */
	SceUInt32 uUpdateCallsActive;	/* number of update calls the voice has been active */
} SceNgsVoiceInfo;

typedef struct SceNgsCallbackInfo {
	SceNgsHVoice hVoiceHandle;
	SceNgsHRack hRackHandle;
	SceNgsModuleID uModuleID;
	SceInt32 nCallbackData;
	SceInt32 nCallbackData2;
	void *pCallbackPtr;
	void *pUserData;
} SceNgsCallbackInfo;

typedef struct SceNgsAT9SkipBufferInfo {
	SceInt32 nStartByteOffset;
	SceInt32 nNumBytes;
	SceInt16 nStartSkip;
	SceInt16 nEndSkip;
	SceInt32 nIsSuperPacket;
} SceNgsAT9SkipBufferInfo;

typedef void (*SceNgsCallbackFunc) (const SceNgsCallbackInfo *pCallbackInfo);

typedef SceNgsCallbackFunc SceNgsRackReleaseCallbackFunc;
typedef SceNgsCallbackFunc SceNgsModuleCallbackFunc;
typedef SceNgsCallbackFunc SceNgsParamsErrorCallbackFunc;

SceInt32 sceNgsSystemGetRequiredMemorySize(const SceNgsSystemInitParams *pSynthParams,
					   SceUInt32 *pnSize);
SceInt32 sceNgsSystemInit(void *pSynthSysMemory, const SceUInt32 uMemSize,
			  const SceNgsSystemInitParams *pSynthParams,
			  SceNgsHSynSystem *pSystemHandle);
SceInt32 sceNgsSystemUpdate(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemRelease(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemLock(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemUnlock(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemSetParamErrorCallback(SceNgsHSynSystem hSystemHandle,
					   const SceNgsParamsErrorCallbackFunc callbackFuncPtr);
SceInt32 sceNgsSystemSetFlags(SceNgsHSynSystem hSystemHandle, const SceUInt32 uSystemFlags);

SceInt32 sceNgsRackGetRequiredMemorySize(SceNgsHSynSystem hSystemHandle,
					 const SceNgsRackDescription *pRackDesc, SceUInt32 *pnSize);
SceInt32 sceNgsRackInit(SceNgsHSynSystem hSystemHandle, SceNgsBufferInfo *pRackBuffer,
			const SceNgsRackDescription *pRackDesc, SceNgsHRack *pRackHandle);
SceInt32 sceNgsRackGetVoiceHandle(SceNgsHRack hRackHandle, const SceUInt32 uIndex,
				  SceNgsHVoice *pVoiceHandle);
SceInt32 sceNgsRackRelease(SceNgsHRack hRackHandle,
			   const SceNgsRackReleaseCallbackFunc callbackFuncPtr);
SceInt32 sceNgsRackSetParamErrorCallback(SceNgsHRack hRackHandle,
					 const SceNgsParamsErrorCallbackFunc callbackFuncPtr);

SceInt32 sceNgsVoiceInit(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pPreset,
			 const SceUInt32 uInitFlags);

SceInt32 sceNgsVoicePlay(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceKeyOff(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceKill(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoicePause(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceResume(SceNgsHVoice hVoiceHandle);

SceInt32 sceNgsVoiceSetPreset(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pVoicePreset);
SceInt32 sceNgsVoiceLockParams(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
			       const SceNgsParamsID uParamsInterfaceId,
			       SceNgsBufferInfo *pParamsBuffer);
SceInt32 sceNgsVoiceUnlockParams(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule);
SceInt32 sceNgsVoiceSetParamsBlock(SceNgsHVoice hVoiceHandle,
				   const SceNgsModuleParamHeader *pParamData, const SceUInt32 uSize,
				   SceInt32 *pnErrorCount);

SceInt32 sceNgsVoiceBypassModule(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				 const SceUInt32 uBypassFlag);
SceInt32 sceNgsVoiceSetModuleCallback(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				      const SceNgsModuleCallbackFunc callbackFuncPtr,
				      void *pUserData);
SceInt32 sceNgsVoiceSetFinishedCallback(SceNgsHVoice hVoiceHandle,
					const SceNgsCallbackFunc callbackFuncPtr, void *pUserData);
SceInt32 sceNgsVoiceGetStateData(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule, void *pMem,
				 const SceUInt32 uMemSize);
SceInt32 sceNgsVoiceGetInfo(SceNgsHVoice hVoiceHandle, SceNgsVoiceInfo *pInfo);
SceInt32 sceNgsVoiceGetModuleType(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				  SceNgsModuleID *pModuleType);
SceInt32 sceNgsVoiceGetModuleBypass(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
				    SceUInt32 *puBypassFlag);
SceInt32 sceNgsVoiceGetParamsOutOfRange(SceNgsHVoice hVoiceHandle, const SceUInt32 uModule,
					char *pszMessageBuffer);

SceInt32 sceNgsPatchCreateRouting(const SceNgsPatchSetupInfo *pPatchInfo,
				  SceNgsHPatch *pPatchHandle);
SceInt32 sceNgsPatchGetInfo(SceNgsHPatch hPatchHandle, SceNgsPatchRouteInfo *pRouteInfo,
			    SceNgsPatchSetupInfo *pSetup);
SceInt32 sceNgsVoiceGetOutputPatch(SceNgsHVoice hVoiceHandle, const SceInt32 nOutputIndex,
				   const SceInt32 nSubIndex, SceNgsHPatch *pPatchHandle);
SceInt32 sceNgsPatchRemoveRouting(SceNgsHPatch hPatchHandle);
SceInt32 sceNgsVoicePatchSetVolume(SceNgsHPatch hPatchHandle, const SceInt32 nOutputChannel,
				   const SceInt32 nInputChannel, const SceFloat32 fVol);
SceInt32 sceNgsVoicePatchSetVolumes(SceNgsHPatch hPatchHandle, const SceInt32 nOutputChannel,
				    const SceFloat32 *pVolumes, const SceInt32 nVols);
SceInt32 sceNgsVoicePatchSetVolumesMatrix(SceNgsHPatch hPatchHandle,
					  const SceNgsVolumeMatrix *pMatrix);

SceInt32 sceNgsModuleGetNumPresets(SceNgsHSynSystem hSystemHandle, const SceNgsModuleID uModuleID,
				   SceUInt32 *puNumPresets);
SceInt32 sceNgsModuleGetPreset(SceNgsHSynSystem hSystemHandle, const SceNgsModuleID uModuleID,
			       const SceUInt32 uPresetIndex, SceNgsBufferInfo *pParamsBuffer);

SceInt32 sceNgsAT9GetSectionDetails(const SceInt32 nStartSampleOffset, const SceInt32 nNumSamples,
				    const SceInt32 nConfigData,
				    SceNgsAT9SkipBufferInfo *pAt9InfoBuffer);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NGS_NGS_TOP_H */
