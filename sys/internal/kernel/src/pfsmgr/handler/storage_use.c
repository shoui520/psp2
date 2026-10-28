/*
 * The underlying file system, as PFS uses it: vnodes by name (ScePfsNode)
 * and files open in it (ScePfsLowerFile), each operation under the vnode's
 * lock.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_ICV_FANOUT		23	/* name unknown: icv entries per tree node */
#define PFS_ICV_SIZE		0x400	/* name unknown */
#define PFS_LOOKUP_FLAGS	0xfffff	/* name unknown */
#define SCE_PFS_ERROR_NAME_TOO_LONG	0x8014011d	/* name unknown */

#if FW_VERSION >= 0x04000000
#endif


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
	ScePfsVd *vd = vp->core.nodeData;
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
	path.nameLength = strnlen(name, sizeof(name) - 1);
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
	path.nameLength = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	vfsLockVnode(vp);
	ret = sceVopGetstat(vp, &path, &stat);
	vfsUnlockVnode(vp);
	if (ret < 0)
		return ret;
	*diskSize = pfsPageRoundSigned(stat.st_size) + filesDb + PFS_PAGE_SIZE;
	return ret;
}
