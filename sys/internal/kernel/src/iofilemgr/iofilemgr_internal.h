#ifndef IOFILEMGR_INTERNAL_H
#define IOFILEMGR_INTERNAL_H

#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
#include <sysmem/sysmem.h>
#include <sysmem/cpu.h>
#include <sysmem/procevent.h>
#include <threadmgr/fast_mutex_object.h>
#include <iofilemgr/vfs.h>
#include <acmgr/acmgr.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/dipsw.h>
#include <sysmem/suspend.h>
#include <sysmem/debug.h>
#include <processmgr/processmgr.h>
#include <intrmgr/syscall.h>
#include <kernel/error.h>
#include <iofilemgr/iofilemgr.h>
#include <threadmgr/event.h>
#include <threadmgr/uid_waitable.h>
#include <threadmgr/fast_mutex.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/sysroot_init.h>
#include <sysmem/uid_guid.h>
#include <sysmem/uid_class.h>

/* Shared declarations describe caller buffers, mount/vnode/file objects,
 * scheduler requests and VFS/VOP callback tables. */
_Static_assert(sizeof(SceIoFdInfo) == 8, "file descriptor information ABI");
_Static_assert(sizeof(SceVfsInfo) == 0x20, "filesystem registration ABI");
_Static_assert(sizeof(SceVfsPath) == 0x10, "filesystem path ABI");
_Static_assert(sizeof(SceVfsMountData) == 0x14, "mount request ABI");

/* process event phases the handlers act on */
#define SCE_PROC_EVENT_PHASE_SUSPEND	0x800
#define SCE_PROC_EVENT_PHASE_RESUME	0x10000

#define IOFILEMGR_PROCESS_MAX		16

/* IofilemgrKpls.phase */
#define IOFILEMGR_KPLS_PHASE_RESUMED	1
#define IOFILEMGR_KPLS_PHASE_SUSPENDED	2
#define IOFILEMGR_KPLS_PHASE_FINALIZED	4

/* name unknown: the module's process local storage ("SceIofilemgr" KPLS) */
typedef struct IofilemgrKpls {
	SceInt32 fdMax;			/* the process's fd limit */
	SceInt32 fdCount;
	SceInt32 dialogFdMax;		/* the same for Common Dialog threads */
	SceInt32 dialogFdCount;
	SceInt32 phase;
	SceInt32 priority;
	SceInt32 reserved;
	SceInt32 depth;
	SceInt32 fdPeak;
	SceInt32 dialogFdPeak;
} IofilemgrKpls;
_Static_assert(sizeof(IofilemgrKpls) == 0x28, "IofilemgrKpls");

/* SceVfsFile.debugPath: the paths a file was opened with */
struct DebugPath {
	char *resolvedOpenPath;
	SceSize resolvedOpenPathLength;
	int resolvedOpenPathPool;	/* the g_vfsDebugBufferPool index */
	char *openPath;			/* after resolvedOpenPath when both fit */
	SceSize openPathLength;
	int openPathPool;
	int flags;
	SceMode mode;
};

/* no I/O priority set for the thread: the process default applies */
#define IO_PRIORITY_UNSET	((SceInt32)0x80000000)

/* name unknown: the thread local storage slot g_iofilemgrTlsKey (0x14 bytes) */
typedef struct IofilemgrTls {
	SceInt32 priority;			/* the thread's I/O priority, IO_PRIORITY_UNSET when none */
	SceSize pathLength;			/* the last path's characters after its device name */
	IoRequest *request;			/* the request the thread runs for the scheduler */
	SceUID requesterThreadId;
	SceUInt16 inAsyncCall;			/* the thread runs an asynchronous dopen, dread or dclose */
	SceInt16 isCountedWorker;			/* counted in IoScheduler.workerNum by the dispatcher */
} IofilemgrTls;
_Static_assert(sizeof(IofilemgrTls) == 0x14, "IofilemgrTls");

/* name unknown: what sceIoGetRemoteKPLSData copies out */
typedef struct IofilemgrRemoteKplsData {
	SceInt32 fdCount;
	SceInt32 fdPeak;
	SceInt32 dialogFdCount;
	SceInt32 dialogFdPeak;
	SceInt32 fdMax;
	SceInt32 depth;
	SceInt32 priority;
} IofilemgrRemoteKplsData;

/* name unknown: a file's I/O scheduling parameters (getFileSchedParam / setFileSchedParam) */
typedef struct IofilemgrSchedInfo {
	SceInt32 priority;			/* the I/O priority, 8 by default */
	SceInt32 mntOpt;			/* the device's mount option; negative: no scheduling */
	SceUID pid;
} IofilemgrSchedInfo;

/* the I/O scheduler: one per kind of device, with a worker thread and a queue per priority */
#define IO_SCHED_PRIORITY_NUM		16

/* I/O priorities: 0 to 15; game programs see theirs shifted by IO_PRIORITY_GAME_BASE */
#define IO_PRIORITY_MAX			15
#define IO_PRIORITY_GAME_BASE		6
#define IO_PRIORITY_GAME_MAX		4	/* shifted */
#define IO_PRIORITY_NON_GAME		11	/* the only priority of a non-game program */

/* the priorities a kernel caller may give a game program (names unknown) */
#define IO_PRIORITY_IS_SYSTEM(p)	((SceUInt32)((p) - 1) <= 2)		/* 1 to 3 */
#define IO_PRIORITY_IS_GAME(p)		((SceUInt32)((p) - 6) <= 4)		/* 6 to 10 */
#define IO_PRIORITY_IS_CDIALOG(p)	((SceUInt32)((p) - 12) <= 2)	/* 12 to 14 */

/* the kinds of device (the low 16 bits of a mount option) a scheduler serves (names unknown) */
#define IO_SCHED_TYPE_INTERNAL		0x100
#define IO_SCHED_TYPE_GAME_CARD		0x201
#define IO_SCHED_TYPE_REMOVABLE		0x202
#define IO_SCHED_TYPE_GAME_CARD2	0x203
#define IO_SCHED_TYPE_HOST		0x300

struct IoScheduler;
struct SchedDispatcher;

/* sched_io_request_queue.c: a scheduler's requests of one priority (name unknown) */
typedef struct IoRequestQueue {
	struct IoScheduler *sched;
	SceInt32 priority;
	IoRequest *head;
	IoRequest *tail;
	SceUInt32 timeout;			/* 10000000 (not yet traced) */
} IoRequestQueue;

/* what a scheduler is created with (name unknown) */
typedef struct IoSchedulerAttr {
	int type;				/* IO_SCHED_TYPE_* */
	const char *name;
	SceSize readChunkSize;
	SceSize writeChunkSize;
} IoSchedulerAttr;

/* a scheduler's worker thread; its address is the thread's argument (name unknown) */
typedef struct IoSchedulerWorker {
	SceUID thid;
	SceUID mutex;				/* IoScheduler.mutex */
	SceUID cond;				/* IoScheduler.cond */
	struct IoScheduler *sched;
} IoSchedulerWorker;

/* sched_scheduler.c (name unknown: Sony's functions call it IoScheduler) */
typedef struct IoScheduler {
	int type;				/* IO_SCHED_TYPE_* */
	IoRequestQueue *queue[IO_SCHED_PRIORITY_NUM];
	SceSize readChunkSize;			/* 0: reads are not split */
	SceSize writeChunkSize;
	struct SchedDispatcher *dispatcher;
	SceUID mutex;
	SceUID cond;
	SceInt8 runningNum;			/* requests executing outside the mutex */
	SceUInt8 reserved0;
	SceInt8 issueCount;			/* sceIoSchedulerIssueAnotherIoRequestInternal */
	SceUInt8 reserved1;
	SceUInt32 reserved2;
	IoSchedulerWorker worker;
	int asyncRequestNum;			/* asynchronous requests not yet complete */
	SceInt16 workerResult;			/* dispatcher worker return value */
	SceInt16 asyncWaiting;			/* an asynchronous requester waits on cond */
	SceUInt32 writeDelay;			/* delay after high-priority I/O, microseconds */
	IoRequest *runningRequest;
	int workerNum;				/* dispatcher workers running for it */
	SceUInt32 eventBits;			/* its bits in the dispatcher's event flag */
	SceUInt64 priorityWaitStart;
	SceUInt64 writeDelayStart;
	SceUInt64 reserved3;
	IoRequest *runningHead;
	IoRequest *runningTail;
} IoScheduler;
_Static_assert(sizeof(IoScheduler) == 0xa8, "IoScheduler");

/* sched_dispatcher.c: hands each request to the scheduler of its device (name unknown) */
typedef struct SchedDispatcher {
	SceUInt32 reserved[20];			/* nothing seen uses it */
	IoScheduler *internal;
	IoScheduler *gameCard;
	IoScheduler *removable;
	IoScheduler *host;
	IoScheduler *other;			/* "Default" */
	SceUInt32 timeout;			/* 10000000 (not yet traced) */
	SceKernelFastMutex mutex;
	SceUID evf;
	int fsResourceNum;			/* fs resources given with the 0x10000 bit */
	SceUInt32 fsResource;			/* the fs resources available */
	int chunkAlignmentOverride;
} SchedDispatcher;
_Static_assert(sizeof(SchedDispatcher) == 0xb8, "SchedDispatcher");

/* io_scheduler_async.c: a "SceUIDIoAsyncEventClass" object (name unknown) */
typedef struct IoAsyncEvent {
	SceUIDEventObject base;
	IoRequest request;
	SceUInt32 reserved[2];			/* not yet traced */
} IoAsyncEvent;
_Static_assert(sizeof(IoAsyncEvent) == 0xf8, "IoAsyncEvent");

/* names unknown: the arguments of the vfs calls made with SceThreadmgrForDriver_E54FD746 */
typedef struct IofilemgrCloseParam {
	SceUID fd;
} IofilemgrCloseParam;

