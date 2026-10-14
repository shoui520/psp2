/* SceIofilemgr's mount daemon and inherited mount events (Sony's mount.c). */
#include <sce/event.h>
#include "iofilemgr_internal.h"

/* Names unknown: the mount daemon's command and mount-point records. */
typedef struct MountPoint MountPoint;
typedef struct SceIoMountEvent {
	ThreadmgrEvent base;
	SceUID uid;
	SceUInt32 pattern;
	SceUInt32 reserved;
	MountPoint *mountPoint;
	struct SceIoMountEvent *next;
} SceIoMountEvent;

typedef struct MountRequest {
	int command;
	int id;
	union {
		const char *path;
		SceVfsMountParam *mountParam;
		SceVfsUmountParam *umountParam;
		int umountFlags;
	};
	SceUInt32 flags;
	void *data;
	SceUID pid;
	SceUInt32 reserved;
	SceUID threadId;
	int result;
	struct MountRequest *next;
} MountRequest;

struct MountPoint {
	int id;
	SceVfsMountParam param;
	SceVfsMountData *mountData;
	SceIoMountEvent *events;
	MountPoint *children;
	MountPoint *next;
	MountPoint *parent;
};
_Static_assert(sizeof(SceIoMountEvent) == 0x4c, "SceIoMountEvent");
_Static_assert(sizeof(MountRequest) == 0x28, "MountRequest");
_Static_assert(sizeof(MountPoint) == 0x38, "MountPoint");

MountRequest *g_mountRequestList;
SceUID g_mountMutex;
SceUID g_mountCond;
SceUID g_mountThread;
MountPoint g_mountPoints[32];

/* Names unknown: the mount definitions selected by init_*(). */
static SceVfsMountData g_mountDataSdstor = {
	"sdstor0:", "blockdev_fs", "sd", NULL, 0x1
};
static SceVfsMountData g_mountDataMemdisk = {
	"md0:", "blockdev_fs", "md", NULL, 0x2
};
static SceVfsMountData g_mountDataOs0 = {
	"os0:", "exfatos0", "sdstor0:int-lp-act-os", NULL, 0x200
};
static SceVfsMountData g_mountDataVs0 = {
	"vs0:", "exfatvs0", "sdstor0:int-lp-ign-vsh", NULL, 0x300
};
static SceVfsMountData g_mountDataSa0 = {
	"sa0:", "exfatsa0", "sdstor0:int-lp-ign-sysdata", NULL, 0xb00
};
static SceVfsMountData g_mountDataVd0 = {
	"vd0:", "exfatvd0", "sdstor0:int-lp-ign-vshdata", NULL, 0x400
};
static SceVfsMountData g_mountDataTm0 = {
	"tm0:", "exfattm0", "sdstor0:int-lp-ign-vtrm", NULL, 0x500
};
static SceVfsMountData g_mountDataUr0 = {
	"ur0:", "exfatur0", "sdstor0:int-lp-ign-user", NULL, 0x600
};
static SceVfsMountData g_mountDataPd0 = {
	"pd0:", "exfatpd0", "sdstor0:int-lp-ign-pidata", NULL, 0xc00
};
static SceVfsMountData g_mountDataUd0 = {
	"ud0:", "exfatud0", "sdstor0:int-lp-ign-updater", NULL, 0x700
};
static SceVfsMountData g_mountDataUx0 = {
	"ux0:", "exfatux0", "sdstor0:xmc-lp-ign-userext", NULL, 0x800
};
static SceVfsMountData g_mountDataGro0 = {
	"gro0:", "exfatgro0", "sdstor0:gcd-lp-ign-gamero", NULL, 0x900
};
static SceVfsMountData g_mountDataGrw0 = {
	"grw0:", "exfatgrw0", "sdstor0:gcd-lp-ign-gamerw", NULL, 0xa00
};
static SceVfsMountData g_mountDataImc0 = {
	"imc0:", "exfatimc0", "sdstor0:int-lp-ign-userext", NULL, 0xd00
};
static SceVfsMountData g_mountDataXmc0 = {
	"xmc0:", "exfatxmc0", "sdstor0:xmc-lp-ign-userext", NULL, 0xe00
};
static SceVfsMountData g_mountDataMfa0 = {
	"mfa0:", "exfatmfa0", "/", NULL, 0x50000
};
static SceVfsMountData g_mountDataMfb0 = {
	"mfb0:", "exfatmfb0", "/", NULL, 0x60000
};
static SceVfsMountData g_mountDataLma0 = {
	"lma0:", "exfatlma0", "/", NULL, 0x10000
};
static SceVfsMountData g_mountDataLmb0 = {
	"lmb0:", "exfatlmb0", "/", NULL, 0x20000
};
static SceVfsMountData g_mountDataSd0 = {
	"sd0:", "exfatsd0", "sdstor0:ext-pp-act-a", "sdstor0:ext-lp-act-entire", 0x100
};
static SceVfsMountData g_mountDataUx0Internal = {
	"ux0:", "exfatux0", "sdstor0:int-lp-ign-userext", NULL, 0x800
};
static SceVfsMountData g_mountDataUma0 = {
	"uma0:", "exfatuma0", "sdstor0:uma-pp-act-a", "sdstor0:uma-lp-act-entire", 0xf00
};

