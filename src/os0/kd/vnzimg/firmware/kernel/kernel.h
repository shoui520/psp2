/*
 * The Venezia kernel: a small multi-core real-time kernel.
 *
 * Each core runs its own threads by priority (1 is highest, 9 the lowest a
 * thread may have; 0 is the core's idle thread). A thread stays on the core it
 * was created on. The cores share the kernel's state in the image: what only a
 * core itself touches is reached through its physical address, what the cores
 * share through the public (L2) window, under the hardware semaphores.
 *
 * Interrupt handlers return the event flags to set in the thread that registered
 * them; that is how a driver thread waits for its device. A core rings another
 * core's doorbell when it readies one of that core's threads.
 */
#pragma once

#include <vnz.h>

#define VK_SECTION(name)	__attribute__((section(name)))
#define VK_LOCAL		VK_SECTION(".kernel.local")	/* reached physically */
#define VK_SHARED		VK_SECTION(".kernel.shared")	/* reached through L2 */

#define VK_PRIORITIES		9	/* 1 to 9 */
#define VK_IDLE_PRIORITY	0

/* the hardware semaphores the kernel uses */
#define VK_SEMAPHORE_THREADS	0	/* + core: a core's run queue (8) */
#define VK_SEMAPHORE_MOVES	8	/* threads moving between cores */
#define VK_SEMAPHORE_POOLS	9	/* + pool: the kernel's page pools (2) */
#define VK_SEMAPHORE_PAGES	11	/* the lock pages (vkLockPageAllocate) */
#define VK_SEMAPHORE_POWER	12	/* suspend and resume */
#define VK_SEMAPHORE_INTERRUPTS	13	/* the interrupt tables */
#define VK_SEMAPHORE_EXITS	14	/* the count of exited threads */
#define VK_SEMAPHORE_HEAPS	15	/* the heap list */

#define vkSemaphore(n)		((volatile uint32_t *)(VNZ_SEMAPHORES + 4 * (n)))

/* thread states (VkThread.state, low four bits) and what a waiting thread waits for */
#define VK_STATE_MASK		0x0f
#define VK_STATE_READY		1
#define VK_STATE_WAITING	2
#define VK_STATE_RUNNING	3
#define VK_STATE_DORMANT	4	/* created, or ended with nothing to report */
#define VK_STATE_EXITED		5	/* ended with an exit status to collect */
#define VK_WAIT_MASK		0x30
#define VK_WAIT_SLEEP		0x00	/* until vkThreadWakeup */
#define VK_WAIT_CHILDREN	0x10	/* until the children have ended */
#define VK_WAIT_FLAGS		0x20	/* until an event flag it waits for is set */

/* VkThread.attributes */
#define VK_THREAD_MOVING	0x80	/* moving to another core (kernel/service.c) */

#define VK_NO_CORE		8	/* VkThread.core of a moving thread, never queued */

#define VK_THREAD_MAGIC		0x54434200	/* "\0BCT" */
#define VK_HANDLER_MAGIC	0x494e5448	/* "HTNI" */
#define VK_HOOK_MAGIC		0x42554f4a	/* "JOUB" */

typedef struct VkThread VkThread;
typedef struct VkHeap VkHeap;

/* a thread (0x80 bytes, from a page pool) */
struct VkThread {
	VkThread *next;			/* in its run queue */
	VkThread *previous;
	VkThread *physical;		/* this, physically */
	uint8_t core;
	uint8_t state;
	uint8_t priority;
	uint8_t attributes;
	uint32_t savedDispatchDisable;
	void *entry;
	uint32_t savedSp;
	uint32_t waitResult;		/* the flags or value it was woken with */
	uint32_t flags;			/* event flags set */
	uint32_t waitFlags;		/* waited for */
	VkThread *parent;
	uint16_t joining;		/* children it waits for */
	uint16_t children;
	VkHeap *heap;			/* its stack's */
	uint32_t interrupts;		/* the interrupts it registered handlers for */
	uint32_t reserved38;
	uint32_t userWord;
	void *stack;
	uint32_t stackSize;
	uint32_t reserved48[4];
	uint32_t local[3];		/* the thread's own words (vkThreadLocal) */
	VkThread *exitNotify;		/* told of its end */
	uint32_t exitNotifyFlags;
	uint32_t exitStatus[2];
	uint32_t magic;
	uint32_t reserved78[2];
};

/* a core's own state (0x40 bytes) */
typedef struct VkCore {
	VkThread *current;
	VkThread *idle;
	uint8_t reserved8;
	uint8_t reschedulePending;	/* when dispatching was disabled */
	uint16_t reservedA;
	uint32_t inKernel;		/* interrupt events wait in the queue below */
	struct VkInterruptHandler *eventsHead;
	struct VkInterruptHandler *eventsTail;
	uint32_t dispatchDisable;
	uint32_t type;			/* from the configuration */
	uint32_t index;
	uint32_t reserved24[7];
} VkCore;

