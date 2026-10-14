/*
 * SceDeci4pSDrfp: "deci4p_drfp_dev_fs", the host0: file system. Lookups and
 * opens ask the host for the file's information; reads and closes go to the
 * host by the host's file descriptor (SceVfsFile.fd).
 */

#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/vfs.h>
#include "sce/debug.h"
#include "sdrfp.h"

#define SDRFP_FS_NAME		"deci4p_drfp_dev_fs"


#define SDRFPFS_TRACE(_line, _fmt, ...)					\
	SCE_DBG_PRINTF_LEVEL(2, 0xf, _line, _fmt, ##__VA_ARGS__)

/* (stores, not memset) */
static inline void statZero(Deci4pFsStat *stat)
{
	stat->mode = 0;
	stat->dev = 0;
	stat->fid = 0;
	stat->size = 0;
	stat->atime = 0;
	stat->mtime = 0;
	stat->ctime = 0;
}

static int dev_drfp_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;

	SDRFPFS_TRACE(155, "args = %p, mp = %p\n", argp, mnt);
	mnt->available_entry_num = 0x80;
	mnt->default_io_cache_size = 0x2000;
	return 0;
}

/* name unknown */
static int dev_drfp_vfs_umount(SceVfsOpUmountArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_drfp_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;
	SceVfsVnode *vp = argp->vp;

	vp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	vp->core.node_data = mnt->data;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.mnt = mnt;
	vp->core.dd = NULL;
	vp->core.fid[0] = 0;
	vp->core.fid[1] = 0;
	vp->core.size = 0;
	vp->core.acl_data[0] = 0;
	vp->core.acl_data[1] = 0;
	return 0;
}

static int dev_drfp_vfs_init(SceVfsOpInitArgs *argp)
{
	SDRFPFS_TRACE(141, "args = %p\n", argp);
	return 0;
}

static int dev_drfp_vfs_fini(SceVfsOpFiniArgs *argp)
{
	SDRFPFS_TRACE(148, "args = %p\n", argp);
	return 0;
}

static int dev_drfp_vfs_decode_path_elem(SceVfsOpDecodePathElemArgs *argp)
{
	const char *path = argp->path;
	const char **start = argp->path2;
	const char **end = argp->path3;
	char *buf = argp->buf;
	SceSize bufsz = argp->buf_len;
	SceSize *len = argp->decode_len;

	SDRFPFS_TRACE(209, "args = %p, path = %s\n", argp, path);
	SDRFPFS_TRACE(211, "start = %p, end = %p, buf = %p, bufsz = %d\n", start, end, buf, bufsz);
	SDRFPFS_TRACE(212, "len = %p\n", len);
	*len = strnlen(path, 0x400);
	*start = path;
	*end = path + *len;
	strncpy(buf, path, bufsz);
	return 0;
}

static int dev_drfp_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp;
	SceVfsVnode **vpp = argp->vpp;
	const char *name = argp->path->name;
	SceVfsVnode *vp;
	Deci4pFsStat stat;
	int ret;

	SDRFPFS_TRACE(234, "args = %p, %p/%p, %s\n", argp, dvp, vpp, name);
	SDRFPFS_TRACE(235, "name_len = %d\n", argp->path->name_length);
	statZero(&stat);
	ret = sdrfpGetstat(name, &stat);
	SDRFPFS_TRACE(240, "stat ret = 0x%x\n", ret);
	if (ret != 0)
		return ret;

	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mnt_vfs_inf->default_vops,
			     0, &vp);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 250, "vfsGetNewVnode failed (0x%08x)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	if (stat.mode & DECI4P_FS_MODE_IFREG) {
		vp->core.type = SCE_VNODE_TYPE_REG;
	} else if (stat.mode & DECI4P_FS_MODE_IFDIR) {
		vp->core.type = SCE_VNODE_TYPE_DIR;
	} else {
		SCE_PRINTF_WITH_INFO(0xf, 262, "unknown file type 0x%x\n", stat.mode);
		vp->core.type = SCE_VNODE_TYPE_REG;
	}
	if (stat.fid == 0) {
		vp->core.fid[0] = (SceUInt32)vp;
		vp->core.fid[1] = 0;
	} else {
		vp->core.fid[0] = stat.fid;
		vp->core.fid[1] = stat.fid >> 32;
	}
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.node_data = NULL;
	vp->core.size = stat.size;
	*vpp = vp;
	return 0;
}

static int dev_drfp_vop_open(SceVopOpenArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceVfsFile *file = argp->file;
	const char *name = argp->path->name;
	int flags = argp->flags;
	Deci4pFsStat stat;
	int fd;

	SDRFPFS_TRACE(287, "args = %p, vp = %p, file = %p\n", argp, vp, file);
	SDRFPFS_TRACE(288, "name = %s, flag = %o\n", name, flags);
	statZero(&stat);
	fd = sdrfpOpen(name, flags, 0, &stat);
	if (fd < 0)
		return fd;

	file->fd = fd;
	SDRFPFS_TRACE(299, "fd = %d\n", fd);
	if (stat.fid == 0) {
		vp->core.fid[0] = (SceUInt32)vp;
		vp->core.fid[1] = 0;
	} else {
		vp->core.fid[0] = stat.fid;
		vp->core.fid[1] = stat.fid >> 32;
	}
	vp->core.size = stat.size;
	return 0;
}

