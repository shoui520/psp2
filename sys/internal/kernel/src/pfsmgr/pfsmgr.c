/*
 * ScePfsMgrForKernel: mounting, approving and unmounting PFS pseudo drives,
 * and the module's start.
 */

#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <sysmem/suspend.h>
#include <sysmem/utils.h>
#include <iofilemgr/iofilemgr.h>
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

#define PFS_MODE_CHECK_APPROVED		0x28	/* ScePfsMgrForKernel_A772209C: only check an existing mount */

SceVfsInfo g_pfsRedirectVfsInfo = {
	.vfsOps	= &g_pfsRedirectVfsOps,
	.vfsName	= "PFS_REDIRECT_INF",
	.vfsNameLen	= sizeof("PFS_REDIRECT_INF"),
	.defaultVops	= &g_pfsRedirectVops,
};

SceVfsInfo g_pfsGdsdVfsInfo = {
	.vfsOps	= &g_pfsGdsdVfsOps,
	.vfsName	= "PFS_GDSD_INF",
	.vfsNameLen	= sizeof("PFS_GDSD_INF"),
	.defaultVops	= &g_pfsGdsdVops,
};

SceVfsInfo g_pfsAcVfsInfo = {
	.vfsOps	= &g_pfsAcVfsOps,
	.vfsName	= "PFS_AC_INF",
	.vfsNameLen	= sizeof("PFS_AC_INF"),
	.defaultVops	= &g_pfsAcVops,
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

	if (mount->fsType != SCE_VFS_FS_TYPE_PFS)
		return SCE_ERROR_ERRNO_EINVAL;

	pmi = (ScePfsPmi *)mount->pData;
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

int ScePfsMgrForKernel_2D48AEA2(const char *path, const char *name, const void *klicensee, SceUInt16 mode)
{
	SceVfsMountParam param;
	SceVfsUmountParam umountParam;
	ScePfsPmi *pmi;
	ScePfsListNode *first;
	unsigned int bit;
	int ret;

	if (!pfsNameIsValid(name) || path == NULL)
		return SCE_ERROR_ERRNO_EINVAL;

	sceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
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
		pmi->mountData.fsName = "Gamedata/Savedata Pseudo Drive.";
		param.vfsName = g_pfsGdsdVfsInfo.vfsName;
	} else if (bit & PFS_MODE_AC_MASK) {
		pmi->mountData.fsName = "AC Pseudo Drive.";
		param.vfsName = g_pfsAcVfsInfo.vfsName;
	} else if (bit & PFS_MODE_REDIRECT_MASK) {
		pmi->mountData.fsName = "Redirect Pseudo Drive.";
		param.vfsName = g_pfsRedirectVfsInfo.vfsName;
	} else {
bad_mode:
		ret = SCE_PFS_ERROR_BAD_MODE;
		pfsPmiDestroy(pmi);
		goto out;
	}

	param.rootPath = pmi->rootPath;
	param.blockdevName = pmi->path;
	param.fsType = SCE_VFS_FS_TYPE_PFS;
	param.opt = 0;
	param.mntFlags = SCE_VFS_MOUNT_TYPE_PFS;
	param.data = pmi;
	param.misc = &pmi->mountData;
	param.vops = NULL;
	if (mode == 20 || mode - 12 <= 1 || mode == 2)
		param.mntFlags = SCE_VFS_MOUNT_TYPE_PFS | SCE_VFS_MOUNT_FLAG_RDONLY;
	if (mode == 3)
		param.mntFlags |= SCE_VFS_MOUNT_FLAG_NOBUF;
	ret = vfsMountForPFS(&param, 0, 0);
	if (ret < 0)
		goto destroy;

	first = g_pfsPmiList.next;
	if (first == NULL) {
		umountParam.assignName = pmi->assignName;
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
	sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

int ScePfsMgrForKernel_680BC384(const char *name)
{
	SceVfsUmountParam umountParam;
	ScePfsPmi *pmi, *mounted;
	ScePfsListNode *node, *next;
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;

	sceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
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
		umountParam.assignName = pmi->assignName;
		umountParam.flag = ret;
		ret = vfsUmountForPFS(&umountParam, 0, 0);
		if (ret < 0)
			goto out;
	}

	mounted = pmi;
	if (pfsModeIsAc(mounted->mode)) {
		sceKernelLockMutex(mounted->mutex, 1, NULL);
		node = mounted->acidDirs.next;
		while (node != NULL && node != &mounted->acidDirs) {
			next = node->next;
			next->prev = node->prev;
			node->prev->next = next;
			pfsPmiDestroy((ScePfsPmi *)node);
			node = next;
		}
		sceKernelUnlockMutex(mounted->mutex, 1);
	}
	next = mounted->link.next;
	next->prev = mounted->link.prev;
	mounted->link.prev->next = next;
	pfsPmiDestroy(mounted);
out:
	sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

int ScePfsMgrForKernel_D8D0FEE5(const char *name, SceUInt64 authId)
{
	ScePfsPmi *pmi;
	int ret;

	if (pfsNameIsValid(name)) {
		sceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
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
		sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	} else {
		ret = SCE_ERROR_ERRNO_EINVAL;
	}
	return ret;
}

int ScePfsMgrForKernel_2D67D8CA(const char *name, SceUInt64 authId)
{
	ScePfsPmi *pmi;
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;

	sceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret == 0) {
		if (pmi == NULL)
			ret = SCE_ERROR_ERRNO_ENODEV;
		else
			ret = pfsPmiDisapprove(pmi, authId);
	}
	sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
	return ret;
}

/* mounts and approves; mode 0x28 only checks an approved mount and returns its mode */
int ScePfsMgrForKernel_A772209C(const char *path, const char *name, SceUInt64 authId, const void *klicensee,
		 SceUInt16 mode)
{
	ScePfsPmi *pmi;
	int ret;

	if (mode != PFS_MODE_CHECK_APPROVED) {
		ret = ScePfsMgrForKernel_2D48AEA2(path, name, klicensee, mode);
		if (ret == 0)
			ret = ScePfsMgrForKernel_D8D0FEE5(name, authId);
		return ret;
	}

	if (name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) > PFS_MOUNT_NAME_MAX - 1)
		return SCE_ERROR_ERRNO_EINVAL;
	sceKernelLockMutex(g_pfsPmiListMutex, 1, NULL);
	ret = pfsPmiFind(&g_pfsPmiList, name, &pmi);
	if (ret == 0) {
		if (pmi != NULL && pfsPmiIsApproved(pmi, authId)) {
			ret = pmi->mode;
			sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
			return ret;
		}
		ret = SCE_ERROR_ERRNO_ENODEV;
	}
	sceKernelUnlockMutex(g_pfsPmiListMutex, 1);
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
	fd = sceIoDopen(path);
	if (fd < 0)
		return fd;
	ret = sceIoIoctl(fd, cmd, in, inSize, NULL, 0);
	if (ret != 0) {
		sceIoDclose(fd);
		return ret;
	}
	return sceIoDclose(fd);
}