int mountEventClassDestroy(SceIoMountEvent *event)
{
	return 0;
}

MountRequest *mntRemoveRequest(MountRequest *request)
{
	MountRequest *prev = g_mountRequestList;
	MountRequest *next;

	if (request != NULL) {
		if (prev == NULL)
			return request;
		if (request != prev) {
			while ((next = prev->next) != request) {
				if (next == NULL)
					break;
				prev = next;
			}
			prev->next = request->next;
			goto removed;
		}
	} else {
		if (prev == NULL)
			return request;
		request = prev;
	}
	g_mountRequestList = g_mountRequestList->next;
removed:
	request->next = NULL;
	return request;
}

int mountEventClassCreate(SceIoMountEvent *event)
{
	memset(&event->uid, 0, sizeof(*event) - sizeof(event->base));
	return 0;
}

int call_daemon(MountRequest *request)
{
	MountRequest *last;
	int ret;

	ret = ksceKernelLockMutex(g_mountMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1262, "mutex lock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	last = g_mountRequestList;
	if (last != NULL) {
		while (last->next != NULL)
			last = last->next;
		last->next = request;
	} else {
		g_mountRequestList = request;
	}
	ksceKernelSignalCondTo(g_mountCond, g_mountThread);
	ret = ksceKernelWaitCond(g_mountCond, NULL);
	if (ret < 0) {
		mntRemoveRequest(request);
		ksceKernelUnlockMutex(g_mountMutex, 1);
		return ret;
	}
	ret = ksceKernelUnlockMutex(g_mountMutex, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1282, "mutex unlock failed ... %d 0x%08x\n", ret, ret);
	return ret;
}

void mount(MountRequest *request)
{
	SceVfsMountParam param;
	MountPoint *point = NULL, *parent;
	SceIoMountEvent *event;
	SceUID oldPid = 0;
	SceUInt32 flags = request->flags;
	int ret, id, i;
	SceInt64 result;

	if (request->command == 3) {
		param = *request->mountParam;
		oldPid = ksceKernelSetProcessIdToTLS(request->pid);
	} else {
		for (i = 0; i < 32; i++) {
			if (g_mountPoints[i].id == request->id) {
				point = &g_mountPoints[i];
				break;
			}
		}
		if (i == 32) {
			request->result = SCE_ERROR_ERRNO_EINVAL;
			return;
		}
		param = point->param;
		if (flags & 1)
			param.mnt_flags |= 0x1000;
		else if (flags & 2)
			param.mnt_flags &= ~0x1000;
		param.blockdev_name = request->path;
		param.mnt_flags |= flags & ~3;
		if (request->data != NULL && point->param.data == NULL) {
			param.data = request->data;
			point->param.data = request->data;
		}
	}
	vfs_increment_io_count();
	vfs_set_sync_pending();
	ret = vfsMount(&param);
	vfs_decrement_io_count();
	request->result = ret;
	if (request->command == 3) {
		ksceKernelSetProcessIdToTLS(oldPid);
		return;
	}
	result = ret;
	if (ret == 0) {
		if (param.blockdev_name != NULL) {
			id = vfsGetMntId(param.blockdev_name);
			if (id < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1441,
						     "vfsGetMntId(%s) failed ... ret=%d 0x%x\n",
						     param.blockdev_name, ret, ret);
			} else {
				parent = NULL;
				for (i = 0; i < 32; i++) {
					if (g_mountPoints[i].id == id) {
						parent = &g_mountPoints[i];
						break;
					}
				}
				SCE_DBG_ASSERT(parent != NULL, 1444);
				if (parent->children != NULL)
					point->next = parent->children;
				parent->children = point;
				point->parent = parent;
			}
		}
		iofile_daemon_notify_mount(request->id);
		for (event = point->events; event != NULL; event = event->next) {
			if (event->pattern & 1) {
				ret = sceKernelSetEventWithNotifyCallback(event->uid, 1, result, 1);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 1466,
							     "sceKernelSetEvent failed ... ret=%d 0x%x event=0x%x\n",
							     ret, ret, event->uid);
			}
		}
	} else {
		for (event = point->events; event != NULL; event = event->next) {
			if (event->pattern & 0x10) {
				ret = sceKernelSetEventWithNotifyCallback(event->uid, 0x10, result, 0x10);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 1483,
							     "sceKernelSetEvent failed ... ret=%d 0x%x event=0x%x\n",
							     ret, ret, event->uid);
			}
		}
	}
}

