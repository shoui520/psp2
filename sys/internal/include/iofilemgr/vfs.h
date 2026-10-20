/*
 * SceIofilemgr's VFS interface (os0:kd/iofilemgr.skprx): the file system, mount, vnode and file
 * records, the VFS and vnode operation tables with their argument records, and the VFS exports.
 *
 * The layouts were read from the firmware (and match vitasdk's <psp2kern/vfs.h>). Field names
 * are Sony's where its messages give them: the record dumps ("============== vnode %p", "mount",
 * "vfsinf", "mnt misc", "file") and expressions such as "vp->linkTo->linkedNum",
 * "mnt->mntRefCount", "mnt->pData", "file->fpos"; the operation tables keep the BSD-style names
 * Sony's file systems use for their entries (dev_tty_vfs_mount, dev_tty_vop_lookup). The rest
 * are ours.
 */

#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/iofilemgr_dirent.h>
#include <threadmgr/fast_mutex_object.h>

typedef struct SceVfsMount SceVfsMount;
typedef struct SceVfsVnode SceVfsVnode;
typedef struct SceVfsFile SceVfsFile;
typedef struct SceVfsOpTable SceVfsOpTable;
typedef struct SceVopTable SceVopTable;

/* SceVfsInfo's type */
#define SCE_VFS_TYPE_FS			0x00000000
#define SCE_VFS_TYPE_DEVFS		0x00000010

/* a file system implementation, added with sceVfsAddVfs ("vfsinf") */
typedef struct SceVfsInfo {
	const SceVfsOpTable *vfsOps;
	const char *vfsName;
	SceSize vfsNameLen;		/* with the terminating NUL */
	SceUInt32 refCount;
	SceUInt32 type;			/* SCE_VFS_TYPE_* */
	const SceVopTable *defaultVops;	/* the vnode operations of its vnodes */
	void *vfsData;
	struct SceVfsInfo *next;
} SceVfsInfo;

_Static_assert(sizeof(SceVfsInfo) == 0x20, "SceVfsInfo");

/* a path element being looked up */
typedef struct SceVfsPath {
	const char *name;
	SceSize nameLength;
	const char *path;		/* the whole path the element is from */
	const char *elem;		/* what vfs_lookup gives sceVopLookup: the undecoded element */
} SceVfsPath;

_Static_assert(sizeof(SceVfsPath) == 0x10, "SceVfsPath");

/* a mount point's names ("mnt misc") */
typedef struct SceVfsMountData {
	const char *assignName;		/* "<mnt>0:" */
	const char *fsName;
	const char *blockdevName;
	const char *blockdevNameNoPart;	/* the block device without its partition */
	SceUInt32 mntId;
} SceVfsMountData;

_Static_assert(sizeof(SceVfsMountData) == 0x14, "SceVfsMountData");

/* SceVfsMount's fsType */
#define SCE_VFS_FS_TYPE_FS		0x01	/* exFAT */
#define SCE_VFS_FS_TYPE_PFS		0x03
#define SCE_VFS_FS_TYPE_HOSTFS		0x04	/* host0: */
#define SCE_VFS_FS_TYPE_BLKDEV		0x10	/* sdstor0:, md0: */
#define SCE_VFS_FS_TYPE_CHRDEV		0x20	/* tty0: */

