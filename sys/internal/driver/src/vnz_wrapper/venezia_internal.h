#pragma once

#include <stddef.h>

#include <sysmem/cpu.h>
#include <threadmgr/fast_mutex.h>
#include <vnz_wrapper/venezia_rpc.h>
#include <vnz_wrapper/vnz_wrapper.h>
#include <lowio/pervasive_regs.h>
#include <lowio/mailbox_regs.h>
#include <dmacmgr/dmacmgr.h>
#include <sysmem/uid_class.h>
#include <vnz_wrapper/venezia_debug.h>
#include <threadmgr/threadmgr.h>

/* name unknown: per-process Venezia suspend and clock bookkeeping. */
typedef struct VeneziaProcessStorage {
	struct ThreadmgrFastMutex suspendMutex;
	volatile SceKernelSpinlock suspendLock;
	unsigned int suspendLockCount;
	unsigned int clockLockCount;
	int suspendInProgress;
	unsigned int suspendPollCount;
} VeneziaProcessStorage;

_Static_assert(sizeof(VeneziaProcessStorage) == 0x54, "Venezia process storage size");

/* name unknown: adjacent global process-storage and clock state. */
typedef struct VeneziaState {
	int processStorageKey;
	SceKernelSpinlock clockLock;
	unsigned int clockLockCount;
} VeneziaState;

extern VeneziaState g_veneziaState; /* name unknown */

/* name unknown: physical memory ranges and their mapped block handles. */
typedef struct VeneziaBootMemory {
	SceUID imageBlock;
	unsigned int imagePhysicalBase;
	void *imageBase;
	SceSize imageSize;
	SceUID registerBlock;
	void *registerBase;
} VeneziaBootMemory;

extern VeneziaBootMemory g_veneziaBootMemory; /* name unknown */

typedef struct VeneziaKernelMemory {
	unsigned int base;
	SceSize size;
} VeneziaKernelMemory; /* name unknown */

extern VeneziaKernelMemory g_veneziaKernelMemory; /* name unknown */

/* name unknown: DSP RPC argument packets and channel allocation state. */
typedef struct VeneziaRpcArg {
	unsigned int command;
	unsigned int sequence;
	unsigned int arguments[4];
	int result;
	unsigned int resultFlags;
	SceUID pid;
	unsigned int extendedArguments[55];
} VeneziaRpcArg;

typedef struct VeneziaRpcControl {
	unsigned int bootState;
	unsigned int mailboxWords[255];
} VeneziaRpcControl;

typedef struct VeneziaRpcMemory {
	VeneziaRpcControl control;
	VeneziaRpcArg arguments[16];
} VeneziaRpcMemory;

typedef struct VeneziaRpcChannel {
	int inUse;
	unsigned int flags;
} VeneziaRpcChannel;

typedef struct VeneziaRpcState {
	SceUID memoryBlock;
	unsigned int physicalBase;
	VeneziaRpcMemory *memory;
	SceSize memorySize;
	SceUID semaphore;
	volatile SceKernelSpinlock lock;
	unsigned int lastChannel;
	unsigned int freeChannels;
	unsigned int sequence;
	unsigned int debugSequence;
	VeneziaRpcChannel channels[16];
	SceUID debugSemaphore;
	SceKernelSpinlock debugLock;
	unsigned int freeDebugChannels;
	unsigned int savedCommunicationMemory[0x500];
} VeneziaRpcState;

_Static_assert(sizeof(VeneziaRpcArg) == 0x100, "Venezia RPC argument size");
_Static_assert(sizeof(VeneziaRpcState) == 0x14b4, "Venezia RPC state size");
extern VeneziaRpcState g_veneziaRpcState;

int _callGenericThunk(unsigned int command, const unsigned int *input,
		      VeneziaRpcPack pack, VeneziaRpcUnpack unpack, unsigned int flags);
int _callDebugThunk(unsigned int command, const unsigned int *input,
		    VeneziaRpcPack pack, VeneziaRpcUnpack unpack);

/*
 * The RPC calls of dsp0_rpc.c for a caller that passes fewer than four argument words: the DSP
 * gets the rest as the caller left the registers (names ours).
 */
