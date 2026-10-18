/*
 * The underlying file system, as PFS uses it: vnodes by name (ScePfsNode)
 * and files open in it (ScePfsLowerFile), each operation under the vnode's
 * lock.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_PAGE_SIZE		0x8000	/* name unknown */
#define PFS_ICV_FANOUT		23	/* name unknown: icv entries per tree node */
#define PFS_ICV_SIZE		0x400	/* name unknown */
#define PFS_NAME_MAX		0x40
#define PFS_LOOKUP_FLAGS	0xfffff	/* name unknown */
#define SCE_PFS_ERROR_NAME_TOO_LONG	0x8014011d	/* name unknown */

static inline SceUInt64 pfsPageRound(SceUInt64 size)	/* name unknown */
{
	return (size + PFS_PAGE_SIZE - 1) / PFS_PAGE_SIZE * PFS_PAGE_SIZE;
}

static inline SceOff pfsPageRoundSigned(SceOff size)	/* name unknown */
{
	return (size + PFS_PAGE_SIZE - 1) / PFS_PAGE_SIZE * PFS_PAGE_SIZE;
}

/* the space a file takes on disk: its pages and, with icv files, its icv tree */
void pfsDiskSize(ScePfsPmi *pmi, SceUInt32 attr, SceUInt32 index, SceUInt32 unused,
		 SceUInt64 size, SceUInt64 *diskSize)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	SceUInt64 pages = pfsPageRound(size);
	SceUInt32 count, div, nodes;
	SceUInt64 icvSize;

	if (index != 0 && info->icvType) {
		if ((SceInt16)attr < 0 || (attr & 0x2000)) {
			*diskSize = pages + PFS_PAGE_SIZE;
			return;
		}
		count = pages / PFS_PAGE_SIZE;
		if (count == 0) {
			icvSize = PFS_PAGE_SIZE;
		} else {
			div = 1;
			nodes = 0;
			do {
				div *= PFS_ICV_FANOUT;
				nodes += count / div + (count % div != 0);
			} while (div < count);
			icvSize = pfsPageRound((SceUInt32)((nodes + 1) * PFS_ICV_SIZE));
		}
		pages += icvSize;
	}
	*diskSize = pages;
}

void pfsVnodeDiskSize(ScePfsPmi *pmi, SceVfsVnode *vp, SceUInt64 *diskSize)
{
	ScePfsVd *vd = vp->core.node_data;
	SceUInt64 size;

	if (vd->attr & 0x8000)
		size = PFS_PAGE_SIZE;
	else
		size = vp->core.size;
	pfsDiskSize(pmi, vd->attr, vd->index, vd->attr, size, diskSize);
}

