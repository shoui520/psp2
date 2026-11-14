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

#define VNZ_KERNEL_SECTION(name)	__attribute__((section(name)))
/* reached physically */
#define VNZ_KERNEL_LOCAL		VNZ_KERNEL_SECTION(".kernel.local")
/* reached through L2 */
#define VNZ_KERNEL_SHARED		VNZ_KERNEL_SECTION(".kernel.shared")

#define VNZ_KERNEL_PRIORITIES		9	/* 1 to 9 */
#define VNZ_KERNEL_IDLE_PRIORITY	0

/* the hardware semaphores the kernel uses */
#define VNZ_KERNEL_SEMAPHORE_THREADS	0	/* + core: a core's run queue (8) */
#define VNZ_KERNEL_SEMAPHORE_MOVES	8	/* threads moving between cores */
#define VNZ_KERNEL_SEMAPHORE_POOLS	9	/* + pool: the kernel's page pools (2) */
#define VNZ_KERNEL_SEMAPHORE_PAGES	11	/* the lock pages (vnzKernelLockPageAllocate) */
#define VNZ_KERNEL_SEMAPHORE_POWER	12	/* suspend and resume */
#define VNZ_KERNEL_SEMAPHORE_INTERRUPTS	13	/* the interrupt tables */
#define VNZ_KERNEL_SEMAPHORE_EXITS	14	/* the count of exited threads */
#define VNZ_KERNEL_SEMAPHORE_HEAPS	15	/* the heap list */

#define vnzKernelSemaphore(n)		((volatile uint32_t *)(VNZ_SEMAPHORES + 4 * (n)))

/* thread states (VnzKernelThread.state, low four bits) and what a waiting thread waits for */
#define VNZ_KERNEL_STATE_MASK		0x0f
#define VNZ_KERNEL_STATE_READY		1
#define VNZ_KERNEL_STATE_WAITING	2
#define VNZ_KERNEL_STATE_RUNNING	3
#define VNZ_KERNEL_STATE_DORMANT	4	/* created, or ended with nothing to report */
#define VNZ_KERNEL_STATE_EXITED		5	/* ended with an exit status to collect */
#define VNZ_KERNEL_WAIT_MASK		0x30
#define VNZ_KERNEL_WAIT_SLEEP		0x00	/* until vnzKernelThreadWakeup */
#define VNZ_KERNEL_WAIT_CHILDREN	0x10	/* until the children have ended */
#define VNZ_KERNEL_WAIT_FLAGS		0x20	/* until an event flag it waits for is set */

/* VnzKernelThread.attributes */
#define VNZ_KERNEL_THREAD_MOVING	0x80	/* moving to another core (kernel/service.c) */

/* VnzKernelThread.core of a moving thread, never queued */
#define VNZ_KERNEL_NO_CORE		8

#define VNZ_KERNEL_THREAD_MAGIC		0x54434200	/* "\0BCT" */
#define VNZ_KERNEL_HANDLER_MAGIC	0x494e5448	/* "HTNI" */
#define VNZ_KERNEL_HOOK_MAGIC		0x42554f4a	/* "JOUB" */

typedef struct VnzKernelThread VnzKernelThread;
typedef struct VnzKernelHeap VnzKernelHeap;

/* a thread (0x80 bytes, from a page pool) */
struct VnzKernelThread {
	VnzKernelThread *next;			/* in its run queue */
	VnzKernelThread *previous;
	VnzKernelThread *physical;		/* this, physically */
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
	VnzKernelThread *parent;
	uint16_t joining;		/* children it waits for */
	uint16_t children;
	VnzKernelHeap *heap;			/* its stack's */
	uint32_t interrupts;		/* the interrupts it registered handlers for */
	uint32_t reserved38;
	uint32_t userWord;
	void *stack;
	uint32_t stackSize;
	uint32_t reserved48[4];
	uint32_t local[3];		/* the thread's own words (vnzKernelThreadLocal0..2) */
	VnzKernelThread *exitNotify;		/* told of its end */
	uint32_t exitNotifyFlags;
	uint32_t exitStatus[2];
	uint32_t magic;
	uint32_t reserved78[2];
};

/* a core's own state (0x40 bytes) */
typedef struct VnzKernelCore {
	VnzKernelThread *current;
	VnzKernelThread *idle;
	uint8_t reserved8;
	uint8_t reschedulePending;	/* when dispatching was disabled */
	uint16_t reservedA;
	uint32_t inKernel;		/* interrupt events wait in the queue below */
	struct VnzKernelInterruptHandler *eventsHead;
	struct VnzKernelInterruptHandler *eventsTail;
	uint32_t dispatchDisable;
	uint32_t type;			/* from the configuration */
	uint32_t index;
	uint32_t reserved24[7];
} VnzKernelCore;

/* an interrupt handler (0x40 bytes) */
typedef struct VnzKernelInterruptHandler {
	struct VnzKernelInterruptHandler *next;	/* in the core's event queue */
	uint8_t core;
	uint8_t interrupt;
	uint8_t level;
	uint8_t edge;
	int8_t enabled;
	uint8_t reserved9[3];
	uint32_t (*handler)(uint32_t argument, uint32_t argument2);
	uint32_t argument;
	uint32_t argument2;
	VnzKernelThread *thread;		/* told of the handler's events */
	uint32_t events;		/* not yet passed on */
	/* the handler while vnzKernelInterruptsHook stands in for it */
	uint32_t (*hookedHandler)(uint32_t argument, uint32_t argument2);
	uint32_t hookedArgument;
	uint32_t hookedArgument2;
	uint32_t magic;
	uint32_t reserved30[4];
} VnzKernelInterruptHandler;

