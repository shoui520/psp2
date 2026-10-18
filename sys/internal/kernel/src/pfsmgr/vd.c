/*
 * PFS vnodes' data (Vd), the map ioctls, reads, and where icv trees live:
 * file indexes, unicv.db, and setting up a file's icv tree for checking.
 */

#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "pfsmgr_internal.h"

#define PFS_MAP			0x4420	/* name unknown */
#define PFS_MAP_SEQUENTIAL	0x4421	/* name unknown */
#define PFS_UNMAP		0x4422	/* name unknown */
#define PFS_NAME_MAX		0x40

#define SCE_PFS_ERROR_NOT_PERMITTED	0x80142010	/* name unknown */
#define SCE_PFS_ERROR_BAD_ICV_TYPE	0x80142009	/* name unknown */

ScePfsBufferList g_pfsVdBufferList;
extern SceUID g_pfsThreadMappingKey;

int pfsVdInit(void)
{
	return pfsBufferListInit(&g_pfsVdBufferList, "ScePfsVdBufferList", sizeof(ScePfsVd), 4,
				 0x800);
}

void pfsVdTerm(void)
{
	pfsBufferListTerm(&g_pfsVdBufferList);
}

int pfsCoreIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, ScePfsFi *fi, int cmd, const void *in,
		 SceSize inLen, void *out, SceSize outLen)
{
	ScePfsBlockRequest *map = out;
	ScePfsListNode *node;
	ScePfsVd *vd;

	if (cmd >= PFS_MAP && cmd <= PFS_MAP_SEQUENTIAL) {
		if (in != NULL && inLen == sizeof(ScePfsMapArg) && out != NULL &&
		    outLen == sizeof(ScePfsBlockRequest))
			return pfsCoreMap(pmi, vp, fi, cmd, (ScePfsMapArg *)in, map);
	} else if (cmd == PFS_UNMAP && inLen == 0 && in == NULL && out != NULL &&
		   outLen == sizeof(ScePfsBlockRequest) && (vd = vp->core.nodeData) != NULL) {
		for (node = vd->pages.next; node != &vd->pages; node = node->next) {
			if (node == &map->link) {
				map->link.next->prev = map->link.prev;
				map->link.prev->next = map->link.next;
				return 0;
			}
		}
		return 0;
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

int pfsCoreRead(ScePfsFi *fi, ScePfsPmi *pmi, void *buf, SceSize nbyte, SceOff offset,
		SceSize *read)
{
	ScePfsBlockRequest *map;

	if (pmi->mode != 3) {
		map = *pfsThreadMapping();
		if (map == NULL)
			return pfsCoreReadData(fi, pmi, buf, nbyte, offset, read);
		if (map->op == PFS_BLOCK_NONE)
			return SCE_PFS_ERROR_PASS_THROUGH;
		if (map->op == PFS_BLOCK_READ)
			return pfsBlockCopyOut(map, buf, nbyte, offset, read);
	}
	return SCE_PFS_ERROR_NOT_PERMITTED;
}

int pfsUnicvOpen(ScePfsPmi *pmi, ScePfsNode *node, int create)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsNode dir;
	SceUID pid;
	int ret;

	if (create) {
		ret = pfsNodeCreate(node, info->fileMode);
		if (ret < 0)
			return ret;
	}
	pid = SceThreadmgrForDriver_0486F239(PFS_KERNEL_PID);
	ret = pfsLowerOpen(&pmi->unicvDb, node, 0, info->unicvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret == 0) {
		ret = pfsUnicvInit(&pmi->unicv, &pmi->icvLru, &pmi->unicvDb,
				   pfsVersionUnicvParam(pmi->version));
		if (ret == 0)
			return 0;
		pfsLowerClose(&pmi->unicvDb);
	}
	if (create) {
		dir.vp = pmi->sceDirVp;
		dir.name = NULL;
		pfsNodeRemove(&dir, node);
		pfsNodeInactive(node);
	}
	return ret;
}

int pfsUnicvClose(ScePfsPmi *pmi)
{
	ScePfsNode node;
	int ret;

	if (!(pmi->flags & PFS_PMI_NOT_MOUNTED)) {
		ret = pfsUnicvTerm(&pmi->unicv);
		if (ret != 0)
			return ret;
		node.vp = pmi->unicvDb.vp;
		node.name = NULL;
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED)) {
			ret = pfsLowerClose(&pmi->unicvDb);
			if (ret == 0)
				ret = pfsNodeInactive(&node);
			return ret;
		}
	} else {
		node.vp = pmi->unicvDb.vp;
		node.name = NULL;
	}
	pfsLowerCloseForce(&pmi->unicvDb);
	pfsNodeRelease(&node);
	return 0;
}

