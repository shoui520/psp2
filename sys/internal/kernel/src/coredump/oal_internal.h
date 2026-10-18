/* Private wire records produced by SceCoredump's operating-system layer. */

#pragma once

#include <stdint.h>
#include <scetypes.h>
#include <modulemgr/module_library.h>
#include <coredump/coredump.h>
#include <ss_mgr/ss_mgr.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include <threadmgr/lw_mutex.h>
#include <sdif/sdif.h>

typedef struct CoredumpVitaCardInfo {
	uint8_t size[4];
	uint8_t headerSize[4];
	char name[32];
	uint8_t recordSize[2];
	uint8_t recordType[2];
	uint8_t present[4];
	SdifCardReg128 cid;
} CoredumpVitaCardInfo;

_Static_assert(sizeof(CoredumpVitaCardInfo) == 64,
	"coredump Vita-card info size");

typedef struct CoredumpReasonCode {
	uint8_t codeLow;
	uint8_t codeHigh;
	uint8_t groupLow;
	uint8_t groupHigh;
} CoredumpReasonCode;

typedef struct CoredumpMemoryCardInfo {
	uint8_t size[4];
	uint8_t headerSize[4];
	char name[32];
	uint8_t recordSize[2];
	uint8_t recordType[2];
	uint8_t present[4];
	int mediaType;
	int writeProtected;
	uint32_t nbytes[2];
	uint32_t nbytes2[2];
	uint32_t sector_size[2];
	uint32_t blockSize;
	uint32_t serialNumber;
	uint32_t assemblyModel;
	uint8_t assemblyMaker;
	uint8_t reservedBeforeDate[3];
	uint8_t assemblyDate[8];
	uint8_t reservedTail[8];
} CoredumpMemoryCardInfo;

_Static_assert(sizeof(CoredumpMemoryCardInfo) == 112,
	"coredump memory-card info size");

typedef struct CoredumpAppInfo {
	uint32_t reservedHeader;
	int appId;
	int launchSourceAppId;
	SceUID processId;
	SceUID launchSourceProcessId;
	char appName[32];
	SceUID budgetId;
	uint32_t launchMode;
	uint32_t processAttr;
	uint32_t fdLimit;
	uint32_t dirLimit;
	SceSize appPathLength;
} CoredumpAppInfo;

_Static_assert(sizeof(CoredumpAppInfo) == 76, "CoredumpAppInfo size");

typedef struct CoredumpDeviceInfo {
	uint32_t reservedHeader;
	SceUID soundExclusivePid;
	SceUID mainOutPortOwner;
	SceUID bgmOutPortOwner;
	SceUID voiceOutPortOwner;
	SceUID inputPortOwner;
	SceUID backRenderPid;
	SceUID inputOwnerPid;
	int bgmProxyAppId;
	SceUID shellPid;
	int activeAppId;
	SceUID displayOwner0;
	SceUID displayOwner1;
	SceUID systemChatPid;
	int backRenderAppId;
} CoredumpDeviceInfo;

_Static_assert(sizeof(CoredumpDeviceInfo) == 60, "CoredumpDeviceInfo size");

typedef struct CoredumpStopReason {
	uint8_t reservedHeader[4];
	CoredumpReasonCode reason;
	CoredumpReasonCode detail;
	uint8_t reservedTail[8];
} CoredumpStopReason;

/* PROCESS_INFO note body; pathSize is filled by the segment writer */
typedef struct CoredumpProcessInfo {
	uint32_t reservedHeader;
	SceUID pid;
	uint32_t flags;			/* 1 valid, 2 status 2, 4 system program, 8 status 3, 0x10000 plaintext dump */
	char name[32];
	uint32_t reserved;
	uint32_t cpuAffinityMask;
	uintptr_t entryPoint;
	SceUInt32 fingerprint;
	union {
		SceUID parentPid;
		int8_t parentPidBytes[4];	/* all ones for pid 0x10005 */
	};
	CoredumpStopReason stopReason;
	SceSize pathSize;
} CoredumpProcessInfo;

