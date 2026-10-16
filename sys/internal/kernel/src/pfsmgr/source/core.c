/*
 * The PFS file system proper: files and directories of the underlying file
 * system, each with an entry in files.db (attributes, size, the hash of its
 * icv tree) and, in icv.db modes, its own "%08x.icv" file.
 */

#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_PAGE_SIZE		0x8000
#define PFS_ATTR_ICV_RENAMED	0x0800	/* name unknown */
#define PFS_NAME_MAX		0x40
#define PFS_MAP			0x4420	/* name unknown */
#define PFS_MAP_SEQUENTIAL	0x4421	/* name unknown: maps at the file position */

#define SCE_PFS_ERROR_READ_ONLY		0x80142010	/* name unknown */
#define SCE_PFS_ERROR_NOT_FILE		0x80142012	/* name unknown */
#define SCE_PFS_ERROR_NOT_DIR		0x80142011	/* name unknown */
#define SCE_PFS_ERROR_QUOTA		0x80142019	/* name unknown */
#define SCE_PFS_ERROR_MAPPED		0x80142318	/* name unknown */
#define SCE_PFS_ERROR_LOOKUP_CLEANED	0x8014001a	/* name unknown */
#define SCE_PFS_ERROR_LRU_ALIGN		0x80140509
#define SCE_PFS_ERROR_NO_LRU		0x8014050b	/* name unknown */

ScePfsBufferList g_pfsFiBufferList;

int pfsFiInit(void)
{
	return pfsBufferListInit(&g_pfsFiBufferList, "ScePfsFiBufferList", sizeof(ScePfsFi), 4,
				 0x100);
}

void pfsFiTerm(void)
{
	pfsBufferListTerm(&g_pfsFiBufferList);
}

static inline void pfsDbKeyInit(ScePfsDbKey *key, ScePfsVd *vd, SceUInt32 parentIndex,
				const char *name)	/* name unknown */
{
	key->parentIndex = parentIndex;
	key->name = name;
	key->mnt = vd->lowerVp->core.mnt;
}

static inline SceOff pfsPageRound(SceOff size)	/* name unknown */
{
	return (size + PFS_PAGE_SIZE - 1) / PFS_PAGE_SIZE * PFS_PAGE_SIZE;
}

/* whether one more entry fits in the quota */
static inline int pfsQuotaCheckCreate(ScePfsPmi *pmi)	/* name unknown */
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	SceUInt64 usage, disk, dbSize, need;
	int ret;

	if (pmi->quota == 0)
		return 0;
	ret = pfsDbGetUsage(pmi, &usage);
	if (ret != 0)
		return ret;
	ret = pfsPmiDiskSize(pmi, &disk);
	if (ret != 0)
		return ret;
	pfsDbGetSize(pmi, &dbSize);
	need = pfsPageRound(dbSize) + (info->icvType == PFS_ICV_UNICV ? PFS_PAGE_SIZE :
				       2 * PFS_PAGE_SIZE);
	if (need + usage + disk > pmi->quota)
		return SCE_PFS_ERROR_QUOTA;
	return 0;
}

/* whether a file of size bytes may grow by grow bytes */
static inline int pfsQuotaCheckGrow(ScePfsPmi *pmi, ScePfsCrypt *ctx, SceOff size, SceOff grow)
{
	SceUInt64 usage, disk;
	SceOff oldIcv, newIcv, used;
	int ret;

	ret = pfsDbGetUsage(pmi, &usage);
	if (ret != 0)
		return ret;
	ret = pfsPmiDiskSize(pmi, &disk);
	if (ret != 0)
		return ret;
	oldIcv = ctx->icvSize(ctx, size);
	newIcv = ctx->icvSize(ctx, size + grow);
	used = usage + disk - pfsPageRound(size) + pfsPageRound(size + grow) +
	       pfsPageRound(newIcv) - pfsPageRound(oldIcv);
	if ((SceUInt64)used > pmi->quota)
		return SCE_PFS_ERROR_QUOTA;
	return 0;
}

int pfsCoreFind(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 *index, SceUInt16 *attr,
		SceUInt64 *size)
{
	ScePfsVd *vd = node->vp->core.node_data;
	ScePfsDbKey key;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	key.name = node->name;
	key.parentIndex = vd->index;
	key.mnt = vd->lowerVp->core.mnt;
	return pfsDbFind(pmi, &key, NULL, index, attr, size);
}

int pfsVnodeCreate(SceVfsVnode **vpp, SceVfsVnode *dvp, SceVfsVnode *lowerVp,
		   SceVfsVnode *icvVp, SceUInt32 index, SceUInt32 parentIndex, SceUInt16 attr,
		   SceUInt32 unused, SceUInt64 size)
{
	SceVfsMount *mnt;
	ScePfsVd *vd;
	SceVfsVnode *vp;
	int ret;

	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mnt_vfs_inf->default_vops,
			     0, vpp);
	if (ret != 0)
		return ret;
	vd = pfsBufferListAlloc(&g_pfsVdBufferList);
	if (vd == NULL) {
		vfsFreeVnode(*vpp);
		return SCE_ERROR_ERRNO_ENOMEM;
	}

	vfsLockVnode(lowerVp);
	vd->lowerVp = lowerVp;
	vd->icvVp = icvVp;
	vd->attr = attr;
	vd->slot = -1;
	vd->parentIndex = parentIndex;
	vd->pages.next = &vd->pages;
	vd->pages.prev = &vd->pages;
	vd->index = index;
	vfsLockVnode(*vpp);
	mnt = dvp->core.mnt;
	(*vpp)->core.dd = dvp;
	(*vpp)->core.mnt = mnt;
	vp = *vpp;
	vp->core.node_data = vd;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = lowerVp->core.type;
	vp->core.acl_data[0] = attr;
	vp->core.acl_data[1] = 0;
	*(SceUInt64 *)vp->core.fid = *(SceUInt64 *)lowerVp->core.fid;
	vp->core.size = size;
	vp->core.mnt_opt = lowerVp->core.mnt_opt | 0x10000;
	vp->core.ref_count = 1;
	vfsUnlockVnode(lowerVp);
	return 0;
}

int pfsCoreCreateEntry(ScePfsPmi *pmi, ScePfsNode *node, int mode, int icvMode,
		       SceUInt32 *index, SceUInt16 attrFlags, SceUInt16 *attr)
{
	ScePfsVd *vd = node->vp->core.node_data;
	SceUInt8 hash[PFS_HASH_SIZE];
	ScePfsDbKey key;
	SceVfsVnode *lowerVp;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerVp = vd->lowerVp;
	ret = pfsModeToAttr(mode, attr);
	if (ret != 0)
		return ret;
	*attr |= attrFlags;
	ret = pfsIndexAlloc(pmi, index);
	if (ret != 0)
		return ret;
	ret = pfsIcvCreate(pmi, *index, *attr, icvMode, hash);
	if (ret == 0) {
		key.parentIndex = vd->index;
		key.name = node->name;
		key.mnt = lowerVp->core.mnt;
		ret = pfsDbInsert(pmi, &key, hash, *index, *attr, attrFlags & PFS_ATTR_DIR);
		if (ret == 0)
			return 0;
	}
	pfsIndexFree(pmi, *index);
	return ret;
}

