/*
 * Internal types, structures, constants, and prototypes for SceNgsUser.
 */
#pragma once

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <ngs/ngs_top.h>
#include <ngs/sulpha_ngs.h>
#include <sulpha.h>

#define NGS_MAX_SYSTEMS			8
#define NGS_MAX_RACKS_PER_SYSTEM	64
#define NGS_MAX_MODULES_PER_VOICE	32

#define NGS_HANDLE_SYSTEM_MAGIC		0x10FABFABU
#define NGS_HANDLE_SYSTEM_MASK		0x1FFFFFFFU
#define NGS_HANDLE_RACK_MAGIC		0x001BEEBAU
#define NGS_HANDLE_RACK_MASK		0x007FFFFFU
#define NGS_HANDLE_VOICE_MAGIC		0x00005BEAU
#define NGS_HANDLE_VOICE_MASK		0x00007FFFU
#define NGS_HANDLE_PATCH_MAGIC		0x0000005EU
#define NGS_HANDLE_PATCH_MASK		0x0000007FU

#define NGS_SULPHA_AGENT_API_TRACE	0x02420001U
#define NGS_SULPHA_AGENT_MODULE_TRACE	0x02420002U
#define NGS_SULPHA_AGENT_SYNTH_TRACE	0x02420003U
#define NGS_SULPHA_AGENT_PCM_CAPTURE	0x02420004U
#define NGS_SULPHA_AGENT_NAMED_OBJECTS	0x02420005U
#define NGS_SULPHA_AGENT_MESSAGE_LOG	0x02420006U

#define NGS_SULPHA_REG_SYSTEM		0x02420100U
#define NGS_SULPHA_REG_RACK		0x02420200U
#define NGS_SULPHA_REG_VOICE		0x02420300U

#define NGS_SULPHA_SNAPSHOT_MAGIC	0x30405060U
#define NGS_SULPHA_ERROR_INTERNAL	-2142437118	/* 0x804D0102 */

#define NGS_SULPHA_HASH_BUCKETS		6151
#define NGS_SULPHA_OBJECT_POOL_COUNT	4096
#define NGS_SULPHA_LOCAL_MEM_SIZE	344204U

#define NGS_SULPHA_AGENT_VERSION	0x00010002U

/* Shared Sulpha protocol descriptor types (matching SceSulpha) */
typedef struct NgsSulphaFieldDef {
	SceUInt8 id;
	SceUInt8 type;
	SceUInt16 count;
	const char *name;
	const char *desc;
	const char *unit;
} NgsSulphaFieldDef;
_Static_assert(sizeof(NgsSulphaFieldDef) == 16, "NgsSulphaFieldDef size");

typedef struct NgsSulphaExtFieldDef {
	SceUInt8 id;
	SceUInt8 type;
	SceUInt16 count;
	const char *name;
	const char *desc;
	const char *unit;
	SceUInt32 minVal;
	SceUInt32 maxVal;
	SceUInt32 defaultVal;
	SceUInt32 reserved;
} NgsSulphaExtFieldDef;
_Static_assert(sizeof(NgsSulphaExtFieldDef) == 32, "NgsSulphaExtFieldDef size");

typedef struct NgsSulphaEventDef {
	SceUInt8 id;
	SceUInt8 fieldCount;
	SceUInt16 flagsAndBlocks;
	const char *name;
	const char *desc;
	void *fields;
} NgsSulphaEventDef;
_Static_assert(sizeof(NgsSulphaEventDef) == 16, "NgsSulphaEventDef size");

typedef struct NgsSulphaEnumValueDef {
	SceInt32 value;
	const char *name;
	const char *desc;
} NgsSulphaEnumValueDef;
_Static_assert(sizeof(NgsSulphaEnumValueDef) == 12, "NgsSulphaEnumValueDef size");

typedef struct NgsSulphaEnumDef {
	SceUInt8 id;
	SceInt8 valueCount;
	SceUInt16 reserved;
	const char *name;
	const char *desc;
	const NgsSulphaEnumValueDef *values;
} NgsSulphaEnumDef;
_Static_assert(sizeof(NgsSulphaEnumDef) == 16, "NgsSulphaEnumDef size");

typedef struct NgsSulphaAgentDef {
	const char *name;
	SceUInt16 versionMajor;
	SceUInt16 versionMinor;
	SceUInt32 versionBuild;
	const char *desc;
	SceUInt32 uuid[4];
	SceUInt32 typeId;
	SceUInt8 eventDefCount;
	SceUInt8 subEventDefCount;
	SceUInt8 enumDefCount;
	SceUInt8 reserved;
	NgsSulphaEventDef *eventDefs;
	NgsSulphaEventDef *subEventDefs;
	const NgsSulphaEnumDef *enumDefs;
} NgsSulphaAgentDef;
_Static_assert(sizeof(NgsSulphaAgentDef) == 52, "NgsSulphaAgentDef size");

typedef struct NgsSulphaAgentMemReq {
	SceUInt32 userDataSize;
	SceUInt32 defCopySize;
	SceUInt32 heapSize;
	SceUInt32 eventCount;
	SceUInt32 nodeCount;
} NgsSulphaAgentMemReq;
_Static_assert(sizeof(NgsSulphaAgentMemReq) == 20, "NgsSulphaAgentMemReq size");