/* SceVfsMount's mntFlags: the mount type in the low byte, then the flags */
#define SCE_VFS_MOUNT_TYPE_PFS		0x00000001
#define SCE_VFS_MOUNT_TYPE_FSROOT	0x00000002
#define SCE_VFS_MOUNT_TYPE_DEVFS	0x00000003
#define SCE_VFS_MOUNT_TYPE_STACKFS	0x00000005	/* a file system in a file */
#define SCE_VFS_MOUNT_TYPE_HOSTFS	0x00000006
#define SCE_VFS_MOUNT_FLAG_TYPE_MASK	0x000000FF
#define SCE_VFS_MOUNT_FLAG_RDONLY	0x00001000
#define SCE_VFS_MOUNT_FLAG_NOBUF	0x00002000	/* no buffer cache */
#define SCE_VFS_MOUNT_FLAG_NUMBERED	0x00004000
#define SCE_VFS_MOUNT_FLAG_REMOTE	0x00008000	/* no name cache entry for a failed lookup */
#define SCE_VFS_MOUNT_FLAG_INTERNAL	0x00010000
#define SCE_VFS_MOUNT_FLAG_EXTERNAL	0x00020000
#define SCE_VFS_MOUNT_FLAG_WRITE_CACHE	0x00040000	/* no buffer cache flush after a write */
#define SCE_VFS_MOUNT_FLAG_NO_RECLAIM	0x00100000	/* its vnodes are never reclaimed (devfs) */

/* a mount point ("mount") */
struct SceVfsMount {
	SceKernelFastMutex fastMutex;
	SceVfsVnode *mntVnode;
	SceUID allocator;
	SceUInt32 state;
	SceUInt8 fsType;		/* SCE_VFS_FS_TYPE_* */
	SceUInt16 opt;
	SceUInt32 mntFlags;		/* SCE_VFS_MOUNT_TYPE_* | SCE_VFS_MOUNT_FLAG_* */
	SceVfsVnode *vnodeList;
	SceUInt32 vnodeNum;
	SceVfsInfo *mntVfsInf;
	SceUInt32 mntRefCount;
	SceUInt32 openedEntryNum;
	SceUInt32 availableEntryNum;
	SceUID pid;
	SceVfsMount *mntedOnList;
	SceVfsMount *mntedOnListPrev;
	SceVfsMount *mntListNext;
	SceVfsMountData *mntData;
	char path[64];
	SceUInt32 defaultIoCacheSize;
	void *pData;			/* the file system's */
	struct FdLock *fdLock;
	struct Fumount *fumount;
	SceUInt32 fdLockBuffer[4];	/* where fdLock points */
	SceUInt32 fumountBuffer[4];	/* where fumount points */
	SceUInt8 padding[16];
};

_Static_assert(sizeof(SceVfsMount) == 0x100, "SceVfsMount");
_Static_assert(offsetof(SceVfsMount, mntVnode) == 0x40, "SceVfsMount mntVnode");
_Static_assert(offsetof(SceVfsMount, mntData) == 0x7C, "SceVfsMount mntData");
_Static_assert(offsetof(SceVfsMount, pData) == 0xC4, "SceVfsMount pData");

/* SceVfsVnode's type */
#define SCE_VNODE_TYPE_REG		0x00000001
#define SCE_VNODE_TYPE_DIR		0x00000002
#define SCE_VNODE_TYPE_CHR		0x00000010
#define SCE_VNODE_TYPE_ROOT		0x00001000
#define SCE_VNODE_TYPE_DEV		0x00002000
#define SCE_VNODE_TYPE_MOUNTED		0x00004000	/* a mount point's backing vnode */
#define SCE_VNODE_TYPE_CHRDEV		(SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)
#define SCE_VNODE_TYPE_ROOTDIR		(SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_DIR)
#define SCE_VNODE_TYPE_ROOTDIR_DEVFS	(0x10000 | 0x20)

/* SceVfsVnode's state */
#define SCE_VNODE_STATE_ACTIVE		0x00000001
#define SCE_VNODE_STATE_INACTIVE	0x00000100
#define SCE_VNODE_STATE_UNMOUNTED	0x00000200
#define SCE_VNODE_STATE_DELETED		0x00000400
#define SCE_VNODE_STATE_RECLAIMED	0x00002000
#define SCE_VNODE_STATE_NEW		0x00008000

/* a vnode's lock (lock_vdlock) */
struct VfsVdlock {
	SceUInt32 waiter;
	SceUID ownerId;
	SceUInt32 recursiveCount;
	SceUID waitId;
	SceUInt32 waitPattern;
};