typedef struct IofilemgrOpenParam {
	const char *path;
	int flags;
	SceMode mode;
} IofilemgrOpenParam;

typedef struct IofilemgrReadParam {
	SceUID fd;
	void *data;
	SceSize size;
} IofilemgrReadParam;

typedef struct IofilemgrLseekParam {
	SceUID fd;
	int whence;
	SceOff offset;
} IofilemgrLseekParam;

typedef struct IofilemgrDopenParam {
	const char *dirname;
} IofilemgrDopenParam;

typedef struct IofilemgrDreadParam {
	SceUID fd;
	SceIoDirent *dir;
	int preserveAttributes;
} IofilemgrDreadParam;

typedef struct IofilemgrDcloseParam {
	SceUID fd;
	int reserved;
} IofilemgrDcloseParam;

typedef struct IofilemgrRemoveParam {
	const char *file;
	int flags;
} IofilemgrRemoveParam;

typedef struct IofilemgrMkdirParam {
	const char *dir;
	SceMode mode;
} IofilemgrMkdirParam;

typedef struct IofilemgrRmdirParam {
	const char *path;
} IofilemgrRmdirParam;

typedef struct IofilemgrGetstatParam {
	const char *file;
	SceIoStat *stat;
	int preserveAttributes;
} IofilemgrGetstatParam;

typedef struct IofilemgrGetstatByFdParam {
	SceUID fd;
	SceIoStat *stat;
} IofilemgrGetstatByFdParam;

typedef struct IofilemgrSyncByFdParam {
	SceUID fd;
	int flag;
} IofilemgrSyncByFdParam;

typedef struct IofilemgrFlockParam {
	SceUID fd;
	int op;
} IofilemgrFlockParam;

typedef struct IofilemgrRenameParam {
	const char *oldname;
	const char *newname;
} IofilemgrRenameParam;

typedef struct IofilemgrDevctlParam {
	const char *dev;
	unsigned int cmd;
	void *indata;
	int inlen;
	void *outdata;
	int outlen;
} IofilemgrDevctlParam;

typedef struct IofilemgrSyncParam {
	const char *device;
	unsigned int flag;
} IofilemgrSyncParam;

/* ioctl and devctl command bits checked by sceIoIoctl and sceIoDevctl */
#define IOFILEMGR_IOCTL_DISABLED	0x200	/* rejected */
#define IOFILEMGR_IOCTL_ACCESS_SYSTEM	0x400	/* with ACCESS_CHECK: system or root programs only */
#define IOFILEMGR_IOCTL_ACCESS_CHECK	0x800

/*
 * Sony's declaration gives sceKernelUserUnmap the mapped range as well (a cast
 * of vitasdk's one-argument prototype would be a call through an incompatible
 * type, which GCC turns into abort())
 */
int ioUserUnmap(SceUID uid, const void *data, SceSize size) __asm__("sceKernelUserUnmap");

/* iofilemgr.c */
int iofilemgrInitDummyTty(uint32_t unused0, void *unused1);
int sceIoLseek32(SceUID fd, int offset, int whence);

/* iofilemgr_for_kernel.c */
SceUID sceIoDopenInternal(const char *dirname);
int sceIoDreadInternal(SceUID fd, SceIoDirent *dir);
int sceIoDcloseInternal(SceUID fd);

/* SceSysmemForDriver_7D4F8B5F: what a mapping allows, and its flags (names unknown) */
#define IO_USER_MAP_READ		1	/* the kernel reads the buffer: a write */
#define IO_USER_MAP_WRITE		2	/* the kernel writes the buffer: a read */
#define IO_USER_MAP_FLAGS		1
#define IO_USER_MAP_FLAGS_GAME		0x11

/* flock.c: advisory file locks, one reader-writer lock per file id of a mount */

/* vfs_flock() op */
#define FLOCK_OP_SH		1	/* shared */
#define FLOCK_OP_EX		2	/* exclusive */
#define FLOCK_OP_RS		4	/* reserved: shared now, the next exclusive one */
#define FLOCK_OP_UN		0x10	/* unlock */
#define FLOCK_OP_MASK		0x1f
#define FLOCK_OP_NB		0x100	/* fail instead of waiting */

/* FlockTag.type: the lock a process holds */
#define FLOCK_TAG_NONE		0
#define FLOCK_TAG_EX		1
#define FLOCK_TAG_SH		2
#define FLOCK_TAG_RS		4
#define FLOCK_TAG_FREE		0xffff

/* FlockLock.lock_val: the exclusive bit (held or waited for) and 2 per shared holder */
#define FLOCK_LOCK_EX		1
#define FLOCK_LOCK_SH		2

/* FlockLock.waitQueue bits */
#define FLOCK_WAKE_EX		1	/* an exclusive waiter */
#define FLOCK_WAKE_SH		2	/* the shared waiters */
#define FLOCK_WAKE_RS		4	/* the reserved holder's exclusive wait */

#define FLOCK_TAG_MAX		16
#define FLOCK_LIST_MAX		16

/* a process's hold on a lock */
typedef struct FlockTag {
	SceUID pid;
	SceUInt16 type;			/* FLOCK_TAG_* */
	SceUInt16 ref_count;		/* the process's files that use the lock */
} FlockTag;

typedef struct FlockLock {
	SceUInt32 lock_val;
	SceInt32 write_waiters;
	SceKernelFastMutex mutex;	/* "SceIofileFlockLock" */
	SceUID waitQueue;		/* "SceIofileFlockWaitQueue" event flag */
	SceInt16 use_count;		/* the Flock entries that share the lock */
	SceInt16 reserved_waiter;
	SceUID reserved_pid;
	FlockTag tag[FLOCK_TAG_MAX];
} FlockLock;
_Static_assert(sizeof(FlockLock) == 0xd4, "FlockLock");

/* SceVfsVnode.core.flock: a file id's lock */
struct Flock {
	SceUInt64 fid;
	SceInt32 refcount;		/* the vnodes that use the entry */
	FlockLock *lock;
	struct Flock *next;
};
_Static_assert(sizeof(struct Flock) == 0x18, "Flock");

/* name unknown: the entries of one mount */
typedef struct FlockList {
	SceVfsMount *mnt;
	struct Flock *head;
} FlockList;

/* a vnode's file id, which the vnode keeps as two words */
static inline SceUInt64 vnodeFid(const SceVfsVnode *vp)
{
	return *(const SceUInt64 *)vp->core.fid;
}

extern int g_flockInitialized;
extern SceKernelFastMutex g_flockMutex;	/* "SceIofileFlock" */
extern FlockList g_flockList[FLOCK_LIST_MAX];

int vfs_flock_init(void);
int vfs_flock(SceVfsFile *file, int op, SceUID pid, SceVfsVnode *vp);
int vfs_flock_close(SceVfsFile *file, SceUID pid, SceVfsVnode *vp);
int vfs_flock_dup(SceVfsVnode *src, SceVfsVnode *dst);
int vfs_flock_release(SceUID pid, SceVfsVnode *vp);
int vfs_release_lock_entry(SceVfsVnode *vp);

/* vfs.c, vfs_subr.c: the name cache, path lookup and mounts */

/* SceVfsMount.state */
#define VFS_MOUNT_STATE_MOUNTED		1
#define VFS_MOUNT_STATE_MOUNTING_BIT	0x100	/* set while vfsMount runs */
#define VFS_MOUNT_STATE_MOUNTING	(VFS_MOUNT_STATE_MOUNTING_BIT | VFS_MOUNT_STATE_MOUNTED)
#define VFS_MOUNT_TYPE_NONE		4	/* name unknown: vfsMount does nothing */
#define VFS_MOUNT_FLAG_PARAM_MASK	0xfffff	/* the mntFlags vfsMount takes from the caller */
#define VFS_MOUNT_OPT_DEV_MASK		0xf00
#define VFS_MOUNT_OPT_DEV_EXTERNAL	0x200	/* removable media (VFS_MOUNT_OPT_VC/MC/SD) */
/* in vp->core.mnt_opt: a write from the I/O buffer's start past its end bypasses it (name unknown) */
#define VFS_VNODE_MNT_OPT_DIRECT_WRITE	0x10000
#define VFS_VNODE_TYPE_HOST_ROOT	0x40	/* name unknown: the root vnode of host0: */
#define VFS_MOUNT_STATE_UMOUNTING	0x10

/* SceVnode.core.type: the root vnode of a mount on another mount (name unknown) */
#define SCE_VNODE_TYPE_ROOT_MOUNT	0x10000

/* vfs_lookup() flags (the low 20 bits are the lookup kind) */
#define VFS_LOOKUP_TRYLOCK		0x40
#define VFS_LOOKUP_CLEAR_NEGATIVE	0x100000

#define VFS_PATH_BUFF_SIZE		0x400
#define VFS_PATH_ELEM_MAX		16

/* PFS's EACCES (name unknown) */
#define VFS_ERROR_PFS_EACCES		0x800F090D
#define VFS_ERROR_RMDIR_IGNORED		0x8014001A	/* name unknown: vfsRmdir reports success */
#define VFS_DEVCTL_SYSTEM_ONLY		0x80000001	/* name unknown: a devctl only system or root programs may send */
#define VFS_CST_VSHBRIDGE_ONLY		0x10000	/* name unknown: a chstat bit only vshbridge may set */

/* Ncache.state */
#define NCACHE_STATE_VALID		1
#define NCACHE_STATE_NEGATIVE		2
#define NCACHE_STATE_WHITEOUT		4	/* a removed file still open, under a SCEDEL~ name */
#define NCACHE_STATE_RENAMING		0x1000
#define NCACHE_STATE_INVALID		0x2000

/* negative entries kept before the oldest is dropped */
#define NCACHE_NEGATIVE_MAX		8

/* one element of a path */
typedef struct VfsPathElem {
	const char *name;
	SceSize len;
} VfsPathElem;

