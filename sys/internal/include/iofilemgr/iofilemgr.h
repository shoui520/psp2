/*
 * SceIofilemgr (os0:kd/iofilemgr.skprx) driver interface, as used so far (all in vitasdk).
 */

#pragma once

#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/stat.h>
#include <psp2kern/io/dirent.h>

/* Maps a process path into the caller-provided resolved-path buffer. */
typedef int (*IoPathMappingFunction)(const char *path, SceUID pid,
				     char *mapped, SceSize size);

/* Buffer-cache settings passed to ioctl 0x1001. */
typedef struct VfsBuffCtlIoctlArg {
	SceSize dataCacheSize;
	SceSize lineSize;
	SceUInt32 wayNum;
	SceSize ioCacheSize;
	int mode;
	SceUInt32 mask;
	SceUInt32 reserved[2];
} VfsBuffCtlIoctlArg;

_Static_assert(sizeof(VfsBuffCtlIoctlArg) == 0x20, "VfsBuffCtlIoctlArg size");
_Static_assert(__builtin_offsetof(VfsBuffCtlIoctlArg, ioCacheSize) == 0xc &&
	       __builtin_offsetof(VfsBuffCtlIoctlArg, reserved) == 0x18,
	       "VfsBuffCtlIoctlArg layout");

typedef struct SceIofileInfo {
	char resolvedOpenPath[0x400];
	char openPath[0x400];
	SceUID pid;
	int flags;
	SceOff position;
	int isDir;
	int mode;
	SceOff size;
	SceUInt32 ioSchedData;
	SceUInt32 reserved;
} SceIofileInfo;

_Static_assert(sizeof(SceIofileInfo) == 0x828, "SceIofileInfo size");
_Static_assert(__builtin_offsetof(SceIofileInfo, position) == 0x808 &&
	       __builtin_offsetof(SceIofileInfo, size) == 0x818 &&
	       __builtin_offsetof(SceIofileInfo, ioSchedData) == 0x820,
	       "SceIofileInfo layout");

/* A filesystem reports one mount error through SceIofilemgr. */
typedef struct IoErrorEventInfo {
	int mntId;
	int error;
	SceUInt32 reserved0[2];			/* zero */
	int detail;				/* not yet traced */
	SceUInt32 reserved1[2];			/* zero */
} IoErrorEventInfo;
_Static_assert(sizeof(IoErrorEventInfo) == 0x1c, "IoErrorEventInfo");

#ifdef __cplusplus
extern "C" {
#endif
int ksceIoGetFileInfo(SceUID fd, SceUID pid, SceIofileInfo *info);
int ksceIoSetPathMappingFunction(IoPathMappingFunction func);
int ksceIoIoctl(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
	       void *outdata, SceSize outlen);
int sceIoSetErrorEvent(int mntId, SceUInt32 pattern, const IoErrorEventInfo *error);
#ifdef __cplusplus
}
#endif

/* mount events: bit 0 mounted, bit 1 unmounted */
SceUID ksceIoCreateMountEvent(SceUInt32 mountPoint, SceUInt32 events);
int ksceIoDeleteMountEvent(SceUID event);

/* SceIofilemgr sched_io_chunk.c: a piece of an I/O request the scheduler runs on its own (name unknown) */
typedef struct IoChunk {
	struct IoChunk *next;
	SceUIntPtr data;				/* request buffer plus offset */
	SceUInt32 offset;			/* offset within the request */
	SceSize size;
	struct IoRequest *request;		/* the request the chunk belongs to */
} IoChunk;
_Static_assert(sizeof(IoChunk) == 0x14, "IoChunk");

/* a request's chunks, carved from a buffer (sceIoChunkCreateIoChunkPool; name unknown) */
typedef struct IoChunkPool {
	IoChunk *freeList;
	SceUInt32 freeNum;
	IoChunk *base;
	SceSize size;				/* bytes */
} IoChunkPool;