int veneziaRpc(unsigned int command, ...) __asm__("callVeneziaRpc");
int veneziaRpcWithoutClockLock(unsigned int command, ...) __asm__("callVeneziaRpcWithoutClockLock");
int veneziaDebugRpc(unsigned int command, ...) __asm__("callVeneziaDebugRpc");

/* name unknown: public snapshot of one RPC channel. */
typedef struct VeneziaRpcHistoryEntry {
	unsigned int resultFlags;
	SceUID pid;
	unsigned int channel;
	unsigned int command;
	unsigned int arguments[4];
	int result;
} VeneziaRpcHistoryEntry;

/* name unknown: simple-heap free-list descriptors and caller-owned storage. */
typedef struct VeneziaSimpleHeapBlock {
	unsigned int next;
	SceSize size;
} VeneziaSimpleHeapBlock;

typedef struct VeneziaSimpleHeap {
	uintptr_t marker;
	void *base;
	SceSize size;
	SceSize blockSize;
	unsigned int lastFreeBlock;
	unsigned int numBlocks;
	volatile SceKernelSpinlock lock;
	VeneziaSimpleHeapBlock blocks[128];
} VeneziaSimpleHeap;

_Static_assert(sizeof(VeneziaSimpleHeap) == 0x41c, "Venezia simple heap size");

typedef struct VeneziaCommunicationMemory {
	struct ThreadmgrFastMutex mutex;
	SceUID event;
	VeneziaSimpleHeap heap;
} VeneziaCommunicationMemory;

extern VeneziaCommunicationMemory g_veneziaCommunicationMemory;

/* name unknown: variable-size heap free blocks and large-alignment descriptors. */
typedef struct UnitFixedCtrl {
	struct UnitFixedCtrl *next;
	unsigned int units;
} UnitFixedCtrl;

typedef struct VeneziaHeap {
	uintptr_t marker;
	unsigned char *base;
	SceSize size;
	unsigned int allocatedUnits;
	UnitFixedCtrl *lastFreeBlock;
	uintptr_t regionBase;
	unsigned int regionCount;
	UnitFixedCtrl regions[];
} VeneziaHeap;

typedef struct VeneziaHeapEntry {
	struct VeneziaHeapEntry *next;
	struct VeneziaHeapEntry *prev;
	VeneziaHeap heap;
} VeneziaHeapEntry;

typedef struct VeneziaHeapManager {
	unsigned int magic;
	void *base;
	SceSize size;
	VeneziaHeapEntry *heaps;
	SceKernelSpinlock lock;
} VeneziaHeapManager;

_Static_assert(sizeof(VeneziaHeapEntry) == 36, "Venezia heap entry size");

extern void *getVeneziaImageBase(void);
extern unsigned int getVeneziaImagePhysicalBase(void);
extern SceSize getVeneziaImageSize(void);
extern VeneziaRpcMemory *getRpcMemoryBase(void);
extern unsigned int getRpcMemoryPhysicalBase(void);
extern SceSize getRpcMemorySize(void);

static inline __attribute__((always_inline)) unsigned int veneziaPhysicalToVirtual(unsigned int physical)
{
	unsigned char *imageBase = getVeneziaImageBase();
	unsigned int imagePhysical = getVeneziaImagePhysicalBase();
	SceSize imageSize = getVeneziaImageSize();
	unsigned char *rpcBase = (unsigned char *)getRpcMemoryBase();
	unsigned int rpcPhysical = getRpcMemoryPhysicalBase();
	SceSize rpcSize = getRpcMemorySize();

	if (imageBase) {
		if (imagePhysical <= physical) {
			if (physical < imageSize + imagePhysical)
				return (uintptr_t)(imageBase + (physical - imagePhysical));
		}
	}
	if (rpcBase) {
		if (rpcPhysical <= physical) {
			if (physical < rpcSize + rpcPhysical)
				return (uintptr_t)(rpcBase + (physical - rpcPhysical));
		}
	}
	return 0;
}

