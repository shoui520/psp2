/* Complete NGS system allocation and its memory-range cache. */

#pragma once

#include <scetypes.h>
#include <ngs/ngs_top.h>
#include <stddef.h>

#include "ngs_platform.h"
#include "ngs_module_memory.h"

struct NgsModuleDefinition;
struct NgsSynRack;
struct NgsVoice;
struct NgsDspRackEntry;

/* NgsSynSystem.state */
enum {
	NGS_SYSTEM_STATE_NONE = 0,
	NGS_SYSTEM_STATE_ACTIVE = 101,
	NGS_SYSTEM_STATE_RELEASING = 102,
};

/* A callback the caller runs after an update (sceNgsSystemGetCallbackList). */
typedef struct NgsCallbackList {
	void *callback;
	SceNgsCallbackInfo *info;	/* in the caller's memory */
} NgsCallbackList;

/* NgsDspBatchEntry.type */
enum {
	NGS_DSP_BATCH_VOICE = 0,	/* heapOffset: the voice's NgsVoiceDsp */
	NGS_DSP_BATCH_RACK = 1,		/* heapOffset: the rack entry (physical) */
	NGS_DSP_BATCH_STATUS = 2,	/* heapOffset: a codec buffer (physical) */
	NGS_DSP_BATCH_UNUSED = 3,
};

/* The status commands that open every batch. */
#define NGS_DSP_BATCH_STATUS_SLOTS	32

/* A command of the batch the codec engine processes. */
typedef struct NgsDspBatchEntry {
	unsigned int type;
	unsigned int heapOffset;
	SceSize size;		/* voice: its rack entry is queued; rack: voices */
} NgsDspBatchEntry;

/* A rack of the batch; the addresses are made physical before the run. */
typedef struct NgsDspBatchRack {
	uintptr_t voiceDspMemory;
	SceSize voiceDspMemorySize;
	uintptr_t voiceDspMemoryEnd;
	SceSize statusMemorySize;
	void *statusMemory;
	uintptr_t statusMemoryPhysical;
	SceSize statusMemorySize2;
	uintptr_t voiceCountPhysical;
} NgsDspBatchRack;

_Static_assert(sizeof(NgsDspBatchRack) == 32, "NgsDspBatchRack size");

/* The batch the codec engine processes at an update (system->dspBatch). */
typedef struct NgsDspBatch {
	NgsDspBatchEntry *entries;
	unsigned int entryCount;
	unsigned int capacity;
	SceSize size;
	NgsDspBatchRack *racks;
	unsigned int rackCount;
	unsigned int rackCapacity;
	uintptr_t entriesPhysical;
	uintptr_t racksPhysical;
} NgsDspBatch;

typedef struct NgsLockedMemoryRange {
	void *address;
	SceSize size;
} NgsLockedMemoryRange;

typedef struct NgsSynSystem {
	int systemId;
	unsigned int signature;
	unsigned int MaxRacks;
	unsigned int MaxVoices;
	unsigned int Granularity;
	unsigned int SampleRate;
	unsigned int flags;
	int copyUserData;
	SceSize initialCoherentAllocationSize;
	struct NgsSynRack **racks;
	struct NgsSynRack **processingRacks;
	int *rackDepths;
	struct NgsDspRackEntry *emptyGraph;
	NgsDspBatch *dspBatch;
	int pendingRackReleases;
	NgsPlatformContext *context;
	unsigned int ModulesLinked;
	unsigned int MaxModules;
	struct NgsModuleDefinition **modules;
	unsigned int nextRackId;
	unsigned int CycleCount;
	unsigned char state;
	SceNgsParamsErrorCallbackFunc paramErrorCallback;
	void *synthUpdateCallback;
	SceNgsBufferInfo debugBuffer;
	SceNgsBufferInfo callerDebugBuffer;
	SceUID debugMemBlock;
	void *dspHeap;
	SceSize dspHeapUsed;
	void *coherentAllocation;
	void *dspHeapAllocation;
	void *systemAllocation;
	void *callbackUserMemory;
	void *callbackKernelMemory;
	SceSize coherentAllocationSize;
	SceSize dspHeapAllocationSize;
	SceSize systemAllocationSize;
	SceSize callbackMemorySize;
	SceUID systemMemBlock;
	SceNgsCallbackInfo *rackReleaseInfo;
	SceNgsCallbackInfo *paramErrorInfo;
	NgsCallbackList *callbacks;
	NgsCallbackList *callerCallbacks;
	unsigned int callbackCount;
	unsigned int callbackCapacity;
	struct NgsVoice **updateVoices;
	unsigned int lockedRangeCount;
	NgsLockedMemoryRange lockedRanges[1024];
	unsigned int pullDataDeferred;
	unsigned int compiledSdkVersion;
	unsigned int fixForBugzilla89940;
} NgsSynSystem;

_Static_assert(sizeof(NgsLockedMemoryRange) == 8, "NgsLockedMemoryRange size");
_Static_assert(offsetof(NgsSynSystem, context) == 60, "NgsSynSystem context");
_Static_assert(offsetof(NgsSynSystem, lockedRanges) == 196, "NgsSynSystem ranges");
_Static_assert(offsetof(NgsSynSystem, compiledSdkVersion) == 8392,
	"NgsSynSystem compiled SDK");