/* EXTNL_PROC_INFO entry: another process, with its path size */
typedef struct CoredumpExtnlProcInfo {
	uint32_t reservedHeader;
	SceUID pid;
	SceUID budgetId;		/* only when the dumped process is a system program */
	uint32_t flags;			/* as CoredumpProcessInfo */
	char name[32];
	uint32_t reserved;
	uint32_t cpuAffinityMask;
	uintptr_t entryPoint;
	SceUInt32 fingerprint;
	union {
		SceUID parentPid;
		int8_t parentPidBytes[4];	/* all ones for pid 0x10005 */
	};
	CoredumpStopReason stopReason;
	SceSize pathSize;
} CoredumpExtnlProcInfo;

/* BUDGET_INFO: a budget and its physical memory partitions */
typedef struct CoredumpBudgetInfo {
	uint32_t reservedHeader;
	SceUID budgetId;
	char name[32];
	SceUInt32 numPhyMemParts;
} CoredumpBudgetInfo;

typedef struct CoredumpPhyMemPartSegment {
	uint32_t reserved;
	uintptr_t pbase;
	SceSize psize;
	SceSize freeSize;		/* the partition's, in every segment */
	SceSize freeSizeMin;
	SceUInt32 freeUnitCount;
	uint32_t reservedTail[9];
} CoredumpPhyMemPartSegment;

typedef struct CoredumpPhyMemPartInfo {
	uint32_t reservedHeader;
	SceUID uid;
	char name[32];
	SceUInt32 type;
	SceUInt32 numSegments;		/* at most 8 */
	CoredumpPhyMemPartSegment segments[8];
} CoredumpPhyMemPartInfo;

/* the main module's unwind tables */
typedef struct CoredumpArmExInfo {
	uintptr_t exidxTop;
	uintptr_t exidxBtm;
	uintptr_t extabTop;
	uintptr_t extabBtm;
} CoredumpArmExInfo;

typedef struct CoredumpThreadStopEvent {
	SceUID threadId;
	int reason;
} CoredumpThreadStopEvent;

typedef struct CoredumpCdnPcAndLrInfo {
	uintptr_t pc;
	uintptr_t lr;
	SceNID pcModuleFingerprint;
	SceNID lrModuleFingerprint;
} CoredumpCdnPcAndLrInfo;

typedef struct CoredumpCdnAccessParam {
	char magic[4];
	uint8_t formatVersion[4];
	uint8_t systemVersion[4];
	uint32_t appVersion;
	uint32_t stopReason;
	uintptr_t pc;
	uint32_t reservedPcHigh;
	uintptr_t lr;
	uint32_t reservedLrHigh;
	SceNID pcModuleFingerprint;
	uint8_t reservedPcModule[16];
	SceNID lrModuleFingerprint;
	uint8_t reservedTail[36];
} CoredumpCdnAccessParam;

_Static_assert(sizeof(CoredumpCdnAccessParam) == 96,
	"CoredumpCdnAccessParam size");

typedef struct CoredumpBuildVerInfo {
	uint32_t reservedHeader;
	uint32_t sysRevision;
	uint32_t vshRevision;
	uint32_t securityRevision;
	char sysRepositoryPath[64];
	char vshRepositoryPath[64];
	char securityRepositoryPath[64];
	uint32_t sdkInternalBuildNumber;
	uint32_t vshBuildNumber;
	uint8_t reservedTail[296];
} CoredumpBuildVerInfo;

typedef struct CoredumpRunContext {
	SceUID pid;
	SceCoredumpTriggerParam trigger;
	uint32_t stopReason;
	uint32_t systemInfoFlagsEnabled;
} CoredumpRunContext;

typedef struct CoredumpMemBlockInfo {
	uint32_t reservedHeader;
	SceUID uid;
	char name[32];
	uint32_t type;
	uintptr_t vbase;
	SceSize size;
	uint32_t reserved[2];
	SceSize mapSize;
	SceSize headFree;
	SceSize tailFree;
} CoredumpMemBlockInfo;

typedef struct CoredumpCompletionContext {
	SceUID threadId;
	uint32_t reserved;
	SceKernelCoredumpStateFinishCallback callback;
	const void *triggerData;
} CoredumpCompletionContext;

typedef struct CoredumpShutdownContext {
	SceUID threadId;
	int *result;
} CoredumpShutdownContext;

typedef int (*CoredumpCompletionCallback)(int requestId, SceUID pid,
	int result, const char *path, SceSize pathLength, int encrypted,
	CoredumpCompletionContext *context);

