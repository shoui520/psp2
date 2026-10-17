/* Filesystem registration; operation tables are installed during phase one. */
#include <iofilemgr/vfs.h>
#include <sdstor/sdstor.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/debug.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr/misc.h>
#include "exfatfs_internal.h"
#include <threadmgr/threadmgr.h>

SceVfsInfo g_exfatfsInfo = {
	.vfs_ops = NULL,
	.vfs_name = "exfat",
	.vfs_name_len = 6,
	.ref_count = 0,
	.type = 2,
	.default_vops = NULL,
	.vfs_data = NULL,
	.next = NULL,
};

int exfatfs_vfs_init(SceVfsOpInitArgs *argp)
{
	return 0;
}

int exfatfs_vfs_fini(SceVfsOpFiniArgs *argp)
{
	return 0;
}

int exfatfs_vop_ioctl(SceVopIoctlArgs *argp)
{
	SCE_DBG_PRINTF_LEVEL(2, 15, 3742, "called IOCTL !!\n");
	return 0x80010030;
}

/* The release compiler inlines this into mount cleanup and finalization. */
#if FW_VERSION >= 0x04000000
int _close_device_file(SceExfatfsDeviceFileInfo *info, int force)
{
	int result, error, locked;

	if (info->device_fd == -1)
		goto closed;
	result = vfsLockVnode(info->device_vnode);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 890, "%s: vfsLockVnode failed[%#x]\n",
			"_close_device_file", result);
		if (!force)
			return result;
		locked = 0;
	} else {
		locked = 1;
	}
	if (!info->device_file) {
		if (info->mount_misc)
			SCE_DBG_PRINTF_LEVEL(1, 0, 899,
				"%s: unexpected info->device_file[%p], assignName(%s), info->device_fd[%#x], info->device_vnode[%p], please report to Bug 24084\n",
				"_close_device_file", info->device_file, info->mount_misc->assign_name,
				info->device_fd, info->device_vnode);
		else
			SCE_DBG_PRINTF_LEVEL(1, 0, 901,
				"%s: unexpected info->device_file[%p], info->mount_misc[%p], info->device_fd[%#x], info->device_vnode[%p], please report to Bug 24084\n",
				"_close_device_file", info->device_file, info->mount_misc,
				info->device_fd, info->device_vnode);
	} else {
		SCE_DBG_PRINTF_LEVEL(2, 0, 904,
			"%s: info->device_file[%p], assignName(%s), info->device_fd[%#x], info->device_vnode[%p]\n",
			"_close_device_file", info->device_file, info->mount_misc->assign_name,
			info->device_fd, info->device_vnode);
	}
	result = ksceVopClose(info->device_vnode, info->device_file);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 908, "%s: sceVopClose failed[%#x]\n",
			"_close_device_file", result);
		if (!force) {
			if (locked) {
				error = vfsUnlockVnode(info->device_vnode);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0, 913, "%s: vfsUnlockVnode failed[%#x]\n",
						"_close_device_file", error);
			}
			return result;
		}
	}
	result = vfsFreeFile(info->device_vnode, info->device_fd);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 921, "%s: vfsFreeFile failed[%#x]\n",
			"_close_device_file", result);
		if (!force) {
			if (locked) {
				error = vfsUnlockVnode(info->device_vnode);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL(1, 0, 926, "%s: vfsUnlockVnode failed[%#x]\n",
						"_close_device_file", error);
			}
			return result;
		}
	} else {
		--info->device_vnode->core.ref_count;
	}
	info->device_file = NULL;
	info->device_fd = -1;
	if (!locked)
		return 0;
	error = vfsUnlockVnode(info->device_vnode);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 939, "%s: vfsUnlockVnode failed[%#x]\n",
			"_close_device_file", error);
closed:
	if (info->device_file)
		SCE_DBG_PRINTF_LEVEL(1, 0, 944,
			"%s: unexpected device_file[%p], device_fd[%#x]\n",
			"_close_device_file", info->device_file, info->device_fd);
	return 0;
}
#endif

extern int _VopGetstatRootdir(SceExfatfsDrive *drive, SceIoStat *stat);
extern int _sceExfatfsVopGetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root);

#if FW_VERSION >= 0x04000000
extern int get_new_vnode(SceVfsMount *mount, SceVfsVnode **output);
#else
static inline int get_new_vnode(SceVfsMount *mount, SceVfsVnode **output)
{
	SceVfsVnode *vnode = NULL;
	int result;

	result = vfsGetNewVnode(mount, mount->mnt_vfs_inf->default_vops, 0, &vnode);
	if (result)
		return result;
	result = vfsLockVnode(vnode);
	if (result < 0)
		vfsFreeVnode(vnode);
	else
		*output = vnode;
	return result;
}
#endif

/* Sony's debug records name this inline in both root operations. */
static inline __attribute__((always_inline)) void _set_root_directory_vnode(
	SceVfsVnode *vnode, SceVfsMount *mount, SceExfatfsFnode *fnode,
	unsigned char attribute)
{
	vnode->core.dd = NULL;
	if (vnode->core.mnt != mount) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1789, "_set_root_directory_vnode", "%s: vp->mnt[%p] != mnt[%p]\n",
			"_set_root_directory_vnode", vnode->core.mnt, mount);
		vnode->core.mnt = mount;
	}
	if (vnode->core.type != 0x1002) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1793, "_set_root_directory_vnode", "%s: vp->type[%#x] != expected[%#x]\n",
			"_set_root_directory_vnode", vnode->core.type, 0x1002);
		vnode->core.type = 0x1002;
	}
	if (vnode->core.state != 1)
		vnode->core.state = 1;
	vnode->core.acl_data[0] = attribute & 0xff;
	vnode->core.acl_data[1] = 0;
	vnode->core.fid[0] = 0;
	vnode->core.fid[1] = 0;
	vnode->core.size = 0;
	vnode->core.node_data = fnode;
}

int exfatfs_vfs_get_root(SceVfsOpGetRootArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceVfsInfo *info = mount->mnt_vfs_inf;
	SceExfatfsDrive *drive = mount->data;
	SceVfsVnode **output = argp->vpp;
	SceExfatfsFnode *fnode;
	SceVfsVnode *vnode;
	SceIoStat stat;
	unsigned int attribute;
	int result;

	SCE_DBG_PRINTF_LEVEL(1, 0, 1864,
		"%s: vfsName(%s), defaultVops[%p], vfsOps[%p], type[%#x], vfsData[%p], refCount(%d), next[%p], mnt->pData[%p]\n",
		"exfatfs_vfs_get_root", info->vfs_name, info->default_vops,
		info->vfs_ops, info->type, info->vfs_data, info->ref_count, info->next, drive);
	*output = info->vfs_data;
	if (info->vfs_data)
		return 0;
	SCE_DBG_PRINTF_LEVEL(1, 0, 1869, "%s: no vfsData for mnt[%p]\n",
		"exfatfs_vfs_get_root", mount);
	if (!drive)
		return 0x80010013;
	result = _VopGetstatRootdir(drive, &stat);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1881, "%s: _VopGetstatRootdir failed[%#x]\n",
			"exfatfs_vfs_get_root", result);
		return result;
	}
	fnode = NULL;
	result = _sceExfatfsVopGetRoot(drive, &fnode);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1887, "%s: _sceExfatfsVopGetRoot failed[%#x]\n",
			"exfatfs_vfs_get_root", result);
		return result;
	}
	vnode = NULL;
	result = get_new_vnode(mount, &vnode);
	if (result)
		return result;
	attribute = stat.st_attr;
	_set_root_directory_vnode(vnode, mount, fnode, attribute);
	vnode->core.ref_count = 1;
	*output = vnode;
	return result;
}

extern SceExfatfsFnode *getDriveWhiteoutDirectory(SceExfatfsDrive *drive);
extern int sceExfatfsVopDopen(SceExfatfsFnode *fnode, unsigned int *fd);
extern int sceExfatfsVopDread(unsigned int fd, SceIoDirent *entry);
extern int sceExfatfsVopDclose(unsigned int fd);
extern int _exfatfs_reference_count(SceExfatfsFnode *parent, const SceIoDirent *entry, int *error);
extern int sceExfatfsVopLookup(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, SceExfatfsFnode **output);
extern int sceExfatfsVopRemove(SceExfatfsFnode *parent, SceExfatfsFnode *fnode);
extern int _check_empty_directory(SceExfatfsFnode *parent, SceExfatfsFnode *fnode);
extern int sceExfatfsVopInactive(SceExfatfsFnode *fnode);

int _delete_whiteout_entry_core(SceExfatfsDrive *drive)
{
	SceExfatfsFnode *directory = getDriveWhiteoutDirectory(drive);
	SceExfatfsFnode *fnode, *rollbackFnode;
	SceIoDirent entry;
	unsigned int fd;
	int result, closeResult, length, error;
	const char *cursor, *character;

	if (!directory)
		return 0x80010002;
	fd = 0;
	result = sceExfatfsVopDopen(directory, &fd);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2301, "%s: _exfatfs_dopen_core failed[%#x]\n",
			"_delete_whiteout_entry_core", result);
		return result;
	}
	for (;;) {
		result = sceExfatfsVopDread(fd, &entry);
		if (result < 0)
			break;
		if (!_exfatfs_reference_count(directory, &entry, NULL)) {
			result = 0;
			break;
		}
	}
	closeResult = sceExfatfsVopDclose(fd);
	if (closeResult < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2257, "%s: _exfatfs_dclose_core failed[%#x]\n",
			"_delete_whiteout_entry_core", closeResult);
	if (result)
		return result;
	fnode = NULL;
	cursor = entry.d_name;
	for (;;) {
		character = cursor++;
		if (!*character) {
			length = character - entry.d_name;
			break;
		}
		length = cursor - entry.d_name;
		if (length == 256)
			break;
	}
	result = sceExfatfsVopLookup(directory, entry.d_name, length, &fnode);
	if (result < 0) {
		if (result != (int)0x80010002)
			SCE_DBG_PRINTF_LEVEL(1, 0, 2275, "%s: _exfatfs_lookup_core failed[%#x]\n",
				"_delete_whiteout_entry_core", closeResult);
		return result;
	}
	if ((entry.d_stat.st_mode & 0xf000) == 0x2000) {
		result = sceExfatfsVopRemove(directory, fnode);
		SCE_DBG_PRINTF_LEVEL(1, 0, 2284, "%s: _exfatfs_remove_core failed[%#x]\n",
			"_delete_whiteout_entry_core", result);
	} else {
		result = _check_empty_directory(directory, fnode);
		SCE_DBG_PRINTF_LEVEL(1, 0, 2291, "%s: _exfatfs_rmdir_core failed[%#x]\n",
			"_delete_whiteout_entry_core", result);
	}
	rollbackFnode = fnode;
	error = sceExfatfsVopInactive(rollbackFnode);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
			"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
			"_exfatfs_lookup_rollback_core", rollbackFnode, error);
	return result;
}

extern int sceFatfsACGetRootDirAttribute(SceExfatfsDrive *drive, unsigned char *attribute);
extern int _sceExfatfsVopSetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root);

int exfatfs_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceExfatfsDrive *drive = mount->data;
	SceVfsVnode *vnode = argp->vp;
	SceExfatfsFnode *fnode = NULL;
	unsigned char attribute = 0;
	int result;

	if (!drive)
		return 0x80010013;
	result = sceFatfsACGetRootDirAttribute(drive, &attribute);
	if (result) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1929,
			"%s: sceFatfsACGetRootDirAttribute for drive[%p] failed[%#x]\n",
			"exfatfs_vfs_set_root", drive, result);
		if (result == (int)0x80010002)
			result = 0x80010016;
	} else {
		result = _sceExfatfsVopSetRoot(drive, &fnode);
	}
	vfsLockVnode(vnode);
	_set_root_directory_vnode(vnode, mount, fnode, attribute);
	vfsUnlockVnode(vnode);
	return result;
}

