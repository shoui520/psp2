/*
 * SceIofilemgr: the "tty" device (Sony's dummy_stdio.c).
 *
 * "dummy_ttyp_dev_fs" mounts tty0: ... ttyN:; what is written to a tty goes to
 * the kernel debug console, reads and the other calls do nothing.
 */

#include "iofilemgr_internal.h"
#include <fios2/fios2_errors.h>

/* the vfs and its tables (names unknown) */
SceVfsInfo g_dummyTtyVfsInfo;
extern const SceVfsOpTable g_dummyTtyVfsOps;
extern const SceVopTable g_dummyTtyVops;
extern const SceVfsMountParam g_dummyTtyMountParam;	/* tty0: */

/* the vnode of a tty: a character device (name unknown) */
#define DUMMY_TTY_VNODE_TYPE	(SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)

/* the entries a tty mount can open (name unknown) */
#define DUMMY_TTY_ENTRY_NUM	0x40

int dev_tty_vfs_fini(SceVfsOpFiniArgs *argp)
{
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 88, "args = %p\n", argp);
	return 0;
}

int dev_tty_vfs_init(SceVfsOpInitArgs *argp)
{
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 81, "args = %p\n", argp);
	return 0;
}

int dev_tty_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mp = argp->mnt;

	SCE_DBG_PRINTF_LEVEL(2, 0xf, 95, "args = %p, mp = %p\n", argp, mp);
	mp->availableEntryNum = DUMMY_TTY_ENTRY_NUM;
	return 0;
}

/* names unknown: the calls a tty does not need */
int dev_tty_vfs_umount(SceVfsOpUmountArgs *argp)
{
	return 0;
}

int dev_tty_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;
	SceVfsVnode *vp = argp->vp;

	vp->core.nodeData = mnt->pData;
	vp->core.mnt = mnt;
	vp->core.type = SCE_VNODE_TYPE_ROOTDIR_DEVFS;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.dd = NULL;
	*(SceUInt64 *)vp->core.fid = 0;
	vp->core.size = 0;
	*(SceUInt64 *)vp->core.aclData = 0;
	return 0;
}

int dev_tty_vop_open(SceVopOpenArgs *argp)
{
	return 0;
}

int dev_tty_vop_close(SceVopCloseArgs *argp)
{
	return 0;
}

SceSSize dev_tty_vop_read(SceVopReadArgs *argp)
{
	return 0;
}

SceOff dev_tty_vop_lseek(SceVopLseekArgs *argp)
{
	return 0;
}

int dev_tty_vop_ioctl(SceVopIoctlArgs *argp)
{
	return 0;
}

int dev_tty_vop_inactive(SceVopInactiveArgs *argp)
{
	return 0;
}

/* name unknown: the bytes go to the debug console */
SceSSize dev_tty_vop_write(SceVopWriteArgs *argp)
{
	const char *buf = argp->buf;
	SceSize nbyte = argp->nbyte;
	const char *p;

	if (nbyte != 0) {
		p = buf;
		do {
			SceDebugForKernel_2AABAEDA(*p++);
		} while (p != buf + nbyte);
	}
	return nbyte;
}

int dev_tty_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp;
	SceVfsVnode **vpp = argp->vpp;
	const char *name = argp->path->name;
	SceSize len = argp->path->nameLength;
	SceVfsVnode *vp;
	int unit, ret;

	SCE_DBG_PRINTF_LEVEL(2, 0xf, 148, "args = %p, %p/%p, %s, %d\n", argp, dvp, vpp, name,
			     len);
	if (len != 4 || name[0] != 't' || name[1] != 't' || name[2] != 'y')
		return SCE_ERROR_ERRNO_ENOENT;

	unit = name[3] - '0';
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 161, "unit = %d\n", unit);
	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mntVfsInf->defaultVops,
			     0, &vp);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 167, "vfsGetNewNode failed (0x%08X)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	vp->core.nodeData = (void *)unit;
	vp->core.type = DUMMY_TTY_VNODE_TYPE;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	*vpp = vp;
	return 0;
}

