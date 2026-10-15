#pragma once

#include <stddef.h>

#include <sce/cpu.h>
#include <sce/fast_mutex.h>
#include <sce/venezia_rpc.h>
#include <sce/pervasive_regs.h>

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


int ksceKernelInitializeFastMutex(struct ThreadmgrFastMutex *mutex,
				  const char *name, int attr, void *opt);
int ksceKernelUnlockFastMutex(struct ThreadmgrFastMutex *mutex);
int ksceKernelLockFastMutex(struct ThreadmgrFastMutex *mutex);
int ksceKernelFinalizeFastMutex(struct ThreadmgrFastMutex *mutex);

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
_Static_assert(sizeof(CodecEngineProcessStorage) == 100, "CodecEngine process storage size");typedef struct VeneziaPerformanceResult {
	unsigned int usage[8];
	unsigned int average;
	unsigned int maximum;
} VeneziaPerformanceResult;


