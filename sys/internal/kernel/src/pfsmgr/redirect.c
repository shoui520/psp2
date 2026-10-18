/*
 * The redirect layer: a pmi without a files.db of its own. Each vnode wraps
 * one in the underlying file system, and each operation is passed on to it
 * with only the mode and PFS attributes translated.
 */

#include <sysmem/sysclib.h>
#include "pfsmgr_internal.h"

int pfsRedirectVnodeCreate(SceVfsVnode **vpp, SceVfsVnode *dvp, SceVfsVnode *lowerVp,
			   SceUInt16 attr)
{
	SceVfsMount *mnt;
	ScePfsVd *vd;
	SceVfsVnode *vp;
	int ret;

	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mntVfsInf->defaultVops,
			     0, vpp);
	if (ret != 0)
		return ret;
	vd = pfsBufferListAlloc(&g_pfsVdBufferList);
	if (vd == NULL) {
		vfsFreeVnode(*vpp);
		return SCE_ERROR_ERRNO_ENOMEM;
	}

	vfsLockVnode(lowerVp);
	vd->slot = -1;
	vd->parentIndex = -1;
	vd->index = -1;
	vd->lowerVp = lowerVp;
	vd->pages.next = &vd->pages;
	vd->pages.prev = &vd->pages;
	vfsLockVnode(*vpp);
	mnt = dvp->core.mnt;
	(*vpp)->core.dd = dvp;
	(*vpp)->core.mnt = mnt;
	vp = *vpp;
	vp->core.nodeData = vd;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = lowerVp->core.type;
	vp->core.size = lowerVp->core.size;
	*(SceUInt64 *)vp->core.aclData = attr;
	*(SceUInt64 *)vp->core.fid = *(SceUInt64 *)lowerVp->core.fid;
	vp->core.refCount = 1;
	vfsUnlockVnode(lowerVp);
	return 0;
}