/* name unknown: the complete CodecEngine process-owned memory and processor state. */
typedef struct CodecEngineUnmapMemBlockEntry {
	SceUID puid;
	SceUID guid;
} CodecEngineUnmapMemBlockEntry;

typedef struct CodecEngineProcessStorage {
	struct ThreadmgrFastMutex mutex;
	CodecEngineUnmapMemBlockEntry unmapMemBlocks[4];
	unsigned int processorCount;
} CodecEngineProcessStorage;
_Static_assert(sizeof(CodecEngineProcessStorage) == 100, "CodecEngine process storage size");

typedef struct VeneziaPerformanceResult {
	unsigned int usage[8];
	unsigned int average;
	unsigned int maximum;
} VeneziaPerformanceResult;

/* the caller's VIP input buffers, read through an iterator (vip/hwd0_vapa.c) */
typedef struct VeneziaVipInputBufferList {
	unsigned int count;
	const VeneziaVipInputBuffer *buffers;
} VeneziaVipInputBufferList;

/* name unknown: cursor through the caller's scatter/gather input buffers. */
typedef struct VeneziaVipInputIterator {
	unsigned int bufferIndex;
	SceSize bufferOffset;
	SceSize position;
	SceSize totalSize;
	const VeneziaVipInputBufferList *list;
} VeneziaVipInputIterator;

int initializeVeneziaVipInputIterator(VeneziaVipInputIterator *iterator,
	const VeneziaVipInputBufferList *list);
SceSize getVeneziaVipInputRemaining(const VeneziaVipInputIterator *iterator);
int copyVeneziaVipInput(uint32_t destination, SceSize size, SceSize *copied,
	VeneziaVipInputIterator *iterator);

/* the VIP, debug-capture, HWD DMA and CodecEngine state the module's files share */
/* PLACEHOLDER: live VIP module controls outside the mapped register and callback
 * fields remain unidentified. The owner initializes the entire 0x20c allocation.
 */
typedef struct VeneziaVipProcessorRequest {
	int captureIndex;
	uint32_t command;
	uint32_t resultFlags;
} VeneziaVipProcessorRequest;

typedef struct VeneziaDebugChannelRecord {
	uint32_t header[2];
	uint32_t flags;
	struct {
		uint32_t control[2];
		uint32_t parameter;
	} records[3];
} VeneziaDebugChannelRecord;

/* PLACEHOLDER: the remaining per-process capture controls are still unidentified.
 * These are live state words, not unused padding. The complete owner allocation
 * and all four capture records are represented here.
 */
typedef struct VeneziaVipMemoryRegion {
	uintptr_t physicalAddress;
	uint32_t virtualAddress;
	SceSize size;
} VeneziaVipMemoryRegion;

typedef struct VeneziaVipMemoryBlock {
	SceUID uid;
	VeneziaVipMemoryRegion region;
} VeneziaVipMemoryBlock;

typedef struct VeneziaVipSnapshotRegion {
	uint32_t physicalAddress;
	uint32_t virtualAddress;
	SceSize size;
	uint32_t copyAddress;
} VeneziaVipSnapshotRegion;

typedef struct VeneziaVipSnapshotStorage {
	VeneziaVipMemoryRegion buffer;
	uint32_t unidentifiedControl;
	uint8_t memoryRegionCount;
	uint8_t processorCount;
	uint8_t reserved[2];
	VeneziaVipSnapshotRegion regions[16];
} VeneziaVipSnapshotStorage;

typedef struct VeneziaVipMemoryLayout {
	uint32_t control;
	VeneziaVipMemoryRegion scratch;
	VeneziaVipMemoryRegion buffer;
	VeneziaVipMemoryRegion snapshot;
} VeneziaVipMemoryLayout;

typedef struct VeneziaVipCaptureImageHeader VeneziaVipCaptureImageHeader;

struct VeneziaVipCommandContext;
struct VeneziaVipCommandAuxiliaryRecord;

typedef struct VeneziaDebugCallback {
	SceUID pid;
	SceUID uid;
	uint32_t pendingEvents;
	uint32_t eventMask;
} VeneziaDebugCallback;

