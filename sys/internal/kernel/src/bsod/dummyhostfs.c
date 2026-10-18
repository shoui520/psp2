/* Crash-screen host filesystem: preserves the native VFS/VOP slot order. */
#include "bsod_internal.h"
#include <iofilemgr/vfs.h>
#include <stddef.h>
#include <kernel/error.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include <sysmem/sysroot.h>
#include <acmgr/acmgr.h>

_Static_assert(offsetof(SceVfsMount, available_entry_num) == 0x68, "mount entries");
_Static_assert(offsetof(SceVfsMount, default_io_cache_size) == 0xc0, "mount cache");
_Static_assert(offsetof(SceVfsMount, data) == 0xc4, "mount private data");
_Static_assert(offsetof(SceVfsVnode, core.node_data) == 0x48, "vnode private data");
_Static_assert(offsetof(SceVfsVnode, core.mnt) == 0x4c, "vnode mount");
_Static_assert(offsetof(SceVfsVnode, core.fid) == 0x60, "vnode file ID");
_Static_assert(offsetof(SceVfsVnode, core.state) == 0x74, "vnode state");
_Static_assert(offsetof(SceVfsVnode, core.size) == 0x80, "vnode size");

#if FW_DEBUG
#define FS_INFO(lineHash, funcHash, line, func) \
	{ 0xc8939664, lineHash, funcHash, "dummyhostfs.c", line, func }
#else
#define FS_INFO(lineHash, funcHash, line, func) \
	{ 0xc8939664, lineHash, funcHash, NULL, 0, NULL }
#endif
static const SceKernelDebugInfo s_fsSystemAccess =
	FS_INFO(0xb4775267, 0xa6e9d5bb, 186, "_sceKernelBsodDummyFsError");
static const SceKernelDebugInfo s_fsDeniedAccess =
	FS_INFO(0x0758483d, 0xa6e9d5bb, 196, "_sceKernelBsodDummyFsError");
static const SceKernelDebugInfo s_fsPanic =
	FS_INFO(0x8dbacd5c, 0xa6e9d5bb, 197, "_sceKernelBsodDummyFsError");

void _sceKernelBsodDummyFsError(const char *name)
{
	uint32_t identifier = (uint32_t)name;
	int pid = SceThreadmgrForDriver_FA54D49A();

	if (pid == SCE_KERNEL_PROCESS_ID)
		return;
	if (sceSblACMgrIsSystemProgram(pid)) {
		if (SceQafMgrForDriver_382C71E8())
			SceDebugForDriver_02B04343(0, &s_fsSystemAccess,
				"Host file system access is forbidden: name='%s'\n", name);
		return;
	}
	if (SceSysrootForDriver_CAE820B2(pid, &identifier) < 0)
		identifier = 0;
	if (identifier >= 0x1800000) {
		SceDebugForDriver_02B04343(0, &s_fsDeniedAccess,
			"Host file system access is forbidden: name='%s'\n", name);
		SceDebugForDriver_391B5B74(&s_fsPanic, __builtin_return_address(0));
	}
}

/* Distinct native slots remain distinct callback addresses. */
#define UNSUPPORTED(name, type) \
	static int __attribute__((no_icf)) name(type *arg) \
	{ (void)arg; return SCE_ERROR_ERRNO_ENOTSUP; }
UNSUPPORTED(bsodFsDevctl, SceVfsOpDevctlArg)
UNSUPPORTED(bsodFsLookup, SceVopLookupArgs)
UNSUPPORTED(bsodFsCreate, SceVopCreateArgs)
UNSUPPORTED(bsodFsClose, SceVopCloseArgs)
UNSUPPORTED(bsodFsRead, SceVopReadArgs)
UNSUPPORTED(bsodFsWrite, SceVopWriteArgs)
UNSUPPORTED(bsodFsDclose, SceVopDcloseArgs)
UNSUPPORTED(bsodFsDread, SceVopDreadArgs)
UNSUPPORTED(bsodFsFgetstat, SceVopFgetstatArgs)
UNSUPPORTED(bsodFsFchstat, SceVopFchstatArgs)
UNSUPPORTED(bsodFsPread, SceVopPreadArgs)
UNSUPPORTED(bsodFsPwrite, SceVopPwriteArgs)

static SceOff bsodFsLseek(SceVopLseekArgs *arg)
{
	(void)arg;
	return (int32_t)SCE_ERROR_ERRNO_ENOTSUP;
}
#define SUCCESS(name, type) \
	static int __attribute__((no_icf)) name(type *arg) \
	{ (void)arg; return 0; }
SUCCESS(bsodFsInit, SceVfsOpInitArgs)
SUCCESS(bsodFsUmount, SceVfsOpUmountArgs)
SUCCESS(bsodFsSync, SceVfsOpSyncArgs)
SUCCESS(bsodFsInactive, SceVopInactiveArgs)
SUCCESS(bsodFsVopSync, SceVopSyncArgs)

static int bsodFsMount(SceVfsOpMountArgs *arg)
{
	SceVfsMount *mount = arg->mnt;

	mount->available_entry_num = 0x80;
	mount->default_io_cache_size = 0x10000;
	return 0;
}

