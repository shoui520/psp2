/*
 * The AC pseudo drive's vfs and vnode operations. Everything outside an acid
 * directory works as on the gamedata/savedata drive. Inside one, an operation
 * goes to the acid directory's own mount, or, while it has none, through the
 * redirect layer with a stand-in pmi.
 */

#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include "../pfsmgr_internal.h"

static inline ScePfsVd *pfsVnodeVd(SceVfsVnode *vp)	/* name unknown */
{
	return (ScePfsVd *)vp->core.nodeData;
}

/* outside every acid directory, or an acid directory itself */
static inline int pfsAcIsOwn(SceVfsVnode *vp)	/* name unknown */
{
	return pfsVnodeVd(vp)->slot == (SceUInt32)-1 ||
	       ((SceUInt16)vp->core.aclData[0] & PFS_ATTR_ACID_DIR);
}

static inline int pfsAcIsAcMode(SceUInt16 mode)	/* name unknown */
{
	return (mode & ~8) == 4 || mode == 8;
}

/* a pmi standing in for an acid directory without a mount of its own */
static inline void pfsAcStandIn(ScePfsPmi *acid, ScePfsPmi *pmi)	/* name unknown */
{
	__builtin_memcpy(acid->path, pmi->path, sizeof(acid->path));
	__builtin_memcpy(acid->rootPath, pmi->rootPath, sizeof(acid->rootPath));
	__builtin_memcpy(acid->assignName, pmi->assignName, sizeof(acid->assignName));
	acid->flags = pmi->flags;
	acid->mode = PFS_MODE_ACID_DIR;
	acid->parent = pmi;
}

/* the files.db index of vp in the acid's mount, found by its path */
static inline void pfsAcResolve(ScePfsPmi *acid, SceVfsVnode *vp, SceVfsPath *path)	/* name unknown */
{
	ScePfsVd *vd = pfsVnodeVd(vp);

	if (vd->index == (SceUInt32)-1)
		_update_aciddir_vnodedata(acid, vd, path->path);
}

/* the same for the directory holding the path's last element */
static inline void pfsAcResolveDir(ScePfsPmi *acid, SceVfsVnode *dvp, SceVfsPath *path)	/* name unknown */
{
	ScePfsVd *vd = pfsVnodeVd(dvp);
	char *slash;

	if (vd->index == (SceUInt32)-1) {
		slash = strrchr(path->path, '/');
		*slash = '\0';
		_update_aciddir_vnodedata(acid, vd, path->path);
		*slash = '/';
	}
}

static inline void pfsAcOpened(ScePfsPmi *acid, int count)	/* name unknown */
{
	sceKernelLockMutex(acid->mutex, 1, NULL);
	acid->openCount += count;
	sceKernelUnlockMutex(acid->mutex, 1);
}

static int dev_ac_init(SceVfsOpInitArgs *arg)
{
	return 0;
}

static int dev_ac_fini(SceVfsOpFiniArgs *arg)
{
	return 0;
}

static int dev_ac_devctl(SceVfsOpDevctlArg *arg)
{
	ScePfsPmi *pmi = (ScePfsPmi *)arg->mnt->pData;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (arg->cmd != PFS_DEVCTL_GET_ACID_DIRS) {
		return pfsErrorToErrno(pfsGdsdDevctl(pmi, arg->mnt, arg->dev, arg->cmd, arg->arg,
						     arg->argLen, arg->buf, arg->bufLen));
	} else if (arg->arg == NULL && arg->argLen == 0 && arg->buf != NULL &&
		   arg->bufLen == sizeof(ScePfsAcidDirs)) {
		ret = ac_get_aciddir_list(pmi, arg->buf);
	} else {
		return pfsErrorToErrno(SCE_PFS_ERROR_INVALID_ARG);
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_mount(SceVfsOpMountArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdMount(arg->mnt->pData, arg->mnt, arg->devFilePath));
}

static int dev_ac_umount(SceVfsOpUmountArgs *arg)
{
	ScePfsPmi *pmi = (ScePfsPmi *)arg->mnt->pData;
	ScePfsListNode *node, *next;
	int ret;

	sceKernelLockMutex(pmi->mutex, 1, NULL);
	node = pmi->acidDirs.next;
	if (node == NULL)
		goto notInList;
	for (; node != &pmi->acidDirs; node = next) {
		next = node->next;
		if (next == NULL)
			goto notInList;
		ret = pfsCoreUmount((ScePfsPmi *)node, arg->flags);
		if (ret != 0) {
			sceKernelUnlockMutex(pmi->mutex, 1);
			return pfsErrorToErrno(ret);
		}
	}
	sceKernelUnlockMutex(pmi->mutex, 1);
	return pfsErrorToErrno(pfsGdsdUmount(pmi, arg->mnt, arg->flags));

notInList:
	sceKernelUnlockMutex(pmi->mutex, 1);
	return pfsErrorToErrno(SCE_PFS_ERROR_NOT_IN_LIST);
}

static int dev_ac_sync(SceVfsOpSyncArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdSync(arg->mnt->pData, arg->mnt, arg->flags));
}