/* a core's interrupt table (0x80 bytes) */
typedef struct VnzKernelInterruptTable {
	uint32_t registered;		/* a bit per interrupt */
	VnzKernelInterruptHandler *handlers[VNZ_INTERRUPTS];	/* -1: being registered */
	uint32_t reserved64[7];
} VnzKernelInterruptTable;

/* a run queue */
typedef struct VnzKernelQueue {
	VnzKernelThread *head;
	VnzKernelThread *tail;
} VnzKernelQueue;

/* the image's configuration (at +0x300) */
typedef struct VnzKernelMemoryRegion {
	uint32_t id;
	uint32_t base;
	uint32_t size;
} VnzKernelMemoryRegion;

typedef struct VnzKernelMemoryMap {
	uint32_t count;
	VnzKernelMemoryRegion regions[];	/* sorted by id */
} VnzKernelMemoryMap;

typedef struct VnzKernelCoreConfiguration {
	uint32_t type;		/* VNZ_KERNEL_CORE_VTHREADS: the core runs a VThread worker */
	uint32_t interrupts;		/* the interrupts a crash records the handlers of */
} VnzKernelCoreConfiguration;

#define VNZ_KERNEL_CORE_VTHREADS	0x4

typedef struct VnzKernelCoreTable {
	uint32_t count;
	VnzKernelCoreConfiguration cores[VNZ_CORES];
} VnzKernelCoreTable;

typedef struct VnzKernelModule VnzKernelModule;

typedef struct VnzKernelModuleTable {
	uint32_t count;
	const VnzKernelModule *modules[];
} VnzKernelModuleTable;

typedef struct VnzKernelStartThread {
	uint32_t coreId;
	uint32_t priority;
	const VnzKernelModule *module;		/* its entry */
	uint32_t stackSize;
	uint32_t argument;
	uint32_t argument2;
} VnzKernelStartThread;

typedef struct VnzKernelStartTable {
	uint32_t count;
	VnzKernelStartThread threads[];
} VnzKernelStartTable;

typedef struct VnzKernelConfiguration {
	uint32_t reserved0;
	uint32_t cores;			/* a bit per core the kernel starts */
	uint32_t reserved8;
	uint32_t maxExited;		/* exited threads before the kernel stops; -1: no limit */
	const VnzKernelMemoryMap *memory;
	const VnzKernelCoreTable *coreTable;
	const VnzKernelModuleTable *modules;
	const VnzKernelStartTable *startThreads;
	uint32_t reserved20;
	uint32_t election;		/* its first byte: taken by the primary core (kernel/start.c) */
	uint32_t bootDone;		/* set by the primary when the shared state is set up */
} VnzKernelConfiguration;

/*
 * The header of a part of the image: the kernel ("VK0", at +0x200), the
 * application ("VK1") and the drivers ("VK2").
 */
struct VnzKernelModule {
	uint32_t magic;
	uint32_t self;			/* physically */
	uint32_t size;
	void *entry;
	uint32_t attributes;
	uint32_t version;
	void (*resume)(uint32_t primary, uint32_t resumed);
	void (*suspend)(void);
};

#define VNZ_KERNEL_MODULE_KERNEL	0x00304b56	/* "VK0" */
#define VNZ_KERNEL_MODULE_APPLICATION	0x00314b56
#define VNZ_KERNEL_MODULE_DRIVER	0x00324b56
#define VNZ_KERNEL_MODULE_ATTRIBUTES	0x0400000f
#define VNZ_KERNEL_MODULE_VERSION	0x8930

#define VNZ_KERNEL_CONFIGURATION	((const VnzKernelConfiguration *)VNZ_PHYSICAL_OF(0x800300))

/* memory regions (VnzKernelMemoryRegion.id) */
#define VNZ_KERNEL_MEMORY_HEAP		0x1000		/* the kernel heap */
#define VNZ_KERNEL_MEMORY_WORK		0x3000
#define VNZ_KERNEL_MEMORY_POOL0		0x1000000	/* the kernel's page pools */
#define VNZ_KERNEL_MEMORY_POOL1		0x1000001
/* the exception hooks' frames (drivers/exception.c) */
#define VNZ_KERNEL_MEMORY_EXCEPTION	0x2001000
#define VNZ_KERNEL_MEMORY_CRASH		0xfffffffe	/* where a crash is recorded */
#define VNZ_KERNEL_MEMORY_LOCAL_RAM	0xffffffff

/* a thread's state, for the debugger (vnzKernelThreadInformation, 0x40 bytes) */
typedef struct VnzKernelThreadInformation {
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
	VnzKernelThread *parent;
	int16_t joining;
	int16_t children;
	uint32_t waitFlags;
	uint32_t reserved2c[5];
} VnzKernelThreadInformation;

/* a thread moving to another core (kernel/service.c, 0x1c bytes) */
typedef struct VnzKernelMigration {
	struct VnzKernelMigration *next;
	VnzKernelThread *thread;
	uint32_t from;			/* core indexes */
	uint32_t to;
	VnzKernelThread *notify;		/* sent notifyFlags when it has moved */
	uint32_t notifyFlags;
	volatile uint8_t *lock;		/* a lock page: the hooked handlers' */
} VnzKernelMigration;

typedef struct VnzKernelMigrationQueue {
	VnzKernelMigration *head;
	VnzKernelMigration *tail;
} VnzKernelMigrationQueue;


/* lists of nodes whose first word links them */
typedef struct VnzKernelListNode {
	struct VnzKernelListNode *next;
} VnzKernelListNode;

typedef struct VnzKernelList {
	VnzKernelListNode *head;
	VnzKernelListNode *tail;
} VnzKernelList;

/*
 * VThreads (kernel/vthread.c).
 */
typedef struct VnzKernelJob VnzKernelJob;
typedef struct VnzKernelVThreadContext VnzKernelVThreadContext;
typedef struct VnzKernelVThreadSystem VnzKernelVThreadSystem;
typedef void (*VnzKernelJobFunction)(uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4);
typedef void (*VnzKernelBroadcastFunction)(uint32_t argument, uint32_t argument2);