static int bsodFsSetRoot(SceVfsOpSetRootArgs *arg)
{
	SceVfsMount *mount = arg->mnt;
	SceVfsVnode *node = arg->vp;

	node->core.mnt = mount;
	node->core.dd = NULL;
	node->core.type = SCE_VNODE_TYPE_ROOTDIR;
	node->core.fid[0] = 0;
	node->core.fid[1] = 0;
	node->core.state = SCE_VNODE_STATE_ACTIVE;
	node->core.size = 0;
	node->core.node_data = mount->data;
	node->core.acl_data[0] = 0;
	node->core.acl_data[1] = 0;
	return 0;
}

#define DENIED(callback, type, member) \
	static int __attribute__((no_icf)) callback(type *arg) \
	{ _sceKernelBsodDummyFsError(arg->member->name); \
	  return SCE_ERROR_ERRNO_ENOTSUP; }
DENIED(bsodFsOpen, SceVopOpenArgs, path)
DENIED(bsodFsRemove, SceVopRemoveArgs, path)
DENIED(bsodFsMkdir, SceVopMkdirArgs, path)
DENIED(bsodFsRmdir, SceVopRmdirArgs, path)
DENIED(bsodFsDopen, SceVopDopenAgrs, path)
DENIED(bsodFsGetstat, SceVopGetstatArgs, path)
DENIED(bsodFsChstat, SceVopChstatArgs, path)
DENIED(bsodFsRename, SceVopRenameArgs, old_path)

static int bsodFsDecodePath(SceVfsOpDecodePathElemArgs *arg)
{
	_sceKernelBsodDummyFsError(arg->path);
	return 0;
}

/* Unspecified operations and reserved entries remain NULL. */
static const SceVopTable s_bsodVops = {
	.vop_open = bsodFsOpen,
	.vop_create = bsodFsCreate,
	.vop_close = bsodFsClose,
	.vop_lookup = bsodFsLookup,
	.vop_read = bsodFsRead,
	.vop_write = bsodFsWrite,
	.vop_lseek = bsodFsLseek,
	.vop_remove = bsodFsRemove,
	.vop_mkdir = bsodFsMkdir,
	.vop_rmdir = bsodFsRmdir,
	.vop_dopen = bsodFsDopen,
	.vop_dclose = bsodFsDclose,
	.vop_dread = bsodFsDread,
	.vop_getstat = bsodFsGetstat,
	.vop_chstat = bsodFsChstat,
	.vop_rename = bsodFsRename,
	.vop_pread = bsodFsPread,
	.vop_pwrite = bsodFsPwrite,
	.vop_inactive = bsodFsInactive,
	.vop_sync = bsodFsVopSync,
	.vop_fgetstat = bsodFsFgetstat,
	.vop_fchstat = bsodFsFchstat,
};
static const SceVfsOpTable s_bsodVfsOps = {
	.vfs_mount = bsodFsMount,
	.vfs_umount = bsodFsUmount,
	.vfs_set_root = bsodFsSetRoot,
	.vfs_sync = bsodFsSync,
	.vfs_init = bsodFsInit,
	.vfs_devctl = bsodFsDevctl,
	.vfs_decode_path_elem = bsodFsDecodePath,
};
static SceVfsInfo s_bsodDummyInfo;
static SceVfsMountData s_bsodMountData = {
	"host0:", "bsod_dummy_host_fs", "bsod_dummy_host_fs", NULL, 0,
};
static SceVfsMountParam s_bsodMount = {
	.root_path = "/host", .fs_type = 4, .opt = 0x300,
	.mnt_flags = 0x8006, .vfs_name = "bsod_dummy_host_fs",
	.misc = &s_bsodMountData,
};
static SceVfsUmountParam s_bsodUmount = { "/host", 0 };

int _sceKernelBsodDummyFsInit(void)
{
	int result = vfsUmount(&s_bsodUmount);
#if FW_DEBUG
	static const SceKernelDebugInfo unmountInfo =
		FS_INFO(0x20d36e63, 0x78c642a4, 134, "_sceKernelBsodDummyFsInit");
	static const SceKernelDebugInfo addInfo =
		FS_INFO(0x76342ad1, 0x78c642a4, 148, "_sceKernelBsodDummyFsInit");
	static const SceKernelDebugInfo mountInfo =
		FS_INFO(0xa4cca18a, 0x78c642a4, 167, "_sceKernelBsodDummyFsInit");
	if (result < 0)
		SceDebugForDriver_D9703808(1, 0xf, &unmountInfo,
			"vfsUmount failed (0x%08X)\n", result);
#endif
	sceVfsDeleteVfs("deci4p_drfp_dev_fs", NULL);
	s_bsodDummyInfo.type = 0x10;
	s_bsodDummyInfo.vfs_ops = (SceVfsOpTable *)&s_bsodVfsOps;
	s_bsodDummyInfo.default_vops = (SceVopTable *)&s_bsodVops;
	s_bsodDummyInfo.vfs_name = "bsod_dummy_host_fs";
	s_bsodDummyInfo.vfs_name_len = 0x13;
	result = sceVfsAddVfs(&s_bsodDummyInfo);
	if (result < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0xf, &addInfo,
			"sceVfsAddVfs failed (0x%08X)\n", result);
#endif
		return result;
	}
	result = vfsMount(&s_bsodMount);
#if FW_DEBUG
	if (result < 0)
		SceDebugForDriver_D9703808(1, 0xf, &mountInfo,
			"vfsMount failed (0x%08X)\n", result);
#endif
	return result;
}