typedef struct VeneziaDebugProcessCapture {
	uint32_t flags;
	uint32_t completedCount;
	uint32_t bufferEnd;
	uint32_t captureVirtualAddress;
	uint32_t capturePhysicalAddress;
	VeneziaVipCaptureParam captureParam;
	VeneziaDebugImageFormat imageFormat;
	union {
		VeneziaDebugSnapshot *snapshot;
		SceUID eventFlag;
	};
	VeneziaVipCaptureImageHeader *imageHeader;
	uint32_t debugResultFlags;
	uint32_t processorCompletionCounts[2];
	uint32_t processorPendingCounts[2];
	struct VeneziaVipCommandContext *commandContext;
	VeneziaDebugCallback callbacks[6];
	uint32_t unidentifiedCaptureState[2];
	uint32_t recoveryFlags;
	uint32_t processorContextState[2];
	uint32_t pendingProcessorEvents[2];
	uint32_t ringReadAddress;
	uint32_t ringEndAddress;
	uint32_t ringWriteAddress;
	uint32_t ringCommitAddress;
	uint32_t unidentifiedRingPendingCount;
	uint32_t ringPendingAddress;
	uint32_t unidentifiedRingResult;
	SceSize ringSize;
	uint32_t ringMode;
	uint32_t queuedAuxiliaryCount;
	const struct VeneziaVipCommandAuxiliaryRecord *auxiliaryRecord;
	uint32_t unidentifiedCaptureCounter;
	uint32_t unidentifiedCaptureStatus;
	uint32_t unidentifiedBeforeCaptureMemory;
	VeneziaVipMemoryRegion captureMemory;
	VeneziaVipMemoryRegion scratchRegion;
	VeneziaVipMemoryRegion bufferRegion;
	VeneziaVipMemoryRegion snapshotRegion;
	uintptr_t imagePhysicalAddress;
	uint32_t imageVirtualAddress;
	uint32_t unidentifiedImageControl;
	SceSize imageSize;
	uint32_t imageMappingMode;
} VeneziaDebugProcessCapture;

typedef struct VeneziaDebugEventCounter {
	uint32_t count;
	uint32_t unidentifiedControl;
} VeneziaDebugEventCounter;

typedef struct VeneziaDebugCallbackState {
	VeneziaDebugEventCounter counters[13];
	VeneziaDebugCallback callbacks[6];
} VeneziaDebugCallbackState;

typedef struct VeneziaVipProcessQueue {
	uint8_t entries[4];
	uint8_t count;
	uint8_t writeIndex;
	uint8_t reserved[2];
} VeneziaVipProcessQueue;

typedef struct VeneziaVipProcessStorage {
	uint32_t state;
	uint32_t count;
	SceUID suspendSemaphore;
	uint32_t mode;
	int processors[4];
	VeneziaDebugProcessCapture captures[4];
	VeneziaVipMemoryLayout *memoryLayout;
	VeneziaVipMemoryLayout captureMemory;
	VeneziaDebugCallbackState callbackState;
	VeneziaVipSnapshotStorage snapshotStorage;
	VeneziaVipProcessQueue queues[2];
	uint32_t pendingCounters[2];
	uint32_t status[3];
} VeneziaVipProcessStorage;

typedef struct VeneziaVipState {
	uint32_t magic;
	SceUID eventFlag;
	struct ThreadmgrFastMutex processMutex;
	struct ThreadmgrFastMutex *processLock;
	VeneziaKernelMemory physicalRanges[2];
	VeneziaVipMemoryBlock codeMemory;
	VeneziaVipMemoryRegion debuggerMemory;
	SceUID resetBlock;
	ScePervasiveResetRegisters *resetRegisters;
	SceUID mailboxBlock;
	ScePervasiveMailboxRegisters *mailboxRegisters;
	SceUID tasBlock;
	ScePervasiveTasRegisters *tasRegisters;
	ScePervasive2Registers *bootRegisters;
	VeneziaDebugCallbackState *callbackState;
	struct ThreadmgrFastMutex apiWrapperMutex;
	struct ThreadmgrFastMutex *apiWrapperLock;
	struct ThreadmgrFastMutex standardBufferMutex;
	struct ThreadmgrFastMutex *standardBufferLock;
	struct ThreadmgrFastMutex commonProcessorMutex;
	struct ThreadmgrFastMutex *commonProcessorLock;
	uint32_t unidentifiedProcessorControls[2];
	VeneziaVipProcessorRequest processorRequests[2];
	uint32_t processorPendingCommands[2];
	struct ThreadmgrFastMutex requestMutex;
	struct ThreadmgrFastMutex *requestLock;
	SceUID processorEvents[2];
	SceUID processorThreads[2];
	union {
		struct {
			SceUID auxiliaryEventFlag;
			SceUID secondaryInterruptEvent;
		};
		SceUID captureInterruptEvents[2];
	};
	VeneziaVipProcessStorage *activeProcess;
	int processorActiveCaptures[2];
	uint32_t processorActiveEvents[2];
	uint32_t processorActiveCommands[2];
	uint32_t processorRecoveryEvents[2];
} VeneziaVipState;

