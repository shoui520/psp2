/*
 * SceIofilemgr's VFS interface (os0:kd/iofilemgr.skprx), vitasdk's
 * <psp2kern/vfs.h> with the declarations it has short corrected to what
 * Sony's callers pass.
 */

#pragma once

#include <stddef.h>

#define vfsMountForPFS		vfsMountForPFS_vitasdk
#define ksceVopWhiteout		ksceVopWhiteout_vitasdk
#define ksceVopCleanup		ksceVopCleanup_vitasdk
#define ksceVopZerofill		ksceVopZerofill_vitasdk
#define SceVopWhiteoutArgs	SceVopWhiteoutArgs_vitasdk
#define SceVopCleanupArgs	SceVopCleanupArgs_vitasdk
#define SceVopZerofillArgs	SceVopZerofillArgs_vitasdk
#define ksceVopRmdir		ksceVopRmdir_vitasdk
#include <psp2kern/vfs.h>
#undef vfsMountForPFS
#undef ksceVopWhiteout
#undef ksceVopCleanup
#undef ksceVopZerofill
#undef SceVopWhiteoutArgs
#undef SceVopCleanupArgs
#undef SceVopZerofillArgs
#undef ksceVopRmdir

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
int vfsMountForPFS(SceVfsMountParam *param, int a2, int a3);

/* a4, a5: passed through unchanged; meaning not yet traced */
typedef struct SceVopWhiteoutArgs {
	SceVfsVnode *dvp;
	SceVfsVnode *vp;
	SceVfsPath *old_path;
	SceVfsPath *new_path;
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

VITASDK_BUILD_ASSERT_EQ(0x20, SceVopZerofillArgs);

int ksceVopWhiteout(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *old_path,
		    SceVfsPath *new_path, SceUInt32 a4, SceUInt32 a5);
int ksceVopCleanup(SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2, SceUInt32 a3);
int ksceVopZerofill(SceVfsVnode *vp, SceUInt64 forwardedValue1,
		   SceUInt64 forwardedValue2, SceUInt64 forwardedValue3);
int ksceVopRmdir(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags);
int ksceVfsOpDevctl(SceVfsMount *mnt, const char *dev, unsigned int cmd,
		   const void *indata, SceSize inlen, void *outdata, SceSize outlen);

/* a2, a3: pfsmgr passes 1, 0; meaning not yet traced */
int vfsBuffCacheFree(SceVfsVnode *vp, SceVfsFile *file, int a2, int a3);