/* the signals a job has had and the number it waits for */
typedef struct VnzKernelJobCounter {
	uint32_t done;
	uint32_t wanted;
} VnzKernelJobCounter;

/* a job (0x40 bytes) */
struct VnzKernelJob {
	VnzKernelJob *next;			/* in a ready list */
	uint32_t index;
	VnzKernelVThreadContext *context;
	uint32_t reservedC;
	uint32_t state;			/* VNZ_KERNEL_JOB_* */
	VnzKernelJobCounter *counter;
	VnzKernelJobFunction function;
	uint32_t reserved1C;
	uint32_t arguments[4];
	VnzKernelThread *notify;		/* sent notifyFlags when it has run */
	uint32_t notifyFlags;
	uint32_t reserved38[2];
};

typedef struct VnzKernelJobList {
	VnzKernelList list;
	uint32_t count;
} VnzKernelJobList;

/* a function run once on each core (0x44 bytes); next[core] links it in that core's list */
typedef struct VnzKernelBroadcast {
	VnzKernelListNode next[VNZ_CORES];
	uint32_t completed;
	uint32_t total;
	VnzKernelBroadcastFunction function;
	uint32_t argument;
	uint32_t argument2;
	VnzKernelBroadcastFunction done;	/* when every core has run it */
	uint32_t doneArgument;
	uint32_t doneArgument2;
	VnzKernelHeap *heap;
} VnzKernelBroadcast;

/* where a resource's jobs are queued (0xb4 bytes) */
typedef struct VnzKernelVThreadSpace {
	volatile uint8_t *lock;
	VnzKernelList ready;
	VnzKernelList *queue[VNZ_CORES];	/* where each core's worker takes jobs from */
	VnzKernelJob **slot[VNZ_CORES];	/* the job a core's worker is given */
	VnzKernelJob *current[VNZ_CORES];	/* the job a core runs */
	VnzKernelList broadcasts[VNZ_CORES];
	VnzKernelJob *jobs;			/* VNZ_CORES, public */
	VnzKernelJobCounter *counters;
} VnzKernelVThreadSpace;

/* a processing resource ("VTPR", 0x20 bytes) */
typedef struct VnzKernelVThreadResource {
	struct VnzKernelVThreadResource *next;	/* in the system's ring */
	volatile uint8_t *lock;
	VnzKernelVThreadSystem *system;
	VnzKernelVThreadSpace *space;
	uint32_t stackSize;
	uint32_t users;			/* workers serving it */
	VnzKernelThread *owner;		/* deleting it */
	uint32_t magic;
} VnzKernelVThreadResource;

/* a context ("VTSP"): its 0x40-byte header, then the counters and jobs */
struct VnzKernelVThreadContext {
	volatile uint8_t *lock;
	uint32_t count;
	VnzKernelJobCounter *counters;		/* public */
	VnzKernelJob *jobs;			/* public */
	VnzKernelJob *jobsPhysical;
	VnzKernelVThreadResource *resource;
	VnzKernelVThreadSpace *space;
	VnzKernelJobList *ready;		/* jobs readied, not yet given to the space */
	uint8_t deleting;
	int8_t users;
	uint8_t deferredFree;
	uint8_t reserved23;
	VnzKernelHeap *heap;
	uint32_t magic;
	uint32_t reserved2c[5];
};

/* a core's worker stack: where a resource's jobs run (0x40 bytes) */
typedef struct VnzKernelWorkerStack {
	void *base;
	uint32_t size;
	VnzKernelJob *slot;			/* the job given to the worker */
	uint32_t reservedC[13];
} VnzKernelWorkerStack;

/* a thread told when the enabled cores have settled ("CCEO", 0x14 bytes) */
typedef struct VnzKernelCoreEvent {
	struct VnzKernelCoreEvent *next;
	struct VnzKernelCoreEvent *previous;
	VnzKernelThread *thread;
	uint32_t flags;
	uint32_t magic;
} VnzKernelCoreEvent;

typedef struct VnzKernelCoreEventList {
	VnzKernelCoreEvent *head;
	VnzKernelCoreEvent *tail;
} VnzKernelCoreEventList;

struct VnzKernelVThreadSystem {
	volatile uint8_t *lock;
	uint32_t workerCount;
	VnzKernelThread *workers[VNZ_CORES];
	uint32_t waitingCount;
	VnzKernelThread *waiting[VNZ_CORES];	/* workers with nothing to do */
	VnzKernelVThreadResource *head;	/* the ring of resources */
	VnzKernelVThreadResource *tail;
	void (*workerStart)(VnzKernelVThreadSystem *system);
	void (*workerEnd)(VnzKernelVThreadSystem *system);
	VnzKernelJob *(*fetch)(struct VnzKernelVThreadResource *resource,
		struct VnzKernelVThreadSpace *space,
		VnzKernelJob **slot);
	VnzKernelWorkerStack *stacks[VNZ_CORES];
	uint32_t runningCores;
	uint32_t previousCores;
	uint32_t enabledCores;
	uint32_t settled;		/* the running cores are the enabled ones */
	VnzKernelList idleQueue;	/* the queue of a core with no worker: never filled */
	VnzKernelCoreEventList coreEvents;
	uint32_t jobsGiven;
	uint32_t jobsTaken;
	uint32_t broadcastsQueued[VNZ_CORES];
	uint32_t broadcastsRun[VNZ_CORES];
};

/*
 * Tracing (kernel/trace.c): a record per event, by event class.
 */
typedef struct VnzKernelTraceRecord {
	uint32_t timestamp;
	uint32_t coreId;
	uint32_t event;
	uint32_t argument;
	uint32_t argument2;
	uint32_t argument3;
} VnzKernelTraceRecord;

