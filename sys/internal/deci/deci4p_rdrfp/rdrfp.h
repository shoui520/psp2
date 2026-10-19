/*
 * SceDeci4pRDrfp: the "RDRFP" DECI4P protocol, through which the development
 * host reaches the target's file systems (ux0:, the memory card, the game
 * card) and the application / save data trees of the running titles.
 *
 * Requests arrive in the protocol handler (rdrfp.c), are queued, and the file
 * system thread (rdrfp_fs.c) runs them (rdrfp_fs_cmds.c) and queues the
 * replies for sending. Writes arriving in pieces are collected in write
 * buffers (rdrfp_writebuf_list.c) and flushed whole or by a timer.
 */

#pragma once

#include <scetypes.h>
#include <kernel/error.h>
#include <kernel/types.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <deci4p_dfmgr/deci4p.h>

/* a doubly linked list; an unlinked node points at itself */
typedef struct RdrfpLink {
	struct RdrfpLink *next;
	struct RdrfpLink *prev;
} RdrfpLink;

/* ===== requests ===== */

/* what a queued request carries */
typedef struct RdrfpDesc {
	SceUInt32 reserved0;
	SceUInt32 id;				/* request ID of the packet */
	int bufType;				/* RDRFP_BUF_* holding the packet */
	int bufIndex;
	void *packet;				/* RdrfpPacket */
	const void *data;			/* second part sent after the packet */
	SceSize dataSize;
	int releaseBuffer;			/* release the buffer once sent */
	void *extra;				/* heap memory freed with the descriptor */
	SceUInt32 reserved1;
} RdrfpDesc;

typedef struct RdrfpRequest {
	RdrfpLink link;
	SceUInt32 id;
	RdrfpDesc *desc;
} RdrfpRequest;

typedef struct RdrfpQueue {
	RdrfpLink head;
	SceUID mutex;
	SceUID evf;				/* 1: something queued */
	int count;
} RdrfpQueue;

/* ===== packets ===== */

typedef struct RdrfpHeader {
	SceDeci4pHeader hdr;
	SceUInt32 type;				/* RDRFP_CMD_* */
	union {
		SceUInt32 id;			/* low 16 bits: request ID */
		SceUInt16 id16;			/* written as 16 bits */
	};
} RdrfpHeader;

/* commands (the reply to each is the command + 1) */
#define RDRFP_CMD_GET_OPTIONS		0x02
#define RDRFP_CMD_OPEN			0x04
#define RDRFP_CMD_CLOSE			0x06
#define RDRFP_CMD_READ			0x08	/* RdrfpReadState attached */
#define RDRFP_CMD_WRITE			0x0a	/* write data, possibly in pieces */
#define RDRFP_CMD_FSTAT			0x0c
#define RDRFP_CMD_STAT			0x0e
#define RDRFP_CMD_MKDIR			0x10
#define RDRFP_CMD_RMDIR			0x12
#define RDRFP_CMD_OPENDIR		0x14
#define RDRFP_CMD_CLOSEDIR		0x16
#define RDRFP_CMD_READDIR		0x18
#define RDRFP_CMD_FTRUNCATE		0x1a
#define RDRFP_CMD_TRUNCATE		0x1c
#define RDRFP_CMD_RENAME		0x1e
#define RDRFP_CMD_UNLINK		0x20
#define RDRFP_CMD_GET_DEVICE_LIST	0x22
#define RDRFP_CMD_FCHSTAT		0x24
#define RDRFP_CMD_CHSTAT		0x26
#define RDRFP_CMD_MOUNT_APP_DATA	0x28
#define RDRFP_CMD_UMOUNT_APP_DATA	0x2a

/* a read too large for one go continues later from where it stopped */
typedef struct RdrfpReadState {
	union {
		SceUInt32 flags;
		struct {
			SceUInt8 continued;
			SceUInt8 reserved[3];
		};
	};
	SceSize remaining;
	SceOff offset __attribute__((packed, aligned(4)));
	SceUInt16 seq;
} RdrfpReadState;

_Static_assert(sizeof(RdrfpReadState) == 20, "RdrfpReadState size");

/* a write piece: header, then (from 0x1000 of its buffer) the data */
typedef struct RdrfpWriteHeader {
	SceUInt32 fd;
	union {
		SceUInt32 flags;		/* bits 1-2: piece kind, bit 1: more */
		SceUInt8 kind;			/* written as 8 bits */
	};
	SceOff offset __attribute__((packed, aligned(4)));
	SceSize size;
} RdrfpWriteHeader;