extern int sceFatfsFinalizeHeap(int reuseHeap);
extern void freeCommonHeapMemory(int unused, void *memory);

void _cleanup_after_umount(SceExfatfsDrive *drive)
{
	int result;

	if (!drive)
		return;
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1070, "_sceExfatfsClearDeviceVnode",
		"%s: drive[%p], flag(%d)\n", "_sceExfatfsClearDeviceVnode", drive, 1);
#if FW_VERSION >= 0x04000000
	_close_device_file(&drive->deviceInfo, 1);
#else
	if (drive->deviceInfo.device_fd != -1) {
		int locked = vfsLockVnode(drive->deviceInfo.device_vnode);

		ksceVopClose(drive->deviceInfo.device_vnode, drive->deviceInfo.device_file);
		if (vfsFreeFile(drive->deviceInfo.device_vnode, drive->deviceInfo.device_fd) >= 0)
			--drive->deviceInfo.device_vnode->core.ref_count;
		drive->deviceInfo.device_file = NULL;
		drive->deviceInfo.device_fd = -1;
		if (locked >= 0)
			vfsUnlockVnode(drive->deviceInfo.device_vnode);
	}
#endif
	drive->deviceInfo.device_vnode = NULL;
	result = sceFatfsFinalizeHeap((int)drive);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 1095, "sceFatfsFinalizeHeap failed (0x%08X)\n", result);
	if (!drive->deviceInfo.mount_misc)
		return;
	result = ksceKernelFinalizeFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 868, "_finalize_drive_information",
			"%s: sceKernelFinalizeFastMutex failed[%#x]\n", "_finalize_drive_information", result);
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 873, "_finalize_drive_information",
		"%s: assignName(%s)\n", "_finalize_drive_information", drive->assignName);
	drive->assignName = NULL;
	drive->fileSystem = NULL;
	drive->reservedWords[0] = 0;
	drive->reservedWords[1] = 0;
	drive->deviceInfo.mount_misc = NULL;
	freeCommonHeapMemory(0, drive);
}

/* Release callers inline the same mounted-drive lock and recheck. */
#if FW_VERSION >= 0x04000000
int _lock_drive_information(SceVfsMount *mount, const char *function)
{
	SceExfatfsDrive *drive = mount->data;
	int result, error;

	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1270, "_lock_drive_information",
			"%s: mp[%p] doesn't have drive\n", function, mount);
		return 0x80010013;
	}
	result = ksceKernelLockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 791, "_lock_drive_information_core",
			"%s: sceKernelLockFastMutex failed[%#x]\n", function, result);
		return result;
	}
	if (drive != mount->data) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1254, "_lock_drive_information",
			"%s: drive[%p]->[%p], mp[%p]\n", function, drive, mount->data, mount);
	} else if (!(drive->state & 1)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1262, "_lock_drive_information",
			"%s: not mounted, mp[%p], drive[%p], flags[%#x]\n",
			function, mount, drive, drive->state);
	} else {
		return result;
	}
	error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", function, error);
	return 0x80010013;
}
#else
static inline int _lock_drive_information(SceVfsMount *mount, const char *function)
{
	SceExfatfsDrive *drive = mount->data;
	int result;

	if (!drive)
		return 0x80010013;
	result = ksceKernelLockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0)
		return result;
	if (drive != mount->data || !(drive->state & 1)) {
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		return 0x80010013;
	}
	return result;
}
#endif

extern int sceExfatfsVopUmount(SceExfatfsDrive *drive, unsigned int mode);

int exfatfs_vfs_umount(SceVfsOpUmountArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceExfatfsDrive *drive;
	unsigned int mode = argp->flags;
	int result, error;

	if (!mount) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1667, "%s: unexpected NULL mp\n", "exfatfs_vfs_umount");
		return 0x80010016;
	}
	drive = mount->data;
	if (!drive) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 1670, "%s: unexpected pData[%p]\n",
			"exfatfs_vfs_umount", drive);
		return 0x80010013;
	}
	result = _lock_drive_information(mount, "exfatfs_vfs_umount");
	if (result < 0)
		return result;
#if FW_VERSION >= 0x04000000
	drive = mount->data;
#endif
	result = sceExfatfsVopUmount(drive, mode);
	drive = mount->data;
	if (!result) {
		if (drive) {
			if (!(drive->state & 0x4000))
				drive->state &= ~1u;
		} else {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1174, "_start_umount_cleanup",
				"%s: unexpected NULL drive\n", "_start_umount_cleanup");
			drive = mount->data;
		}
	}
	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
		return result;
	}
	error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vfs_umount", error);
	return result;
}

extern int sceExfatfsVopDecodePathElem(SceExfatfsDrive *drive, const unsigned char *path,
	SceSize *offset, SceSize *length, unsigned char *destination, SceSize size);

int exfatfs_vfs_decode_path_elem(SceVfsOpDecodePathElemArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceExfatfsDrive *drive = mount->data;
	const char *path = argp->path;
	const char **path2 = argp->path2;
	const char **path3 = argp->path3;
	char *buffer = argp->buf;
	SceSize size = argp->buf_len;
	SceSize *length = argp->decode_len;
	SceSize offset;
	int result, error;

	if (!drive)
		return 0x80010013;
	result = _lock_drive_information(mount, "exfatfs_vfs_decode_path_elem");
	*length = 0;
	if (result < 0)
		return result;
	offset = 0;
	result = sceExfatfsVopDecodePathElem(drive, (const unsigned char *)path,
		&offset, length, (unsigned char *)buffer, size);
	if (!result) {
		const char *start = path + offset;
		const char *end = start + *length;

		*path2 = start;
		*path3 = end;
	} else if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 1995,
			"sceExfatfsVopDecodePathElem() failed (0x%08X)\n", result);
	}
	drive = mount->data;
	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
		return result;
	}
	error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vfs_decode_path_elem", error);
	return result;
}

int exfatfs_vop_dread(SceVopDreadArgs *argp)
{
	SceVfsFile *file = argp->file;
	SceVfsVnode *expectedVnode = argp->vp;
	SceVfsVnode *vnode;
	SceVfsMount *mount;
	SceExfatfsDrive *drive;
	SceIoDirent *entry = argp->dir;
	int result, error;
	char prefix[8];

	SCE_DBG_PRINTF_LEVEL(2, 15, 2890, "VOP called DREAD \n");
	vnode = file->vp;
	if (vnode != expectedVnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2892, "%s: vp[%p] != file->vnode[%p]\n",
			"exfatfs_vop_dread", expectedVnode, vnode);
		vnode = file->vp;
	}
	mount = vnode->core.mnt;
	result = _lock_drive_information(mount, "_exfatfs_dread");
	if (result < 0)
		goto complete;
	for (;;) {
		result = sceExfatfsVopDread(file->fd, entry);
		if (result == (int)0x800f090d || result == (int)0x800f0916 ||
			result == (int)0x8001000d) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 2485, "_exfatfs_dread",
				"%s: skip error[%#x]\n", "_exfatfs_dread", result);
			continue;
		}
		if (result)
			break;
		__builtin_memcpy(prefix, "SCEDEL~", sizeof(prefix));
		if (strncmp(prefix, entry->d_name, 7) ||
			!_exfatfs_reference_count(vnode->core.node_data, entry, &error))
			break;
		memset(entry, 0, sizeof(*entry));
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_dread", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
complete:
	if (result == (int)0x80010002)
		return 0;
	if (result >= 0)
		return 1;
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2506, "_exfatfs_dread",
		"sceExfatfsVopDread() failed (0x%08X)\n", result);
	return result;
}

extern SceExfatfsFnode *getWhiteoutDirectory(SceExfatfsFnode *fnode);
extern int sceExfatfsVopRename(SceExfatfsFnode *oldFnode, SceExfatfsFnode *parent,
	const void *suffix, int suffixLength, SceExfatfsFnode **output);

int _exfatfs_rename(SceVfsVnode *vnode, SceExfatfsFnode *parent,
	SceVfsPath *path, SceExfatfsFnode **output)
{
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsFnode *whiteout;
	SceExfatfsDrive *drive;
	int result, error;

	result = _lock_drive_information(mount, "_exfatfs_rename");
	if (result < 0)
		return result;
	whiteout = getWhiteoutDirectory(vnode->core.node_data);
	if (vnode->core.node_data == whiteout) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2691, "%s: can't rename whiteout directory\n", "_exfatfs_rename");
		result = 0x80010010;
	} else {
		const char *name = path->name;
		SceSize nameLength = path->name_length;

		if (!parent)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
				"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
		result = sceExfatfsVopLookup(parent, name, nameLength, NULL);
		if (!result) {
			result = 0x80010011;
			if (whiteout != parent)
				SCE_DBG_PRINTF_LEVEL(1, 0, 2704, "%s: already exist\n", "_exfatfs_rename");
		} else if (result == (int)0x80010002) {
			result = sceExfatfsVopRename(vnode->core.node_data, parent,
				path->name, path->name_length, output);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2708, "%s: _exfatfs_lookup_core failed[%#x]\n",
				"_exfatfs_rename", result);
		}
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_rename", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0 && !(result == (int)0x80010011 && whiteout == parent))
		SCE_DBG_PRINTF_LEVEL(1, 0, 2727, "%s: _exfatfs_rename_core failed[%#x]\n",
			"_exfatfs_rename", result);
	return result;
}

/* Release whiteout/lookup callers inline this rollback. */
#if FW_VERSION >= 0x04000000
void _exfatfs_lookup_rollback(SceVfsVnode *vnode, SceExfatfsFnode *fnode)
#else
static inline void _exfatfs_lookup_rollback(SceVfsVnode *vnode, SceExfatfsFnode *fnode)
#endif
{
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	int result;

	result = _lock_drive_information(mount, "_exfatfs_lookup_rollback");
	if (result)
		return;
	result = sceExfatfsVopInactive(fnode);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
			"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
			"_exfatfs_lookup_rollback_core", fnode, result);
	drive = mount->data;
	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
		return;
	}
	result = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_lookup_rollback", result);
}

extern int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
	SceUInt32 permission, unsigned char *attribute);
extern int sceExfatfsVopCreate(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output);
extern SceUInt64 fnodeToFileId(const SceExfatfsFnode *fnode);