/* an interrupt handler (0x40 bytes) */
typedef struct VkInterruptHandler {
	struct VkInterruptHandler *next;	/* in the core's event queue */
	uint8_t core;
	uint8_t interrupt;
	uint8_t level;
	uint8_t edge;
	int8_t enabled;
	uint8_t reserved9[3];
	uint32_t (*handler)(uint32_t argument, uint32_t argument2);
	uint32_t argument;
	uint32_t argument2;
	VkThread *thread;		/* told of the handler's events */
	uint32_t events;		/* not yet passed on */
	/* the handler while vkInterruptsHook stands in for it */
	uint32_t (*hookedHandler)(uint32_t argument, uint32_t argument2);
	uint32_t hookedArgument;
	uint32_t hookedArgument2;
	uint32_t magic;
	uint32_t reserved30[4];
} VkInterruptHandler;

/* a core's interrupt table (0x80 bytes) */
typedef struct VkInterruptTable {
	uint32_t registered;		/* a bit per interrupt */
	VkInterruptHandler *handlers[VNZ_INTERRUPTS];	/* -1: being registered */
	uint32_t reserved64[7];
} VkInterruptTable;

/* a run queue */
typedef struct VkQueue {
	VkThread *head;
	VkThread *tail;
} VkQueue;

/* the image's configuration (at +0x300) */
typedef struct VkMemoryRegion {
	uint32_t id;
	uint32_t base;
	uint32_t size;
} VkMemoryRegion;

typedef struct VkMemoryMap {
	uint32_t count;
	VkMemoryRegion regions[];	/* sorted by id */
} VkMemoryMap;

typedef struct VkCoreConfiguration {
	uint32_t type;			/* VK_CORE_VTHREADS: the core runs a VThread worker */
	uint32_t interrupts;		/* the interrupts a crash records the handlers of */
} VkCoreConfiguration;

#define VK_CORE_VTHREADS	0x4

typedef struct VkCoreTable {
	uint32_t count;
	VkCoreConfiguration cores[VNZ_CORES];
} VkCoreTable;

typedef struct VkModule VkModule;

typedef struct VkModuleTable {
	uint32_t count;
	const VkModule *modules[];
} VkModuleTable;

typedef struct VkStartThread {
	uint32_t coreId;
	uint32_t priority;
	const VkModule *module;		/* its entry */
	uint32_t stackSize;
	uint32_t argument;
	uint32_t argument2;
} VkStartThread;

typedef struct VkStartTable {
	uint32_t count;
	VkStartThread threads[];
} VkStartTable;

typedef struct VkConfiguration {
	uint32_t reserved0;
	uint32_t cores;			/* a bit per core the kernel starts */
	uint32_t reserved8;
	uint32_t maxExited;		/* exited threads before the kernel stops; -1: no limit */
	const VkMemoryMap *memory;
	const VkCoreTable *coreTable;
	const VkModuleTable *modules;
	const VkStartTable *startThreads;
	uint32_t reserved20;
	uint32_t election;		/* its first byte: taken by the primary core (kernel/start.c) */
	uint32_t bootDone;		/* set by the primary when the shared state is set up */
} VkConfiguration;

/*
 * The header of a part of the image: the kernel ("VK0", at +0x200), the
 * application ("VK1") and the drivers ("VK2").
 */
struct VkModule {
	uint32_t magic;
	uint32_t self;			/* physically */
	uint32_t size;
	void *entry;
	uint32_t attributes;
	uint32_t version;
	void (*resume)(uint32_t primary, uint32_t resumed);
	void (*suspend)(void);
};

#define VK_MODULE_KERNEL	0x00304b56	/* "VK0" */
#define VK_MODULE_APPLICATION	0x00314b56
#define VK_MODULE_DRIVER	0x00324b56
#define VK_MODULE_ATTRIBUTES	0x0400000f
#define VK_MODULE_VERSION	0x8930

#define VK_CONFIGURATION	((const VkConfiguration *)VNZ_PHYSICAL_OF(0x800300))

/* memory regions (VkMemoryRegion.id) */
#define VK_MEMORY_HEAP		0x1000		/* the kernel heap */
#define VK_MEMORY_WORK		0x3000
#define VK_MEMORY_POOL0		0x1000000	/* the kernel's page pools */
#define VK_MEMORY_POOL1		0x1000001
#define VK_MEMORY_EXCEPTION	0x2001000	/* the exception hooks' frames (drivers/exception.c) */
#define VK_MEMORY_CRASH		0xfffffffe	/* where a crash is recorded */
#define VK_MEMORY_LOCAL_RAM	0xffffffff

