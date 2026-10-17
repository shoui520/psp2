/*
 * ScePfsMgrForKernel: mounting, approving and unmounting PFS pseudo drives,
 * and the module's start.
 */

#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/suspend.h>
#include <sysmem/utils.h>
#include <psp2kern/io/fcntl.h>
#include <psp2kern/io/devctl.h>
#include <psp2kern/io/dirent.h>
#include <sysmem/sblaimgr.h>
#include "pfsmgr_internal.h"

#define PFS_IOCTL_ACID_DIR_SET		0x4401
#define PFS_IOCTL_ACID_DIR_MOUNT	0x4402
#define PFS_IOCTL_ACID_DIR_APPROVE	0x4403
#define PFS_IOCTL_ACID_DIR_UNMOUNT	0x4404

#define PFS_DEVCTL_GET_SIZE		0x4410	/* name unknown */
#define PFS_DEVCTL_SET_4411		0x4411	/* name unknown */
#define PFS_DEVCTL_GET_4413		0x4413	/* name unknown */
#define PFS_DEVCTL_4414			0x4414	/* name unknown */

#define PFS_MODE_CHECK_APPROVED		0x28	/* kscePfsMount: only check an existing mount */

SceVfsInfo g_pfsRedirectVfsInfo = {
	.vfs_ops	= &g_pfsRedirectVfsOps,
	.vfs_name	= "PFS_REDIRECT_INF",
	.vfs_name_len	= sizeof("PFS_REDIRECT_INF"),
	.default_vops	= &g_pfsRedirectVops,
};

SceVfsInfo g_pfsGdsdVfsInfo = {
	.vfs_ops	= &g_pfsGdsdVfsOps,
	.vfs_name	= "PFS_GDSD_INF",
	.vfs_name_len	= sizeof("PFS_GDSD_INF"),
	.default_vops	= &g_pfsGdsdVops,
};

SceVfsInfo g_pfsAcVfsInfo = {
	.vfs_ops	= &g_pfsAcVfsOps,
	.vfs_name	= "PFS_AC_INF",
	.vfs_name_len	= sizeof("PFS_AC_INF"),
	.default_vops	= &g_pfsAcVops,
};

SceUID g_pfsPmiListMutex;
ScePfsListNode g_pfsPmiList;
SceInt32 g_pfsBusy;

static inline int pfsNameIsValid(const char *name)	/* name unknown */
{
	return name != NULL && strnlen(name, PFS_MOUNT_NAME_MAX) - 1 <= PFS_MOUNT_NAME_MAX - 2;
}

static inline int pfsModeIsAc(unsigned int mode)	/* name unknown */
{
	return (mode & ~8) == 4 || mode == 8;
}

/* called by sysroot: what a PFS mount is */
int ScePfsMgrForKernel_4C148288(SceVfsMount *mount, ScePfsMountInfo *info)
{
	ScePfsPmi *pmi, *child;
	ScePfsListNode *node;
	int i;

	if (mount->fs_type != SCE_VFS_FS_TYPE_PFS)
		return SCE_ERROR_ERRNO_EINVAL;

	pmi = (ScePfsPmi *)mount->data;
	info->mount = mount;
	info->quota = pmi->quota;
	info->path = pmi->path;
	info->mode = pmi->mode;
	info->version = pmi->version;
	info->rootPath = pmi->rootPath;
	info->assignName = pmi->assignName;
	info->approved = pmi->approved;
	info->acidDirCount = 0;
	info->approvedMax = pmi->approvedMax;
	for (i = 0; i < 16; i++)
		info->acidDirs[i] = NULL;
	if (!pfsModeIsAc(pmi->mode))
		return 0;

	node = pmi->acidDirs.next;
	if (node == &pmi->acidDirs) {
		i = 0;
	} else {
		child = (ScePfsPmi *)node;
		info->acidDirs[0] = child->path;
		for (i = 1; ; i++) {
			node = node->next;
			if (node == &pmi->acidDirs)
				break;
			child = (ScePfsPmi *)node;
			info->acidDirs[i] = child->path;
			if (i + 1 == 16) {
				i = 16;
				break;
			}
		}
	}
	info->acidDirCount = i;
	return 0;
}

/* no suspend while PFS is busy */
static int pfsSysEventHandler(int resume, int eventid, void *args, void *opt)	/* name unknown */
{
	if (resume == 0 && eventid == 0x402)
		return g_pfsBusy != 0 ? -1 : 0;
	return 0;
}