#define VNZ_KERNEL_TRACE_SWITCH		0x01
#define VNZ_KERNEL_TRACE_CREATE		0x02
#define VNZ_KERNEL_TRACE_DELETE		0x03
#define VNZ_KERNEL_TRACE_JOB_START	0x11
#define VNZ_KERNEL_TRACE_JOB_END	0x12
#define VNZ_KERNEL_TRACE_JOB_SET	0x13
#define VNZ_KERNEL_TRACE_JOB_SIGNAL	0x14
#define VNZ_KERNEL_TRACE_START		0x81
#define VNZ_KERNEL_TRACE_STOP		0x82

/* event classes (g_vnzKernelTraceMask) */
#define VNZ_KERNEL_TRACE_THREADS	0x0001
#define VNZ_KERNEL_TRACE_THREAD_ENDS	0x0100
#define VNZ_KERNEL_TRACE_JOB_RUNS	0x0200
#define VNZ_KERNEL_TRACE_JOBS		0x0400
#define VNZ_KERNEL_TRACE_SIGNALS	0x0800

/*
 * Memory (kernel/heap.c): pools and heaps.
 */
typedef struct VnzKernelPool {
	struct VnzKernelFreeBlock *head;
	uint32_t reserved;
} VnzKernelPool;

typedef int32_t (*VnzKernelHeapCreate)(void *context, uint32_t base, uint32_t size);
typedef void (*VnzKernelHeapDelete)(void *context, uint32_t base, uint32_t size);
typedef void *(*VnzKernelHeapAllocate)(void *context, uint32_t size, uint32_t alignment,
	int32_t window);
typedef void (*VnzKernelHeapFree)(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window);

/* a heap (0x40 bytes), in the heap tree by address */
struct VnzKernelHeap {
	VnzKernelHeap *left;
	VnzKernelHeap *right;
	uint32_t base;
	uint32_t size;
	VnzKernelHeapCreate create;
	VnzKernelHeapDelete delete;
	VnzKernelHeapAllocate allocate;
	VnzKernelHeapFree free;
	void *context;			/* its own (0x100 bytes) */
	uint32_t reserved24[7];
};

/* the context of a heap of the kernel's kind (vnzKernelHeapCreatePool) */
typedef struct VnzKernelPoolHeap {
	volatile uint8_t *lock;
	VnzKernelPool pool;
} VnzKernelPoolHeap;

/* the window an allocation is returned in */
#define VNZ_KERNEL_WINDOW_RAW		(-6)	/* physical, untouched */
#define VNZ_KERNEL_WINDOW_UNCACHED	(-2)
/* physical, purged from the data cache when freed */
#define VNZ_KERNEL_WINDOW_PHYSICAL	(-1)
#define VNZ_KERNEL_WINDOW_PUBLIC	0

/* L2 operations (kernel/cache.c; what each does is inferred from where it is used) */
#define VNZ_KERNEL_L2_SYNC		0
#define VNZ_KERNEL_L2_INVALIDATE	1
#define VNZ_KERNEL_L2_WRITEBACK		2
#define VNZ_KERNEL_L2_PURGE		3
#define VNZ_KERNEL_L2_FLUSH		4
#define VNZ_KERNEL_L2_LINE_LIMIT	0x6400	/* line by line up to this size */

/*
 * Lock pages, message queues, object arenas (kernel/pages.c).
 */
typedef struct VnzKernelLockPages {
	struct VnzKernelLockBlock *head;
	struct VnzKernelLockBlock *tail;
} VnzKernelLockPages;

/* a message is the function the receiver calls, with its own argument and the sender's */
typedef void VnzKernelMessage(uint32_t argument, uint32_t messageArgument);

typedef struct VnzKernelMessageEntry {
	VnzKernelMessage *message;
	uint32_t argument;
} VnzKernelMessageEntry;

/* a message queue (0x20 bytes) */
typedef struct VnzKernelMessageQueue {
	volatile uint8_t *lock;
	uint32_t read;
	uint32_t write;
	uint32_t mask;			/* entries - 1 */
	VnzKernelThread *thread;		/* the receiver */
	uint32_t flags;			/* set in it for each message */
	VnzKernelMessageEntry *entries;
	uint32_t magic;
} VnzKernelMessageQueue;

#define VNZ_KERNEL_MESSAGE_QUEUE_MAGIC	0x43510000	/* "\0\0QC" */

/* an arena block's header (0x40 bytes) */
typedef struct VnzKernelArenaBlock {
	struct VnzKernelArenaBlock *next;	/* in the arena's ring */
	struct VnzKernelArenaBlock *previous;
	uint32_t used;			/* bytes given out, the header's included */
	uint32_t freed;
	volatile uint8_t *lock;
	uint32_t reserved14[11];
} VnzKernelArenaBlock;

typedef struct VnzKernelArena {
	VnzKernelArenaBlock *current;
	uint32_t blockSize;
	uint32_t minimum;		/* blocks */
	uint32_t maximum;
	uint32_t count;
} VnzKernelArena;

/*
 * Power (kernel/power.c): threads told, functions called, when the cores
 * up reach the target.
 */
typedef struct VnzKernelPowerHook {
	struct VnzKernelPowerHook *next;
	struct VnzKernelPowerHook *previous;
	VnzKernelThread *thread;
	uint32_t flags;
	void (*function)(uint32_t cores, uint32_t argument);
	uint32_t argument;
	uint32_t registered;
	uint32_t magic;
} VnzKernelPowerHook;

typedef struct VnzKernelPowerList {
	VnzKernelPowerHook *head;
	VnzKernelPowerHook *tail;
} VnzKernelPowerList;