static int dev_ac_set_root(SceVfsOpSetRootArgs *arg)
{
	return pfsErrorToErrno(pfsGdsdSetRoot(arg->mnt->pData, arg->mnt, arg->unused, arg->vp));
}

static int dev_ac_decode_path_elem(SceVfsOpDecodePathElemArgs *arg)
{
	return pfsErrorToErrno(pfs_decode_path_elem(arg->mnt->pData, arg->mnt, arg->path,
						     arg->path2, arg->path3, arg->buf,
						     arg->bufLen, arg->decodeLen));
}

static int dev_ac_lookup(SceVopLookupArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);
	ScePfsPmi standIn, *acid;
	ScePfsVd *vd;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	if (pfsVnodeVd(arg->dvp)->slot == (SceUInt32)-1) {
		ret = pfsGdsdLookup(pmi, arg->dvp, arg->vpp, arg->path, arg->flags);
		if (ret != 0)
			goto out;
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->dvp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->dvp, arg->path);
			ret = pfsGdsdLookup(acid, arg->dvp, arg->vpp, arg->path, arg->flags);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_lookup(&standIn, arg->dvp, arg->vpp, arg->path, arg->flags);
		}
		if (ret != 0)
			goto out;
		pfsVnodeVd(*arg->vpp)->slot = pfsVnodeVd(arg->dvp)->slot;
	}
	if ((SceUInt16)(*arg->vpp)->core.aclData[0] & PFS_ATTR_ACID_DIR) {
		vd = pfsVnodeVd(*arg->vpp);
		vd->slot = vd->index;
		vd->index = 0;
	}
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_open(SceVopOpenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdOpen(pmi, arg->vp, arg->file, arg->path, arg->flags);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			arg->vp->core.mntOpt |= 0x10000;
			pfsAcResolve(acid, arg->vp, arg->path);
			ret = pfsGdsdOpen(acid, arg->vp, arg->file, arg->path, arg->flags);
			if (ret == 0)
				pfsAcOpened(acid, 1);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			arg->vp->core.mntOpt &= ~0x10000;
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_open(&standIn, arg->vp, arg->file, arg->path, arg->flags);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_create(SceVopCreateArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsVnodeVd(arg->dvp)->slot == (SceUInt32)-1) {
		ret = pfsGdsdCreate(pmi, arg->dvp, arg->vpp, arg->path, arg->flags, arg->mode);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->dvp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->dvp, arg->path);
			ret = pfsGdsdCreate(acid, arg->dvp, arg->vpp, arg->path, arg->flags,
					    arg->mode);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_create(&standIn, arg->dvp, arg->vpp, arg->path, arg->flags,
						arg->mode);
		} else {
			goto out;
		}
		if (ret == 0)
			pfsVnodeVd(*arg->vpp)->slot = pfsVnodeVd(arg->dvp)->slot;
	}
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_close(SceVopCloseArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdClose(pmi, arg->vp, arg->file);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdClose(acid, arg->vp, arg->file);
			if (ret == 0)
				pfsAcOpened(acid, -1);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_close(&standIn, arg->vp, arg->file);
		}
	}
	return pfsErrorToErrno(ret);
}