/* a thread's state, for the debugger (vkThreadInformation, 0x40 bytes) */
typedef struct VkThreadInformation {
	uint8_t state;			/* with the attributes */
	int8_t priority;
	uint16_t reserved2;
	uint32_t savedSp;
	void *stack;
	uint32_t stackSize;
	uint32_t reserved10;
	uint32_t reserved14;
	uint32_t flags;
	uint32_t dispatchDisable;
	VkThread *parent;
	int16_t joining;
	int16_t children;
	uint32_t waitFlags;
	uint32_t reserved2c[5];
} VkThreadInformation;

/* a thread moving to another core (kernel/service.c, 0x1c bytes) */
typedef struct VkMigration {
	struct VkMigration *next;
	VkThread *thread;
	uint32_t from;			/* core indexes */
	uint32_t to;
	VkThread *notify;		/* sent notifyFlags when it has moved */
	uint32_t notifyFlags;
	volatile uint8_t *lock;		/* a lock page: the hooked handlers' */
} VkMigration;

typedef struct VkMigrationQueue {
	VkMigration *head;
	VkMigration *tail;
} VkMigrationQueue;


/* lists of nodes whose first word links them */
typedef struct VkListNode {
	struct VkListNode *next;
} VkListNode;

typedef struct VkList {
	VkListNode *head;
	VkListNode *tail;
} VkList;

/*
 * VThreads (kernel/vthread.c).
 */
typedef struct VkJob VkJob;
typedef struct VkVThreadContext VkVThreadContext;
typedef struct VkVThreadSystem VkVThreadSystem;
typedef void (*VkJobFunction)(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4);
typedef void (*VkBroadcastFunction)(uint32_t argument, uint32_t argument2);

/* the signals a job has had and the number it waits for */
typedef struct VkJobCounter {
	uint32_t done;
	uint32_t wanted;
} VkJobCounter;

/* a job (0x40 bytes) */
struct VkJob {
	VkJob *next;			/* in a ready list */
	uint32_t index;
	VkVThreadContext *context;
	uint32_t reservedC;
	uint32_t state;			/* VK_JOB_* */
	VkJobCounter *counter;
	VkJobFunction function;
	uint32_t reserved1C;
	uint32_t arguments[4];
	VkThread *notify;		/* sent notifyFlags when it has run */
	uint32_t notifyFlags;
	uint32_t reserved38[2];
};

typedef struct VkJobList {
	VkList list;
	uint32_t count;
} VkJobList;

/* a function run once on each core (0x44 bytes); next[core] links it in that core's list */
typedef struct VkBroadcast {
	VkListNode next[VNZ_CORES];
	uint32_t completed;
	uint32_t total;
	VkBroadcastFunction function;
	uint32_t argument;
	uint32_t argument2;
	VkBroadcastFunction done;	/* when every core has run it */
	uint32_t doneArgument;
	uint32_t doneArgument2;
	VkHeap *heap;
} VkBroadcast;

/* where a resource's jobs are queued (0xb4 bytes) */
typedef struct VkVThreadSpace {
	volatile uint8_t *lock;
	VkList ready;
	VkList *queue[VNZ_CORES];	/* where each core's worker takes jobs from */
	VkJob **slot[VNZ_CORES];	/* the job a core's worker is given */
	VkJob *current[VNZ_CORES];	/* the job a core runs */
	VkList broadcasts[VNZ_CORES];
	VkJob *jobs;			/* VNZ_CORES, public */
	VkJobCounter *counters;
} VkVThreadSpace;

/* a processing resource ("VTPR", 0x20 bytes) */
typedef struct VkVThreadResource {
	struct VkVThreadResource *next;	/* in the system's ring */
	volatile uint8_t *lock;
	VkVThreadSystem *system;
	VkVThreadSpace *space;
	uint32_t stackSize;
	uint32_t users;			/* workers serving it */
	VkThread *owner;		/* deleting it */
	uint32_t magic;
} VkVThreadResource;

/* a context ("VTSP"): its 0x40-byte header, then the counters and jobs */
struct VkVThreadContext {
	volatile uint8_t *lock;
	uint32_t count;
	VkJobCounter *counters;		/* public */
	VkJob *jobs;			/* public */
	VkJob *jobsPhysical;
	VkVThreadResource *resource;
	VkVThreadSpace *space;
	VkJobList *ready;		/* jobs readied, not yet given to the space */
	uint8_t deleting;
	int8_t users;
	uint8_t deferredFree;
	uint8_t reserved23;
	VkHeap *heap;
	uint32_t magic;
	uint32_t reserved2c[5];
};

/* a core's worker stack: where a resource's jobs run (0x40 bytes) */
typedef struct VkWorkerStack {
	void *base;
	uint32_t size;
	VkJob *slot;			/* the job given to the worker */
	uint32_t reservedC[13];
} VkWorkerStack;

/* a thread told when the enabled cores have settled ("CCEO", 0x14 bytes) */
typedef struct VkCoreEvent {
	struct VkCoreEvent *next;
	struct VkCoreEvent *previous;
	VkThread *thread;
	uint32_t flags;
	uint32_t magic;
} VkCoreEvent;