/* the space the mount's databases take */
int pfsPmiDiskSize(ScePfsPmi *pmi, SceUInt64 *diskSize)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	SceVfsVnode *vp = pmi->filesDb.vp;
	char name[PFS_NAME_MAX];
	SceSize nameLen;
	SceVfsPath path;
	SceIoStat stat;
	SceOff filesDb;
	int ret;

	vfsGetVnodeName(vp, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	vfsLockVnode(vp);
	ret = sceVopGetstat(vp, &path, &stat);
	vfsUnlockVnode(vp);
	if (ret < 0)
		return ret;

	filesDb = pfsPageRoundSigned(stat.st_size);
	if (info->icvType) {
		*diskSize = filesDb + 2 * PFS_PAGE_SIZE;
		return ret;
	}
	vp = pmi->unicvDb.vp;
	vfsGetVnodeName(vp, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	vfsLockVnode(vp);
	ret = sceVopGetstat(vp, &path, &stat);
	vfsUnlockVnode(vp);
	if (ret < 0)
		return ret;
	*diskSize = pfsPageRoundSigned(stat.st_size) + filesDb + PFS_PAGE_SIZE;
	return ret;
}

int pfsNodeInactive(ScePfsNode *node)
{
	int ret;

	vfsLockVnode(node->vp);
	ret = sceVopInactive(node->vp);
	if (ret == 0) {
		ret = vfsFreeVnode(node->vp);
		if (ret == 0)
			return 0;
	}
	vfsUnlockVnode(node->vp);
	return ret;
}

int pfsNodeRelease(ScePfsNode *node)
{
	vfsLockVnode(node->vp);
	sceVopInactive(node->vp);
	if (vfsFreeVnode(node->vp) != 0)
		vfsUnlockVnode(node->vp);
	return 0;
}

int pfsNodeLookup(ScePfsNode *node)
{
	const char *path2, *path3;
	char name[PFS_NAME_MAX];
	SceSize decodeLen;
	SceVfsVnode *vp;
	SceVfsPath path;
	int ret;

	ret = SceIofilemgrForDriver_F7DAC0F5(node->vp->core.mnt, node->name, &path2, &path3, name,
				      sizeof(name), &decodeLen);
	if (ret != 0)
		return ret;
	if (decodeLen >= sizeof(name))
		return SCE_PFS_ERROR_NAME_TOO_LONG;
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	vfsLockVnode(node->vp);
	ret = sceVopLookup(node->vp, &vp, &path, PFS_LOOKUP_FLAGS);
	vfsUnlockVnode(node->vp);
	if (ret == 0) {
		vfsUnlockVnode(vp);
		node->vp = vp;
	}
	return ret;
}

int pfsNodeCreate(ScePfsNode *node, int mode)
{
	SceVfsVnode *dvp = node->vp, *vp;
	SceVfsPath path;
	int ret;

	path.name = node->name;
	path.name_length = strnlen(path.name, PFS_NAME_MAX - 1);
	path.path = NULL;
	vfsLockVnode(dvp);
	ret = sceVopCreate(dvp, &vp, &path, SCE_O_RDWR | SCE_O_CREAT | SCE_O_EXCL, mode);
	vfsUnlockVnode(dvp);
	if (ret >= 0) {
		vfsUnlockVnode(vp);
		node->vp = vp;
	}
	return ret;
}

int pfsLowerOpen(ScePfsLowerFile *lf, ScePfsNode *node, SceUInt32 cookie, int flags)
{
	SceVfsPath path;
	int ret;

	path.name = node->name;
	lf->cookie = cookie;
	lf->vp = node->vp;
	path.name_length = strnlen(path.name, PFS_NAME_MAX - 1);
	path.path = NULL;
	vfsLockVnode(lf->vp);
	lf->fd = vfsAllocateFile(lf->vp, &lf->file, path.name);
	vfsUnlockVnode(lf->vp);
	ret = lf->fd;
	if (ret < 0)
		return ret;
	vfsLockVnode(lf->vp);
	ret = sceVopOpen(lf->vp, &path, flags, lf->file);
	vfsUnlockVnode(lf->vp);
	if (ret < 0) {
		vfsLockVnode(lf->vp);
		vfsFreeFile(lf->vp, lf->fd);
		vfsUnlockVnode(lf->vp);
	}
	return ret;
}

int pfsLowerClose(ScePfsLowerFile *lf)
{
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopClose(lf->vp, lf->file);
	vfsUnlockVnode(lf->vp);
	if (ret < 0)
		return ret;
	vfsLockVnode(lf->vp);
	ret = vfsFreeFile(lf->vp, lf->fd);
	vfsUnlockVnode(lf->vp);
	return ret;
}

/* close even when the underlying close fails, dropping the file's cached data */
int pfsLowerCloseForce(ScePfsLowerFile *lf)
{
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopClose(lf->vp, lf->file);
	vfsUnlockVnode(lf->vp);
	if (ret < 0) {
		vfsLockVnode(lf->vp);
		vfsBuffCacheFree(lf->vp, lf->file, 1, 0);
		vfsUnlockVnode(lf->vp);
		vfsLockVnode(lf->vp);
		sceVopClose(lf->vp, lf->file);
		vfsUnlockVnode(lf->vp);
	}
	vfsLockVnode(lf->vp);
	vfsFreeFile(lf->vp, lf->fd);
	vfsUnlockVnode(lf->vp);
	return 0;
}

int pfsLowerCloseLocked(ScePfsLowerFile *lf)
{
	vfsLockVnode(lf->vp);
	if (sceVopClose(lf->vp, lf->file) < 0) {
		sceVopCleanup(lf->vp, lf->file, 0, 0);
		sceVopClose(lf->vp, lf->file);
	}
	vfsFreeFile(lf->vp, lf->fd);
	vfsUnlockVnode(lf->vp);
	return 0;
}

int pfsLowerCleanup(ScePfsLowerFile *lf, SceUInt32 a2, SceUInt32 a3)
{
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopCleanup(lf->vp, lf->file, a2, a3);
	vfsUnlockVnode(lf->vp);
	return ret;
}

int pfsLowerPread(ScePfsLowerFile *lf, void *buf, SceSize nbyte, SceOff offset, SceSize *read)
{
	SceSize result = 0;
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopPread(lf->vp, lf->file, buf, nbyte, offset, &result);
	vfsUnlockVnode(lf->vp);
	if (ret >= 0) {
		ret = 0;
		*read = result;
	}
	return ret;
}

int pfsLowerPwrite(ScePfsLowerFile *lf, const void *buf, SceSize nbyte, SceOff offset,
		   SceSize *written)
{
	SceSize result = 0;
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopPwrite(lf->vp, lf->file, buf, nbyte, offset, &result);
	vfsUnlockVnode(lf->vp);
	if (ret >= 0) {
		ret = 0;
		*written = result;
	}
	return ret;
}

int pfsLowerGetSize(ScePfsLowerFile *lf, SceUInt64 *size)
{
	char name[PFS_NAME_MAX];
	SceSize nameLen;
	SceVfsPath path;
	SceIoStat stat;
	int ret;

	vfsGetVnodeName(lf->vp, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	vfsLockVnode(lf->vp);
	ret = sceVopGetstat(lf->vp, &path, &stat);
	vfsUnlockVnode(lf->vp);
	if (ret >= 0)
		*size = stat.st_size;
	return ret;
}

int pfsLowerSetSize(ScePfsLowerFile *lf, SceUInt64 size)
{
	char name[PFS_NAME_MAX];
	SceSize nameLen;
	SceVfsPath path;
	SceIoStat stat;
	int ret;

	vfsGetVnodeName(lf->vp, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	stat.st_size = size;
	vfsLockVnode(lf->vp);
	ret = sceVopChstat(lf->vp, &path, &stat, SCE_CST_SIZE);
	vfsUnlockVnode(lf->vp);
	return ret;
}

int pfsNodeRemove(ScePfsNode *dir, ScePfsNode *node)
{
	SceVfsVnode *dvp;
	SceVfsPath path;
	int ret;

	path.name = node->name;
	path.name_length = strnlen(path.name, PFS_NAME_MAX - 1);
	dvp = dir->vp;
	path.path = NULL;
	if (dvp == NULL) {
		vfsLockVnode(node->vp);
		ret = sceVopRemove(NULL, node->vp, &path, 0);
		vfsUnlockVnode(node->vp);
	} else {
		vfsLockVnode(dvp);
		vfsLockVnode(node->vp);
		ret = sceVopRemove(dvp, node->vp, &path, 0);
		vfsUnlockVnode(node->vp);
		vfsUnlockVnode(dvp);
	}
	return ret;
}

int pfsNodeMkdir(ScePfsNode *node, int mode)
{
	SceVfsVnode *vp;
	SceVfsPath path;
	int ret;

	path.name = node->name;
	path.name_length = strnlen(path.name, PFS_NAME_MAX - 1);
	path.path = NULL;
	vfsLockVnode(node->vp);
	ret = sceVopMkdir(node->vp, &vp, &path, mode);
	vfsUnlockVnode(node->vp);
	if (ret == 0) {
		vfsUnlockVnode(vp);
		node->vp = vp;
	}
	return ret;
}

int pfsNodeRmdir(ScePfsNode *dir, ScePfsNode *node)
{
	SceVfsVnode *dvp = dir->vp;
	SceVfsPath path;
	int ret;

	path.name = node->name;
	path.name_length = strnlen(path.name, PFS_NAME_MAX - 1);
	path.path = NULL;
	vfsLockVnode(dvp);
	vfsLockVnode(node->vp);
	ret = sceVopRmdir(dvp, node->vp, &path, 0);
	vfsUnlockVnode(node->vp);
	vfsUnlockVnode(dvp);
	return ret;
}

int pfsNodeRename(ScePfsNode *oldDir, ScePfsNode *oldNode, ScePfsNode *newNode)
{
	SceVfsVnode *odvp = oldDir->vp, *ovp = oldNode->vp, *ndvp, *nvp;
	SceVfsPath oldPath, newPath;
	int ret;

	oldPath.name = oldNode->name;
	oldPath.name_length = strnlen(oldPath.name, PFS_NAME_MAX - 1);
	oldPath.path = NULL;
	newPath.name = newNode->name;
	ndvp = newNode->vp;
	newPath.name_length = strnlen(newPath.name, PFS_NAME_MAX - 1);
	newPath.path = NULL;
	if (ndvp == odvp) {
		vfsLockVnode(ndvp);
		vfsLockVnode(ovp);
		ret = sceVopRename(ndvp, ovp, &oldPath, ndvp, &nvp, &newPath);
		vfsUnlockVnode(ovp);
		vfsUnlockVnode(ndvp);
	} else {
		vfsLockVnode(odvp);
		vfsLockVnode(ovp);
		vfsLockVnode(ndvp);
		ret = sceVopRename(odvp, ovp, &oldPath, ndvp, &nvp, &newPath);
		vfsUnlockVnode(ndvp);
		vfsUnlockVnode(ovp);
		vfsUnlockVnode(odvp);
	}
	if (ret >= 0) {
		vfsUnlockVnode(nvp);
		newNode->vp = nvp;
	}
	return ret;
}

int pfsLowerSync(ScePfsLowerFile *lf)
{
	int ret;

	vfsLockVnode(lf->vp);
	ret = sceVopSync(lf->vp, lf->file, 2);
	vfsUnlockVnode(lf->vp);
	return ret;
}

SceUInt32 pfsLowerCookie(ScePfsLowerFile *lf)
{
	return lf->cookie;
}