int pfsCoreMap(ScePfsPmi *pmi, SceVfsVnode *vp, ScePfsFi *fi, int cmd, ScePfsMapArg *arg,
	       ScePfsBlockRequest *map)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = vp->core.node_data;
	ScePfsListNode *node;
	ScePfsCrypt ctx;
	SceOff offset, first, last;
	SceSize length, blockSize;
	int ret, err;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (info->icvType == PFS_ICV_UNICV)
		ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, vp->core.size, vd->hash);
	else
		ret = pfsCryptInit(&ctx, pmi, &fi->icv, vd->index, vd->attr, vp->core.size, vd->hash);
	if (ret != 0)
		return ret;

	if (cmd == PFS_MAP_SEQUENTIAL)
		offset = fi->position;
	else
		offset = arg->offset;
	length = arg->length;
	blockSize = ctx.blockSize(&ctx);
	first = offset / blockSize;
	last = (offset + length - 1) / blockSize;
	if (arg->count < (SceUInt32)(last - first) + 1)
		length = (first + arg->count) * blockSize - offset;
	if (arg->key->klicensee == NULL)
		pfsFileKeyInit(arg->key, vd->index, vp->core.size, vd->attr, info->type, &ctx, pmi);
	pfsBlockRequestInitRead(map, arg->key, offset, length, arg->buf);

	err = ret;
	if (map->length != 0) {
		map->icvs = arg->icvs;
		map->plain = arg->plain;
		map->disk = arg->disk;
		map->allocated = arg->count;
		err = pfsBlockRead(map, &fi->lower, &ctx);
		if (err == 0) {
			for (node = vd->pages.next; node != &vd->pages; node = node->next)
				if (node == &map->link)
					goto linked;
			map->link.next = &vd->pages;
			map->link.prev = vd->pages.prev;
			vd->pages.prev = &map->link;
			map->link.prev->next = &map->link;
		linked:
			err = ret;
			if (cmd == PFS_MAP_SEQUENTIAL) {
				fi->position += map->length;
				err = 0;
			}
		}
	}
	ctx.fini(&ctx);
	pfsCryptClear(&ctx);
	return err;
}

int pfsCoreReadData(ScePfsFi *fi, ScePfsPmi *pmi, void *buf, SceSize nbyte, SceOff offset,
		    SceSize *read)
{
	SceVfsVnode *vp = fi->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = vp->core.node_data;
	ScePfsCrypt ctx;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (info->icvType == PFS_ICV_UNICV)
		ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, vp->core.size, vd->hash);
	else
		ret = pfsCryptInit(&ctx, pmi, &fi->icv, vd->index, vd->attr, vp->core.size, vd->hash);
	if (ret != 0)
		return ret;
	ret = pfsFileRead(&fi->lower, vd->index, vp->core.size, vd->attr, pmi, &ctx, buf, nbyte,
			  offset, read);
	ctx.fini(&ctx);
	pfsCryptClear(&ctx);
	return ret;
}

