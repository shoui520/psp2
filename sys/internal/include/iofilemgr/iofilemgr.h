/*
 * SceIofilemgr (os0:kd/iofilemgr.skprx): the I/O file manager's exports, each declared once with
 * its definition as the prototype, and the records they take.
 */

#pragma once

#include <scetypes.h>
#include <stdint.h>
#include <kernel/types.h>
#include <kernel/iofilemgr.h>

/*
 * st_mode's access bits Sony's SDK does not name (it names SCE_STM_RWU and SCE_STM_RU), and
 * st_attr's directory bit, which a vnode's acl_data[0] also carries: our names.
 */
#define IO_STM_WU		(00200)		/* the user may write */
#define IO_STM_RS		(00004)		/* the system may read */
#define IO_STM_WS		(00002)		/* the system may write */
#define IO_STM_RWS		(00006)
#define IO_STA_FDIR		0x0010		/* a directory */

/* devctl 0x3001 ("get device information") fills it: a device's size and free space */
typedef struct SceIoDevInfo {
	SceOff maxSize;				/* bytes */
	SceOff freeSize;			/* bytes */
	SceSize clusterSize;			/* bytes */
	void *reserved;				/* NULL */
} SceIoDevInfo;
_Static_assert(sizeof(SceIoDevInfo) == 0x18, "SceIoDevInfo size");

/* a file descriptor and its process (sceIoGetGUIDFdListForDebugger, ...PUID...) */
typedef struct SceIoFdInfo {
	SceUID fd;
	SceUID pid;
} SceIoFdInfo;
_Static_assert(sizeof(SceIoFdInfo) == 8, "SceIoFdInfo size");

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


/* the option block of an asynchronous call (name unknown) */
typedef struct IoAsyncParam {
	union {
		SceInt64 result;
		struct {
			SceUID event;
			SceUInt32 reserved0;
		};
	};
	int priority;
	SceUInt32 reserved1[2];			/* not yet traced */
	int completionStatus;			/* name unknown: zero on completion */
} IoAsyncParam;

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
	int forVshbridge;		/* name unknown: 1 from sceIoChstatForVshbridge */
} IofilemgrChstatParam;

typedef struct IofilemgrChstatByFdParam {
	SceUID fd;
	SceIoStat *stat;
	int bits;
} IofilemgrChstatByFdParam;

/* SceIofilemgrForDriver */
int SceIofilemgrForDriver_03F6A684(SceUID pid, SceInt32 depth);
int sceIoSyncByFdAsync(SceUID fd, int flag, IoAsyncParam *asyncParam);
int sceIoGetstatByFdAsync(SceUID fd, SceIoStat *stat, IoAsyncParam *asyncParam);
int sceIoCloseAsync(SceUID fd, IoAsyncParam *asyncParam);
/* the request the calling thread runs for the I/O scheduler, or NULL (name unknown) */
IoRequest *SceIofilemgrForDriver_12F8D58E(void);
/* nonzero when a thread of this I/O weight is to give way (name unknown) */
int SceIofilemgrForDriver_15C17487(SceInt32 weight);
int SceIofilemgrForDriver_16336A0D(SceUID fd, int op);	/* vitasdk: ksceIoFlock */
SceUID sceIoPwriteAsync(SceUID fd, const void *data, SceSize size, SceOff offset,
				      IoAsyncParam *asyncParam);
int sceIoUmount(int id, int flags, SceUInt32 reservedLow, int reservedHigh);
SceUID sceIoMkdirAsync(const char *dir, SceMode mode, IoAsyncParam *asyncParam);
int SceIofilemgrForDriver_2EFDFA12(SceUID fd, const char *path);
int SceIofilemgrForDriver_338DCD68(SceUID fd, int *error);	/* vitasdk: ksceIoSyncByFd */
/* called when buffers for I/O of this weight run out (name unknown) */
int SceIofilemgrForDriver_3675ECB9(SceUInt32 weight);
int SceIofilemgrForDriver_3A79FAC9(SceUID fd);
int sceIoSetThreadDefaultPriorityForSystem(SceInt32 priority);
int sceIoClearErrorEvent(SceUID uid);
int sceIoSyncByFd(SceUID fd, int flag);
/* vitasdk: ksceIoGetRemoteKPLSData */
int SceIofilemgrForDriver_44EDCE57(SceUID pid, void *dst);
SceUID sceIoOpenAsync(const char *file, int flags, SceMode mode,
				      IoAsyncParam *asyncParam);
