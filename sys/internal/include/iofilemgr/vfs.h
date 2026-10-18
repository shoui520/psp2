/*
 * SceIofilemgr's VFS interface (os0:kd/iofilemgr.skprx), vitasdk's
 * <psp2kern/vfs.h> with the declarations it has short corrected to what
 * Sony's callers pass.
 */

#pragma once

#include <stddef.h>

#define vfsMountForPFS		vfsMountForPFS_vitasdk
#define SceVopWhiteoutArgs	SceVopWhiteoutArgs_vitasdk
#define SceVopCleanupArgs	SceVopCleanupArgs_vitasdk
#define SceVopZerofillArgs	SceVopZerofillArgs_vitasdk
#include <psp2kern/vfs.h>
#undef vfsMountForPFS
#undef SceVopWhiteoutArgs
#undef SceVopCleanupArgs
#undef SceVopZerofillArgs

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

int SceIofilemgrForDriver_0D8A806E(SceVfsVnode *fvp, SceVfsVnode *tvp);	/* vitasdk: ksceVopUnlink */
int SceIofilemgrForDriver_942AA61F(SceVfsVnode *fvp, SceVfsVnode *tvp);	/* vitasdk: ksceVopLink */
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
int sceVopIoctl(SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in_data, SceSize in_len,
		void *out_data, SceSize out_len);
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
int sceVopRename(SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *old_path, SceVfsVnode *ndvp,
		 SceVfsVnode **nvpp, SceVfsPath *new_path);
int sceVopRmdir(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path, SceUInt32 flags);
int sceVopSync(SceVfsVnode *vp, SceVfsFile *file, int flags);
int sceVopWhiteout(SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *old_path, SceVfsPath *new_path,
		   SceUInt32 a4, SceUInt32 a5);
int sceVopWrite(SceVfsVnode *vp, SceVfsFile *file, const void *data, SceSize nbyte,
		SceSize *result);
int sceVopZerofill(SceVfsVnode *vp, SceUInt64 forwardedValue1, SceUInt64 forwardedValue2,
		   SceUInt64 forwardedValue3);

/* a2, a3: pfsmgr passes 1, 0; meaning not yet traced */
int vfsBuffCacheFree(SceVfsVnode *vp, SceVfsFile *file, int a2, int a3);