typedef struct VkCoreEventList {
	VkCoreEvent *head;
	VkCoreEvent *tail;
} VkCoreEventList;

struct VkVThreadSystem {
	volatile uint8_t *lock;
	uint32_t workerCount;
	VkThread *workers[VNZ_CORES];
	uint32_t waitingCount;
	VkThread *waiting[VNZ_CORES];	/* workers with nothing to do */
	VkVThreadResource *head;	/* the ring of resources */
	VkVThreadResource *tail;
	void (*workerStart)(VkVThreadSystem *system);
	void (*workerEnd)(VkVThreadSystem *system);
	VkJob *(*fetch)(struct VkVThreadResource *resource, struct VkVThreadSpace *space,
		VkJob **slot);
	VkWorkerStack *stacks[VNZ_CORES];
	uint32_t runningCores;
	uint32_t previousCores;
	uint32_t enabledCores;
	uint32_t settled;		/* the running cores are the enabled ones */
	VkList idleQueue;		/* the queue of a core with no worker: never filled */
	VkCoreEventList coreEvents;
	uint32_t jobsGiven;
	uint32_t jobsTaken;
	uint32_t broadcastsQueued[VNZ_CORES];
	uint32_t broadcastsRun[VNZ_CORES];
};

/*
 * Tracing (kernel/trace.c): a record per event, by event class.
 */
typedef struct VkTraceRecord {
	uint32_t timestamp;
	uint32_t coreId;
	uint32_t event;
	uint32_t argument;
	uint32_t argument2;
	uint32_t argument3;
} VkTraceRecord;

#define VK_TRACE_SWITCH		0x01
#define VK_TRACE_CREATE		0x02
#define VK_TRACE_DELETE		0x03
#define VK_TRACE_JOB_START	0x11
#define VK_TRACE_JOB_END	0x12
#define VK_TRACE_JOB_SET	0x13
#define VK_TRACE_JOB_SIGNAL	0x14
#define VK_TRACE_START		0x81
#define VK_TRACE_STOP		0x82

/* event classes (g_vkTraceMask) */
#define VK_TRACE_THREADS	0x0001
#define VK_TRACE_THREAD_ENDS	0x0100
#define VK_TRACE_JOB_RUNS	0x0200
#define VK_TRACE_JOBS		0x0400
#define VK_TRACE_SIGNALS	0x0800

/*
 * Memory (kernel/heap.c): pools and heaps.
 */
typedef struct VkPool {
	struct VkFreeBlock *head;
	uint32_t reserved;
} VkPool;

typedef int32_t (*VkHeapCreate)(void *context, uint32_t base, uint32_t size);
typedef void (*VkHeapDelete)(void *context, uint32_t base, uint32_t size);
typedef void *(*VkHeapAllocate)(void *context, uint32_t size, uint32_t alignment, int32_t window);
typedef void (*VkHeapFree)(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window);

/* a heap (0x40 bytes), in the heap tree by address */
struct VkHeap {
	VkHeap *left;
	VkHeap *right;
	uint32_t base;
	uint32_t size;
	VkHeapCreate create;
	VkHeapDelete delete;
	VkHeapAllocate allocate;
	VkHeapFree free;
	void *context;			/* its own (0x100 bytes) */
	uint32_t reserved24[7];
};

/* the context of a heap of the kernel's kind (vkHeapCreatePool) */
typedef struct VkPoolHeap {
	volatile uint8_t *lock;
	VkPool pool;
} VkPoolHeap;

/* the window an allocation is returned in */
#define VK_WINDOW_RAW		(-6)	/* physical, untouched */
#define VK_WINDOW_UNCACHED	(-2)
#define VK_WINDOW_PHYSICAL	(-1)	/* physical, purged from the data cache when freed */
#define VK_WINDOW_PUBLIC	0

/* L2 operations (kernel/cache.c; what each does is inferred from where it is used) */
#define VK_L2_SYNC		0
#define VK_L2_INVALIDATE	1
#define VK_L2_WRITEBACK		2
#define VK_L2_PURGE		3
#define VK_L2_FLUSH		4
#define VK_L2_LINE_LIMIT	0x6400	/* line by line up to this size */

/*
 * Lock pages, message queues, object arenas (kernel/pages.c).
 */
typedef struct VkLockPages {
	struct VkLockBlock *head;
	struct VkLockBlock *tail;
} VkLockPages;

/* a message is the function the receiver calls, with its own argument and the sender's */
typedef void VkMessage(uint32_t argument, uint32_t messageArgument);

typedef struct VkMessageEntry {
	VkMessage *message;
	uint32_t argument;
} VkMessageEntry;

