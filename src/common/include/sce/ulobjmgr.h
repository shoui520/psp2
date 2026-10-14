/* User-level object records returned to the coredump. */
#pragma once

#include <psp2kern/types.h>

typedef struct UlobjFiberInfo {
	char name[32];
	SceUIntPtr address;
	SceInt32 state;
	SceUIntPtr addrContext;
	SceUInt32 sizeContext;
	SceUInt32 entry;
	SceUInt32 threadId;
	SceUInt32 fpscr;
	SceUInt32 reg[8];
	SceUInt32 sp;
	SceUInt32 lr;
	SceUInt32 reserved;
	SceUInt64 vfp[8];
} UlobjFiberInfo;

typedef struct UlobjUltInfo {
	char name[32];
	SceUIntPtr address;
	SceUInt32 runtime;		/* as the caller gives it */
	SceUIntPtr fiber;
	SceUInt32 state;
	SceUInt32 opaque0;		/* Ult.opaque[0] */
	SceUInt32 threadId;
	SceUInt32 opaque1, opaque2;
} UlobjUltInfo;

typedef struct UlobjUltRuntimeInfo {
	char name[32];
	SceUIntPtr address;
	SceUInt32 opaque;		/* UltRuntime.opaque */
	SceUInt32 numUlts;
	SceUInt32 numUsedUlts;
} UlobjUltRuntimeInfo;

typedef struct UlobjSyncObjInfoHeader {
	SceUInt32 type;
	SceUIntPtr address;
	char name[32];
} UlobjSyncObjInfoHeader;

typedef struct UlobjMutexInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 attr;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 locked;
	SceUInt32 ownerThreadType;
	SceUInt32 ownerThreadId;
	SceUInt32 numWaitThreads;
	SceUInt32 waitThreadId[];
} UlobjMutexInfo;

typedef struct UlobjCondInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 reserved0;
	SceUIntPtr mutex;
	SceUInt32 numWaitThreads;
	SceUInt32 waitThreadId[];
} UlobjCondInfo;

typedef struct UlobjSemaInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 reserved0;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 count;
	SceUInt32 numWaitThreads;
	SceUInt32 waitThreadId[];
} UlobjSemaInfo;

typedef struct UlobjRwlockInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 reserved0;
	SceUIntPtr waitingQueueResourcePool;
	SceUInt32 state;		/* 2: read locked, 1: write locked */
	SceUInt32 numReaders;
	SceUInt32 numReadWaitThreads;
	SceUInt32 numWriteWaitThreads;
	SceUInt32 waitThreadId[];	/* readers, writers, then the writer */
} UlobjRwlockInfo;

typedef struct UlobjQueueInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 reserved0;
	SceUIntPtr waitingQueueResourcePool;
	SceUIntPtr queueDataResourcePool;
	SceUInt32 dataSize;
	SceUInt32 numSendWaitThreads;
	SceUInt32 numReceiveWaitThreads;
	SceUInt32 numData;
	SceUInt32 waitThreadId[];	/* senders, receivers, then the data */
} UlobjQueueInfo;

typedef struct UlobjWaitingQueueResourcePoolInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 numThreads;
	SceInt32 numFreeThreads;	/* -1: list longer than numThreads */
	SceUInt32 numSyncObjects;
	SceInt32 numUsedSyncObjects;	/* -1: free list longer than numSyncObjects */
} UlobjWaitingQueueResourcePoolInfo;

typedef struct UlobjQueueDataResourcePoolInfo {
	UlobjSyncObjInfoHeader header;
	SceUInt32 reserved0;
	SceUInt32 numData;
	SceUInt32 numUsedData;
	SceUInt32 dataSize;
	SceUInt32 numQueues;
	SceInt32 numUsedQueues;		/* -1: free list longer than numQueues */
} UlobjQueueDataResourcePoolInfo;

_Static_assert(sizeof(UlobjQueueInfo) == 0x44, "UlobjQueueInfo");
_Static_assert(sizeof(UlobjWaitingQueueResourcePoolInfo) == 0x38, "WQRP info");
_Static_assert(sizeof(UlobjQueueDataResourcePoolInfo) == 0x40, "QDRP info");
_Static_assert(sizeof(UlobjSyncObjInfoHeader) == 0x28, "UlobjSyncObjInfoHeader");
_Static_assert(sizeof(UlobjMutexInfo) == 0x40, "UlobjMutexInfo");
_Static_assert(sizeof(UlobjCondInfo) == 0x34, "UlobjCondInfo");
_Static_assert(sizeof(UlobjSemaInfo) == 0x38, "UlobjSemaInfo");
_Static_assert(sizeof(UlobjRwlockInfo) == 0x40, "UlobjRwlockInfo");
_Static_assert(sizeof(UlobjFiberInfo) == 0xa8, "UlobjFiberInfo");
_Static_assert(sizeof(UlobjUltInfo) == 0x40, "UlobjUltInfo");
_Static_assert(sizeof(UlobjUltRuntimeInfo) == 0x30, "UlobjUltRuntimeInfo");

int sceUlobjMgrGetIdForFiber(SceUID pid, SceUInt32 *ids, SceUInt32 maxIds,
			   SceUInt32 skip, int type);
int SceUlobjMgrForDriver_3D6E2EF3(SceUID pid, SceUIntPtr runtimeId,
			      SceUInt32 *ids, SceUInt32 maxIds, SceUInt32 skip);
int SceUlobjMgrForDriver_8E200387(SceUID pid, SceUIntPtr fiberId,
			      UlobjFiberInfo *info);
int SceUlobjMgrForDriver_FFC8C811(SceUID pid, SceUIntPtr ultId,
			      SceUInt32 runtime, UlobjUltInfo *info);
int SceUlobjMgrForDriver_525A9BDB(SceUID pid, SceUIntPtr runtimeId,
			      UlobjUltRuntimeInfo *info);
int SceUlobjMgrForDriver_6D2C8928(SceUID pid, SceUIntPtr address,
			      UlobjMutexInfo *info, SceUInt32 *ids, SceUInt32 *pNumIds);
int SceUlobjMgrForDriver_362C9488(SceUID pid, SceUIntPtr address,
			      UlobjCondInfo *info, SceUInt32 *ids, SceUInt32 *pNumIds);
int SceUlobjMgrForDriver_74B9199C(SceUID pid, SceUIntPtr address,
			      UlobjSemaInfo *info, SceUInt32 *ids, SceUInt32 *pNumIds);
int SceUlobjMgrForDriver_0C29AFEC(SceUID pid, SceUIntPtr address,
			      UlobjRwlockInfo *info, SceUInt32 *pWriterThreadId,
			      SceUInt32 maxIds, SceUInt32 *ids,
			      SceUInt32 *pNumReaders, SceUInt32 *pNumWriters);
int SceUlobjMgrForDriver_5D7A7925(SceUID pid, SceUIntPtr address,
			      UlobjQueueInfo *info, SceUInt32 maxIds, SceUInt32 *ids,
			      SceUInt32 *pNumSenders, SceUInt32 *pNumReceivers,
			      SceUInt32 *pNumData);
int SceUlobjMgrForDriver_6BCB4568(SceUID pid, SceUIntPtr address,
			      UlobjWaitingQueueResourcePoolInfo *info);
int SceUlobjMgrForDriver_ADAAA961(SceUID pid, SceUIntPtr address,
			      UlobjQueueDataResourcePoolInfo *info);