/* SceVfsMount.fumount (in the mount's opaque2): a mount whose device can go away */
struct Fumount {
	SceVopTable *vops;		/* the vops that fail while the device is gone */
	SceUInt32 busy;			/* name unknown: set around a sync of the mount */
	SceUInt32 *devState;		/* 1: the device is active */
	SceUID *activationEvf;		/* the event flag set when the device comes back */
};

/* SceVfsVnode.core.ncache: a name cache entry */
struct Ncache {
	struct Ncache *next;		/* the next child of ncdd */
	struct Ncache *ncdd;		/* the parent directory's entry */
	struct Ncache *listNext;	/* the list of every entry */
	struct Ncache *child;		/* the first child */
	union {
		SceVfsVnode *vp;
		struct Ncache *negativeNext;	/* NCACHE_STATE_NEGATIVE: the negative list */
	};
	SceUInt32 state;		/* NCACHE_STATE_* */
	SceSize nameLength;
	char *name;
};
_Static_assert(sizeof(struct Ncache) == 0x20, "Ncache");

/* vfs.c */
void vfs_umount_cleanup(SceVfsMount *mnt);
void vfs_decrement_vp_mnt_ref(SceVfsVnode *vp);
int vfs_enter_root_ncache(SceVfsVnode *vp, const VfsPathElem *elem);
int clear_ncache(struct Ncache *nc, int recursive);
int enter_ncache(SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem, int state, int decode);
int vfs_lookup(SceVfsVnode *dvp, const VfsPathElem *elem, const char *path, SceUInt32 flags,
	       SceVfsVnode **vpp);
int vfs_find_in_vnode(SceVfsVnode *dvp, const VfsPathElem *elem, const char *path, SceUInt32 flags,
		      SceVfsVnode **vpp);
int vfs_get_vnode_name(SceVfsVnode *vp, char *name, SceSize n, SceSize *result);
int set_ncache_state(struct Ncache *nc, SceUInt32 from, SceUInt32 to);
int restore_ncache(SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem, SceVfsVnode *ovp,
		   SceUID oldHeap);
int check_ncache(struct Ncache *nc);
int replace_flat_path(const char *path, char *flat, VfsPathElem *elem, int *num, int absolute);
void vfs_release_lookup(char *path, SceVfsVnode *vp1, SceVfsVnode *vp2, char *path2,
			SceVfsVnode *vp3, SceVfsVnode *vp4, int keepMnt);
int vfs_find_in_path(VfsPathElem *elem, int num, SceVfsVnode **vpp, const char *path,
		     SceUInt32 flags);
SceVfsVnode *vfs_get_devfile_vnode(const char *path, char *flat, VfsPathElem *last);
int vfs_system_event_handler(int resume, int eventid, void *args, void *opt);

/* vfs_subr.c */

/* a SceUIDVfsFileClass object: the UID of an open file */
typedef struct SceUIDVfsFileObject {
	SceObjectBase base;
	SceVfsFile file;
} SceUIDVfsFileObject;
_Static_assert(sizeof(SceUIDVfsFileObject) == 0x48, "SceUIDVfsFileObject");

#define VFS_MOUNT_MAX			0x60
#define VFS_VNODE_MAX			0x1000
#define VFS_VOP_TABLE_CACHE_NUM		4
#define VFS_PATH_BUFF_CACHE_NUM		4
#define VFS_DEBUG_BUFF_POOL_NUM		16
#define VFS_MOUNT_OPT_VC		0x201	/* the game card */
#define VFS_MOUNT_OPT_MC		0x202	/* the memory card */
#define VFS_MOUNT_OPT_SD		0x203
#define VFS_FILE_ATTR_INTERNAL		0x1	/* on a SCE_VFS_MOUNT_FLAG_INTERNAL mount */
#define VFS_FILE_ATTR_EXTERNAL		0x2	/* on a SCE_VFS_MOUNT_FLAG_EXTERNAL mount */
#define VDLOCK_OWNER_HANDOVER		(-3)	/* the owner while the lock passes to a waiter */
#define FDLOCK_STATE_BUSY		1
#define FDLOCK_STATE_UMOUNTING		0x10

/* SceVfsMount.fd_lock (in the mount's opaque): the lock of the mount's files */
struct FdLock {
	SceUID mutex;			/* "SceVfsFdLock" */
	SceUID cond;			/* "SceVfsFdCond" */
	SceUInt32 state;		/* FDLOCK_STATE_* */
	SceInt32 waitNum;		/* the threads waiting on cond */
};

/* a pool of fixed size buffers (bufferPoolInitialize) */
typedef struct BufferPoolNode {
	void *buf;
	struct BufferPoolNode *next;
} BufferPoolNode;

typedef struct BufferPool {
	BufferPoolNode *volatile freeList;
	BufferPoolNode *volatile nodeList;
	void *base;
	SceSize size;
	SceUInt32 num;
	SceUInt32 reserved[3];
} BufferPool;
_Static_assert(sizeof(BufferPool) == 0x20, "BufferPool");

int vfs_fumount_vop_enodev(void *argp);
int vfs_file_class_constructor(void *data);
int vfs_file_class_destructor(void *data);
SceVfsMount *vfs_alloc_mount(void);
void vfs_init_mount(SceVfsMount *mnt, SceVfsVnode *vp, SceUID allocator, SceVfsInfo *vfsInf);
int vfs_create_mnt_list_mtx(void);
int vfs_lock_mnt_list(void);
int vfs_unlock_mnt_list(void);
const char *find_mounted_path(const char *name, SceSize len);
int vfs_create_ncache_mtx(void);
void vfs_lock_ncache(void);
void vfs_unlock_ncache(void);
int vfs_create_vfs_mtx(void);
int vfs_lock_vfs(void);
int vfs_unlock_vfs(void);
int vfs_free_vop_table(SceVopTable *tbl);
void fini_vnode(SceVfsVnode *vp);
SceVopTable *vfs_alloc_vop_table(void);
int vfs_debug_extend_buff(int index);
char *vfs_debug_allocate_buff(int *index);
void vfs_free_mount(SceVfsMount *mnt);
SceBool vfsIsLockedMnt(SceVfsMount *mnt);
int vfsLockFd(SceVfsFile *file);
int vfsLockFdByVnode(SceVfsVnode *vp);
int vfsTrylockFd(SceVfsFile *file);
int vfsUnlockFd(SceVfsFile *file);
int vfsWaitFd(SceVfsFile *file);
SceBool vfsIsLockedFd(SceVfsFile *file);
int vfsLockFdAll(SceVfsMount *mnt);
int vfsUnlockFdAll(SceVfsMount *mnt);
int vfsSignalFd(SceVfsMount *mnt);
int lock_vdlock(SceVfsVnode *vp);
SceBool vfsIsOwnerVnode(SceVfsVnode *vp);
struct Ncache *vfs_alloc_ncache(SceUID heap, const VfsPathElem *elem);
void vfs_free_ncache_heap(struct Ncache *nc, SceUID heap);
void vfs_free_ncache(struct Ncache *nc);
void vfs_set_ncache_entry(struct Ncache *nc, SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem,
		    SceUInt32 state);
void vfs_insert_mnt_list(SceVfsMount *mnt);
int vfs_remove_mnt_list(SceVfsMount *mnt);
void vfs_insert_mntedon_list(SceVfsMount *pmnt, SceVfsMount *mnt);
int vfs_remove_mntedon_list(SceVfsMount *pmnt, SceVfsMount *mnt);
int vfs_remove_mnt_vnode_list(SceVfsMount *mnt, SceVfsVnode *vp);
int vfs_finalize_mount(SceVfsMount *mnt);
SceVfsInfo *vfs_acquire_vfs(const char *name);
int vfs_release_vfs(const char *name);
void vfs_initialize_file(SceVfsVnode *vp, SceVfsFile *file);
int vfs_close_file(SceVfsVnode *vp, SceUID fd);
int vfs_close_file_dummy(SceUID fd);
int vfs_convert_file(SceUID fd, SceVfsFile **file);
int vfs_release_file(SceUID fd);
int vfs_get_file_state_error(SceVfsFile *file);
int vfs_get_file_closed_error(SceVfsFile *file);
void vfs_insert_ncache_list(struct Ncache *nc);
void vfs_remove_ncache_list(struct Ncache *nc);
SceVfsMount *vfs_get_mnt_list(void);
int vfs_file_class_init(void);
int vfs_vnode_initialize(void);
int vfs_is_full_path(const char *path);
int vfs_set_fumount_vop(SceVfsMount *mnt);
int vfs_set_fumount_vop_vnode(SceVfsVnode *vp);
int vfs_process_finalize(SceUID pid);
BufferPool *vfs_get_vnode_pool(void);
void vfs_vop_table_cache_initialize(void);
char *vfs_path_buff_alloc(void);
void vfs_path_buff_free(char *buf);
void vfs_path_buff_initialize(void);
int vfs_sync(SceVfsMount *mnt);
void vfs_set_mnt_sync_request(SceVfsMount *mnt);
void vfs_clear_mnt_sync_request(SceVfsMount *mnt);
int vfs_sync_all_mount_point(void);
int vfs_debug_path_create(SceVfsFile *file, const char *path);
int vfs_debug_set_resolved_path(SceVfsFile *file, const char *path);
int vfs_debug_set_open_path(SceVfsFile *file, const char *path);
int vfs_debug_set_open_param(SceVfsFile *file, int flags, SceMode mode);
int vfs_debug_path_free(SceVfsFile *file);
int vfs_finalize_file(SceVfsVnode *vp, SceVfsFile *file);
void vfs_debug_buff_initialize(void);
int vfs_set_dev_wait_evf(SceVfsMount *mnt);
int vfs_wait_fumount(SceVfsMount *mnt);
int vfs_check_fd_status(SceVfsFile *file, int closing);
int vfs_suspend_fd(SceUID guid);
int vfs_process_suspend(SceUID pid, int num);
int vfs_change_file_attr(SceUID guid, SceUInt32 bits, int set);
int vfs_signal_fumount(SceVfsMount *mnt);
int vfs_prepare_for_resume(void);
int vfs_create_fdlock(struct FdLock *fdlock);
int vfs_is_cdialog_thread(void);
int vfs_create_file_object(SceUIDVfsFileObject **obj, const char *name);
SceUID vfs_allocate_file(SceVfsVnode *vp, SceVfsFile **file, const char *name);
SceUID vfs_allocate_file_dummy(SceVfsFile **file, const char *path);
int vfs_extend_vnode_buffer(int cur);
int vfs_get_available_vnode_num(void);
int vfs_mount_initialize(SceUID heap);
IofilemgrTls *getIofilemgrTls(void);