/* the vnode's fields its allocation clears in one block */
struct VfsVnodeCore {
	SceVopTable *ops;
	SceUInt32 nodeInf;		/* the file system's */
	void *nodeData;			/* the file system's */
	SceVfsMount *mnt;
	SceVfsVnode *dd;		/* the parent */
	SceVfsVnode *next;
	SceUInt32 refCount;
	struct BuffCache *bc;
	SceUInt32 fid[2];
	struct Flock *flock;
	SceUID allocator;
	struct Ncache *ncache;
	SceUInt32 state;		/* SCE_VNODE_STATE_* */
	SceUInt32 type;			/* SCE_VNODE_TYPE_* */
	SceVopTable *vopTbl;
	SceOff size;
	SceUInt32 aclData[2];
	SceVfsFile *fdList;
	SceUInt32 fdNum;
	SceVfsVnode *linkTo;
	SceUInt32 linkedNum;
	SceUInt8 reserved[48];
	SceUInt32 mntOpt;
	SceUInt32 reserved2;
};

_Static_assert(sizeof(struct VfsVnodeCore) == 0x98, "VfsVnodeCore");

/* a vnode ("vnode") */
struct SceVfsVnode {
	struct VfsVdlock vdlock;
	SceUInt8 padding[44];
	struct VfsVnodeCore core;
	SceUInt8 padding2[40];
};

_Static_assert(sizeof(SceVfsVnode) == 0x100, "SceVfsVnode");
_Static_assert(offsetof(SceVfsVnode, core) == 0x40, "SceVfsVnode core");

/* SceVfsFile's state */
#define SCE_VFS_FILE_STATE_OPEN		0x00000001
#define SCE_VFS_FILE_STATE_CLOSED	0x00000200
#define SCE_VFS_FILE_STATE_UNK		0x00000800
#define SCE_VFS_FILE_STATE_DUMMY	0x00010000

/* an open file ("file") */
struct SceVfsFile {
	SceUInt32 is_dread;		/* opened as a directory */
	SceUInt32 openMode;
	SceOff fpos;
	SceUInt32 state;		/* SCE_VFS_FILE_STATE_* */
	SceUID pid;
	SceVfsVnode *vnode;
	SceVfsFile *next;
	SceUInt32 fd;			/* the file system's */
	SceUInt16 flockBusyCount;
	SceUInt8 isLocked;
	SceUInt8 hasFlockEnt;
	struct FdLock *fdLock;
	SceUInt32 idata;		/* vfs_set_fd_idata */
	struct DebugPath *debugPath;
	SceUInt32 ioSchedData[3];
};

_Static_assert(sizeof(SceVfsFile) == 0x40, "SceVfsFile");
_Static_assert(offsetof(SceVfsFile, fpos) == 0x8, "SceVfsFile fpos");
_Static_assert(offsetof(SceVfsFile, idata) == 0x2C, "SceVfsFile idata");

/* the VFS operations' arguments */
typedef struct SceVfsOpMountArgs {
	SceVfsMount *mnt;
	SceVfsPath *devFilePath;
} SceVfsOpMountArgs;

typedef struct SceVfsOpUmountArgs {
	SceVfsMount *mnt;
	int flags;
} SceVfsOpUmountArgs;

typedef struct SceVfsOpSetRootArgs {
	SceVfsMount *mnt;
	int unused;			/* passed through */
	struct SceVfsVnode *vp;
} SceVfsOpSetRootArgs;

typedef struct SceVfsOpGetRootArgs {
	SceVfsMount *mnt;
	int unused;			/* passed through */
	struct SceVfsVnode **vpp;
} SceVfsOpGetRootArgs;

typedef struct SceVfsOpSyncArgs {
	SceVfsMount *mnt;
	int flags;
} SceVfsOpSyncArgs;

typedef struct SceVfsOpInitArgs {
	SceVfsInfo *vfsInf;
} SceVfsOpInitArgs;