int _exfatfs_create(SceVopCreateArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode **output = argp->vpp;
	SceVfsPath *path = argp->path;
	int flags = argp->flags;
	int mode = argp->mode;
	SceExfatfsFnode *fnode = NULL;
	SceVfsVnode *vnode = NULL;
	SceExfatfsDrive *drive = parent->core.mnt->data;
	SceVfsMount *mount;
	unsigned char attribute = 0;
	unsigned char fileAttribute;
	SceUInt64 id;
	int result, error;

	*output = NULL;
	if (!(flags & 0x200))
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4120, "exfatfs_vop_create",
			"%s: flag[%#x] without SCE_O_CREAT[%#x]\n", "exfatfs_vop_create", flags, 0x200);
	result = sceFatfsACSetFSAttributeByPermission(drive, mode, &attribute);
	if (result < 0) {
		if (result == (int)0x800f0916) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4137, "exfatfs_vop_create",
				"%s: invalid mode[%#o], name(%s)\n", "exfatfs_vop_create", mode, path->name);
			return 0x80010016;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4140, "exfatfs_vop_create",
			"%s: sceFatfsACSetFSAttributeByPermission failed[%#x], mode[%#o], name(%s)\n",
			"exfatfs_vop_create", result, mode, path->name);
		return result;
	}
	mount = parent->core.mnt;
	result = _lock_drive_information(mount, "_exfatfs_create");
	if (result < 0)
		return result;
	result = 0;
	if (parent->core.node_data == getWhiteoutDirectory(parent->core.node_data)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2541, "%s: can't access whiteout directory\n", "_exfatfs_create");
		result = 0x8001000d;
	}
	if (!parent->core.node_data)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2547,
			"%s: NULL dvp->nodeData, please report to Bug 30621\n", "_exfatfs_create");
	if (!result) {
		const char *name = path->name;
		SceSize nameLength = path->name_length;
		SceExfatfsFnode *parentFnode = parent->core.node_data;

		if (!parentFnode)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
				"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
		result = sceExfatfsVopLookup(parentFnode, name, nameLength, NULL);
		if (!result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2554, "%s: already exist\n", "_exfatfs_create");
			result = 0x80010011;
		} else if (result == (int)0x80010002) {
			fnode = NULL;
			result = sceExfatfsVopCreate(parent->core.node_data, path->name,
				path->name_length, mode, &fnode);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2557, "%s: _exfatfs_lookup_core failed[%#x]\n",
				"_exfatfs_create", result);
		}
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_create", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2573, "%s: sceExfatfsVopCreate failed[%#x]\n", "_exfatfs_create", result);
		return result;
	}
	result = get_new_vnode(parent->core.mnt, &vnode);
	if (result) {
		SceExfatfsFnode *rollbackFnode = fnode;

		mount = parent->core.mnt;
		if (!_lock_drive_information(mount, "_exfatfs_create_rollback")) {
			sceExfatfsVopRemove(parent->core.node_data, rollbackFnode);
			error = sceExfatfsVopInactive(rollbackFnode);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
					"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
					"_exfatfs_lookup_rollback_core", rollbackFnode, error);
			drive = mount->data;
			if (drive) {
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_create_rollback", error);
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
					"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
			}
		}
		return result;
	}
	fileAttribute = attribute;
	vnode->core.size = 0;
#if FW_VERSION >= 0x04000000
	vnode->core.mnt = parent->core.mnt;
	vnode->core.dd = parent;
	vnode->core.node_data = fnode;
	vnode->core.state = 1;
	vnode->core.type = 1;
#else
	vnode->core.dd = parent;
	vnode->core.node_data = fnode;
	vnode->core.type = 1;
	vnode->core.mnt = parent->core.mnt;
	vnode->core.state = 1;
#endif
	id = fnodeToFileId(fnode);
	vnode->core.fid[0] = id;
	vnode->core.fid[1] = id >> 32;
	vnode->core.acl_data[0] = fileAttribute;
	vnode->core.acl_data[1] = 0;
	vnode->core.ref_count = 1;
	*output = vnode;
	return result;
}

extern int sceExfatfsVopMkdir(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output);

int _exfatfs_mkdir(SceVopMkdirArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode **output = argp->vpp;
	SceVfsPath *path = argp->path;
	int mode = argp->mode;
	SceExfatfsFnode *fnode = NULL;
	SceVfsVnode *vnode = NULL;
	SceExfatfsDrive *drive;
	SceVfsMount *mount;
	unsigned char attribute = 0;
	unsigned char fileAttribute;
	SceUInt64 id;
	int result, error;

	*output = NULL;
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 4177, "exfatfs_vop_mkdir",
		"VOP called MKDIR %s dvp=%p\n", path->name, parent);
	result = sceFatfsACSetFSAttributeByPermission(parent->core.mnt->data, mode, &attribute);
	if (result < 0) {
		if (result == (int)0x800f0916) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4189, "exfatfs_vop_mkdir",
				"%s: invalid mode[%#o], name(%s)\n", "exfatfs_vop_mkdir", mode, path->name);
			return 0x80010016;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4192, "exfatfs_vop_mkdir",
			"%s: sceFatfsACSetFSAttributeByPermission failed[%#x], mode[%#o], name(%s)\n",
			"exfatfs_vop_mkdir", result, mode, path->name);
		return result;
	}
	mount = parent->core.mnt;
	result = _lock_drive_information(mount, "_exfatfs_mkdir");
	if (result < 0)
		return result;
	if (parent->core.node_data == getWhiteoutDirectory(parent->core.node_data)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2592, "%s: can't access whiteout directory\n", "_exfatfs_mkdir");
		result = 0x8001000d;
	} else {
		result = 0;
	}
	if (!parent->core.node_data)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2598,
			"%s: NULL dvp->nodeData, please report to Bug 30621\n", "_exfatfs_mkdir");
	if (!result) {
		if (!parent->core.node_data)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
				"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
		result = sceExfatfsVopLookup(parent->core.node_data, path->name, path->name_length, NULL);
		if (!result) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2605, "%s: already exist\n", "_exfatfs_mkdir");
			result = 0x80010011;
		} else if (result == (int)0x80010002) {
			fnode = NULL;
			result = sceExfatfsVopMkdir(parent->core.node_data, path->name,
				path->name_length, mode, &fnode);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2608, "%s: _exfatfs_lookup_core failed[%#x]\n",
				"_exfatfs_mkdir", result);
		}
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_mkdir", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2624, "%s: _exfatfs_mkdir_core failed[%#x]\n", "_exfatfs_mkdir", result);
		return result;
	}
	result = get_new_vnode(parent->core.mnt, &vnode);
	if (result) {
		SceExfatfsFnode *rollbackFnode = fnode;

		mount = parent->core.mnt;
		if (!_lock_drive_information(mount, "_exfatfs_mkdir_rollback")) {
			_check_empty_directory(parent->core.node_data, rollbackFnode);
			error = sceExfatfsVopInactive(rollbackFnode);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
					"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
					"_exfatfs_lookup_rollback_core", rollbackFnode, error);
			drive = mount->data;
			if (drive) {
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_mkdir_rollback", error);
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
					"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
			}
		}
		return result;
	}
	fileAttribute = attribute;
	vnode->core.mnt = parent->core.mnt;
	vnode->core.size = 0;
	vnode->core.dd = parent;
	vnode->core.node_data = fnode;
	vnode->core.state = 1;
	vnode->core.type = 2;
	id = fnodeToFileId(fnode);
	vnode->core.fid[0] = id;
	vnode->core.fid[1] = id >> 32;
	vnode->core.acl_data[0] = fileAttribute;
	vnode->core.acl_data[1] = 0;
	vnode->core.ref_count = 1;
	*output = vnode;
	return result;
}

int exfatfs_vop_rename(SceVopRenameArgs *argp)
{
	SceVfsVnode *oldParent = argp->odvp;
	SceVfsVnode *oldVnode = argp->ovp;
	SceVfsVnode *parent = argp->ndvp;
	SceVfsVnode **output = argp->nvpp;
	SceExfatfsDrive *drive = oldParent->core.mnt->data;
	SceExfatfsFnode *fnode = NULL;
	SceVfsVnode *vnode = NULL;
	SceVfsMount *mount;
	SceUInt64 id;
	int result, error;

	if (parent) {
		if (!parent->core.node_data)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4233,
				"%s: NULL ndvp->nodeData, please report to Bug 30621\n", "exfatfs_vop_rename");
		if (parent->core.mnt->data != drive)
			return 0x80010001;
	} else {
		parent = oldParent;
		if (!oldParent->core.node_data)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4240,
				"%s: NULL odvp->nodeData, please report to Bug 30621\n", "exfatfs_vop_rename");
	}
	result = _exfatfs_rename(oldVnode, parent->core.node_data, argp->new_path, &fnode);
	if (result < 0)
		return result;
	result = get_new_vnode(parent->core.mnt, &vnode);
	if (result) {
		SceExfatfsFnode *rollbackFnode = fnode;

		mount = oldVnode->core.mnt;
		if (!_lock_drive_information(mount, "_exfatfs_rename_rollback")) {
			error = sceExfatfsVopInactive(rollbackFnode);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
					"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
					"_exfatfs_lookup_rollback_core", rollbackFnode, error);
			drive = mount->data;
			if (drive) {
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_rename_rollback", error);
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
					"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
			}
		}
		return result;
	}
#if FW_VERSION >= 0x04000000
	vnode->core.mnt = parent->core.mnt;
#endif
	vnode->core.dd = parent;
	vnode->core.state = 1;
	vnode->core.type = oldVnode->core.type;
	vnode->core.node_data = fnode;
#if FW_VERSION < 0x04000000
	vnode->core.mnt = parent->core.mnt;
#endif
	vnode->core.size = oldVnode->core.size;
	id = fnodeToFileId(fnode);
	vnode->core.fid[0] = id;
	vnode->core.fid[1] = id >> 32;
	vnode->core.ref_count = 1;
	vnode->core.acl_data[0] = oldVnode->core.acl_data[0];
	vnode->core.acl_data[1] = oldVnode->core.acl_data[1];
	*output = vnode;
	return result;
}

int exfatfs_vop_dopen(SceVopDopenAgrs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount = vnode->core.mnt;
	SceVfsFile *file = argp->file;
	SceExfatfsDrive *drive;
	int result, error;

	SCE_DBG_PRINTF_LEVEL(2, 15, 2835, "called DOPEN %s vp = %p\n", argp->path->name, vnode);
	result = _lock_drive_information(mount, "exfatfs_vop_dopen");
	if (result < 0)
		return result;
	result = sceExfatfsVopDopen(vnode->core.node_data, &file->fd);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_dopen", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern void *allocateHeapBuffer(SceExfatfsDrive *unusedDrive);
extern void freeHeapBuffer(int unused, void *buffer);
extern int sceExfatfsVopGetstat(SceExfatfsFnode *fnode, SceIoStat *stat);

int exfatfs_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode **output = argp->vpp;
	const char *name = argp->path->name;
	int length = argp->path->name_length;
	SceVfsMount *mount = parent->core.mnt;
	SceExfatfsFnode *fnode = NULL;
	SceVfsVnode *vnode = NULL;
	SceExfatfsDrive *drive;
	SceIoStat stat;
	SceUInt64 id;
	unsigned char attribute;
	char *buffer;
	int result, error;

	SCE_DBG_ASSERT(length >= 0, 2912);
	result = _lock_drive_information(mount, "exfatfs_vop_lookup");
	if (result < 0)
		goto failed;
	if (!parent->core.node_data)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2918,
			"%s: NULL dvp->nodeData, please report to Bug 30621\n", "exfatfs_vop_lookup");
	if (name[length] == '/') {
		drive = parent->core.mnt->data;
		buffer = allocateHeapBuffer(drive);
		if (buffer) {
			if (length)
				memcpy(buffer, name, length);
			buffer[length] = 0;
			SceExfatfsFnode *parentFnode = parent->core.node_data;

			if (!parentFnode)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
					"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
			result = sceExfatfsVopLookup(parentFnode, buffer, length, &fnode);
			freeHeapBuffer((int)drive, buffer);
		} else {
			SCE_DBG_PRINTF_LEVEL(1, 0, 2931, "%s: sceFatfsCraeteHeap failed\n", "exfatfs_vop_lookup");
			result = 0x8001000c;
		}
	} else {
		SceExfatfsFnode *parentFnode = parent->core.node_data;

		if (!parentFnode)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
				"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
		result = sceExfatfsVopLookup(parentFnode, name, length, &fnode);
	}
	if (result >= 0) {
		result = sceExfatfsVopGetstat(fnode, &stat);
		if (result < 0) {
			sceExfatfsVopInactive(fnode);
			fnode = NULL;
		}
	} else {
		SCE_DBG_PRINTF_LEVEL(2, 15, 2939, "sceExfatfsVopLookup() failed (0x%08X)\n", result);
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_lookup", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0)
		goto failed;
	result = get_new_vnode(parent->core.mnt, &vnode);
	if (result) {
		if (fnode)
			_exfatfs_lookup_rollback(parent, fnode);
		return result;
	}
	attribute = stat.st_attr;
	if ((stat.st_mode & 0xf000) == 0x1000) {
#if FW_VERSION >= 0x04000000
		vnode->core.type = 2;
		vnode->core.dd = parent;
		vnode->core.node_data = fnode;
		vnode->core.mnt = parent->core.mnt;
		vnode->core.state = 1;
#else
		vnode->core.node_data = fnode;
		vnode->core.state = 1;
		vnode->core.type = 2;
		vnode->core.mnt = parent->core.mnt;
		vnode->core.dd = parent;
#endif
	} else {
		vnode->core.dd = parent;
#if FW_VERSION >= 0x04000000
		vnode->core.type = 1;
		vnode->core.node_data = fnode;
		vnode->core.state = 1;
		vnode->core.mnt = parent->core.mnt;
#else
		vnode->core.node_data = fnode;
		vnode->core.type = 1;
		vnode->core.mnt = parent->core.mnt;
		vnode->core.state = 1;
#endif
	}
	vnode->core.size = stat.st_size;
	id = fnodeToFileId(fnode);
	vnode->core.acl_data[0] = attribute;
	vnode->core.acl_data[1] = 0;
	vnode->core.fid[0] = id;
	vnode->core.fid[1] = id >> 32;
	vnode->core.ref_count = 1;
	*output = vnode;
	return result;
failed:
	SCE_DBG_PRINTF_LEVEL(2, 15, 2951, "err ret = %x\n", result);
	return result;
}