typedef struct CoredumpEvent {
	uint32_t kind;
	SceUID pid;
	int requestId;
	SceCoredumpTriggerParam trigger;
	SceKernelCoredumpStateUpdateCallback progressCallback;
	CoredumpCompletionCallback completionCallback;
	CoredumpCompletionContext completionContext;
	int (*shutdownCallback)(int result, CoredumpShutdownContext *context);
	CoredumpShutdownContext shutdownContext;
	int canceled;
} CoredumpEvent;

typedef struct CoredumpEventQueue {
	unsigned int count;
	SceUID mutexId;
	SceUID condId;
	CoredumpEvent events[10];
} CoredumpEventQueue;

typedef struct CoredumpCorefileInfo {
	uint32_t reservedHeader;
	uint8_t formatVersion[4];
	uint32_t programClass;
	uint32_t reserved[6];
} CoredumpCorefileInfo;

typedef struct CoredumpCafSectionHash {
	uint64_t sectionId;
	uint8_t digest[32];
	uint8_t reserved[8];
} CoredumpCafSectionHash;

typedef struct CoredumpCafHeader {
	uint8_t magic[8];
	uint8_t formatVersion[8];
	uint8_t primaryTypeFlag[8];
	uint32_t segmentCountLow;
	uint32_t segmentCountHigh;
	uint32_t headerSizeLow;
	uint32_t headerSizeHigh;
	uint32_t dataSizeLow;
	uint32_t dataSizeHigh;
} CoredumpCafHeader;

_Static_assert(sizeof(CoredumpCafHeader) == 48, "CoredumpCafHeader size");

typedef struct CoredumpCafIv {
	uint32_t words[4];
} CoredumpCafIv;

typedef struct CoredumpCafSegmentTable {
	uint32_t segmentIdLow;
	uint32_t segmentIdHigh;
	uint32_t fileOffsetLow;
	uint32_t fileOffsetHigh;
	uint32_t segmentSize;
	uint32_t segmentIndex;
	uint8_t hashKeySelector[8];
	uint8_t cipherKeySelector[8];
	CoredumpCafIv iv;
	uint8_t reserved[8];
} CoredumpCafSegmentTable;

_Static_assert(sizeof(CoredumpCafSegmentTable) == 64,
	"CoredumpCafSegmentTable size");

typedef struct CoredumpSystemInfo {
	uint32_t reservedHeader;
	uint8_t systemVersion[4];
	uint32_t reservedVersion;
	uint8_t kernelVersion[4];
	uint32_t reservedId;
	SceKernelOpenPsId openPsId;
	uint8_t hardwareType[4];
	uint32_t dipswInfo;
	union {
		uint8_t flags[4];
		uint32_t flagBits;
	};
	union {
		uint8_t isDolceBytes[2];
		uint16_t isDolce;
	};
	uint8_t reservedHardware[2];
	SceKernelOpenPsId encryptedOpenPsId;
	uint32_t timestampLow;
	uint32_t timestampHigh;
	uint32_t qaEnabled;
	uint32_t vshBuildNumber;
	uint32_t gpi;
	SceSize grownPhyMemPartSize;
} CoredumpSystemInfo;

typedef struct CoredumpHwInfo {
	uint32_t reservedHeader;
	uint32_t productCode;
	uint32_t productSubCode;
	uint32_t qaFlags[4];
	char qafName[16];
	uint8_t reserved[80];
} CoredumpHwInfo;

typedef struct CoredumpThreadSnapshot {
	ThreadmgrThreadInfoForDebugger info;
	uint32_t waitDetailCaptureTail;
} CoredumpThreadSnapshot;

typedef struct CoredumpThreadInfo {
	uint8_t formatSize[4];
	SceUID threadId;
	char name[32];
	uint32_t initAttr;
	uint32_t attr;
	uint32_t status;
	uintptr_t entry;
	uintptr_t userStackBase;
	SceSize userStackSize;
	uint32_t reservedStack;
	SceSize argSize;
	uintptr_t pArgBlock;
	int initPriority;
	int currentPriority;
	uint32_t initCpuAffinityMask;
	uint32_t cpuAffinityMask;
	int lastCpuId;
	uint32_t waitType;
	SceUID waitId;
	uint32_t runClocksLow;
	uint32_t runClocksHigh;
	CoredumpStopReason stopReason;
	int exitStatus;
	uint32_t intrPreemptCount;
	uint32_t threadPreemptCount;
	uint32_t threadReleaseCount;
	uint32_t changeCpuCount;
	uint8_t formatVersion[4];
	uintptr_t pc;
	uint32_t timeout;
	uint32_t waitDetail[9];
} CoredumpThreadInfo;