void umount(MountRequest *request)
{
	union {
		SceVfsUmountParam param;
		MountRequest sub;
	} work;
	MountPoint *point = NULL, *child, *prev;
	SceIoMountEvent *event;
	SceUID oldPid = 0;
	SceInt64 result;
	int ret, i;

	if (request->command == 4) {
		work.param = *request->umountParam;
		oldPid = ksceKernelSetProcessIdToTLS(request->pid);
	} else {
		for (i = 0; i < 32; i++) {
			if (g_mountPoints[i].id == request->id) {
				point = &g_mountPoints[i];
				break;
			}
		}
		if (i == 32) {
			request->result = SCE_ERROR_ERRNO_EINVAL;
			return;
		}
		while ((child = point->children) != NULL) {
			work.sub = *request;
			work.sub.next = NULL;
			work.sub.id = child->id;
			umount(&work.sub);
		}
		work.param.assign_name = point->param.root_path;
		work.param.flag = request->umountFlags;
	}
	vfs_increment_io_count();
	vfs_set_sync_pending();
	ret = vfsUmount(&work.param);
	vfs_decrement_io_count();
	request->result = ret;
	if (request->command == 4) {
		ksceKernelSetProcessIdToTLS(oldPid);
		return;
	}
	result = ret;
	if (ret == 0) {
		if (point->parent != NULL) {
			prev = point->parent->children;
			if (prev != NULL) {
				if (prev == point) {
					point->parent->children = point->next;
				} else {
					while (prev->next != point && prev->next != NULL)
						prev = prev->next;
					prev->next = point->next;
				}
				point->next = NULL;
			}
			point->parent = NULL;
		}
		for (event = point->events; event != NULL; event = event->next) {
			if (event->pattern & 2) {
				ret = sceKernelSetEventWithNotifyCallback(event->uid, 2, result, 2);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 1562,
							     "sceKernelSetEvent failed ... ret=%d 0x%x event=0x%x\n",
							     ret, ret, event->uid);
			}
		}
	} else {
		for (event = point->events; event != NULL; event = event->next) {
			if (event->pattern & 0x20) {
				ret = sceKernelSetEventWithNotifyCallback(event->uid, 0x20, result, 0x20);
				if (ret < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0xf, 1579,
							     "sceKernelSetEvent failed ... ret=%d 0x%x event=0x%x\n",
							     ret, ret, event->uid);
			}
		}
	}
}

