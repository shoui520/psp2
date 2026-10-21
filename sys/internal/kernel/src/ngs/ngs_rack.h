/*
 * Racks, voices and patches. A rack holds the voices of one voice
 * definition; its system memory is mirrored into the caller's rack buffer
 * (the *Caller pointers are the same objects at the caller's address), and
 * its DSP block is read by the codec engine.
 */

#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <ngs/ngs_top.h>
#include <ngs/modules/at9_player.h>
#include <ngs/modules/input_mixer.h>
#include <ngs/modules/output.h>
#include <ngs/modules/pcm_player.h>

#include "ngs_system.h"
#include "ngs_voice_definition.h"

#define NGS_RACK_SIGNATURE		67892	/* 0x10934 */
#define NGS_VOICE_SIGNATURE		98232	/* 0x17fb8 */
#define NGS_PATCH_SIGNATURE		0x93b5
#define NGS_SYSTEM_SIGNATURE		54321

/* handles: system id, rack index, voice index and patch index, and a tag */
#define NGS_RACK_HANDLE_TAG		0x1beeba
#define NGS_VOICE_HANDLE_TAG		0x5bea
#define NGS_PATCH_HANDLE_TAG		0x5e

enum NgsRackState {
	NGS_RACK_STATE_ACTIVE = 1,
	NGS_RACK_STATE_RELEASING = 2,
};

enum NgsVoiceRunState {
	NGS_VOICE_RUN_STATE_IDLE = 0,
	NGS_VOICE_RUN_STATE_PLAYING = 1,
	NGS_VOICE_RUN_STATE_KEY_OFF = 2,
	NGS_VOICE_RUN_STATE_UNLOADING = 3,
};

struct NgsSynRack;
struct NgsVoice;

/* The rack's entry in the codec engine's rack list (rack->dspRackList). */
typedef struct NgsDspRackEntry {
	int queuedCount;		/* voices of the batch queued here */
	unsigned int processingIndex;	/* the rack's place in the batch */
	unsigned int *voices;		/* batch commands that wait for it */
	unsigned int voiceCapacity;
	unsigned int voiceCount;
	unsigned int rackIndex;
	unsigned int commandIndex;	/* its NGS_DSP_BATCH_RACK command */
	SceSize size;
	uintptr_t voicesPhysical;
	unsigned int voiceSlots[];
} NgsDspRackEntry;

/* One rack's view of another rack of the system (rack->links[other index]). */
typedef struct NgsRackLink {
	struct NgsSynRack *rack;
	int outputPatches;	/* patches from this rack's voices into the other's */
	int inputPatches;	/* patches from the other rack's voices into this */
} NgsRackLink;

/* A module of the rack's voice definition. */
typedef struct NgsRackModule {
	unsigned int moduleId;
	unsigned int inputCount;
	unsigned int outputCount;
	unsigned int *inputBindings;
	unsigned int *outputBindings;
} NgsRackModule;

/* The modules of the rack's voice definition. */
typedef struct NgsRackModuleList {
	unsigned int moduleCount;
	NgsRackModule *modules;
} NgsRackModuleList;

/* A preset of the rack's voice definition; offsets are from the entry. */
typedef struct NgsRackPreset {
	int nameOffset;
	unsigned int nameLength;
	int paramsOffset;
	unsigned int paramsSize;
	int bypassFlagsOffset;
	unsigned int bypassFlagCount;
} NgsRackPreset;

/* Where a module's state, context and parameters sit in a voice's DSP block. */
typedef struct NgsDspModuleLayout {
	unsigned int paramsOffset;
	unsigned int contextOffset;
	SceSize contextSize;
	unsigned int stateOffset;
	SceSize stateSize;
} NgsDspModuleLayout;

/* The rack's block read by the codec engine (rack->dsp, 128 bytes + tables). */
typedef struct NgsRackDsp {
	unsigned int moduleCount;
	unsigned int channelsPerVoice;
	unsigned int voiceOutputCount;
	unsigned int patchesPerOutput;
	unsigned int dspModuleCount;
	SceSize size;
	struct NgsAudioFormat format;	/* the first module's with a format */
	uintptr_t dispatchIds;		/* unsigned int[moduleCount] until mapped */
	uintptr_t dispatchIds2;
	uintptr_t inputCounts;		/* unsigned char[moduleCount] */
	uintptr_t outputCounts;
	uintptr_t dspControls;
	uintptr_t layoutsPhysical;
	NgsDspModuleLayout *layouts;
	SceSize voiceSize;
	SceSize stateSize;
	SceSize paramsSize;
	SceSize inputMixerSize;
	SceSize sharedWorkSize;
	SceSize contextSize;
	SceSize contextAndPatchSize;
	uintptr_t voiceListPhysical;
	uintptr_t voiceCountPhysical;
	unsigned int reserved;
	unsigned int dspConfiguration;
	unsigned char padding[16];
} NgsRackDsp;