int ScePfsMgrForKernel_B0CC0A1A(const char *name, const char *dir)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_SET, NULL, 0);
}

int ScePfsMgrForKernel_4A4A8243(const char *name, const char *dir, const void *klicensee)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_MOUNT, klicensee, 0x10);
}

int ScePfsMgrForKernel_8CDA249A(const char *name, const char *dir)
{
	if (dir == NULL || name == NULL || strnlen(name, PFS_MOUNT_NAME_MAX) - 1 > PFS_MOUNT_NAME_MAX - 2 ||
	    strnlen(dir, PFS_PATH_MAX) - 1 > PFS_PATH_MAX - 2)
		return SCE_ERROR_ERRNO_EINVAL;
	return pfsAcidDirIoctl(name, dir, PFS_IOCTL_ACID_DIR_APPROVE, NULL, 0);
}

int ScePfsMgrForKernel_00C8AF80(const char *name, const char *dir)
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
	return sceIoDevctl(device, PFS_DEVCTL_GET_4413, NULL, 0, out, 0x404);
}

int ScePfsMgrForKernel_27FB7618(const char *name, SceUInt32 *out1, SceUInt32 *out2)
{
	char device[0x24];
	SceUInt32 result[2];
	int ret;

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	ret = sceIoDevctl(device, PFS_DEVCTL_GET_SIZE, NULL, 0, result, sizeof(result));
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
	return sceIoDevctl(device, PFS_DEVCTL_SET_4411, &in, sizeof(in), NULL, 0);
}

int ScePfsMgrForKernel_E9D672AB(const char *name)
{
	char device[0x24];

	if (!pfsNameIsValid(name))
		return SCE_ERROR_ERRNO_EINVAL;
	snprintf(device, 0x21, "%s0:", name);
	return sceIoDevctl(device, PFS_DEVCTL_4414, NULL, 0, NULL, 0);
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

	pfsRandomInit(SceThreadmgrForDriver_47F6DE49());
	g_pfsPmiList.next = &g_pfsPmiList;
	g_pfsPmiList.prev = &g_pfsPmiList;
	ret = sceKernelCreateMutex("ScePfsThePmi", 0x2000, 0, NULL);
	g_pfsPmiListMutex = ret;
	if (ret < 0)
		goto lru;
	ret = sceVfsAddVfs(&g_pfsRedirectVfsInfo);
	if (ret != 0)
		goto mutex;
	ret = sceVfsAddVfs(&g_pfsGdsdVfsInfo);
	if (ret != 0)
		goto redirect;
	ret = sceVfsAddVfs(&g_pfsAcVfsInfo);
	if (ret == 0)
		return 0;

	sceVfsDeleteVfs(g_pfsGdsdVfsInfo.vfsName, NULL);
redirect:
	sceVfsDeleteVfs(g_pfsRedirectVfsInfo.vfsName, NULL);
mutex:
	sceKernelDeleteMutex(g_pfsPmiListMutex);
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