/* take the size of the underlying file into vp and files.db when it changed */
int pfsCoreUpdateSize(ScePfsPmi *pmi, SceVfsVnode *vp)
{
	ScePfsVd *vd = vp->core.node_data;
	SceVfsVnode *lowerVp = vd->lowerVp;
	char name[PFS_NAME_MAX];
	SceSize nameLen;
	SceVfsPath path;
	SceIoStat stat;
	SceUInt64 size;
	ScePfsDbKey key;
	int ret;

	vfsGetVnodeName(vp, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	memset(&stat, 0, sizeof(stat));
	vfsLockVnode(lowerVp);
	ret = ksceVopGetstat(lowerVp, &path, &stat);
	vfsUnlockVnode(lowerVp);
	if (ret < 0 || vp->core.size == (SceUInt64)stat.st_size)
		return ret;
	size = stat.st_size;
	if (vd->parentIndex != (SceUInt32)-1) {
		key.mnt = vd->lowerVp->core.mnt;
		key.name = name;
		key.parentIndex = vd->parentIndex;
		ret = pfsDbUpdate(pmi, &key, NULL, NULL, &size, NULL, 0);
		if (ret != 0)
			return ret;
	}
	vfsLockVnode(vp);
	vp->core.size = size;
	vfsUnlockVnode(vp);
	return ret;
}

/* remove a file's files.db entry, or its icv file when it has its own */
int pfsCoreRemoveEntry(ScePfsPmi *pmi, SceVfsVnode *dvp, ScePfsNode *node, SceUInt32 index,
		       SceUInt16 attr)
{
	char name[PFS_NAME_MAX];
	ScePfsNode dir, icv;
	ScePfsDbKey key;
	ScePfsVd *vd;

	if (attr & PFS_ATTR_ICV_RENAMED) {
		dir.vp = pmi->icvDirVp;
		dir.name = NULL;
		vd = node->vp->core.node_data;
		if (vd != NULL) {
			snprintf(name, sizeof(name), "%08x.icv", index);
			icv.vp = vd->icvVp;
			icv.name = name;
			return pfsNodeRemove(&dir, &icv);
		}
	} else {
		pfsIndexFree(pmi, index);
		vd = dvp->core.node_data;
		if (vd != NULL) {
			key.name = node->name;
			key.parentIndex = vd->index;
			key.mnt = vd->lowerVp->core.mnt;
			return pfsDbRemove(pmi, &key);
		}
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

/* release the LRU tables set up by pfsCoreMount */
static inline void pfsLruFree(ScePfsLru *lru)	/* name unknown */
{
	ScePfsLruEntry **buckets;
	void *pool;

	pfsLruTermTable(lru, &buckets, &pool);
	pfsBufferListFree(&g_pfsLruTblHdrBufferList, buckets);
	pfsBufferListFree(&g_pfsLruPoolBufferList, pool);
}

static inline int pfsLruAlloc(ScePfsLru *lru)	/* name unknown */
{
	ScePfsLruEntry **buckets;
	void *pool;

	if (g_pfsLruTblHdrBufferList.elementSize != PFS_LRU_TBL_HDR_SIZE ||
	    g_pfsLruPoolBufferList.elementSize != PFS_LRU_POOL_SIZE)
		return SCE_PFS_ERROR_LRU_ALIGN;
	buckets = pfsBufferListAlloc(&g_pfsLruTblHdrBufferList);
	if (buckets == NULL)
		return SCE_PFS_ERROR_NO_LRU;
	pool = pfsBufferListAlloc(&g_pfsLruPoolBufferList);
	if (pool == NULL) {
		pfsBufferListFree(&g_pfsLruTblHdrBufferList, buckets);
		return SCE_PFS_ERROR_NO_LRU;
	}
	pfsLruInitTable(lru, 5, 0x400, 5, buckets, pool);
	return 0;
}

int pfsCoreMount(ScePfsPmi *pmi, ScePfsNode *root)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	SceUInt32 devInfo[6];
	ScePfsNode node, dir;
	int ret, createdSceDir, createdDb;

	ret = ksceVfsOpDevctl(root->vp->core.mnt, root->vp->core.mnt->mnt_data->assign_name,
			      0x3001, NULL, 0, devInfo, sizeof(devInfo));
	if (ret == 0)
		pmi->clusterSize = devInfo[4];
	else if (ret < 0)
		return ret;
	if (info->readOnly)
		pmi->flags |= PFS_PMI_READ_ONLY;

	ret = pfsLruAlloc(&pmi->filesDbLru);
	if (ret != 0)
		return ret;
	ret = pfsLruAlloc(&pmi->icvLru);
	if (ret != 0)
		goto freeDbLru;

	vfsLockVnode(root->vp);
	node.vp = root->vp;
	node.name = "sce_pfs";
	ret = pfsNodeLookup(&node);
	if (ret == 0) {
		vfsUnlockVnode(root->vp);
		pmi->sceDirVp = node.vp;
		createdSceDir = 0;
	} else if (ret == SCE_ERROR_ERRNO_ENOENT && !info->noCreate) {
		ret = pfsNodeMkdir(&node, info->dirMode);
		vfsUnlockVnode(root->vp);
		if (ret < 0)
			goto freeIcvLru;
		createdSceDir = 1;
		pmi->sceDirVp = node.vp;
	} else {
		vfsUnlockVnode(root->vp);
		goto freeIcvLru;
	}

	vfsLockVnode(pmi->sceDirVp);
	node.vp = pmi->sceDirVp;
	node.name = "files.db";
	ret = pfsNodeLookup(&node);
	if (ret == 0) {
		ret = pfsDbOpen(pmi, &node);
		if (ret < 0) {
			pfsNodeInactive(&node);
			goto unlockSceDir;
		}
		createdDb = 0;
	} else if (ret == SCE_ERROR_ERRNO_ENOENT) {
		if (info->noCreate)
			goto unlockSceDir;
		ret = pfsDbCreate(pmi, &node);
		if (ret < 0)
			goto unlockSceDir;
		createdDb = 1;
	} else if (ret >= 0) {
		createdDb = 0;
	} else {
		goto unlockSceDir;
	}

	if (info->icvType == PFS_ICV_FILES) {
		node.vp = pmi->sceDirVp;
		node.name = "icv.db";
		ret = pfsNodeLookup(&node);
		if (ret != 0 && (ret != SCE_ERROR_ERRNO_ENOENT || info->noCreate ||
				 (ret = pfsNodeMkdir(&node, info->dirMode)) < 0))
			goto closeDb;
		pmi->icvDirVp = node.vp;
	} else if (info->icvType == PFS_ICV_UNICV) {
		node.vp = pmi->sceDirVp;
		pmi->icvDirVp = NULL;
		node.name = "unicv.db";
		ret = pfsNodeLookup(&node);
		if (ret == 0) {
			ret = pfsUnicvOpen(pmi, &node, 0);
			if (ret != 0) {
				pfsNodeInactive(&node);
				goto closeDb;
			}
		} else if (ret != SCE_ERROR_ERRNO_ENOENT || info->noCreate ||
			   (ret = pfsUnicvOpen(pmi, &node, 1)) != 0) {
			goto closeDb;
		}
		ret = 0;
	}
	vfsUnlockVnode(pmi->sceDirVp);
	return ret;

closeDb:
	pfsDbClose(pmi);
	node.vp = pmi->filesDb.vp;
	node.name = "files.db";
	if (createdDb) {
		dir.vp = pmi->sceDirVp;
		dir.name = NULL;
		pfsNodeRemove(&dir, &node);
	}
	pfsNodeInactive(&node);
unlockSceDir:
	vfsUnlockVnode(pmi->sceDirVp);
	node.vp = pmi->sceDirVp;
	node.name = "sce_pfs";
	if (createdSceDir) {
		dir.vp = root->vp;
		dir.name = NULL;
		pfsNodeRmdir(&dir, &node);
	}
	pfsNodeInactive(&node);
freeIcvLru:
	pfsLruFree(&pmi->icvLru);
freeDbLru:
	pfsLruFree(&pmi->filesDbLru);
	return ret;
}

int pfsCoreUmount(ScePfsPmi *pmi, int flags)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsNode node;
	int ret;

	if (flags & 1)
		pmi->flags |= PFS_PMI_NOT_MOUNTED;
	else
		pmi->flags &= ~PFS_PMI_NOT_MOUNTED;
	ret = pfsDbClose(pmi);
	if (ret < 0)
		return ret;

	node.vp = pmi->filesDb.vp;
	node.name = NULL;
	if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
		pfsNodeRelease(&node);
	} else {
		ret = pfsNodeInactive(&node);
		if (ret < 0)
			return ret;
	}
	pfsLruFree(&pmi->filesDbLru);

	if (info->icvType == PFS_ICV_UNICV) {
		pfsUnicvClose(pmi);
	} else {
		node.vp = pmi->icvDirVp;
		node.name = NULL;
		if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
			pfsNodeRelease(&node);
		} else {
			ret = pfsNodeInactive(&node);
			if (ret < 0)
				return ret;
		}
	}
	pfsLruFree(&pmi->icvLru);

	node.vp = pmi->sceDirVp;
	node.name = "sce_pfs";
	if (pmi->flags & PFS_PMI_NOT_MOUNTED)
		pfsNodeRelease(&node);
	else
		pfsNodeInactive(&node);
	return ret;
}

int pfsCoreSync(ScePfsPmi *pmi, int flags)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);

	if (info->noSync)
		return 0;
	return pfsLowerSync(&pmi->filesDb);
}