/* a message queue (0x20 bytes) */
typedef struct VkMessageQueue {
	volatile uint8_t *lock;
	uint32_t read;
	uint32_t write;
	uint32_t mask;			/* entries - 1 */
	VkThread *thread;		/* the receiver */
	uint32_t flags;			/* set in it for each message */
	VkMessageEntry *entries;
	uint32_t magic;
} VkMessageQueue;

#define VK_MESSAGE_QUEUE_MAGIC	0x43510000	/* "\0\0QC" */

/* an arena block's header (0x40 bytes) */
typedef struct VkArenaBlock {
	struct VkArenaBlock *next;	/* in the arena's ring */
	struct VkArenaBlock *previous;
	uint32_t used;			/* bytes given out, the header's included */
	uint32_t freed;
	volatile uint8_t *lock;
	uint32_t reserved14[11];
} VkArenaBlock;

typedef struct VkArena {
	VkArenaBlock *current;
	uint32_t blockSize;
	uint32_t minimum;		/* blocks */
	uint32_t maximum;
	uint32_t count;
} VkArena;

/*
 * Power (kernel/power.c): threads told, functions called, when the cores
 * up reach the target.
 */
typedef struct VkPowerHook {
	struct VkPowerHook *next;
	struct VkPowerHook *previous;
	VkThread *thread;
	uint32_t flags;
	void (*function)(uint32_t cores, uint32_t argument);
	uint32_t argument;
	uint32_t registered;
	uint32_t magic;
} VkPowerHook;

typedef struct VkPowerList {
	VkPowerHook *head;
	VkPowerHook *tail;
} VkPowerList;

/* the crash hook (kernel/crash.c) */
typedef void (*VkCrashHook)(uint32_t coreId, uint32_t exception, uint32_t argument,
	uint32_t argument2);

/* the kernel's state */
extern VkCore g_vkCores[VNZ_CORES] VK_LOCAL;
extern VkThread g_vkIdleThreads[VNZ_CORES] VK_SHARED;
extern VkInterruptTable g_vkInterruptTables[VNZ_CORES] VK_SHARED;
extern VkQueue g_vkRunQueues[VNZ_CORES][VK_PRIORITIES] VK_SHARED;
extern uint32_t g_vkThreadCounts[VNZ_CORES] VK_SHARED;
extern VkThread *g_vkNextThreads[VNZ_CORES] VK_SHARED;
extern VkThread *g_vkServiceThreads[VNZ_CORES] VK_SHARED;

extern VkVThreadSystem g_vkVThreads VK_SHARED;
extern VkLockPages g_vkLockPages VK_SHARED;
extern VkHeap g_vkSystemHeap VK_SHARED;
extern uint32_t g_vkExitedThreads VK_SHARED;
extern uint32_t g_vkExitedLimit VK_SHARED;
extern VkMigrationQueue g_vkServiceQueues[VNZ_CORES][2] VK_SHARED;
extern uint8_t g_vkBootStacks[VNZ_CORES][0x200] VK_SHARED;
extern uint32_t g_vkBootStarted[VNZ_CORES] VK_SHARED;
extern uint32_t g_vkBootReady[VNZ_CORES] VK_SHARED;

/* api.c: memory for the current thread */
void *vkAllocatePublic(uint32_t size);
void vkFreePublic(void *memory, uint32_t size);

/* core.c */
uint32_t vkCurrentCoreId(void);
uint32_t vkCoreType(uint32_t coreId);
VkCore *vkCurrentCore(uint32_t *index);
void vkTakeSemaphore(volatile uint32_t *semaphore);
int32_t vkTrySemaphore(volatile uint32_t *semaphore);
void vkGiveSemaphore(volatile uint32_t *semaphore);
uint32_t vkTestAndSet(volatile uint8_t *lock);
void vkSpinLock(volatile uint8_t *lock);
uint32_t vkSpinTry(volatile uint8_t *lock);
void vkSpinUnlock(volatile uint8_t *lock);
void vkDisableDispatch(void);
void vkEnableDispatch(void);