/* name unknown: full 192-KiB command memory region supplied by VIP initialization.
 * Unidentified protocol banks remain placeholders until their users are converted. */
typedef struct VeneziaVipCommandQueueSlot {
	uint32_t address;
	uint8_t unidentifiedControl[3];
	uint8_t busy;
} VeneziaVipCommandQueueSlot;

typedef struct VeneziaVipCommandQueue {
	uint32_t base;
	uint32_t physicalBase;
	union {
		uint8_t flags[4];
		struct {
			uint8_t accessBlocked;
			uint8_t unidentifiedActivity[3];
		};
	};
	SceSize size;
	VeneziaVipCommandQueueSlot slots[4];
} VeneziaVipCommandQueue;

typedef struct VeneziaVipCommandContext {
	uint32_t magic;
	uint32_t physicalBase;
	uint8_t processorMode[2];
	uint8_t processorState[2];
	uint32_t commandValues[2];
	uint16_t payloadOffsets[2];
	uint32_t bufferBase;
	SceSize bufferOffset;
	SceSize auxiliaryBufferOffset;
	uint32_t captureParameter;
	uint32_t captureAddress;
	uint32_t captureOption;
	uint32_t unidentifiedBeforeQueues;
	int primarySlot;
	VeneziaVipCommandQueue *queues;
	int secondarySlot;
	VeneziaVipCommandQueue *auxiliaryQueue;
	VeneziaVipMemoryRegion captureMemory;
	SceSize captureLength;
	VeneziaVipCommandQueue *activeQueue;
	uint8_t resultFlags;
	uint8_t unidentifiedAfterFlags[3];
	uint32_t contextValue;
	uint8_t payload[];
} VeneziaVipCommandContext;

/* name unknown: auxiliary ring records carry an address in the main result buffer. */
typedef struct VeneziaVipCommandAuxiliaryRecord {
	uint32_t value;
	uint16_t control;
	uint16_t unidentifiedControl;
	uint32_t resultAddress;
	uint32_t unidentifiedTail;
} VeneziaVipCommandAuxiliaryRecord;

/* name unknown: result metadata preserves ten protocol words and image dimensions. */
typedef union VeneziaVipCommandResultMetadata {
	uint32_t words[12];
	struct {
		uint16_t operandLow, operandHigh;
		uint16_t instructionLow, instructionHigh;
		uint16_t unidentifiedInstruction[2];
		uint16_t addressLow, addressHigh;
		uint32_t unidentifiedBeforeContextControl[4];
		uint32_t contextControl;
		uint32_t unidentifiedAfterContextControl;
		union {
			uint32_t contextAddress;
			struct {
				uint16_t width;
				uint16_t height;
			};
		};
		uint32_t unidentifiedTail;
	};
} VeneziaVipCommandResultMetadata;

/* name unknown: decoded auxiliary value and its otherwise unidentified control bit. */
typedef struct VeneziaVipCommandAuxiliaryStatus {
	uint32_t value;
	uint32_t controlFlag;
} VeneziaVipCommandAuxiliaryStatus;

/* PLACEHOLDER: the first common-debug bank words and remaining page controls
 * have unidentified meanings. The named tail is decoded by the capture owner. */
