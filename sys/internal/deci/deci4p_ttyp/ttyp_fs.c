/*
 * SceDeci4pTtyp: "deci4p_ttyp_dev_fs", the tty0: .. tty7: devices.
 */

#include <sysmem/sysmem.h>
#include <iofilemgr/vfs.h>
#include <sysmem/debug.h>
#include "ttyp.h"

#define TTYP_FS_NAME		"deci4p_ttyp_dev_fs"

/* ioctls */
#define TTYP_IOCTL_SET_PID	0x6400		/* in: 8 bytes, the first word a process id */
#define TTYP_IOCTL_CLEAR_PID	0x6401

#if FW_DEBUG
#define TTYP_PRINTF(_line, _fmt, ...)	SCE_PRINTF_WITH_INFO(0xf, _line, _fmt, ##__VA_ARGS__)
#else
#define TTYP_PRINTF(_line, _fmt, ...)	do { } while (0)
#endif

static SceVfsInfo s_vfsInfo;

/* name unknown */
static int dev_tty_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;

	mnt->allocator = ttypGetHeap();
	mnt->availableEntryNum = 0x40;
	return 0;
}

/* name unknown */
static int dev_tty_vfs_umount(SceVfsOpUmountArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_tty_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;
	SceVfsVnode *vp = argp->vp;

	vp->core.nodeData = mnt->pData;
	vp->core.mnt = mnt;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = SCE_VNODE_TYPE_ROOTDIR_DEVFS;
	vp->core.dd = NULL;
	vp->core.fid[0] = 0;
	vp->core.fid[1] = 0;
	vp->core.size = 0;
	vp->core.aclData[0] = 0;
	vp->core.aclData[1] = 0;
	return 0;
}