/* thread.c */
void vkLockThread(VkThread *thread);
void vkUnlockThread(VkThread *thread);
void vkReschedule(VkCore *core);
void vkRingCore(VkCore *core, uint32_t current, uint32_t target);
int32_t vkThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize, VkHeap *heap);
int32_t vkThreadCreateHere(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize);
int32_t vkThreadStart(VkThread *thread, uint32_t argument, uint32_t argument2);
void vkThreadExit(void);
void vkThreadExitWithStatus(uint32_t status, uint32_t status2);
int32_t vkThreadDelete(VkThread *thread);
void vkThreadSetPriority(VkThread *thread, uint32_t priority);
void vkThreadYield(void);
uint32_t vkThreadSleep(void);
int32_t vkThreadWakeup(VkThread *thread, uint32_t value);
uint32_t vkThreadWaitChildren(void);
void vkThreadDetach(VkThread *thread);
void vkThreadSetFlags(VkThread *thread, uint32_t flags);
uint32_t vkThreadWaitFlags(uint32_t flags);
uint32_t vkThreadClearFlags(uint32_t flags);
VkThread *vkThreadCurrent(void);
uint32_t vkThreadPriority(VkThread *thread);
uint32_t vkThreadIsDormant(VkThread *thread);
uint32_t vkThreadHasExited(VkThread *thread);
int32_t vkThreadNotifyExit(VkThread *thread, uint32_t flags);
int32_t vkThreadCancelExitNotify(VkThread *thread);
int32_t vkThreadExitStatus(VkThread *thread, uint32_t *status, uint32_t *status2);
uint32_t vkExitedThreads(void);
void vkThreadInformation(VkThread *thread, VkThreadInformation *information);
uint32_t vkThreadJoining(void);
void vkThreadSetUserWord(uint32_t word);
uint32_t *vkThreadLocal(uint32_t index);
uint32_t vkThreadAttributes(VkThread *thread);
void vkThreadResume(VkThread *thread);
uint32_t vkThreadSetInterrupts(uint32_t interrupts);
uint32_t vkThreadClearInterrupts(uint32_t interrupts);
uint32_t vkWakeFromInterrupt(uint32_t coreIndex, VkThread *physical, VkThread *thread, uint32_t flags);
void vkThreadsInitialize(uint32_t primary);
void vkIdleInitialize(uint32_t primary, uint32_t start, uint32_t coreIndex);

/* core.c */
void vkCoreInitialize(uint32_t primary);

/* queue.c */
void vkQueueAppend(VkQueue *queue, VkThread *thread);
void vkQueuePrepend(VkQueue *queue, VkThread *thread);
void vkQueueRemove(VkQueue *queue, VkThread *thread);

/* thread.c */
void vkThreadRelease(VkThread *thread);
void vkThreadFinish(VkThread *thread, VkCore *core);
void vkThreadHoldForResume(VkThread *thread);
uint32_t vkMakeReady(VkThread *thread, uint32_t result);
void vkThreadReleaseFromResume(VkThread *thread);
uint32_t vkThreadIsAlive(VkThread *thread);
VkThread *vkThreadAllocate(void);
void vkThreadFree(VkThread *thread);
void vkStartThreads(uint32_t coreId, const VkConfiguration *configuration);
void vkIdleEntry(uint32_t coreId, const VkConfiguration *configuration, uint32_t primary,
	uint32_t firstStart);

/* interrupt.c */
void vkInterruptSetEdge(uint32_t interrupt, uint32_t edge);
void vkInterruptSetLevel(uint32_t interrupt, uint32_t level);
void vkInterruptSetEnable(uint32_t interrupt, uint32_t enable);
void vkInterruptClear(uint32_t interrupt);
VkInterruptHandler *vkInterruptRegister(uint32_t interrupt, uint32_t level, uint32_t edge,
	uint32_t (*handler)(uint32_t argument, uint32_t argument2),
	uint32_t argument, uint32_t argument2);
void vkInterruptRelease(VkInterruptHandler *handler);
int32_t vkInterruptEnable(VkInterruptHandler *handler);
int32_t vkInterruptDisable(VkInterruptHandler *handler);
void vkInterruptsInitialize(uint32_t primary, uint32_t firstStart);
void vkEnableDoorbell(void);
void vkDoorbellInitialize(void);
void vkDoorbell2Initialize(void);
void vkInterruptDispatch(uint32_t interrupt);
void vkDoorbellDispatch(void);
void vkInterruptEvents(VkCore *core);
void vkDeliverEvents(uint32_t index, VkCore *core);
void vkProcessEvents(VkCore *core);
uint32_t vkInterruptsReserve(uint32_t index, uint32_t interrupts);
void vkInterruptsHook(VkMigration *migration);
uint32_t vkInterruptHookHandler(uint32_t argument, uint32_t argument2);
void vkInterruptsMove(VkMigration *migration);

/* service.c */
int32_t vkThreadMigrate(VkThread *thread, uint32_t coreId, VkThread *notify, uint32_t notifyFlags);
void vkServiceStart(void);

/* trace.c */
void vkTraceInitialize(uint32_t primary);
int32_t vkTraceWrite(uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3);
void vkTrace(uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t class);
uint32_t vkTraceMask(void);
int32_t vkTraceUser(uint32_t class, uint32_t event, uint32_t argument, uint32_t argument2,
	uint32_t argument3);
int32_t vkTraceSetup(void *buffer, uint32_t size, uint32_t mode);
int32_t vkTraceSetShare(uint32_t coreId, uint32_t size);
int32_t vkTraceStart(uint32_t mask);
int32_t vkTraceStop(void);
void vkTraceRelease(void);
void *vkTraceBuffer(uint32_t *size);

/* crash.c */
void vkCrashInitialize(uint32_t primary);
int32_t vkCrashSetHook(VkCrashHook hook, uint32_t argument, uint32_t argument2);
int32_t vkCrashClearHook(void);
int32_t vkCrashCopyRegister(uint32_t id, const void *source, uint32_t size);