_Static_assert(sizeof(NgsRackDsp) == 128, "NgsRackDsp size");

/* A voice output's patch buffer in the voice's DSP block. */
typedef struct NgsDspPatchBuffer {
	void *destination;
	void *buffer;
	uintptr_t destinationPhysical;
	uintptr_t bufferPhysical;
} NgsDspPatchBuffer;

/* A codec module's buffer in the voice's DSP block. */
typedef struct NgsDspCodecBuffer {
	uintptr_t memoryPhysical;
	unsigned int memoryOffset;
	int codecHandle;
	unsigned int memorySize;
} NgsDspCodecBuffer;

/* The module index of an NgsVoiceDspCallback that holds no callback. */
#define NGS_VOICE_DSP_NO_MODULE	32

/* The most patches an input mixer mixes, and the buffers its context holds. */
#define NGS_INPUT_MIXER_MAX_PATCHES	3
#define NGS_INPUT_MIXER_BUFFER_SLOTS	6

/* The codec engine's state of one input mixer of a voice (128 bytes). */
typedef struct NgsInputMixerContext {
	unsigned int patchCount;
	unsigned int channels;
	unsigned int reserved[6];
	uintptr_t buffers[NGS_INPUT_MIXER_BUFFER_SLOTS][2];	/* physical */
	unsigned int restart[NGS_INPUT_MIXER_BUFFER_SLOTS];	/* set at a voice restart */
	unsigned int padding[6];
} NgsInputMixerContext;

_Static_assert(sizeof(NgsInputMixerContext) == 128,
	       "NgsInputMixerContext size");

/* A module callback the codec engine raised for a voice. */
typedef struct NgsVoiceDspCallback {
	unsigned int moduleIndex;
	void *callbackPtr;
	int callbackData;
	int callbackData2;
} NgsVoiceDspCallback;

/* What the codec engine reports for a voice after an update (dsp->status). */
typedef struct NgsVoiceDspStatus {
	unsigned int moduleUpdateMask;
	unsigned int event;
	unsigned int reserved[2];
	unsigned short paramsError;	/* a module's parameters were out of range */
	unsigned short paramsErrorModule;
	NgsVoiceDspCallback callbacks[2];
} NgsVoiceDspStatus;

_Static_assert(sizeof(NgsVoiceDspStatus) == 52, "NgsVoiceDspStatus size");

/* The per-voice block read by the codec engine (rack->voiceDsps[voice]). */
typedef struct NgsVoiceDsp {
	SceSize size;
	unsigned int heapOffset;
	unsigned char *moduleMemory;
	SceSize moduleMemorySize;
	void *inputMixerMemory;
	SceSize inputMixerMemorySize;
	unsigned char *params;
	SceSize paramsSize;
	unsigned char *inputs;
	unsigned char *outputs;
	struct NgsVoice *voice;
	SceSize inputMixerStateSize;
	struct NgsDspRackEntry *rackEntry;	/* where the voice was queued */
	NgsRackDsp *rackDsp;
	unsigned int initId;		/* the voice's initId as of the update */
	NgsDspPatchBuffer *patchBuffers;
	unsigned int controlFlags;	/* NGS_VOICE_DSP_* */
	unsigned int bypassFlags;	/* the voice's bypassFlags as of the update */
	unsigned int updatedParams;	/* modules whose parameters changed, or ~0 */
	NgsVoiceDspStatus *status;
	SceSize statusSize;
	uintptr_t moduleMemoryPhysical;
	uintptr_t inputMixerMemoryPhysical;
	uintptr_t paramsPhysical;
	uintptr_t inputsPhysical;
	uintptr_t outputsPhysical;
	uintptr_t rackEntryPhysical;
	uintptr_t rackDspPhysical;
	uintptr_t patchBuffersPhysical;
	uintptr_t statusPhysical;
	NgsDspCodecBuffer *codecBuffers;
} NgsVoiceDsp;

_Static_assert(sizeof(NgsVoiceDsp) == 124, "NgsVoiceDsp size");