typedef struct VeneziaVipCommonDebugData {
	uint32_t unidentifiedRegisters[136];
	uint32_t executionControl;
	uint32_t executionStatus;
	uint32_t pipelineControl;
	uint32_t programCounter;
	uint32_t stackPointer;
	uint32_t unidentifiedTail;
} VeneziaVipCommonDebugData;

struct VeneziaVipCaptureImageHeader {
	VeneziaVipCommandResultMetadata metadata;
	VeneziaVipCommonDebugData commonDebugData;
	uint32_t debugData[2][130];
	VeneziaDebugChannelRecord channels[2];
	uint32_t statusData[2][2];
	uint32_t secondaryData[2][16];
	uint32_t controlData[2][4];
	VeneziaVipCommandAuxiliaryStatus auxiliaryStatus;
	uint32_t captureBase;
	uint32_t captureNext;
	SceSize captureLength;
	uint32_t unidentifiedTail[537];
};

/* name unknown: the complete eight-transfer blocking-copy request. */
typedef struct VeneziaHwdDmaTransfer {
	uint32_t unidentifiedTransferControl;
	const void *source;
	void *destination;
	SceSize size;
	uint32_t unidentifiedTransferFlags;
} VeneziaHwdDmaTransfer;

typedef struct VeneziaHwdDmaRequest {
	unsigned int channel;
	unsigned int count;
	VeneziaHwdDmaTransfer transfers[8];
} VeneziaHwdDmaRequest;

typedef struct VeneziaHwdDmaState {
	struct ThreadmgrFastMutex mutex;
	struct ThreadmgrFastMutex *activeMutex;
	SceKernelDmaOpId operations[1];
} VeneziaHwdDmaState;

/* name unknown: complete processor-count owner, adjacent to its mutex. */
typedef struct CodecEngineProcessorState {
	struct ThreadmgrFastMutex mutex;
	unsigned int processorCount;
} CodecEngineProcessorState;

/* Full memory UID subclass; the base is Sysmem's canonical object header. */
typedef struct CodecEngineMemory {
	SceObjectBase object;
	void *userBase;
	void *kernelBase;
	uintptr_t physicalBase;
	SceSize size;
	SceUID userMemBlock;
	SceUID kernelMemBlock;
	VeneziaHeapManager heapManager;
} CodecEngineMemory;

extern VeneziaVipState g_veneziaVipState;

/* codecengine/codecengine.c */
int initCodecEngine(void);
int _sceCodecEngineProcessCreateEntry(SceUID pid, SceUInt32 type);
int _sceCodecEngineProcessExitEntry(SceUID pid, SceUInt32 type);
CodecEngineProcExitHandler *getProcExitHandlers(void);
CodecEngineProcExitHandler *getProcKillHandlers(void);
int _sceCodecEngineProcessSuspendPhase0(SceUID pid);
int _sceCodecEngineProcessResume(SceUID pid);
struct ThreadmgrFastMutex *_sceCodecEngineGetMutex(SceUID pid);
CodecEngineUnmapMemBlockEntry *_sceCodecEngineGetUnmapMemBlockEntry(SceUID pid, SceUID guid);
unsigned int *_sceCodecEngineGetProcessor(SceUID pid);

/* codecengine/memory.c */
int initializeCodecEngineMemoryClass(void);
int isVeneziaPhysicalRangeContiguous(const void *base, SceSize size);
int sceCodecEngineOpenUnmapMemBlockForUser(void *base, SceSize size);
int _closeUnmapMemBlock(SceUID puid);
void *sceCodecEngineAllocMemoryFromUnmapMemBlockForUser(SceUID puid, SceSize size,
		SceSize alignment);
int sceCodecEngineFreeMemoryFromUnmapMemBlockForUser(SceUID puid, void *address);
int _closeUnmapMemBlockAll(SceUID pid);

/* codecengine/processor.c */
int initializeCodecEngineProcessor(void);
int _sceCodecEngineProcessorProcessSuspend(SceUID pid);
int _sceCodecEngineProcessorProcessResume(SceUID pid);
int sceCodecEngineChangeNumWorkerCoresDefaultForUser(void);
int sceCodecEngineChangeNumWorkerCoresMaxForUser(void);