extern int sceFatfsACCheckRDirEntry(SceExfatfsDrive *drive, unsigned char attribute);

/* Both release stat operations inline this common access and locking path. */
#if FW_VERSION >= 0x04000000
int _exfatfs_getstat_common(SceVfsVnode *vnode, SceIoStat *stat, const char *function)
#else
static inline int _exfatfs_getstat_common(SceVfsVnode *vnode, SceIoStat *stat, const char *function)
#endif
{
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	int result, error;

	result = sceFatfsACCheckRDirEntry(mount->data, vnode->core.acl_data[0]);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 2993, "%s: sceFatfsACCheckRDirEntry failed[%#x]\n",
			"_exfatfs_getstat_common", result);
		return result;
	}
	result = _lock_drive_information(mount, function);
	if (result < 0)
		return result;
	result = sceExfatfsVopGetstat(vnode->core.node_data, stat);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", function, error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

int exfatfs_vop_fgetstat(SceVopFgetstatArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceIoStat *stat = argp->stat;

	if (!file) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3046, "%s: unexpected NULL file[%p], vp[%p]\n",
			"exfatfs_vop_fgetstat", file, vnode);
		SCE_DBG_ASSERT(file != NULL, 3047);
	} else if (file->vp != vnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3049, "%s: vp[%p] != file->vnode[%p], file[%p]\n",
			"exfatfs_vop_fgetstat", vnode, file->vp, file);
		SCE_DBG_ASSERT(0, 3050);
	}
	memset(stat, 0, sizeof(*stat));
	return _exfatfs_getstat_common(vnode, stat, "exfatfs_vop_fgetstat");
}

int exfatfs_vop_getstat(SceVopGetstatArgs *argp)
{
	SceIoStat *stat = argp->stat;
	SceVfsVnode *vnode = argp->vp;

	memset(stat, 0, sizeof(*stat));
	return _exfatfs_getstat_common(vnode, stat, "exfatfs_vop_getstat");
}

extern int sceExfatfsVopOpen(SceExfatfsFnode *fnode, int flags, int *fd);
extern int _exfatfs_renamed_file(const SceExfatfsFnode *vnode, const SceExfatfsFnode *file);

static inline void _file_size_sync_check(SceVfsVnode *vnode, SceVfsFile *file, const char *function)
{
	if (vnode != file->vp) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3061, "_file_size_sync_check",
			"%s: vp[%p] != file->vnode[%p]\n", function, vnode, file->vp);
		if (!_exfatfs_renamed_file(vnode->core.node_data, file->vp->core.node_data))
			SCE_DBG_ASSERT_FUNC(vnode == file->vp, 3069, "_file_size_sync_check");
	}
}

int exfatfs_vop_open(SceVopOpenArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount = vnode->core.mnt;
	int flags = argp->flags & ~0x200;
	SceVfsFile *file = argp->file;
	SceExfatfsDrive *drive;
	int result, error;

	SCE_DBG_PRINTF_LEVEL(2, 15, 3164, "called OPEN %s vp = %p flag = 0x%x\n",
		argp->path->name, vnode, argp->flags);
	result = _lock_drive_information(mount, "exfatfs_vop_open");
	if (result < 0)
		return result;
	result = sceExfatfsVopOpen(vnode->core.node_data, flags, (int *)&file->fd);
	_file_size_sync_check(vnode, file, "exfatfs_vop_open");
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_open", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern int sceExfatfsVopLseek(unsigned int fd, SceOff offset, int whence, SceOff *position);

static inline void _file_size_check_before_lseek(SceVfsVnode *vnode, SceVfsFile *file)
{
	if (vnode != file->vp) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3132, "_file_size_check_before_lseek",
			"%s: vp[%p] != file->vnode[%p]\n", "exfatfs_vop_lseek", vnode, file->vp);
		if (!_exfatfs_renamed_file(vnode->core.node_data, file->vp->core.node_data))
			SCE_DBG_ASSERT_FUNC(vnode == file->vp, 3140, "_file_size_check_before_lseek");
	}
}

SceOff exfatfs_vop_lseek(SceVopLseekArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount = vnode->core.mnt;
	SceVfsFile *file = argp->file;
	SceOff offset = argp->offset;
	int whence = argp->whence;
	SceExfatfsDrive *drive;
	SceOff position;
	int result, error;

	result = _lock_drive_information(mount, "exfatfs_vop_lseek");
	if (result < 0)
		return result;
	_file_size_check_before_lseek(vnode, file);
	if (whence == 1) {
		offset += file->position;
		whence = 0;
	} else if (whence == 2) {
		offset += vnode->core.size;
		whence = 0;
	}
	result = sceExfatfsVopLseek(file->fd, offset, whence, &position);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_lseek", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0)
		return result;
	file->position = position;
	return position;
}

extern int validateDateTime(const SceExfatfsDateTime *dateTime);
extern int sceFatfsACSetFSAttributeByAttribute(SceExfatfsDrive *drive,
	unsigned char attribute, unsigned char *output);
extern int _chstat_setup(SceExfatfsFnode *fnode, SceIoStat *stat, unsigned int bits);
extern SceOff _exfatfs_vnode_size(SceExfatfsFnode *fnode);

int _exfatfs_chstat_common(SceVfsVnode *vnode, SceIoStat *stat, unsigned int bits)
{
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	unsigned char attribute = 0;
	int result, error;

	if ((bits & 8) && validateDateTime((const SceExfatfsDateTime *)&stat->st_ctime))
		goto invalidArgument;
	if ((bits & 0x20) && validateDateTime((const SceExfatfsDateTime *)&stat->st_mtime))
		goto invalidArgument;
	if ((bits & 0x10) && validateDateTime((const SceExfatfsDateTime *)&stat->st_atime))
		goto invalidArgument;
	if (bits & 1) {
		result = sceFatfsACSetFSAttributeByPermission(mount->data, stat->st_mode, &attribute);
		if (result < 0) {
			if (result == (int)0x800f0916) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 3786, "%s: invalid mode[%#o]\n",
					"_exfatfs_chstat_common", stat->st_mode);
				goto invalidArgument;
			}
			SCE_DBG_PRINTF_LEVEL(1, 0, 3789,
				"%s: sceFatfsACSetFSAttributeByPermission failed[%#x], mode[%#o]\n",
				"_exfatfs_chstat_common", result, stat->st_mode);
			return result;
		}
	}
	if (bits & 0x10000) {
		result = sceFatfsACSetFSAttributeByAttribute(mount->data, stat->st_attr, &attribute);
		if (result < 0) {
			if (result == (int)0x800f0916) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 3799, "%s: invalid st_attr[%#x]\n",
					"_exfatfs_chstat_common", stat->st_attr);
				goto invalidArgument;
			}
			SCE_DBG_PRINTF_LEVEL(1, 0, 3802,
				"%s: sceFatfsACSetFSAttributeByAttribute failed[%#x], st_attr[%#x]\n",
				"_exfatfs_chstat_common", result, stat->st_attr);
			return result;
		}
	}
	result = _lock_drive_information(mount, "_exfatfs_chstat_common");
	if (result < 0)
		return result;
	if (vnode->core.node_data == getWhiteoutDirectory(vnode->core.node_data)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3812, "%s: can't access whiteout directory\n",
			"_exfatfs_chstat_common");
		result = 0x8001000d;
	} else if (!result) {
		result = _chstat_setup(vnode->core.node_data, stat, bits);
		if (!result) {
			if (bits & 4)
				vnode->core.size = stat->st_size;
			if (bits & 0x10001) {
				vnode->core.acl_data[0] = attribute;
				vnode->core.acl_data[1] = 0;
			}
			goto unlock;
		}
	}
	if (bits & 4)
		vnode->core.size = _exfatfs_vnode_size(vnode->core.node_data);
unlock:
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_exfatfs_chstat_common", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
invalidArgument:
	return 0x80010016;
}

int exfatfs_vop_fchstat(SceVopFchstatArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceIoStat *stat = argp->stat;
	unsigned int bits = argp->bit;

	SCE_DBG_PRINTF_LEVEL(2, 0, 3865, "%s: vp[%p], file[%p], aclData[%#llx]\n",
		"exfatfs_vop_fchstat", vnode, file,
		(SceUInt64)vnode->core.acl_data[0] | (SceUInt64)vnode->core.acl_data[1] << 32);
	if (!file) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3867, "%s: unexpected NULL file[%p], vp[%p]\n",
			"exfatfs_vop_fchstat", file, vnode);
		SCE_DBG_ASSERT(file != NULL, 3868);
	} else if (file->vp != vnode) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3870, "%s: vp[%p] != file->vnode[%p], file[%p]\n",
			"exfatfs_vop_fchstat", vnode, file->vp, file);
		SCE_DBG_ASSERT(0, 3871);
	}
	return _exfatfs_chstat_common(vnode, stat, bits);
}

int exfatfs_vop_chstat(SceVopChstatArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsPath *path = argp->path;
	SceIoStat *stat = argp->stat;
	unsigned int bits = argp->bit;

	SCE_DBG_PRINTF_LEVEL(2, 0, 3851, "%s: vp[%p], name(%s), size(%d), aclData[%#llx]\n",
		"exfatfs_vop_chstat", vnode, path->name, path->name_length,
		(SceUInt64)vnode->core.acl_data[0] | (SceUInt64)vnode->core.acl_data[1] << 32);
	return _exfatfs_chstat_common(vnode, stat, bits);
}