int pfsCoreLookup(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 flags)
{
	SceVfsVnode *dvp = node->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = dvp->core.node_data;
	SceVfsVnode *lowerVp, *icvVp, *vp;
	SceUInt64 size, oldDisk, newDisk, icvDisk;
	char name[PFS_NAME_MAX];
	ScePfsNode lower, file;
	ScePfsDbKey key;
	SceUInt32 index;
	SceUInt16 attr;
	SceUInt16 mode;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerVp = vd->lowerVp;
	key.name = node->name;
	key.parentIndex = vd->index;
	key.mnt = lowerVp->core.mnt;
	ret = pfsDbFind(pmi, &key, NULL, &index, &attr, &size);
	if (ret != 0)
		return ret;

	if ((SceInt16)attr < 0 || (memset(name, 0, sizeof(name)), info->icvType == PFS_ICV_UNICV)) {
		icvVp = NULL;
	} else {
		snprintf(name, sizeof(name), "%08x.icv", index);
		lower.vp = pmi->icvDirVp;
		lower.name = name;
		if (pfsNodeLookup(&lower) != 0) {
			mode = pmi->mode;
			ret = SCE_ERROR_ERRNO_ENOENT;
			icvVp = NULL;
			goto cleanup;
		}
		icvVp = lower.vp;
	}

	lower.name = node->name;
	lower.vp = lowerVp;
	ret = pfsNodeLookup(&lower);
	if (ret == 0) {
		lowerVp = lower.vp;
		ret = pfsVnodeCreate(&vp, dvp, lowerVp, icvVp, index, vd->index,
				     pfsAdjustAttr(pmi, attr), 0, size);
		if (ret == 0) {
			node->vp = vp;
			return 0;
		}
		if (lowerVp != NULL) {
			lower.name = node->name;
			lower.vp = lowerVp;
			pfsNodeInactive(&lower);
		}
		goto out;
	}
	mode = pmi->mode;
	if (mode != 3)
		goto out;

cleanup:
	/* the entry has no file: drop it */
	if ((unsigned int)(mode - 5) < 5 && ((1 << (mode - 5)) & 0x17) &&
	    (flags == 5 || flags == 0x20) && ret == SCE_ERROR_ERRNO_ENOENT) {
		if (icvVp != NULL) {
			lower.vp = icvVp;
			lower.name = name;
			pfsNodeInactive(&lower);
		}
		dvp = node->vp;
		vd = dvp->core.node_data;
		if (vd != NULL) {
			lowerVp = vd->lowerVp;
			pfsVnodeDiskSize(pmi, dvp, &oldDisk);
			pfsDiskSize(pmi, attr, index, attr, size, &icvDisk);
			file.vp = lowerVp;
			file.name = node->name;
			if (pfsNodeLookup(&file) == 0) {
				lower.vp = lowerVp;
				lower.name = NULL;
				pfsNodeRemove(&lower, &file);
				pfsNodeInactive(&file);
			}
			pfsCoreUpdateSize(pmi, dvp);
			pfsCoreRemoveEntry(pmi, dvp, node, index, attr);
			pfsVnodeDiskSize(pmi, dvp, &newDisk);
			pfsDbAddUsage(pmi, newDisk - oldDisk - icvDisk);
		}
		return SCE_PFS_ERROR_LOOKUP_CLEANED;
	}
out:
	if (icvVp != NULL) {
		lower.vp = icvVp;
		lower.name = name;
		pfsNodeInactive(&lower);
	}
	return ret;
}

int pfsCoreCreate(ScePfsPmi *pmi, ScePfsNode *node, int mode, SceUInt32 icvMode)
{
	SceVfsVnode *dvp = node->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = dvp->core.node_data;
	SceUInt64 size = 0, oldDisk, dirDisk, fileDisk;
	SceVfsVnode *lowerDvp, *lowerVp, *icvVp, *vp;
	char name[PFS_NAME_MAX];
	ScePfsNode lower, dir;
	SceUInt32 index;
	SceUInt16 attr;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerDvp = vd->lowerVp;
	pfsVnodeDiskSize(pmi, dvp, &oldDisk);
	ret = pfsQuotaCheckCreate(pmi);
	if (ret != 0)
		return ret;
	if (!info->noCreate) {
		ret = pfsCoreCreateEntry(pmi, node, mode, icvMode, &index, 0, &attr);
		if (ret != 0)
			return ret;
	} else {
		ret = pfsCoreFind(pmi, node, &index, &attr, &size);
		if (ret != 0)
			return ret;
		if ((SceInt16)attr < 0)
			return SCE_PFS_ERROR_NOT_FILE;
	}

	lower.vp = lowerDvp;
	lower.name = node->name;
	ret = pfsNodeCreate(&lower, info->fileMode);
	lowerVp = lower.vp;
	if (ret < 0)
		goto removeEntry;
	if (info->icvType == PFS_ICV_UNICV) {
		icvVp = NULL;
	} else {
		snprintf(name, sizeof(name), "%08x.icv", index);
		lower.vp = pmi->icvDirVp;
		lower.name = name;
		ret = pfsNodeLookup(&lower);
		if (ret != 0)
			goto removeFile;
		icvVp = lower.vp;
	}
	ret = pfsVnodeCreate(&vp, dvp, lowerVp, icvVp, index, vd->index, pfsAdjustAttr(pmi, attr),
			     0, size);
	if (ret == 0) {
		if (info->noCreate)
			goto done;
		ret = pfsCoreUpdateSize(pmi, dvp);
		if (ret == 0) {
			pfsVnodeDiskSize(pmi, dvp, &dirDisk);
			pfsVnodeDiskSize(pmi, vp, &fileDisk);
			ret = pfsDbAddUsage(pmi, dirDisk + fileDisk - oldDisk);
			if (ret == 0)
				goto done;
		}
		pfsBufferListFree(&g_pfsVdBufferList, vp->core.node_data);
		vfsFreeVnode(vp);
	}
	if (info->icvType != PFS_ICV_UNICV) {
		snprintf(name, sizeof(name), "%08x.icv", index);
		dir.vp = pmi->icvDirVp;
		dir.name = NULL;
		lower.vp = icvVp;
		lower.name = name;
		pfsNodeRemove(&dir, &lower);
		pfsNodeInactive(&lower);
	}
removeFile:
	lower.name = node->name;
	dir.name = NULL;
	dir.vp = lowerDvp;
	lower.vp = lowerVp;
	pfsNodeRemove(&dir, &lower);
	pfsNodeInactive(&lower);
removeEntry:
	if (!info->noCreate)
		pfsCoreRemoveEntry(pmi, dvp, node, index, attr);
	return ret;
done:
	node->vp = vp;
	return 0;
}

int pfsCoreOpen(ScePfsFi *fi, ScePfsPmi *pmi, ScePfsNode *node, int flags)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	SceVfsVnode *vp = node->vp;
	ScePfsVd *vd = vp->core.node_data, *dvd;
	SceUInt8 hash[PFS_HASH_SIZE];
	char name[PFS_NAME_MAX];
	ScePfsCrypt ctx;
	ScePfsDbKey key;
	ScePfsNode lower;
	SceUInt64 size;
	SceVfsVnode *icvVp;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lower.vp = vd->lowerVp;
	key.name = node->name;
	icvVp = vd->icvVp;
	key.parentIndex = vd->parentIndex;
	key.mnt = vd->lowerVp->core.mnt;
	fi->vp = vp;
	fi->position = 0;
	ret = pfsDbFind(pmi, &key, hash, &vd->index, &vd->attr, &size);
	if (ret != 0)
		return ret;
	if (vp->core.size != size)
		return SCE_ERROR_ERRNO_EINVAL;
	pfsHashCopy(vd->hash, hash);

	dvd = node->vp->core.node_data;
	if (pmi->mode == 10 && (unsigned int)((flags & 3) - 2) < 2) {
		key.name = node->name;
		key.parentIndex = dvd->parentIndex;
		vd->attr |= 0x400;
		key.mnt = dvd->lowerVp->core.mnt;
		ret = pfsDbUpdate(pmi, &key, NULL, &vd->attr, NULL, &size, 0);
		if (ret != 0)
			return ret;
		dvd->attr = vd->attr;
	}

	lower.name = node->name;
	ret = pfsLowerOpen(&fi->lower, &lower, -1, flags | 0x10000000);
	if (ret != 0)
		return ret;
	if (info->icvType == PFS_ICV_UNICV) {
		ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, size, vd->hash);
	} else {
		snprintf(name, sizeof(name), "%08x.icv", vd->index);
		lower.vp = icvVp;
		lower.name = name;
		ret = pfsLowerOpen(&fi->icv, &lower, vd->index, flags | 0x10000000);
		if (ret != 0)
			goto closeLower;
		if (info->icvType == PFS_ICV_UNICV)
			ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, size, vd->hash);
		else
			ret = pfsCryptInit(&ctx, pmi, &fi->icv, vd->index, vd->attr, size,
					   vd->hash);
	}
	if (ret == 0) {
		pfsCryptClear(&ctx);
		return 0;
	}
	if (info->icvType != PFS_ICV_UNICV)
		pfsLowerClose(&fi->icv);