/* A patch from one voice output to another voice's input. */
typedef struct NgsPatch {
	struct NgsVoice *destination;
	void *mixerContext;		/* the destination input's mixer context */
	uintptr_t mixerContextPhysical;
	SceNgsVolumeMatrix *volumes;
	struct NgsVoice *source;
	unsigned char outputIndex;
	unsigned char channels;
	unsigned char reserved22;
	unsigned char reserved23;
	unsigned char inputIndex;
	unsigned char index;
	unsigned short signature;
} NgsPatch;

_Static_assert(sizeof(NgsPatch) == 28, "NgsPatch size");

/* The patches into one input mixer of a voice. */
typedef struct NgsVoiceInput {
	unsigned int maxPatches;
	unsigned int patchCount;
	NgsPatch **patches;
	void *mixerContext;		/* NgsInputMixerContext */
	uintptr_t mixerContextPhysical;
} NgsVoiceInput;

/* A module's parameters of a voice, at the kernel's and the caller's address. */
typedef struct NgsVoiceModuleParams {
	void *paramsCaller;
	void *params;
	int locked;
	SceNgsModuleCallbackFunc callback;
	void *callbackUserData;
} NgsVoiceModuleParams;

/* Whether a voice plays, and whether it is paused. */
typedef struct NgsVoiceState {
	unsigned int pauseState;
	unsigned int runState;
} NgsVoiceState;

typedef struct NgsVoice {
	unsigned int signature;
	NgsVoiceState playState;
	NgsVoiceState callerPlayState;	/* playState as of the last pull */
	unsigned short dirtyFlags;
	unsigned short dirtyFlags2;
	unsigned int initId;		/* ngsGetNextRackId() at the last voice init */
	unsigned short callerFlags;	/* flags as of the last pull */
	unsigned char callerDirtyFlags;	/* dirtyFlags as of the last pull */
	unsigned char padding;
	unsigned short index;
	unsigned char flags;
	unsigned char channels;
	unsigned int bypassFlags;
	unsigned int patchDirtyMask;
	NgsPatch *patches;
	unsigned int paramsDirtyMask;
	struct NgsSynRack *rack;
	NgsVoiceInput *inputs;
	NgsVoiceModuleParams *moduleParams;
	SceNgsCallbackFunc finishedCallback;
	unsigned int moduleUpdateMask;	/* player modules to refresh */
	void *state;
	void *outputParams;
	SceSize outputParamsSize;
} NgsVoice;

_Static_assert(sizeof(NgsVoice) == 84, "NgsVoice size");

/* The rack as the caller's library reads it, at the start of the rack's callback
 * memory; pointers are at the caller's address (864 bytes; name ours). */
typedef struct NgsRackCallerInfo {
	unsigned int voiceCount;
	unsigned int patchesPerVoice;
	unsigned int channelsPerVoice;
	unsigned int inputMixerModuleCount;
	unsigned int moduleCount;
	unsigned int voiceOutputCount;
	unsigned int patchesPerOutput;
	unsigned int routingIndexCount;
	unsigned int routingIndices[32];
	unsigned int moduleIds[32];
	unsigned int *voiceResults;
	unsigned int *voiceRequests;
	unsigned int *updateCount;
	unsigned int *patchUpdateMasks;
	float *patchVolumes;
	unsigned int *paramsUpdateMasks;
	unsigned int *voiceFlags2;
	unsigned char *params;
	SceSize voiceParamsSize;
	unsigned int paramsOffsets[32];
	unsigned int paramsSizes[32];
	SceNgsPatchSetupInfo *patchSetups;
	unsigned int *patchChannels;
	unsigned int stateOffsets[32];
	unsigned int stateSizes[32];
	unsigned char *voiceStates;
	SceSize voiceStateSize;
	unsigned int padding[3];
} NgsRackCallerInfo;

_Static_assert(sizeof(NgsRackCallerInfo) == 0x360, "NgsRackCallerInfo size");