int sceIoGetstatByFd(SceUID fd, SceIoStat *stat);
SceUID sceIoSyncAsync(const char *device, unsigned int flag,
				      IoAsyncParam *asyncParam);
SceUID sceIoLseekAsync(SceUID fd, SceOff offset, int whence,
				      IoAsyncParam *asyncParam);
int sceIoDreadAsync(SceUID fd, SceIoDirent *dir, IoAsyncParam *asyncParam);
SceUID sceIoPreadAsync(SceUID fd, void *data, SceSize size, SceOff offset,
				      IoAsyncParam *asyncParam);
SceUID sceIoReadAsync(SceUID fd, void *data, SceSize size,
				      IoAsyncParam *asyncParam);
int SceIofilemgrForDriver_6BEEDDB4(SceUID fd, int handle);
SceInt32 sceIoGetThreadDefaultPriorityForSystem(void);
SceUID sceIoDopenAsync(const char *dirname, IoAsyncParam *asyncParam);
SceUID sceIoChstatAsync(const char *file, SceIoStat *stat, unsigned int bits,
				      IoAsyncParam *asyncParam);
SceUID sceIoGetstatAsync(const char *file, SceIoStat *stat, IoAsyncParam *asyncParam);
/* vitasdk: ksceIoGetMediaType */
int SceIofilemgrForDriver_9C220246(SceUID pid, const char *path, int unused, SceUInt32 *type);
SceInt32 sceIoGetPriorityForSystem(SceUID fd);
SceUID sceIoWriteAsync(SceUID fd, const void *data, SceSize size,
				      IoAsyncParam *asyncParam);
/* vitasdk: ksceIoGetFileInfo */
int SceIofilemgrForDriver_A1EF2648(SceUID fd, SceUID pid, SceIofileInfo *info);
int SceIofilemgrForDriver_A39A9CA7(const char *dev, int flags, SceUInt32 reservedLow, int reservedHigh);
/* vitasdk: ksceIoDevctlAsync */
int SceIofilemgrForDriver_A9302946(const char *dev, unsigned int cmd, const void *indata,
				   SceSize inlen, void *outdata, SceSize outlen,
				   IoAsyncParam *asyncParam);
SceUID sceIoRenameAsync(const char *oldname, const char *newname,
				      IoAsyncParam *asyncParam);
/* vitasdk: ksceIoSetProcessDefaultPriorityForSystem */
int SceIofilemgrForDriver_ABE65071(SceInt32 priority);
int SceIofilemgrForDriver_AC0E9AAA(SceUID pid, SceInt32 fdMax);
int SceIofilemgrForDriver_B499287E(IoAsyncParam *params, int num, int *numCompleted);
/* vitasdk: ksceIoIoctlAsync */
int SceIofilemgrForDriver_B761E91B(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
				   void *outdata, SceSize outlen, IoAsyncParam *asyncParam);
int sceIoDcloseAsync(SceUID fd, IoAsyncParam *asyncParam);
/* vitasdk: ksceIoOpenForPid */
SceUID SceIofilemgrForDriver_C3D34965(SceUID pid, const char *path, int flags, SceMode mode);
int sceIoDeleteErrorEvent(SceUID uid);
/* called when they are given back (name unknown) */
int SceIofilemgrForDriver_C722DF35(SceUInt32 weight);
int sceIoSetPriorityForSystem(SceUID fd, SceInt32 priority);
/* vitasdk: ksceIoGetProcessDefaultPriorityForSystem */
SceInt32 SceIofilemgrForDriver_CE397158(void);
/* vitasdk: ksceIoSetPathMappingFunction */
int SceIofilemgrForDriver_CFAECF18(IoPathMappingFunction func);
int sceIoMount(int id, const char *path, unsigned int flags, void *data,
				   SceUInt32 reservedLow, int reservedHigh);
