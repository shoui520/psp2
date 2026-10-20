/*
 * SceUlobjMgr private interfaces.
 *
 * libult (the user-level thread library) keeps its objects in user memory;
 * the kernel only owns, per process, an "info array" that libult fills with
 * tagged pointers to them. The coredump and debugger drivers read the objects
 * back through SceUlobjMgrForDriver.
 */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <sysmem/spinlock.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <ulobjmgr/ulobjmgr.h>
#include <sysmem/sysroot.h>

/* newest libult SDK the kernel accepts */
#define ULOBJMGR_SDK_VERSION_MAX	(FW_SDK_VERSION & 0xffff0000)
#define ULOBJMGR_MAX_OBJECTS		0x2000000
#define ULOBJMGR_PROTOCOL_MIN		0x1800000

/* ---------------------------------------------------------------------------
 * Kernel side
 */

#define ULOBJ_PROC_CB_COUNT	16

/* one slot per process using libult */
typedef struct UlobjProcCB {
	SceUID pid;			/* -1: free */
	SceUID driverMemBlock;		/* kernel alias of the info array */
	SceUID userMemBlock;		/* user mapping of the info array */
	SceUInt32 sdkVersion;
	SceUInt32 numEntries;
	struct UlobjInfoArray *infoArray;	/* driverMemBlock base */
} UlobjProcCB;

typedef struct UlobjInstance {
	UlobjProcCB procCB[ULOBJ_PROC_CB_COUNT];
	SceKernelSpinlock lock;
} UlobjInstance;

/* info array entry: object address | ULOBJ_ENTRY_USED | kind */
#define ULOBJ_ENTRY_KIND_MASK	3
#define ULOBJ_ENTRY_USED	4
#define ULOBJ_ENTRY_TAG_MASK	7
#define ULOBJ_ENTRY_ADDRESS(e)	((e) & ~(SceUInt32)ULOBJ_ENTRY_TAG_MASK)
#define ULOBJ_ENTRY_END		0xfffffff8	/* last link of the free list */

enum {
	ULOBJ_KIND_FIBER = 0,
	ULOBJ_KIND_ULT_RUNTIME = 1,
	ULOBJ_KIND_SYNC_OBJECT = 2,
};

typedef struct UlobjInfoArray {
	SceUInt32 userData[3];		/* the user library's; the kernel clears the last */
	SceUInt32 ulobjmgrFlag;		/* the debugger's (dbgpUpdateUlobjmgrFlag*) */
	SceUInt32 entry[];		/* free entries link to the next as index * 8 */
} UlobjInfoArray;

extern UlobjInstance g_ulobjInstance;

/* instance.c */
int initializeUlobjProcCB(void);
UlobjProcCB *getAndLockUlobjInfoArray(SceUID pid, SceKernelIntrStatus *pLockState);
UlobjProcCB *getAndLockUlobjInfoArrayForKernel(SceUID pid);
int allocateAndLockUlobjInfoArray(SceUID pid, UlobjProcCB **pProcCB, SceKernelIntrStatus *pLockState);
void unlockUlobjInfoArrayImpl(UlobjProcCB *procCB, SceKernelIntrStatus lockState);
void unlockUlobjInfoArrayForKernelImpl(UlobjProcCB *procCB);
void freeAndUnlockUlobjInfoArrayImpl(UlobjProcCB *procCB, SceKernelIntrStatus lockState);
void freeAndUnlockUlobjInfoArrayForKernelImpl(UlobjProcCB *procCB);
UlobjProcCB *findUlobjInfoArray(SceUID pid);

/* ulobjmgr.c */
int registerUlobjProcessEvents(void);

/* info_array.c */
int allocateInfoArray(UlobjProcCB *procCB, SceUInt32 numEntries, SceUInt32 sdkVersion,
		      void **pUserBase);
void freeInfoArray(UlobjProcCB *procCB);
void initializeInfoArray(UlobjProcCB *procCB, SceUInt32 numEntries, SceUInt32 sdkVersion);

/* ---------------------------------------------------------------------------
 * libult objects in user memory (read with sceKernelCopyFromUserProc)
 */

/* a pointer field of a user object is valid unless it is NULL or -1 */
static inline int isValidUltPointer(SceUIntPtr p)
{
	return p - 1 < 0xfffffffe;
}

/* doubly linked list node; the first node of every list is its head */
typedef struct UltLink {
	SceUInt32 flags;		/* waiter kind, tested against a mask */
	SceUIntPtr next;		/* low 3 bits are tags */
	SceUIntPtr data;
} UltLink;

/* a waiting thread */
typedef struct UltWaiter {
	SceUInt32 thread;		/* bit 0 set: tid << 1, else address of the tid */
	SceUInt32 reserved;
	SceUIntPtr object;		/* the object waited on */
} UltWaiter;

enum UltObjectType {
	ULT_OBJECT_MUTEX = 1,
	ULT_OBJECT_COND = 2,
	ULT_OBJECT_SEMA = 3,
	ULT_OBJECT_RWLOCK = 4,
	ULT_OBJECT_QUEUE = 5,
	ULT_OBJECT_WAITING_QUEUE_RESOURCE_POOL = 6,
	ULT_OBJECT_QUEUE_DATA_RESOURCE_POOL = 7,
};

typedef struct UltObjectHeader {
	char name[32];
	SceUInt16 size;
	SceUInt8 type;
	SceUInt8 reserved;
} UltObjectHeader;