/* vthread.c */
VkVThreadContext *vkVThreadContextCreate(uint32_t count, VkVThreadResource *resource);
void vkVThreadContextDelete(VkVThreadContext *context);
VkVThreadContext *vkVThreadContextLock(VkVThreadContext *context);
int32_t vkVThreadContextUnlock(VkVThreadContext *context);
uint32_t vkVThreadJobSet(VkVThreadContext *context, uint32_t index, uint32_t wanted,
	VkJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4);
uint32_t vkVThreadJobSetNotify(VkVThreadContext *context, uint32_t index, uint32_t wanted,
	VkJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4, VkThread *notify, uint32_t notifyFlags);
int32_t vkVThreadJobSignal(VkVThreadContext *context, uint32_t index);
uint32_t vkVThreadJobClear(VkVThreadContext *context, uint32_t index);
uint32_t vkVThreadJobState(VkVThreadContext *context, uint32_t index);
uint32_t vkVThreadJobStateUnlocked(VkVThreadContext *context, uint32_t index);
int32_t vkVThreadBroadcastTo(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VkBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2, uint32_t mode);
int32_t vkVThreadBroadcast(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2);
int32_t vkVThreadBroadcastDone(VkVThreadResource *resource, VkBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VkBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2);
VkVThreadResource *vkVThreadResourceCreate(uint32_t stackSize);
void vkVThreadResourceDelete(VkVThreadResource *resource);
void vkVThreadServe(VkVThreadSystem *system, VkVThreadResource *resource);
void vkVThreadInitialize(void);
void vkVThreadStartWorker(void);
int32_t vkVThreadSetCores(uint32_t expected, uint32_t cores);
uint32_t vkVThreadCores(void);
VkCoreEvent *vkVThreadCoreEventCreate(VkThread *thread, uint32_t flags);
void vkVThreadCoreEventRegister(VkCoreEvent *event);
void vkVThreadCoreEventUnregister(VkCoreEvent *event);
void vkVThreadCoreEventDelete(VkCoreEvent *event);

/* address.c */
uint32_t vkPhysicalToPublic(uint32_t address);
uint32_t vkPhysicalToUncached(uint32_t address);
uint32_t vkPublicToPhysical(uint32_t address);
uint32_t vkPublicToUncached(uint32_t address);
uint32_t vkUncachedToPhysical(uint32_t address);
uint32_t vkUncachedToPublic(uint32_t address);
uint32_t vkToPhysical(uint32_t address);
uint32_t vkToPublic(uint32_t address);
uint32_t vkToUncached(uint32_t address);

/* heap.c */
int32_t vkPoolInitialize(VkPool *pool, uint32_t start, uint32_t end);
uint32_t vkPoolTake(VkPool *pool, uint32_t size, uint32_t mode);
void vkPoolGive(VkPool *pool, uint32_t address, uint32_t size, uint32_t mode);
uint32_t vkKernelPoolTake(uint32_t pool, uint32_t size, uint32_t mode);
void vkKernelPoolGive(uint32_t pool, uint32_t address, uint32_t size, uint32_t mode);
VkHeap *vkHeapFind(uint32_t address, uint32_t size);
VkHeap *vkHeapCreate(uint32_t base, uint32_t size, VkHeapCreate create, VkHeapDelete delete,
	VkHeapAllocate allocate, VkHeapFree free);
void vkHeapDelete(VkHeap *heap);
VkHeap *vkHeapCreatePool(uint32_t base, uint32_t size);
void *vkHeapAllocateStack(VkHeap *heap, uint32_t size);
void *vkHeapAllocatePublic(VkHeap *heap, uint32_t size);
void *vkHeapAllocateUncached(VkHeap *heap, uint32_t size);
void *vkHeapAllocate(VkHeap *heap, uint32_t size, uint32_t alignment);
void vkHeapFreeStack(VkHeap *heap, void *memory, uint32_t size);
void vkHeapFreePublic(VkHeap *heap, void *memory, uint32_t size);
void vkHeapFreeUncached(VkHeap *heap, void *memory, uint32_t size);
void vkHeapFree(VkHeap *heap, void *memory, uint32_t size, uint32_t alignment);
VkHeap *vkSystemHeap(void);
VkHeap *vkCurrentHeap(void);
VkHeap *vkThreadHeap(VkThread *thread);
void vkHeapInitialize(uint32_t primary);
uint32_t vkMemoryRegion(uint32_t id, uint32_t *size);