_Static_assert(sizeof(NgsSynSystem) == 8400, "NgsSynSystem size");

/* ngs_access.c */
int ngsCopyToCaller(void *destination, const void *source, SceSize size, int copyUserData);
int ngsCopyFromCaller(void *destination, const void *source, SceSize size, int copyUserData);
int ngsIsCallerRangeAccessible(const void *address, SceSize size, int copyUserData);
unsigned int ngsGetSystemHandle(NgsSynSystem *system);
NgsSynSystem *ngsGetSystemFromHandle(unsigned int handle);
int ngsRunDspBatch(NgsSynSystem *system, NgsDspBatchEntry *entries,
		   unsigned int entryCount);
uintptr_t ngsGetPhysicalAddress(const void *address);
int ngsIsRangeReadable(const void *address, SceSize size);

/* ngs_builtin.c */
int ngsRegisterBuiltinModules(NgsSynSystem *system);

/* ngs_sulpha.c */
void ngsSulphaTraceUpdate(NgsSynSystem *system, NgsDspBatchEntry *entries,
			  int entryCount, SceNgsBufferInfo *buffer);
void ngsSulphaEndUpdate(NgsSynSystem *system, NgsDspBatchEntry *entries,
			int entryCount);

/* ngs_at9_player.c */
int ngsGetAt9PlayerModule(NgsModuleDefinition **module);

/* ngs_atomic.c */
int ngsAtomicSet(int *value, int replacement, int expected);
int ngsLockSystemMutex(NgsPlatformContext *context);
int ngsUnlockSystemMutex(NgsPlatformContext *context);
int ngsLockVoice(NgsPlatformContext *context);
int ngsUnlockVoice(NgsPlatformContext *context);
int ngsLockUpdate(NgsPlatformContext *context);
int ngsUnlockUpdate(NgsPlatformContext *context);
int ngsLockSystem(NgsPlatformContext *context);
int ngsUnlockSystem(NgsPlatformContext *context);
int ngsTryLockSystem(NgsPlatformContext *context);
int ngsLockParams(NgsPlatformContext *context);
int ngsUnlockParams(NgsPlatformContext *context);
int ngsLockCodec(NgsPlatformContext *context);
int ngsUnlockCodec(NgsPlatformContext *context);
int ngsLockProcess(NgsProcessStorage *storage);
int ngsUnlockProcess(NgsProcessStorage *storage);
int ngsLockTransfer(NgsPlatformContext *context);
int ngsUnlockTransfer(NgsPlatformContext *context);
void ngsCleanDcacheRange(void *address, int size);

/* ngs_biquad_filter.c */
int ngsGetBiquadFilterModule(NgsModuleDefinition **module);

/* ngs_builtin.c */
int ngsFindBuiltinModule(unsigned int moduleId, NgsModuleDefinition **module);
int ngsFindSystemModule(NgsSynSystem *system, unsigned int moduleId,
			NgsModuleDefinition **module);
int ngsGetModuleMemoryInfo(NgsSynSystem *system, unsigned int moduleId,
			   int channels, NgsModuleMemoryInfo *info);
int ngsIsCodecModule(NgsSynSystem *system, unsigned int moduleId);
int ngsCheckModuleOutputs(NgsSynSystem *system, unsigned int moduleId,
			  unsigned int outputCount);

/* ngs_params.c */
int ngsCheckParamType(uint32_t paramsId, uint32_t expectedParamsId);
int ngsGetParamsOutOfRange(NgsSynSystem *system, int channels,
		unsigned int moduleId, const SceNgsParamsDescriptor *params,
		SceSize paramsSize, char *message);
int ngsCheckParamsInRange(NgsSynSystem *system, unsigned int channels,
		unsigned int moduleId, const SceNgsParamsDescriptor *params,
		SceSize paramsSize);

/* ngs_compressor.c */
int ngsGetCompressorModule(NgsModuleDefinition **module);

/* ngs_delay.c */
int ngsGetDelayModule(NgsModuleDefinition **module);

/* ngs_distortion.c */
int ngsGetDistortionModule(NgsModuleDefinition **module);

/* ngs_envelope.c */
int ngsGetEnvelopeModule(NgsModuleDefinition **module);

/* ngs_input.c */
int ngsGetInputModule(NgsModuleDefinition **module);

/* ngs_mixer.c */
int ngsGetMixerModule(NgsModuleDefinition **module);

/* ngs_output.c */
int ngsGetOutputModule(NgsModuleDefinition **module);

/* ngs_pauser.c */
int ngsGetPauserModule(NgsModuleDefinition **module);

/* ngs_pcm_player.c */
int ngsGetPcmVagPlayerModule(NgsModuleDefinition **module);

/* ngs_pitch_shift.c */
int ngsGetPitchShiftModule(NgsModuleDefinition **module);

/* ngs_reverb.c */
int ngsGetReverbModule(NgsModuleDefinition **module);

/* ngs_signal_generator.c */
int ngsGetSignalGeneratorModule(NgsModuleDefinition **module);

/* ngs_system.c */
unsigned int ngsGetNextRackId(NgsSynSystem *system);