int pfsIndexAlloc(ScePfsPmi *pmi, SceUInt32 *index)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	char name[PFS_NAME_MAX];
	ScePfsNode node;
	SceUInt32 r;
	int ret;

	if (info->icvType == PFS_ICV_UNICV) {
		vfsLockVnode(pmi->unicvDb.vp);
		ret = pfsUnicvAllocIndex(&pmi->unicv, index);
		vfsUnlockVnode(pmi->unicvDb.vp);
		return ret;
	}

	vfsLockVnode(pmi->icvDirVp);
	for (;;) {
		do
			r = pfsRandom();
		while (r - 1 > 0xfffffffd);
		snprintf(name, sizeof(name), "%08x.icv", r);
		node.vp = pmi->icvDirVp;
		node.name = name;
		ret = pfsNodeLookup(&node);
		if (ret != 0)
			break;
		pfsNodeInactive(&node);
	}
	if (ret == SCE_ERROR_ERRNO_ENOENT && (ret = pfsNodeCreate(&node, info->fileMode)) == 0) {
		*index = r;
		pfsNodeInactive(&node);
	}
	vfsUnlockVnode(pmi->icvDirVp);
	return ret;
}

int pfsIndexFree(ScePfsPmi *pmi, SceUInt32 index)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	char name[PFS_NAME_MAX];
	ScePfsNode dir, node;
	int ret = 0;

	if (info->icvType != PFS_ICV_UNICV) {
		snprintf(name, sizeof(name), "%08x.icv", index);
		dir.vp = pmi->icvDirVp;
		dir.name = NULL;
		node.vp = pmi->icvDirVp;
		node.name = name;
		ret = pfsNodeLookup(&node);
		if (ret == 0) {
			ret = pfsNodeRemove(&dir, &node);
			pfsNodeInactive(&node);
		}
	}
	return ret;
}

static inline void pfsCryptZero(ScePfsCrypt *ctx)	/* name unknown */
{
	ctx->fini = NULL;
	ctx->getIcv = NULL;
	ctx->setIcv = NULL;
	ctx->getCount = NULL;
	ctx->resize = NULL;
	ctx->blockSize = NULL;
	ctx->getHash = NULL;
	ctx->hashBlock = NULL;
	ctx->icvSize = NULL;
	ctx->lf = NULL;
	ctx->lru = NULL;
}

/* a new file's icv tree: its first hash */
int pfsIcvCreate(ScePfsPmi *pmi, SceUInt32 index, SceUInt16 attr, int mode, void *hash)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsHash key;
	char name[PFS_NAME_MAX];
	ScePfsLowerFile lf;
	ScePfsCrypt ctx;
	ScePfsNode node;
	SceUInt32 icvType;
	int ret;

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, index, pmi->a6);
	icvType = info->icvType;
	if (icvType == PFS_ICV_UNICV) {
		ctx.key = key;
		ctx.kind = 0;
		ret = pfsUnicvTreeCreate(&ctx, &pmi->unicv, index, mode, &key);
		goto done;
	}

	snprintf(name, sizeof(name), "%08x.icv", index);
	node.vp = pmi->icvDirVp;
	node.name = name;
	ret = pfsNodeLookup(&node);
	if (ret != 0)
		return ret;
	ret = pfsLowerOpen(&lf, &node, index, PFS_OPEN_KERNEL | SCE_O_RDWR);
	if (ret != 0)
		return ret;
	ctx.kind = icvType;
	if (icvType == PFS_ICV_FILES) {
		if ((attr & 0x2000) || (attr & 0x8000)) {
			ctx.kind = 2;
			goto flat;
		}
		ctx.key = key;
		ret = pfsIcvTreeCreate(&ctx, &pmi->icvLru, &lf, mode, &key,
				       pfsVersionIcvParam(pmi->version));
	} else {
		ctx.key = key;
		if (icvType == 2) {
		flat:
			ctx.key = key;
			icvType = 2;
			ret = pfsIcvFlatCreate(&ctx, &lf, mode, &key, pfsVersionHashParam(pmi->version));
		} else if (icvType == 3) {
			ret = pfsUnicvTree3Create(&ctx, &pmi->unicv, index, mode);
		} else {
			ret = SCE_PFS_ERROR_BAD_ICV_TYPE;
			goto close;
		}
	}