static SceSSize dev_ac_read(SceVopReadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		if (pfsAcIsOwn(arg->vp)) {
			ret = pfsGdsdRead(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		} else {
			ret = pfsAcidFind(&acid, pmi, arg->vp);
			if (ret == 0) {
				ret = pfsGdsdRead(acid, arg->vp, arg->file, arg->buf, arg->nbyte);
			} else if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = redirect_read(&standIn, arg->vp, arg->file, arg->buf,
						      arg->nbyte);
			}
		}
		if (ret >= 0 || ret == SCE_PFS_ERROR_PASS_THROUGH)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize dev_ac_write(SceVopWriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		if (pfsAcIsOwn(arg->vp)) {
			ret = pfsGdsdWrite(pmi, arg->vp, arg->file, arg->buf, arg->nbyte);
		} else {
			ret = pfsAcidFind(&acid, pmi, arg->vp);
			if (ret == 0) {
				ret = pfsGdsdWrite(acid, arg->vp, arg->file, arg->buf, arg->nbyte);
			} else if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = redirect_write(&standIn, arg->vp, arg->file, arg->buf,
						       arg->nbyte);
			}
		}
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize dev_ac_pread(SceVopPreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		if (pfsAcIsOwn(arg->vp)) {
			acid = pmi;
		} else {
			ret = pfsAcidFind(&acid, pmi, arg->vp);
			if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = pfsRedirectPread(&standIn, arg->vp, arg->file, arg->buf,
						       arg->nbyte, arg->offset);
				goto done;
			}
			if (ret != 0)
				goto done;
		}
		ret = pfsGdsdPread(acid, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
done:
		if (ret >= 0 || ret == SCE_PFS_ERROR_PASS_THROUGH)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceSSize dev_ac_pwrite(SceVopPwriteArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		if (pfsAcIsOwn(arg->vp)) {
			acid = pmi;
		} else {
			ret = pfsAcidFind(&acid, pmi, arg->vp);
			if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = pfsRedirectPwrite(&standIn, arg->vp, arg->file, arg->buf,
							arg->nbyte, arg->offset);
				goto done;
			}
			if (ret != 0)
				goto done;
		}
		ret = pfsGdsdPwrite(acid, arg->vp, arg->file, arg->buf, arg->nbyte, arg->offset);
done:
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno(ret);
}

static SceOff dev_ac_lseek(SceVopLseekArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	SceOff ret;
	int err;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		if (pfsAcIsOwn(arg->vp)) {
			acid = pmi;
		} else {
			err = pfsAcidFind(&acid, pmi, arg->vp);
			ret = err;
			if (err == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = redirect_lseek(&standIn, arg->vp, arg->file, arg->offset,
						       arg->whence);
				goto done;
			}
			if (err != 0)
				goto done;
		}
		ret = pfsGdsdLseek(acid, arg->vp, arg->file, arg->offset, arg->whence);
done:
		if (ret >= 0)
			return ret;
	}
	return pfsErrorToErrno((int)ret);
}