typedef struct RdrfpWritePacket {
	RdrfpHeader head;
	RdrfpWriteHeader write;
} RdrfpWritePacket;

#define RDRFP_PACKET_MAX	0x10000

/* ===== buffers ===== */

#define RDRFP_BUF_RECV		0	/* 9 x 0x11000 */
#define RDRFP_BUF_READ		1	/* 2 x 0x41000 */
#define RDRFP_BUF_WRITE		2	/* 8 x 0x41000 */
#define RDRFP_BUF_TYPES		3

#define RDRFP_BUF_HEADER	0xfd4	/* packet header within a buffer */
#define RDRFP_BUF_DATA		0x1000	/* bulk data within a buffer */
#define RDRFP_WRITE_MAX		0x40000

typedef struct RdrfpBuffer {
	SceUInt32 flags;			/* 1: reserved */
	SceUID block;
	void *base;
} RdrfpBuffer;

typedef struct RdrfpBufferTable {
	SceUID mutex;
	SceUID evf;				/* 1: a buffer was released */
	SceSize size;
	int count;
	RdrfpBuffer *buffers;
} RdrfpBufferTable;

/* ===== write buffers ===== */

typedef struct RdrfpWritebuf {
	RdrfpLink link;
	SceUInt32 ids[8];			/* request IDs of the pieces */
	int count;
	SceUInt32 fd;
	SceSize size;
	int bufType;
	int bufIndex;
	char *buf;
	int flushed;				/* the timer saw it once */
	SceUInt32 reserved;
	SceOff offset;				/* file offset of the first piece */
} RdrfpWritebuf;

/* a file's next expected write offset, remembered for the host */
typedef struct RdrfpFileCacheEntry {
	int used;
	SceUInt32 fd;
	int age;				/* lookups since it was added */
	SceUInt32 result;			/* of the write that ended there */
	SceOff offset;
} RdrfpFileCacheEntry;

#define RDRFP_FILE_CACHE	8

/* ===== devices ===== */

typedef struct RdrfpDevice {
	const char *path;
	SceUInt32 reserved;
	const char *name;			/* as the host shows it */
	SceUInt32 mountPoint;			/* sceIoCreateMountEvent */
	SceUInt32 type;
} RdrfpDevice;

/* a device as the host sees it (device notify, device list) */
typedef struct RdrfpDeviceInfo {
	SceSize size;				/* of this record, padded */
	SceUInt32 reserved;
	SceUInt32 type;
	SceUInt8 attribute[4];			/* 0, 4, 0, 0 */
	SceOff total __attribute__((packed, aligned(4)));
	SceOff free __attribute__((packed, aligned(4)));
	SceSize nameSize;
	char name[];
} RdrfpDeviceInfo;

#define RDRFP_TYPE_DEVICE_NOTIFY	0x40

/* device notify: whether the device is mounted, and what it is */
typedef struct RdrfpDeviceNotify {
	RdrfpHeader head;
	SceUInt32 mounted;
	RdrfpDeviceInfo info;
} RdrfpDeviceNotify;

#define RDRFP_DEVICES		3

/* ===== the protocol ===== */

typedef struct RdrfpContext {
	int handle;				/* sceDfMgrOpen */
	int connected;
	int enabled;				/* protocol enabled by the host */
} RdrfpContext;

/* rdrfp.c */
extern RdrfpContext rdrfp;
int rdrfpIsEnabled(void);
RdrfpRequest *rdrfpDequeueRequest(void);
RdrfpRequest *rdrfpPrepareRequest(SceUInt32 id, int bufType, int bufIndex, void *packet);
void rdrfpTerminateRequest(RdrfpRequest *req);
void rdrfpSetPacketSize(void *packet, SceSize size);
void rdrfpSetPacketType(RdrfpHeader *packet, SceUInt32 type);
void rdrfpSetPacketId(RdrfpHeader *packet, SceUInt16 id);
int rdrfpSendPacket(RdrfpRequest *req);
int rdrfpIsRequestQueueEmpty(void);
void rdrfpQueueRequest(RdrfpRequest *req);
void *rdrfpAlloc(SceSize size);
void rdrfpFree(void *p);
void rdrfpRequestWritebuf(RdrfpWritebuf *wb, SceUInt8 kind);
void rdrfpFlushWritebuf(void);