extern SceKernelFastMutex g_mntListMutex;
extern SceKernelFastMutex g_ncacheMutex;
extern SceKernelFastMutex g_vfsMutex;
extern SceVfsMount *g_mntList;
extern SceVfsMount *g_freeMntList;
extern SceKernelSpinlock g_freeMntListLock;
extern SceVfsInfo *g_vfsList;
extern struct Ncache *g_ncacheList;
extern SceKernelSpinlock g_pathBuffLock;
extern char *g_pathBuffCache[VFS_PATH_BUFF_CACHE_NUM];
extern SceVopTable *g_vopTableCache[VFS_VOP_TABLE_CACHE_NUM];
extern SceUID g_vfsDebugMemBlock[VFS_DEBUG_BUFF_POOL_NUM];
extern BufferPool *g_vfsDebugBufferPool[VFS_DEBUG_BUFF_POOL_NUM];
extern int g_total_vnode_num;
extern volatile int g_available_vnode_num;
extern BufferPool g_vnodePool;
extern SceClass g_vfsFileClass;
/* names unknown: the activation event flag ("SceIofileDevWaitFor..") and state of each device */
extern SceUID g_vfsDevWaitForMCEvf;
extern SceUInt32 g_vfsDevWaitForMCState;
extern SceUID g_vfsDevWaitForSDEvf;
extern SceUInt32 g_vfsDevWaitForSDState;
extern SceUID g_vfsDevWaitForVCEvf;
extern SceUInt32 g_vfsDevWaitForVCState;
extern SceUID g_vfsDevEvf;		/* the other devices */
extern SceUInt32 g_vfsDevState;
extern int g_vfsSuspended;		/* name unknown: set while the system is suspended */
extern int g_vfsReclaimIndex;		/* name unknown: where vfs_reclaim_vnode resumes its scan */
extern int g_need_mnt_sync;
extern int g_vfsIoCount;		/* name unknown: I/O calls in progress */
extern int g_vfsSuspendQueryCount;	/* name unknown: suspend queries since the last resume */
extern int g_vfsInitialized;		/* name unknown: vfsInitialize ran */
extern struct FdLock g_vfsDummyFdLock;	/* name unknown: the fd lock of the dummy files */

/* mount.c */
int mnt_initialize(void);
struct MountPoint;
struct MountPoint *mnt_find_mount_point(int mntId);			/* name unknown */
SceVfsMountData *mnt_get_mount_data(int mntId);		/* name unknown */
int mnt_get_mount_opt(int mntId);			/* name unknown: 0 when not mounted */

/* daemon.c: flushes written devices */
int iof_daemon_main(SceSize args, void *argp);
void iofile_daemon_start(void);
int iofile_daemon_notify_write(int mntOpt);		/* name unknown */
void iofile_daemon_notify_mount(int mntId);

/*
 * ioerror.c: a "SceUIDIoErrorEventClass" event a process waits on for errors
 * a file system reports on one mount (names unknown)
 */
typedef struct IoErrorEvent {
	SceUIDEventObject base;
	int mntId;
	SceUID pid;				/* the creator, whose memory userInfo is */
	SceUInt32 pattern;			/* the event bits it waits for */
	int isSet;				/* set until SceIofilemgrForDriver_40B933C7 */
	IoErrorEventInfo *userInfo;		/* where the error is written */
	SceUInt32 reserved;			/* not yet traced */
} IoErrorEvent;
_Static_assert(sizeof(IoErrorEvent) == 0x50, "IoErrorEvent");

extern struct ThreadmgrWaitableClass g_ioErrorEventClass;	/* "SceUIDIoErrorEventClass" (name unknown) */

int ioerror_initialize(void);					/* name unknown */

/* vfs_buff.c, sched / misc still in frames.S */
int vfsBuffFree(struct BuffCache *bc, int flags, SceVfsVnode *vp, SceVfsFile *fdList);
int vfsBuffDataFlushAll(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file);
int vfsBuffIoFlush(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file);
int vfsBuffIoFetch(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file);
int vfsBuffIoFillZero(struct BuffCache *bc, SceOff pos, SceSize size, SceVfsVnode *vp,
		      SceSize fillSize);
int vfsBuffIoBuffSeek(struct BuffCache *bc, SceOff pos, int extend);
int vfsBuffIoOverWrite(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		       SceVfsVnode *vp);
int vfsBuffIoWrite(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		   SceVfsVnode *vp, SceSize grow);
int vfsBuffIoSet(struct BuffCache *bc, SceOff pos, SceSize size, const void *buf,
		 SceVfsVnode *vp);

/* vfs_vnops.c */
/* the hook is only set while a debugger traces the vops */
#define VFS_VOP_HOOK_CALL(...) do {					\
	if (__builtin_expect(g_vfsVopHook != NULL, 0))			\
		g_vfsVopHook(__VA_ARGS__);				\
} while (0)

/* the hook after the operation: Sony's compiler laid its call out in line */
#define VFS_VOP_HOOK_CALL_PLAIN(...) do {				\
	if (g_vfsVopHook != NULL)					\
		g_vfsVopHook(__VA_ARGS__);				\
} while (0)

#define VFS_VOP_HOOK_OPEN		0x02
#define VFS_VOP_HOOK_CLOSE		0x06
#define VFS_VOP_HOOK_READ		0x0a
#define VFS_VOP_HOOK_WRITE		0x0e
#define VFS_VOP_HOOK_PREAD		0x12
#define VFS_VOP_HOOK_PWRITE		0x16
#define VFS_VOP_HOOK_LSEEK		0x1a
#define VFS_VOP_HOOK_MKDIR		0x1e
#define VFS_VOP_HOOK_RMDIR		0x22
#define VFS_VOP_HOOK_GETSTAT		0x26
#define VFS_VOP_HOOK_REMOVE		0x2a
#define VFS_VOP_HOOK_RENAME		0x2e
#define VFS_VOP_HOOK_DOPEN		0x34
#define VFS_VOP_HOOK_DREAD		0x38
#define VFS_VOP_HOOK_DCLOSE		0x3c
#define VFS_VOP_HOOK_CHSTAT		0x40
#define VFS_VOP_HOOK_IOCTL		0x44
#define VFS_VOP_HOOK_SYNC		0x48
#define VFS_VOP_HOOK_FGETSTAT		0x4c
#define VFS_VOP_HOOK_FCHSTAT		0x50
#define VFS_VOP_HOOK_LOOKUP		0x52
#define VFS_VOP_HOOK_CREATE		0x54
#define VFS_VOP_HOOK_INACTIVE		0x56
#define VFS_VOP_HOOK_WHITEOUT		0x58
#define VFS_VOP_HOOK_CLEANUP		0x5b
#define VFS_VFSOP_HOOK_MOUNT		0x5d
#define VFS_VFSOP_HOOK_UMOUNT		0x5f
#define VFS_VFSOP_HOOK_DEVCTL		0x61
#define VFS_VFSOP_HOOK_SYNC		0x63
#define VFS_VFSOP_HOOK_DECODE_PATH_ELEM	0x65

/* the buffer cache sceVopOpen gives a vnode */
#define VFS_BUFF_DEFAULT_DATA_CACHE_SIZE	0x2000
#define VFS_BUFF_DEFAULT_WAY_NUM		2
#define VFS_BUFF_DEFAULT_LINE_SIZE		0x200

/* vfs_buf_allocate_memory's memory types (names unknown) */
#define VFS_BUFF_MEM_HEAP		1
#define VFS_BUFF_MEM_DEFAULT		2
#define VFS_BUFF_MEM_CDIALOG		3
#define VFS_BUFF_MEM_HOST		4

/* an open flag that keeps a file without buffer cache (name unknown) */
#define VFS_O_NO_BUFFCACHE		0x10000000

/* the memory a vnode's buffer cache comes from */
static inline __attribute__((always_inline)) int vfs_buff_mem_type(SceVfsVnode *vp)
{
	int type;

	type = vfs_is_cdialog_thread() != 0 ? VFS_BUFF_MEM_CDIALOG : VFS_BUFF_MEM_DEFAULT;
	if (vp->core.mnt->fsType == SCE_VFS_FS_TYPE_HOSTFS)
		type = VFS_BUFF_MEM_HOST;
	return type;
}

void *vfs_buf_allocate_memory(SceUID id, SceSize size, int memType);
int vfs_buf_free_memory(SceUID id, void *ptr, int memType);
int vfsBuffDataRead(struct BuffCache *bc, SceVfsVnode *vp, SceOff pos, SceSize size, void *buf);
int vfsBuffDataWrite(struct BuffCache *bc, SceVfsVnode *vp, SceOff pos, SceSize size,
		     const void *buf);
int vfs_buf_in_one_line(struct BuffCache *bc, SceOff pos, SceSize size);
int vfs_buf_io_cached_contains(struct BuffCache *bc, SceOff pos, SceSize size);
int vfs_buf_io_cached_overlaps(struct BuffCache *bc, SceOff pos, SceSize size);
int vfs_buf_io_buff_contains(struct BuffCache *bc, SceOff pos, SceSize size);