typedef struct NgsSulphaAgentCallbacks {
	SceInt32 (*getDef)(void *userData, const NgsSulphaAgentDef **outDef);
	SceInt32 (*getMemReq)(const void *cfg, NgsSulphaAgentMemReq *outReq);
	SceInt32 (*init)(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
	SceInt32 (*onStart)(void *agent, void *userData);
	SceInt32 (*shutdown)(void *agent, void *userData);
	SceInt32 (*onStarted)(void *agent, void *userData);
	void (*onCommand)(void *agent, void *userData, void *reader);
} NgsSulphaAgentCallbacks;
_Static_assert(sizeof(NgsSulphaAgentCallbacks) == 28, "NgsSulphaAgentCallbacks size");

typedef struct NgsSulphaAgentHeader {
	SceUInt32 typeId;
	SceUInt32 configSize;
	SceUInt32 version;
	const NgsSulphaAgentCallbacks *callbacks;
} NgsSulphaAgentHeader;
_Static_assert(sizeof(NgsSulphaAgentHeader) == 16, "NgsSulphaAgentHeader size");

typedef struct NgsSulphaAgentsConfig {
	SceUInt32 count;
	void *const *agents;
} NgsSulphaAgentsConfig;

/* Per-agent config structures */
typedef struct NgsSulphaApiTraceParamDesc {
	const char *name;
	SceUInt32 fieldType;
	const char *unit;
} NgsSulphaApiTraceParamDesc;
_Static_assert(sizeof(NgsSulphaApiTraceParamDesc) == 12, "NgsSulphaApiTraceParamDesc size");

typedef struct NgsSulphaApiTraceEventDesc {
	SceUInt32 eventId;
	const char *name;
	SceUInt32 paramCount;
	const NgsSulphaApiTraceParamDesc *params;
} NgsSulphaApiTraceEventDesc;
_Static_assert(sizeof(NgsSulphaApiTraceEventDesc) == 16, "NgsSulphaApiTraceEventDesc size");

typedef struct NgsSulphaApiTraceConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 eventCount;
	const NgsSulphaApiTraceEventDesc *events;
	SceUInt32 reserved;
	SceUInt32 pad;
	SceUInt32 maxEvents;
} NgsSulphaApiTraceConfig;
_Static_assert(sizeof(NgsSulphaApiTraceConfig) == 36, "NgsSulphaApiTraceConfig size");

typedef struct NgsSulphaModuleTraceConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 moduleCount;
	const SceUInt32 *moduleIds;
	SceUInt32 maxInstances;
} NgsSulphaModuleTraceConfig;
_Static_assert(sizeof(NgsSulphaModuleTraceConfig) == 28, "NgsSulphaModuleTraceConfig size");

typedef struct NgsSulphaSynthTraceConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 maxObjects;
} NgsSulphaSynthTraceConfig;
_Static_assert(sizeof(NgsSulphaSynthTraceConfig) == 20, "NgsSulphaSynthTraceConfig size");

typedef struct NgsSulphaPcmCaptureConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 maxSinks;
	SceUInt32 maxSources;
	SceUInt32 framesPerObject;
} NgsSulphaPcmCaptureConfig;
_Static_assert(sizeof(NgsSulphaPcmCaptureConfig) == 28, "NgsSulphaPcmCaptureConfig size");

typedef struct NgsSulphaNamedObjectsConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 maxNamedObjects;
} NgsSulphaNamedObjectsConfig;
_Static_assert(sizeof(NgsSulphaNamedObjectsConfig) == 20, "NgsSulphaNamedObjectsConfig size");

typedef struct NgsSulphaMessageLogConfig {
	NgsSulphaAgentHeader header;
	SceUInt32 maxTraceBufferBytes;
} NgsSulphaMessageLogConfig;
_Static_assert(sizeof(NgsSulphaMessageLogConfig) == 20, "NgsSulphaMessageLogConfig size");

/* Shared userspace NGS rack/system memory layouts */
typedef struct NgsRackShared {
	SceUInt32 numVoices;
	SceUInt32 patchesPerVoice;
	SceInt32 numChannelsPerVoice;
	SceUInt32 numInputs;
	SceInt32 numModules;
	SceUInt32 numOutputs;
	SceUInt32 patchesPerOutput;
	SceUInt32 maxModuleIndex;
	SceInt32 moduleIndexMap[NGS_MAX_MODULES_PER_VOICE];
	SceNgsModuleID moduleTypes[NGS_MAX_MODULES_PER_VOICE];
	SceUInt32 *voiceUpdateCounts;
	SceUInt32 *voiceStates;
	volatile SceUInt32 *dirtyCounter;
	SceUInt32 *patchDirtyMask;
	SceFloat32 *patchVolumes;
	SceUInt32 *paramDirtyMask;
	SceUInt32 *paramLockMask;
	SceUInt8 *paramBufferBase;
	SceUInt32 paramVoiceStride;
	SceUInt32 paramModuleOffsets[NGS_MAX_MODULES_PER_VOICE];
	SceUInt32 paramModuleSizes[NGS_MAX_MODULES_PER_VOICE];
	SceNgsPatchSetupInfo *patchSetupInfos;
	SceInt32 *patchInputChannels;
	SceUInt32 stateModuleOffsets[NGS_MAX_MODULES_PER_VOICE];
	SceUInt32 stateModuleSizes[NGS_MAX_MODULES_PER_VOICE];
	SceUInt8 *stateBufferBase;
	SceUInt32 stateVoiceStride;
	SceNgsHRack rackHandle;
	volatile SceUInt32 stateSpinLock;
	volatile SceUInt32 paramSpinLock;
} NgsRackShared;
_Static_assert(sizeof(NgsRackShared) == 864, "NgsRackShared size");

typedef struct NgsSystemShared {
	SceUInt32 systemFlags;
	NgsRackShared *racks[NGS_MAX_RACKS_PER_SYSTEM];
} NgsSystemShared;
_Static_assert(sizeof(NgsSystemShared) == 260, "NgsSystemShared size");

typedef struct NgsSystemSlot {
	SceUID processId;
	NgsSystemShared *shared;
} NgsSystemSlot;
_Static_assert(sizeof(NgsSystemSlot) == 8, "NgsSystemSlot size");