typedef struct UltWaitingQueueResourcePool {
	UltObjectHeader header;
	SceUInt32 numThreads;
	SceUInt32 numSyncObjects;
	SceUInt32 reserved0[2];
	SceUIntPtr threadList;
	SceUInt8 reserved1[0x4c - 0x38];
	SceUIntPtr syncObjectList;
} UltWaitingQueueResourcePool;

#define ULT_MAX_NUM_THREADS		0xaab2a
#define ULT_MAX_NUM_SYNC_OBJECTS	0x100000
#define ULT_MAX_NUM_DATA		0x1555555

typedef struct UltQueueDataResourcePool {
	UltObjectHeader header;
	SceUInt32 numData;
	SceUInt32 numQueues;
	SceUInt32 reserved0[2];
	SceUIntPtr dataList;
	SceUInt8 reserved1[0x4c - 0x38];
	SceUIntPtr queueList;
	SceUInt8 reserved2[0x84 - 0x50];
	SceUIntPtr waitList;
	SceUInt8 reserved3[0xa4 - 0x88];
	SceUInt32 dataSize;
	SceUIntPtr waitingQueueResourcePool;
} UltQueueDataResourcePool;

typedef struct UltMutex {
	UltObjectHeader header;
	SceUInt32 attr;
	SceUInt32 owner;		/* bit 0 set: tid << 1, else address of the tid */
	SceUInt32 ownerThreadType;
	SceUInt32 lockCount;
	SceUIntPtr waitList;
	SceUInt8 reserved1[0x60 - 0x38];
	SceUIntPtr waitingQueueResourcePool;
} UltMutex;

typedef struct UltCond {
	UltObjectHeader header;
	SceUInt32 reserved0[2];
	SceUIntPtr waitList;
	SceUInt8 reserved1[0x40 - 0x30];
	SceUIntPtr mutex;
} UltCond;

typedef struct UltSema {
	UltObjectHeader header;
	SceUInt32 reserved0;
	SceUInt32 count;
	SceUIntPtr waitList;
	SceUInt8 reserved1[0x68 - 0x30];
	SceUIntPtr waitingQueueResourcePool;
} UltSema;

typedef struct UltRwlock {
	UltObjectHeader header;
	SceUInt32 writer;		/* bit 0 set: tid << 1, else address of the tid */
	SceUInt8 writeLocked;
	SceUInt8 reserved0;
	SceUInt16 numReaders;
	SceUIntPtr waitList;
	SceUInt8 reserved1[0x68 - 0x30];
	SceUIntPtr waitingQueueResourcePool;
} UltRwlock;

#define ULT_RWLOCK_WAIT_READ	1
#define ULT_RWLOCK_WAIT_WRITE	2

typedef struct UltQueue {
	UltObjectHeader header;
	SceUInt32 reserved0[2];
	SceUIntPtr waitList;
	SceUInt8 reserved1[0x4c - 0x30];
	SceUIntPtr dataList;
	SceUInt8 reserved2[0x88 - 0x50];
	SceUIntPtr queueDataResourcePool;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 dataSize;
} UltQueue;

_Static_assert(sizeof(UltObjectHeader) == 0x24, "UltObjectHeader");
_Static_assert(sizeof(UltWaitingQueueResourcePool) == 0x50, "UltWaitingQueueResourcePool");
_Static_assert(sizeof(UltQueueDataResourcePool) == 0xac, "UltQueueDataResourcePool");
_Static_assert(sizeof(UltMutex) == 0x64, "UltMutex");
_Static_assert(sizeof(UltCond) == 0x44, "UltCond");
_Static_assert(sizeof(UltSema) == 0x6c, "UltSema");
_Static_assert(sizeof(UltRwlock) == 0x6c, "UltRwlock");
_Static_assert(sizeof(UltQueue) == 0x94, "UltQueue");

/* ---------------------------------------------------------------------------
 * Information handed to the drivers
 */

/* SceUlobjMgrSyncObjInfo.type */
#define ULOBJ_INFO_SEMA				0x12
#define ULOBJ_INFO_MUTEX			0x32
#define ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL	0x42
#define ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL	0x43
#define ULOBJ_INFO_QUEUE			0x52
#define ULOBJ_INFO_RWLOCK			0x92
#define ULOBJ_INFO_COND				0xa2

/* ult_sync_obj.c */
int getUltWaitThreadIds(SceUID pid, SceUIntPtr waitList, SceUInt32 mask, SceUIntPtr object,
			SceUInt32 maxThreads, SceUInt32 maxIds, SceUInt32 *ids);
int copyUltQueueData(SceUID pid, SceUIntPtr dataList, SceUInt32 maxData, SceSize dataSize,
		     SceSize stride, void *data);
int getUltQueueDataIds(SceUID pid, SceUIntPtr dataList, SceUInt32 maxData, SceUInt32 maxIds,
		       SceUInt32 *ids);
int getUltQueueDataResourcePoolInfo(SceUID pid, SceUIntPtr address,
				    UlobjQueueDataResourcePoolInfo *info);
int getUltWaitingQueueResourcePoolInfo(SceUID pid, SceUIntPtr address,
				       UlobjWaitingQueueResourcePoolInfo *info);

/* driver_api_for_ult_sync_obj.c */
int sceUlobjMgrGetUltSyncObjId(SceUID pid, SceUInt32 *ids, SceUInt32 maxIds, SceUInt32 skip,
			       int type, UlobjProcCB *procCB);
int getUltQueueInfoSize(SceUID pid, SceUIntPtr address);
int getUltQueueInfo(SceUID pid, SceUIntPtr address, UlobjQueueInfo *info);