typedef struct SceVfsOpFiniArgs {
	SceVfsInfo *vfsInf;
} SceVfsOpFiniArgs;

typedef struct SceVfsOpDevctlArg {
	SceVfsMount *mnt;
	const char *dev;
	unsigned int cmd;
	const void *arg;
	SceSize argLen;
	void *buf;
	SceSize bufLen;
} SceVfsOpDevctlArg;

typedef struct SceVfsOpDecodePathElemArgs {
	SceVfsMount *mnt;
	const char *path;
	const char **path2;
	const char **path3;
	char *buf;
	SceSize bufLen;
	SceSize *decodeLen;
} SceVfsOpDecodePathElemArgs;

_Static_assert(sizeof(SceVfsOpDevctlArg) == 0x1C, "SceVfsOpDevctlArg");
_Static_assert(sizeof(SceVfsOpDecodePathElemArgs) == 0x1C, "SceVfsOpDecodePathElemArgs");

/* a file system's operations */
struct SceVfsOpTable {
	int (*vfs_mount)(SceVfsOpMountArgs *argp);
	int (*vfs_umount)(SceVfsOpUmountArgs *argp);
	int (*vfs_set_root)(SceVfsOpSetRootArgs *argp);
	int (*vfs_get_root)(SceVfsOpGetRootArgs *argp);
	void *reserved[2];
	int (*vfs_sync)(SceVfsOpSyncArgs *argp);
	void *reserved2;
	int (*vfs_init)(SceVfsOpInitArgs *argp);
	int (*vfs_fini)(SceVfsOpFiniArgs *argp);
	void *reserved3;
	int (*vfs_devctl)(SceVfsOpDevctlArg *arg);
	int (*vfs_decode_path_elem)(SceVfsOpDecodePathElemArgs *argp);
};

_Static_assert(sizeof(SceVfsOpTable) == 0x34, "SceVfsOpTable");

/* the vnode operations' arguments */
typedef struct SceVopOpenArgs {
	SceVfsVnode *vp;
	SceVfsPath *path;
	int flags;
	SceVfsFile *file;
} SceVopOpenArgs;

typedef struct SceVopCreateArgs {
	SceVfsVnode *dvp;
	SceVfsVnode **vpp;
	SceVfsPath *path;
	int flags;
	int mode;
} SceVopCreateArgs;

typedef struct SceVopCloseArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
} SceVopCloseArgs;

typedef struct SceVopLookupArgs {
	SceVfsVnode *dvp;
	SceVfsVnode **vpp;
	SceVfsPath *path;
	SceUInt32 flags;
} SceVopLookupArgs;

typedef struct SceVopReadArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	void *buf;
	SceSize nbyte;
} SceVopReadArgs;

typedef struct SceVopWriteArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	const void *buf;
	SceSize nbyte;
} SceVopWriteArgs;

typedef struct SceVopLseekArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceOff offset;
	int whence;
} SceVopLseekArgs;

typedef struct SceVopIoctlArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	int cmd;
	const void *inData;
	SceSize inLen;
	void *outData;
	SceSize outLen;
} SceVopIoctlArgs;

typedef struct SceVopRemoveArgs {
	SceVfsVnode *dvp;
	SceVfsVnode *vp;
	SceVfsPath *path;
	SceUInt32 flags;
} SceVopRemoveArgs;

typedef struct SceVopMkdirArgs {
	SceVfsVnode *dvp;
	SceVfsVnode **vpp;
	SceVfsPath *path;
	int mode;
} SceVopMkdirArgs;

typedef struct SceVopRmdirArgs {
	SceVfsVnode *dvp;
	SceVfsVnode *vp;
	SceVfsPath *path;
} SceVopRmdirArgs;

typedef struct SceVopDopenArgs {
	SceVfsVnode *vp;
	SceVfsPath *path;
	SceVfsFile *file;
} SceVopDopenArgs;