int exfatfs_vop_remove(SceVopRemoveArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
#if FW_VERSION < 0x04000000
	SceExfatfsDrive *lockedDrive = mount->data;
#endif
	int result, error, locked;

	SCE_DBG_PRINTF_LEVEL(2, 15, 3892, "VOP called REMOVE %s dvp=%p vp=%p\n",
		argp->path->name, parent, vnode);
	result = _lock_drive_information(mount, "exfatfs_vop_remove");
	if (result < 0)
		return result;
	locked = result == 0;
	SCE_DBG_ASSERT(locked, 3901);
	if (!parent)
		result = sceExfatfsVopRemove(NULL, vnode->core.node_data);
	else if (locked)
		result = sceExfatfsVopRemove(parent->core.node_data, vnode->core.node_data);
#if FW_VERSION < 0x04000000
	else {
		drive = lockedDrive;
		goto unlock;
	}
#endif
	drive = mount->data;
#if FW_VERSION < 0x04000000
unlock:
#endif
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_remove", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

int exfatfs_vop_rmdir(SceVopRmdirArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	SceExfatfsFnode *whiteout, *fnode;
	int result, error, locked;

	SCE_DBG_PRINTF_LEVEL(2, 15, 3949, "VOP called RMDIR %s dvp=%p vp=%p\n",
		argp->path->name, parent, vnode);
	result = _lock_drive_information(mount, "exfatfs_vop_rmdir");
	if (result < 0)
		return result;
	locked = result == 0;
	SCE_DBG_ASSERT(locked, 3958);
	whiteout = getWhiteoutDirectory(vnode->core.node_data);
	fnode = vnode->core.node_data;
	if (fnode == whiteout) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3960, "%s: can't delete whiteout directory\n", "exfatfs_vop_rmdir");
		result = 0x80010010;
	} else if (!parent) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 3976, "%s: unexpected NULL dvp for directory vp[%p]\n",
			"exfatfs_vop_rmdir", vnode);
		result = 0x80010016;
	} else if (locked) {
		result = _check_empty_directory(parent->core.node_data, fnode);
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_rmdir", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern int SceSblACMgrForDriver_D7AD8471(SceUID pid, const char *assignName);
extern int exfatfs_get_drive_information(SceExfatfsDrive *drive, void *information, int size);
extern int exfatfs_get_file_size_limit(SceExfatfsDrive *drive, SceUInt64 *limit);
extern int exfatfs_set_fake_free(SceExfatfsDrive *drive, SceUInt64 freeSize);
extern int exfatfs_clear_fake_free(SceExfatfsDrive *drive);
extern int checkDriveWritable(SceExfatfsDrive *drive);
extern void _exfatfs_set_whiteout_directory(SceExfatfsFnode *fnode);

int exfatfs_vfs_devctl(SceVfsOpDevctlArg *argp)
{
	SceVfsMount *mount = argp->mnt;
	int command = argp->cmd;
	const SceIoDevInfo *argument = argp->arg;
	int arglen = argp->arg_len;
	SceIoDevInfo *information = argp->buf;
	int buflen = argp->buf_len;
	SceExfatfsDrive *drive;
	SceVfsMountData *mountData;
	const char *function;
	SceUInt64 fakeFree;
	int result, error, access;

	if (!mount->data)
		return 0x80010013;
	switch (command) {
	case 0x3001:
		if (SceSblACMgrForDriver_D7AD8471(ksceKernelGetProcessIdFromTLS(),
			mount->mnt_data->assign_name) != 1)
			return 0x80010030;
		if (arglen)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4465,
				"%s(get_drive_information): ignore arglen[%#x], arg[%p], devname(%s)\n",
				"exfatfs_vfs_devctl", arglen, argument, argp->dev);
		if (buflen != 24 && buflen != 40) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 4469, "%s: buflen(%d) != expected(%d or %d)\n",
				"exfatfs_vfs_devctl", buflen, 24, 40);
			return 0x80010016;
		}
		function = "_get_drive_information";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		drive = mount->data;
		mountData = drive->deviceInfo.mount_misc;
		result = exfatfs_get_drive_information(drive, information, buflen);
		if (result >= 0 && mountData->mnt_id == 0x800) {
			SceUInt64 freeSize = information->free_size;
			information->free_size = freeSize <= 0x2000000 ? 0 : freeSize - 0x2000000;
		}
		break;
	case 0x80000001:
		if (buflen != 8) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 4543, "%s: buflen(%d) != expected(%d)\n",
				"exfatfs_vfs_devctl", buflen, 8);
			return 0x80010016;
		}
		function = "_get_file_size_limit";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		result = exfatfs_get_file_size_limit(mount->data, (SceUInt64 *)information);
		break;
	case 0x3004:
		access = SceThreadmgrForDriver_332E127C();
		if (access != 0x80 && access != 0x40)
			return 0x80010030;
		if (buflen)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4504,
				"%s(set_fake_free): ignore buflen[%#x], bufp[%p], devname(%s)\n",
				"exfatfs_vfs_devctl", buflen, information, argp->dev);
		if (arglen != 24) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 4507, "%s: arglen(%d) != expected(%d)\n",
				"exfatfs_vfs_devctl", arglen, 24);
			return 0x80010016;
		}
		fakeFree = argument->free_size;
		function = "_set_fake_free";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		result = exfatfs_set_fake_free(mount->data, fakeFree);
		break;
	case 0x3005:
		access = SceThreadmgrForDriver_332E127C();
		if (access != 0x80 && access != 0x40)
			return 0x80010030;
		if (buflen)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4520,
				"%s(clear_fake_free): ignore buflen[%#x], bufp[%p], devname(%s)\n",
				"exfatfs_vfs_devctl", buflen, information, argp->dev);
		if (arglen) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 4523, "%s: arglen(%d) != expected(%d)\n",
				"exfatfs_vfs_devctl", arglen, 0);
			return 0x80010016;
		}
		function = "_clear_fake_free";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		result = exfatfs_clear_fake_free(mount->data);
		break;
	case 0x3803:
		access = SceThreadmgrForDriver_332E127C();
		if (access != 0x80 && access != 0x40)
			return 0x80010030;
		if (arglen | buflen)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4494,
				"%s(delete_whiteout_entry): ignore arglen[%#x], arg[%p], buflen[%#x], bufp[%p], devname(%s)\n",
				"exfatfs_vfs_devctl", arglen, argument, buflen, information, argp->dev);
		function = "_delete_whiteout_entry";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		result = _delete_whiteout_entry_core(mount->data);
		break;
	case 0x3802:
		access = SceThreadmgrForDriver_332E127C();
		if (access != 0x80 && access != 0x40)
			return 0x80010030;
		if (arglen | buflen)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4481,
				"%s(set_whiteout_directory): ignore arglen[%#x], arg[%p], buflen[%#x], bufp[%p], devname(%s)\n",
				"exfatfs_vfs_devctl", arglen, argument, buflen, information, argp->dev);
		function = "_set_whiteout_directory";
		result = _lock_drive_information(mount, function);
		if (result < 0)
			return result;
		drive = mount->data;
		error = checkDriveWritable(drive);
		if (error == (int)0x8001001e) {
			result = error;
		} else if (error) {
			if (error == (int)0x80010002)
				result = 0x80010013;
		} else {
			SceExfatfsFnode *root = NULL, *whiteout = NULL;
			result = _sceExfatfsVopGetRoot(drive, &root);
			if (!result) {
				SceExfatfsFnode *parentFnode = root;
				char lookupName[] = "SCEIOTRASH";

				if (!parentFnode)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2050, "_exfatfs_lookup_core",
						"%s: NULL parent_data, please report to Bug 30621\n", "_exfatfs_lookup_core");
				result = sceExfatfsVopLookup(parentFnode, lookupName, 10, &whiteout);
				if (result == (int)0x80010002) {
					unsigned char attribute = 0;
					char mkdirName[] = "SceIoTrash";
					result = sceFatfsACSetFSAttributeByPermission(drive, 6, &attribute);
					if (result < 0) {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2365, "_set_whiteout_directory_core",
							"%s: sceFatfsACSetFSAttributeByPermission failed[%#x]\n",
							"_set_whiteout_directory_core", result);
					} else {
						whiteout = NULL;
						result = sceExfatfsVopMkdir(root, mkdirName, 10, 6, &whiteout);
						if (result) {
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2369, "_set_whiteout_directory_core",
								"%s: sceExfatfsVopMkdir failed[%#x]\n", "_set_whiteout_directory_core", result);
							if (result == (int)0x8001001c) {
								SceIoDevInfo info;
								error = exfatfs_get_drive_information(drive, &info, 24);
								if (error) {
									result = error;
									SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2383, "_set_whiteout_directory_core",
										"%s: get_drive_information failed[%#x]\n", "_set_whiteout_directory_core", error);
								} else if (info.free_size) {
									result = 0x80010005;
								}
							}
						}
					}
				} else if (result) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2396, "_set_whiteout_directory_core",
						"%s: sceExfatfsVopLookup failed[%#x]\n", "_set_whiteout_directory_core", result);
				}
				parentFnode = root;
				error = sceExfatfsVopInactive(parentFnode);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2037, "_exfatfs_lookup_rollback_core",
						"%s: _decrement_entry_reference for entry_data[%p] failed[%#x]\n",
						"_exfatfs_lookup_rollback_core", parentFnode, error);
				if (!result)
					_exfatfs_set_whiteout_directory(whiteout);
			}
		}
		break;
	default:
		return 0x80010030;
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", function, error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result < 0 && command == 0x3004)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4064, "_set_fake_free",
			"%s: exfatfs_set_fake_free failed[%#x]\n", "_set_fake_free", result);
	if (result < 0 && command == 0x3005)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 4091, "_clear_fake_free",
			"%s: exfatfs_clear_fake_free failed[%#x]\n", "_clear_fake_free", result);
	return result;
}

extern int sceExfatfsVopLink(SceExfatfsFnode *from, SceExfatfsFnode *to);

int exfatfs_vop_link(SceVopLinkArgs *argp)
{
	SceVfsVnode *from = argp->fvp;
	SceVfsVnode *to = argp->tvp;
	SceVfsMount *mount = from->core.mnt;
	SceExfatfsDrive *drive;
	int result, error;

	result = _lock_drive_information(mount, "exfatfs_vop_link");
	if (result < 0)
		return result;
	result = sceExfatfsVopLink(from->core.node_data, to->core.node_data);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_link", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

/* Full Sony _lock_drive_information variant; mounted-only callers use the
 * specialized two-argument body above. Release callers inline this variant. */
#if FW_VERSION >= 0x04000000
int lockDriveInformationAllowUnmount(SceVfsMount *mount, int allowUnmount, const char *function)
{
	SceExfatfsDrive *drive = mount->data;
	SceExfatfsDrive *current;
	int result, error;

	if (!drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1270, "_lock_drive_information",
			"%s: mp[%p] doesn't have drive\n", function, mount);
		return 0x80010013;
	}
	result = ksceKernelLockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 791, "_lock_drive_information_core",
			"%s: sceKernelLockFastMutex failed[%#x]\n", function, result);
		return result;
	}
	current = mount->data;
	if (current != drive) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1254, "_lock_drive_information",
			"%s: drive[%p]->[%p], mp[%p]\n", function, drive, current, mount);
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		goto unlockFailed;
	}
	if (!(current->state & 1)) {
		if (allowUnmount) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1260, "_lock_drive_information",
				"%s: unmounting, mp[%p], drive[%p], flags[%#x]\n",
				function, mount, current, current->state);
			return result;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1262, "_lock_drive_information",
			"%s: not mounted, mp[%p], drive[%p], flags[%#x]\n",
			function, mount, current, current->state);
	} else {
		return result;
	}
	error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