typedef struct CoredumpThreadRegInfo {
	uint8_t formatSize[4];
	SceUID threadId;
	ThreadmgrDebugRegisters registers;
	uintptr_t tls;
	uint32_t vfp[64];
	uint32_t fpexc;
	uint32_t tpidrurw;
	uint32_t cpacr;
	uint32_t userDacr;
	uint32_t savedControl;
	uint32_t ifsr;
	uint32_t ifar;
	uint32_t dfsr;
	uint32_t dfar;
} CoredumpThreadRegInfo;

typedef struct CoredumpStackInfo {
	uint32_t reservedHeader;
	SceUID threadId;
	SceSize userStackMaxUsed;
	SceSize userStackUsed;
} CoredumpStackInfo;

typedef struct CoredumpModuleInfo {
	uint32_t reservedHeader;
	SceUID moduleId;
	uint32_t sdkVersion;
	uint8_t version[4];
	uint8_t type;
	uint8_t reservedType;
	uint16_t flags;
	uintptr_t moduleStart;
	uint32_t reservedStart;
	uintptr_t moduleStop;
	uintptr_t moduleExit;
	char name[32];
	uint32_t status;
	uint32_t fingerprint;
	uint32_t nSegments;
} CoredumpModuleInfo;

typedef struct CoredumpFileInfo {
	uint32_t reservedHeader;
	SceUID fd;
	int isDir;
	int flags;
	SceUID pid;
	int mode;
	SceOff position;
	SceOff size;
	uint32_t ioSchedData;
	uint32_t reserved[7];
} CoredumpFileInfo;

typedef struct CoredumpWaitingThread {
	SceUID processId;
	SceUID userThreadId;
} CoredumpWaitingThread;

typedef struct CoredumpSemaphoreInfo {
	uint32_t reservedHeader;
	SceUID semaId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	int initCount;
	int currentCount;
	int maxCount;
	int numWaitThreads;
} CoredumpSemaphoreInfo;

typedef struct CoredumpEventFlagInfo {
	uint32_t reservedHeader;
	SceUID eventFlagId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	uint32_t initPattern;
	uint32_t currentPattern;
	int numWaitThreads;
} CoredumpEventFlagInfo;

typedef struct CoredumpMutexInfo {
	uint32_t reservedHeader;
	SceUID mutexId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	int initCount;
	int currentCount;
	SceUID currentOwnerId;
	int numWaitThreads;
	int ceilingPriority;
} CoredumpMutexInfo;

typedef struct CoredumpLwMutexInfo {
	uint32_t reservedHeader;
	SceUID lwMutexId;
	char name[32];
	uint32_t attr;
	ThreadmgrLwMutexWork *work;
	int initCount;
	int currentCount;
	union {
		SceUID currentOwnerId;
		uint8_t ownerBytes[4];
	};
	int numWaitThreads;
} CoredumpLwMutexInfo;

typedef struct CoredumpCondInfo {
	uint32_t reservedHeader;
	SceUID condId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	SceUID mutexId;
	int numWaitThreads;
} CoredumpCondInfo;

typedef struct CoredumpLwCondInfo {
	uint32_t reservedHeader;
	SceUID lwCondId;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	SceUID lwMutexId;
	int numWaitThreads;
} CoredumpLwCondInfo;

typedef struct CoredumpMsgPipeInfo {
	uint32_t recordSize;
	SceUID msgPipeId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	SceSize bufferSize;
	SceSize freeSize;
	int numSendWaitThreads;
	int numReceiveWaitThreads;
} CoredumpMsgPipeInfo;

typedef struct CoredumpCallbackInfo {
	uint32_t reservedHeader;
	SceUID callbackId;
	SceUID pid;
	SceUID threadId;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	uintptr_t entry;
	void *common;
	uint32_t notifyCount;
	uint32_t notifyArg;
} CoredumpCallbackInfo;