typedef struct NgsSynRack {
	unsigned int signature;
	NgsSynSystem *system;
	unsigned int voiceCount;
	NgsVoice *voices;
	NgsRackLink *links;
	NgsDspRackEntry *dspRackList;
	int *depthScratch;
	NgsRackModuleList moduleList;
	unsigned int routingIndexCount;
	unsigned int *routingIndices;
	unsigned int presetCount;
	NgsRackPreset *presets;
	int patchesPerOutput;
	void *userReleaseData;
	unsigned int *playerModules;
	unsigned int playerModuleCount;
	uintptr_t *playerModuleOffsets;
	unsigned int *at9Modules;
	unsigned int at9ModuleCount;
	uintptr_t *at9ModuleOffsets;
	unsigned int *inputMixerModules;
	unsigned int inputMixerModuleCount;
	uintptr_t *inputMixerModuleOffsets;
	unsigned int state;
	unsigned char channelsPerVoice;
	unsigned char index;
	unsigned char processingIndex;	/* its place in the update's order */
	unsigned char codecModuleCount;
	SceNgsRackReleaseCallbackFunc releaseCallback;
	SceNgsParamsErrorCallbackFunc paramErrorCallback;
	NgsRackDsp *dsp;
	SceSize dspSize;
	NgsVoiceDsp **voiceDsps;
	uintptr_t *voiceList;
	unsigned int *voiceListCount;
	void *voiceDspMemory;
	SceSize voiceDspMemorySize;
	void *voiceDspMemoryEnd;
	SceSize statusMemorySize;
	void *statusMemory;
	SceSize statusMemorySize2;
	void *coherentMemory;
	void *dspHeapMemory;
	void *callerBuffer;
	void *callbackMemory;
	void *systemMemory;
	SceSize coherentSize;
	SceSize dspHeapSize;
	SceSize systemSize;
	SceSize callbackSize;
	SceUID memBlock;
	unsigned char dspModuleCount;
	unsigned char patchesPerVoice;
	unsigned char moduleCount8;
	unsigned char voiceOutputCount;
	unsigned int *stateOffsets;
	unsigned int *stateSizes;
	unsigned int *paramsOffsets;
	unsigned int *paramsSizes;
	unsigned int *codecModules;
	SceNgsCallbackInfo *callbackInfos;
	SceNgsCallbackInfo *rackCallbackInfo;
	SceNgsCallbackInfo *voiceCallbackInfos;
	unsigned int *updateCount;
	unsigned int *updateCountCaller;
	unsigned int *updateRequested;
	unsigned int *patchUpdateMasks;
	float *patchVolumes;
	unsigned int *paramsUpdateMasks;
	unsigned int *voiceFlags2;
	unsigned char *params;
	unsigned char *paramsCaller;
	SceSize voiceParamsSize;
	SceSize updateBlockSize;
	SceNgsPatchSetupInfo *patchSetups;
	SceNgsPatchSetupInfo *patchSetupsCaller;
	unsigned int *patchChannels;
	unsigned int *patchChannelsCaller;
	unsigned int maxPatchesPerInput;
	unsigned int *voiceRequests;
	unsigned int *voiceRequestsCaller;
	unsigned int *voiceResults;
	unsigned int *voiceResultsCaller;
	unsigned char *voiceStatesMemory;
	unsigned char *voiceStatesMemoryCaller;
	SceSize requestBlockSize;
	int requestsDirty;
} NgsSynRack;

_Static_assert(offsetof(NgsSynRack, state) == 96, "NgsSynRack state");
_Static_assert(offsetof(NgsSynRack, dsp) == 112, "NgsSynRack dsp");
_Static_assert(offsetof(NgsSynRack, coherentMemory) == 156, "NgsSynRack coherent");
_Static_assert(offsetof(NgsSynRack, dspModuleCount) == 196, "NgsSynRack counts");
_Static_assert(offsetof(NgsSynRack, updateCount) == 232, "NgsSynRack update");
_Static_assert(offsetof(NgsSynRack, maxPatchesPerInput) == 292, "NgsSynRack max");
_Static_assert(sizeof(NgsSynRack) == 328, "NgsSynRack size");

/* ngs_access.c */
SceNgsHRack ngsGetRackHandle(const NgsSynRack *rack);
NgsSynRack *ngsGetRackFromHandle(SceNgsHRack handle);
SceNgsHVoice ngsGetVoiceHandle(const NgsVoice *voice);
NgsVoice *ngsGetVoiceFromHandle(SceNgsHVoice handle);
SceNgsHPatch ngsGetPatchHandle(const NgsPatch *patch);
NgsSynSystem *ngsGetSystemForVoiceHandle(SceNgsHVoice handle);
NgsPatch *ngsGetPatchFromHandle(SceNgsHPatch handle, NgsVoice **source);
NgsSynSystem *ngsGetVoiceSystem(const NgsVoice *voice);