unlockFailed:
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", function, error);
	return 0x80010013;
}
#else
static inline int lockDriveInformationAllowUnmount(SceVfsMount *mount, int allowUnmount,
	const char *function)
{
	SceExfatfsDrive *drive = mount->data;
	int result;

	if (!drive)
		return 0x80010013;
	result = ksceKernelLockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (result < 0)
		return result;
	if (drive != mount->data || (!(drive->state & 1) && !allowUnmount)) {
		ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		return 0x80010013;
	}
	return result;
}
#endif

extern int sceExfatfsVopCleanup(SceExfatfsFnode *fnode, SceUInt32 fd);

int exfatfs_vop_cleanup(SceVopCleanupArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	int result, error;

	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vop_cleanup");
#if FW_VERSION >= 0x04000000
	if (result == (int)0x80010013) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4599, "%s: NULL mnt->pData, after force umount?\n",
			"exfatfs_vop_cleanup");
	} else if (result < 0) {
		return result;
	}
#else
	if (result < 0) {
		if (result != (int)0x80010013)
			return result;
		SCE_DBG_PRINTF_LEVEL(1, 0, 4599, "%s: NULL mnt->pData, after force umount?\n",
			"exfatfs_vop_cleanup");
	}
#endif
	if (!vnode->core.node_data)
		result = 0x80010013;
	else if (file)
		result = sceExfatfsVopCleanup(vnode->core.node_data, file->fd);
	else
		result = sceExfatfsVopCleanup(vnode->core.node_data, 0);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_cleanup", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern int sceExfatfsVopSync(SceExfatfsFnode *fnode, SceUInt32 fd, int flags);

int exfatfs_vop_sync(SceVopSyncArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	int flags = argp->flags;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	SceExfatfsFnode *fnode;
	int result, error;

	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vop_sync");
#if FW_VERSION >= 0x04000000
	if (result == (int)0x80010013) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4568, "%s: NULL mnt->pData, after force umount?\n",
			"exfatfs_vop_sync");
	} else if (result < 0) {
		return result;
	}
#else
	if (result < 0) {
		if (result != (int)0x80010013)
			return result;
		SCE_DBG_PRINTF_LEVEL(1, 0, 4568, "%s: NULL mnt->pData, after force umount?\n",
			"exfatfs_vop_sync");
	}
#endif
	fnode = vnode->core.node_data;
	if (!fnode) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 2157, "_exfatfs_sync_core",
			"%s: unexpected NULL nodeData, vp[%p], please report to Bug 29181\n",
			"_exfatfs_sync_core", vnode);
		result = 0x80010013;
	} else if (file) {
		result = sceExfatfsVopSync(fnode, file->fd, flags);
	} else {
		result = sceExfatfsVopSync(fnode, 0, flags);
	}
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_sync", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern int _exfatfs_flush_all_data(SceExfatfsDrive *drive);

int exfatfs_vfs_sync(SceVfsOpSyncArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceExfatfsDrive *drive;
	int result, error;

#if FW_VERSION < 0x04000000
	drive = mount->data;
#endif
	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vfs_sync");
#if FW_VERSION >= 0x04000000
	if (result == (int)0x80010013) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 4625, "%s: NULL mnt->pData, after force umount?\n",
			"exfatfs_vfs_sync");
	} else if (result < 0) {
		return result;
	}
	drive = result == 0 ? mount->data : NULL;
#else
	if (result < 0 && result != (int)0x80010013)
		return result;
	if (result != 0)
		drive = NULL;
#endif
	result = _exfatfs_flush_all_data(drive);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vfs_sync", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	return result;
}

extern int sceExfatfsVopPread(unsigned int fd, void *buffer, unsigned int size,
	SceOff offset, unsigned int *readSize);

int exfatfs_vop_read(SceVopReadArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceSize size = argp->nbyte;
	SceVfsMount *mount = vnode->core.mnt;
	void *buffer = argp->buf;
	SceExfatfsDrive *drive;
	unsigned int readSize = 0;
	int result, error;

	SCE_DBG_PRINTF_LEVEL(2, 15, 3399, "VOP called READ vp=%p size=%x file=%p\n",
		vnode, size, file);
	if (!size)
		return 0;
	if ((uintptr_t)buffer & 3)
		SCE_DBG_PRINTF_LEVEL(1, 0, 3423, "%s: unaligned buffer[%p]\n",
			"exfatfs_vop_read", buffer);
	result = _lock_drive_information(mount, "exfatfs_vop_read");
	if (result < 0)
		return result;
	_file_size_sync_check(vnode, file, "exfatfs_vop_read");
	result = sceExfatfsVopPread(file->fd, buffer, size, file->position, &readSize);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_read", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result >= 0) {
		file->position += readSize;
		result = readSize;
	}
	return result;
}

extern int sceExfatfsVopPwrite(unsigned int fd, const void *buffer, unsigned int size,
	SceOff offset, unsigned int *writeSize);
extern SceOff _exfatfs_file_size(unsigned int fd);

int exfatfs_vop_write(SceVopWriteArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceSize size = argp->nbyte;
	SceVfsMount *mount = vnode->core.mnt;
	const unsigned char *buffer = argp->buf;
	SceExfatfsDrive *drive;
	unsigned int writeSize = 0, total = 0, left = size;
	SceUInt64 fileSize;
	int result, error;

	SCE_DBG_PRINTF_LEVEL(2, 15, 3573, "VOP called WRITE vp=%p size=%x file=%p\n",
		vnode, size, file);
	if (!size)
		return 0;
	if ((uintptr_t)buffer & 3)
		SCE_DBG_PRINTF_LEVEL(1, 0, 3583, "%s: unaligned buffer[%p]\n",
			"exfatfs_vop_write", buffer);
	result = _lock_drive_information(mount, "exfatfs_vop_write");
	if (result < 0)
		return result;
	if (vnode != file->vp) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3090, "_file_size_check_before_write",
			"%s: vp[%p] != file->vnode[%p]\n", "exfatfs_vop_write", vnode, file->vp);
		if (!_exfatfs_renamed_file(vnode->core.node_data, file->vp->core.node_data))
			SCE_DBG_ASSERT_FUNC(vnode == file->vp, 3098, "_file_size_check_before_write");
	}
	for (;;) {
		result = sceExfatfsVopPwrite(file->fd, buffer + total, left,
			file->position + total, &writeSize);
		if (result < 0)
			break;
		if (!writeSize) {
			if (size > total)
				SCE_DBG_PRINTF_LEVEL(1, 0, 3630,
					"%s: unexpected wbyte(%d), nbyte(%d), total(%d), left(%d)\n",
					"exfatfs_vop_write", writeSize, size, total, left);
			break;
		}
		total += writeSize;
		writeSize = 0;
		if (size <= total)
			break;
		left = size - total;
		SCE_DBG_PRINTF_LEVEL(1, 0, 3626, "%s: nbyte(%d) > total(%d), left(%d)\n",
			"exfatfs_vop_write", size, total, left);
	}
	writeSize = total;
	fileSize = _exfatfs_file_size(file->fd);
	if ((SceUInt64)vnode->core.size < fileSize)
		vnode->core.size = fileSize;
	_file_size_sync_check(vnode, file, "exfatfs_vop_write");
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_write", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result >= 0) {
		file->position += writeSize;
		result = writeSize;
		if (size != writeSize)
			SCE_DBG_PRINTF_LEVEL(2, 15, 3655, "write_req: %d, write_ret: %d\n", size, writeSize);
	}
	return result;
}

int exfatfs_vop_pread(SceVopPreadArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	void *buffer = argp->buf;
	SceSize size = argp->nbyte;
	SceOff offset = argp->offset;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	unsigned int readSize;
	int result, error;

	if (!size)
		return 0;
	result = _lock_drive_information(mount, "exfatfs_vop_pread");
	if (result < 0)
		return result;
	_file_size_sync_check(vnode, file, "exfatfs_vop_pread");
	result = sceExfatfsVopPread(file->fd, buffer, size, offset, &readSize);
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_pread", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result >= 0)
		result = readSize;
	return result;
}

int exfatfs_vop_pwrite(SceVopPwriteArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	const void *buffer = argp->buf;
	SceSize size = argp->nbyte;
	SceOff offset = argp->offset;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive;
	unsigned int writeSize;
	SceUInt64 fileSize;
	int result, error;

	if (!size)
		return 0;
	result = _lock_drive_information(mount, "exfatfs_vop_pwrite");
	if (result < 0)
		return result;
	if (vnode != file->vp) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 3090, "_file_size_check_before_write",
			"%s: vp[%p] != file->vnode[%p]\n", "exfatfs_vop_pwrite", vnode, file->vp);
		if (!_exfatfs_renamed_file(vnode->core.node_data, file->vp->core.node_data))
			SCE_DBG_ASSERT_FUNC(vnode == file->vp, 3098, "_file_size_check_before_write");
	}
	result = sceExfatfsVopPwrite(file->fd, buffer, size, offset, &writeSize);
	fileSize = _exfatfs_file_size(file->fd);
	if ((SceUInt64)vnode->core.size < fileSize)
		vnode->core.size = fileSize;
	drive = mount->data;
	if (drive) {
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_pwrite", error);
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1282, "_unlock_drive_information",
			"%s: mp[%p] doesn't have drive\n", "_unlock_drive_information", mount);
	}
	if (result >= 0)
		result = writeSize;
	return result;
}

extern int probeWhiteoutDestination(SceExfatfsFnode *parent, const SceVfsPath *path);
extern void _exfatfs_set_whiteout_bit(SceExfatfsFnode *fnode);

int exfatfs_vop_whiteout(SceVopWhiteoutArgs *argp)
{
	SceVfsVnode *parent = argp->dvp;
	SceVfsVnode *vnode = argp->vp;
	SceVfsPath *path = argp->new_path;
	SceExfatfsFnode *whiteout = getWhiteoutDirectory(parent->core.node_data);
	SceVfsVnode *newVnode = NULL;
	SceVfsVnode *releasedVnode;
	SceExfatfsFnode *output = NULL;
	int result, error;

	if (!whiteout) {
		whiteout = parent->core.node_data;
		if (!whiteout)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4280, "%s: NULL dvp->nodeData, please report to Bug 30621\n",
				"exfatfs_vop_whiteout");
	}
	if (!probeWhiteoutDestination(whiteout, path))
		return 0x80010011;
	result = get_new_vnode(parent->core.mnt, &newVnode);
	if (result < 0)
		return result;
	result = _exfatfs_rename(vnode, whiteout, path, &output);
	if (!result) {
		if (vnode->core.node_data != output)
			SCE_DBG_PRINTF_LEVEL(1, 0, 4297, "%s: new_entry_data[%p] != old_node[%p]\n",
				"exfatfs_vop_whiteout", output, vnode->core.node_data);
		if (parent->core.node_data != whiteout)
			_exfatfs_set_whiteout_bit(vnode->core.node_data);
		_exfatfs_lookup_rollback(parent, output);
	}
	releasedVnode = newVnode;
	error = vfsUnlockVnode(releasedVnode);
	if (error < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1744, "release_new_vnode",
			"%s: vfsUnlockVnode failed[%#x]\n", "release_new_vnode", error);
	} else {
		error = vfsFreeVnode(releasedVnode);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1748, "release_new_vnode",
				"%s: vfsFreeVnode failed[%#x]\n", "release_new_vnode", error);
	}
	return result;
}