typedef struct SceVopDcloseArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
} SceVopDcloseArgs;

typedef struct SceVopDreadArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceIoDirent *dir;
} SceVopDreadArgs;

typedef struct SceVopGetstatArgs {
	SceVfsVnode *vp;
	SceVfsPath *path;
	SceIoStat *stat;
} SceVopGetstatArgs;

typedef struct SceVopChstatArgs {
	SceVfsVnode *vp;
	SceVfsPath *path;
	SceIoStat *stat;
	int bit;
} SceVopChstatArgs;

typedef struct SceVopFchstatArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceIoStat *stat;
	SceUInt32 bit;
} SceVopFchstatArgs;

typedef struct SceVopFgetstatArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceIoStat *stat;
} SceVopFgetstatArgs;

typedef struct SceVopInactiveArgs {
	SceVfsVnode *vp;
} SceVopInactiveArgs;

typedef struct SceVopLinkArgs {
	SceVfsVnode *fvp;
	SceVfsVnode *tvp;
} SceVopLinkArgs;

typedef struct SceVopUnlinkArgs {
	SceVfsVnode *fvp;
	SceVfsVnode *tvp;
} SceVopUnlinkArgs;

typedef struct SceVopSyncArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	int flags;
} SceVopSyncArgs;

typedef struct SceVopRenameArgs {
	SceVfsVnode *odvp;
	SceVfsVnode *ovp;
	SceVfsPath *oldPath;
	SceVfsVnode *ndvp;
	SceVfsVnode **nvpp;
	SceVfsPath *newPath;
} SceVopRenameArgs;

typedef struct SceVopPreadArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	void *buf;
	SceSize nbyte;
	SceOff offset;
} SceVopPreadArgs;

typedef struct SceVopPwriteArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	const void *buf;
	SceSize nbyte;
	SceOff offset;
} SceVopPwriteArgs;

/* a4, a5: passed through unchanged; meaning not yet traced */
typedef struct SceVopWhiteoutArgs {
	SceVfsVnode *dvp;
	SceVfsVnode *vp;
	SceVfsPath *oldPath;
	SceVfsPath *newPath;
	SceUInt32 a4;
	SceUInt32 a5;
} SceVopWhiteoutArgs;

typedef struct SceVopCleanupArgs {
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceUInt32 a2;
	SceUInt32 a3;
} SceVopCleanupArgs;

/* The vnode wrapper forwards these three values unchanged to the filesystem. */
typedef struct SceVopZerofillArgs {
	SceVfsVnode *vp;
	SceUInt64 forwardedValue1;
	SceUInt64 forwardedValue2;
	SceUInt64 forwardedValue3;
} SceVopZerofillArgs;

_Static_assert(sizeof(SceVopLseekArgs) == 0x18, "SceVopLseekArgs");
_Static_assert(sizeof(SceVopIoctlArgs) == 0x1C, "SceVopIoctlArgs");
_Static_assert(sizeof(SceVopRenameArgs) == 0x18, "SceVopRenameArgs");
_Static_assert(sizeof(SceVopPreadArgs) == 0x18, "SceVopPreadArgs");
_Static_assert(sizeof(SceVopZerofillArgs) == 0x20, "SceVopZerofillArgs");