int mnt_main(SceSize args, void *argp)
{
	MountRequest *request;
	int ret;

	ret = ksceKernelLockMutex(g_mountMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1198, "mutex lock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	for (;;) {
		request = mntRemoveRequest(NULL);
		while (request == NULL) {
			ret = ksceKernelWaitCond(g_mountCond, NULL);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1213, "%s wait failed ... %d 0x%08x\n",
						     "SceIofilemgrMount", ret, ret);
				ksceKernelUnlockMutex(g_mountMutex, 1);
				SCE_DBG_ASSERT(0, 1216);
			}
			request = mntRemoveRequest(NULL);
		}
		switch (request->command) {
		case 1:
		case 3:
			mount(request);
			break;
		case 2:
		case 4:
			umount(request);
			break;
		default:
			request->result = SCE_ERROR_ERRNO_EINVAL;
			break;
		}
		ret = ksceKernelSignalCondTo(g_mountCond, request->threadId);
		if (ret < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1238,
					     "sceKernelSignalCondTo failed ... ret=%d 0x%x thr=0x%x\n",
					     ret, ret, request->threadId);
	}
}

MountPoint *mnt_find_mount_point(int id)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == id)
			return &g_mountPoints[i];
	}
	return NULL;
}

const char *mount_get_path_by_id(int id)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == id)
			return g_mountPoints[i].param.root_path;
	}
	return NULL;
}

SceVfsMountData *mnt_get_mount_data(int id)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == id)
			return g_mountPoints[i].mountData;
	}
	return NULL;
}

int mnt_get_mount_opt(int id)
{
	MountPoint *point = mnt_find_mount_point(id);
	VfsPathParam param;
	int ret;

	if (point == NULL)
		return 0;
	if ((point->param.mnt_flags & 0xff) != SCE_VFS_MOUNT_TYPE_STACKFS)
		return (short)point->param.opt;
	param.path = mnt_get_mount_data(id)->assign_name;
	ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfs_get_vnode_mnt_opt,
				     &param);
	return ret & ~(ret >> 31);
}

SceClass g_mountEventClass;

static inline __attribute__((always_inline)) MountPoint *mntFindFreeMountPoint(void)
{
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == 0)
			return &g_mountPoints[i];
	}
	return NULL;
}

static inline __attribute__((always_inline)) void init_sdstor(void)
{
	MountPoint *point;
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == 0) {
			point = &g_mountPoints[i];
			goto found;
		}
	}
	SCE_DBG_ASSERT_FUNC(0, 233, "init_sdstor");
	point = NULL;
found:
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataSdstor;
	point->param.misc = &g_mountDataSdstor;
	point->param.root_path = "/dev";
	point->param.vfs_name = "sdstor_dev_fs";
	point->id = 0x1;
	point->param.fs_type = 0x10;
	point->param.mnt_flags = 3;
}

static inline __attribute__((always_inline)) void init_memdisk(void)
{
	MountPoint *point;
	int i;

	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == 0) {
			point = &g_mountPoints[i];
			goto found;
		}
	}
	SCE_DBG_ASSERT_FUNC(0, 275, "init_memdisk");
	point = NULL;
found:
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataMemdisk;
	point->param.misc = &g_mountDataMemdisk;
	point->param.root_path = "/md";
	point->param.vfs_name = "md_dev_fs";
	point->id = 0x2;
	point->param.fs_type = 0x10;
	point->param.mnt_flags = 0x2003;
}

static inline __attribute__((always_inline)) void init_sd0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/sd/exfat",
		.fs_type = 0x1,
		.opt = 0x203,
		.mnt_flags = 0x30002,
		.vfs_name = "exfat",
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 341, "init_sd0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataSd0;
	point->id = 0x100;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataSd0;
}

static inline __attribute__((always_inline)) void init_os0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/os/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x1002,
		.vfs_name = "exfat",
		.misc = &g_mountDataOs0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 383, "init_os0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataOs0;
	point->id = 0x200;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataOs0;
}