int pfsRedirectSetRoot(ScePfsPmi *pmi, SceVfsMount *mnt, int unk, SceVfsVnode *vp)
{
	SceVfsVnode *lowerVp = mnt->mntVnode;
	ScePfsVd *vd;

	vd = pfsBufferListAlloc(&g_pfsVdBufferList);
	if (vd == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	vd->slot = -1;
	vd->parentIndex = -1;
	vd->index = -1;
	vd->lowerVp = lowerVp;
	vd->pages.next = &vd->pages;
	vd->pages.prev = &vd->pages;
	vp->core.nodeData = vd;
	vp->core.mnt = mnt;
	vp->core.size = 0;
	vp->core.dd = NULL;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	*(SceUInt64 *)vp->core.aclData =
		(SceUInt16)pfsCreateAttr(pmi, (SceUInt16)lowerVp->core.aclData[0]);
	*(SceUInt64 *)vp->core.fid = *(SceUInt64 *)lowerVp->core.fid;
	return 0;
}

int pfsRedirectDevctl(ScePfsPmi *pmi, SceVfsMount *mnt, const char *dev, unsigned int cmd,
		      const void *arg, SceSize argLen, void *buf, SceSize bufLen)
{
	SceVfsMount *lower;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DEVCTL, cmd, &perm);
	if (ret != 0)
		return ret;
	lower = mnt->mntVnode->core.mnt;
	if (lower == NULL)
		ret = SCE_ERROR_ERRNO_EINVAL;
	else
		ret = sceVfsOpDevctl(lower, dev, cmd, arg, argLen, buf, bufLen);
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectLookup(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		      int flags)
{
	ScePfsVd *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_LOOKUP, 0, &perm);
	if (ret != 0)
		return ret;
	vd = dvp->core.nodeData;
	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerDvp = vd->lowerVp;
	if (lowerDvp == NULL) {
		pfsAccessEnd(perm);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	vfsLockVnode(lowerDvp);
	ret = sceVopLookup(lowerDvp, &lowerVp, path, flags);
	vfsUnlockVnode(lowerDvp);
	if (ret >= 0) {
		vfsUnlockVnode(lowerVp);
		ret = pfsRedirectVnodeCreate(vpp, dvp, lowerVp,
					     pfsCreateAttr(pmi, (SceUInt16)lowerVp->core.aclData[0]));
		if (ret != 0) {
			vfsLockVnode(lowerVp);
			sceVopInactive(lowerVp);
			vfsFreeVnode(lowerVp);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectOpen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceVfsPath *path,
		    int flags)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	int perm, ret, err;

	ret = pfsAccessBegin(pmi, PFS_OP_OPEN, flags, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		err = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	lowerVp = vd->lowerVp;
	fi = pfsBufferListAlloc(&g_pfsFiBufferList);
	if (fi == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	fi->type = PFS_FI_FILE;
	vfsLockVnode(lowerVp);
	fi->lower.fd = vfsAllocateFile(lowerVp, &lowerFile, path->name);
	vfsUnlockVnode(lowerVp);
	err = fi->lower.fd;
	if (err < 0) {
		pfsBufferListFree(&g_pfsFiBufferList, fi);
		goto out;
	}
	vfsLockVnode(lowerVp);
	err = sceVopOpen(lowerVp, path, flags | 0x10000000, lowerFile);
	vfsUnlockVnode(lowerVp);
	if (err < 0) {
		vfsLockVnode(lowerVp);
		vfsFreeFile(lowerVp, fi->lower.fd);
		vfsUnlockVnode(lowerVp);
		pfsBufferListFree(&g_pfsFiBufferList, fi);
		goto out;
	}
	fi->lower.file = lowerFile;
	file->fd = (SceUInt32)fi;
	err = ret;
out:
	pfsAccessEnd(perm);
	return err;
}

int pfsRedirectCreate(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		      int flags, int mode)
{
	ScePfsVd *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CREATE, 0, &perm);
	if (ret != 0)
		return ret;
	vd = dvp->core.nodeData;
	if (vd == NULL) {
		pfsAccessEnd(perm);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	lowerDvp = vd->lowerVp;
	vfsLockVnode(lowerDvp);
	ret = sceVopCreate(lowerDvp, &lowerVp, path, flags, pfsLowerMode(mode));
	vfsUnlockVnode(lowerDvp);
	if (ret >= 0) {
		vfsUnlockVnode(lowerVp);
		ret = pfsRedirectVnodeCreate(vpp, dvp, lowerVp,
					     pfsCreateAttr(pmi, (SceUInt16)lowerVp->core.aclData[0]));
		if (ret != 0) {
			vfsLockVnode(lowerVp);
			vfsLockVnode(lowerDvp);
			sceVopRemove(lowerDvp, lowerVp, path, 0);
			vfsUnlockVnode(lowerDvp);
			sceVopInactive(lowerVp);
			vfsFreeVnode(lowerVp);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectClose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file)
{
	ScePfsFi *fi;
	ScePfsVd *vd;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CLOSE, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi->type != PFS_FI_FILE) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else if ((vd = vp->core.nodeData) == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		lowerFile = fi->lower.file;
		if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
			/* going away anyway: errors do not matter */
			vfsLockVnode(lowerVp);
			sceVopClose(lowerVp, lowerFile);
			vfsUnlockVnode(lowerVp);
			vfsLockVnode(lowerVp);
			vfsFreeFile(lowerVp, fi->lower.fd);
			vfsUnlockVnode(lowerVp);
			pfsBufferListFree(&g_pfsFiBufferList, fi);
		} else {
			vfsLockVnode(lowerVp);
			ret = sceVopClose(lowerVp, lowerFile);
			vfsUnlockVnode(lowerVp);
			if (ret == 0) {
				vfsLockVnode(lowerVp);
				ret = vfsFreeFile(lowerVp, fi->lower.fd);
				vfsUnlockVnode(lowerVp);
				if (ret >= 0)
					pfsBufferListFree(&g_pfsFiBufferList, fi);
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectRead(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	SceSize n;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_READ, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		ret = SCE_ERROR_ERRNO_EBADFD;
		if (fi->type == PFS_FI_FILE) {
			lowerFile = fi->lower.file;
			lowerFile->fpos = file->fpos;
			vfsLockVnode(lowerVp);
			ret = sceVopRead(lowerVp, lowerFile, buf, nbyte, &n);
			vfsUnlockVnode(lowerVp);
			if (ret >= 0) {
				file->fpos += (int)n;
				ret = n;
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectWrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		     SceSize nbyte)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	SceSize n;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_WRITE, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		ret = SCE_ERROR_ERRNO_EBADFD;
		if (fi->type == PFS_FI_FILE) {
			lowerFile = fi->lower.file;
			lowerFile->fpos = file->fpos;
			vfsLockVnode(lowerVp);
			ret = sceVopWrite(lowerVp, lowerFile, buf, nbyte, &n);
			vfsUnlockVnode(lowerVp);
			if (ret >= 0) {
				vp->core.size = lowerVp->core.size;
				file->fpos += (int)n;
				ret = n;
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectPread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte,
		     SceOff offset)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceSize n;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_PREAD, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		ret = SCE_ERROR_ERRNO_EBADFD;
		if (fi->type == PFS_FI_FILE) {
			vfsLockVnode(lowerVp);
			ret = sceVopPread(lowerVp, fi->lower.file, buf, nbyte, offset, &n);
			vfsUnlockVnode(lowerVp);
			if (ret >= 0)
				ret = n;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectPwrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		      SceSize nbyte, SceOff offset)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceSize n;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_PWRITE, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		ret = SCE_ERROR_ERRNO_EBADFD;
		if (fi->type == PFS_FI_FILE) {
			vfsLockVnode(lowerVp);
			ret = sceVopPwrite(lowerVp, fi->lower.file, buf, nbyte, offset, &n);
			vfsUnlockVnode(lowerVp);
			if (ret >= 0) {
				vp->core.size = lowerVp->core.size;
				ret = n;
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

SceOff pfsRedirectLseek(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceOff offset,
			int whence)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	SceOff pos;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_LSEEK, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		if (fi->type == PFS_FI_FILE) {
			lowerFile = fi->lower.file;
			lowerFile->fpos = file->fpos;
			vfsLockVnode(lowerVp);
			pos = sceVopLseek(lowerVp, lowerFile, offset, whence);
			vfsUnlockVnode(lowerVp);
			if (pos >= 0) {
				file->fpos = pos;
				pfsAccessEnd(perm);
				return pos;
			}
			pfsAccessEnd(perm);
			return (int)pos;
		}
		ret = SCE_ERROR_ERRNO_EBADFD;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in,
		     SceSize inLen, void *out, SceSize outLen)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_IOCTL, cmd, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		ret = SCE_ERROR_ERRNO_EBADFD;
		if (fi->type == PFS_FI_FILE) {
			vfsLockVnode(lowerVp);
			ret = sceVopIoctl(lowerVp, fi->lower.file, cmd, in, inLen, out, outLen);
			vfsUnlockVnode(lowerVp);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectInactive(SceVfsVnode *vp)
{
	ScePfsVd *vd = vp->core.nodeData;
	ScePfsPmi *pmi = vp->core.mnt->pData;
	SceVfsVnode *lowerVp;
	ScePfsNode node;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerVp = vd->lowerVp;
	pfsBufferListFree(&g_pfsVdBufferList, vd);
	if (vp->core.type < SCE_VNODE_TYPE_ROOTDIR) {
		node.vp = lowerVp;
		node.name = NULL;
		if (pmi->flags & PFS_PMI_NOT_MOUNTED)
			pfsNodeRelease(&node);
		else
			pfsNodeInactive(&node);
	}
	return 0;
}

int pfsRedirectRemove(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path,
		      int flags)
{
	ScePfsVd *dvd, *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_REMOVE, 0, &perm);
	if (ret != 0)
		return ret;
	if (dvp == NULL) {
		vd = vp->core.nodeData;
		if (vd == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			goto out;
		}
		lowerVp = vd->lowerVp;
		goto noDir;
	}
	dvd = dvp->core.nodeData;
	vd = vp->core.nodeData;
	if (dvd == NULL || vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	lowerDvp = dvd->lowerVp;
	lowerVp = vd->lowerVp;
	if (lowerDvp == NULL) {
noDir:
		vfsLockVnode(lowerVp);
		ret = sceVopRemove(NULL, lowerVp, path, flags);
		vfsUnlockVnode(lowerVp);
	} else {
		vfsLockVnode(lowerDvp);
		vfsLockVnode(lowerVp);
		ret = sceVopRemove(lowerDvp, lowerVp, path, flags);
		vfsUnlockVnode(lowerVp);
		vfsUnlockVnode(lowerDvp);
	}
out:
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectMkdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		     int mode)
{
	ScePfsVd *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_MKDIR, 0, &perm);
	if (ret != 0)
		return ret;
	vd = dvp->core.nodeData;
	if (vd == NULL) {
		pfsAccessEnd(perm);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	lowerDvp = vd->lowerVp;
	vfsLockVnode(lowerDvp);
	ret = sceVopMkdir(lowerDvp, &lowerVp, path, pfsLowerMode(mode));
	vfsUnlockVnode(lowerDvp);
	if (ret >= 0) {
		vfsUnlockVnode(lowerVp);
		ret = pfsRedirectVnodeCreate(vpp, dvp, lowerVp,
					     pfsCreateAttr(pmi, (SceUInt16)lowerVp->core.aclData[0]));
		if (ret != 0) {
			vfsLockVnode(lowerVp);
			vfsLockVnode(lowerDvp);
			sceVopRmdir(lowerDvp, lowerVp, path, 0);
			vfsUnlockVnode(lowerDvp);
			sceVopInactive(lowerVp);
			vfsFreeVnode(lowerVp);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectRmdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path)
{
	ScePfsVd *dvd, *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_RMDIR, 0, &perm);
	if (ret != 0)
		return ret;
	dvd = dvp->core.nodeData;
	vd = vp->core.nodeData;
	if (dvd == NULL || vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerDvp = dvd->lowerVp;
		lowerVp = vd->lowerVp;
		vfsLockVnode(lowerDvp);
		vfsLockVnode(lowerVp);
		ret = sceVopRmdir(lowerDvp, lowerVp, path, 0);
		vfsUnlockVnode(lowerVp);
		vfsUnlockVnode(lowerDvp);
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectDopen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	int perm, ret, err;

	ret = pfsAccessBegin(pmi, PFS_OP_DOPEN, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		err = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	lowerVp = vd->lowerVp;
	fi = pfsBufferListAlloc(&g_pfsFiBufferList);
	if (fi == NULL) {
		err = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	fi->type = PFS_FI_DIR;
	vfsLockVnode(lowerVp);
	fi->lower.fd = vfsAllocateFile(lowerVp, &lowerFile, path->name);
	vfsUnlockVnode(lowerVp);
	err = fi->lower.fd;
	if (err < 0) {
		pfsBufferListFree(&g_pfsFiBufferList, fi);
		goto out;
	}
	vfsLockVnode(lowerVp);
	err = sceVopDopen(lowerVp, path, lowerFile);
	vfsUnlockVnode(lowerVp);
	if (err < 0) {
		vfsFreeFile(lowerVp, fi->lower.fd);
		pfsBufferListFree(&g_pfsFiBufferList, fi);
		goto out;
	}
	fi->lower.file = lowerFile;
	file->fd = (SceUInt32)fi;
	err = ret;
out:
	pfsAccessEnd(perm);
	return err;
}

int pfsRedirectDread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceUInt32 attr, mode;
	int perm, ret, err;

	ret = pfsAccessBegin(pmi, PFS_OP_DREAD, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		if (fi->type == PFS_FI_DIR) {
			vfsLockVnode(lowerVp);
			ret = sceVopDread(lowerVp, fi->lower.file, dir);
			vfsUnlockVnode(lowerVp);
			if (ret >= 0) {
				attr = pfsCreateAttr(pmi, dir->d_stat.st_attr);
				err = pfsAttrToMode(attr, &mode);
				if (err == 0) {
					dir->d_stat.st_attr = attr;
					dir->d_stat.st_mode = mode & 0xff0fffff;
				} else {
					ret = err;
				}
			}
		} else {
			ret = SCE_ERROR_ERRNO_EBADFD;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectDclose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceVfsFile *lowerFile;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DCLOSE, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		fi = (ScePfsFi *)file->fd;
		lowerVp = vd->lowerVp;
		if (fi->type != PFS_FI_DIR) {
			ret = SCE_ERROR_ERRNO_EBADFD;
		} else {
			lowerFile = fi->lower.file;
			if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
				/* going away anyway: errors do not matter */
				vfsLockVnode(lowerVp);
				sceVopDclose(lowerVp, lowerFile);
				vfsUnlockVnode(lowerVp);
				vfsLockVnode(lowerVp);
				vfsFreeFile(lowerVp, fi->lower.fd);
				vfsUnlockVnode(lowerVp);
				pfsBufferListFree(&g_pfsFiBufferList, fi);
			} else {
				vfsLockVnode(lowerVp);
				ret = sceVopDclose(lowerVp, lowerFile);
				vfsUnlockVnode(lowerVp);
				if (ret >= 0) {
					vfsLockVnode(lowerVp);
					ret = vfsFreeFile(lowerVp, fi->lower.fd);
					vfsUnlockVnode(lowerVp);
					if (ret >= 0)
						pfsBufferListFree(&g_pfsFiBufferList, fi);
				}
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectGetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat)
{
	ScePfsVd *vd;
	SceVfsVnode *lowerVp;
	SceUInt32 attr, mode;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_GETSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		pfsAccessEnd(perm);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	lowerVp = vd->lowerVp;
	vfsLockVnode(lowerVp);
	ret = sceVopGetstat(lowerVp, path, stat);
	vfsUnlockVnode(lowerVp);
	if (ret >= 0) {
		attr = pfsCreateAttr(pmi, stat->st_attr);
		ret = pfsAttrToMode(attr, &mode);
		if (ret == 0) {
			stat->st_attr = attr;
			stat->st_mode = mode & 0xff0fffff;
			pfsAccessEnd(perm);
			return ret;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectChstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit)
{
	ScePfsVd *vd;
	SceVfsVnode *lowerVp;
	SceIoStat lowerStat;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CHSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		memcpy(&lowerStat, stat, sizeof(lowerStat));
		if (bit & SCE_CST_MODE)
			lowerStat.st_mode = pfsLowerMode(lowerStat.st_mode);
		vfsLockVnode(lowerVp);
		ret = sceVopChstat(lowerVp, path, &lowerStat, bit);
		vfsUnlockVnode(lowerVp);
		if (ret >= 0 && (bit & SCE_CST_SIZE))
			vp->core.size = lowerVp->core.size;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectFgetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceUInt32 attr, mode;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_GETSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi == NULL) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else {
		vd = vp->core.nodeData;
		if (vd == NULL) {
			pfsAccessEnd(perm);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		lowerVp = vd->lowerVp;
		vfsLockVnode(lowerVp);
		ret = sceVopFgetstat(lowerVp, fi->lower.file, stat);
		vfsUnlockVnode(lowerVp);
		if (ret >= 0) {
			attr = pfsCreateAttr(pmi, stat->st_attr);
			ret = pfsAttrToMode(attr, &mode);
			if (ret == 0) {
				stat->st_attr = attr;
				stat->st_mode = mode & 0xff0fffff;
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectFchstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat,
		       int bit)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	SceIoStat lowerStat;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CHSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi == NULL) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else if ((vd = vp->core.nodeData) == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		memcpy(&lowerStat, stat, sizeof(lowerStat));
		if (bit & SCE_CST_MODE)
			lowerStat.st_mode = pfsLowerMode(lowerStat.st_mode);
		vfsLockVnode(lowerVp);
		ret = sceVopFchstat(lowerVp, fi->lower.file, &lowerStat, bit);
		vfsUnlockVnode(lowerVp);
		if (ret >= 0 && (bit & SCE_CST_SIZE))
			vp->core.size = lowerVp->core.size;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectRename(ScePfsPmi *pmi, SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *oldPath,
		      SceVfsVnode *ndvp, SceVfsVnode **nvpp, SceVfsPath *newPath)
{
	ScePfsVd *odvd, *ovd, *ndvd;
	SceVfsVnode *lowerOdvp, *lowerOvp, *lowerNdvp, *lowerNvp;
	SceVfsFile *file;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_RENAME, 0, &perm);
	if (ret != 0)
		return ret;
	odvd = odvp->core.nodeData;
	if (ndvp == NULL) {
		ovd = ovp->core.nodeData;
		if (odvd == NULL || ovd == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			goto out;
		}
		lowerOdvp = odvd->lowerVp;
		lowerOvp = ovd->lowerVp;
		vfsLockVnode(lowerOdvp);
		vfsLockVnode(lowerOvp);
		ret = sceVopRename(lowerOdvp, lowerOvp, oldPath, lowerOdvp, &lowerNvp, newPath);
		vfsUnlockVnode(lowerOvp);
		vfsUnlockVnode(lowerOdvp);
		ndvp = odvp;
	} else {
		ndvd = ndvp->core.nodeData;
		ovd = ovp->core.nodeData;
		if (ndvd == NULL || odvd == NULL || ovd == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			goto out;
		}
		lowerOdvp = odvd->lowerVp;
		lowerOvp = ovd->lowerVp;
		lowerNdvp = ndvd->lowerVp;
		vfsLockVnode(lowerOdvp);
		vfsLockVnode(lowerOvp);
		vfsLockVnode(lowerNdvp);
		ret = sceVopRename(lowerOdvp, lowerOvp, oldPath, lowerNdvp, &lowerNvp, newPath);
		vfsUnlockVnode(lowerNdvp);
		vfsUnlockVnode(lowerOvp);
		vfsUnlockVnode(lowerOdvp);
	}
	if (ret < 0)
		goto out;

	vfsUnlockVnode(lowerNvp);
	ret = pfsRedirectVnodeCreate(nvpp, ndvp, lowerNvp,
				     pfsCreateAttr(pmi, (SceUInt16)lowerNvp->core.aclData[0]));
	if (ret != 0) {
		sceVopInactive(lowerNvp);
		vfsFreeVnode(lowerNvp);
		goto out;
	}
	/* the files open on the old lower vnode move to the new one */
	vfsLockVnode(lowerNvp);
	vfsLockVnode(lowerOvp);
	lowerNvp->core.bc = lowerOvp->core.bc;
	lowerNvp->core.fdList = lowerOvp->core.fdList;
	lowerNvp->core.fdNum = lowerOvp->core.fdNum;
	for (file = lowerNvp->core.fdList; file != NULL; file = file->next)
		file->vnode = lowerNvp;
	lowerOvp->core.fdList = NULL;
	lowerOvp->core.fdNum = 0;
	lowerOvp->core.bc = NULL;
	vfsUnlockVnode(lowerOvp);
	vfsUnlockVnode(lowerNvp);
	for (file = ovp->core.fdList; file != NULL; file = file->next)
		((ScePfsFi *)file->fd)->lower.vp = lowerNvp;
out:
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectVnodeSync(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int flags)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	int perm, ret;

	if (file == NULL)
		return 0;
	ret = pfsAccessBegin(pmi, PFS_OP_VNODE_SYNC, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		fi = (ScePfsFi *)file->fd;
		if (fi->type == PFS_FI_FILE) {
			vfsLockVnode(lowerVp);
			ret = sceVopSync(lowerVp, fi->lower.file, flags);
			vfsUnlockVnode(lowerVp);
		} else if (fi->type == PFS_FI_DIR) {
			ret = 0;
		} else {
			ret = SCE_ERROR_ERRNO_EBADFD;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
			SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5)
{
	ScePfsVd *dvd, *vd;
	SceVfsVnode *lowerDvp, *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_WHITEOUT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.nodeData;
	dvd = dvp->core.nodeData;
	if (vd == NULL || dvd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerDvp = dvd->lowerVp;
		lowerVp = vd->lowerVp;
		vfsLockVnode(lowerDvp);
		vfsLockVnode(lowerVp);
		ret = sceVopWhiteout(lowerDvp, lowerVp, oldPath, newPath, a4, a5);
		vfsUnlockVnode(lowerVp);
		vfsUnlockVnode(lowerDvp);
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsRedirectCleanup(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2,
		       SceUInt32 a3)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lowerVp;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CLEANUP, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi->type != PFS_FI_FILE) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else if ((vd = vp->core.nodeData) == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		lowerVp = vd->lowerVp;
		vfsLockVnode(lowerVp);
		ret = sceVopCleanup(lowerVp, fi->lower.file, a2, a3);
		vfsUnlockVnode(lowerVp);
	}
	pfsAccessEnd(perm);
	return ret;
}