int sceIoChstatByFd(SceUID fd, SceIoStat *stat, int bits);
int sceIoChstatByFdAsync(SceUID fd, SceIoStat *stat, int bits, IoAsyncParam *asyncParam);
void SceIofilemgrForDriver_F4F8C59A(void *arg);
SceUID sceIoRmdirAsync(const char *dirname, IoAsyncParam *asyncParam);
SceUID sceIoRemoveAsync(const char *file, IoAsyncParam *asyncParam);
int sceIoCancel(SceUID uid);
int sceIoChstat(const char *file, SceIoStat *stat, int bits);
int sceIoChstatForVshbridge(const char *file, SceIoStat *stat, int bits);
int sceIoClose(SceUID fd);
int sceIoComplete(SceUID uid);
SceUID sceIoCreateErrorEvent(int mntId, SceUInt32 pattern, IoErrorEventInfo *userInfo);
/* mount events: bit 0 mounted, bit 1 unmounted */
SceUID sceIoCreateMountEvent(SceUInt32 mountPoint, SceUInt32 events);
int sceIoDclose(SceUID fd);
int sceIoDeleteMountEvent(SceUID event);
int sceIoDevctl(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata,
		int outlen);
SceUID sceIoDopen(const char *dirname);
int sceIoDread(SceUID fd, SceIoDirent *dir);
int sceIoDreadForVshbridge(SceUID fd, SceIoDirent *dir);
int sceIoGetGUIDFdListForDebugger(int vis_level, SceIoFdInfo *dst, SceSize max_size,
				  SceSize *res_size);
int sceIoGetPUIDFdListForDebugger(int vis_level, SceIoFdInfo *dst, SceSize max_size,
				  SceSize *res_size);
int sceIoGetstat(const char *file, SceIoStat *stat);
int sceIoGetstatForVshbridge(const char *file, SceIoStat *stat);
int sceIoIoctl(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen, void *outdata,
	       SceSize outlen);
SceOff sceIoLseek(SceUID fd, SceOff offset, int whence);
int sceIoMkdir(const char *dir, SceMode mode);
SceUID sceIoOpen(const char *path, int flags, SceMode mode);
int sceIoPread(SceUID fd, void *data, SceSize size, SceOff offset);
int sceIoPwrite(SceUID fd, const void *data, SceSize size, SceOff offset);
int sceIoRead(SceUID fd, void *data, SceSize size);
int sceIoRemove(const char *file);
int sceIoRename(const char *oldname, const char *newname);
int sceIoRmdir(const char *path);
int sceIoSetErrorEvent(int mntId, SceUInt32 pattern, const IoErrorEventInfo *error);
int sceIoSetMountEvent(int id, SceUInt32 pattern, SceInt64 result, SceUInt32 reserved);
int sceIoSync(const char *device, unsigned int flag);
int sceIoWrite(SceUID fd, const void *data, SceSize size);
SceUID vfsAllocateFileDummy(int flags, int isDir, const char *path);
int vfsChstat(IofilemgrChstatParam *param);
int vfsChstatByFd(IofilemgrChstatByFdParam *param);
int vfsIoctl(IofilemgrIoctlParam *param);
int vfsNotifyDeviceStatus(int mntId);
int vfsPread(IofilemgrPreadParam *param);
int vfsPwrite(IofilemgrPwriteParam *param);
int vfsWrite(IofilemgrWriteParam *param);

/* SceIofilemgr: the user syscalls */
int _sceIoChstatByFd(SceUID fd, const SceIoStat *stat, int bits);
int _sceIoCompleteMultiple(void *a0, int a1, int *result);
int _sceIoDread(SceUID fd, SceIoDirent *dir);
int _sceIoGetstatByFd(SceUID fd, SceIoStat *stat);

#ifdef __cplusplus
}
#endif