static inline __attribute__((always_inline)) void init_vs0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/vs/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x1002,
		.vfs_name = "exfat",
		.misc = &g_mountDataVs0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 422, "init_vs0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataVs0;
	point->id = 0x300;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataVs0;
}

static inline __attribute__((always_inline)) void init_sa0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/sa/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x1002,
		.vfs_name = "exfat",
		.misc = &g_mountDataSa0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 805, "init_sa0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataSa0;
	point->id = 0xb00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataSa0;
}

static inline __attribute__((always_inline)) void init_vd0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/vd/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x10002,
		.vfs_name = "exfat",
		.misc = &g_mountDataVd0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 462, "init_vd0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataVd0;
	point->id = 0x400;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataVd0;
}

static inline __attribute__((always_inline)) void init_tm0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/tm/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x10002,
		.vfs_name = "exfat",
		.misc = &g_mountDataTm0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 497, "init_tm0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataTm0;
	point->id = 0x500;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataTm0;
}

static inline __attribute__((always_inline)) void init_ur0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/ur/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x50002,
		.vfs_name = "exfat",
		.misc = &g_mountDataUr0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 532, "init_ur0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataUr0;
	point->id = 0x600;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataUr0;
}

static inline __attribute__((always_inline)) void init_pd0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/pd/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x1002,
		.vfs_name = "exfat",
		.misc = &g_mountDataPd0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 844, "init_pd0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataPd0;
	point->id = 0xc00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataPd0;
}

static inline __attribute__((always_inline)) void init_ud0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/ud/exfat",
		.fs_type = 0x1,
		.opt = 0x100,
		.mnt_flags = 0x10002,
		.vfs_name = "exfat",
		.misc = &g_mountDataUd0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 568, "init_ud0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataUd0;
	point->id = 0x700;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataUd0;
}

static inline __attribute__((always_inline)) void init_ux0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/ux/exfat",
		.fs_type = 0x1,
		.opt = 0x202,
		.mnt_flags = 0x30002,
		.vfs_name = "exfat",
		.misc = &g_mountDataUx0,
	};

	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 612, "init_ux0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataUx0;
	point->id = 0x800;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	if (!ksceSysrootIsManufacturingMode() && ksceSysrootIsNonRemovableCardMode()) {
		point->mountData = &g_mountDataUx0Internal;
		point->param.misc = &g_mountDataUx0Internal;
	}
}

static inline __attribute__((always_inline)) void init_gro0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/gro/exfat",
		.fs_type = 0x1,
		.opt = 0x201,
		.mnt_flags = 0x21002,
		.vfs_name = "exfat",
		.misc = &g_mountDataGro0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 729, "init_gro0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataGro0;
	point->id = 0x900;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataGro0;
}

static inline __attribute__((always_inline)) void init_grw0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/grw/exfat",
		.fs_type = 0x1,
		.opt = 0x201,
		.mnt_flags = 0x20002,
		.vfs_name = "exfat",
		.misc = &g_mountDataGrw0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 770, "init_grw0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataGrw0;
	point->id = 0xa00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataGrw0;
}

static inline __attribute__((always_inline)) void init_imc0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/imc/exfat",
		.fs_type = 0x1,
		.opt = 0x202,
		.mnt_flags = 0x30002,
		.vfs_name = "exfat",
		.misc = &g_mountDataImc0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 655, "init_imc0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataImc0;
	point->id = 0xd00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataImc0;
}

static inline __attribute__((always_inline)) void init_xmc0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/xmc/exfat",
		.fs_type = 0x1,
		.opt = 0x202,
		.mnt_flags = 0x30002,
		.vfs_name = "exfat",
		.misc = &g_mountDataXmc0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 692, "init_xmc0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataXmc0;
	point->id = 0xe00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataXmc0;
}

static inline __attribute__((always_inline)) void init_mfa0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/mfa/exfat",
		.fs_type = 0x1,
		.opt = 0x0,
		.mnt_flags = 0x5,
		.vfs_name = "exfat",
		.misc = &g_mountDataMfa0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 883, "init_mfa0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataMfa0;
	point->id = 0x50000;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataMfa0;
}

