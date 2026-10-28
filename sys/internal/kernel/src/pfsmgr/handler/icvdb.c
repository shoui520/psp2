/*
 * PFS vnodes' data (Vd), the map ioctls, reads, and where icv trees live:
 * file indexes, unicv.db, and setting up a file's icv tree for checking.
 */

#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "pfsmgr_internal.h"

#define SCE_PFS_ERROR_BAD_ICV_TYPE	0x80142009	/* name unknown */

ScePfsBufferList g_pfsVdBufferList;
extern SceUID g_pfsThreadMappingKey;


int mount_flat_icvdb(ScePfsPmi *pmi, ScePfsNode *node, int create)
{
	const ScePfsModeInfo *info = pfsModeInfo(pmi->mode);
	ScePfsNode dir;
	SceUID pid;
	int ret;

	if (create) {
		ret = file_access_create(node, info->fileMode);
		if (ret < 0)
			return ret;
	}
	pid = SceThreadmgrForDriver_0486F239(SCE_KERNEL_PROCESS_ID);
	ret = file_access_open(&pmi->unicvDb, node, 0, info->unicvOpenFlags | PFS_OPEN_KERNEL);
	SceThreadmgrForDriver_0486F239(pid);
	if (ret == 0) {
		ret = pfsUnicvInit(&pmi->unicv, &pmi->icvLru, &pmi->unicvDb,
				   pfsVersionUnicvParam(pmi->version));
		if (ret == 0)
			return 0;
		file_access_close(&pmi->unicvDb);
	}
	if (create) {
		dir.vp = pmi->sceDirVp;
		dir.name = NULL;
		file_access_remove(&dir, node);
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
			ret = file_access_close(&pmi->unicvDb);
			if (ret == 0)
				ret = pfsNodeInactive(&node);
			return ret;
		}
	} else {
		node.vp = pmi->unicvDb.vp;
		node.name = NULL;
	}
	file_access_close_force(&pmi->unicvDb);
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
		ret = file_access_lookup(&node);
		if (ret != 0)
			break;
		pfsNodeInactive(&node);
	}
	if (ret == SCE_ERROR_ERRNO_ENOENT && (ret = file_access_create(&node, info->fileMode)) == 0) {
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
		ret = file_access_lookup(&node);
		if (ret == 0) {
			ret = file_access_remove(&dir, &node);
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

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, index, pmi->a6);
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
	ret = file_access_lookup(&node);
	if (ret != 0)
		return ret;
	ret = file_access_open(&lf, &node, index, PFS_OPEN_KERNEL | SCE_O_RDWR);
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
				       pfsVersionHashParam(pmi->version));
	} else {
		ctx.key = key;
		if (icvType == 2) {
		flat:
			ctx.key = key;
			icvType = 2;
			ret = pfsIcvFlatCreate(&ctx, &lf, mode, &key, pfsVersionIcvParam(pmi->version));
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
	file_access_close(&lf);
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

	pfs_get_secret(&key, pmi->klicensee, pmi->a8, pmi->flags, index, pmi->a6);
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
		ret = pfsIcvFlatOpen(ctx, icv, &key, size, pfsVersionIcvParam(pmi->version));
	} else if (kind == 3) {
		ret = pfsUnicvTree3Open(ctx, &pmi->unicv, icv->cookie, &key, size);
	} else if (kind == 1) {
		ret = pfsIcvTreeOpen(ctx, &pmi->icvLru, icv, &key, size,
				     pfsVersionHashParam(pmi->version));
	} else {
		return SCE_PFS_ERROR_BAD_ICV_TYPE;
	}
check:
	if (ret == 0) {
		ctx->getHash(ctx, current);
		if (!icv_equal(hash, current))
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
int _update_aciddir_vnodedata(ScePfsPmi *pmi, ScePfsVd *vd, const char *path)
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