typedef struct NgsSystemLockSlot {
	SceKernelLwMutexWork lwMutex;
	SceNgsHSynSystem systemHandle;
	SceUInt32 reserved;
} NgsSystemLockSlot;
_Static_assert(sizeof(NgsSystemLockSlot) == 40, "NgsSystemLockSlot size");

typedef struct NgsCallbackEntry {
	void (*callback)(const SceNgsCallbackInfo *info);
	const SceNgsCallbackInfo *info;
} NgsCallbackEntry;

typedef struct NgsSulphaRegistration {
	SceUInt32 type;
	SceUInt32 handle;
	SceUInt32 isCreate;
} NgsSulphaRegistration;
_Static_assert(sizeof(NgsSulphaRegistration) == 12, "NgsSulphaRegistration size");

typedef struct NgsSulphaModuleQuery {
	char paramName[128];
	char paramDesc[128];
	SceUInt32 paramType;
	SceUInt32 minVal;
	SceUInt32 maxVal;
	SceUInt32 defaultVal;
	SceUInt32 reserved[8];
	SceUInt32 sectionCount;
	SceUInt32 moduleId;
	SceUInt32 sectionIndex;
	SceUInt32 paramSize;
} NgsSulphaModuleQuery;

typedef struct NgsSulphaModuleQueryHeader {
	SceUInt32 moduleId;
	SceUInt32 sectionIndex;
	SceUInt32 paramCount;
	SceUInt32 paramIndex;
	char descText[80];
} NgsSulphaModuleQueryHeader;

/* Sulpha NGS runtime object tracking structures */
typedef struct NgsSulphaListNode {
	struct NgsSulphaListNode *next;
	struct NgsSulphaListNode *prev;
} NgsSulphaListNode;

typedef struct NgsSulphaPool {
	void *storage;
	void **freeStack;
	SceInt32 capacity;
	SceInt32 elemSize;
	SceInt32 allocIndex;
} NgsSulphaPool;
_Static_assert(sizeof(NgsSulphaPool) == 20, "NgsSulphaPool size");

typedef struct NgsSulphaTrackedObject {
	SceUInt32 handle;
	void *agentRecord;
	void *sulphaNode;
	void *parentSulphaNode;
} NgsSulphaTrackedObject;
_Static_assert(sizeof(NgsSulphaTrackedObject) == 16, "NgsSulphaTrackedObject size");

typedef struct NgsSulphaHashEntry {
	SceUInt32 key;
	NgsSulphaTrackedObject *value;
	struct NgsSulphaHashEntry *next;
} NgsSulphaHashEntry;
_Static_assert(sizeof(NgsSulphaHashEntry) == 12, "NgsSulphaHashEntry size");

typedef struct NgsSulphaHashMap {
	NgsSulphaPool entryPool;
	NgsSulphaHashEntry **buckets;
	SceInt32 bucketCount;
	SceInt32 count;
	SceInt32 reserved;
} NgsSulphaHashMap;
_Static_assert(sizeof(NgsSulphaHashMap) == 36, "NgsSulphaHashMap size");

typedef struct NgsSulphaContext {
	SceUInt64 synthTraceHandle;
	SceUInt64 apiTraceHandle;
	SceUInt64 moduleTraceHandle;
	SceUInt64 pcmCaptureHandle;
	SceUInt64 namedObjectsHandle;
	SceUInt64 messageLogHandle;
	SceNgsBufferInfo updateBuffer;
	SceNgsBufferInfo queryBuffer;
	volatile SceUInt32 rwLock;
	NgsSulphaHashMap objectMap;
	NgsSulphaPool objectPool;
	SceUInt32 reserved;
} NgsSulphaContext;
_Static_assert(sizeof(NgsSulphaContext) == 128, "NgsSulphaContext size");

/* Snapshot record header from SceNgsInternal */
typedef struct NgsSulphaSnapshotHeader {
	SceUInt32 version;
	SceUInt32 magic;
	SceUInt32 recordCount;
	SceUInt32 totalSize;
} NgsSulphaSnapshotHeader;

typedef struct NgsSulphaRecordHeader {
	SceUInt32 recordType;
	SceUInt32 nextOffset;
} NgsSulphaRecordHeader;

/* Per-agent runtime userData structs */
typedef struct NgsSulphaNamedEntry {
	void *sulphaNode;
	const void *location;
	SceUInt32 length;
	char name[36];
} NgsSulphaNamedEntry;
_Static_assert(sizeof(NgsSulphaNamedEntry) == 48, "NgsSulphaNamedEntry size");

typedef struct NgsSulphaNamedObjectsAgent {
	SceUInt32 typeId;
	volatile SceUInt32 started;
	SceUInt32 capacity;
	SceUInt32 count;
	void *rootNode;
	NgsSulphaNamedEntry *entries;
} NgsSulphaNamedObjectsAgent;
_Static_assert(sizeof(NgsSulphaNamedObjectsAgent) == 24, "NgsSulphaNamedObjectsAgent size");

typedef struct NgsSulphaPcmSinkRecord {
	NgsSulphaListNode list;
	void *sulphaNode;
	SceUInt32 handle;
	SceUInt32 dataType;
	SceUInt32 format;
	SceUInt32 channels;
	SceUInt32 sampleRate;
} NgsSulphaPcmSinkRecord;
_Static_assert(sizeof(NgsSulphaPcmSinkRecord) == 32, "NgsSulphaPcmSinkRecord size");

typedef struct NgsSulphaPcmSourceRecord {
	NgsSulphaListNode list;
	void *sulphaNode;
	SceUInt32 handle;
	SceUInt32 position;
	SceInt16 loopCount;
	SceInt8 currentBuffer;
	SceUInt8 bufferCount;
} NgsSulphaPcmSourceRecord;
_Static_assert(sizeof(NgsSulphaPcmSourceRecord) == 24, "NgsSulphaPcmSourceRecord size");