struct VfsBuffIoOps;
struct BuffLine;
int vfsBuffAllocate(SceSize dataCacheSize, SceUInt32 wayNum, SceSize lineSize,
		    SceSize ioCacheSize, SceUID pid, SceUInt16 memType,
		    const struct VfsBuffIoOps *ioOps, struct BuffCache **bcp);
int vfs_bop_flush_data_cache(struct BuffCache *bc, struct BuffLine *line, SceVfsVnode *vp,
			     SceVfsFile *file);

/* vfs_buf_op_subr's requests on the line that holds an offset (names unknown) */
#define VFS_BUFF_OP_LOOKUP		1	/* 1 if the line is cached */
#define VFS_BUFF_OP_FLUSH		2	/* write the line back if dirty */
#define VFS_BUFF_OP_HOLD		3
#define VFS_BUFF_OP_RELEASE		4
#define VFS_BUFF_OP_IS_HELD		5
#define VFS_BUFF_OP_INVALIDATE		6

int vfs_buf_op_subr(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file,
		    int mode);
int vfs_buf_data_cached(struct BuffCache *bc, SceOff pos);
int vfsBuffDataFlush(struct BuffCache *bc, SceOff pos, SceSize size, SceVfsVnode *vp,
		     SceVfsFile *file);
int vfs_buf_area_op_subr(struct BuffCache *bc, SceOff start, SceOff end, SceVfsVnode *vp,
			 SceVfsFile *file, int mode);
int vfs_buf_area_cached(struct BuffCache *bc, SceOff start, SceOff end);
int vfsBuffDataFlushArea(struct BuffCache *bc, SceOff start, SceOff end, SceSize size,
			 SceVfsVnode *vp, SceVfsFile *file);
int vfs_buf_area_invalidate(struct BuffCache *bc, SceOff start, SceOff end, SceVfsVnode *vp);
int vfsBuffDataEnter(struct BuffCache *bc, SceOff pos, SceSize size, const void *data, int state,
		     SceVfsVnode *vp, SceVfsFile *file);
int vfs_buf_free_data_buff(struct BuffCache *bc, SceVfsVnode *vp, SceVfsFile *file);
int vfs_buf_data_invalidate_all(struct BuffCache *bc, SceVfsVnode *vp);
int vfs_buf_io_read(struct BuffCache *bc, SceOff pos, SceSize size, void *buf);
void vfsBuffIoUpdateCachedSize(struct BuffCache *bc, SceSize size);
int vfsBuffIo2DataBuff(struct BuffCache *bc, SceOff pos, SceVfsVnode *vp, SceVfsFile *file);
int vfs_buf_io_buff_reaches(struct BuffCache *bc, SceOff pos, SceSize size);
int buff_cache_write(SceOff pos, const void *data, SceSize nbyte, SceVfsVnode *vp,
		     SceVfsFile *file, int pwrite, SceSize *wsz);
int buff_cache_read(SceOff pos, void *data, SceSize nbyte, SceVfsVnode *vp, SceVfsFile *file,
		    int pread, SceSize *rsz);
int uncached_data_read(struct BuffCache *bc, SceOff pos, SceSize nbyte, void *data,
		       SceVfsVnode *vp, SceVfsFile *file, int pread, SceSize *rsz);
/* a read error data_read_subr does not report (name unknown) */
#define VFS_ERROR_READ_QUIET		0x80142301
int buff_cache_fill_zero(struct BuffCache *bc, SceOff start, SceVfsVnode *vp, SceVfsFile *file);
int uncached_data_write(struct BuffCache *bc, SceOff start, SceSize nbyte, const void *data,
			SceVfsVnode *vp, SceVfsFile *file, int pwrite, SceSize *wsz);
int vfs_reclaim_vnode(SceVfsMount *mnt, SceVfsVnode **vpp);
void vfs_clear_sync_pending(void);
int vfsCleanup(SceUID fd);
int sceVfsOpMount(SceVfsMount *mnt, SceVfsPath *devPath);
int sceVfsOpUmount(SceVfsMount *mnt, int flags);
int sceVfsOpInit(SceVfsInfo *vfsInf);
int sceVfsOpSetRoot(SceVfsMount *mnt, int unused, SceVfsVnode *vp);
int sceVfsOpFini(SceVfsInfo *vfsInf);
int sceVfsOpSync(SceVfsMount *mnt, int flags);
void vfs_dump_fast_mutex(SceKernelFastMutex *mtx);
int bufferPoolInitialize(BufferPool *pool, SceUInt32 num, void *base, SceSize size,
			 BufferPoolNode *nodes);
void *bufferPoolAlloc(BufferPool *pool);
int bufferPoolFree(BufferPool *pool, void *buf);

/*
 * rfs_table.c: the tree of device roots ("/" and the "xxx0:" names under it), a fixed
 * table of RFS_TABLE_ENTRY_NUM entries; entry 0 is "/".
 */
#define RFS_TABLE_ENTRY_NUM		0x60
#define RFS_ENTRY_NAME_MAX		0x20

/* RfsEntry.flags (names unknown) */
#define RFS_ENTRY_VALID			0x01
#define RFS_ENTRY_PLACEHOLDER		0x02	/* entered by a lookup that did not create it */
#define RFS_ENTRY_TOP			0x10	/* the "/" entry */
#define RFS_ENTRY_ROOT			0x20	/* a device root: vp is set */
#define RFS_ENTRY_CREATED		0x40

/* rfs_table_lookup_entry flags (names unknown) */
#define RFS_LOOKUP_ENTER		0x100	/* enter a missing element as a placeholder */
#define RFS_LOOKUP_CREATE		0x200	/* enter a missing element */
#define RFS_LOOKUP_STOP_AT_ROOT		0x400	/* stop at the first device root */

typedef struct RfsEntry {
	char name[RFS_ENTRY_NAME_MAX];
	SceVfsVnode *vp;		/* RFS_ENTRY_ROOT: the device's root vnode */
	int attr;			/* rfs_add_entry's attr */
	SceUInt32 flags;		/* RFS_ENTRY_* */
	struct RfsEntry *child;
	struct RfsEntry *next;
	struct RfsEntry *parent;
} RfsEntry;
_Static_assert(sizeof(RfsEntry) == 0x38, "RfsEntry");

/* what rfs_get_root returns (name unknown) */
typedef struct RfsRootInfo {
	SceVfsVnode *vp;
	int attr;
} RfsRootInfo;

extern RfsEntry g_rfsTable[RFS_TABLE_ENTRY_NUM];
extern SceKernelFastMutex g_rfsLock;

int rfs_table_lookup_entry(const VfsPathElem *elem, int num, SceUInt32 flags, RfsEntry **entp,
			   int *depth);
void rfs_table_initialize(void);
int rfs_add_entry(const VfsPathElem *elem, int num, SceVfsVnode *vp, int attr);
SceVfsVnode *rfs_remove_entry(const VfsPathElem *elem, int num);
void rfs_lock(void);
int rfs_trylock(void);
void rfs_unlock(void);
int rfs_get_root(const VfsPathElem *elem, int num, RfsRootInfo *root, int *depth);

/* vfs_acl.c */
/*
 * vfs_acl.c: set_attr_info selects an ACMgr value type and byte size from
 * the mount's fs_type, preserving the original attribute storage.
 */
#define VFS_AC_FS_KIND_FS		3
#define VFS_AC_FS_KIND_2		4
#define VFS_AC_FS_KIND_PFS		6
#define VFS_AC_FS_KIND_BLKDEV		8

/* the attribute that asks for the mount point's attributes ("SCE_ELONG") */
#define VFS_AC_ATTR_LONG		0xf
#define VFS_MOUNT_TYPE_LOOP		5	/* SceVfsMount.mntFlags & 0xff (name unknown) */

/* SceSblACMgrForKernel_75AAF981's mode */
#define VFS_AC_MODE_READ		1
#define VFS_AC_MODE_WRITE		2
#define VFS_AC_MODE_CHSTAT		8

void set_attr_info(SceVfsVnode *vp, SceUInt64 *attr, SceSblACMgrValue *info, SceUID pid);
int vfs_check_vnode_rw_access(SceVfsVnode *vp);
int vfs_check_vnode_write_access(SceVfsVnode *vp);
int vfs_check_vnode_read_access(SceVfsVnode *vp);
int vfs_check_vnode_chstat_access(SceVfsVnode *vp);

/* vfs_vnops.c */
void vfs_dump_vnode(SceVfsVnode *vp, int unused);

/* iofilemgr_for_user.c: a path's length and the characters after its device name */
int vfs_check_path_length(const char *path, SceSize *len, SceSize *nameLen, SceSize max);

extern struct Ncache *g_negativeNcacheList;
extern int g_negativeNcacheNum;
extern SceUID g_vfsNcacheHeap;		/* name unknown: the name cache and path buffers */
extern const char *g_vfsHostfsAssignName;	/* name unknown */

/* still in frames.S */
void sceIoAsyncProcessFinalize(SceUID pid);			/* name unknown */
int ioerror_process_finalize(SceUID pid);
int mount_event_process_finalize(SceUID pid);
int mnt_start(void);
int sceIoSchedDispatcherInitializeSchedDispatcher(SchedDispatcher *dispatcher);
int vfsInitialize(void);
int sceIoAsyncInitialize(void);				/* name unknown */
/* the system events vfs_system_event_handler handles (names unknown) */
#define VFS_SYSEVENT_SUSPEND_PREPARE	0x400
#define VFS_SYSEVENT_SUSPEND		0x401
#define VFS_SYSEVENT_SUSPEND_QUERY	0x402
#define VFS_SYSEVENT_RESUME		0x10000
#define VFS_SYSEVENT_RESUME_DONE	0x400000

/* the argument block of vfs_get_vnode_mnt_opt (name unknown) */
typedef struct VfsPathParam {
	const char *path;
} VfsPathParam;

int vfs_get_vnode_mnt_opt(VfsPathParam *param);
int vfs_check_and_sync_on_host(SceVfsVnode *vp, SceVfsPath *path);
int sceVfsACCheckOpen(SceVfsVnode *vp, int flags);

