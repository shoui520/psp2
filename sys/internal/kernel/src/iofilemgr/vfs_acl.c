/*
 * SceIofilemgr: vnode access checks through SceSblACMgr (Sony's vfs_acl.c).
 */

#include "iofilemgr_internal.h"

void set_attr_info(SceVfsVnode *vp, SceUInt64 *attr, SceSblACMgrValue *info, SceUID pid)
{
	SceVfsMount *mnt = vp->core.mnt;
	SceVfsMountData *mntData;
	SceUInt8 fsType;
	int ret;

	info->value = (unsigned char *)attr;
	fsType = mnt->fs_type;
	switch (fsType) {
	case SCE_VFS_FS_TYPE_FS:
		info->type = VFS_AC_FS_KIND_FS;
		info->size = 1;
		if (*attr == VFS_AC_ATTR_LONG) {
			mntData = mnt->mnt_data;
			if ((SceUInt8)mnt->mnt_flags != VFS_MOUNT_TYPE_LOOP)
				SCE_DBG_PRINTF(0xf, 68, "NO LOOP MNT [%x] but SCE_ELONG is set\n",
					       (SceUInt8)mnt->mnt_flags);
			if ((vp->core.type & SCE_VNODE_TYPE_ROOTDIR) != SCE_VNODE_TYPE_ROOTDIR)
				SCE_DBG_PRINTF(0xf, 73, "%p it not root vnode but SCE_ELONG is set\n", vp);
			ret = SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint(pid, info, info, mntData->mnt_id);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 82,
					       "sceSblACMgrSetFSAttributeByAttributeWithMountPoint() failed ... %x\n",
					       ret);
		}
		break;
	case 2:
		info->type = VFS_AC_FS_KIND_2;
		info->size = 4;
		if (*attr == VFS_AC_ATTR_LONG) {
			mntData = mnt->mnt_data;
			if ((SceUInt8)mnt->mnt_flags != VFS_MOUNT_TYPE_LOOP)
				SCE_DBG_PRINTF(0xf, 100, "NO LOOP MNT [%x] but SCE_ELONG is set\n",
					       (SceUInt8)mnt->mnt_flags);
			if ((vp->core.type & SCE_VNODE_TYPE_ROOTDIR) != SCE_VNODE_TYPE_ROOTDIR)
				SCE_DBG_PRINTF(0xf, 105, "%p it not root vnode but SCE_ELONG is set\n",
					       vp);
			ret = SceSblACMgrForKernel_sceSblACMgrSetFSAttributeByAttributeWithMountPoint(pid, info, info, mntData->mnt_id);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 114,
					       "sceSblACMgrSetFSAttributeByAttributeWithMountPoint() failed ... %x\n",
					       ret);
		}
		break;
	case SCE_VFS_FS_TYPE_BLKDEV:
		info->type = VFS_AC_FS_KIND_BLKDEV;
		info->size = 6;
		break;
	case SCE_VFS_FS_TYPE_PFS:
		info->type = VFS_AC_FS_KIND_PFS;
		info->size = 2;
		break;
	default:
		SCE_DBG_PRINTF(0xf, 134, "mnt->fsType = %d 0x%x\n", fsType, fsType);
		break;
	}
}

/* the access check every vfs_check_vnode_*_access makes, with its mode */
static inline __attribute__((always_inline)) int vfs_check_vnode_access(SceVfsVnode *vp,
									SceUInt32 mode)
{
	SceSblACMgrValue info;
	SceUInt64 attr;
	SceUID pid;

	if (vp->core.mnt->fs_type == SCE_VFS_FS_TYPE_HOSTFS ||
	    (vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) ==
		    (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR))
		return 0;
	pid = ksceKernelGetProcessIdFromTLS();
	attr = *(SceUInt64 *)vp->core.acl_data;
	set_attr_info(vp, &attr, &info, pid);
	return SceSblACMgrForKernel_75AAF981(pid, &info, mode);
}

int vfs_check_vnode_rw_access(SceVfsVnode *vp)
{
	return vfs_check_vnode_access(vp, VFS_AC_MODE_READ | VFS_AC_MODE_WRITE);
}

int vfs_check_vnode_read_access(SceVfsVnode *vp)
{
	return vfs_check_vnode_access(vp, VFS_AC_MODE_READ);
}

int vfs_check_vnode_write_access(SceVfsVnode *vp)
{
	return vfs_check_vnode_access(vp, VFS_AC_MODE_WRITE);
}

int vfs_check_vnode_chstat_access(SceVfsVnode *vp)
{
	return vfs_check_vnode_access(vp, VFS_AC_MODE_CHSTAT);
}

int sceVfsACCheckOpen(SceVfsVnode *vp, int flags)
{
	SceSblACMgrValue info;
	SceUInt64 attr;
	SceUInt32 mode;
	SceUID pid;
	int access;

	if (vp->core.mnt->fs_type == SCE_VFS_FS_TYPE_HOSTFS ||
	    (vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR)) ==
		    (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_CHR))
		return 0;
	pid = ksceKernelGetProcessIdFromTLS();
	attr = *(SceUInt64 *)vp->core.acl_data;
	access = flags & SCE_O_RDWR;
	set_attr_info(vp, &attr, &info, pid);
	switch (access) {
	case SCE_O_RDONLY:
		mode = VFS_AC_MODE_READ;
		break;
	case SCE_O_WRONLY:
		mode = VFS_AC_MODE_WRITE;
		break;
	case SCE_O_RDWR:
		mode = VFS_AC_MODE_READ | VFS_AC_MODE_WRITE;
		break;
	default:
		mode = 0;
		break;
	}
	return SceSblACMgrForKernel_75AAF981(pid, &info, mode);
}