extern int sceFatfsInitializeHeap(int reuseHeap);
extern void *allocateCommonHeapMemory(int unused, SceSize size);
extern int _cache_all_fat_data_drive(SceExfatfsDrive *drive, int unused,
	const char *devname, int unusedMode, int flags);
extern SceSize getDriveClusterSize(SceExfatfsDrive *drive);
extern int initializeLegacyDrive(SceExfatfsDrive *drive);
extern void _free_all_cluster_cache(SceExfatfsDrive *drive);

int exfatfs_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mount = argp->mnt;
	SceExfatfsDrive *drive = mount->data;
	SceVfsPath *path = argp->dev_file_path;
	SceVfsMountData *mountData;
	SceVfsVnode *deviceVnode;
	SceSdstorPartitionInfo partitionInfo;
	const char *fsName, *deviceName, *assignName;
	char *suffix;
	SceUInt32 mountFlags, flags;
	SceSize clusterSize;
	int result, error, openFlags;

	if (!drive) {
		drive = allocateCommonHeapMemory(0, sizeof(*drive));
		if (!drive)
			return 0x8001000c;
		memset(drive, 0, sizeof(*drive));
		mountData = mount->mnt_data;
		drive->deviceInfo.mount_misc = mountData;
		drive->deviceInfo.device_vnode = mount->mnt_vnode;
		drive->deviceInfo.device_file = NULL;
		drive->deviceInfo.device_fd = -1;
		if (!drive->assignName) {
			char mutexName[32] = "SceExfatfsDrive";

			assignName = mountData->assign_name;
			mutexName[15] = *assignName++ - 32;
			suffix = &mutexName[15];
			if (*assignName != '0') {
				do {
					*++suffix = *assignName++;
				} while (*assignName != '0' && suffix < &mutexName[31]);
			}
			suffix[1] = 0;
			result = ksceKernelInitializeFastMutex((SceKernelFastMutex *)&drive->fastmutex,
				mutexName, 0x2000, 0);
			if (result < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 845, "_initialize_drive_information",
					"sceKernelInitializeFastMutex failed (0x%08X)\n", result);
				freeCommonHeapMemory(0, drive);
				return result;
			}
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 849, "_initialize_drive_information",
				"%s: assignName(%s)\n", "_initialize_drive_information", mount->mnt_data->assign_name);
			mountData = mount->mnt_data;
			drive->assignName = mountData->assign_name;
		}
	} else {
		mountData = mount->mnt_data;
	}
	fsName = mountData->fs_name;
	deviceName = mountData->blockdev_name;
	if (drive->state)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1112, "_start_mount",
			"%s: unexpected drive state[%#x]\n", "_start_mount", drive->state);
	drive->state |= 0x2000;
	result = sceFatfsInitializeHeap((int)drive);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 15, 1531, "sceFatfsInitializeHeap() failed (0x%08X)\n", result);
		goto finalizeDrive;
	}
	deviceVnode = mount->mnt_vnode;
	drive->deviceInfo.device_file = NULL;
	drive->deviceInfo.device_fd = -1;
	mountFlags = mount->mnt_flags;
	drive->deviceInfo.device_vnode = deviceVnode;
	result = vfsLockVnode(deviceVnode);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 961, "_open_device_file",
			"%s: vfsLockVnode failed[%#x]\n", "_open_device_file", result);
		goto finalizeHeap;
	}
	result = vfsAllocateFile(deviceVnode, &drive->deviceInfo.device_file, path->name);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 975, "_open_device_file",
			"%s: vfsAllocateFile failed[%#x], device_file[%p]\n",
			"_open_device_file", result, drive->deviceInfo.device_file);
		drive->deviceInfo.device_file = NULL;
		error = vfsUnlockVnode(deviceVnode);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 980, "_open_device_file",
				"%s: vfsUnlockVnode failed[%#x]\n", "_open_device_file", error);
		goto finalizeHeap;
	}
	drive->deviceInfo.device_fd = result;
	if (mountFlags & 0x1000) {
		openFlags = 1;
	} else {
		if (deviceVnode->core.type & 0x2000) {
			result = ksceVfsOpDevctl(deviceVnode->core.mnt, path->name, 1,
				path->name, path->name_length + 1, &partitionInfo, sizeof(partitionInfo));
			if (result < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1013, "_open_device_file",
					"%s: sceVfsOpDevctl for (%s) failed[%#x]\n",
					"_open_device_file", path->path, result);
				if (result != (int)0x80010030) {
					drive->deviceInfo.device_file->flags = 3;
					goto freeFile;
				}
			} else if (partitionInfo.writeProtected & 1) {
				result = 0x8001001e;
				drive->deviceInfo.device_file->flags = 3;
				goto freeFile;
			} else if (result) {
				drive->deviceInfo.device_file->flags = 3;
				goto opened;
			}
		}
		openFlags = 3;
	}
	drive->deviceInfo.device_file->flags = openFlags;
	result = ksceVopOpen(deviceVnode, path, openFlags, drive->deviceInfo.device_file);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1042, "_open_device_file",
			"%s: vop_open for (%s) failed[%#x]\n", "_open_device_file",
			path->path, result);
freeFile:
		error = vfsFreeFile(deviceVnode, drive->deviceInfo.device_fd);
		if (error < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1049, "_open_device_file",
				"%s: vfsFreeFile failed[%#x]\n", "_open_device_file", error);
		} else {
			drive->deviceInfo.device_file = NULL;
			drive->deviceInfo.device_fd = -1;
		}
	} else {
opened:
		result = 0;
		++deviceVnode->core.ref_count;
	}
	error = vfsUnlockVnode(deviceVnode);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1061, "_open_device_file",
			"%s: vfsUnlockVnode failed[%#x]\n", "_open_device_file", error);
	if (result)
		goto finalizeHeap;
	flags = 0x80000001;
	if (drive->deviceInfo.device_file->flags & 2)
		flags = 0x80000000;
	result = _cache_all_fat_data_drive(drive, (int)fsName, deviceName, (int)"/", flags);
	if (result < 0) {
		if (drive->state & 0x2000)
			drive->state |= 0x20;
		else
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1191, "_mount_failed",
				"%s: unexpected drive state[%#x]\n", "_mount_failed", drive->state);
		_cleanup_after_umount(drive);
		return result;
	}
	mount->data = drive;
	if (drive->state & 0x4000) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1603, "%s: mp[%p], drive[%p]\n", "exfatfs_vfs_mount", mount, drive);
		_free_all_cluster_cache(drive);
		drive->state = 0;
		_cleanup_after_umount(drive);
		return 0x80010013;
	}
	if (drive->state & 0x2000) {
		drive->state |= 1;
	} else {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1128, "_mount_succeeded",
			"%s: unexpected drive state[%#x]\n", "_mount_succeeded", drive->state);
		drive = mount->data;
	}
	if (!drive->deviceInfo.mount_misc) {
		result = initializeLegacyDrive(drive);
		if (result < 0) {
			drive = mount->data;
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1070, "_sceExfatfsClearDeviceVnode",
				"%s: drive[%p], flag(%d)\n", "_sceExfatfsClearDeviceVnode", drive, 1);
#if FW_VERSION >= 0x04000000
			_close_device_file(&drive->deviceInfo, 1);
#else
			if (drive->deviceInfo.device_fd != -1) {
				int locked = vfsLockVnode(drive->deviceInfo.device_vnode);

				ksceVopClose(drive->deviceInfo.device_vnode, drive->deviceInfo.device_file);
				if (vfsFreeFile(drive->deviceInfo.device_vnode, drive->deviceInfo.device_fd) >= 0)
					--drive->deviceInfo.device_vnode->core.ref_count;
				drive->deviceInfo.device_file = NULL;
				drive->deviceInfo.device_fd = -1;
				if (locked >= 0)
					vfsUnlockVnode(drive->deviceInfo.device_vnode);
			}
#endif
			drive->deviceInfo.device_vnode = NULL;
			return result;
		}
	}
	mount->available_entry_num = 64;
	clusterSize = getDriveClusterSize(mount->data);
	if (clusterSize <= 0x8000)
		mount->default_io_cache_size = clusterSize;
	else
		mount->default_io_cache_size = 0x8000;
	return result;
finalizeHeap:
	error = sceFatfsFinalizeHeap((int)drive);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 1553, "sceFatfsFinalizeHeap failed (0x%08X)\n", error);
finalizeDrive:
	if (drive->deviceInfo.mount_misc) {
		error = ksceKernelFinalizeFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 868, "_finalize_drive_information",
				"%s: sceKernelFinalizeFastMutex failed[%#x]\n", "_finalize_drive_information", error);
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 873, "_finalize_drive_information",
			"%s: assignName(%s)\n", "_finalize_drive_information", drive->assignName);
		drive->assignName = NULL;
		drive->fileSystem = NULL;
		drive->reservedWords[0] = 0;
		drive->reservedWords[1] = 0;
		drive->deviceInfo.mount_misc = NULL;
		freeCommonHeapMemory(0, drive);
	}
	return result;
}

extern int sceExfatfsVopClose(unsigned int fd);
extern int _used_resource_count(SceVfsMount *mount);
extern void _free_all_cluster_cache(SceExfatfsDrive *drive);

int exfatfs_vop_close(SceVopCloseArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive, *current;
	int result, error;

	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vop_close");
	SCE_DBG_PRINTF_LEVEL(2, 15, 3202, "called CLOSE vp = %p\n", vnode);
#if FW_VERSION >= 0x04000000
	if (result != (int)0x80010013 && result < 0)
#else
	if (result < 0 && result != (int)0x80010013)
#endif
		return result;
	_file_size_sync_check(vnode, file, "exfatfs_vop_close");
	result = sceExfatfsVopClose(file->fd);
	drive = mount->data;
	if (drive) {
		if (drive->state & 0x4000) {
			if (!_used_resource_count(mount)) {
				current = mount->data;
				if (!current)
					goto closed;
				mount->data = NULL;
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1236, "_umount_after_all_close",
					"%s: mp[%p], drive[%p]\n", "_umount_after_all_close", mount, current);
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&current->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_force_umount_check", error);
				_free_all_cluster_cache(current);
				current->state = 0;
				_cleanup_after_umount(current);
			}
			current = mount->data;
			if (!current)
				goto closed;
			if (drive != current) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1307, "_force_umount_check",
					"%s: mp->pData[%p] != expected[%p]\n", "exfatfs_vop_close", current, drive);
				goto closed;
			}
		}
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_close", error);
	}
closed:
	if (!result)
		file->fd = 0;
	return result;
}

extern void _clear_whiteout_directory(SceExfatfsFnode *fnode);