/* vfsOpen: the mode bits a create accepts, and those it rejects (names unknown) */
#define VFS_OPEN_MODE_VALID		0xf101ff
#define VFS_OPEN_MODE_INVALID		0xff0efe00
/* vfsOpen flag (name unknown): a system or root program opens the file as not internal */
#define VFS_O_EXTERNAL_FILE		0x10000

/* a lookup to retry: an entry on the path is being changed (name unknown) */
#define VFS_LOOKUP_AGAIN		(-2)

/* vfsRename on host0: the devctl that resolves a new name next to the old path (names unknown) */
#define VFS_DEVCTL_HOST_RENAME_PATH	0x5400

typedef struct VfsHostRenameArg {
	const char *oldPath;
	SceSize oldPathLength;
	const char *newName;
	SceSize newNameLength;
} VfsHostRenameArg;

/* vfsGetMntList's entries (name unknown) */
typedef struct VfsMountListEntry {
	SceVfsMount *mnt;
	int mntId;			/* SceVfsMountData.mntId, -1 for PFS */
	char assignName[0x20];
	SceUInt32 openedEntryNum;
	int isPfs;
	void *data;			/* SceVfsMount.data */
} VfsMountListEntry;
_Static_assert(sizeof(VfsMountListEntry) == 0x34, "VfsMountListEntry");

/* vfsGetMntInfo's result (name unknown); the fields copy SceVfsMount and its parts */
typedef struct VfsMountInfo {
	SceVfsMount *mnt;
	int mntId;
	char assignName[0x20];
	SceUInt32 openedEntryNum;
	SceUInt32 state;
	SceUInt32 mntFlags;
	SceUInt32 fsType;		/* fs_type and opt */
	SceVfsMount *self;
	SceVfsVnode *mntVnode;
	SceUID allocator;
	SceUInt32 mntRefCount;
	SceUInt32 vnodeNum;
	SceVfsVnode *vnodeList;
	SceVfsMount *mntedOnList;
	SceVfsMount *mntedOnListPrev;
	SceUInt32 defaultIoCacheSize;
	void *data;
	struct FdLock *fdLock;
	SceUID fdLockMutex;
	SceUID fdLockCond;
	SceUInt32 fdLockState;
	SceUInt32 fdLockWaitNum;
	struct Fumount *fumount;
	SceUInt32 fumountBusy;
	SceUInt32 devState;
	SceUID activationEvf;
	SceVfsMountData **mntDataPtr;
	char path[0x400];
	SceVfsMountData *mntData;
	char fsName[0x40];
	char blockdevName[0x40];
	char blockdevNameNoPart[0x40];
} VfsMountInfo;
_Static_assert(__builtin_offsetof(VfsMountInfo, blockdevNameNoPart) == 0x50c, "VfsMountInfo");

int vfs_get_mount_list(VfsMountListEntry *list, int max, int *num);
int vfs_get_mount_info(SceVfsMount *target, VfsMountInfo *info);
int vfs_check_fd_vnode(const char *path, SceUID fd);
const char *mount_get_path_by_id(int mntId);	/* mount.c (name unknown) */

void vfs_set_sync_pending(void);
void vfs_increment_io_count(void);
void vfs_decrement_io_count(void);
void vfs_clear_external_dev_state(void);
int vfsGetMntId(const char *path);
int vfsClose(IofilemgrCloseParam *param);
int vfsOpen(IofilemgrOpenParam *param);
int vfsRead(IofilemgrReadParam *param);
int vfsDopen(IofilemgrDopenParam *param);	/* vfs.c: dopen */
int vfsDread(IofilemgrDreadParam *param);			/* vfs.c: dread */
int vfsDclose(IofilemgrDcloseParam *param);
int vfsRemove(IofilemgrRemoveParam *param);
int vfsMkdir(IofilemgrMkdirParam *param);
int vfsRmdir(IofilemgrRmdirParam *param);
int vfsGetstat(IofilemgrGetstatParam *param);	/* vfs.c: getstat */
int vfsRename(IofilemgrRenameParam *param);
int vfsDevctl(IofilemgrDevctlParam *param);	/* vfs.c: devctl */
int vfsSync(IofilemgrSyncParam *param);
int vfsGetFdInfoForDebugger(SceUID fd, SceIofileInfo *info);
int vfsGetstatByFd(IofilemgrGetstatByFdParam *param);	/* vfs.c: getstat by fd */
int vfsSyncByFd(IofilemgrSyncByFdParam *param);		/* vfs.c: sync by fd */
int vfsFlock(IofilemgrFlockParam *param);

/*
 * A vnode's buffer cache (vfs_buff.c): an I/O buffer and a set-associative
 * data cache of setNum sets with wayNum lines of lineSize bytes each.
 * Sony's strings name ioBuffSize, ioBuffOffset, ioBuffCachedSize and lineSize.
 */
/* a buffer cache's own writer for dirty lines (names unknown; every caller passes NULL) */
typedef struct VfsBuffIoWriteArgs {
	SceOff offset;
	const void *data;
	SceSize size;
	SceVfsVnode *vp;
} VfsBuffIoWriteArgs;

struct VfsBuffIoOps {
	int (*write)(VfsBuffIoWriteArgs *args);
};

struct BuffLine {
	void *data;			/* lineSize bytes */
	int state;			/* VFS_BUFF_LINE_* */
	SceOff offset;			/* file offset of the line, lineSize aligned */
	struct BuffLine *next;		/* the set's list, most recently used first */
};

/* BuffLine states (names unknown) */
#define VFS_BUFF_LINE_FLUSHED		0x1	/* written back at least once */
#define VFS_BUFF_LINE_DIRTY		0x2
#define VFS_BUFF_LINE_HELD		0x4	/* set and cleared by request */
#define VFS_BUFF_LINE_INVALID		0x8	/* above: not usable */
#define VFS_BUFF_LINE_FREE		0x10

struct BuffCache {
	void *ioBuff;			/* the I/O buffer, aligned to 0x200 */
	SceSize ioBuffSize;
	SceSize ioBuffCachedSize;
	int ioBuffState;			/* VFS_BUFF_LINE_* */
	SceOff ioBuffOffset;
	SceUInt8 wayNum;		/* lines per set */
	SceUInt8 setNum;
	SceUInt16 memType;		/* vfs_buf_allocate_memory's type */
	SceUID pid;
	SceSize lineSize;
	void *dataBuffMemory;		/* the data cache's allocation */
	void *ioBuffMemory;		/* the I/O buffer's allocation */
	const struct VfsBuffIoOps *ioOps;	/* name unknown; NULL: the vnode's vop_write */
	SceUID hostMemBlock;		/* SceIoBuffCacheForHost */
	void *hostBuff;
	struct BuffLine *lineList[];	/* one list per set */
};
_Static_assert(__builtin_offsetof(struct BuffCache, lineList) == 0x38, "BuffCache");

int vfsBuffCacheCtl(SceVfsVnode *vp, SceSize dataCacheSize, SceUInt32 wayNum, SceSize lineSize,
		    SceSize ioCacheSize, int mode, int a6);

/* vfsBuffCtl's argument block (filled by vfsIoctl) */
typedef struct IofilemgrBuffCtlParam {
	SceUID fd;
	SceSize dataCacheSize;
	SceSize lineSize;
	SceUInt32 wayNum;
	SceSize ioCacheSize;
	SceUInt32 mask;			/* VFS_BUFFCTL_SET_*: which sizes are given */
	int mode;
	void *outdata;
	SceSize outlen;
	unsigned int cmd;
} IofilemgrBuffCtlParam;

/* the ioctl result of VFS_IOCTL_BUFFCTL_GET */
typedef struct VfsBuffCacheInfo {
	SceSize dataCacheSize;
	SceSize lineSize;
	SceUInt32 wayNum;
	SceSize ioCacheSize;
	SceUInt32 reserved[4];
} VfsBuffCacheInfo;

#define VFS_BUFFCTL_SET_DATA_CACHE_SIZE	1
#define VFS_BUFFCTL_SET_LINE_SIZE	2
#define VFS_BUFFCTL_SET_WAY_NUM		4
#define VFS_BUFFCTL_SET_IO_CACHE_SIZE	8

/* vfsIoctl's own commands (names unknown) */
#define VFS_IOCTL_SET_IO_SCHED_DATA	0x200
#define VFS_IOCTL_GET_IO_SCHED_DATA	0x201
#define VFS_IOCTL_BUFFCTL_SET		0x1001
#define VFS_IOCTL_BUFFCTL_GET		0x1002
#define VFS_IOCTL_BUFFCTL_MAX		0x17ff
#define VFS_IOCTL_SET_INTERNAL		0x1801
#define VFS_IOCTL_CLEAR_INTERNAL	0x1802
#define VFS_IOCTL_IS_PFS			0x1803 /* name unknown */
#define VFS_IOCTL_VFS_MAX		0x1fff
#define VFS_IOCTL_MAX			0x7fff

int vfsBuffCtl(IofilemgrBuffCtlParam *param);
		/* flock */
SceOff vfsLseek(IofilemgrLseekParam *param);	/* vfs.c: lseek */

/* data still defined in frames.S */
extern SceUID g_iofilemgrKplsKey;
extern int g_iofilemgrTlsKey;			/* name unknown: a 0x14-byte TLS slot */
extern SceInt32 g_gameProgramCount;
extern SceInt32 g_iofilemgrPid[IOFILEMGR_PROCESS_MAX];
extern SceKernelFastMutex g_iofilemgrMutex;
extern int g_iofilemgrResourceLimitSet;
extern SceClass g_vfsFileClass;
extern SceUID g_ioVfsHeapUid;			/* name unknown: the "SceIofile" heap */
extern int g_ioSchedEnabled;
/* maps a process's path to the one it opens (sceIoSetPathMappingFunction) */
extern IoPathMappingFunction g_ioPathMappingFunction;
/*
 * name unknown: each holds the address of the Fios2 import its calls make, tested
 * against NULL before the call (the Fios2 overlay paths of these calls)
 */