/* the crash hook (kernel/crash.c) */
typedef void (*VnzKernelCrashHook)(uint32_t coreId, uint32_t exception, uint32_t argument,
	uint32_t argument2);

/* the kernel's state */
extern VnzKernelCore g_vnzKernelCores[VNZ_CORES] VNZ_KERNEL_LOCAL;
extern VnzKernelThread g_vnzKernelIdleThreads[VNZ_CORES] VNZ_KERNEL_SHARED;
extern VnzKernelInterruptTable g_vnzKernelInterruptTables[VNZ_CORES] VNZ_KERNEL_SHARED;
extern VnzKernelQueue g_vnzKernelRunQueues[VNZ_CORES][VNZ_KERNEL_PRIORITIES] VNZ_KERNEL_SHARED;
extern uint32_t g_vnzKernelThreadCounts[VNZ_CORES] VNZ_KERNEL_SHARED;
extern VnzKernelThread *g_vnzKernelNextThreads[VNZ_CORES] VNZ_KERNEL_SHARED;
extern VnzKernelThread *g_vnzKernelServiceThreads[VNZ_CORES] VNZ_KERNEL_SHARED;

extern VnzKernelVThreadSystem g_vnzKernelVThreads VNZ_KERNEL_SHARED;
extern VnzKernelLockPages g_vnzKernelLockPages VNZ_KERNEL_SHARED;
extern VnzKernelHeap g_vnzKernelSystemHeap VNZ_KERNEL_SHARED;
extern uint32_t g_vnzKernelExitedThreads VNZ_KERNEL_SHARED;
extern uint32_t g_vnzKernelExitedLimit VNZ_KERNEL_SHARED;
extern VnzKernelMigrationQueue g_vnzKernelServiceQueues[VNZ_CORES][2] VNZ_KERNEL_SHARED;
extern uint8_t g_vnzKernelBootStacks[VNZ_CORES][0x200] VNZ_KERNEL_SHARED;
extern uint32_t g_vnzKernelBootStarted[VNZ_CORES] VNZ_KERNEL_SHARED;
extern uint32_t g_vnzKernelBootReady[VNZ_CORES] VNZ_KERNEL_SHARED;

/* api.c: memory for the current thread */
void *vnzKernelAllocatePublic(uint32_t size);
void vnzKernelFreePublic(void *memory, uint32_t size);

/* core.c */
uint32_t vnzKernelCurrentCoreId(void);
uint32_t vnzKernelCoreType(uint32_t coreId);
VnzKernelCore *vnzKernelCurrentCore(uint32_t *index);
void vnzKernelTakeSemaphore(volatile uint32_t *semaphore);
int32_t vnzKernelTrySemaphore(volatile uint32_t *semaphore);
void vnzKernelGiveSemaphore(volatile uint32_t *semaphore);
void vnzKernelSpinLock(volatile uint8_t *lock);
uint32_t vnzKernelSpinTry(volatile uint8_t *lock);
void vnzKernelSpinUnlock(volatile uint8_t *lock);
void vnzKernelDisableDispatch(void);
void vnzKernelEnableDispatch(void);

/* thread.c */
void vnzKernelLockThread(VnzKernelThread *thread);
void vnzKernelUnlockThread(VnzKernelThread *thread);
void vnzKernelReschedule(VnzKernelCore *core);
void vnzKernelRingCore(VnzKernelCore *core, uint32_t current, uint32_t target);
int32_t vnzKernelThreadCreate(void *entry, uint32_t coreId, uint32_t priority, uint32_t stackSize,
	VnzKernelHeap *heap);
int32_t vnzKernelThreadCreateHere(void *entry, uint32_t coreId, uint32_t priority,
	uint32_t stackSize);
int32_t vnzKernelThreadStart(VnzKernelThread *thread, uint32_t argument, uint32_t argument2);
void vnzKernelThreadExit(void);
void vnzKernelThreadExitWithStatus(uint32_t status, uint32_t status2);
int32_t vnzKernelThreadDelete(VnzKernelThread *thread);
void vnzKernelThreadSetPriority(VnzKernelThread *thread, uint32_t priority);
void vnzKernelThreadYield(void);
uint32_t vnzKernelThreadSleep(void);
int32_t vnzKernelThreadWakeup(VnzKernelThread *thread, uint32_t value);
uint32_t vnzKernelThreadWaitChildren(void);
void vnzKernelThreadDetach(VnzKernelThread *thread);
void vnzKernelThreadSetFlags(VnzKernelThread *thread, uint32_t flags);
uint32_t vnzKernelThreadWaitFlags(uint32_t flags);
uint32_t vnzKernelThreadClearFlags(uint32_t flags);
VnzKernelThread *vnzKernelThreadCurrent(void);
uint32_t vnzKernelThreadPriority(VnzKernelThread *thread);
uint32_t vnzKernelThreadIsDormant(VnzKernelThread *thread);
uint32_t vnzKernelThreadHasExited(VnzKernelThread *thread);
int32_t vnzKernelThreadNotifyExit(VnzKernelThread *thread, uint32_t flags);
int32_t vnzKernelThreadCancelExitNotify(VnzKernelThread *thread);
int32_t vnzKernelThreadExitStatus(VnzKernelThread *thread, uint32_t *status, uint32_t *status2);
uint32_t vnzKernelExitedThreads(void);
void vnzKernelThreadInformation(VnzKernelThread *thread, VnzKernelThreadInformation *information);
uint32_t vnzKernelThreadJoining(void);
void vnzKernelThreadSetUserWord(uint32_t word);
uint32_t *vnzKernelThreadLocal0(void);
uint32_t *vnzKernelThreadLocal1(void);
uint32_t *vnzKernelThreadLocal2(void);
uint32_t vnzKernelThreadAttributes(VnzKernelThread *thread);
void vnzKernelThreadResume(VnzKernelThread *thread);
uint32_t vnzKernelThreadSetInterrupts(uint32_t interrupts);
uint32_t vnzKernelThreadClearInterrupts(uint32_t interrupts);
uint32_t vnzKernelWakeFromInterrupt(uint32_t coreIndex, VnzKernelThread *physical,
	VnzKernelThread *thread, uint32_t flags);
