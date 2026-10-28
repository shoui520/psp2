/*
 * handler/redirect_vops.c (file name estimated: no debug record names this object) -- the vfs
 * and vnode operation entries of the os0:/vs0: redirection drives. Functions in address
 * order. */

#include "pfsmgr_internal.h"

#define PFS_REDIRECT_ENTRIES	0x20	/* name unknown: open entries a mount allows */

static int pfsRedirectVfsInit(SceVfsOpInitArgs *arg)
{
	return 0;
}

static int pfsRedirectVfsFini(SceVfsOpFiniArgs *arg)
{
	return 0;
}

static int pfsRedirectVfsDevctl(SceVfsOpDevctlArg *arg)
{
	return pfsErrorToErrno(pfsRedirectDevctl(arg->mnt->pData, arg->mnt, arg->dev, arg->cmd,
						 arg->arg, arg->argLen, arg->buf, arg->bufLen));
}

static int pfsRedirectVfsMount(SceVfsOpMountArgs *arg)
{
	SceVfsMount *mnt = arg->mnt;

	mnt->availableEntryNum = PFS_REDIRECT_ENTRIES;
	mnt->defaultIoCacheSize = mnt->mntVnode->core.mnt->defaultIoCacheSize;
	return 0;
}

static int pfsRedirectVfsUmount(SceVfsOpUmountArgs *arg)
{
	ScePfsPmi *pmi = (ScePfsPmi *)arg->mnt->pData;

	if (arg->flags & 1)
		pmi->flags |= PFS_PMI_NOT_MOUNTED;
	else
		pmi->flags &= ~PFS_PMI_NOT_MOUNTED;
	return 0;
}

static int pfsRedirectVfsSync(SceVfsOpSyncArgs *arg)
{
	return 0;
}

static int pfsRedirectVfsSetRoot(SceVfsOpSetRootArgs *arg)
{
	return pfsErrorToErrno(pfsRedirectSetRoot(arg->mnt->pData, arg->mnt, arg->unused, arg->vp));
}

static int pfsRedirectVfsDecodePathElem(SceVfsOpDecodePathElemArgs *arg)
{
	return pfsErrorToErrno(pfs_decode_path_elem(arg->mnt->pData, arg->mnt, arg->path, arg->path2,
						     arg->path3, arg->buf, arg->bufLen,
						     arg->decodeLen));
}