extern void *g_ioFiosOpenEnabled;
extern void *g_ioFiosDopenEnabled;
extern void *g_ioFiosDcloseEnabled;
extern void *g_ioFiosPathEnabled;	/* the path calls of SceIofilemgr (user) */
extern SchedDispatcher g_schedDispatcher;
extern SceUID g_ioSchedHeap;		/* "SceIoScheduler" (name unknown) */
extern struct ThreadmgrWaitableClass g_ioAsyncEventClass;	/* "SceUIDIoAsyncEventClass" (name unknown) */
/*
 * A hook every vop wrapper calls (set by SceIofilemgrForDriver_F4F8C59A; name unknown):
 * (event, argc, args...) before the vop and (event + 1, 2, vp, result) after it.
 */
typedef void (*VfsVopHook)(int event, int argc, ...);
extern VfsVopHook g_vfsVopHook;

/* the user side of the calls (sceIo*ForUser), called by the syscalls with kernel copies */
SceUID sceIoOpenForUser(const char *file, int flags, SceMode mode);
SceOff sceIoLseekForUser(SceUID fd, SceOff offset, int whence);
int sceIoRemoveForUser(const char *file);
int sceIoRenameForUser(const char *oldname, const char *newname);
int sceIoMkdirForUser(const char *dir, SceMode mode);
int sceIoRmdirForUser(const char *path);
SceUID sceIoDopenForUser(const char *dirname);
int sceIoDreadForUser(SceUID fd, SceIoDirent *dir);
int sceIoGetstatForUser(const char *file, SceIoStat *stat);
int sceIoChstatForUser(const char *file, SceIoStat *stat, int bits);
int sceIoSyncForUser(const char *device, unsigned int flag);
int sceIoDevctlForUser(const char *dev, unsigned int cmd, const void *indata, SceSize inlen, void *outdata, SceSize outlen);
int sceIoIoctlForUser(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen, void *outdata, SceSize outlen);
int sceIoGetstatByFdForUser(SceUID fd, SceIoStat *stat);
int sceIoChstatByFdForUser(SceUID fd, SceIoStat *stat, int bits);
SceUID sceIoOpenAsyncForUser(const char *file, int flags, SceMode mode, void *asyncParam);
SceUID sceIoLseekAsyncForUser(SceUID fd, SceOff offset, int whence, void *asyncParam);
SceUID sceIoIoctlAsyncForUser(SceUID fd, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen, void *asyncParam);
SceUID sceIoRemoveAsyncForUser(const char *file, void *asyncParam);
SceUID sceIoMkdirAsyncForUser(const char *dir, SceMode mode, void *asyncParam);
SceUID sceIoRmdirAsyncForUser(const char *path, void *asyncParam);
SceUID sceIoRenameAsyncForUser(const char *oldname, const char *newname, void *asyncParam);
SceUID sceIoChstatAsyncForUser(const char *file, SceIoStat *stat, int bits, void *asyncParam);
SceUID sceIoGetstatAsyncForUser(const char *file, SceIoStat *stat, void *asyncParam);
SceUID sceIoDevctlAsyncForUser(const char *dev, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen, void *asyncParam);
SceUID sceIoSyncAsyncForUser(const char *device, unsigned int flag, void *asyncParam);
int sceIoCompleteMultipleForUser(void *a0, int a1, int *result);
SceSSize sceIoPreadForUser(SceUID fd, void *data, SceSize size, SceOff offset);
SceSSize sceIoPwriteForUser(SceUID fd, const void *data, SceSize size, SceOff offset);
SceUID sceIoPreadAsyncForUser(SceUID fd, void *data, SceSize size, SceOff offset, void *asyncParam);
SceUID sceIoPwriteAsyncForUser(SceUID fd, const void *data, SceSize size, SceOff offset, void *asyncParam);

/* SceSblACMgr */

/*
 * SceFios2Kernel. iofilemgr keeps a directory handle as an int (its dummy files carry it),
 * so it declares the calls it makes itself rather than take fios2/fios2.h's
 * SceFiosKernelDH pointers.
 */
/* vitasdk: ksceFiosKernelOverlayResolveSync */
int SceFios2KernelForDriver_0F456345(SceUID pid, int resolveFlag, const char *inPath, char *outPath,
				     SceSize maxPath);

/* the overlay's directory calls (names unknown) */
int SceFios2KernelForDriver_3B329E86(int *handle, const char *dirname, int a2, int a3);
int SceFios2KernelForDriver_28E28A58(int handle, SceIoDirent *dir);
int SceFios2KernelForDriver_DD7627EC(int handle);

/* the Fios2 handle behind a dummy file (vfs.c) */

int SceFios2KernelForDriver_2649408B(int handle, SceIoStat *stat);
int SceFios2KernelForDriver_35E7E75C(int handle, const SceIoStat *stat, int bits);
int SceFios2KernelForDriver_990F46A5(int handle, unsigned int flag);

int sceIoSetProcessDefaultPriorityInternal(SceInt32 priority, int fromKernel);
SceInt32 sceIoGetProcessDefaultPriorityInternal(int fromKernel);
int sceIoSetThreadDefaultPriorityInternal(SceInt32 priority, int fromKernel);
SceInt32 sceIoGetThreadDefaultPriorityInternal(int fromKernel);
int sceIoSetPriorityInternal(SceUID fd, SceInt32 priority, int fromKernel);
SceInt32 sceIoGetPriorityInternal(SceUID fd, int fromKernel);

/* iofilemgr.c: the per-process fd counts */
int check_limit_and_increment_user_fd_count(SceUID pid, SceUID thid);
void decrement_user_fd_count(SceUID pid, SceUID thid, int keepPeak);
int check_limit_user_fd_count(SceUID pid, SceUID thid);

/* dummy_stdio.c: the tty device; a Fios2 error (facility 0x082) as an errno error */
int DummyTtyStart(void);
int fiosErrorToErrno(int error);			/* name unknown */
int convertFiosError(int error);

/* io_scheduler.c: the calling thread's I/O priority (IofilemgrTls.priority) */
int setThreadIoPriority(SceInt32 priority);
SceInt32 swapThreadIoPriority(SceInt32 priority);
SceInt32 getThreadIoPriority(void);

/* io_scheduler.c: the calls the I/O scheduler runs */
int sceIoSchedulerInitialize(void);
int getPathMntOpt(const char *path, int full);
int sceIoSchedulerRemove(const char *file, int mntOpt);
int sceIoSchedulerMkdir(const char *dir, SceMode mode, int mntOpt);
int sceIoSchedulerRmdir(const char *path, int mntOpt);
int sceIoSchedulerRename(const char *oldname, const char *newname, int mntOpt);
int sceIoSchedulerDevctl(const char *dev, unsigned int cmd, const void *indata, SceSize inlen,
			 void *outdata, SceSize outlen, int mntOpt);
int sceIoSchedulerSync(const char *device, unsigned int flag, int mntOpt);
SceUID sceIoSchedulerOpen(const char *path, int flags, SceMode mode, int mntOpt);
int sceIoSchedulerClose(SceUID fd, const IofilemgrSchedInfo *info);
int sceIoSchedulerIoctl(SceUID fd, unsigned int cmd, const void *indata, SceSize inlen,
			void *outdata, SceSize outlen, const IofilemgrSchedInfo *info);
SceOff sceIoSchedulerLseek(SceUID fd, SceOff offset, int whence, const IofilemgrSchedInfo *info);
int sceIoSchedulerLseek32(SceUID fd, int offset, int whence, const IofilemgrSchedInfo *info);
int sceIoSchedulerRead(SceUID fd, void *data, SceSize size, const IofilemgrSchedInfo *info);
int sceIoSchedulerWrite(SceUID fd, const void *data, SceSize size,
			const IofilemgrSchedInfo *info);
int sceIoSchedulerPread(SceUID fd, void *data, SceSize size, SceOff offset,
			const IofilemgrSchedInfo *info);
int sceIoSchedulerPwrite(SceUID fd, const void *data, SceSize size, SceOff offset,
			 const IofilemgrSchedInfo *info);
int sceIoSchedulerSyncByFd(SceUID fd, int flag, const IofilemgrSchedInfo *info);
SceUID sceIoSchedulerDopen(const char *dirname, int mntOpt);
int sceIoSchedulerDclose(SceUID fd, const IofilemgrSchedInfo *info);
int sceIoSchedulerDread(SceUID fd, SceIoDirent *dir, const IofilemgrSchedInfo *info);
int sceIoSchedulerDreadForVsh(SceUID fd, SceIoDirent *dir, const IofilemgrSchedInfo *info);
int sceIoSchedulerChstat(const char *file, SceIoStat *stat, int bits, int mntOpt);
int sceIoSchedulerGetstat(const char *file, SceIoStat *stat, int mntOpt);
int sceIoSchedulerGetstatByFd(SceUID fd, SceIoStat *stat, const IofilemgrSchedInfo *info);
int sceIoSchedulerChstatByFd(SceUID fd, SceIoStat *stat, int bits,
			     const IofilemgrSchedInfo *info);
int sceIoSchedulerChstatForVsh(const char *file, SceIoStat *stat, int bits, int mntOpt);
int sceIoSchedulerGetstatForVsh(const char *file, SceIoStat *stat, int mntOpt);
/* a file's scheduling parameters (vfsIoctl VFS_IOCTL_SET/GET_IO_SCHED_DATA) */
int setFileSchedParam(SceUID fd, const IofilemgrSchedInfo *info);
int getFileSchedParam(SceUID fd, IofilemgrSchedInfo *info, int flag);