void vnzKernelThreadsInitialize(uint32_t primary);
void vnzKernelIdleInitialize(uint32_t primary, uint32_t start, uint32_t coreIndex);

/* core.c */
void vnzKernelCoreInitialize(uint32_t primary);

/* queue.c */
void vnzKernelQueueAppend(VnzKernelQueue *queue, VnzKernelThread *thread);
void vnzKernelQueuePrepend(VnzKernelQueue *queue, VnzKernelThread *thread);
void vnzKernelQueueRemove(VnzKernelQueue *queue, VnzKernelThread *thread);

/* thread.c */
void vnzKernelThreadRelease(VnzKernelThread *thread);
void vnzKernelThreadFinish(VnzKernelThread *thread, VnzKernelCore *core);
void vnzKernelThreadHoldForResume(VnzKernelThread *thread);
uint32_t vnzKernelMakeReady(VnzKernelThread *thread, uint32_t result);
void vnzKernelThreadReleaseFromResume(VnzKernelThread *thread);
uint32_t vnzKernelThreadIsAlive(VnzKernelThread *thread);
VnzKernelThread *vnzKernelThreadAllocate(void);
void vnzKernelThreadFree(VnzKernelThread *thread);
void vnzKernelStartThreads(uint32_t coreId, const VnzKernelConfiguration *configuration);
void vnzKernelIdleEntry(uint32_t coreId, const VnzKernelConfiguration *configuration,
	uint32_t primary,
	uint32_t firstStart);

/* interrupt.c */
void vnzKernelInterruptSetEdge(uint32_t interrupt, uint32_t edge);
void vnzKernelInterruptSetLevel(uint32_t interrupt, uint32_t level);
void vnzKernelInterruptSetEnable(uint32_t interrupt, uint32_t enable);
void vnzKernelInterruptClear(uint32_t interrupt);
VnzKernelInterruptHandler *vnzKernelInterruptRegister(uint32_t interrupt, uint32_t level,
	uint32_t edge,
	uint32_t (*handler)(uint32_t argument, uint32_t argument2),
	uint32_t argument, uint32_t argument2);
void vnzKernelInterruptRelease(VnzKernelInterruptHandler *handler);
int32_t vnzKernelInterruptEnable(VnzKernelInterruptHandler *handler);
int32_t vnzKernelInterruptDisable(VnzKernelInterruptHandler *handler);
void vnzKernelInterruptsInitialize(uint32_t primary, uint32_t firstStart);
void vnzKernelEnableDoorbell(void);
void vnzKernelDoorbellInitialize(void);
void vnzKernelDoorbell2Initialize(void);
void vnzKernelInterruptDispatch(uint32_t interrupt);
void vnzKernelDoorbellDispatch(void);
void vnzKernelInterruptEvents(VnzKernelCore *core);
void vnzKernelDeliverEvents(uint32_t index, VnzKernelCore *core);
void vnzKernelProcessEvents(VnzKernelCore *core);
uint32_t vnzKernelInterruptsReserve(uint32_t index, uint32_t interrupts);
void vnzKernelInterruptsHook(VnzKernelMigration *migration);
uint32_t vnzKernelInterruptHookHandler(uint32_t argument, uint32_t argument2);
void vnzKernelInterruptsMove(VnzKernelMigration *migration);

/* service.c */
int32_t vnzKernelThreadMigrate(VnzKernelThread *thread, uint32_t coreId, VnzKernelThread *notify,
	uint32_t notifyFlags);
void vnzKernelServiceStart(void);

/* trace.c */
void vnzKernelTraceInitialize(uint32_t primary);
int32_t vnzKernelTraceWrite(uint32_t event, uint32_t argument, uint32_t argument2,
	uint32_t argument3);
void vnzKernelTrace(uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t class);
uint32_t vnzKernelTraceMask(void);
int32_t vnzKernelTraceUser(uint32_t class, uint32_t event, uint32_t argument, uint32_t argument2,
	uint32_t argument3);
int32_t vnzKernelTraceSetup(void *buffer, uint32_t size, uint32_t mode);
int32_t vnzKernelTraceSetShare(uint32_t coreId, uint32_t size);
int32_t vnzKernelTraceStart(uint32_t mask);
int32_t vnzKernelTraceStop(void);
void vnzKernelTraceRelease(void);
void *vnzKernelTraceBuffer(uint32_t *size);

/* crash.c */
void vnzKernelCrashInitialize(uint32_t primary);
int32_t vnzKernelCrashSetHook(VnzKernelCrashHook hook, uint32_t argument, uint32_t argument2);
int32_t vnzKernelCrashClearHook(void);
int32_t vnzKernelCrashCopyRegister(uint32_t id, const void *source, uint32_t size);

/* vthread.c */
VnzKernelVThreadContext *vnzKernelVThreadContextCreate(uint32_t count,
	VnzKernelVThreadResource *resource);
void vnzKernelVThreadContextDelete(VnzKernelVThreadContext *context);
VnzKernelVThreadContext *vnzKernelVThreadContextLock(VnzKernelVThreadContext *context);
int32_t vnzKernelVThreadContextUnlock(VnzKernelVThreadContext *context);
uint32_t vnzKernelVThreadJobSet(VnzKernelVThreadContext *context, uint32_t index, uint32_t wanted,
	VnzKernelJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4);