/* ngs_rack.c */
void ngsCountRackDepth(NgsRackLink **links, unsigned int rackCount, int *depth);
int ngsBuildRack(SceNgsHSynSystem handle, void *callerBuffer,
		 SceSize callerSize, const SceNgsRackDescription *description,
		 SceNgsHRack *rackHandle, SceSize *callbackSize,
		 SceSize *coherentSize, SceSize *dspHeapSize, SceSize *systemSize);
int ngsGetRackMemorySize(SceNgsHSynSystem handle,
	const SceNgsRackDescription *description, SceSize *callbackSize,
	SceSize *coherentSize, SceSize *dspHeapSize, SceSize *systemSize);
int ngsSortRacks(NgsSynSystem *system);
void ngsRemoveRack(NgsSynSystem *system, unsigned int index, NgsSynRack *rack);
int ngsGetRackDepth(NgsSynRack *rack, NgsSynRack *other,
		    unsigned int rackCount, int *depth);
int ngsAddRackPatch(NgsSynRack *source, NgsSynRack *destination);
int ngsRemoveRackPatch(NgsSynRack *source, NgsSynRack *destination);
int ngsReleaseRack(SceNgsHRack handle, SceNgsRackReleaseCallbackFunc callback);
int ngsFlushInputMixer(NgsSynRack *rack, unsigned int voiceIndex,
		       unsigned int moduleIndex);
void ngsRelocateRackMemory(NgsSynRack *rack, void *releasedAddress,
			   SceSize releasedSize);

/* ngs_module_memory.c */
int ngsGetInputMixerMemorySize(NgsSynSystem *system,
			       const SceNgsRackDescription *description,
			       SceSize *contextSize, SceSize *bufferSize);
void ngsInitInputMixerContext(NgsSynSystem *system,
			      const SceNgsRackDescription *description,
			      void *memory, unsigned char *buffers);

/* ngs_buffer.c */
void ngsAssignModuleBuffers(unsigned char *inputSlots,
			    unsigned char *outputSlots, const NgsVoiceDsp *dsp,
			    const NgsRackModuleList *list);

/* ngs_routing.c */
int ngsGetPatchIndex(int nPatchesPerOutput, int nSourceOutputIndex,
		     int nSourceOutputSubIndex);

/* ngs_voice.c */
unsigned int ngsCountCodecModules(NgsSynSystem *system,
	const struct SceNgsVoiceDefinition *definition);
void ngsInitVoice(NgsSynSystem *system, NgsVoice *voice, NgsSynRack *rack,
		  const SceNgsRackDescription *description,
		  unsigned char **stateCursor, unsigned char **cursor,
		  SceSize stateSize, SceSize paramsSize, int sizeOnly);
NgsVoiceDsp *ngsAllocateVoiceDsp(NgsSynSystem *system, NgsSynRack *rack,
				 const SceNgsRackDescription *description,
				 unsigned char **cursor, int sizeOnly,
				 SceSize paramsSize, unsigned int codecCount);
void ngsInitVoiceInputMixers(NgsSynSystem *system, NgsVoice *voice,
			     NgsVoiceDsp *dsp,
			     const SceNgsRackDescription *description,
			     unsigned char **cursor, int sizeOnly,
			     SceSize moduleMemorySize,
			     SceSize inputMixerMemorySize, SceSize codecSize);
void ngsAllocateVoiceDspStatus(NgsSynSystem *system, NgsVoiceDsp *dsp,
			       const SceNgsRackDescription *description,
			       unsigned char **cursor, int sizeOnly);
void ngsMapVoiceDsp(NgsVoiceDsp *dsp);
int ngsUpdateVoiceFlags(NgsVoice *voice, NgsSynRack *rack);
SceInt32 sceNgsVoiceKillInternal(SceNgsHVoice handle);
SceInt32 sceNgsVoiceInitInternal(SceNgsHVoice handle,
	const SceNgsVoicePreset *preset, const SceUInt32 flags);

/* ngs_patch.c */
SceNgsHPatch ngsConnectPatch(NgsVoice *source, unsigned int index,
	NgsVoice *destination, unsigned int inputIndex,
	NgsSynRack *destinationRack, NgsSynRack *sourceRack);
int ngsPublishPatch(unsigned int voiceIndex, NgsSynRack *rack,
	unsigned int patchIndex, unsigned int channels,
	const SceNgsPatchSetupInfo *setup);
SceInt32 sceNgsPatchRemoveRoutingInternal(SceNgsHPatch handle);