static int dev_ac_ioctl(SceVopIoctlArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	switch (arg->cmd) {
	case PFS_IOCTL_ACID_DIR_SET:
		ret = pfsAcidDirSet(pmi, arg->vp);
		break;
	case PFS_IOCTL_ACID_DIR_MOUNT:
		ret = _alloc_aciddir_pmi(pmi, arg->vp, arg->inData, arg->inLen);
		break;
	case PFS_IOCTL_ACID_DIR_SEAL:
		ret = pfsAcidDirSeal(pmi, arg->vp);
		break;
	case PFS_IOCTL_ACID_DIR_UMOUNT:
		ret = _umount_aciddir_pmi(pmi, arg->vp);
		break;
	default:
		if (pfsAcIsOwn(arg->vp)) {
			acid = pmi;
		} else {
			ret = pfsAcidFind(&acid, pmi, arg->vp);
			if (ret != 0) {
				if (ret == SCE_PFS_ERROR_NO_ACID) {
					pfsAcStandIn(&standIn, pmi);
					ret = pfsRedirectIoctl(&standIn, arg->vp, arg->file, arg->cmd,
							       arg->inData, arg->inLen,
							       arg->outData, arg->outLen);
				}
				break;
			}
		}
		ret = pfsGdsdIoctl(acid, arg->vp, arg->file, arg->cmd, arg->inData, arg->inLen,
				   arg->outData, arg->outLen);
		break;
	}
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_inactive(SceVopInactiveArgs *arg)
{
	int ret;

	if (pfsAcIsOwn(arg->vp) || pfsVnodeVd(arg->vp)->index != (SceUInt32)-1)
		ret = pfs_inactive(arg->vp);
	else
		ret = pfsRedirectInactive(arg->vp);
	return pfsErrorToErrno(ret);
}

static int dev_ac_remove(SceVopRemoveArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdRemove(pmi, arg->dvp, arg->vp, arg->path, arg->flags);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->dvp, arg->path);
			pfsAcResolve(acid, arg->vp, arg->path);
			ret = pfsGdsdRemove(acid, arg->dvp, arg->vp, arg->path, arg->flags);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_remove(&standIn, arg->dvp, arg->vp, arg->path, arg->flags);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_mkdir(SceVopMkdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->dvp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsVnodeVd(arg->dvp)->slot == (SceUInt32)-1) {
		ret = pfsGdsdMkdir(pmi, arg->dvp, arg->vpp, arg->path, arg->mode);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->dvp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->dvp, arg->path);
			ret = pfsGdsdMkdir(acid, arg->dvp, arg->vpp, arg->path, arg->mode);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_mkdir(&standIn, arg->dvp, arg->vpp, arg->path, arg->mode);
		} else {
			goto out;
		}
		if (ret == 0)
			pfsVnodeVd(*arg->vpp)->slot = pfsVnodeVd(arg->dvp)->slot;
	}
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_rmdir(SceVopRmdirArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdRmdir(pmi, arg->dvp, arg->vp, arg->path);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->dvp, arg->path);
			pfsAcResolve(acid, arg->vp, arg->path);
			ret = pfsGdsdRmdir(acid, arg->dvp, arg->vp, arg->path);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_rmdir(&standIn, arg->dvp, arg->vp, arg->path);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_dopen(SceVopDopenArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdDopen(pmi, arg->vp, arg->path, arg->file);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			pfsAcResolve(acid, arg->vp, arg->path);
			ret = pfsGdsdDopen(acid, arg->vp, arg->path, arg->file);
			if (ret == 0 && !pfsAcIsAcMode(acid->mode))
				pfsAcOpened(acid, 1);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_dopen(&standIn, arg->vp, arg->path, arg->file);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_dread(SceVopDreadArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsVnodeVd(arg->vp)->slot == (SceUInt32)-1) {
		ret = pfsGdsdDread(pmi, arg->vp, arg->file, arg->dir);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdDread(acid, arg->vp, arg->file, arg->dir);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = pfsRedirectDread(&standIn, arg->vp, arg->file, arg->dir);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_dclose(SceVopDcloseArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdDclose(pmi, arg->vp, arg->file);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdDclose(acid, arg->vp, arg->file);
			if (ret == 0 && !pfsAcIsAcMode(acid->mode))
				pfsAcOpened(acid, -1);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_dclose(&standIn, arg->vp, arg->file);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_getstat(SceVopGetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	if (pfsAcIsOwn(arg->vp)) {
		acid = pmi;
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret != 0) {
			if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = pfsRedirectGetstat(&standIn, arg->vp, arg->path, arg->stat);
			}
			goto out;
		}
		pfsAcResolve(acid, arg->vp, arg->path);
	}
	ret = pfsGdsdGetstat(acid, arg->vp, arg->path, arg->stat);
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_chstat(SceVopChstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	if (pfsAcIsOwn(arg->vp)) {
		acid = pmi;
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret != 0) {
			if (ret == SCE_PFS_ERROR_NO_ACID) {
				pfsAcStandIn(&standIn, pmi);
				ret = redirect_chstat(&standIn, arg->vp, arg->path, arg->stat,
							arg->bit);
			}
			goto out;
		}
		pfsAcResolve(acid, arg->vp, arg->path);
	}
	ret = pfs_chstat(acid, arg->vp, arg->path, arg->stat, arg->bit);
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_fgetstat(SceVopFgetstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdFgetstat(pmi, arg->vp, arg->file, arg->stat);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdFgetstat(acid, arg->vp, arg->file, arg->stat);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = pfsRedirectFgetstat(&standIn, arg->vp, arg->file, arg->stat);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_fchstat(SceVopFchstatArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfs_fchstat(pmi, arg->vp, arg->file, arg->stat, arg->bit);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfs_fchstat(acid, arg->vp, arg->file, arg->stat, arg->bit);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_fchstat(&standIn, arg->vp, arg->file, arg->stat, arg->bit);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_rename(SceVopRenameArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->odvp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->odvp)) {
		ret = pfsGdsdRename(pmi, arg->odvp, arg->ovp, arg->oldPath, arg->ndvp, arg->nvpp,
				    arg->newPath);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->odvp);
		if (ret == 0) {
			pfsAcResolveDir(acid, arg->odvp, arg->oldPath);
			pfsAcResolve(acid, arg->ovp, arg->oldPath);
			if (arg->ndvp != NULL)
				pfsAcResolveDir(acid, arg->ndvp, arg->newPath);
			ret = pfsGdsdRename(acid, arg->odvp, arg->ovp, arg->oldPath, arg->ndvp,
					    arg->nvpp, arg->newPath);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			if (arg->odvp != arg->ndvp) {
				ret = SCE_PFS_ERROR_ACID_CROSS;
			} else {
				pfsAcStandIn(&standIn, pmi);
				ret = redirect_rename(&standIn, arg->odvp, arg->ovp, arg->oldPath,
							arg->odvp, arg->nvpp, arg->newPath);
				if (ret == 0)
					pfsVnodeVd(*arg->nvpp)->slot = pfsVnodeVd(arg->ndvp)->slot;
			}
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_fsync(SceVopSyncArgs *arg)
{
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (!pfsAcIsAcMode(pmi->mode))
		SCE_PANIC(1279);
	if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdVnodeSync(pmi, arg->vp, arg->file, arg->flags);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdVnodeSync(acid, arg->vp, arg->file, arg->flags);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_fsync(&standIn, arg->vp, arg->file, arg->flags);
		}
	}
	return pfsErrorToErrno(ret);
}

static int dev_ac_whiteout(SceVopWhiteoutArgs *args)
{
	SceVopWhiteoutArgs *arg = (void *)args;
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi *acid;
	int ret;

	if (!pfsPmiIsMounted(pmi)) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	if (pfsAcIsOwn(arg->vp)) {
		acid = pmi;
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdWhiteout(acid, arg->dvp, arg->vp, arg->oldPath, arg->newPath,
					      arg->a4, arg->a5);
			goto out;
		}
		if (ret != SCE_PFS_ERROR_NO_ACID)
			goto out;
	}
	ret = redirect_whiteout(acid, arg->dvp, arg->vp, arg->oldPath, arg->newPath, arg->a4,
				  arg->a5);
out:
	return pfsErrorToErrno(ret);
}

static int dev_ac_cleanup(SceVopCleanupArgs *args)
{
	SceVopCleanupArgs *arg = (void *)args;
	ScePfsPmi *pmi = pfsVnodePmi(arg->vp);
	ScePfsPmi standIn, *acid;
	int ret;

	if (pfsAcIsOwn(arg->vp)) {
		ret = pfsGdsdCleanup(pmi, arg->vp, arg->file, arg->a2, arg->a3);
	} else {
		ret = pfsAcidFind(&acid, pmi, arg->vp);
		if (ret == 0) {
			ret = pfsGdsdCleanup(acid, arg->vp, arg->file, arg->a2, arg->a3);
		} else if (ret == SCE_PFS_ERROR_NO_ACID) {
			pfsAcStandIn(&standIn, pmi);
			ret = redirect_cleanup(&standIn, arg->vp, arg->file, arg->a2, arg->a3);
		}
	}
	return pfsErrorToErrno(ret);
}

const SceVfsOpTable g_pfsAcVfsOps = {
	.vfs_mount		= dev_ac_mount,
	.vfs_umount		= dev_ac_umount,
	.vfs_set_root		= dev_ac_set_root,
	.vfs_sync		= dev_ac_sync,
	.vfs_init		= dev_ac_init,
	.vfs_fini		= dev_ac_fini,
	.vfs_devctl		= dev_ac_devctl,
	.vfs_decode_path_elem	= dev_ac_decode_path_elem,
};

const SceVopTable g_pfsAcVops = {
	.vop_open	= dev_ac_open,
	.vop_create	= dev_ac_create,
	.vop_close	= dev_ac_close,
	.vop_lookup	= dev_ac_lookup,
	.vop_read	= dev_ac_read,
	.vop_write	= dev_ac_write,
	.vop_lseek	= dev_ac_lseek,
	.vop_ioctl	= dev_ac_ioctl,
	.vop_remove	= dev_ac_remove,
	.vop_mkdir	= dev_ac_mkdir,
	.vop_rmdir	= dev_ac_rmdir,
	.vop_dopen	= dev_ac_dopen,
	.vop_dclose	= dev_ac_dclose,
	.vop_dread	= dev_ac_dread,
	.vop_getstat	= dev_ac_getstat,
	.vop_chstat	= dev_ac_chstat,
	.vop_rename	= dev_ac_rename,
	.vop_pread	= dev_ac_pread,
	.vop_pwrite	= dev_ac_pwrite,
	.vop_inactive	= dev_ac_inactive,
	.vop_sync	= dev_ac_fsync,
	.vop_fgetstat	= dev_ac_fgetstat,
	.vop_fchstat	= dev_ac_fchstat,
	.vop_whiteout	= dev_ac_whiteout,
	.vop_cleanup	= dev_ac_cleanup,
};