typedef struct CoredumpTimerTime {
	uint32_t low;
	uint32_t high;
} CoredumpTimerTime;

typedef struct CoredumpMetaDataInfo {
	SceUID moduleId;
	SceSize size;
} CoredumpMetaDataInfo;

typedef struct CoredumpFiberInfo {
	uint32_t reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceInt32 state;
	SceUIntPtr addrContext;
	SceUInt32 sizeContext;
	SceUInt32 entry;
	SceUInt32 threadId;
	SceUInt32 fpscr;
	SceUInt32 reg[8];
	SceUInt32 sp;
	SceUInt32 lr;
	SceUInt64 vfp[8];
} CoredumpFiberInfo;

typedef struct CoredumpUltRuntimeInfo {
	uint32_t reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 opaque;
	SceUInt32 numUlts;
	SceUInt32 numUsedUlts;
} CoredumpUltRuntimeInfo;

typedef struct CoredumpUltInfo {
	uint32_t reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 state;
	SceUInt32 opaque0;
	SceUInt32 opaque1;
	SceUInt32 threadId;
	SceUInt32 opaque2;
	SceUIntPtr fiber;
	uint32_t reservedTail;
} CoredumpUltInfo;

typedef struct CoredumpUltMutexInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 attr;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 locked;
	SceUInt32 ownerThreadType;
	SceUInt32 ownerThreadId;
	SceUInt32 numWaitThreads;
} CoredumpUltMutexInfo;

typedef struct CoredumpUltCondInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 reserved;
	SceUIntPtr mutex;
	SceUInt32 numWaitThreads;
} CoredumpUltCondInfo;

typedef struct CoredumpUltSemaInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 reserved;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 count;
	SceUInt32 numWaitThreads;
} CoredumpUltSemaInfo;

typedef struct CoredumpUltRwlockInfo {
	SceUInt32 recordSize;
	SceUIntPtr address;
	char name[32];
	SceUInt32 reserved;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 state;
	SceUInt32 numReaders;
	SceUInt32 numReadWaitThreads;
	SceUInt32 numWriteWaitThreads;
} CoredumpUltRwlockInfo;

typedef struct CoredumpUltQueueInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 reserved;
	SceUIntPtr waitingQueueResourcePool;
	SceUIntPtr queueDataResourcePool;
	SceUInt32 dataSize;
	SceUInt32 numSendWaitThreads;
	SceUInt32 numReceiveWaitThreads;
	SceUInt32 numData;
} CoredumpUltQueueInfo;

typedef struct CoredumpUltWaitingQueueResourcePoolInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 numThreads;
	int numFreeThreads;
	SceUInt32 numSyncObjects;
	int numUsedSyncObjects;
} CoredumpUltWaitingQueueResourcePoolInfo;

typedef struct CoredumpUltQueueDataResourcePoolInfo {
	SceUInt32 reservedHeader;
	SceUIntPtr address;
	char name[32];
	SceUInt32 reserved;
	SceUInt32 numData;
	SceUInt32 numUsedData;
	SceUInt32 dataSize;
	SceUInt32 numQueues;
	int numUsedQueues;
} CoredumpUltQueueDataResourcePoolInfo;

typedef struct CoredumpCommonEventInfo {
	uint32_t pattern;
	uint32_t userData[2];
} CoredumpCommonEventInfo;

typedef struct CoredumpSimpleEventInfo {
	uint32_t recordSize;
	SceUID eventId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint32_t pattern;
	uint32_t userData[2];
	int numWaitThreads;
} CoredumpSimpleEventInfo;

typedef struct CoredumpRWLockInfo {
	SceSize size;
	SceUID rwLockId;
	SceUID pid;
	char name[32];
	SceUInt32 attr;
	uint8_t formatVersion[4];
	SceUInt32 numReadWaitThreads;
	SceUInt32 numWriteWaitThreads;
	SceUID currentOwnerId;
	int lockCount;
} CoredumpRWLockInfo;

typedef struct CoredumpTimerInfo {
	uint32_t recordSize;
	SceUID timerId;
	SceUID pid;
	char name[32];
	uint32_t attr;
	uint8_t formatVersion[4];
	int fActive;
	CoredumpTimerTime baseTime;
	CoredumpTimerTime currentTime;
	CoredumpTimerTime schedule;
	CoredumpTimerTime interval;
	int type;
	int fRepeat;
	unsigned int numWaitThreads;
} CoredumpTimerInfo;