static inline __attribute__((always_inline)) void init_mfb0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/mfb/exfat",
		.fs_type = 0x1,
		.opt = 0x0,
		.mnt_flags = 0x5,
		.vfs_name = "exfat",
		.misc = &g_mountDataMfb0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 918, "init_mfb0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataMfb0;
	point->id = 0x60000;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataMfb0;
}

static inline __attribute__((always_inline)) void init_lma0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/lma/exfat",
		.fs_type = 0x1,
		.opt = 0x0,
		.mnt_flags = 0x5,
		.vfs_name = "exfat",
		.misc = &g_mountDataLma0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 953, "init_lma0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataLma0;
	point->id = 0x10000;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataLma0;
}

static inline __attribute__((always_inline)) void init_lmb0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/lmb/exfat",
		.fs_type = 0x1,
		.opt = 0x0,
		.mnt_flags = 0x5,
		.vfs_name = "exfat",
		.misc = &g_mountDataLmb0,
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 988, "init_lmb0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataLmb0;
	point->id = 0x20000;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataLmb0;
}

static inline __attribute__((always_inline)) void init_uma0(void)
{
	static const SceVfsMountParam param = {
		.root_path = "/uma/exfat",
		.fs_type = 0x1,
		.opt = 0x203,
		.mnt_flags = 0x2,
		.vfs_name = "exfat",
	};
	MountPoint *point = mntFindFreeMountPoint();

	if (point == NULL)
		SCE_DBG_ASSERT_FUNC(0, 1040, "init_uma0");
	memset(point, 0, sizeof(*point));
	point->mountData = &g_mountDataUma0;
	point->id = 0xf00;
	__builtin_memcpy(&point->param, &param, sizeof(param));
	point->param.misc = &g_mountDataUma0;
}

int mnt_initialize(void)
{
	int ret;

	ret = sceKernelUIDInheritedEventClassInit(&g_mountEventClass,
			"SceUIDIoMountEventClass", sizeof(SceIoMountEvent),
			(int (*)(void *))mountEventClassCreate,
			(int (*)(void *))mountEventClassDestroy);
	if (ret < 0)
		return ret;
	g_mountMutex = ksceKernelCreateMutex("SceIofilemgrMount", 0, 0, NULL);
	if (g_mountMutex < 0)
		return g_mountMutex;
	g_mountCond = ksceKernelCreateCond("SceIofilemgrMount", 0, g_mountMutex, NULL);
	if (g_mountCond < 0) {
		ksceKernelDeleteMutex(g_mountMutex);
		return g_mountCond;
	}
	init_sdstor();
	init_memdisk();
	if (ksceSysrootIsManufacturingMode())
		init_sd0();
	init_os0();
	init_vs0();
	init_sa0();
	init_vd0();
	init_tm0();
	init_ur0();
	init_pd0();
	init_ud0();
	init_ux0();
	init_gro0();
	init_grw0();
	init_imc0();
	init_xmc0();
	init_mfa0();
	init_mfb0();
	init_lma0();
	init_lmb0();
	init_uma0();
	return 0;
}


int mnt_start(void)
{
	int ret;

	g_mountThread = ksceKernelCreateThread("SceIofilemgrMount", mnt_main,
			100, 0x2000, 0x10000000, 0x80000, NULL);
	if (g_mountThread < 0)
		SCE_DBG_ASSERT(0, 1151);
	ret = ksceKernelStartThread(g_mountThread, 0, NULL);
	if (ret < 0)
		SCE_DBG_ASSERT(0, 1156);
	return ret;
}