int kscePfsMount2(const char *path, const char *name, const void *klicensee, SceUInt16 mode)
{
	SceVfsMountParam param;
	SceVfsUmountParam umountParam;
	ScePfsPmi *pmi;
	ScePfsListNode *first;
	unsigned int bit;
	int ret;

	if (!pfsNameIsValid(name) || path == NULL)
		return SCE_ERROR_ERRNO_EINVAL;

	ksceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret != 0)
		goto out;
	ret = SCE_ERROR_ERRNO_EEXIST;
	if (pmi != NULL)
		goto out;
	ret = pfsPmiCreate(&pmi, path, name, klicensee, mode);
	if (ret != 0)
		goto out;

	pmi->flags |= PFS_PMI_NOT_APPROVED;
	if (mode > 23)
		goto bad_mode;
	bit = 1 << mode;
	if (bit & PFS_MODE_GDSD_MASK) {
		pmi->mountData.fs_name = "Gamedata/Savedata Pseudo Drive.";
		param.vfs_name = g_pfsGdsdVfsInfo.vfs_name;
	} else if (bit & PFS_MODE_AC_MASK) {
		pmi->mountData.fs_name = "AC Pseudo Drive.";
		param.vfs_name = g_pfsAcVfsInfo.vfs_name;
	} else if (bit & PFS_MODE_REDIRECT_MASK) {
		pmi->mountData.fs_name = "Redirect Pseudo Drive.";
		param.vfs_name = g_pfsRedirectVfsInfo.vfs_name;
	} else {
bad_mode:
		ret = SCE_PFS_ERROR_BAD_MODE;
		pfsPmiDestroy(pmi);
		goto out;
	}

	param.root_path = pmi->rootPath;
	param.blockdev_name = pmi->path;
	param.fs_type = SCE_VFS_FS_TYPE_PFS;
	param.opt = 0;
	param.mnt_flags = SCE_VFS_MOUNT_TYPE_PFS;
	param.data = pmi;
	param.misc = &pmi->mountData;
	param.vops = NULL;
	if (mode == 20 || mode - 12 <= 1 || mode == 2)
		param.mnt_flags = SCE_VFS_MOUNT_TYPE_PFS | SCE_VFS_MOUNT_FLAG_RDONLY;
	if (mode == 3)
		param.mnt_flags |= SCE_VFS_MOUNT_FLAG_NOBUF;
	ret = vfsMountForPFS(&param, 0, 0);
	if (ret < 0)
		goto destroy;

	first = g_pfsPmiList.next;
	if (first == NULL) {
		umountParam.assign_name = pmi->assignName;
		umountParam.flag = 0;
		vfsUmountForPFS(&umountParam, 0, 0);
		ret = SCE_PFS_ERROR_NOT_IN_LIST;
destroy:
		pfsPmiDestroy(pmi);
		goto out;
	}
	pmi->link.next = first;
	pmi->link.prev = first->prev;
	first->prev = &pmi->link;
	pmi->link.prev->next = &pmi->link;
	ret = 0;
out:
	ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

int kscePfsUnmount(const char *name)
{
	SceVfsUmountParam umountParam;
	ScePfsPmi *pmi, *mounted;
	ScePfsListNode *node, *next;
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;

	ksceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret != 0)
		goto out;
	if (pmi == NULL) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		goto out;
	}
	if (pmi->flags & PFS_PMI_NOT_MOUNTED) {
		ret = SCE_ERROR_ERRNO_ENODEV;
	} else {
		umountParam.assign_name = pmi->assignName;
		umountParam.flag = ret;
		ret = vfsUmountForPFS(&umountParam, 0, 0);
		if (ret < 0)
			goto out;
	}

	mounted = pmi;
	if (pfsModeIsAc(mounted->mode)) {
		ksceKernelLockMutex(mounted->mutex, 1, NULL);
		node = mounted->acidDirs.next;
		while (node != NULL && node != &mounted->acidDirs) {
			next = node->next;
			next->prev = node->prev;
			node->prev->next = next;
			pfsPmiDestroy((ScePfsPmi *)node);
			node = next;
		}
		ksceKernelUnlockMutex(mounted->mutex, 1);
	}
	next = mounted->link.next;
	next->prev = mounted->link.prev;
	mounted->link.prev->next = next;
	pfsPmiDestroy(mounted);
out:
	ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

int kscePfsApprove(const char *name, SceUInt64 authId)
{
	ScePfsPmi *pmi;
	int ret;

	if (pfsNameIsValid(name)) {
		ksceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
		ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
		if (ret == 0) {
			if (pmi == NULL) {
				ret = SCE_ERROR_ERRNO_ENODEV;
			} else {
				ret = pfsPmiApprove(pmi, authId);
				if (ret == 0)
					pmi->flags &= ~PFS_PMI_NOT_APPROVED;
			}
		}
		ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	return ret;
}

int kscePfsDisapprove(const char *name, SceUInt64 authId)
{
	ScePfsPmi *pmi;
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;

	ksceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret == 0) {
		if (pmi == NULL)
			ret = SCE_ERROR_ERRNO_ENODEV;
		else
			ret = pfsPmiDisapprove(pmi, authId);
	}
	ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

/* mounts and approves; mode 0x28 only checks an approved mount and returns its mode */
int kscePfsMount(const char *path, const char *name, SceUInt64 authId, const void *klicensee,
		 SceUInt16 mode)
{
	ScePfsPmi *pmi;
	int ret;

	if (mode != PFS_MODE_CHECK_APPROVED) {
		ret = kscePfsMount2(path, name, klicensee, mode);
		if (ret == 0)
			ret = kscePfsApprove(name, authId);
		return ret;
	}

	if (name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) > PFS_MOUNT_NAME_MAX - 1)
		return SCE_ERROR_ERRNO_EINVAL;
	ksceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret == 0) {
		if (pmi != NULL && pfsPmiIsApproved(pmi, authId)) {
			ret = pmi->mode;
			ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
			return ret;
		}
		ret = SCE_ERROR_ERRNO_ENODEV;
	}
	ksceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