typedef struct CoredumpLibraryInfo {
	uint32_t reservedHeader;
	SceUID libraryId;
	SceUID moduleId;
	uint32_t flags;
	uint8_t reservedVersion[4];
	uint32_t numFunctions;
	uint32_t numVariables;
	uint32_t reservedEntries;
	uint32_t numClients;
} CoredumpLibraryInfo;

_Static_assert(sizeof(CoredumpStopReason) == 20, "coredump stop reason size");
_Static_assert(sizeof(CoredumpProcessInfo) == 88, "coredump process info size");
_Static_assert(sizeof(CoredumpExtnlProcInfo) == 92, "coredump external process info size");
_Static_assert(sizeof(CoredumpBudgetInfo) == 44, "coredump budget info size");
_Static_assert(sizeof(CoredumpPhyMemPartInfo) == 528, "coredump partition info size");
_Static_assert(sizeof(CoredumpArmExInfo) == 16, "coredump ARM exception info size");
_Static_assert(sizeof(CoredumpThreadStopEvent) == 8, "coredump stop event size");
_Static_assert(sizeof(CoredumpBuildVerInfo) == 512, "coredump build version size");
_Static_assert(sizeof(CoredumpRunContext) == 64, "coredump run context size");
_Static_assert(sizeof(CoredumpMemBlockInfo) == 72, "coredump memblock info size");
_Static_assert(sizeof(CoredumpEvent) == 104, "coredump event size");
_Static_assert(sizeof(CoredumpEventQueue) == 1052, "coredump queue size");
_Static_assert(sizeof(CoredumpCorefileInfo) == 36, "coredump corefile info size");
_Static_assert(sizeof(CoredumpCafSectionHash) == 48, "coredump CAF section hash size");
_Static_assert(sizeof(CoredumpSystemInfo) == 92, "coredump system info size");
_Static_assert(sizeof(CoredumpHwInfo) == 124, "coredump hardware info size");
_Static_assert(sizeof(CoredumpThreadSnapshot) == 300, "coredump thread snapshot size");
_Static_assert(sizeof(CoredumpThreadInfo) == 200, "coredump thread info size");
_Static_assert(sizeof(CoredumpThreadRegInfo) == 376, "coredump thread registers size");
_Static_assert(sizeof(CoredumpStackInfo) == 16, "coredump stack info size");
_Static_assert(sizeof(CoredumpModuleInfo) == 80, "coredump module info size");
_Static_assert(sizeof(CoredumpLibraryInfo) == 36, "coredump library info size");
_Static_assert(sizeof(CoredumpFileInfo) == 72, "coredump file info size");
_Static_assert(sizeof(CoredumpSemaphoreInfo) == 68, "coredump semaphore info size");
_Static_assert(sizeof(CoredumpWaitingThread) == 8, "coredump waiting thread size");
_Static_assert(sizeof(CoredumpEventFlagInfo) == 64, "coredump event flag info size");
_Static_assert(sizeof(CoredumpCondInfo) == 60, "coredump condition info size");
_Static_assert(sizeof(CoredumpMutexInfo) == 72, "coredump mutex info size");
_Static_assert(sizeof(CoredumpLwMutexInfo) == 64, "coredump lw mutex info size");
_Static_assert(sizeof(CoredumpLwCondInfo) == 56, "coredump lw condition info size");
_Static_assert(sizeof(CoredumpMsgPipeInfo) == 68, "coredump message pipe info size");
_Static_assert(sizeof(CoredumpCallbackInfo) == 72, "coredump callback info size");
_Static_assert(sizeof(CoredumpTimerTime) == 8, "coredump timer time size");
_Static_assert(sizeof(CoredumpTimerInfo) == 100, "coredump timer info size");
_Static_assert(sizeof(CoredumpRWLockInfo) == 68, "coredump rwlock info size");

extern CoredumpRunContext coredumpRun;

int _getThreadStopReason(SceUID threadId,
	const CoredumpThreadStopEvent *stopEvent, CoredumpStopReason *stopReason);
int _getCdnPcAndLrInfo(SceUID pid, const CoredumpThreadStopEvent *stopEvent,
	CoredumpCdnPcAndLrInfo *location);