/* rdrfp_queue.c */
int rdrfpInitQueue(RdrfpQueue *queue);
void rdrfpEnqueue(RdrfpQueue *queue, RdrfpRequest *req);
void rdrfpRemoveFromQueue(RdrfpQueue *queue, RdrfpRequest *req);
RdrfpRequest *rdrfpDequeue(RdrfpQueue *queue, int wait);
int rdrfpIsQueueEmpty(const RdrfpQueue *queue);

/* rdrfp_buffer.c */
int rdrfpInitBufferTable(void);
int rdrfpReserveBuffer(int type, int wait);
void rdrfpReleaseBuffer(int type, int index);
void *rdrfpBufferAddr(int type, int index);
int rdrfpInitDesc(void);
RdrfpDesc *rdrfpCreateDesc(SceUInt32 id, int bufType, int bufIndex, void *packet);
void rdrfpDeleteDesc(RdrfpDesc *desc);
void rdrfpSetDesc(RdrfpDesc *desc, int bufType, int bufIndex, void *packet, const void *data,
		  SceSize dataSize, int releaseBuffer);
void rdrfpSetDescExtra(RdrfpDesc *desc, void *extra);

/* rdrfp_writebuf_list.c */
int rdrfpInitWritebufList(void);
RdrfpWritebuf *rdrfpCreateWritebuf(SceUInt32 fd, SceOff offset, int bufType, int bufIndex);
void rdrfpDeleteWritebuf(RdrfpWritebuf *wb);
void rdrfpPutWritebufToList(RdrfpWritebuf *wb);
RdrfpWritebuf *rdrfpGetWritebufFromList(void);
RdrfpWritebuf *rdrfpFindWritebuf(SceUInt32 fd, SceOff offset, SceSize size);
void rdrfpWritebufAddSize(RdrfpWritebuf *wb, SceSize size);
void rdrfpWritebufAddId(RdrfpWritebuf *wb, SceUInt32 id);
int rdrfpRegisterWritebufTimer(void);
void rdrfpUnregisterWritebufTimer(void);
int rdrfpInitFileCache(void);
RdrfpFileCacheEntry *rdrfpAddFileCache(SceUInt32 fd, SceOff offset, SceUInt32 result);
void rdrfpDeleteFileCache(RdrfpFileCacheEntry *entry);
RdrfpFileCacheEntry *rdrfpFindFileCache(SceUInt32 fd, SceOff offset);
void rdrfpFileCacheAddOffset(RdrfpFileCacheEntry *entry, SceSize size);

/* rdrfp_fs.c */
extern const RdrfpDevice rdrfpDevices[RDRFP_DEVICES];
int rdrfpInitFS(void);
SceSize rdrfpMakeDeviceInfo(int dev, SceOff total, SceOff free, RdrfpDeviceInfo *info);
int getDeviceInfo(int dev, SceOff *total, SceOff *free);
int rdrfpCheckDeviceName(const char *path, SceSize len);

/* rdrfp_fs_cmds.c */
int rdrfpFSCallCommandHandler(RdrfpRequest *req, SceUInt32 command, RdrfpHeader *packet, void *args);

/* rdrfp_list.c: lists guarded by a mutex */
typedef struct RdrfpList {
	RdrfpLink head;
	SceUID mutex;
	int count;
} RdrfpList;

typedef struct RdrfpListElem {
	RdrfpLink link;
	void *data;
	SceUInt32 reserved;
} RdrfpListElem;

typedef int (*RdrfpListCompare)(void *data, const void *key);

int rdrfpInitList(RdrfpList *list);
int rdrfpDeleteList(RdrfpList *list);
RdrfpListElem *rdrfpAddToList(RdrfpList *list, void *data);
void rdrfpRemoveFromList(RdrfpList *list, RdrfpListElem *elem);
RdrfpListElem *rdrfpGetListElem(RdrfpList *list, int index);
RdrfpListElem *rdrfpFindListElem(RdrfpListCompare compare, RdrfpList *list, const void *key);

/* rdrfp_vdentry.c: the virtual directories of the host's tree */
typedef struct RdrfpVDEntry {
	char *name;
	SceSize nameSize;
	SceUInt32 flags;
	struct RdrfpVDEntry *parent;
	RdrfpList children;
	char *mountPoint;			/* SceAppMgr mount, when mounted */
	int visited;				/* found by the last update */
} RdrfpVDEntry;