int ksceIoMount(int id, const char *path, unsigned int flags, void *data, uint64_t reserved)
{
	MountRequest request;
	int ret;

	if (id == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (reserved != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	request.data = data;
	request.pid = 0;
	if ((id & 0xffff00) == 0)
		path = NULL;
	request.id = id;
	request.path = path;
	request.flags = flags;
	request.reserved = 0;
	request.command = 1;
	request.threadId = ksceKernelGetThreadId();
	request.next = NULL;
	if (g_mountThread != 0) {
		ret = call_daemon(&request);
		if (ret >= 0)
			ret = request.result;
		return ret;
	}
	mount(&request);
	return request.result;
}

int ksceIoUmount(int id, int flags, uint64_t reserved)
{
	MountRequest request;
	int ret;

	if (id == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	if (reserved != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	request.id = id;
	request.umountFlags = flags;
	request.command = 2;
	request.flags = 0;
	request.data = NULL;
	request.threadId = ksceKernelGetThreadId();
	request.next = NULL;
	if (g_mountThread != 0) {
		ret = call_daemon(&request);
		if (ret >= 0)
			ret = request.result;
		return ret;
	}
	umount(&request);
	return request.result;
}

SceUID ksceIoCreateMountEvent(SceUInt32 id, SceUInt32 pattern)
{
	MountPoint *point;
	SceVfsMountData *mountData;
	SceIoMountEvent *event = NULL;
	SceIoMountEvent *last;
	char name[32];
	int i;
	char c;
	SceUID uid;
	int ret, closed;

	if (id == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == id)
			break;
	}
	if (i == 32)
		return SCE_ERROR_ERRNO_EINVAL;
	point = &g_mountPoints[i];
	pattern &= 0x333;
	if (pattern == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	strncpy(name, "SceIoMountEvent", sizeof(name) - 1);
	i = 0;
	mountData = point->mountData;
	for (;;) {
		c = mountData->assign_name[i];
		if (i == 0)
			c = toupper((SceInt8)c);
		if (c == ':') {
			name[15 + i] = '\0';
			break;
		}
		name[15 + i] = c;
		if (c == '\0' || ++i == 16)
			break;
	}
	name[sizeof(name) - 1] = '\0';
	uid = ksceGUIDKernelCreate(&g_mountEventClass, name, (SceObjectBase **)&event);
	if (uid < 0)
		return uid;
	ret = sceKernelEventObjectInit(uid, 0x100, 0, 0, 0);
	if (ret < 0) {
		closed = ksceGUIDClose(uid);
		SCE_DBG_ASSERT_FUNC(closed == 0, 1641, "sceIoCreateMountEvent");
		return ret;
	}
	event->uid = uid;
	event->pattern = pattern;
	ret = ksceKernelLockMutex(g_mountMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1652, "sceIoCreateMountEvent",
				"mutex lock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	last = point->events;
	if (last != NULL) {
		while (last->next != NULL)
			last = last->next;
		last->next = event;
	} else {
		point->events = event;
	}
	event->mountPoint = point;
	ret = ksceKernelUnlockMutex(g_mountMutex, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1662, "sceIoCreateMountEvent",
				"mutex unlock failed ... %d 0x%08x\n", ret, ret);
	return uid;
}

int ksceIoDeleteMountEvent(SceUID uid)
{
	SceIoMountEvent *event, *prev, *next;
	MountPoint *point;
	SceUID handle;
	int ret;

	handle = ksceGUIDOpenByGUID(uid);
	if (handle < 0)
		return handle;
	if (!SceSysmemForKernel_7F7D5C4A(SceSysmemForKernel_368F1C36(uid),
					 &g_mountEventClass))
		return 0x80028002;
	event = SceSysmemForKernel_FA6C098C(uid);
	ksceGUIDClose(handle);
	ret = ksceKernelLockMutex(g_mountMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1696, "sceIoDeleteMountEvent",
				"mutex lock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	point = event->mountPoint;
	prev = point->events;
	if (prev != NULL) {
		if (prev == event) {
			point->events = event->next;
		} else {
			for (;;) {
				next = prev->next;
				if (next == event || next == NULL)
					break;
				prev = prev->next;
			}
			prev->next = event->next;
		}
		event->next = NULL;
	}
	ret = ksceKernelUnlockMutex(g_mountMutex, 1);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0xf, 1705, "sceIoDeleteMountEvent",
				"mutex unlock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	return ksceGUIDClose(uid);
}

int SceIofilemgrForDriver_A39A9CA7(const char *dev, int flags, SceUInt64 reserved)
{
	IofilemgrRmdevParam param = { dev, VFS_RMDEV_FLAG_BUSY_CHECK };

	if (reserved != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	return vfsRmdev(&param);
}

int mount_event_process_finalize(SceUID pid)
{
	SceUID puids[64];
	SceSize num;
	SceUID guid;
	int ret, i;

	do {
		ret = SceSysmemForKernel_08C05493(pid, &g_mountEventClass, 7, puids, 64, &num);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 1742,
				"scePUIDGetUIDVectorByClass failed 0x%x %d\n", ret, ret);
			break;
		}
		if ((int)num <= 0)
			break;
		for (i = 0; i < (int)num; i++) {
			guid = kscePUIDtoGUID(pid, puids[i]);
			if (guid < 0) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1752,
					"PUIDtoGUID failed err= %d 0x%x puid=0x%x\n",
					guid, guid, puids[i]);
				continue;
			}
			ret = ksceIoDeleteMountEvent(guid);
			if (ret == 0) {
				kscePUIDClose(pid, puids[i]);
			} else {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1761,
					"sceIoDeleteMountEvent failed %d 0x%x uid=0x%x %d\n",
					ret, ret, guid, guid);
			}
		}
	} while (num == 64);
	return ret;
}

int sceIoSetMountEvent(int id, SceUInt32 pattern, SceInt64 result, SceUInt32 reserved)
{
	MountPoint *point;
	SceIoMountEvent *event;
	int ret, i;

	if (id == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	for (i = 0; i < 32; i++) {
		if (g_mountPoints[i].id == id)
			break;
	}
	if (i == 32)
		return SCE_ERROR_ERRNO_EINVAL;
	point = &g_mountPoints[i];
	ret = ksceKernelLockMutex(g_mountMutex, 1, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1788,
			"mutex lock failed ... %d 0x%08x\n", ret, ret);
		return ret;
	}
	for (event = point->events; event != NULL; event = event->next) {
		if ((pattern & event->pattern) != 0) {
			ret = sceKernelSetEventWithNotifyCallback(event->uid,
				pattern & event->pattern, result, pattern & event->pattern);
			if (ret < 0)
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 1802,
					"sceKernelSetEvent failed ... ret=%d 0x%x event=0x%x\n",
					ret, ret, event->uid);
		}
	}
	ret = ksceKernelUnlockMutex(g_mountMutex, 1);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1810,
			"mutex unlock failed ... %d 0x%08x\n", ret, ret);
	return ret;
}