static int pfsRedirectVopLookup(SceVopLookupArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_lookup(pmi, arg->dvp, arg->vpp, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopOpen(SceVopOpenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_open(pmi, arg->vp, arg->file, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopCreate(SceVopCreateArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_create(pmi, arg->dvp, arg->vpp, arg->path, arg->flags,
						     arg->mode));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopClose(SceVopCloseArgs *arg)
{
	return pfsErrorToErrno(redirect_close(pfsVnodePmi(arg->vp), arg->vp, arg->file));
}

static SceSSize pfsRedirectVopRead(SceVopReadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = redirect_read(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsRedirectVopWrite(SceVopWriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = redirect_write(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsRedirectVopPread(SceVopPreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsRedirectPread(pmi, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize pfsRedirectVopPwrite(SceVopPwriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = pfsRedirectPwrite(pmi, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceOff pfsRedirectVopLseek(SceVopLseekArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	SceOff ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		ret = redirect_lseek(pmi, arg->vp, arg->file, arg->offset, arg->whence);
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno((int)ret);
}

static int pfsRedirectVopIoctl(SceVopIoctlArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(pfsRedirectIoctl(pmi, arg->vp, arg->file, arg->cmd, arg->inData,
						    arg->inLen, arg->outData, arg->outLen));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopInactive(SceVopInactiveArgs *arg)
{
	return pfsErrorToErrno(pfsRedirectInactive(arg->vp));
}

static int pfsRedirectVopRemove(SceVopRemoveArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_remove(pmi, arg->dvp, arg->vp, arg->path, arg->flags));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopMkdir(SceVopMkdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_mkdir(pmi, arg->dvp, arg->vpp, arg->path, arg->mode));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopRmdir(SceVopRmdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(redirect_rmdir(pmi, arg->dvp, arg->vp, arg->path));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopDopen(SceVopDopenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(redirect_dopen(pmi, arg->vp, arg->path, arg->file));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopDread(SceVopDreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsRedirectDread(pmi, arg->vp, arg->file, arg->dir));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopDclose(SceVopDcloseArgs *arg)
{
	return pfsErrorToErrno(redirect_dclose(pfsVnodePmi(arg->vp), arg->vp, arg->file));
}

static int pfsRedirectVopGetstat(SceVopGetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsRedirectGetstat(pmi, arg->vp, arg->path, arg->stat));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopChstat(SceVopChstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_chstat(pmi, arg->vp, arg->path, arg->stat, arg->bit));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopFgetstat(SceVopFgetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(pfsRedirectFgetstat(pmi, arg->vp, arg->file, arg->stat));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopFchstat(SceVopFchstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_fchstat(pmi, arg->vp, arg->file, arg->stat, arg->bit));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopRename(SceVopRenameArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->odvp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_rename(pmi, arg->odvp, arg->ovp, arg->oldPath,
						     arg->ndvp, arg->nvpp, arg->newPath));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopSync(SceVopSyncArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pmi != NULL) {
		if (!(pmi->flags & PFS_PMI_NOT_MOUNTED))
			return pfsErrorToErrno(redirect_fsync(pmi, arg->vp, arg->file, arg->flags));
	}
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopWhiteout(SceVopWhiteoutArgs *args)
{
	SceVopWhiteoutArgs *arg = (void *)args;
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);

	if (pfsPmiIsMounted(pmi))
		return pfsErrorToErrno(redirect_whiteout(pmi, arg->dvp, arg->vp, arg->oldPath,
						       arg->newPath, arg->a4, arg->a5));
	return pfsErrorToErrno(SCE_ERROR_ERRNO_ENODEV);
}

static int pfsRedirectVopCleanup(SceVopCleanupArgs *args)
{
	SceVopCleanupArgs *arg = (void *)args;
	return pfsErrorToErrno(redirect_cleanup(pfsVnodePmi(arg->vp), arg->vp, arg->file,
						      arg->a2, arg->a3));
}

const SceVfsOpTable g_pfsRedirectVfsOps = {
	.vfs_mount		= pfsRedirectVfsMount,
	.vfs_umount		= pfsRedirectVfsUmount,
	.vfs_set_root		= pfsRedirectVfsSetRoot,
	.vfs_sync		= pfsRedirectVfsSync,
	.vfs_init		= pfsRedirectVfsInit,
	.vfs_fini		= pfsRedirectVfsFini,
	.vfs_devctl		= pfsRedirectVfsDevctl,
	.vfs_decode_path_elem	= pfsRedirectVfsDecodePathElem,
};

const SceVopTable g_pfsRedirectVops = {
	.vop_open	= pfsRedirectVopOpen,
	.vop_create	= pfsRedirectVopCreate,
	.vop_close	= pfsRedirectVopClose,
	.vop_lookup	= pfsRedirectVopLookup,
	.vop_read	= pfsRedirectVopRead,
	.vop_write	= pfsRedirectVopWrite,
	.vop_lseek	= pfsRedirectVopLseek,
	.vop_ioctl	= pfsRedirectVopIoctl,
	.vop_remove	= pfsRedirectVopRemove,
	.vop_mkdir	= pfsRedirectVopMkdir,
	.vop_rmdir	= pfsRedirectVopRmdir,
	.vop_dopen	= pfsRedirectVopDopen,
	.vop_dclose	= pfsRedirectVopDclose,
	.vop_dread	= pfsRedirectVopDread,
	.vop_getstat	= pfsRedirectVopGetstat,
	.vop_chstat	= pfsRedirectVopChstat,
	.vop_rename	= pfsRedirectVopRename,
	.vop_pread	= pfsRedirectVopPread,
	.vop_pwrite	= pfsRedirectVopPwrite,
	.vop_inactive	= pfsRedirectVopInactive,
	.vop_sync	= pfsRedirectVopSync,
	.vop_fgetstat	= pfsRedirectVopFgetstat,
	.vop_fchstat	= pfsRedirectVopFchstat,
	.vop_whiteout	= pfsRedirectVopWhiteout,
	.vop_cleanup	= pfsRedirectVopCleanup,
};