closeLower:
	pfsLowerClose(&fi->lower);
	return ret;
}

int pfsCoreClose(ScePfsFi *fi, ScePfsPmi *pmi)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);

	if (info->icvType != PFS_ICV_UNICV) {
		if (pmi->flags & PFS_PMI_NOT_MOUNTED)
			pfsLowerCloseForce(&fi->icv);
		else
			pfsLowerCloseLocked(&fi->icv);
	}
	if (pmi->flags & PFS_PMI_NOT_MOUNTED)
		pfsLowerCloseForce(&fi->lower);
	else
		pfsLowerCloseLocked(&fi->lower);
	return 0;
}

/* after a change of vp's size: its new hash and size into files.db */
static inline int pfsCoreCommit(ScePfsPmi *pmi, ScePfsFi *fi, ScePfsCrypt *ctx,
				SceUInt64 oldDisk)	/* name unknown */
{
	SceVfsVnode *vp = fi->vp;
	ScePfsVd *vd = vp->core.node_data;
	SceUInt8 hash[PFS_HASH_SIZE], dbHash[PFS_HASH_SIZE];
	char name[PFS_NAME_MAX];
	SceUInt64 dbSize, size, newDisk;
	SceSize nameLen;
	ScePfsDbKey key;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	ctx->getHash(ctx, hash);
	vfsGetVnodeName(vp, name, sizeof(name), &nameLen);
	key.parentIndex = vd->parentIndex;
	key.name = name;
	key.mnt = vd->lowerVp->core.mnt;
	ret = pfsDbFind(pmi, &key, dbHash, NULL, NULL, &dbSize);
	if (ret == SCE_PFS_ERROR_NOT_FOUND)
		return 0;
	if (ret != 0)
		return ret;
	if (vp->core.size == dbSize && pfsHashEqual(hash, dbHash))
		return 0;
	pfsVnodeDiskSize(pmi, vp, &newDisk);
	size = vp->core.size;
	ret = pfsDbUpdate(pmi, &key, hash, NULL, &size, &dbSize, newDisk - oldDisk);
	if (ret != 0)
		return ret;
	pfsHashCopy(vd->hash, hash);
	return 0;
}

int pfsCoreWrite(ScePfsFi *fi, ScePfsPmi *pmi, const void *buf, SceSize nbyte, SceOff offset,
		 SceSize *written)
{
	SceVfsVnode *vp = fi->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = vp->core.node_data;
	SceUInt64 oldDisk;
	ScePfsCrypt ctx;
	SceOff size, end;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	pfsVnodeDiskSize(pmi, vp, &oldDisk);
	if (vd->pages.next != &vd->pages)
		return SCE_PFS_ERROR_MAPPED;
	if (info->icvType == PFS_ICV_UNICV)
		ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, vp->core.size, vd->hash);
	else
		ret = pfsCryptInit(&ctx, pmi, &fi->icv, vd->index, vd->attr, vp->core.size, vd->hash);
	if (ret != 0)
		return ret;

	size = vp->core.size;
	if (pmi->quota != 0) {
		end = offset + nbyte;
		if (end < size)
			end = size;
		if (end - size > 0) {
			ret = pfsQuotaCheckGrow(pmi, &ctx, size, end - size);
			if (ret != 0)
				goto out;
			size = vp->core.size;
		}
	}
	if (size < offset) {
		/* extend to the write's start first */
		ret = pfsFileResize(&fi->lower, vd->index, size, vd->attr, pmi, &ctx, offset);
		if (ret != 0)
			goto out;
		vp->core.size = offset;
		if (!info->noCreate) {
			ret = pfsCoreCommit(pmi, fi, &ctx, oldDisk);
			if (ret != 0)
				goto out;
			pfsVnodeDiskSize(pmi, vp, &oldDisk);
		}
	}
	ret = pfsFileWrite(&fi->lower, vd->index, vp->core.size, vd->attr, pmi, &ctx, buf, nbyte,
			   offset, written);
	if (ret == 0) {
		end = offset + *written;
		if (end < (SceOff)vp->core.size)
			end = vp->core.size;
		vp->core.size = end;
		if (!info->noCreate)
			ret = pfsCoreCommit(pmi, fi, &ctx, oldDisk);
	}
out:
	ctx.fini(&ctx);
	pfsCryptClear(&ctx);
	return ret;
}

int pfsCoreLseek(ScePfsFi *fi, ScePfsPmi *pmi, SceOff offset, int whence, SceOff *position)
{
	if (whence == SCE_SEEK_CUR) {
		offset += *position;
	} else if (whence == SCE_SEEK_END) {
		offset += fi->vp->core.size;
		if (offset < 0)
			return SCE_ERROR_ERRNO_EINVAL;
	} else if (whence != SCE_SEEK_SET) {
		return SCE_ERROR_ERRNO_EINVAL;
	}
	*position = offset;
	fi->position = offset;
	return 0;
}

int pfsCoreRemove(ScePfsPmi *pmi, ScePfsNode *dir, ScePfsNode *node)
{
	SceVfsVnode *dvp = dir->vp, *vp = node->vp;
	SceUInt64 oldDir = 0, newDir = 0, fileDisk = 0;
	ScePfsNode lowerDir, lower;
	ScePfsVd *vd;
	int ret;

	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	vd = vp->core.node_data;
	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (dvp == NULL) {
		pfsVnodeDiskSize(pmi, vp, &fileDisk);
		lowerDir.vp = NULL;
	} else {
		if (dvp->core.node_data == NULL)
			return SCE_ERROR_ERRNO_EINVAL;
		pfsVnodeDiskSize(pmi, dvp, &oldDir);
		pfsVnodeDiskSize(pmi, vp, &fileDisk);
		lowerDir.vp = ((ScePfsVd *)dvp->core.node_data)->lowerVp;
	}
	lower.vp = vd->lowerVp;
	lower.name = node->name;
	lowerDir.name = NULL;
	ret = pfsNodeRemove(&lowerDir, &lower);
	if (ret != 0)
		return ret;
	if (dvp == NULL) {
		pfsCoreRemoveEntry(pmi, NULL, node, vd->index, vd->attr);
	} else {
		ret = pfsCoreUpdateSize(pmi, dvp);
		if (ret != 0)
			return ret;
		pfsCoreRemoveEntry(pmi, dvp, node, vd->index, vd->attr);
		if (dvp->core.node_data == NULL)
			return SCE_ERROR_ERRNO_EINVAL;
		pfsVnodeDiskSize(pmi, dvp, &newDir);
	}
	if (vd->attr & PFS_ATTR_ICV_RENAMED)
		return pfsDbAddUsage(pmi, -fileDisk);
	return pfsDbAddUsage(pmi, newDir - oldDir - fileDisk);
}

