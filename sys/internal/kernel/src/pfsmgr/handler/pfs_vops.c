/*
 * handler/pfs_vops.c (file name estimated: no debug record names this object) -- the vfs and vnode
 * operation entries of the gamedata/savedata drives: each takes the pmi of the mount,
 * refuses an unmounted one, calls the handler/pfops.c function and turns a pfs error into
 * an errno value. Functions in address order. */

#include "pfsmgr_internal.h"

static int pfsGdsdVfsInit(SceVfsOpInitArgs *arg)
{
	return 0;
}

static int pfsGdsdVfsFini(SceVfsOpFiniArgs *arg)
{
	return 0;
}

static int pfsGdsdVfsDevctl(SceVfsOpDevctlArg *arg)
{
	ScePfsPmi *pmi = (ScePfsPmi *)arg->mnt->pData;

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdDevctl(pmi, arg->mnt, arg->dev, arg->cmd, arg->arg,
						     arg->argLen, arg->buf, arg->bufLen));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVfsMount(SceVfsOpMountArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdMount(arg->mnt->pData, arg->mnt, arg->devFilePath));
}

static int pfsGdsdVfsUmount(SceVfsOpUmountArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdUmount(arg->mnt->pData, arg->mnt, arg->flags));
}

static int pfsGdsdVfsSync(SceVfsOpSyncArgs *arg)
{
	ScePfsPmi *pmi = (ScePfsPmi *)arg->mnt->pData;

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdSync(pmi, arg->mnt, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVfsSetRoot(SceVfsOpSetRootArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdSetRoot(arg->mnt->pData, arg->mnt, arg->unused, arg->vp));
}

static int pfsGdsdVfsDecodePathElem(SceVfsOpDecodePathElemArgs *arg)
{
	return pfsErrorToErrno(pfs_decode_path_elem(arg->mnt->pData, arg->mnt, arg->path, arg->path2,
						     arg->path3, arg->buf, arg->bufLen,
						     arg->decodeLen));
}

static int pfsGdsdVopLookup(SceVopLookupArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdLookup(pmi, arg->dvp, arg->vpp, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopOpen(SceVopOpenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdOpen(pmi, arg->vp, arg->file, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopCreate(SceVopCreateArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdCreate(pmi, arg->dvp, arg->vpp, arg->path, arg->flags,
						     arg->mode));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopClose(SceVopCloseArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdClose(pfsVnodePmi(arg->vp), arg->vp, arg->file));
}

static SceSSize pfsGdsdVopRead(SceVopReadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsGdsdRead(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		if (ret >= 0 || ret == SCE_PFS_ERROR_PASS_THROUGH)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsGdsdVopWrite(SceVopWriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsGdsdWrite(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsGdsdVopPread(SceVopPreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsGdsdPread(pmi, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
		if (ret >= 0 || ret == SCE_PFS_ERROR_PASS_THROUGH)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsGdsdVopPwrite(SceVopPwriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsGdsdPwrite(pmi, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceOff pfsGdsdVopLseek(SceVopLseekArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	SceOff ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsGdsdLseek(pmi, arg->vp, arg->file, arg->offset, arg->whence);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno((int)ret);
}

static int pfsGdsdVopIoctl(SceVopIoctlArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdIoctl(pmi, arg->vp, arg->file, arg->cmd, arg->inData,
						    arg->inLen, arg->outData, arg->outLen));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopInactive(SceVopInactiveArgs *arg)
{
	return pfsErrorToErrno(pfs_inactive(arg->vp));
}

static int pfsGdsdVopRemove(SceVopRemoveArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdRemove(pmi, arg->dvp, arg->vp, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopMkdir(SceVopMkdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdMkdir(pmi, arg->dvp, arg->vpp, arg->path, arg->mode));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopRmdir(SceVopRmdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdRmdir(pmi, arg->dvp, arg->vp, arg->path));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopDopen(SceVopDopenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdDopen(pmi, arg->vp, arg->path, arg->file));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopDread(SceVopDreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdDread(pmi, arg->vp, arg->file, arg->dir));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopDclose(SceVopDcloseArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdDclose(pfsVnodePmi(arg->vp), arg->vp, arg->file));
}

static int pfsGdsdVopGetstat(SceVopGetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdGetstat(pmi, arg->vp, arg->path, arg->stat));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopChstat(SceVopChstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfs_chstat(pmi, arg->vp, arg->path, arg->stat, arg->bit));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopFgetstat(SceVopFgetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdFgetstat(pmi, arg->vp, arg->file, arg->stat));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopFchstat(SceVopFchstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfs_fchstat(pmi, arg->vp, arg->file, arg->stat, arg->bit));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopRename(SceVopRenameArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->odvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdRename(pmi, arg->odvp, arg->ovp, arg->oldPath,
						     arg->ndvp, arg->nvpp, arg->newPath));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopSync(SceVopSyncArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsGdsdVnodeSync(pmi, arg->vp, arg->file, arg->flags));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopWhiteout(SceVopWhiteoutArgs *args)
{
	SceVopWhiteoutArgs *arg = (void *)args;
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsGdsdWhiteout(pmi, arg->dvp, arg->vp, arg->oldPath,
						       arg->newPath, arg->a4, arg->a5));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsGdsdVopCleanup(SceVopCleanupArgs *args)
{
	SceVopCleanupArgs *arg = (void *)args;
	return pfsErrorToErrno(pfsGdsdCleanup(pfsVnodePmi(arg->vp), arg->vp, arg->file,
						      arg->a2, arg->a3));
}

const SceVfsOpTable g_pfsGdsdVfsOps = {
	.vfs_mount		= pfsGdsdVfsMount,
	.vfs_umount		= pfsGdsdVfsUmount,
	.vfs_set_root		= pfsGdsdVfsSetRoot,
	.vfs_sync		= pfsGdsdVfsSync,
	.vfs_init		= pfsGdsdVfsInit,
	.vfs_fini		= pfsGdsdVfsFini,
	.vfs_devctl		= pfsGdsdVfsDevctl,
	.vfs_decode_path_elem	= pfsGdsdVfsDecodePathElem,
};

const SceVopTable g_pfsGdsdVops = {
	.vop_open	= pfsGdsdVopOpen,
	.vop_create	= pfsGdsdVopCreate,
	.vop_close	= pfsGdsdVopClose,
	.vop_lookup	= pfsGdsdVopLookup,
	.vop_read	= pfsGdsdVopRead,
	.vop_write	= pfsGdsdVopWrite,
	.vop_lseek	= pfsGdsdVopLseek,
	.vop_ioctl	= pfsGdsdVopIoctl,
	.vop_remove	= pfsGdsdVopRemove,
	.vop_mkdir	= pfsGdsdVopMkdir,
	.vop_rmdir	= pfsGdsdVopRmdir,
	.vop_dopen	= pfsGdsdVopDopen,
	.vop_dclose	= pfsGdsdVopDclose,
	.vop_dread	= pfsGdsdVopDread,
	.vop_getstat	= pfsGdsdVopGetstat,
	.vop_chstat	= pfsGdsdVopChstat,
	.vop_rename	= pfsGdsdVopRename,
	.vop_pread	= pfsGdsdVopPread,
	.vop_pwrite	= pfsGdsdVopPwrite,
	.vop_inactive	= pfsGdsdVopInactive,
	.vop_sync	= pfsGdsdVopSync,
	.vop_fgetstat	= pfsGdsdVopFgetstat,
	.vop_fchstat	= pfsGdsdVopFchstat,
	.vop_whiteout	= pfsGdsdVopWhiteout,
	.vop_cleanup	= pfsGdsdVopCleanup,
};