done:
	if (ret == 0) {
		ctx.getHash(&ctx, hash);
		ctx.fini(&ctx);
		pfsCryptZero(&ctx);
		memset(&ctx.key, 0, sizeof(ctx.key));
		if (ctx.kind == 0)
			pfsUnicvTreeTerm(&ctx);
		else if (ctx.kind == 3)
			pfsUnicvTree3Term(&ctx);
	}
	if (icvType == PFS_ICV_UNICV)
		return ret;
close:
	node.vp = lf.vp;
	node.name = NULL;
	pfsLowerClose(&lf);
	pfsNodeInactive(&node);
	return ret;
}

/* set ctx up for the file's icv tree and check it against the hash in files.db */
int pfsCryptInit(ScePfsCrypt *ctx, ScePfsPmi *pmi, ScePfsLowerFile *icv, SceUInt32 index,
		 SceUInt16 attr, SceUInt64 size, const void *hash)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsHash key;
	SceUInt8 current[PFS_HASH_SIZE];
	SceUInt32 kind;
	int ret;

	pfsDeriveKey(&key, pmi->klicensee, pmi->a8, pmi->flags, index, pmi->a6);
	kind = info->icvType;
	if (kind == PFS_ICV_FILES) {
		if ((attr & 0x2000) || (attr & 0x8000))
			kind = 2;
		else
			kind = 1;
	} else if (kind == PFS_ICV_UNICV) {
		if (!(attr & 0x400)) {
			ctx->key = key;
			ctx->kind = 0;
			ret = pfsUnicvTreeOpen(ctx, &pmi->unicv, index, &key, size);
		} else {
			ctx->kind = 3;
			ctx->key = key;
			ret = pfsUnicvTree3Open(ctx, &pmi->unicv, index, &key, size);
		}
		goto check;
	}
	ctx->key = key;
	ctx->kind = kind;
	if (kind == 2) {
		ret = pfsIcvFlatOpen(ctx, icv, &key, 0, pfsVersionHashParam(pmi->version));
	} else if (kind == 3) {
		ret = pfsUnicvTree3Open(ctx, &pmi->unicv, icv->cookie, &key, size);
	} else if (kind == 1) {
		ret = pfsIcvTreeOpen(ctx, &pmi->icvLru, icv, &key, size,
				     pfsVersionIcvParam(pmi->version));
	} else {
		return SCE_PFS_ERROR_BAD_ICV_TYPE;
	}
check:
	if (ret == 0) {
		ctx->getHash(ctx, current);
		if (!pfsHashEqual(hash, current))
			ret = SCE_PFS_ERROR_HASH;
	}
	return ret;
}

void pfsCryptClear(ScePfsCrypt *ctx)
{
	pfsCryptZero(ctx);
	memset(&ctx->key, 0, sizeof(ctx->key));
	if (ctx->kind == 0)
		pfsUnicvTreeTerm(ctx);
	else if (ctx->kind == 3)
		pfsUnicvTree3Term(ctx);
}

/* the files.db indexes of path's last component and its directory */
int pfsPathResolve(ScePfsPmi *pmi, ScePfsVd *vd, const char *path)
{
	const char *slash;
	char name[PFS_NAME_MAX];
	SceUInt32 index = 0, parentIndex = (SceUInt32)-1, last;
	SceUInt16 attr = 0;
	ScePfsDbKey key;
	int depth = 0, perm, ret;

	while (path != NULL) {
		slash = strchr(path + 1, '/');
		if (slash == NULL) {
			strncpy(name, path + 1, sizeof(name));
		} else {
			pfsMemcpyChecked(name, path + 1, slash - path, sizeof(name));
			name[slash - path - 1] = '\0';
		}
		last = index;
		if (depth > 1) {
			key.parentIndex = index;
			key.name = name;
			key.mnt = vd->lowerVp->core.mnt;
			perm = sceKernelSetThreadAccessLevel(0x40);
			ret = pfsDbFind(pmi, &key, NULL, &index, &attr, NULL);
			sceKernelSetThreadAccessLevel(perm);
			parentIndex = last;
			if (ret != 0)
				return ret;
		}
		depth++;
		path = slash;
	}
	vd->index = index;
	vd->parentIndex = parentIndex;
	vd->attr = attr;
	vd->icvVp = NULL;
	return 0;
}