static int dev_drfp_vop_close(SceVopCloseArgs *argp)
{
	SceUInt32 fd = argp->file->fd;
	int ret;

	SDRFPFS_TRACE(319, "args = %p, vp = %p, file = %p\n", argp, argp->vp, argp->file);
	SDRFPFS_TRACE(320, "fd = %d\n", fd);
	ret = sdrfpClose(fd);
	if (ret == DRFP_ERROR_8009000A)
		ret = 0;
	return ret;
}

static SceSSize dev_drfp_vop_read(SceVopReadArgs *argp)
{
	SceVfsFile *file = argp->file;
	SceUInt32 fd = file->fd;
	void *buf = argp->buf;
	SceSize nbyte = argp->nbyte;
	SceOff offset = file->position;
	int ret;

	SDRFPFS_TRACE(344, "args = %p, vp = %p, file = %p\n", argp, argp->vp, file);
	SDRFPFS_TRACE(345, "fd = %d, buf = %p, nbyte = %d\n", fd, buf, nbyte);
	SDRFPFS_TRACE(346, "offset = 0x%016llx\n", offset);
	ret = sdrfpRead(fd, buf, offset, nbyte);
	if (ret >= 0)
		file->position += ret;
	return ret;
}

static SceOff dev_drfp_vop_lseek(SceVopLseekArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceVfsFile *file = argp->file;
	SceOff offset = argp->offset;
	int whence = argp->whence;

	SDRFPFS_TRACE(366, "args = %p, vp = %p, file = %p\n", argp, vp, file);
	SDRFPFS_TRACE(368, "fd = %d, offset = 0x%016llx, whence = %d\n", file->fd, offset, whence);
	switch (whence) {
	case SCE_SEEK_SET:
		break;
	case SCE_SEEK_CUR:
		offset += file->position;
		break;
	case SCE_SEEK_END:
		offset += vp->core.size;
		break;
	default:
		SCE_DBG_ASSERT(0, 381);
		return file->position;
	}
	file->position = offset;
	return offset;
}

static int dev_drfp_vop_inactive(SceVopInactiveArgs *argp)
{
	SDRFPFS_TRACE(391, "args = %p\n", argp);
	return 0;
}

static const SceVfsOpTable s_vfsOps = {
	.vfs_mount = dev_drfp_vfs_mount,
	.vfs_umount = dev_drfp_vfs_umount,
	.vfs_set_root = dev_drfp_vfs_set_root,
	.vfs_init = dev_drfp_vfs_init,
	.vfs_fini = dev_drfp_vfs_fini,
	.vfs_decode_path_elem = dev_drfp_vfs_decode_path_elem,
};

static const SceVopTable s_vops = {
	.vop_open = dev_drfp_vop_open,
	.vop_close = dev_drfp_vop_close,
	.vop_lookup = dev_drfp_vop_lookup,
	.vop_read = dev_drfp_vop_read,
	.vop_lseek = dev_drfp_vop_lseek,
	.vop_inactive = dev_drfp_vop_inactive,
};

static SceVfsInfo s_vfsInfo = {
	.vfs_ops = &s_vfsOps,
	.type = SCE_VFS_TYPE_DEVFS,
	.default_vops = &s_vops,
};

static const SceVfsMountData s_mountData = {
	.assign_name = "host0:",
	.fs_name = SDRFP_FS_NAME,
	.blockdev_name = SDRFP_FS_NAME,
};

static const SceVfsMountParam s_mountParam = {
	.root_path = "/host",
	.fs_type = SCE_VFS_FS_TYPE_HOSTFS,
	.opt = 0x300,
	.mnt_flags = SCE_VFS_MOUNT_FLAG_REMOTE | SCE_VFS_MOUNT_FLAG_NOBUF | SCE_VFS_MOUNT_TYPE_HOSTFS,
	.vfs_name = SDRFP_FS_NAME,
	.misc = (SceVfsMountData *)&s_mountData,
};

int sdrfpVfsMount(void)
{
	int ret;

	s_vfsInfo.vfs_name = SDRFP_FS_NAME;
	s_vfsInfo.vfs_name_len = sizeof(SDRFP_FS_NAME);
	ret = ksceVfsAddVfs(&s_vfsInfo);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 120, "sceVfsAddVfs failed (0x%08X)\n", ret);
		return ret;
	}
	ret = vfsMount((SceVfsMountParam *)&s_mountParam);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 127, "vfsMount failed (0x%08X)\n", ret);
		return ret;
	}
	return ret;
}