/* pages.c */
void vkLockPagesInitialize(uint32_t primary);
volatile uint8_t *vkLockPageAllocate(void);
void vkLockPageFree(volatile uint8_t *lock);
uint32_t vkLockPageTry(volatile uint8_t *lock);
void vkLockPageUnlock(volatile uint8_t *lock);
void vkLockPageLock(volatile uint8_t *lock);
VkMessageQueue *vkMessageQueueCreate(VkThread *thread, uint32_t flags, uint32_t count);
void vkMessageQueueDelete(VkMessageQueue *queue);
uint32_t vkMessageSend(VkMessageQueue *queue, VkMessage *message, uint32_t argument);
uint32_t vkMessageReceive(VkMessageQueue *queue, uint32_t argument);
void vkArenaDelete(VkArena *arena);
VkArena *vkArenaCreate(uint32_t blockSize, uint32_t minimum, uint32_t maximum);
VkArena *vkArenaSetMinimum(VkArena *arena, uint32_t minimum);
void vkArenaSetMaximum(VkArena *arena, uint32_t maximum);
void vkArenaTrim(VkArena *arena);
void *vkArenaAllocate(VkArena *arena, uint32_t size);
void vkArenaFree(void *object, uint32_t size);

/* cache.c */
void vkCachePurge(uint32_t address, uint32_t size);
void vkCacheFlush(uint32_t address, uint32_t size);
void vkCacheWriteback(uint32_t address, uint32_t size);
void vkCachePrefetch(uint32_t address, uint32_t size);
void vkCacheInvalidate(uint32_t address, uint32_t size);
uint32_t vkCacheSize(void);
void vkCacheInvalidateAll(void);
void vkCacheWritebackAll(void);
void vkCacheInvalidateTags(uint32_t address, uint32_t size, uint32_t keepDirty);
void vkCachePurgeAllCores(uint32_t address, uint32_t size);
void vkL2PurgeAllCores(uint32_t address, uint32_t size);
void vkL2InvalidateTags(uint32_t address, uint32_t size);
void vkCacheWorkersInitialize(void);
void vkCacheStartWorkers(uint32_t coreId);
void vkL2Operation(uint32_t address, uint32_t size, uint32_t operation);
void vkL2LineOperation(uint32_t address, uint32_t size, uint32_t operation);
void vkL2OperationAll(uint32_t operation);
void vkL2SetMode(uint32_t mode);
void vkL2Disable(void);
void vkL2Enable(void);
void vkLocalCodeStartWorker(uint32_t coreId);
void vkL2Initialize(void);
uint32_t vkDramAddress(uint32_t address);
void vkCacheBootConfigure(void);
void vkL2TagWalkMark(void);
void vkL2WalkTags(uint32_t address, uint32_t size, uint32_t tags, uint32_t tagMask,
	uint32_t indexMask);
void vkLocalL2SetMode(uint32_t mode);
void vkLocalPark(volatile uint8_t *flag);
void vkLocalL2Disable(void);

/* power.c */
void vkPowerInitialize(uint32_t firstBoot, uint32_t primary);
void vkPowerStart(uint32_t primary, uint32_t firstStart);
void vkPowerSaveInterrupts(void);
void vkPowerRestoreInterrupts(void);
void vkPowerCoreDown(void);
void vkPowerWaitUp(void);
void vkPowerBootWait(uint32_t primary, uint32_t firstStart, uint32_t starting, uint32_t core);
void vkPowerCoreUp(void);
void vkPowerResume(void);
void vkPowerLocalRestore(void);
void vkPowerLocalFree(void);
uint32_t vkPowerSetCores(uint32_t expected, uint32_t cores);
uint32_t vkPowerActiveCores(void);
VkPowerHook *vkPowerNotifyCreate(VkThread *thread, uint32_t flags);
VkPowerHook *vkPowerCallbackCreate(void (*function)(uint32_t cores, uint32_t argument),
	uint32_t argument);
void vkPowerHookPrepend(VkPowerList *list, VkPowerHook *hook);
void vkPowerNotifyRegister(VkPowerHook *hook);
void vkPowerCallbackRegister(VkPowerHook *hook);
void vkPowerNotifyUnregister(VkPowerHook *hook);
void vkPowerCallbackUnregister(VkPowerHook *hook);
void vkPowerNotifyDelete(VkPowerHook *hook);
void vkPowerCallbackDelete(VkPowerHook *hook);
void vkModulesResume(void);
void vkModulesSuspend(void);

/* start.c */
void vkBootBarrier(uint32_t primary, uint32_t core, uint32_t cores, uint32_t stage, int32_t next);
void vkLocalCodeCopy(uint32_t copy);
void vkKernelStart(void);

/* vectors.S */
void vkEndThread(VkThread *thread, VkCore *core, uint32_t idleSp);
void vkExceptionTooManyExited(void);
void vkSwitchContext(VkCore *core, VkThread *current, VkThread *next);
uint32_t vkInitialFrame(uint32_t top, void *entry, uint32_t argument, uint32_t argument2);
void vkCallOnStack(VkVThreadSystem *system, VkVThreadResource *resource, void *stack);
void vkPowerSuspendCore(VkThread *thread, VkCore *core);