int DummyTtyStart(void)
{
	int ret;

	g_dummyTtyVfsInfo.vfsOps = &g_dummyTtyVfsOps;
	g_dummyTtyVfsInfo.vfsName = "dummy_ttyp_dev_fs";
	g_dummyTtyVfsInfo.type = SCE_VFS_TYPE_DEVFS;
	g_dummyTtyVfsInfo.defaultVops = &g_dummyTtyVops;
	g_dummyTtyVfsInfo.vfsNameLen = sizeof("dummy_ttyp_dev_fs");
	ret = sceVfsAddVfs(&g_dummyTtyVfsInfo);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 246, "sceVfsAddVfs failed (0x%08X)\n", ret);
		return ret;
	}
	ret = vfsMount((SceVfsMountParam *)&g_dummyTtyMountParam);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 265, "vfsMount failed (0x%08X)\n", ret);
		return ret;
	}
	return ret;
}

/* name unknown */
__attribute__((noinline)) int fiosErrorToErrno(int error)
{
	switch (error) {
	case SCE_FIOS_ERROR_CANT_ALLOCATE_DH:
	case SCE_FIOS_ERROR_BAD_SIZE:
		return SCE_ERROR_ERRNO_ENOMEM;
	case SCE_FIOS_ERROR_BAD_PATH:
		return SCE_ERROR_ERRNO_ENOENT;
	case SCE_FIOS_ERROR_BAD_PTR:
		return SCE_ERROR_ERRNO_EFAULT;
	case SCE_FIOS_ERROR_BAD_DH:
		return SCE_ERROR_ERRNO_EBADF;
	case SCE_FIOS_ERROR_NOT_A_FILE:
		return SCE_ERROR_ERRNO_EISDIR;
	case SCE_FIOS_ERROR_NOT_A_DIRECTORY:
		return SCE_ERROR_ERRNO_ENOTDIR;
	case SCE_FIOS_ERROR_PATH_TOO_LONG:
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

int convertFiosError(int error)
{
	if (((unsigned int)error << 4) >> 20 != SCE_ERROR_FACILITY_FIOS)
		return error;
	return fiosErrorToErrno(error);
}

/* name unknown: a failed user mapping is a bad address */
int ioUserMapErrorToErrno(int err)
{
	if (err == SCE_KERNEL_ERROR_VA2PA_FAULT)
		err = SCE_ERROR_ERRNO_EFAULT;
	return err;
}

static const SceVfsMountData dummyTtyMountData = {
	.assignName = "tty",
	.fsName = "dummy_ttyp_dev_fs",
	.blockdevName = "dummy_ttyp_dev_fs",
};

const SceVfsMountParam g_dummyTtyMountParam = {
	.rootPath = "/tty",
	.fsType = SCE_VFS_FS_TYPE_CHRDEV,
	.opt = 0x400,
	.mntFlags = SCE_VFS_MOUNT_TYPE_DEVFS | SCE_VFS_MOUNT_FLAG_NOBUF |
		     SCE_VFS_MOUNT_FLAG_NUMBERED,
	.vfsName = "dummy_ttyp_dev_fs",
	.misc = (SceVfsMountData *)&dummyTtyMountData,
};

const SceVopTable g_dummyTtyVops = {
	.vop_open = dev_tty_vop_open,
	.vop_close = dev_tty_vop_close,
	.vop_lookup = dev_tty_vop_lookup,
	.vop_read = dev_tty_vop_read,
	.vop_write = dev_tty_vop_write,
	.vop_lseek = dev_tty_vop_lseek,
	.vop_ioctl = dev_tty_vop_ioctl,
	.vop_inactive = dev_tty_vop_inactive,
};

const SceVfsOpTable g_dummyTtyVfsOps = {
	.vfs_mount = dev_tty_vfs_mount,
	.vfs_umount = dev_tty_vfs_umount,
	.vfs_set_root = dev_tty_vfs_set_root,
	.vfs_init = dev_tty_vfs_init,
	.vfs_fini = dev_tty_vfs_fini,
};