/* name unknown */
static int dev_tty_vfs_init(SceVfsOpInitArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_tty_vfs_fini(SceVfsOpFiniArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_tty_vop_open(SceVopOpenArgs *argp)
{
	return TtypOpen((int)argp->vp->core.nodeData);
}

/* name unknown */
static int dev_tty_vop_close(SceVopCloseArgs *argp)
{
	SceVfsFile *file = argp->file;
	void *pid = (void *)file->fd;
	int unit = (int)argp->vp->core.nodeData;

	if (pid != NULL) {
		sceKernelFreeHeapMemory(ttypGetHeap(), pid);
		argp->file->fd = 0;
	}
	return TtypClose(unit);
}

/* "ttyN": unit N */
static int dev_tty_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp, **vpp = argp->vpp, *vp;
	const signed char *name = (const signed char *)argp->path->name;
	int unit, ret;

	if (argp->path->nameLength != 4 || name[0] != 't' || name[1] != 't' || name[2] != 'y')
		return SCE_ERROR_ERRNO_ENOENT;
	unit = name[3] - '0';
	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mntVfsInf->defaultVops,
			     0, &vp);
	if (ret < 0) {
		TTYP_PRINTF(303, "vfsGetNewNode failed (0x%08X)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	vp->core.nodeData = (void *)unit;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = SCE_VNODE_TYPE_CHRDEV;
	*vpp = vp;
	return 0;
}

static SceSSize dev_tty_vop_read(SceVopReadArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	int unit = (int)vp->core.nodeData;
	void *buf = argp->buf;
	SceSize nbyte = argp->nbyte;
	int ret, err;

	err = vfsUnlockVnode(vp);
	if (err < 0) {
		TTYP_PRINTF(362, "vfsUnlockVnode failed (0x%08x)\n", err);
		return err;
	}
	ret = TtypRead(unit, buf, nbyte, 0);
	err = vfsLockVnode(vp);
	if (err < 0) {
		TTYP_PRINTF(370, "vfsLockVnode failed (0x%08x)\n", err);
		return err;
	}
	return ret;
}

static SceSSize dev_tty_vop_write(SceVopWriteArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	int unit = (int)vp->core.nodeData;
	const SceUID *pid = (const SceUID *)argp->file->fd;
	const void *buf = argp->buf;
	SceSize nbyte = argp->nbyte;
	int ret, err;

	err = vfsUnlockVnode(vp);
	if (err < 0) {
		TTYP_PRINTF(393, "vfsUnlockVnode failed (0x%08x)\n", err);
		return err;
	}
	ret = TtypWrite(unit, buf, nbyte, pid);
	err = vfsLockVnode(vp);
	if (err < 0) {
		TTYP_PRINTF(401, "vfsLockVnode failed (0x%08x)\n", err);
		return err;
	}
	return ret;
}

/*
 * SET_PID: the process whose output this file carries (from the kernel). The
 * copy hangs off SceVfsFile.fd, which this file system does not otherwise use.
 */
static int ioctl_impl(SceVopIoctlArgs *argp)
{
	SceVfsFile *file = argp->file;
	int unit = (int)argp->vp->core.nodeData;
	int cmd = argp->cmd;
	const SceUInt32 *in = argp->inData;
	SceSize inlen = argp->inLen;
	SceUInt32 *pid;
	SceUInt32 value;
	int ret;

	if (unit > TTYP_CHANNELS - 1)
		return SCE_ERROR_ERRNO_ENXIO;
	ret = ttypLock();
	if (ret < 0) {
		TTYP_PRINTF(170, "LockMutex failed (0x%08X)\n", ret);
		return ret;
	}
	switch (cmd) {
	case TTYP_IOCTL_SET_PID:
		if (inlen != 8) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			break;
		}
		pid = (SceUInt32 *)file->fd;
		if (pid == NULL) {
			pid = SceSysmemForDriver_7B4CB60A(ttypGetHeap(), inlen);
			file->fd = (SceUInt32)pid;
			if (pid == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				break;
			}
		}
		value = in[1];
		pid[0] = in[0];
		pid[1] = value;
		break;
	case TTYP_IOCTL_CLEAR_PID:
		pid = (SceUInt32 *)file->fd;
		if (pid != NULL) {
			sceKernelFreeHeapMemory(ttypGetHeap(), pid);
			file->fd = 0;
		}
		break;
	default:
		ret = SCE_ERROR_ERRNO_EINVAL;
		TTYP_PRINTF(203, "invalid cmd (0x%08x)\n", cmd);
		break;
	}
	ttypUnlock();
	return ret;
}

/* name unknown */
static int dev_tty_vop_inactive(SceVopInactiveArgs *argp)
{
	return 0;
}

static const SceVfsOpTable s_vfsOps = {
	.vfs_mount = dev_tty_vfs_mount,
	.vfs_umount = dev_tty_vfs_umount,
	.vfs_set_root = dev_tty_vfs_set_root,
	.vfs_init = dev_tty_vfs_init,
	.vfs_fini = dev_tty_vfs_fini,
};

static const SceVopTable s_vops = {
	.vop_open = dev_tty_vop_open,
	.vop_close = dev_tty_vop_close,
	.vop_lookup = dev_tty_vop_lookup,
	.vop_read = dev_tty_vop_read,
	.vop_write = dev_tty_vop_write,
	.vop_ioctl = ioctl_impl,
	.vop_inactive = dev_tty_vop_inactive,
};

static const SceVfsMountData s_mountData = {
	.assignName = "tty",
	.fsName = TTYP_FS_NAME,
	.blockdevName = TTYP_FS_NAME,
};

static const SceVfsMountParam s_mountParam = {
	.rootPath = "/tty",
	.fsType = SCE_VFS_FS_TYPE_CHRDEV,
	.opt = 0x400,
	.mntFlags = SCE_VFS_MOUNT_FLAG_NUMBERED | SCE_VFS_MOUNT_FLAG_NOBUF | SCE_VFS_MOUNT_TYPE_DEVFS,
	.vfsName = TTYP_FS_NAME,
	.misc = (SceVfsMountData *)&s_mountData,
};

/* (writable: kept away from the operation tables) */
static SceVfsUmountParam s_umountParam = {
	.assignName = "/tty",
};

int TtypFsInitVfs(void)
{
	int ret;

	ret = vfsUmount(&s_umountParam);
	if (ret < 0) {
		TTYP_PRINTF(88, "vfsUmount failed (0x%08X)\n", ret);
		return ret;
	}
	s_vfsInfo.vfsOps = &s_vfsOps;
	s_vfsInfo.vfsName = TTYP_FS_NAME;
	s_vfsInfo.type = SCE_VFS_TYPE_DEVFS;
	s_vfsInfo.defaultVops = &s_vops;
	s_vfsInfo.vfsNameLen = sizeof(TTYP_FS_NAME);
	s_vfsInfo.vfsData = NULL;
	ret = sceVfsAddVfs(&s_vfsInfo);
	if (ret < 0) {
		TTYP_PRINTF(112, "sceVfsAddVfs failed (0x%08X)\n", ret);
		return ret;
	}
	ret = vfsMount((SceVfsMountParam *)&s_mountParam);
	if (ret < 0) {
		TTYP_PRINTF(131, "vfsMount failed (0x%08X)\n", ret);
		return ret;
	}
	return ret;
}