uint32_t vnzKernelVThreadJobSetNotify(VnzKernelVThreadContext *context, uint32_t index,
	uint32_t wanted,
	VnzKernelJobFunction function, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t argument4, VnzKernelThread *notify, uint32_t notifyFlags);
int32_t vnzKernelVThreadJobSignal(VnzKernelVThreadContext *context, uint32_t index);
uint32_t vnzKernelVThreadJobClear(VnzKernelVThreadContext *context, uint32_t index);
uint32_t vnzKernelVThreadJobState(VnzKernelVThreadContext *context, uint32_t index);
uint32_t vnzKernelVThreadJobStateUnlocked(VnzKernelVThreadContext *context, uint32_t index);
int32_t vnzKernelVThreadBroadcastTo(VnzKernelVThreadResource *resource,
	VnzKernelBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VnzKernelBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2, uint32_t mode);
int32_t vnzKernelVThreadBroadcast(VnzKernelVThreadResource *resource,
	VnzKernelBroadcastFunction function,
	uint32_t argument, uint32_t argument2);
int32_t vnzKernelVThreadBroadcastDone(VnzKernelVThreadResource *resource,
	VnzKernelBroadcastFunction function,
	uint32_t argument, uint32_t argument2, VnzKernelBroadcastFunction done,
	uint32_t doneArgument, uint32_t doneArgument2);
VnzKernelVThreadResource *vnzKernelVThreadResourceCreate(uint32_t stackSize);
void vnzKernelVThreadResourceDelete(VnzKernelVThreadResource *resource);
void vnzKernelVThreadServe(VnzKernelVThreadSystem *system, VnzKernelVThreadResource *resource);
void vnzKernelVThreadInitialize(void);
void vnzKernelVThreadStartWorker(void);
int32_t vnzKernelVThreadSetCores(uint32_t expected, uint32_t cores);
uint32_t vnzKernelVThreadCores(void);
VnzKernelCoreEvent *vnzKernelVThreadCoreEventCreate(VnzKernelThread *thread, uint32_t flags);
void vnzKernelVThreadCoreEventRegister(VnzKernelCoreEvent *event);
void vnzKernelVThreadCoreEventUnregister(VnzKernelCoreEvent *event);
void vnzKernelVThreadCoreEventDelete(VnzKernelCoreEvent *event);

/* address.c */
uint32_t vnzKernelPhysicalToPublic(uint32_t address);
uint32_t vnzKernelPhysicalToUncached(uint32_t address);
uint32_t vnzKernelPublicToPhysical(uint32_t address);
uint32_t vnzKernelPublicToUncached(uint32_t address);
uint32_t vnzKernelUncachedToPhysical(uint32_t address);
uint32_t vnzKernelUncachedToPublic(uint32_t address);
uint32_t vnzKernelToPhysical(uint32_t address);
uint32_t vnzKernelToPublic(uint32_t address);
uint32_t vnzKernelToUncached(uint32_t address);

/* heap.c */
int32_t vnzKernelPoolInitialize(VnzKernelPool *pool, uint32_t start, uint32_t end);
uint32_t vnzKernelPoolTake(VnzKernelPool *pool, uint32_t size, uint32_t mode);
void vnzKernelPoolGive(VnzKernelPool *pool, uint32_t address, uint32_t size, uint32_t mode);
uint32_t vnzKernelKernelPoolTake(uint32_t pool, uint32_t size, uint32_t mode);
void vnzKernelKernelPoolGive(uint32_t pool, uint32_t address, uint32_t size, uint32_t mode);
VnzKernelHeap *vnzKernelHeapFind(uint32_t address, uint32_t size);
VnzKernelHeap *vnzKernelHeapCreate(uint32_t base, uint32_t size, VnzKernelHeapCreate create,
	VnzKernelHeapDelete delete,
	VnzKernelHeapAllocate allocate, VnzKernelHeapFree free);
void vnzKernelHeapDelete(VnzKernelHeap *heap);
void *vnzKernelHeapContext(VnzKernelHeap *heap);
VnzKernelHeap *vnzKernelHeapCreatePool(uint32_t base, uint32_t size);
void vnzKernelHeapDeletePool(VnzKernelHeap *heap);
int32_t vnzKernelPoolHeapCreate(void *context, uint32_t base, uint32_t size);
void vnzKernelPoolHeapDelete(void *context, uint32_t base, uint32_t size);
void *vnzKernelPoolHeapAllocate(void *context, uint32_t size, uint32_t alignment, int32_t window);
void vnzKernelPoolHeapFree(void *context, void *memory, uint32_t size, uint32_t alignment,
	int32_t window);