/* the acid directory calls go to the mounted pseudo drive as ioctls on "<name>0:<dir>" */
static inline int pfsAcidDirIoctl(const char *name, const char *dir, unsigned int cmd,
				  const void *in, SceSize inSize)	/* name unknown */
{
	char path[0x80];
	SceUID fd;
	int ret;

	ret = snprintf(path, sizeof(path) - 1, "%s0:%s", name, dir);
	if (ret < 0)
		return ret;
	fd = ksceIoDopen(path);
	if (fd < 0)
		return fd;
	ret = ksceIoIoctl(fd, cmd, in, inSize, NULL, 0);
	if (ret != 0) {
		ksceIoDclose(fd);
		return ret;
	}
	return ksceIoDclose(fd);
}

int kscePfsAcidDirSet(const char *name, const char *dir)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_SET, NULL, 0);
}

int kscePfsAcidDirMount(const char *name, const char *dir, const void *klicensee)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_MOUNT, klicensee, 0x10);
}

int kscePfsAcidDirApprove(const char *name, const char *dir)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_APPROVE, NULL, 0);
}

int kscePfsAcidDirUnmount(const char *name, const char *dir)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_UNMOUNT, NULL, 0);
}

int ScePfsMgrForKernel_7E4B76E6(const char *name, void *out)
{
	char device[0x24];

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	return ksceIoDevctl(device, PFS_DEVCTL_GET_4413, NULL, 0, out, 0x404);
}

int ScePfsMgrForKernel_27FB7618(const char *name, SceUInt32 *out1, SceUInt32 *out2)
{
	char device[0x24];
	SceUInt32 result[2];
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	ret = ksceIoDevctl(device, PFS_DEVCTL_GET_SIZE, NULL, 0, result, sizeof(result));
	if (ret >= 0) {
		*out1 = result[0];
		*out2 = result[1];
	}
	return ret;
}

int ScePfsMgrForKernel_806D2EA1(const char *name, SceUInt32 value)
{
	char device[0x24];
	SceUInt32 in;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	in = value;
	return ksceIoDevctl(device, PFS_DEVCTL_SET_4411, &in, sizeof(in), NULL, 0);
}

int ScePfsMgrForKernel_E9D672AB(const char *name)
{
	char device[0x24];

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	return ksceIoDevctl(device, PFS_DEVCTL_4414, NULL, 0, NULL, 0);
}

int module_start(SceSize args, void *argp)
{
	int ret;

	ret = sceKernelRegisterSysEventHandler("ScePfsMgr", pfsSysEventHandler, NULL);
	if (ret < 0)
		return ret;
	SceSysrootForKernel_CC119171(ScePfsMgrForKernel_4C148288);

	ret = pfsGlobalInit();
	if (ret != 0)
		return ret;
	ret = pfsPmiInit();
	if (ret != 0)
		goto global;
	ret = pfsFiInit();
	if (ret != 0)
		goto pmi;
	ret = pfsVdInit();
	if (ret != 0)
		goto fi;
	ret = pfsLruInit();
	if (ret != 0)
		goto vd;

	pfsRandomInit(ksceKernelGetSystemTimeLow());
	g_pfsPmiList.next = &g_pfsPmiList;
	g_pfsPmiList.prev = &g_pfsPmiList;
	ret = ksceKernelCreateMutex("ScePfsThePmi", 0x2000, 0, NULL);
	g_pfsPmiListMutex = ret;
	if (ret < 0)
		goto lru;
	ret = ksceVfsAddVfs(&g_pfsRedirectVfsInfo);
	if (ret != 0)
		goto mutex;
	ret = ksceVfsAddVfs(&g_pfsGdsdVfsInfo);
	if (ret != 0)
		goto redirect;
	ret = ksceVfsAddVfs(&g_pfsAcVfsInfo);
	if (ret == 0)
		return 0;

	ksceVfsDeleteVfs(g_pfsGdsdVfsInfo.vfs_name, NULL);
redirect:
	ksceVfsDeleteVfs(g_pfsRedirectVfsInfo.vfs_name, NULL);
mutex:
	ksceKernelDeleteMutex(g_pfsPmiListMutex);
lru:
	pfsLruTerm();
vd:
	pfsVdTerm();
fi:
	pfsFiTerm();
pmi:
	pfsPmiTerm();
global:
	pfsGlobalTerm();
	return ret;
}