int pfsCoreMkdir(ScePfsPmi *pmi, ScePfsNode *node, int mode)
{
	SceVfsVnode *dvp = node->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd = dvp->core.node_data;
	SceUInt64 size = PFS_PAGE_SIZE, oldDisk, dirDisk, newDisk;
	SceVfsVnode *lowerDvp, *lowerVp, *icvVp, *vp;
	char name[PFS_NAME_MAX];
	ScePfsNode lower, dir;
	SceUInt32 index;
	SceUInt16 attr;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerDvp = vd->lowerVp;
	pfsVnodeDiskSize(pmi, dvp, &oldDisk);
	ret = pfsQuotaCheckCreate(pmi);
	if (ret != 0)
		return ret;
	if (!info->noCreate) {
		ret = pfsCoreCreateEntry(pmi, node, mode, 0, &index, PFS_ATTR_DIR, &attr);
		if (ret != 0)
			return ret;
	} else {
		ret = pfsCoreFind(pmi, node, &index, &attr, &size);
		if (ret != 0)
			return ret;
		if ((SceInt16)attr >= 0)
			return SCE_PFS_ERROR_NOT_DIR;
	}

	lower.vp = lowerDvp;
	lower.name = node->name;
	ret = pfsNodeMkdir(&lower, info->dirMode);
	lowerVp = lower.vp;
	if (ret != 0)
		goto removeEntry;
	if (info->icvType == PFS_ICV_UNICV) {
		icvVp = NULL;
	} else {
		snprintf(name, sizeof(name), "%08x.icv", index);
		lower.vp = pmi->icvDirVp;
		lower.name = name;
		ret = pfsNodeLookup(&lower);
		icvVp = lower.vp;
		if (ret != 0)
			goto removeDir;
	}
	ret = pfsVnodeCreate(&vp, dvp, lowerVp, icvVp, index, vd->index, pfsAdjustAttr(pmi, attr),
			     0, size);
	if (ret == 0) {
		if (info->noCreate)
			goto done;
		ret = pfsCoreUpdateSize(pmi, dvp);
		if (ret != 0)
			goto removeDir;
		pfsVnodeDiskSize(pmi, dvp, &dirDisk);
		pfsVnodeDiskSize(pmi, vp, &newDisk);
		ret = pfsDbAddUsage(pmi, dirDisk + newDisk - oldDisk);
		if (ret == 0)
			goto done;
		lower.vp = vp;
		lower.name = NULL;
		pfsNodeInactive(&lower);
	}
	if (info->icvType != PFS_ICV_UNICV) {
		snprintf(name, sizeof(name), "%08x.icv", index);
		dir.vp = pmi->icvDirVp;
		dir.name = name;
		lower.vp = icvVp;
		lower.name = name;
		pfsNodeRemove(&dir, &lower);
		pfsNodeInactive(&lower);
	}
removeDir:
	lower.name = node->name;
	dir.name = NULL;
	dir.vp = lowerDvp;
	lower.vp = lowerVp;
	pfsNodeRmdir(&dir, &lower);
	pfsNodeInactive(&lower);
removeEntry:
	if (!info->noCreate)
		pfsCoreRemoveEntry(pmi, dvp, node, index, attr);
	return ret;
done:
	node->vp = vp;
	return 0;
}