/* a vnode's operations */
struct SceVopTable {
	int (*vop_open)(SceVopOpenArgs *argp);
	int (*vop_create)(SceVopCreateArgs *argp);
	int (*vop_close)(SceVopCloseArgs *argp);
	int (*vop_lookup)(SceVopLookupArgs *argp);
	SceSSize (*vop_read)(SceVopReadArgs *argp);
	SceSSize (*vop_write)(SceVopWriteArgs *argp);
	SceOff (*vop_lseek)(SceVopLseekArgs *argp);
	int (*vop_ioctl)(SceVopIoctlArgs *argp);
	int (*vop_remove)(SceVopRemoveArgs *argp);
	int (*vop_mkdir)(SceVopMkdirArgs *argp);
	int (*vop_rmdir)(SceVopRmdirArgs *argp);
	int (*vop_dopen)(SceVopDopenArgs *argp);
	int (*vop_dclose)(SceVopDcloseArgs *argp);
	int (*vop_dread)(SceVopDreadArgs *argp);
	int (*vop_getstat)(SceVopGetstatArgs *argp);
	int (*vop_chstat)(SceVopChstatArgs *argp);
	int (*vop_rename)(SceVopRenameArgs *argp);
	const void *reserved;
	SceSSize (*vop_pread)(SceVopPreadArgs *argp);
	SceSSize (*vop_pwrite)(SceVopPwriteArgs *argp);
	int (*vop_inactive)(SceVopInactiveArgs *argp);
	int (*vop_link)(SceVopLinkArgs *argp);
	int (*vop_unlink)(SceVopUnlinkArgs *argp);
	int (*vop_sync)(SceVopSyncArgs *argp);
	int (*vop_fgetstat)(SceVopFgetstatArgs *argp);
	int (*vop_fchstat)(SceVopFchstatArgs *argp);
	int (*vop_whiteout)(SceVopWhiteoutArgs *argp);
	int (*vop_cleanup)(SceVopCleanupArgs *argp);
	int (*vop_zerofill)(SceVopZerofillArgs *argp);
};

_Static_assert(sizeof(SceVopTable) == 0x74, "SceVopTable");

/* sceVfsMount's parameters */
typedef struct SceVfsMountParam {
	const char *rootPath;		/* the mount point's root inside the file system (/ux/exfat) */
	const char *blockdevName;	/* replaces misc->blockdevName */
	SceUInt8 fsType;		/* SCE_VFS_FS_TYPE_* */
	SceUInt16 opt;			/* the I/O scheduler queue */
	SceUInt32 mntFlags;		/* SCE_VFS_MOUNT_TYPE_* | SCE_VFS_MOUNT_FLAG_* */
	const char *vfsName;
	void *data;			/* becomes the mount point's pData */
	SceVfsMountData *misc;
	SceVopTable *vops;		/* replaces the VFS's defaultVops for the root vnode */
} SceVfsMountParam;

_Static_assert(sizeof(SceVfsMountParam) == 0x20, "SceVfsMountParam");

/* sceVfsUmount's flag */
#define SCE_VFS_UMOUNT_FLAG_FORCE	0x1

typedef struct SceVfsUmountParam {
	const char *assignName;
	int flag;			/* SCE_VFS_UMOUNT_FLAG_* */
} SceVfsUmountParam;

/* Complete vfsRmdev argument block, owned by SceIofilemgr. */
typedef struct IofilemgrRmdevParam {
	const char *dev;
	unsigned int flags;		/* VFS_RMDEV_FLAG_* */
} IofilemgrRmdevParam;

#define VFS_RMDEV_FLAG_BUSY_CHECK	1	/* fail while files are open */

_Static_assert(sizeof(IofilemgrRmdevParam) == 8, "IofilemgrRmdevParam");
_Static_assert(offsetof(IofilemgrRmdevParam, dev) == 0, "IofilemgrRmdevParam dev");
_Static_assert(offsetof(IofilemgrRmdevParam, flags) == 4, "IofilemgrRmdevParam flags");

int vfsRmdev(IofilemgrRmdevParam *param);

/* a2, a3: passed by pfsmgr as 0, meaning not yet traced */
int vfsMountForPFS(SceVfsMountParam *param, SceUInt32 reservedLow, int reservedHigh);

int SceIofilemgrForDriver_0D8A806E(SceVfsVnode *fvp, SceVfsVnode *tvp);	/* vitasdk: ksceVopUnlink */
int SceIofilemgrForDriver_942AA61F(SceVfsVnode *fvp, SceVfsVnode *tvp);	/* vitasdk: ksceVopLink */
/* vitasdk: ksceVfsOpDecodePathElem */
int SceIofilemgrForDriver_F7DAC0F5(SceVfsMount *mnt, const char *path, const char **path2,
	const char **path3, char *buf, SceSize buf_len, SceSize *decode_len);