typedef struct NgsSulphaPcmCaptureAgent {
	SceUInt32 typeId;
	volatile SceUInt32 started;
	volatile SceUInt32 rwLock;
	NgsSulphaPool sinkPool;
	NgsSulphaPool sourcePool;
	NgsSulphaListNode sinkList;
	NgsSulphaListNode sourceList;
} NgsSulphaPcmCaptureAgent;
_Static_assert(sizeof(NgsSulphaPcmCaptureAgent) == 68, "NgsSulphaPcmCaptureAgent size");

typedef struct NgsSulphaSynthRecord {
	NgsSulphaListNode list;
	void *sulphaNode;
	SceUInt32 handle;
	SceUInt16 index;
	SceUInt8 objectType;
	SceUInt8 pad;
	char name[28];
	SceUInt32 params[8];
} NgsSulphaSynthRecord;
_Static_assert(sizeof(NgsSulphaSynthRecord) == 80, "NgsSulphaSynthRecord size");

typedef struct NgsSulphaSynthTraceAgent {
	SceUInt32 typeId;
	volatile SceUInt32 started;
	volatile SceUInt32 rwLock;
	NgsSulphaPool recordPool;
	NgsSulphaListNode recordList;
} NgsSulphaSynthTraceAgent;
_Static_assert(sizeof(NgsSulphaSynthTraceAgent) == 40, "NgsSulphaSynthTraceAgent size");

typedef struct NgsSulphaModuleRecord {
	NgsSulphaListNode list;
	void *sulphaNode;
	SceUInt32 cycles;
	SceUInt8 eventId;
	SceUInt8 bypassFlag;
	SceUInt8 moduleIndex;
	SceUInt8 pad;
	/* Followed by variable-length module parameter bytes */
} NgsSulphaModuleRecord;
_Static_assert(sizeof(NgsSulphaModuleRecord) == 20, "NgsSulphaModuleRecord size");

typedef struct NgsSulphaModuleTraceAgent {
	SceUInt32 typeId;
	volatile SceUInt32 started;
	volatile SceUInt32 rwLock;
	NgsSulphaPool recordPool;
	NgsSulphaListNode recordList;
	SceUInt32 maxParamBytes;
	SceUInt32 moduleCount;
	SceUInt32 moduleIds[NGS_MAX_MODULES_PER_VOICE];
	SceUInt32 stringUsed;
	char stringPool[8192];
	NgsSulphaAgentDef agentDef;
} NgsSulphaModuleTraceAgent;
_Static_assert(sizeof(NgsSulphaModuleTraceAgent) == 8424, "NgsSulphaModuleTraceAgent size");

typedef void (*NgsSulphaObjectNotifyFn)(const NgsSulphaRegistration *reg);
typedef int (*NgsSulphaApiTraceFn)(const NgsSulphaApiTraceEventDesc *desc, ...);
typedef void (*NgsSulphaSynthUpdateFn)(SceNgsBufferInfo *buf);

/* ARMv7 exclusive load/store primitives (ngs_atomic.S) */
SceUInt32 ngsAtomicLoad32(const volatile void *ptr);
SceUInt32 ngsAtomicStore32(SceUInt32 val, volatile void *ptr);

/* Inline reader-writer spinlock helpers */
static inline __attribute__((always_inline)) void ngsRwLockReadLock(volatile SceUInt32 *lock)
{
	SceUInt32 oldVal, newVal, loaded;
	do {
		do {
			oldVal = *lock;
		} while ((oldVal & 0x7FFF0000U) != 0);
		newVal = (oldVal == 0) ? 0x80000001U : (oldVal + 1U);
		do {
			loaded = ngsAtomicLoad32(lock);
		} while (loaded == oldVal && ngsAtomicStore32(newVal, lock) != 0);
	} while (loaded != oldVal);
}

static inline __attribute__((always_inline)) void ngsRwLockReadUnlock(volatile SceUInt32 *lock)
{
	SceUInt32 oldVal, newVal, loaded;
	do {
		oldVal = *lock;
		newVal = oldVal - 1U;
		if ((SceUInt16)oldVal == 1U)
			newVal &= ~0x80000000U;
		do {
			loaded = ngsAtomicLoad32(lock);
		} while (loaded == oldVal && ngsAtomicStore32(newVal, lock) != 0);
	} while (loaded != oldVal);
}

static inline __attribute__((always_inline)) void ngsRwLockWriteLock(volatile SceUInt32 *lock)
{
	for (;;) {
		while (*lock != 0)
			;
		if (ngsAtomicLoad32(lock) == 0 && ngsAtomicStore32(0x00010000U, lock) == 0)
			break;
	}
}

static inline __attribute__((always_inline)) void ngsRwLockWriteUnlock(volatile SceUInt32 *lock)
{
	SceUInt32 oldVal, newVal, loaded;
	do {
		oldVal = *lock;
		newVal = oldVal - 0x00010000U;
		if ((newVal & 0x7FFF0000U) == 0 && (SceUInt16)oldVal != 0)
			newVal |= 0x80000000U;
		do {
			loaded = ngsAtomicLoad32(lock);
		} while (loaded == oldVal && ngsAtomicStore32(newVal, lock) != 0);
	} while (loaded != oldVal);
}

static inline void ngsAtomicInc32(volatile SceUInt32 *val)
{
	SceUInt32 oldVal, loaded;
	do {
		oldVal = *val;
		do {
			loaded = ngsAtomicLoad32(val);
		} while (loaded == oldVal && ngsAtomicStore32(oldVal + 1U, val) != 0);
	} while (loaded != oldVal);
}

static inline void ngsListInit(NgsSulphaListNode *head)
{
	head->next = head;
	head->prev = head;
}

