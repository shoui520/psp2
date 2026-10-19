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
struct NgsDspGraph;
struct NgsDspBatch;
struct NgsCallbackList;

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
	struct NgsSynRack **rackWorkList;
	struct NgsDspGraph *emptyGraph;
	struct NgsDspBatch *dspBatch;
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
	struct NgsCallbackList *callbacks;
	struct NgsCallbackList *callerCallbacks;
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

/* ngs_at9_player.c */
int ngsGetAt9PlayerModule(NgsModuleDefinition **module);

/* ngs_atomic.c */
int ngsAtomicSet(int *value, int replacement, int expected);
int ngsLockCodec(NgsPlatformContext *context);
int ngsUnlockCodec(NgsPlatformContext *context);

/* ngs_biquad_filter.c */
int ngsGetBiquadFilterModule(NgsModuleDefinition **module);

/* ngs_builtin.c */
int ngsFindBuiltinModule(unsigned int moduleId, NgsModuleDefinition **module);

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