/* sched_dispatcher.c */
IoScheduler *sceIoSchedDispatcherGetIoScheduler(SchedDispatcher *dispatcher, int mntOpt);
#if FW_VERSION >= 0x04000000
IoScheduler *sceIoSchedDispatcherGetIoSchedulerByEvent(SchedDispatcher *dispatcher,
						      SceUInt32 bits);
#endif
IoScheduler *sceIoSchedDispatcherGetIoScheduler(SchedDispatcher *dispatcher,
						 int mntOpt);	/* name unknown */
int sceIoSchedDispatcherDispatchIoRequest(SchedDispatcher *dispatcher, IoRequest *req);
int sceIoSchedDispatcherRemoveIoRequest(SchedDispatcher *dispatcher, IoRequest *req);
int sceIoSchedDispatcherFreeAllAsyncEvents(SchedDispatcher *dispatcher, SceUID pid);
int sceIoSchedDispatcherFsResourceAvailable(SchedDispatcher *dispatcher, SceUInt32 weight);
int sceIoSchedDispatcherFsResourceUnavailable(SchedDispatcher *dispatcher, SceUInt32 weight);
int sceIoSchedDispatcherIssueAnotherIoRequest(SchedDispatcher *dispatcher, SceInt32 weight);
void sceIoSchedDispatcherWorker(SchedDispatcher *dispatcher, IoScheduler *sched);

/* sched_scheduler.c (still in frames.S) */
IoScheduler *sceIoSchedulerCreateIoScheduler(SchedDispatcher *dispatcher,
					     const IoSchedulerAttr *attr);
int sceIoSchedulerExecuteSyncIoRequest(IoScheduler *sched, IoRequest *req);
void sceIoSchedulerAddAsyncIoRequest(IoScheduler *sched, IoRequest *req);
int sceIoSchedulerCancelAsyncIoRequest(IoScheduler *sched, IoRequest *req);
int sceIoSchedulerRemoveIoRequest(IoScheduler *sched, IoRequest *req);	/* name unknown */
int sceIoSchedulerIssueAnotherIoRequestInternal(IoScheduler *sched);
int sceIoSchedulerExecuteIoRequestWorkerForDispatcher(IoScheduler *sched);

/* io_scheduler_async.c */
SceUID sceIoAsyncEventCreate(IoAsyncEvent **event);
int sceIoAsyncEventDestroy(SceUID uid);
char *sceIoAsyncAllocPath(const char *path);			/* name unknown */
int sceIoAsyncFreePath(const char *path);				/* name unknown */

/* a user buffer an asynchronous read or write could not map (name unknown; still in frames.S) */
int ioUserMapErrorToErrno(int err);

/* sched_io_chunk.c */
int sceIoChunkIsInPool(IoChunkPool *pool, IoChunk *chunk);	/* name unknown */
int sceIoChunkCreateIoChunkPool(IoChunk *chunks, int chunkNum, IoChunkPool *pool);
IoChunk *sceIoChunkCreateIoChunk(IoChunkPool *pool, IoRequest *req, SceUIntPtr data,
				SceSize offset, SceSize size);
void sceIoChunkDestroyIoChunk(IoChunkPool *pool, IoChunk *chunk);

/* sched_io_request.c */
#if FW_DEBUG
int sceIoRequestIsValid(IoRequest *req);				/* name unknown */
#endif
int sceIoRequestCreateIoRequest(IoRequest *req, int type, int isAsync, SceUID asyncEvent,
				IoAsyncParam *asyncParam, int mntOpt, SceInt32 priority, IoChunk *chunks,
				int chunkNum);
void sceIoRequestSetCacheMode(IoRequest *req, int isWrite, int isDataIo);	/* name unknown */
SceBool sceIoRequestIsFinished(IoRequest *req);				/* name unknown */
IoChunk *sceIoRequestPopIoChunk(IoRequest *req);
void sceIoRequestPushIoChunk(IoRequest *req, IoChunk *chunk);

/*
 * sched_io_request.c: a new request; the calls give the
 * device's mount option, the I/O priority, whether the request is asynchronous
 * and its event and option block, then their own arguments.
 */
#define IO_REQUEST_CREATE_ARGS	IoRequest *req, int mntOpt, SceInt32 priority, int isAsync, \
				SceUID asyncEvent, IoAsyncParam *asyncParam
int sceIoRequestCreateIoRequestForOpen(IO_REQUEST_CREATE_ARGS, const char *path, int flags,
				       SceMode mode);
int sceIoRequestCreateIoRequestForClose(IO_REQUEST_CREATE_ARGS, SceUID fd);
int sceIoRequestCreateIoRequestForRead(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
				       SceUID fd, void *data, SceSize size);
int sceIoRequestCreateIoRequestForWrite(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
					SceUID fd, const void *data, SceSize size);
int sceIoRequestCreateIoRequestForLseek(IO_REQUEST_CREATE_ARGS, SceUID fd, SceOff offset,
					int whence);
int sceIoRequestCreateIoRequestForLseek32(IO_REQUEST_CREATE_ARGS, SceUID fd, int offset,
					  int whence);
int sceIoRequestCreateIoRequestForRemove(IO_REQUEST_CREATE_ARGS, const char *file);
int sceIoRequestCreateIoRequestForRename(IO_REQUEST_CREATE_ARGS, const char *oldname,
					 const char *newname);
int sceIoRequestCreateIoRequestForMkdir(IO_REQUEST_CREATE_ARGS, const char *dir, SceMode mode);
int sceIoRequestCreateIoRequestForRmdir(IO_REQUEST_CREATE_ARGS, const char *path);
int sceIoRequestCreateIoRequestForDopen(IO_REQUEST_CREATE_ARGS, const char *dirname);
int sceIoRequestCreateIoRequestForDclose(IO_REQUEST_CREATE_ARGS, SceUID fd);
int sceIoRequestCreateIoRequestForDread(IO_REQUEST_CREATE_ARGS, SceUID fd, SceIoDirent *dir);
int sceIoRequestCreateIoRequestForDreadForVsh(IO_REQUEST_CREATE_ARGS, SceUID fd,
					      SceIoDirent *dir);
int sceIoRequestCreateIoRequestForGetstat(IO_REQUEST_CREATE_ARGS, const char *file,
					  SceIoStat *stat);
int sceIoRequestCreateIoRequestForGetstatByFd(IO_REQUEST_CREATE_ARGS, SceUID fd,
					      SceIoStat *stat);
int sceIoRequestCreateIoRequestForChstat(IO_REQUEST_CREATE_ARGS, const char *file,
					 SceIoStat *stat, int bits);
int sceIoRequestCreateIoRequestForChstatByFd(IO_REQUEST_CREATE_ARGS, SceUID fd,
					     SceIoStat *stat, int bits);
int sceIoRequestCreateIoRequestForGetstatForVsh(IO_REQUEST_CREATE_ARGS, const char *file,
						SceIoStat *stat);
int sceIoRequestCreateIoRequestForChstatForVsh(IO_REQUEST_CREATE_ARGS, const char *file,
					       SceIoStat *stat, int bits);
int sceIoRequestCreateIoRequestForSync(IO_REQUEST_CREATE_ARGS, const char *device,
				       unsigned int flag);
int sceIoRequestCreateIoRequestForSyncByFd(IO_REQUEST_CREATE_ARGS, SceUID fd, int flag);
int sceIoRequestCreateIoRequestForDevctl(IO_REQUEST_CREATE_ARGS, const char *dev,
					 unsigned int cmd, const void *indata, SceSize inlen,
					 void *outdata, SceSize outlen);
int sceIoRequestCreateIoRequestForIoctl(IO_REQUEST_CREATE_ARGS, SceUID fd, unsigned int cmd,
					const void *indata, SceSize inlen, void *outdata,
					SceSize outlen);
int sceIoRequestCreateIoRequestForPread(IO_REQUEST_CREATE_ARGS, IoChunk *chunks, int chunkNum,
					SceUID fd, void *data, SceSize size, SceOff offset);
int sceIoRequestCreateIoRequestForPwrite(IO_REQUEST_CREATE_ARGS, IoChunk *chunks,
					 int chunkNum, SceUID fd, const void *data, SceSize size,
					 SceOff offset);
int sceIoRequestCreateIoRequestForDopenAsync(IoRequest *req, SceInt32 priority,
					     SceUID asyncEvent, IoAsyncParam *asyncParam,
					     const char *dirname);		/* name unknown */
int sceIoRequestCreateIoRequestForDcloseAsync(IoRequest *req, SceInt32 priority,
					      SceUID asyncEvent, IoAsyncParam *asyncParam,
					      SceUID fd);			/* name unknown */
int sceIoRequestCreateIoRequestForDreadAsync(IoRequest *req, SceInt32 priority,
					     SceUID asyncEvent, IoAsyncParam *asyncParam, SceUID fd,
					     SceIoDirent *dir);		/* name unknown */

#endif

/* sched_io_request_queue.c */
/* name unknown: allocates one priority queue */
IoRequestQueue *sceIoRequestQueueCreateIoRequestQueue(IoScheduler *sched, int priority);
int sceIoRequestQueueAddIoRequest(IoRequestQueue *queue, IoRequest *req);
int sceIoRequestQueueRemoveIoRequest(IoRequestQueue *queue, IoRequest *req);
IoRequest *sceIoRequestQueueGetIoRequest(IoRequestQueue *queue, int *waiting);
int sceIoRequestQueueChunkIoRequest(IoScheduler *sched, IoRequest *req, int canChunk);
int sceIoRequestQueueFinishAndDestroyIoChunk(IoRequest *req, IoChunk *chunk, int result);
int sceIoRequestQueueFinishIoRequest(IoScheduler *sched, IoRequest *req);
int sceIoRequestQueueExecuteIoChunk(IoRequest *req, IoChunk *chunk);

/* sched_scheduler.c */
int sceIoSchedulerIsDataIo(IoRequest *req);
int sceIoSchedulerIsStorageIo(IoRequest *req);

#endif