RdrfpVDEntry *rdrfpCreateVDEntry(const char *name);
void rdrfpDeleteVDEntry(RdrfpVDEntry *entry);
RdrfpVDEntry *rdrfpAddVDEntry(RdrfpVDEntry *parent, const char *name);
RdrfpVDEntry *rdrfpFindVDEntry(RdrfpVDEntry *parent, const char *name);
int rdrfpVDEntryDopen(RdrfpVDEntry *entry);
int rdrfpVDEntryDclose(RdrfpVDEntry *entry);
void rdrfpVDEntryGetstat(RdrfpVDEntry *entry, SceIoStat *stat);
int rdrfpVDEntryDread(RdrfpVDEntry *entry, int index, SceIoDirent *dirent);
int rdrfpUpdateVDEntryOfApp(RdrfpVDEntry *title);
int rdrfpUpdateVDEntryOfSavedata(RdrfpVDEntry *title, const char *path);
int rdrfpVDEntryMakePath(RdrfpVDEntry *entry, const char *rest, char **path);
int rdrfpVDEntrySetMountPoint(RdrfpVDEntry *entry, const char *mountPoint);
void rdrfpVDEntryClearMountPoint(RdrfpVDEntry *entry);
int rdrfpVDEntryIsMounted(RdrfpVDEntry *entry);

/* rdrfp_vdtree.c: the file system as the host sees it */
typedef struct RdrfpFd {
	SceUInt32 reserved0;
	SceUInt32 reserved1;
	SceUInt32 fd;				/* the host's number for it */
	SceUID uid;				/* -1: a virtual directory */
	RdrfpVDEntry *entry;			/* a virtual directory */
	int readIndex;				/* next virtual directory entry */
	SceUInt32 flags;			/* RDRFP_FD_* */
	SceUInt32 reserved2;
} RdrfpFd;

int rdrfpGetstat(const char *path, SceSize len, SceIoStat *stat);
int rdrfpGetstatByFd(SceUInt32 fd, SceIoStat *stat);
int rdrfpChstat(const char *path, SceSize len, SceIoStat *stat, int bits);
int rdrfpChstatByFd(SceUInt32 fd, SceIoStat *stat, int bits);
int rdrfpOpen(const char *path, SceSize len, int flags, SceIoMode mode, RdrfpFd **fd);
int rdrfpClose(SceUInt32 fd);
int rdrfpRead(SceUInt32 fd, void *buf, SceSize size, SceOff offset);
int rdrfpWrite(SceUInt32 fd, const void *buf, SceSize size, SceOff offset);
int rdrfpDopen(const char *path, SceSize len, RdrfpFd **fd);
int rdrfpDclose(SceUInt32 fd);
int rdrfpDread(SceUInt32 fd, SceIoDirent *dirent);
int rdrfpRmdir(const char *path, SceSize len);
int rdrfpRemove(const char *path, SceSize len);
int rdrfpMkdir(const char *path, SceSize len, SceIoMode mode);
int rdrfpRename(const char *oldPath, SceSize oldLen, const char *newPath, SceSize newLen);
int rdrfpMountAppData(int type, const char *titleId, void *arg);
int rdrfpUmountAppData(int type, const char *titleId);

int rdrfpVDTreeInit(void);
int rdrfpUpdateVDTree(void);
void rdrfpDeleteVDTreeOfGameCard(void);

/* rdrfp_fd.c */
/* RdrfpFd.flags */
#define RDRFP_FD_ROOT_DIR		1	/* ux0:/ */
#define RDRFP_FD_SCE_SYS		2	/* a title's sce_sys file */
#define RDRFP_FD_SAVEDATA_DIR		4	/* ux0:data/savedata */
#define RDRFP_FD_SCE_SYS_DIR		8	/* a title's sce_sys */
#define RDRFP_FD_ELEVATED		(RDRFP_FD_SCE_SYS | RDRFP_FD_SAVEDATA_DIR | RDRFP_FD_SCE_SYS_DIR)

void rdrfpInitFdList(void);
RdrfpFd *rdrfpCreateFd(RdrfpVDEntry *entry, SceUID uid);
void rdrfpDeleteFd(SceUInt32 fd);
RdrfpFd *rdrfpFindFd(SceUInt32 fd);
void rdrfpSetFdFlags(RdrfpFd *f, SceUInt32 flags);
int rdrfpFdIsRootDir(RdrfpFd *f);
int rdrfpFdIsElevated(RdrfpFd *f);
int rdrfpFdIsSceSysDir(RdrfpFd *f);