/* IoRequest.type: the call a request makes (names unknown) */
#define IO_REQUEST_OPEN			0x01
#define IO_REQUEST_CLOSE		0x02
#define IO_REQUEST_READ			0x03
#define IO_REQUEST_WRITE		0x04
#define IO_REQUEST_LSEEK		0x05
#define IO_REQUEST_LSEEK32		0x06
#define IO_REQUEST_IOCTL		0x07
#define IO_REQUEST_REMOVE		0x08
#define IO_REQUEST_DOPEN		0x09
#define IO_REQUEST_DCLOSE		0x0a
#define IO_REQUEST_DREAD		0x0b
#define IO_REQUEST_MKDIR		0x0c
#define IO_REQUEST_RMDIR		0x0d
#define IO_REQUEST_RENAME		0x0e
#define IO_REQUEST_CHSTAT		0x0f
#define IO_REQUEST_CHSTAT_BY_FD		0x10
#define IO_REQUEST_GETSTAT		0x11
#define IO_REQUEST_GETSTAT_BY_FD	0x12
#define IO_REQUEST_DEVCTL		0x13
#define IO_REQUEST_SYNC			0x14
#define IO_REQUEST_SYNC_BY_FD		0x15
#define IO_REQUEST_PREAD		0x16
#define IO_REQUEST_PWRITE		0x17
#define IO_REQUEST_GETSTAT_FOR_VSH	0x18
#define IO_REQUEST_CHSTAT_FOR_VSH	0x19
#define IO_REQUEST_DREAD_FOR_VSH	0x1a
#define IO_REQUEST_DOPEN_ASYNC		0x1b
#define IO_REQUEST_DREAD_ASYNC		0x1c
#define IO_REQUEST_DCLOSE_ASYNC		0x1d

/* the chunks a read or write request is split into */
#define IO_REQUEST_CHUNK_NUM		8

/* a 64-bit argument of a request: the arguments are only word aligned */
typedef SceOff IoRequestOff __attribute__((aligned(4)));

/* the arguments and result of each call (names unknown) */
typedef union IoRequestArgs {
	struct { const char *path; int flags; SceMode mode; SceUID result; } open;
	struct { SceUID fd; int result; } fd;			/* close, dclose */
	struct { const char *path; int result; } path;		/* remove, rmdir, dopen */
	struct {
		SceUID fd;
		void *data;
		void *mappedData;			/* a user buffer's kernel mapping */
		SceSize size;
		SceUID mapUid;				/* that mapping */
		IoRequestOff offset;			/* -1: the file position */
		int result;
	} rw;							/* read, pread, pwrite */
	struct {
		SceUID fd;
		const void *data;
		void *mappedData;			/* a user buffer's kernel mapping */
		SceSize size;
		SceUID mapUid;				/* that mapping */
		int result;
	} write;
	struct {
		SceUID fd;
		int whence;
		SceUInt32 reserved;
		IoRequestOff offset;
		IoRequestOff result;
	} lseek;
	struct { SceUID fd; int offset; int whence; int result; } lseek32;
	struct {
		const char *dev;
		unsigned int cmd;
		const void *indata;
		SceSize inlen;
		void *outdata;
		SceSize outlen;
		int result;
	} devctl;
	struct {
		SceUID fd;
		unsigned int cmd;
		const void *indata;
		SceSize inlen;
		void *outdata;
		SceSize outlen;
		int result;
	} ioctl;
	struct { const char *path; SceMode mode; int result; } mkdir;
	struct { const char *oldname; const char *newname; int result; } rename;
	struct { SceUID fd; SceIoDirent *dir; int result; } dread;
	struct { const char *path; SceIoStat *stat; int result; } getstat;
	struct { SceUID fd; SceIoStat *stat; int result; } getstatByFd;
	struct { const char *path; SceIoStat *stat; int bits; int result; } chstat;
	struct { SceUID fd; SceIoStat *stat; int bits; int result; } chstatByFd;
	struct { const char *device; unsigned int flag; int result; } sync;
	struct { SceUID fd; int flag; int result; } syncByFd;
} IoRequestArgs;