int pfsCoreRmdir(ScePfsPmi *pmi, ScePfsNode *dir, ScePfsNode *node)
{
	SceVfsVnode *dvp = dir->vp, *vp = node->vp;
	SceUInt64 oldDir = 0, newDir = 0, oldDisk = 0;
	ScePfsNode lowerDir, lower;
	ScePfsVd *vd, *dvd;
	int ret;

	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	vd = vp->core.node_data;
	if (vd == NULL || (dvd = dvp->core.node_data) == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	pfsVnodeDiskSize(pmi, dvp, &oldDir);
	pfsVnodeDiskSize(pmi, vp, &oldDisk);
	lowerDir.vp = dvd->lowerVp;
	lower.vp = vd->lowerVp;
	lower.name = node->name;
	lowerDir.name = NULL;
	ret = pfsNodeRmdir(&lowerDir, &lower);
	if (ret != 0)
		return ret;
	ret = pfsCoreUpdateSize(pmi, dvp);
	if (ret != 0)
		return ret;
	ret = pfsCoreRemoveEntry(pmi, dvp, node, vd->index, vd->attr);
	if (ret != 0)
		return ret;
	pfsVnodeDiskSize(pmi, dvp, &newDir);
	return pfsDbAddUsage(pmi, newDir - oldDir - oldDisk);
}

int pfsCoreRename(ScePfsPmi *pmi, ScePfsNode *oldDir, ScePfsNode *oldNode, ScePfsNode *newNode)
{
	SceVfsVnode *odvp = oldDir->vp, *ovp = oldNode->vp, *ndvp = newNode->vp;
	SceVfsVnode *lowerOvp, *lowerNdvp, *icvVp, *nvp, *lowerNvp;
	ScePfsVd *odvd, *ovd, *ndvd;
	SceUInt8 hash[PFS_HASH_SIZE];
	SceUInt64 oldDisk, newDisk, a, b, size;
	ScePfsNode lowerOdir, lowerOld, lowerNew;
	SceVfsFile *file;
	ScePfsDbKey key;
	SceUInt32 index, oldParent, newParent;
	SceUInt16 attr;
	SceVfsMount *mnt;
	int ret, err;

	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	odvd = odvp->core.node_data;
	ovd = ovp->core.node_data;
	ndvd = ndvp->core.node_data;
	if (ovd == NULL || odvd == NULL || ndvd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (odvp == ndvp) {
		pfsVnodeDiskSize(pmi, odvp, &oldDisk);
	} else {
		pfsVnodeDiskSize(pmi, ndvp, &a);
		pfsVnodeDiskSize(pmi, odvp, &b);
		oldDisk = a + b;
	}

	newParent = ndvd->index;
	mnt = odvd->lowerVp->core.mnt;
	oldParent = odvd->index;
	lowerNdvp = ndvd->lowerVp;
	lowerOvp = ovd->lowerVp;
	icvVp = ovd->icvVp;
	key.parentIndex = oldParent;
	key.name = oldNode->name;
	key.mnt = mnt;
	err = pfsDbFind(pmi, &key, hash, &index, &attr, &size);
	if (err != 0)
		return err;
	key.parentIndex = newParent;
	key.name = newNode->name;
	key.mnt = odvd->lowerVp->core.mnt;
	ret = pfsDbInsert(pmi, &key, hash, index, attr, size);
	if (ret != 0)
		return ret;

	lowerOdir.vp = odvd->lowerVp;
	lowerOdir.name = NULL;
	lowerOld.vp = lowerOvp;
	lowerOld.name = oldNode->name;
	lowerNew.vp = lowerNdvp;
	lowerNew.name = newNode->name;
	ret = pfsNodeRename(&lowerOdir, &lowerOld, &lowerNew);
	if (ret != 0)
		goto removeNew;
	lowerNvp = lowerNew.vp;
	ret = pfsVnodeCreate(&nvp, ndvp, lowerNvp, icvVp, index, newParent,
			     pfsAdjustAttr(pmi, attr), 0, size);
	if (ret != 0)
		goto releaseNew;
	key.parentIndex = oldParent;
	key.name = oldNode->name;
	key.mnt = odvd->lowerVp->core.mnt;
	ret = pfsDbRemove(pmi, &key);
	if (ret != 0) {
		lowerNew.vp = nvp;
		lowerNew.name = NULL;
		pfsNodeInactive(&lowerNew);
		goto releaseNew;
	}

	/* the open files of the old vnode move to the new one */
	vfsLockVnode(lowerNvp);
	vfsLockVnode(lowerOvp);
	lowerNvp->core.fd_list = lowerOvp->core.fd_list;
	lowerNvp->core.fd_num = lowerOvp->core.fd_num;
	lowerNvp->core.bc = lowerOvp->core.bc;
	for (file = lowerOvp->core.fd_list; file != NULL; file = file->next)
		file->vp = lowerNvp;
	lowerOvp->core.fd_list = NULL;
	lowerOvp->core.fd_num = 0;
	lowerOvp->core.bc = NULL;
	vfsUnlockVnode(lowerOvp);
	vfsUnlockVnode(lowerNvp);
	if (icvVp != NULL) {
		vfsLockVnode(icvVp);
		icvVp->core.ref_count++;
		vfsUnlockVnode(icvVp);
	}
	for (file = ovp->core.fd_list; file != NULL; file = file->next) {
		((ScePfsFi *)file->fd)->vp = nvp;
		((ScePfsFi *)file->fd)->lower.vp = lowerNvp;
	}

	if (odvp == ndvp) {
		pfsCoreUpdateSize(pmi, odvp);
		pfsVnodeDiskSize(pmi, odvp, &newDisk);
	} else {
		pfsCoreUpdateSize(pmi, ndvp);
		pfsCoreUpdateSize(pmi, odvp);
		pfsVnodeDiskSize(pmi, ndvp, &a);
		pfsVnodeDiskSize(pmi, odvp, &b);
		newDisk = a + b;
	}
	ret = pfsDbAddUsage(pmi, newDisk - oldDisk);
	newNode->vp = nvp;
	return ret;

releaseNew:
	lowerNew.vp = lowerNvp;
	lowerNew.name = NULL;
	pfsNodeInactive(&lowerNew);
removeNew:
	key.parentIndex = newParent;
	key.name = newNode->name;
	key.mnt = odvd->lowerVp->core.mnt;
	pfsDbRemove(pmi, &key);
	return ret;
}

int pfsCoreGetAttr(ScePfsPmi *pmi, ScePfsNode *node, SceUInt16 *attr, SceUInt64 *size)
{
	ScePfsVd *vd = node->vp->core.node_data;
	ScePfsDbKey key;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	key.parentIndex = vd->parentIndex;
	key.name = node->name;
	key.mnt = vd->lowerVp->core.mnt;
	ret = pfsDbFind(pmi, &key, NULL, NULL, attr, size);
	if (ret == 0 && attr != NULL)
		*attr = pfsAdjustAttr(pmi, *attr);
	return ret;
}

int pfsCoreTruncate(ScePfsFi *fi, ScePfsPmi *pmi, SceOff newSize)
{
	SceVfsVnode *vp = fi->vp;
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsVd *vd;
	SceUInt64 oldDisk;
	ScePfsCrypt ctx;
	SceOff size;
	SceUInt32 pad;
	int ret;

	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	if (newSize < 0 || (vd = vp->core.node_data) == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	pfsVnodeDiskSize(pmi, vp, &oldDisk);
	if (vd->pages.next != &vd->pages)
		return SCE_PFS_ERROR_MAPPED;
	if (info->icvType == PFS_ICV_UNICV)
		ret = pfsCryptInit(&ctx, pmi, NULL, vd->index, vd->attr, vp->core.size, vd->hash);
	else
		ret = pfsCryptInit(&ctx, pmi, &fi->icv, vd->index, vd->attr, vp->core.size, vd->hash);
	if (ret != 0)
		return ret;

	size = vp->core.size;
	if (pmi->quota != 0 && newSize - size > 0) {
		ret = pfsQuotaCheckGrow(pmi, &ctx, size, newSize - size);
		if (ret != 0)
			goto out;
		size = vp->core.size;
	}
	if (newSize < size) {
		/* the underlying file keeps whole pages */
		pad = (PFS_PAGE_SIZE - newSize % PFS_PAGE_SIZE) % PFS_PAGE_SIZE;
		ret = pfsLowerSetSize(&fi->lower, newSize + pad);
		if (ret != 0)
			goto out;
		size = vp->core.size;
	}
	ret = pfsFileResize(&fi->lower, vd->index, size, vd->attr, pmi, &ctx, newSize);
	if (ret == 0) {
		vp->core.size = newSize;
		if (!info->noCreate)
			ret = pfsCoreCommit(pmi, fi, &ctx, oldDisk);
	}
out:
	ctx.fini(&ctx);
	pfsCryptClear(&ctx);
	return ret;
}

int pfsCoreSetAttr(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 attr)
{
	ScePfsVd *vd = node->vp->core.node_data;
	SceUInt16 newAttr = attr;
	ScePfsDbKey key;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	key.parentIndex = vd->parentIndex;
	key.name = node->name;
	key.mnt = vd->lowerVp->core.mnt;
	return pfsDbUpdate(pmi, &key, NULL, &newAttr, NULL, NULL, 0);
}

int pfsCoreVnodeSync(ScePfsFi *fi, ScePfsPmi *pmi, int flags)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	int ret = 0;

	if (!info->noCreate && info->icvType != PFS_ICV_UNICV)
		ret = pfsLowerSync(&fi->icv);
	if (!info->noSync)
		ret = pfsLowerSync(&fi->lower);
	return ret;
}

int pfsCoreDopen(ScePfsFi *fi, ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path)
{
	ScePfsVd *vd = vp->core.node_data;
	SceVfsVnode *lowerVp;
	SceVfsFile *file;
	SceUID fd;
	int ret;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerVp = vd->lowerVp;
	vfsLockVnode(lowerVp);
	fd = vfsAllocateFile(lowerVp, &file, path->name);
	vfsUnlockVnode(lowerVp);
	if (fd < 0)
		return fd;
	vfsLockVnode(lowerVp);
	ret = ksceVopDopen(lowerVp, path, file);
	vfsUnlockVnode(lowerVp);
	if (ret != 0) {
		vfsLockVnode(lowerVp);
		vfsFreeFile(lowerVp, fd);
		vfsUnlockVnode(lowerVp);
		return ret;
	}
	fi->lower.vp = lowerVp;
	fi->lower.fd = fd;
	fi->lower.cookie = vd->index;
	fi->vp = vp;
	fi->lower.file = file;
	return 0;
}

int pfsCoreDread(ScePfsFi *fi, ScePfsPmi *pmi, SceIoDirent *dir, int a3)
{
	SceUInt64 size;
	ScePfsDbKey key;
	SceUInt32 mode;
	SceUInt16 attr;
	int count, ret;

	do {
		for (;;) {
			vfsLockVnode(fi->lower.vp);
			count = ksceVopDread(fi->lower.vp, fi->lower.file, dir);
			vfsUnlockVnode(fi->lower.vp);
			if (count <= 0)
				return count;
			key.parentIndex = fi->lower.cookie;
			key.mnt = fi->lower.vp->core.mnt;
			key.name = dir->d_name;
			ret = pfsDbFind(pmi, &key, NULL, NULL, &attr, &size);
			if (ret == 0)
				break;
			if (ret != SCE_PFS_ERROR_NOT_FOUND)
				return ret;
		}
		attr = pfsAdjustAttr(pmi, attr);
	} while ((a3 == 0x20 || a3 == 0x10) && pmi->mode != 9 && pmi->mode != 7 &&
		 (attr & 6) == 6);

	attr = pfsAdjustAttr(pmi, attr);
	ret = pfsAttrToMode(attr, &mode);
	if (ret != 0)
		return ret;
	dir->d_stat.st_mode = mode & 0xff0fffff;
	dir->d_stat.st_attr = attr;
	dir->d_stat.st_size = size;
	return count;
}

int pfsCoreDclose(ScePfsFi *fi, ScePfsPmi *pmi)
{
	int ret;

	if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
		vfsLockVnode(fi->lower.vp);
		ksceVopDclose(fi->lower.vp, fi->lower.file);
		vfsUnlockVnode(fi->lower.vp);
	} else {
		vfsLockVnode(fi->lower.vp);
		ret = ksceVopDclose(fi->lower.vp, fi->lower.file);
		vfsUnlockVnode(fi->lower.vp);
		if (ret != 0)
			return ret;
	}
	vfsLockVnode(fi->lower.vp);
	vfsFreeFile(fi->lower.vp, fi->lower.fd);
	vfsUnlockVnode(fi->lower.vp);
	return 0;
}

int pfsCoreDevctl(ScePfsPmi *pmi, const char *dev, unsigned int cmd, const void *arg,
		  SceSize argLen, void *buf, SceSize bufLen)
{
	SceUInt64 usage, disk;
	int ret;

	switch (cmd) {
	case 0x4411:	/* set the quota */
		if (argLen != 4 || arg == NULL)
			break;
		ret = pfsDbGetUsage(pmi, &usage);
		if (ret != 0 || (ret = pfsPmiDiskSize(pmi, &disk)) != 0)
			return ret;
		if (disk + usage < *(const SceUInt32 *)arg) {
			pmi->quota = *(const SceUInt32 *)arg;
			return 0;
		}
		return SCE_ERROR_ERRNO_ENOSPC;
	case 0x4410:	/* the quota and the space used */
		if (bufLen != 8 || buf == NULL)
			break;
		ret = pfsDbGetUsage(pmi, &usage);
		if (ret != 0 || (ret = pfsPmiDiskSize(pmi, &disk)) != 0)
			return ret;
		((SceUInt32 *)buf)[0] = pmi->quota;
		((SceUInt32 *)buf)[1] = disk + usage;
		return 0;
	case 0x4414:
		if (arg != NULL || argLen != 0 || buf != NULL || bufLen != 0)
			break;
		return pfsDbCheck(pmi);
	case 0x4812:
		if (arg != NULL || argLen != 0 || buf == NULL || bufLen - 1 >= 0x10000)
			break;
		return pfsDbDump(pmi, buf, bufLen);
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

int pfsCoreWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
		    SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5)
{
	const ScePfsModeInfo *info;
	ScePfsVd *vd = vp->core.node_data, *dvd = dvp->core.node_data;
	char oldName[PFS_NAME_MAX], newName[PFS_NAME_MAX];
	SceVfsPath icvOld, icvNew;
	SceVfsVnode *lowerDvp, *lowerVp;
	SceUInt64 oldDisk, newDisk;
	ScePfsDbKey key;
	SceUInt32 index;
	int ret;

	if (pmi->mode == 3)
		return SCE_PFS_ERROR_READ_ONLY;
	if (vd == NULL || dvd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerDvp = dvd->lowerVp;
	lowerVp = vd->lowerVp;
	pfsVnodeDiskSize(pmi, dvp, &oldDisk);
	vfsLockVnode(lowerDvp);
	vfsLockVnode(lowerVp);
	ret = ksceVopWhiteout(lowerDvp, lowerVp, oldPath, newPath, a4, a5);
	vfsUnlockVnode(lowerVp);
	vfsUnlockVnode(lowerDvp);
	if (ret != 0)
		return ret;

	info = pfsModeInfo(pmi->mode);
	dvd = dvp->core.node_data;
	if (dvd != NULL) {
		key.parentIndex = dvd->index;
		key.name = oldPath->name;
		key.mnt = dvd->lowerVp->core.mnt;
		pfsDbRemove(pmi, &key);
		if (info->icvType != PFS_ICV_UNICV) {
			/* keep the icv file under a fresh name */
			snprintf(oldName, sizeof(oldName) - 1, "%08x.icv", vd->index);
			icvOld.name = oldName;
			icvOld.name_length = strnlen(oldName, sizeof(oldName) - 1);
			index = vd->index;
			for (;;) {
				snprintf(newName, sizeof(newName) - 1, "%08x.icv", index);
				icvNew.name = newName;
				icvNew.name_length = strnlen(newName, sizeof(newName) - 1);
				vfsLockVnode(pmi->icvDirVp);
				vfsLockVnode(vd->icvVp);
				ret = ksceVopWhiteout(pmi->icvDirVp, vd->icvVp, &icvOld, &icvNew, a4,
						      a5);
				vfsUnlockVnode(vd->icvVp);
				vfsUnlockVnode(pmi->icvDirVp);
				if (ret != SCE_ERROR_ERRNO_EEXIST)
					break;
				index = pfsRandom();
			}
			if (ret == 0) {
				vd->index = index;
				vd->attr |= PFS_ATTR_ICV_RENAMED;
			}
		}
	}
	pfsCoreUpdateSize(pmi, dvp);
	pfsVnodeDiskSize(pmi, dvp, &newDisk);
	return pfsDbAddUsage(pmi, newDisk - oldDisk);
}

int pfsCoreCleanup(ScePfsFi *fi, ScePfsPmi *pmi, SceUInt32 a2, SceUInt32 a3)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);

	if (info->icvType != PFS_ICV_UNICV && pfsLowerCleanup(&fi->icv, a2, a3) != 0)
		return 0;
	pfsLowerCleanup(&fi->lower, a2, a3);
	return 0;
}