int sceVfsAddVfs(SceVfsInfo *vfsInf);
int sceVfsDeleteVfs(const char *name, SceVfsInfo **vfsInf);
int sceVfsOpDevctl(SceVfsMount *mnt, const char *dev, unsigned int cmd, const void *indata,
		   SceSize inlen, void *outdata, SceSize outlen);
int sceVopChstat(SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit);
int sceVopCleanup(SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2, SceUInt32 a3);
int sceVopClose(SceVfsVnode *vp, SceVfsFile *file);
int sceVopCreate(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int flags, int mode);
int sceVopDclose(SceVfsVnode *vp, SceVfsFile *file);
int sceVopDopen(SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file);
int sceVopDread(SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir);
int sceVopFchstat(SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat, int bit);
int sceVopFgetstat(SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat);
int sceVopGetstat(SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat);
int sceVopInactive(SceVfsVnode *vp);
int sceVopIoctl(SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *inData, SceSize inLen,
		void *outData, SceSize outLen);
int sceVopLookup(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, SceUInt32 flags);
SceOff sceVopLseek(SceVfsVnode *vp, SceVfsFile *file, SceOff offset, int whence);
int sceVopMkdir(SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int mode);
int sceVopOpen(SceVfsVnode *vp, SceVfsPath *path, int flags, SceVfsFile *file);
int sceVopPread(SceVfsVnode *vp, SceVfsFile *file, void *data, SceSize nbyte, SceOff offset,
		SceSize *result);
int sceVopPwrite(SceVfsVnode *vp, SceVfsFile *file, const void *data, SceSize nbyte, SceOff offset,
		 SceSize *result);
int sceVopRead(SceVfsVnode *vp, SceVfsFile *file, void *data, SceSize nbyte, SceSize *result);
int sceVopRemove(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags);
int sceVopRename(SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *oldPath, SceVfsVnode *ndvp,
		 SceVfsVnode **nvpp, SceVfsPath *newPath);
int sceVopRmdir(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags);
int sceVopSync(SceVfsVnode *vp, SceVfsFile *file, int flags);
int sceVopWhiteout(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath, SceVfsPath *newPath,
		   SceUInt32 a4, SceUInt32 a5);
int sceVopWrite(SceVfsVnode *vp, SceVfsFile *file, const void *data, SceSize nbyte,
		SceSize *result);
int sceVopZerofill(SceVfsVnode *vp, SceUInt64 forwardedValue1, SceUInt64 forwardedValue2,
		   SceUInt64 forwardedValue3);

SceUID vfsAllocateFile(SceVfsVnode *vp, SceVfsFile **file, const char *name);
int vfsFreeFile(SceVfsVnode *vp, SceUID fd);
int vfsFreeVnode(SceVfsVnode *vp);
int vfsGetNewVnode(SceVfsMount *mnt, SceVopTable *ops, int unused, SceVfsVnode **vpp);
int vfsGetVnodeName(SceVfsVnode *vp, char *name, SceSize n, SceSize *result);
int vfsLockMnt(SceVfsMount *mnt);
int vfsLockVnode(SceVfsVnode *vp);
int vfsMount(SceVfsMountParam *param);
int vfsUmount(SceVfsUmountParam *param);
int vfsUmountForPFS(SceVfsUmountParam *param, SceUInt32 reservedLow, int reservedHigh);
int vfsUnlockMnt(SceVfsMount *mnt);
int vfsUnlockVnode(SceVfsVnode *vp);

/* a2, a3: pfsmgr passes 1, 0; meaning not yet traced */
int vfsBuffCacheFree(SceVfsVnode *vp, SceVfsFile *file, int a2, int a3);
