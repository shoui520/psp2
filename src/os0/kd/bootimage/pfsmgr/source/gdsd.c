/*
 * The gamedata/savedata pseudo drive: each operation is checked against the
 * caller's access (pfsAccessBegin), then done by the PFS core or passed to
 * the underlying file system.
 */

#include <psp2kern/kernel/sysclib.h>
#include "pfsmgr_internal.h"

#define PFS_DEVCTL_GROUP	4	/* cmd >> 12: PFS's own devctls and ioctls */
#define PFS_DEVCTL_FREE_SPACE	0x80000001	/* name unknown */

#define PFS_STAT_MODE_MASK	0xff0fffff	/* name unknown */

static inline int pfsIsRoot(SceVfsVnode *vp)	/* name unknown */
{
	return vp->core.type >= SCE_VNODE_TYPE_ROOTDIR;
}

int pfsGdsdDevctl(ScePfsPmi *pmi, SceVfsMount *mnt, const char *dev, unsigned int cmd,
		  const void *arg, SceSize argLen, void *buf, SceSize bufLen)
{
	SceVfsMount *lower;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DEVCTL, cmd, &perm);
	if (ret != 0)
		return ret;
	if (cmd >> 12 == PFS_DEVCTL_GROUP) {
		ret = pfsCoreDevctl(pmi, dev, cmd, arg, argLen, buf, bufLen);
	} else {
		lower = mnt->mnt_vnode->core.mnt;
		if (lower == NULL) {
			pfsAccessEnd(perm);
			return SCE_ERROR_ERRNO_EINVAL;
		}
		ret = ksceVfsOpDevctl(lower, dev, cmd, arg, argLen, buf, bufLen);
		/* report at most 4 GiB */
		if (ret >= 0 && cmd == PFS_DEVCTL_FREE_SPACE && *(SceUInt64 *)buf >> 32 != 0)
			*(SceUInt64 *)buf = 0xffffffff;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdMount(ScePfsPmi *pmi, SceVfsMount *mnt, SceVfsPath *devFilePath)
{
	ScePfsNode root;
	int perm, ret;

	mnt->available_entry_num = 0x20;
	mnt->default_io_cache_size = 0x8000;
	ret = pfsAccessBegin(pmi, PFS_OP_MOUNT, 0, &perm);
	if (ret != 0)
		return ret;
	root.vp = mnt->mnt_vnode;
	root.name = NULL;
	ret = pfsCoreMount(pmi, &root);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdUmount(ScePfsPmi *pmi, SceVfsMount *mnt, int flags)
{
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_UMOUNT, 0, &perm);
	if (ret != 0)
		return ret;
	ret = pfsCoreUmount(pmi, flags);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdSync(ScePfsPmi *pmi, SceVfsMount *mnt, int flags)
{
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_SYNC, 0, &perm);
	if (ret != 0)
		return ret;
	ret = pfsCoreSync(pmi, flags);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdDecodePathElem(ScePfsPmi *pmi, SceVfsMount *mnt, const char *path, const char **path2,
			  const char **path3, char *buf, SceSize bufLen, SceSize *decodeLen)
{
	SceVfsMount *lower = mnt->mnt_vnode->core.mnt;
	int perm, ret;

	if (lower == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	ret = pfsAccessBegin(pmi, PFS_OP_DECODE_PATH_ELEM, 0, &perm);
	if (ret != 0)
		return ret;
	ret = ksceVfsOpDecodePathElem(lower, path, path2, path3, buf, bufLen, decodeLen);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdSetRoot(ScePfsPmi *pmi, SceVfsMount *mnt, int unk, SceVfsVnode *vp)
{
	SceVfsVnode *lower;
	ScePfsVd *vd;
	char name[0x40];
	SceSize nameLen;
	SceVfsPath path;
	SceIoStat stat;
	int ret;

	vd = pfsBufferListAlloc(&g_pfsVdBufferList);
	if (vd == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	lower = mnt->mnt_vnode;
	vd->index = -1;
	vd->attr = PFS_ATTR_DIR;
	vd->pages.next = &vd->pages;
	vd->pages.prev = &vd->pages;
	vd->lowerVp = lower;
	vd->icvVp = NULL;
	vd->index = 0;
	*(SceUInt64 *)vp->core.fid = *(SceUInt64 *)lower->core.fid;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	vp->core.mnt_opt = lower->core.mnt_opt | 0x10000;
	vp->core.node_data = vd;
	vp->core.mnt = mnt;
	vp->core.dd = NULL;
	*(SceUInt64 *)vp->core.acl_data = PFS_ATTR_DIR;

	vfsGetVnodeName(lower, name, sizeof(name), &nameLen);
	path.name = name;
	path.name_length = strnlen(name, sizeof(name) - 1);
	path.path = NULL;
	memset(&stat, 0, sizeof(stat));
	vfsLockVnode(lower);
	ret = ksceVopGetstat(lower, &path, &stat);
	vfsUnlockVnode(lower);
	if (ret < 0)
		vp->core.size = PFS_ATTR_DIR;
	else
		vp->core.size = stat.st_size;
	return 0;
}

int pfsGdsdLookup(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		  SceUInt32 flags)
{
	ScePfsNode node;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_LOOKUP, 0, &perm);
	if (ret != 0)
		return ret;
	node.vp = dvp;
	node.name = path->name;
	ret = pfsCoreLookup(pmi, &node, flags);
	if (ret == 0)
		*vpp = node.vp;
	else if (ret == SCE_PFS_ERROR_NOT_FOUND)
		ret = SCE_ERROR_ERRNO_ENOENT;
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdOpen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceVfsPath *path, int flags)
{
	ScePfsNode node;
	ScePfsFi *fi;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_OPEN, flags, &perm);
	if (ret != 0)
		return ret;
	fi = pfsBufferListAlloc(&g_pfsFiBufferList);
	if (fi == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		node.vp = vp;
		node.name = path->name;
		fi->type = PFS_FI_FILE;
		ret = pfsCoreOpen(fi, pmi, &node, flags);
		if (ret == 0)
			file->fd = (SceUInt32)fi;
		else
			pfsBufferListFree(&g_pfsFiBufferList, fi);
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdCreate(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		  int flags, int mode)
{
	ScePfsNode node;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CREATE, 0, &perm);
	if (ret != 0)
		return ret;
	node.vp = dvp;
	node.name = path->name;
	ret = pfsCoreCreate(pmi, &node, mode, PFS_ATTR_DIR);
	if (ret == 0) {
		*vpp = node.vp;
		pfsAccessEnd(perm);
	} else {
		pfsAccessEnd(perm);
	}
	return ret;
}

int pfsGdsdClose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file)
{
	ScePfsFi *fi;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CLOSE, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (fi->type == PFS_FI_FILE && (ret = pfsCoreClose(fi, pmi)) == 0)
		pfsBufferListFree(&g_pfsFiBufferList, fi);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdRead(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte)
{
	ScePfsFi *fi;
	SceSize read;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_READ, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi->type != PFS_FI_FILE) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else if (file->position < (SceOff)fi->vp->core.size) {
		ret = pfsCoreRead(fi, pmi, buf, nbyte, file->position, &read);
		if (ret == 0) {
			file->position += read;
			ret = read;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdWrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		 SceSize nbyte)
{
	ScePfsFi *fi;
	SceSize written;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_WRITE, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (fi->type == PFS_FI_FILE) {
		ret = pfsCoreWrite(fi, pmi, buf, nbyte, file->position, &written);
		if (ret == 0) {
			file->position += written;
			fi->position += written;
			ret = written;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdPread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte,
		 SceOff offset)
{
	ScePfsFi *fi;
	SceSize read;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_PREAD, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi->type != PFS_FI_FILE) {
		ret = SCE_ERROR_ERRNO_EBADFD;
	} else if (offset < (SceOff)fi->vp->core.size) {
		ret = pfsCoreRead(fi, pmi, buf, nbyte, offset, &read);
		if (ret == 0)
			ret = read;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdPwrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		  SceSize nbyte, SceOff offset)
{
	ScePfsFi *fi;
	SceSize written;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_PWRITE, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (fi->type == PFS_FI_FILE) {
		ret = pfsCoreWrite(fi, pmi, buf, nbyte, offset, &written);
		if (ret == 0)
			ret = written;
	}
	pfsAccessEnd(perm);
	return ret;
}

SceOff pfsGdsdLseek(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceOff offset, int whence)
{
	SceOff position = 0;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_LSEEK, 0, &perm);
	if (ret != 0)
		return ret;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (((ScePfsFi *)file->fd)->type == PFS_FI_FILE) {
		position = file->position;
		ret = pfsCoreLseek((ScePfsFi *)file->fd, pmi, offset, whence, &position);
		if (ret == 0) {
			file->position = position;
			pfsAccessEnd(perm);
			return position;
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in,
		 SceSize inLen, void *out, SceSize outLen)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lower;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_IOCTL, cmd, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (fi->type == PFS_FI_FILE) {
		if ((unsigned int)cmd >> 12 == PFS_DEVCTL_GROUP) {
			ret = pfsCoreIoctl(pmi, vp, fi, cmd, in, inLen, out, outLen);
		} else if ((vd = vp->core.node_data) == NULL) {
			ret = SCE_ERROR_ERRNO_EINVAL;
		} else {
			lower = vd->lowerVp;
			vfsLockVnode(lower);
			ret = ksceVopIoctl(lower, fi->lower.file, cmd, in, inLen, out, outLen);
			vfsUnlockVnode(lower);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdInactive(SceVfsVnode *vp)
{
	ScePfsVd *vd = vp->core.node_data;
	SceVfsVnode *lowerVp, *icvVp;
	ScePfsNode node;

	if (vd == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	lowerVp = vd->lowerVp;
	icvVp = vd->icvVp;
	pfsBufferListFree(&g_pfsVdBufferList, vd);
	if (pfsIsRoot(vp))
		return 0;

	node.vp = lowerVp;
	node.name = NULL;
	pfsNodeRelease(&node);
	if (icvVp != NULL) {
		vfsLockVnode(icvVp);
		if (icvVp->core.ref_count > 1) {
			icvVp->core.ref_count--;
			vfsUnlockVnode(icvVp);
			return 0;
		}
		node.vp = icvVp;
		node.name = NULL;
		if (pfsNodeRelease(&node) != 0)
			vfsUnlockVnode(icvVp);
	}
	return 0;
}

int pfsGdsdRemove(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path,
		  SceUInt32 flags)
{
	ScePfsNode dir, node;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_REMOVE, 0, &perm);
	if (ret != 0)
		return ret;
	dir.vp = dvp;
	dir.name = NULL;
	node.vp = vp;
	node.name = path->name;
	ret = pfsCoreRemove(pmi, &dir, &node);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdMkdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int mode)
{
	ScePfsNode node;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_MKDIR, 0, &perm);
	if (ret != 0)
		return ret;
	node.vp = dvp;
	node.name = path->name;
	ret = pfsCoreMkdir(pmi, &node, mode);
	if (ret == 0) {
		*vpp = node.vp;
		pfsAccessEnd(perm);
	} else {
		pfsAccessEnd(perm);
	}
	return ret;
}

int pfsGdsdRmdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path)
{
	ScePfsNode dir, node;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_RMDIR, 0, &perm);
	if (ret != 0)
		return ret;
	dir.vp = dvp;
	dir.name = NULL;
	node.vp = vp;
	node.name = path->name;
	ret = pfsCoreRmdir(pmi, &dir, &node);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdDopen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file)
{
	ScePfsFi *fi;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DOPEN, 0, &perm);
	if (ret != 0)
		return ret;
	fi = pfsBufferListAlloc(&g_pfsFiBufferList);
	if (fi == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
	} else {
		fi->type = PFS_FI_DIR;
		ret = pfsCoreDopen(fi, pmi, vp, path);
		if (ret == 0) {
			file->fd = (SceUInt32)fi;
		} else {
			if (ret == SCE_PFS_ERROR_NOT_FOUND)
				ret = SCE_ERROR_ERRNO_ENOENT;
			pfsBufferListFree(&g_pfsFiBufferList, fi);
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdDread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir)
{
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DREAD, 0, &perm);
	if (ret != 0)
		return ret;
	if (((ScePfsFi *)file->fd)->type == PFS_FI_DIR)
		ret = pfsCoreDread((ScePfsFi *)file->fd, pmi, dir, perm);
	else
		ret = SCE_ERROR_ERRNO_EBADFD;
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdDclose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file)
{
	ScePfsFi *fi;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_DCLOSE, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi->type == PFS_FI_DIR) {
		ret = pfsCoreDclose(fi, pmi);
		if (ret == 0)
			pfsBufferListFree(&g_pfsFiBufferList, fi);
	} else {
		ret = SCE_ERROR_ERRNO_EBADFD;
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdGetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat)
{
	ScePfsVd *vd;
	SceVfsVnode *lower;
	ScePfsNode node;
	SceUInt64 size = 0;
	SceUInt16 attr = 0;
	SceUInt32 mode;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_GETSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.node_data;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	lower = vd->lowerVp;
	if (!pfsIsRoot(vp)) {
		node.vp = vp;
		node.name = path->name;
		ret = pfsCoreGetAttr(pmi, &node, &attr, &size);
		if (ret < 0)
			goto out;
	}
	vfsLockVnode(lower);
	ret = ksceVopGetstat(lower, path, stat);
	vfsUnlockVnode(lower);
	if (ret >= 0 && !pfsIsRoot(vp) && pfsAttrToMode(attr, &mode) == 0) {
		stat->st_attr = attr;
		if (attr & PFS_ATTR_DIR)
			stat->st_size = 0;
		else
			stat->st_size = size;
		stat->st_mode = mode & PFS_STAT_MODE_MASK;
	}
out:
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdFgetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lower;
	SceUInt32 mode;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_GETSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.node_data;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
	} else {
		fi = (ScePfsFi *)file->fd;
		lower = vd->lowerVp;
		if (fi == NULL) {
			pfsAccessEnd(perm);
			return SCE_ERROR_ERRNO_EBADFD;
		}
		vfsLockVnode(lower);
		ret = ksceVopFgetstat(lower, fi->lower.file, stat);
		vfsUnlockVnode(lower);
		if (ret >= 0 && !pfsIsRoot(vp)) {
			stat->st_attr = (SceUInt16)vp->core.acl_data[0];
			if (pfsAttrToMode(stat->st_attr, &mode) == 0) {
				if (stat->st_attr & PFS_ATTR_DIR)
					stat->st_size = 0;
				else
					stat->st_size = vp->core.size;
				stat->st_mode = mode & PFS_STAT_MODE_MASK;
			}
		}
	}
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdChstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lower;
	ScePfsNode node;
	SceUInt64 size;
	SceUInt16 attr;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CHSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	vd = vp->core.node_data;
	if (vd == NULL) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	lower = vd->lowerVp;
	if (!pfsIsRoot(vp)) {
		if (bit & SCE_CST_MODE) {
			ret = pfsModeToAttr(stat->st_mode, &attr);
			if (ret != 0)
				goto out;
			node.vp = vp;
			node.name = path->name;
			attr |= vd->attr & 0xff00;
			ret = pfsCoreSetAttr(pmi, &node, attr);
			if (ret < 0)
				goto out;
			vd->attr = attr;
			*(SceUInt64 *)vp->core.acl_data = attr;
		}
		if (bit & SCE_CST_SIZE) {
			size = stat->st_size;
			fi = pfsBufferListAlloc(&g_pfsFiBufferList);
			if (fi == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto out;
			}
			node.vp = vp;
			node.name = path->name;
			fi->type = PFS_FI_FILE;
			ret = pfsCoreOpen(fi, pmi, &node, SCE_O_RDWR);
			if (ret != 0) {
				if (ret == SCE_PFS_ERROR_NOT_FOUND)
					ret = SCE_ERROR_ERRNO_ENOENT;
				pfsBufferListFree(&g_pfsFiBufferList, fi);
				goto out;
			}
			ret = pfsCoreTruncate(fi, pmi, size);
			if (ret != 0) {
				pfsCoreClose(fi, pmi);
				pfsBufferListFree(&g_pfsFiBufferList, fi);
				goto out;
			}
			vp->core.size = size;
			pfsCoreClose(fi, pmi);
			pfsBufferListFree(&g_pfsFiBufferList, fi);
		}
		bit &= ~(SCE_CST_MODE | SCE_CST_SIZE);
	}
	vfsLockVnode(lower);
	ret = ksceVopChstat(lower, path, stat, bit);
	vfsUnlockVnode(lower);
out:
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdFchstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat,
		   SceUInt32 bit)
{
	ScePfsVd *vd;
	ScePfsFi *fi;
	SceVfsVnode *lower;
	ScePfsNode node;
	char name[0x40];
	SceSize nameLen;
	SceUInt64 size;
	SceUInt16 attr;
	int notFound, perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CHSTAT, 0, &perm);
	if (ret != 0)
		return ret;
	fi = (ScePfsFi *)file->fd;
	if (fi == NULL) {
		ret = SCE_ERROR_ERRNO_EBADFD;
		goto out;
	}
	vd = vp->core.node_data;
	if (vd == NULL) {
		pfsAccessEnd(perm);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	lower = vd->lowerVp;
	ret = vfsGetVnodeName(vp, name, sizeof(name), &nameLen);
	if (ret != 0)
		goto out;
	if (!pfsIsRoot(vp)) {
		node.vp = vp;
		node.name = name;
		ret = pfsCoreGetAttr(pmi, &node, &attr, &size);
		if (ret == SCE_PFS_ERROR_NOT_FOUND)
			notFound = 1;
		else if (ret < 0)
			goto out;
		else
			notFound = 0;
		if (bit & SCE_CST_MODE) {
			ret = pfsModeToAttr(stat->st_mode, &attr);
			if (ret != 0)
				goto out;
			attr |= vd->attr & 0xff00;
			if (!notFound) {
				node.vp = vp;
				node.name = name;
				ret = pfsCoreSetAttr(pmi, &node, attr);
				if (ret < 0)
					goto out;
			}
			vd->attr = attr;
			*(SceUInt64 *)vp->core.acl_data = attr;
		}
		if (bit & SCE_CST_SIZE) {
			size = stat->st_size;
			ret = pfsCoreTruncate(fi, pmi, size);
			if (ret != 0)
				goto out;
			vp->core.size = size;
		}
		bit &= ~(SCE_CST_MODE | SCE_CST_SIZE);
	}
	vfsLockVnode(lower);
	ret = ksceVopFchstat(lower, fi->lower.file, stat, bit);
	vfsUnlockVnode(lower);
out:
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdRename(ScePfsPmi *pmi, SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *oldPath,
		  SceVfsVnode *ndvp, SceVfsVnode **nvpp, SceVfsPath *newPath)
{
	ScePfsNode oldDir, oldNode, newNode;
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_RENAME, 0, &perm);
	if (ret != 0)
		return ret;
	oldNode.vp = ovp;
	oldNode.name = oldPath->name;
	if (ndvp == NULL)
		ndvp = odvp;
	newNode.name = newPath->name;
	oldDir.vp = odvp;
	oldDir.name = NULL;
	newNode.vp = ndvp;
	ret = pfsCoreRename(pmi, &oldDir, &oldNode, &newNode);
	if (ret == 0)
		*nvpp = newNode.vp;
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdVnodeSync(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int flags)
{
	ScePfsFi *fi;
	int perm, ret = 0;

	if (file != NULL && (ret = pfsAccessBegin(pmi, PFS_OP_VNODE_SYNC, 0, &perm)) == 0) {
		fi = (ScePfsFi *)file->fd;
		if (fi->type == PFS_FI_FILE)
			ret = pfsCoreVnodeSync(fi, pmi, flags);
		else if (fi->type == PFS_FI_DIR)
			ret = 0;
		else
			ret = SCE_ERROR_ERRNO_EBADFD;
		pfsAccessEnd(perm);
	}
	return ret;
}

int pfsGdsdWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
		    SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5)
{
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_WHITEOUT, 0, &perm);
	if (ret != 0)
		return ret;
	ret = pfsCoreWhiteout(pmi, dvp, vp, oldPath, newPath, a4, a5);
	pfsAccessEnd(perm);
	return ret;
}

int pfsGdsdCleanup(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2,
		   SceUInt32 a3)
{
	int perm, ret;

	ret = pfsAccessBegin(pmi, PFS_OP_CLEANUP, 0, &perm);
	if (ret != 0)
		return ret;
	ret = SCE_ERROR_ERRNO_EBADFD;
	if (((ScePfsFi *)file->fd)->type == PFS_FI_FILE)
		ret = pfsCoreCleanup((ScePfsFi *)file->fd, pmi, a2, a3);
	pfsAccessEnd(perm);
	return ret;
}