/* dsp0_debug.c */
int resumeHwd(void);
int initializeVeneziaVipProcessStorage(VeneziaVipProcessStorage *storage);
int initializeVeneziaVipResources(void);
int startVeneziaVipDriver(int mode);
int dispatchVeneziaVipProcessSuspend(VeneziaVipProcessStorage *storage, int reason);
int dispatchVeneziaVipProcessResume(VeneziaVipProcessStorage *storage);
int dispatchVeneziaVipProcessReset(VeneziaVipProcessStorage *storage);

/* dsp0_mailbox.c */
int initVeneziaMailbox(void);
int enableVeneziaMailboxInterrupts(void);
int submitVeneziaRpc(unsigned int channel);
int waitVeneziaRpc(unsigned int channel);
unsigned int getCompletedVeneziaRpc(void);
int callVeneziaRpcCommand500(unsigned int argument0, unsigned int argument1);

/* entry.c */
extern const SceKernelProcEventHandler codecEngineProcessEvents; /* name unknown */
unsigned int codecEngineSystemEventHandler(int phase, unsigned int event);
int codecEngineStartPhase2(void *arg);

/* venezia/booter.c */
int allocVeneziaImage(void);
int _sceVeneziaBootPhase2(void);
int _sceVeneziaSuspend(void);
int _sceVeneziaResume(void);
volatile ScePervasive2Registers *getVeneziaBootRegisters(void);

/* venezia/dsp0_rpc.c */
int initVeneziaRpc(void);
int _sceVeneziaRpcClockDown(void);
int callVeneziaRpcThunkWithoutClockLock(unsigned int command, const unsigned int *input,
		VeneziaRpcPack pack, VeneziaRpcUnpack unpack);
int waitVeneziaRpcSemas(void);
int signalVeneziaRpcSemas(void);
unsigned int getRpcFreeChannelCount(void);
void *getRpcHeapBase(void);
SceSize getRpcHeapSize(void);
unsigned int getRpcBootState(void);
unsigned int setRpcBootState(unsigned int state);
int _sceVeneziaRpcGetHistory(VeneziaRpcHistoryEntry *history);
void _sceVeneziaRpcPrintBlockingCalls(SceUID pid);
int callVeneziaDebugRpcThunk(unsigned int command, const unsigned int *input, VeneziaRpcPack pack,
		VeneziaRpcUnpack unpack);
unsigned int getRpcFreeDebugChannelCount(void);
int _sceVeneziaStoreContext(void);
int _sceVeneziaLoadContext(void);

/* venezia/heap.c */
void _sceVeneziaHeapInit(void *base, VeneziaHeap *heap, SceSize size, SceSize regionSize);
void *_storeUnitFixedCtrl(VeneziaHeap *heap, SceSize size, SceSize alignment);
int freeVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize unused);
int registerVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
int unregisterVeneziaHeapMemory(VeneziaHeap *heap, void *memory, SceSize size);
int _sceVeneziaHeapIsRegistered(VeneziaHeap *heap, unsigned int unused0, unsigned int unused1);

/* venezia/heapmgr.c */
int initVeneziaHeapManager(VeneziaHeapManager *manager, void *base, SceSize size);
void *_sceVeneziaHeapManagerAllocMemory(VeneziaHeapManager *manager, SceSize size,
		SceSize alignment);
int _procMemory(VeneziaHeapManager *manager, void *memory);
int registerVeneziaHeapManagerMemory(VeneziaHeapManager *manager, void *memory, SceSize size);
int unregisterVeneziaHeapManagerMemory(VeneziaHeapManager *manager, void *memory, SceSize size);
int isVeneziaHeapManagerRegistered(VeneziaHeapManager *manager);

/* venezia/simple_heap.c */
void _sceVeneziaSimpleHeapInit(void *base, SceSize size, VeneziaSimpleHeap *heap, SceSize workSize,
		SceSize blockSize);