int vfsMountForPFS(SceVfsMountParam *param, int reservedLow, int reservedHigh)
{
	MountRequest request;
	int ret;

	if (reservedHigh != 0 || reservedLow != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	request.command = 3;
	request.id = 0;
	request.flags = 0;
	request.data = NULL;
	request.reserved = 0;
	request.result = 0;
	request.mountParam = param;
	request.pid = ksceKernelGetProcessIdFromTLS();
	request.threadId = ksceKernelGetThreadId();
	request.next = NULL;
	if (g_mountThread == 0)
		return SCE_ERROR_ERRNO_EPERM;
	ret = call_daemon(&request);
	if (ret < 0)
		return ret;
	return request.result;
}

int vfsUmountForPFS(SceVfsUmountParam *param, int reservedLow, int reservedHigh)
{
	MountRequest request;
	int ret;

	if (reservedHigh != 0 || reservedLow != 0)
		return SCE_ERROR_ERRNO_EINVAL;
	request.command = 4;
	request.id = 0;
	request.flags = 0;
	request.data = NULL;
	request.reserved = 0;
	request.result = 0;
	request.umountParam = param;
	request.pid = ksceKernelGetProcessIdFromTLS();
	request.threadId = ksceKernelGetThreadId();
	request.next = NULL;
	if (g_mountThread == 0)
		return SCE_ERROR_ERRNO_EPERM;
	ret = call_daemon(&request);
	if (ret < 0)
		return ret;
	return request.result;
}