static inline void ngsListPushBack(NgsSulphaListNode *head, NgsSulphaListNode *node)
{
	NgsSulphaListNode *tail = head->prev;
	node->next = head;
	node->prev = tail;
	tail->next = node;
	head->prev = node;
}

static inline void ngsListRemove(NgsSulphaListNode *node)
{
	NgsSulphaListNode *next = node->next;
	NgsSulphaListNode *prev = node->prev;
	next->prev = prev;
	prev->next = next;
	node->next = node;
	node->prev = node;
}

/* Imported SceLibKernel / SceThreadmgr functions */
SceUInt32 SceLibKernel_9F793F84(void); /* sceKernelGetCompiledSdkVersion */
SceUID sceKernelGetProcessId(void);
int sceKernelDelayThread(SceUInt32 delay);
void *sceClibMemcpy_safe(void *dst, const void *src, SceSize len);
void *sceClibMemset(void *dst, int ch, SceSize len);
int sceClibMemcmp(const void *lhs, const void *rhs, SceSize len);
char *sceClibStrncpy(char *dst, const char *src, SceSize len);
SceSize sceClibStrnlen(const char *s, SceSize maxLen);
int sceClibVsnprintf(char *buf, SceSize size, const char *fmt, va_list ap);
int sceClibPrintf(const char *fmt, ...);

/* Imported SceSulpha functions */
int sceSulphaAgentsGetNeededMemory(const NgsSulphaAgentsConfig *config, SceUInt32 *sizeInBytes);
int sceSulphaAgentsRegister(const NgsSulphaAgentsConfig *config, void *buffer,
			    SceUInt32 sizeInBytes, SceUInt64 *handles);
int sceSulphaAgentsUnregister(const SceUInt64 *handles, SceUInt32 count);
int sceSulphaGetAgent(SceUInt64 handle, void **outAgent, void **outUserData);
int sceSulphaNodeNew(void **outNode, void *agent, void *parent);
int sceSulphaNodeDelete(void *node, SceUInt8 eventId);
int sceSulphaEventNew(void **outEvent, void *agent, SceUInt8 eventId, SceUInt32 flags);
int sceSulphaEventDelete(void *event);
int sceSulphaEventAdd(void *event, SceUInt8 fieldId, const void *data, SceUInt32 size);
int sceSulphaEventReport(void *event, void *node);

/* Imported SceNgsInternal functions */
SceInt32 sceNgsSystemGetRequiredMemorySizeInternal(const SceNgsSystemInitParams *pSynthParams, SceUInt32 *pnSize);
SceInt32 sceNgsSystemInitInternal(SceNgsBufferInfo *pBuf, SceUInt32 sdkVersion,
				  const SceNgsSystemInitParams *pSynthParams, SceNgsHSynSystem *pSystemHandle);