int exfatfs_vop_inactive(SceVopInactiveArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsMount *mount;
	SceExfatfsDrive *drive, *current;
	int result, error;

	result = lockDriveInformationAllowUnmount(vnode->core.mnt, 1, "exfatfs_vop_inactive");
	SCE_DBG_PRINTF_LEVEL(2, 15, 2795, "called INACTIVE vp = %p\n", vnode);
#if FW_VERSION >= 0x04000000
	if (result != (int)0x80010013 && result < 0)
#else
	if (result < 0 && result != (int)0x80010013)
#endif
		return result;
	if (vnode->core.type == 0x1002)
		_clear_whiteout_directory(vnode->core.node_data);
	result = sceExfatfsVopInactive(vnode->core.node_data);
	mount = vnode->core.mnt;
	drive = mount->data;
	if (drive && !(drive->state & 0x4000) && (drive->state & 1))
		goto unlock;
	if (_used_resource_count(mount)) {
		current = mount->data;
		if (current && (current->state & 0x4000) &&
		    (vnode->core.type & 0x1002) == 0x1002) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1207, "_umount_after_root_inactive",
				"%s: mp[%p], drive[%p]\n", "_umount_after_root_inactive", mount, current);
			mount->data = NULL;
			error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&current->fastmutex);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
					"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_umount_after_root_inactive", error);
			_free_all_cluster_cache(current);
			_cleanup_after_umount(current);
		}
	} else {
		current = mount->data;
		if (current) {
			mount->data = NULL;
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1236, "_umount_after_all_close",
				"%s: mp[%p], drive[%p]\n", "_umount_after_all_close", mount, current);
			error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&current->fastmutex);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
					"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_umount_check", error);
			_free_all_cluster_cache(current);
			current->state = 0;
			_cleanup_after_umount(current);
		}
	}
	if (drive) {
		current = mount->data;
		if (current) {
			if (current == drive) {
unlock:
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_umount_check", error);
			} else {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1352, "_umount_check",
					"%s: mp->pData[%p] != expected[%p]\n", "_umount_check", current, drive);
			}
		}
	}
	if (!result)
		vnode->core.node_data = NULL;
	return result;
}

extern int sceExfatfsVopDclose(unsigned int fd);

int exfatfs_vop_dclose(SceVopDcloseArgs *argp)
{
	SceVfsVnode *vnode = argp->vp;
	SceVfsFile *file = argp->file;
	SceVfsMount *mount = vnode->core.mnt;
	SceExfatfsDrive *drive, *current;
	int result, error;

	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vop_dclose");
	SCE_DBG_PRINTF_LEVEL(2, 15, 2861, "called DCLOSE vp = %p\n", vnode);
	if (vnode != file->vp)
		SCE_DBG_PRINTF_LEVEL(1, 0, 2863, "%s: vp[%p] != file->vnode[%p]\n",
			"exfatfs_vop_dclose", vnode, file->vp);
	if (result != (int)0x80010013 && result < 0)
		return result;
	result = sceExfatfsVopDclose(file->fd);
	drive = mount->data;
	if (drive) {
		if (drive->state & 0x4000) {
			if (!_used_resource_count(mount)) {
				current = mount->data;
				if (!current)
					goto closed;
				mount->data = NULL;
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1236, "_umount_after_all_close",
					"%s: mp[%p], drive[%p]\n", "_umount_after_all_close", mount, current);
				error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&current->fastmutex);
				if (error < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
						"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_force_umount_check", error);
				_free_all_cluster_cache(current);
				current->state = 0;
				_cleanup_after_umount(current);
			}
			current = mount->data;
			if (!current)
				goto closed;
			if (drive != current) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1307, "_force_umount_check",
					"%s: mp->pData[%p] != expected[%p]\n", "exfatfs_vop_dclose", current, drive);
				goto closed;
			}
		}
		error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
		if (error < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
				"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_dclose", error);
	}
closed:
	if (!result)
		file->fd = 0;
	return result;
}

extern int sceExfatfsVopUnlink(SceExfatfsFnode *from, SceExfatfsFnode *to);

int exfatfs_vop_unlink(SceVopUnlinkArgs *argp)
{
	SceVfsVnode *from = argp->fvp;
	SceVfsVnode *to = argp->tvp;
	SceVfsMount *mount = from->core.mnt;
	SceExfatfsDrive *drive, *current;
	int result, error;

	result = lockDriveInformationAllowUnmount(mount, 1, "exfatfs_vop_unlink");
	if (result != (int)0x80010013 && result < 0)
		return result;
	result = sceExfatfsVopUnlink(from->core.node_data, to->core.node_data);
	drive = mount->data;
	if (!drive)
		return result;
	if (drive->state & 0x4000) {
		if (!_used_resource_count(mount)) {
			current = mount->data;
			if (!current)
				return result;
			mount->data = NULL;
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 0, 1236, "_umount_after_all_close",
				"%s: mp[%p], drive[%p]\n", "_umount_after_all_close", mount, current);
			error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&current->fastmutex);
			if (error < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
					"%s: sceKernelUnlockFastMutex failed[%#x]\n", "_force_umount_check", error);
			_free_all_cluster_cache(current);
			current->state = 0;
			_cleanup_after_umount(current);
		}
		current = mount->data;
		if (!current)
			return result;
		if (drive != current) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 1307, "_force_umount_check",
				"%s: mp->pData[%p] != expected[%p]\n", "exfatfs_vop_unlink", current, drive);
			return result;
		}
	}
	error = ksceKernelUnlockFastMutex((SceKernelFastMutex *)&drive->fastmutex);
	if (error < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 811, "_unlock_drive_information_core",
			"%s: sceKernelUnlockFastMutex failed[%#x]\n", "exfatfs_vop_unlink", error);
	return result;
}

/* These notifications are empty in both TOOL originals. */
void _exfatfs_notify_invalid_boot_signature(SceExfatfsDrive *drive)
{
}

void _exfatfs_notify_invalid_sector_shift(SceExfatfsDrive *drive, SceUInt32 sectorSize)
{
}

void _exfatfs_notify_unsupported_sector_size(SceExfatfsDrive *drive)
{
}

void _exfatfs_notify_broken_cluster_chain(SceExfatfsDrive *drive,
	SceUInt32 cluster, SceUInt32 next)
{
	SceVfsMountData *mountData = drive->deviceInfo.mount_misc;
	IoErrorEventInfo info;
	int result;

	info.mntId = mountData->mnt_id;
	info.error = 0x808d0007;
	info.reserved0[0] = 0;
	info.reserved0[1] = 0;
	info.detail = -1;
	info.reserved1[0] = 0;
	info.reserved1[1] = 0;
	SCE_DBG_PRINTF_LEVEL(1, 0, 280,
		"%s: broken cluster chain found, cluster[%#x]->[%#x], please check whole (%s), mntId[%#x], error[%#x]\n",
		"_exfatfs_notify_broken_cluster_chain", cluster, next, mountData->assign_name,
		info.mntId, 0x808d0007);
	result = sceIoSetErrorEvent(mountData->mnt_id, 1, &info);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 283, "%s: sceIoSetErrorEvent failed[%#x]\n",
			"_exfatfs_notify_broken_cluster_chain", result);
}

void _exfatfs_notify_invalid_directory_entry(SceExfatfsDrive *drive,
	const unsigned char *entry)
{
	SceVfsMountData *mountData = drive->deviceInfo.mount_misc;
	IoErrorEventInfo info;
	int result;

	info.mntId = mountData->mnt_id;
	info.error = 0x808d0008;
	info.reserved0[0] = 0;
	info.reserved0[1] = 0;
	info.detail = -1;
	info.reserved1[0] = 0;
	info.reserved1[1] = 0;
	SCE_DBG_PRINTF_LEVEL(1, 0, 305,
		"%s: invalid directory entry found, first byte[%#x], assignName(%s), mntId[%#x], error[%#x]\n",
		"_exfatfs_notify_invalid_directory_entry", *entry, mountData->assign_name,
		info.mntId, 0x808d0008);
	result = sceIoSetErrorEvent(mountData->mnt_id, 1, &info);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 0, 308, "%s: sceIoSetErrorEvent failed[%#x]\n",
			"_exfatfs_notify_invalid_directory_entry", result);
}

void _exfatfs_notify_directory_loop(SceExfatfsDrive *drive, SceUInt32 startCluster,
	SceOff offset, unsigned char attributes, SceOff fileSize)
{
	SceVfsMountData *mountData = drive->deviceInfo.mount_misc;
	IoErrorEventInfo info;
	int result;

	info.mntId = mountData->mnt_id;
	info.error = 0x808d000b;
	info.reserved0[0] = 0;
	info.reserved0[1] = 0;
	info.detail = -1;
	info.reserved1[0] = 0;
	info.reserved1[1] = 0;
	if (!((startCluster == 0) & (fileSize == 0)) || (attributes & 0x10)) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 329,
			"%s: directory loop found, start_cluster[%#x], offset[%#llx], assignName(%s), mntId[%#x], error[%#x]\n",
			"_exfatfs_notify_directory_loop", startCluster, offset, mountData->assign_name,
			info.mntId, 0x808d000b);
		result = sceIoSetErrorEvent(mountData->mnt_id, 1, &info);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(1, 0, 332, "%s: sceIoSetErrorEvent failed[%#x]\n",
				"_exfatfs_notify_directory_loop", result);
	}
}

static const SceVopTable g_exfatfsVopTable = {
	.vop_open = exfatfs_vop_open,
	.vop_create = _exfatfs_create,
	.vop_close = exfatfs_vop_close,
	.vop_lookup = exfatfs_vop_lookup,
	.vop_read = exfatfs_vop_read,
	.vop_write = exfatfs_vop_write,
	.vop_lseek = exfatfs_vop_lseek,
	.vop_ioctl = exfatfs_vop_ioctl,
	.vop_remove = exfatfs_vop_remove,
	.vop_mkdir = _exfatfs_mkdir,
	.vop_rmdir = exfatfs_vop_rmdir,
	.vop_dopen = exfatfs_vop_dopen,
	.vop_dclose = exfatfs_vop_dclose,
	.vop_dread = exfatfs_vop_dread,
	.vop_getstat = exfatfs_vop_getstat,
	.vop_chstat = exfatfs_vop_chstat,
	.vop_rename = exfatfs_vop_rename,
	.reserved = NULL,
	.vop_pread = exfatfs_vop_pread,
	.vop_pwrite = exfatfs_vop_pwrite,
	.vop_inactive = exfatfs_vop_inactive,
	.vop_link = exfatfs_vop_link,
	.vop_unlink = exfatfs_vop_unlink,
	.vop_sync = exfatfs_vop_sync,
	.vop_fgetstat = exfatfs_vop_fgetstat,
	.vop_fchstat = exfatfs_vop_fchstat,
	/* SDK table types predate the canonical extended argument records. */
	.vop_whiteout = (int (*)(SceVopWhiteoutArgs_vitasdk *))exfatfs_vop_whiteout,
	.vop_cleanup = (int (*)(SceVopCleanupArgs_vitasdk *))exfatfs_vop_cleanup,
	.vop_verofill = NULL,
};

static const SceVfsOpTable g_exfatfsVfsOpTable = {
	.vfs_mount = exfatfs_vfs_mount,
	.vfs_umount = exfatfs_vfs_umount,
	.vfs_set_root = exfatfs_vfs_set_root,
	.vfs_get_root = exfatfs_vfs_get_root,
	.reserved = { NULL, NULL },
	.vfs_sync = exfatfs_vfs_sync,
	.reserved2 = NULL,
	.vfs_init = exfatfs_vfs_init,
	.vfs_fini = exfatfs_vfs_fini,
	.reserved3 = NULL,
	.vfs_devctl = exfatfs_vfs_devctl,
	.vfs_decode_path_elem = exfatfs_vfs_decode_path_elem,
};

int _register_exfatfs_vops(SceVfsInfo *info)
{
	int result;

	info->vfs_ops = &g_exfatfsVfsOpTable;
	info->default_vops = &g_exfatfsVopTable;
	result = ksceVfsAddVfs(info);
	if (result == (int)0x80010011) {
		result = 0;
		SCE_DBG_PRINTF_LEVEL(1, 0, 716, "%s: already registered, vfsName(%s), type(%d)\n",
			"_register_exfatfs_vops", info->vfs_name, info->type);
	} else if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 719, "%s: sceVfsAddVfs failed[%#x]\n",
			"_register_exfatfs_vops", result);
	}
	return result;
}