/* SceIofilemgr sched_io_request.c: an I/O call the scheduler runs (Sony's functions call it IoRequest) */
typedef struct IoRequest {
	struct IoRequest *next;			/* not yet traced */
	struct IoRequest *prev;			/* not yet traced */
	IoChunk *chunkHead;
	IoChunk *chunkTail;
	SceUInt16 chunkNum;
	SceUInt16 runningChunkNum;		/* chunks popped and not yet done */
	SceUInt16 doneChunkNum;			/* not yet traced */
	SceInt8 isWaiting;			/* the requester waits on the scheduler's cond */
	SceInt8 isWaitingIo;			/* not yet traced */
	void *ioData;				/* read / write: the next byte to transfer */
	SceSize ioDone;
	SceSize ioRemain;
	int type;				/* IO_REQUEST_* */
	SceInt32 priority;
	SceInt32 mntOpt;			/* the device's mount option: its low 16 bits weigh the I/O */
	SceInt8 isAsync;
	SceInt8 isCanceled;			/* not yet traced */
	SceInt8 isRunning;			/* not yet traced */
	SceInt8 isFinished;			/* not yet traced */
	SceUID pid;				/* sceKernelGetProcessId */
	SceUID thid;				/* 0 for an asynchronous request */
	SceUID processId;			/* SceThreadmgrForDriver_FA54D49A */
	SceUID threadAcl;			/* SceThreadmgrForDriver_332E127C (not yet traced) */
	int cpuAffinityMask;
	int threadPriority;
	IoChunkPool chunkPool;
	SceInt16 fromUser;			/* the caller runs a syscall (TPIDRURO) */
	SceInt16 chunkAlignment;
	SceUID asyncEvent;			/* the event an asynchronous request sets when done */
	IoRequestArgs args;
	SceUInt32 argsPadding;			/* not yet traced */
	void *asyncParam;			/* the caller's IoAsyncParam */
	int asyncPriority;			/* its word at 8 (not yet traced) */
	SceInt64 asyncResult;			/* what the asynchronous request returns */
	int isWrite;				/* 1 for a call that changes the device (removable devices) */
	int cacheFlags;				/* 0x8000 for a read or write on a removable device */
	SceUInt32 chunkOffset;			/* initial file offset within a chunk */
	struct IoRequest *waitNext;		/* forgetIoReq's list (not yet traced) */
	SceInt64 readPosition;			/* last successful read seek position */
	SceUInt64 time;				/* sceKernelGetSystemTimeWide at creation */
} IoRequest;
_Static_assert(__builtin_offsetof(IoRequest, args) == 0x64, "IoRequest.args");
_Static_assert(__builtin_offsetof(IoRequest, asyncParam) == 0x88, "IoRequest.asyncParam");
_Static_assert(sizeof(IoRequest) == 0xb8, "IoRequest");

/* the request the calling thread runs for the I/O scheduler, or NULL (name unknown) */
IoRequest *SceIofilemgrForDriver_12F8D58E(void);

/* nonzero when a thread of this I/O weight is to give way (name unknown) */
int SceIofilemgrForDriver_15C17487(SceInt32 weight);

/* called when buffers for I/O of this weight run out, and when they are given back (names unknown) */
int SceIofilemgrForDriver_3675ECB9(SceUInt32 weight);
int SceIofilemgrForDriver_C722DF35(SceUInt32 weight);

/* what the vfs* entry points take: one call's arguments */
typedef struct IofilemgrWriteParam {
	SceUID fd;
	const void *data;
	SceSize size;
} IofilemgrWriteParam;

typedef struct IofilemgrPreadParam {
	SceUID fd;
	void *data;
	SceSize size;
	SceUInt32 padding;
	SceOff offset;
} IofilemgrPreadParam;

typedef struct IofilemgrPwriteParam {
	SceUID fd;
	const void *data;
	SceSize size;
	SceUInt32 padding;
	SceOff offset;
} IofilemgrPwriteParam;

typedef struct IofilemgrIoctlParam {
	SceUID fd;
	unsigned int cmd;
	const void *indata;
	SceSize inlen;
	void *outdata;
	SceSize outlen;
} IofilemgrIoctlParam;

typedef struct IofilemgrChstatParam {
	const char *file;
	SceIoStat *stat;
	int bits;
	int forVshbridge;		/* name unknown: 1 from ksceIoChstatForVshbridge */
} IofilemgrChstatParam;

typedef struct IofilemgrChstatByFdParam {
	SceUID fd;
	SceIoStat *stat;
	int bits;
} IofilemgrChstatByFdParam;

int vfsWrite(IofilemgrWriteParam *param);
int vfsPread(IofilemgrPreadParam *param);
int vfsPwrite(IofilemgrPwriteParam *param);
int vfsIoctl(IofilemgrIoctlParam *param);
int vfsChstat(IofilemgrChstatParam *param);
int vfsChstatByFd(IofilemgrChstatByFdParam *param);

int ksceIoChstatForVshbridge(const char *file, SceIoStat *stat, int bits);
int ksceIoClearErrorEvent(SceUID uid);
int ksceIoDeleteErrorEvent(SceUID uid);
int ksceIoDreadForVshbridge(SceUID fd, SceIoDirent *dir);
int ksceIoFlock(SceUID fd, int op);
int ksceIoGetMediaType(SceUID pid, const char *path, int unused, SceUInt32 *type);
SceInt32 ksceIoGetProcessDefaultPriorityForSystem(void);
int ksceIoGetstatForVshbridge(const char *file, SceIoStat *stat);
SceUID ksceIoOpenForPid(SceUID pid, const char *path, int flags, SceMode mode);
int ksceIoSetProcessDefaultPriorityForSystem(SceInt32 priority);
int ksceIoSyncByFd2(SceUID fd, int flag);
int sceIoSetMountEvent(int id, SceUInt32 pattern, SceInt64 result, SceUInt32 reserved);
int vfsNotifyDeviceStatus(int mntId);