SceInt32 sceNgsSystemUpdateInternal(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemReleaseInternal(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemLockInternal(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemUnlockInternal(SceNgsHSynSystem hSystemHandle);
SceInt32 sceNgsSystemSetParamErrorCallbackInternal(SceNgsHSynSystem hSystemHandle,
						   const SceNgsParamsErrorCallbackFunc callbackFuncPtr);
SceInt32 sceNgsSystemSetFlagsInternal(SceNgsHSynSystem hSystemHandle, SceUInt32 uSystemFlags);
SceInt32 sceNgsSystemGetCallbackListInternal(SceNgsHSynSystem hSystemHandle, NgsCallbackEntry **pList,
					     SceUInt32 *pUnused);
SceInt32 sceNgsSystemIsFixForBugzilla89940(SceNgsHSynSystem hSystemHandle);
SceNgsHSynSystem sceNgsSystemGetSysHandleFromRack(SceNgsHRack hRackHandle);
SceInt32 sceNgsSystemPullDataInternal(SceNgsHSynSystem hSystemHandle, SceUInt32 dirtyLo, SceUInt32 dirtyHi);
SceInt32 sceNgsSystemPushDataInternal(SceNgsHSynSystem hSystemHandle);

SceInt32 sceNgsModuleGetNumPresetsInternal(SceNgsHSynSystem hSystemHandle, SceNgsModuleID uModuleID,
					   SceUInt32 *puNumPresets);
SceInt32 sceNgsModuleGetPresetInternal(SceNgsHSynSystem hSystemHandle, SceNgsModuleID uModuleID,
				       SceUInt32 uPresetIndex, SceNgsBufferInfo *pParamsBuffer);
SceInt32 sceNgsModuleCheckParamsInRangeInternal(SceNgsHVoice hVoiceHandle, SceNgsModuleID moduleType,
						const SceNgsParamsDescriptor *pParams, SceUInt32 paramSize);

SceInt32 sceNgsRackGetRequiredMemorySizeInternal(SceNgsHSynSystem hSystemHandle,
						 const SceNgsRackDescription *pRackDesc, SceUInt32 *pnSize);
SceInt32 sceNgsRackInitInternal(SceNgsHSynSystem hSystemHandle, SceNgsBufferInfo *pRackBuffer,
				const SceNgsRackDescription *pRackDesc, SceNgsHRack *pRackHandle);
SceInt32 sceNgsRackGetVoiceHandleInternal(SceNgsHRack hRackHandle, SceUInt32 uIndex, SceNgsHVoice *pVoiceHandle);
SceInt32 sceNgsRackReleaseInternal(SceNgsHRack hRackHandle, const SceNgsRackReleaseCallbackFunc callbackFuncPtr);
SceInt32 sceNgsRackSetParamErrorCallbackInternal(SceNgsHRack hRackHandle,
						 const SceNgsParamsErrorCallbackFunc callbackFuncPtr);

SceInt32 sceNgsVoiceInitInternal(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pPreset, SceUInt32 uInitFlags);
SceInt32 sceNgsVoicePlayInternal(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceKeyOffInternal(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceKillInternal(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoicePauseInternal(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceResumeInternal(SceNgsHVoice hVoiceHandle);
SceInt32 sceNgsVoiceSetPresetInternal(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *pVoicePreset);
SceInt32 sceNgsVoiceBypassModuleInternal(SceNgsHVoice hVoiceHandle, SceUInt32 uModule, SceUInt32 uBypassFlag);
SceInt32 sceNgsVoiceSetModuleCallbackInternal(SceNgsHVoice hVoiceHandle, SceUInt32 uModule,
					      const SceNgsModuleCallbackFunc callbackFuncPtr, void *pUserData);
SceInt32 sceNgsVoiceSetFinishedCallbackInternal(SceNgsHVoice hVoiceHandle,
						const SceNgsCallbackFunc callbackFuncPtr, void *pUserData);
SceInt32 sceNgsVoiceGetModuleBypassInternal(SceNgsHVoice hVoiceHandle, SceUInt32 uModule, SceUInt32 *puBypassFlag);
SceInt32 sceNgsVoiceGetParamsOutOfRangeBufferedInternal(SceNgsHVoice hVoiceHandle, SceUInt32 uModule,
							char *pszMessageBuffer);
SceInt32 sceNgsVoiceSetAllBypassesInternal(SceNgsHVoice hVoiceHandle, SceUInt32 bypassMask);
SceInt32 sceNgsVoiceClearDirtyFlagInternal(SceNgsHVoice hVoiceHandle, SceUInt32 moduleBit);

SceInt32 sceNgsPatchCreateRoutingInternal(const SceNgsPatchSetupInfo *pPatchInfo, SceNgsHPatch *pPatchHandle);
SceInt32 sceNgsVoiceGetOutputPatchInternal(SceNgsHVoice hVoiceHandle, SceInt32 nOutputIndex,
					   SceInt32 nSubIndex, SceNgsHPatch *pPatchHandle);
SceInt32 sceNgsPatchRemoveRoutingInternal(SceNgsHPatch hPatchHandle);

const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetCompressorSideChainBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDistortionBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEnvelopeBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetEqBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMasterBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetMixerBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPauserBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetReverbBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleVoiceInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetTemplate1Internal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetDelayBussInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSasEmuVoiceInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetAtrac9VoiceInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetSimpleAtrac9VoiceInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamVoiceInternal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetScreamVoiceAT9Internal(void);
const struct SceNgsVoiceDefinition *sceNgsVoiceDefGetPitchshiftBussInternal(void);

SceInt32 sceNgsSulphaSetSynthUpdateCallbackInternal(SceNgsHSynSystem hSystemHandle,
						    NgsSulphaSynthUpdateFn callback, SceNgsBufferInfo *pBuf);
SceInt32 sceNgsSulphaGetSynthUpdateCallbackInternal(SceNgsHSynSystem hSystemHandle,
						    NgsSulphaSynthUpdateFn *pCallback, SceNgsBufferInfo *pBuf);
SceInt32 sceNgsSulphaGetModuleListInternal(SceUInt32 *moduleIds, SceUInt32 maxCount, SceUInt32 *outCount);
SceInt32 sceNgsSulphaGetInfoInternal(const NgsSulphaRegistration *reg, SceNgsBufferInfo *pBuf);
SceInt32 sceNgsSulphaQueryModuleInternal(SceUInt32 queryType, void *queryBuf);

/* Globals */
extern NgsSystemLockSlot g_ngsSystemLocks[NGS_MAX_SYSTEMS];
extern NgsSulphaObjectNotifyFn g_ngsSulphaObjectNotify;
extern NgsSulphaApiTraceFn g_ngsSulphaApiTrace;
extern NgsSystemSlot g_ngsSystemSlots[NGS_MAX_SYSTEMS];
extern volatile SceInt32 g_ngsSulphaState;
extern NgsSulphaContext g_ngsSulphaCtx;

/* Static tables (ngs_sulpha_tables.c) */
extern const NgsSulphaApiTraceEventDesc g_ngsApiTraceEvents[63];
extern const NgsSulphaEnumDef g_ngsReturnCodeEnumDef;
extern const NgsSulphaApiTraceConfig g_ngsDefaultApiTraceConfig;
extern const NgsSulphaMessageLogConfig g_ngsDefaultMessageLogConfig;
extern const NgsSulphaModuleTraceConfig g_ngsDefaultModuleTraceConfig;
extern const NgsSulphaNamedObjectsConfig g_ngsDefaultNamedObjectsConfig;
extern const NgsSulphaPcmCaptureConfig g_ngsDefaultPcmCaptureConfig;
extern const NgsSulphaSynthTraceConfig g_ngsDefaultSynthTraceConfig;
extern const NgsSulphaAgentDef g_ngsMessageLogAgentDef;
extern const NgsSulphaAgentDef g_ngsNamedObjectsAgentDef;
extern const NgsSulphaAgentDef g_ngsPcmCaptureAgentDef;
extern const NgsSulphaAgentDef g_ngsSynthTraceAgentDef;
extern NgsSulphaEventDef g_ngsMessageLogEventDefs[1];
extern NgsSulphaEventDef g_ngsNamedObjectsEventDefs[1];
extern NgsSulphaEventDef g_ngsPcmCaptureEventDefs[3];
extern NgsSulphaEventDef g_ngsSynthTraceEventDefs[4];
extern const NgsSulphaAgentsConfig g_ngsSulphaAgentsConfig;
extern const SceUInt8 g_ngsModuleParamTypeMap[9];

/* libngs.c internal helpers */
SceInt32 ngsSetSulphaObjectNotify(NgsSulphaObjectNotifyFn fn);
SceInt32 ngsSetSulphaApiTrace(NgsSulphaApiTraceFn fn);
SceInt32 ngsGetApiTraceTable(const NgsSulphaApiTraceEventDesc **outTable, SceUInt8 *outCount);
SceInt32 ngsSulphaSetSynthUpdateCallback(SceNgsHSynSystem hSystemHandle,
					 NgsSulphaSynthUpdateFn callback, SceNgsBufferInfo *pBuf);
SceInt32 ngsSulphaGetModuleList(SceUInt32 *moduleIds, SceUInt32 maxCount, SceUInt32 *outCount);
SceInt32 ngsSulphaGetInfo(const NgsSulphaRegistration *reg, SceNgsBufferInfo *pBuf);
SceInt32 ngsSulphaQueryModule(SceUInt32 queryType, void *queryBuf);

/* ngs_clib.c */
void *ngsMemcpy(void *dst, const void *src, SceSize len);
void *ngsMemcpyChecked(void *dst, SceSize dstSize, const void *src, SceSize len);
void *ngsMemset(void *dst, int ch, SceSize len);
int ngsVsnprintf(char *buf, SceSize size, const char *fmt, va_list ap);
char *ngsStrncpy(char *dst, const char *src, SceSize len);
SceSize ngsStrnlen(const char *s, SceSize maxLen);
int ngsMemcmp(const void *lhs, const void *rhs, SceSize len);

/* ngs_at9.c */
SceInt32 ngsAt9GetSectionDetailsImpl(SceInt32 startSampleOffset, SceInt32 numSamples,
				     SceInt32 configData, SceNgsAT9SkipBufferInfo *at9InfoBuffer);

/* ngs_params.c */
SceInt32 ngsPatchSetVolumeUser(SceNgsHPatch hPatchHandle, SceInt32 outputChannel,
			       SceInt32 inputChannel, SceFloat32 vol);
SceInt32 ngsPatchSetVolumesUser(SceNgsHPatch hPatchHandle, SceInt32 outputChannel,
				const SceFloat32 *volumes, SceInt32 numVols);
SceInt32 ngsPatchSetVolumesMatrixUser(SceNgsHPatch hPatchHandle, const SceNgsVolumeMatrix *matrix);
SceInt32 ngsPatchGetInfoUser(SceNgsHPatch hPatchHandle, SceNgsPatchRouteInfo *routeInfo,
			     SceNgsPatchSetupInfo *setup);
SceInt32 ngsVoiceGetInfoUser(SceNgsHVoice hVoiceHandle, SceNgsVoiceInfo *info);
SceInt32 ngsVoiceGetStateDataUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				  void *mem, SceUInt32 memSize);
SceInt32 ngsVoiceGetModuleTypeUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				   SceNgsModuleID *moduleType);
SceInt32 ngsVoiceLockParamsUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx,
				SceUInt16 paramsInterfaceId, SceNgsBufferInfo *paramsBuffer);
SceInt32 ngsVoiceUnlockParamsUser(SceNgsHVoice hVoiceHandle, SceUInt32 moduleIdx);
SceInt32 ngsVoiceSetParamsBlockUser(SceNgsHVoice hVoiceHandle, const SceNgsModuleParamHeader *paramData,
				    SceUInt32 size, SceInt32 *errorCount);
SceInt32 ngsVoiceApplyPresetUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset);
SceInt32 ngsVoiceSetPresetUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset);
SceInt32 ngsVoiceInitUser(SceNgsHVoice hVoiceHandle, const SceNgsVoicePreset *preset, SceUInt32 initFlags);
SceInt32 ngsPatchResetVolumesUser(SceNgsHPatch hPatchHandle);
SceInt32 ngsVoiceMarkPlayUser(SceNgsHVoice hVoiceHandle);
SceInt32 ngsVoiceMarkKillUser(SceNgsHVoice hVoiceHandle);
SceInt32 ngsVoiceMarkKeyOffUser(SceNgsHVoice hVoiceHandle);
SceInt32 ngsVoiceMarkPauseUser(SceNgsHVoice hVoiceHandle);
SceInt32 ngsVoiceMarkResumeUser(SceNgsHVoice hVoiceHandle);
NgsSystemShared *ngsSystemSetFlagsUser(SceNgsHSynSystem hSystemHandle, SceUInt32 flags);
SceInt32 ngsSystemRegisterUser(SceNgsHSynSystem hSystemHandle, NgsSystemShared *shared);
SceInt32 ngsSystemUnregisterUser(SceNgsHSynSystem hSystemHandle);
SceInt32 ngsRackRegisterUser(SceNgsHRack hRackHandle, NgsRackShared *shared);
SceInt32 ngsRackUnregisterUser(SceNgsHRack hRackHandle);
SceInt32 ngsSystemPullDataUser(SceNgsHSynSystem hSystemHandle);
SceInt32 ngsSystemPushDataUser(SceNgsHSynSystem hSystemHandle);
SceInt32 ngsResolvePatchHandle(SceNgsHPatch hPatchHandle, SceUInt32 *outVoiceIdx,
			       SceUInt32 *outPatchIdx, NgsRackShared **outRack);
SceInt32 ngsResolveVoiceHandle(SceNgsHVoice hVoiceHandle, SceUInt32 *outVoiceIdx,
			       NgsRackShared **outRack);
NgsSystemShared *ngsResolveSystemHandle(SceNgsHSynSystem hSystemHandle);
NgsSystemShared *ngsResolveSystemFromVoice(SceNgsHVoice hVoiceHandle);
SceInt32 ngsDecodeSystemHandle(SceNgsHSynSystem hSystemHandle, SceUInt32 *outSysIdx);
SceInt32 ngsDecodeRackHandle(SceNgsHRack hRackHandle, SceUInt32 *outSysIdx, SceUInt32 *outRackIdx);
SceUInt32 ngsRackParamLock(NgsRackShared *rack);
SceUInt32 ngsRackParamUnlock(NgsRackShared *rack);
SceUInt32 ngsRackStateLock(NgsRackShared *rack);
SceUInt32 ngsRackStateUnlock(NgsRackShared *rack);

/* ngs_sulpha_agents.c */
SceInt32 ngsApiTraceAgentInit(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
SceInt32 ngsModuleTraceAgentInit(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
SceInt32 ngsNamedObjectsAgentInit(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
SceInt32 ngsPcmCaptureAgentInit(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
SceInt32 ngsSynthTraceAgentInit(void *agent, const void *cfg, void *userData, SceUInt32 userDataSize);
SceInt32 ngsApiTraceAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsMessageLogAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsModuleTraceAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsNamedObjectsAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsPcmCaptureAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsSynthTraceAgentGetMemReq(const void *cfg, NgsSulphaAgentMemReq *outReq);
SceInt32 ngsApiTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsMessageLogAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsModuleTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsNamedObjectsAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsPcmCaptureAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsSynthTraceAgentGetDef(void *userData, const NgsSulphaAgentDef **outDef);
SceInt32 ngsApiTraceGetDefaultConfig(NgsSulphaApiTraceConfig *cfg);
SceInt32 ngsApiTraceReportCall(SceUInt64 agentHandle, SceUInt32 eventId, SceUInt32 paramCount, va_list ap);
SceInt32 ngsMessageLogEmit(void *agent, SceUInt8 eventId, const char *component, const char *fmt, va_list ap);
SceInt32 ngsMessageLogGetDefaultConfig(NgsSulphaMessageLogConfig *cfg);
SceInt32 ngsMessageLogReport(SceUInt64 agentHandle, SceUInt32 eventId, const char *component, const char *fmt, ...);
SceInt32 ngsModuleTraceAgentOnStart(void *agent, void *userData);
SceInt32 ngsNamedObjectsAgentOnStart(void *agent, void *userData);
SceInt32 ngsPcmCaptureAgentOnStart(void *agent, void *userData);
SceInt32 ngsSynthTraceAgentOnStart(void *agent, void *userData);
SceInt32 ngsModuleTraceEmitRecord(void *event, const NgsSulphaEventDef *eventDef, const NgsSulphaModuleRecord *rec);
SceInt32 ngsModuleTraceAgentOnStarted(void *agent, void *userData);
SceInt32 ngsNamedObjectsAgentOnStarted(void *agent, void *userData);
SceInt32 ngsPcmCaptureAgentOnStarted(void *agent, void *userData);
SceInt32 ngsSynthTraceAgentOnStarted(void *agent, void *userData);
SceInt32 ngsModuleTraceGetDefaultConfig(NgsSulphaModuleTraceConfig *cfg);
char *ngsSafeStrncpy(char *dst, SceUInt32 dstSize, const char *src, SceUInt32 maxCount);
SceInt32 ngsFormatString(char *buf, SceInt32 size, const char *fmt, va_list ap);
SceInt32 ngsModuleTraceCreate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsModuleTraceUpdate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsModuleTraceDestroy(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj);
SceInt32 ngsNamedObjectsAgentShutdown(void *agent, void *userData);
SceInt32 ngsNamedObjectsGetDefaultConfig(NgsSulphaNamedObjectsConfig *cfg);
SceInt32 ngsNamedObjectsSetSample(SceUInt64 agentHandle, const void *location, SceUInt32 length, const char *name);
SceInt32 ngsPcmCaptureGetDefaultConfig(NgsSulphaPcmCaptureConfig *cfg);
SceInt32 ngsPcmCaptureCreateSource(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsPcmCaptureCreateSink(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsPcmCaptureUpdateSource(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsPcmCaptureUpdateSink(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsPcmCaptureDestroySink(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj);
SceInt32 ngsPcmCaptureDestroySource(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj);
SceInt32 ngsSynthTraceEmitRecord(void *event, const NgsSulphaSynthRecord *rec, const char *name);
SceInt32 ngsSynthTraceCopyParams(NgsSulphaSynthRecord *rec, const SceUInt32 *snapshotData);
SceInt32 ngsSynthTraceGetDefaultConfig(NgsSulphaSynthTraceConfig *cfg);
SceInt32 ngsSynthTraceCreate(SceUInt64 agentHandle, SceUInt32 objectType,
			     const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsSynthTraceDestroy(SceUInt64 agentHandle, SceUInt8 eventId, NgsSulphaTrackedObject *obj);
SceInt32 ngsSynthTraceUpdate(SceUInt64 agentHandle, const SceUInt32 *snapshotData, NgsSulphaTrackedObject *obj);
SceInt32 ngsSynthTraceIsSystem(SceUInt64 agentHandle, const NgsSulphaTrackedObject *obj);
SceInt32 ngsSynthTraceSetName(SceUInt64 agentHandle, NgsSulphaTrackedObject *obj, const char *name);

/* ngs_sulpha_ngs.c */
const NgsSulphaAgentsConfig *ngsSulphaBuildAgentsConfig(const SceSulphaNgsConfig *config);
int ngsSulphaApiTraceHook(const NgsSulphaApiTraceEventDesc *desc, ...);
void ngsSulphaSynthUpdateCallback(SceNgsBufferInfo *buf);
SceInt32 ngsSulphaUnregisterRack(SceNgsHRack rackHandle);
SceInt32 ngsSulphaUnregisterSystem(SceNgsHSynSystem systemHandle);
void ngsSulphaObjectNotifyHook(const NgsSulphaRegistration *reg);
SceInt32 ngsSulphaLookupObject(SceUInt32 handle, NgsSulphaTrackedObject **outObj);