void *_sceVeneziaSimpleHeapAllocMemory(VeneziaSimpleHeap *heap, SceSize size);
int _sceVeneziaSimpleHeapFreeMemory(VeneziaSimpleHeap *heap, void *memory);

/* venezia/system.c */
int initCodecEnginePerformanceMonitor(void);
int resumePerformanceMonitor(void);
int startPerformanceMonitorSuspend(void);
int isPerformanceMonitorSuspended(void);
int startCodecEnginePerformanceMonitor(void);
int stopCodecEnginePerformanceMonitor(void);
int getCodecEnginePerformanceResults(VeneziaPerformanceResult *result);
int clearCodecEnginePerformanceCounters(void);
int setVeneziaClockSelector(unsigned int selector);
unsigned int getVeneziaPreferredClock(void);
int veneziaClockSelector(int frequency);

/* venezia/vapa.c */
int initVeneziaRpcDma(void);
void unpackVeneziaRpcMemory(VeneziaRpcThunk *thunk);
int packVeneziaRpcWord(VeneziaRpcThunk *thunk, const unsigned int *arguments, unsigned int *packed);
int packVeneziaRpcBuffer64(VeneziaRpcThunk *thunk, const unsigned int *arguments,
		unsigned int *packed);

/* venezia/venezia.c */
int _sceVeneziaProcessCreateEntry(void);
int _sceVeneziaStartEntryPhase2(void);
int _sceVeneziaProcessCreateEntryInternal2(SceUID pid, SceUInt32 type);
int _sceVeneziaProcessExitEntry(SceUID pid, SceUInt32 type);
int _lockProcessSuspendCore(void);
int _sceVeneziaStartProcessSuspend(SceUID pid);
int _sceVeneziaProcessSuspendPhase0(SceUID pid);
int _sceVeneziaProcessResume(SceUID pid);
int _sceVeneziaStartSystemSuspend(void);
int _sceVeneziaIsSystemSuspended(void);
int suspendSystem(void);
int _sceVeneziaSystemResumePhase0(void);
int resumeSystem(void);
void _sceVeneziaPrintBlockingCalls(SceUID pid);
unsigned int getVeneziaClockLockCount(void);
int _sceVeneziaRunInBackground(void);
int _sceVeneziaStopInBackground(void);

/* venezia/vnzmem.c */
int initVeneziaRpcCommunicationMemory(void);
int finalizeVeneziaRpcCommunicationMemory(void);
int lockVeneziaCommunicationMemory(void);
int unlockVeneziaCommunicationMemory(void);
int signalVeneziaCommunicationMemory(unsigned int channel);
void *_sceVeneziaAllocCommunicationMemory(SceSize size);
int freeVeneziaCommunicationMemory(void *memory);

/* vip/hwd0_vapa.c */
int _hwd0DmacMemcpyBlocking(const VeneziaHwdDmaRequest *request);
int allocateVeneziaVipMemoryBlock(VeneziaVipMemoryBlock *block, const char *name,
		unsigned int type, SceSize size, uintptr_t physicalAddress);
int releaseVeneziaVipMemoryBlock(VeneziaVipMemoryBlock *block);
int offsetVeneziaVipMemoryRegion(VeneziaVipMemoryRegion *destination,
		const VeneziaVipMemoryRegion *source, SceSize offset);
int setVeneziaVipMemoryRegion(VeneziaVipMemoryRegion *destination,
		const VeneziaVipMemoryRegion *source, SceSize offset, SceSize size);
int initializeVeneziaHwdDma(void);
unsigned int decodeVeneziaVipNalLength(const uint8_t *bytes, unsigned int size);

/* vip_fw_ver2/vip_process.c */
int initializeVeneziaVipProcesses(void);
int _sceHwd0ProcessCreateEntry(SceUID pid, SceUInt32 type);
int releaseVeneziaVipProcessStorage(SceUID pid, SceUInt32 type);
VeneziaVipProcessStorage *_sceVipGetVipProcessResource(SceUID pid);
int _sceVipCheckProcessSuspend(void);
int _sceHwd0StartProcessSuspend(SceUID pid);
int isHwdProcessSuspended(SceUID pid);
int _sceHwd0ProcessResume(SceUID pid);