void *vnzKernelHeapAllocateStack(VnzKernelHeap *heap, uint32_t size);
void *vnzKernelHeapAllocatePublic(VnzKernelHeap *heap, uint32_t size);
void *vnzKernelHeapAllocateUncached(VnzKernelHeap *heap, uint32_t size);
void *vnzKernelHeapAllocate(VnzKernelHeap *heap, uint32_t size, uint32_t alignment);
void vnzKernelHeapFreeStack(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzKernelHeapFreePublic(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzKernelHeapFreeUncached(VnzKernelHeap *heap, void *memory, uint32_t size);
void vnzKernelHeapFree(VnzKernelHeap *heap, void *memory, uint32_t size, uint32_t alignment);
VnzKernelHeap *vnzKernelSystemHeap(void);
VnzKernelHeap *vnzKernelCurrentHeap(void);
VnzKernelHeap *vnzKernelThreadHeap(VnzKernelThread *thread);
void vnzKernelHeapInitialize(uint32_t primary);
uint32_t vnzKernelMemoryRegion(uint32_t id, uint32_t *size);

/* pages.c */
void vnzKernelLockPagesInitialize(uint32_t primary);
volatile uint8_t *vnzKernelLockPageAllocate(void);
void vnzKernelLockPageFree(volatile uint8_t *lock);
VnzKernelMessageQueue *vnzKernelMessageQueueCreate(VnzKernelThread *thread, uint32_t flags,
	uint32_t count);
void vnzKernelMessageQueueDelete(VnzKernelMessageQueue *queue);
uint32_t vnzKernelMessageSend(VnzKernelMessageQueue *queue, VnzKernelMessage *message,
	uint32_t argument);
uint32_t vnzKernelMessageReceive(VnzKernelMessageQueue *queue, uint32_t argument);
void vnzKernelArenaDelete(VnzKernelArena *arena);
VnzKernelArena *vnzKernelArenaCreate(uint32_t blockSize, uint32_t minimum, uint32_t maximum);
VnzKernelArena *vnzKernelArenaSetMinimum(VnzKernelArena *arena, uint32_t minimum);
void vnzKernelArenaSetMaximum(VnzKernelArena *arena, uint32_t maximum);
void vnzKernelArenaTrim(VnzKernelArena *arena);
void *vnzKernelArenaAllocate(VnzKernelArena *arena, uint32_t size);
void vnzKernelArenaFree(void *object, uint32_t size);

/* cache.c */
void vnzKernelCachePurge(uint32_t address, uint32_t size);
void vnzKernelCacheFlush(uint32_t address, uint32_t size);
void vnzKernelCacheWriteback(uint32_t address, uint32_t size);
void vnzKernelCachePrefetch(uint32_t address, uint32_t size);
void vnzKernelCacheInvalidate(uint32_t address, uint32_t size);
uint32_t vnzKernelCacheSize(void);
void vnzKernelCacheInvalidateAll(void);
void vnzKernelCacheWritebackAll(void);
void vnzKernelCacheInvalidateTags(uint32_t address, uint32_t size, uint32_t keepDirty);
void vnzKernelCachePurgeAllCores(uint32_t address, uint32_t size);
void vnzKernelL2PurgeAllCores(uint32_t address, uint32_t size);
void vnzKernelL2InvalidateTags(uint32_t address, uint32_t size);
void vnzKernelCacheWorkersInitialize(void);
void vnzKernelCacheStartWorkers(uint32_t coreId);
void vnzKernelL2Operation(uint32_t address, uint32_t size, uint32_t operation);
void vnzKernelL2LineOperation(uint32_t address, uint32_t size, uint32_t operation);
void vnzKernelL2OperationAll(uint32_t operation);
void vnzKernelL2SetMode(uint32_t mode);
void vnzKernelL2Disable(void);
void vnzKernelL2Enable(void);
void vnzKernelLocalCodeStartWorker(uint32_t coreId);
void vnzKernelL2Initialize(void);
uint32_t vnzKernelDramAddress(uint32_t address);
void vnzKernelCacheBootConfigure(void);
void vnzKernelL2TagWalkMark(void);
void vnzKernelL2WalkTags(uint32_t address, uint32_t size, uint32_t tags, uint32_t tagMask,
	uint32_t indexMask);
void vnzKernelLocalL2SetMode(uint32_t mode);
void vnzKernelLocalPark(volatile uint8_t *flag);
void vnzKernelLocalL2Disable(void);

/* power.c */
void vnzKernelPowerInitialize(uint32_t firstBoot, uint32_t primary);
void vnzKernelPowerStart(uint32_t primary, uint32_t firstStart);
void vnzKernelPowerSaveInterrupts(void);
void vnzKernelPowerRestoreInterrupts(void);
void vnzKernelPowerCoreDown(void);
void vnzKernelPowerWaitUp(void);
void vnzKernelPowerBootWait(uint32_t primary, uint32_t firstStart, uint32_t starting,
	uint32_t core);
void vnzKernelPowerCoreUp(void);
void vnzKernelPowerResume(void);
void vnzKernelPowerLocalRestore(void);
void vnzKernelPowerLocalFree(void);
uint32_t vnzKernelPowerSetCores(uint32_t expected, uint32_t cores);
uint32_t vnzKernelPowerActiveCores(void);
VnzKernelPowerHook *vnzKernelPowerNotifyCreate(VnzKernelThread *thread, uint32_t flags);
VnzKernelPowerHook *vnzKernelPowerCallbackCreate(void (*function)(uint32_t cores,
	uint32_t argument),
	uint32_t argument);
void vnzKernelPowerNotifyRegister(VnzKernelPowerHook *hook);
void vnzKernelPowerCallbackRegister(VnzKernelPowerHook *hook);
void vnzKernelPowerNotifyUnregister(VnzKernelPowerHook *hook);
void vnzKernelPowerCallbackUnregister(VnzKernelPowerHook *hook);
void vnzKernelPowerNotifyDelete(VnzKernelPowerHook *hook);
void vnzKernelPowerCallbackDelete(VnzKernelPowerHook *hook);
void vnzKernelModulesResume(void);
void vnzKernelModulesSuspend(void);

/* start.c */
void vnzKernelBootBarrier(uint32_t primary, uint32_t core, uint32_t cores, uint32_t stage,
	int32_t next);
void vnzKernelLocalCodeCopy(uint32_t copy);
void vnzKernelKernelStart(void);

/* vectors.S */
void vnzKernelEndThread(VnzKernelThread *thread, VnzKernelCore *core, uint32_t idleSp);
void vnzKernelExceptionTooManyExited(void);
void vnzKernelSwitchContext(VnzKernelCore *core, VnzKernelThread *current, VnzKernelThread *next);
uint32_t vnzKernelInitialFrame(uint32_t top, void *entry, uint32_t argument, uint32_t argument2);
void vnzKernelCallOnStack(VnzKernelVThreadSystem *system, VnzKernelVThreadResource *resource,
	void *stack);
void vnzKernelPowerSuspendCore(VnzKernelThread *thread, VnzKernelCore *core);
