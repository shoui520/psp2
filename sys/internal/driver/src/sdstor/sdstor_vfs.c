/* SceSdstor VFS operations (Sony source: sdstor_vfs.c). */
#include <sysmem/sysroot.h>
#include "sdstor_internal.h"
/* VFS carries the SDK's opaque mutex declarations; use our complete owner. */
#define ksceKernelInitializeFastMutex ksceKernelInitializeFastMutex_vitasdk
#define ksceKernelFinalizeFastMutex ksceKernelFinalizeFastMutex_vitasdk
#define ksceKernelLockFastMutex ksceKernelLockFastMutex_vitasdk
#define ksceKernelUnlockFastMutex ksceKernelUnlockFastMutex_vitasdk
#include <iofilemgr/vfs.h>
#undef ksceKernelInitializeFastMutex
#undef ksceKernelFinalizeFastMutex
#undef ksceKernelLockFastMutex
#undef ksceKernelUnlockFastMutex
#include <sysmem/debug.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <psp2kern/kernel/sysroot.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <string.h>

typedef struct SceSdstorVnodeData {
	SceSdstorPartitionEntry *pentry;
	SceSdstorSddev *sddev;
	uint32_t reserved;
} SceSdstorVnodeData;

_Static_assert(sizeof(SceSdstorVnodeData) == 12, "SceSdstor vnode data size");

typedef struct SceSdstorEraseRequest {
	SceUID callback;
	char name[20];
} SceSdstorEraseRequest;

_Static_assert(sizeof(SceSdstorEraseRequest) == 24, "SceSdstor erase request size");

static const SceSdstorPartitionAccess sdstorDevctlAccess = {
	.flags = 0xfff, .mode = 0
};

int dev_sdstor_vfs_sync(SceVopSyncArgs *args);
int dev_sdstor_vfs_inactive(SceVopInactiveArgs *args);
int dev_sdstor_vfs_init(SceVfsOpInitArgs *args);
int dev_sdstor_vfs_set_root(SceVfsOpSetRootArgs *args);
int dev_sdstor_vfs_umount(SceVfsOpUmountArgs *args);
int dev_sdstor_vfs_mount(SceVfsOpMountArgs *args);
SceSSize dev_sdstor_vfs_pwrite(SceVopPwriteArgs *args);
SceOff dev_sdstor_vfs_lseek(SceVopLseekArgs *args);
SceSSize dev_sdstor_vfs_write(SceVopWriteArgs *args);
SceSSize dev_sdstor_vfs_pread(SceVopPreadArgs *args);
SceSSize dev_sdstor_vfs_read(SceVopReadArgs *args);
int dev_sdstor_vfs_remove(SceVopRemoveArgs *args);
int dev_sdstor_vfs_lookup(SceVopLookupArgs *args);
int dev_sdstor_vfs_close(SceVopCloseArgs *args);
int dev_sdstor_vfs_open(SceVopOpenArgs *args);
int dev_sdstor_vfs_decode_path_elem(SceVfsOpDecodePathElemArgs *args);
int dev_sdstor_vfs_devctl(SceVfsOpDevctlArg *args);
int sdstor_read(SceSdstorFd *handle, int64_t offset, void *buffer, uint32_t size);
int sdstor_write(SceSdstorFd *handle, int64_t offset, const void *buffer,
		 uint32_t size);
SceSdstorPartitionEntry *sdpart_get_object(const char *name, int length);
int sdstor_destruct(SceSdstorFd *handle);
int sdstor_construct(SceSdstorPartitionEntry *partition, int flags,
		     SceSdstorFd **resultHandle);
int sddev_is_read_write(SceSdstorSddev *device);
int sddev_format_rm(SceSdstorSddev *device);
int sddev_erase_sd(SceSdstorSddev *device, SceUID callback);
int sddev_has_write_protect_area(SceSdstorSddev *device, uint32_t sector,
				 uint32_t count, uint8_t *writeProtected);
unsigned int sdpart_get_mc_port(void);
int sdpart_write_sd_mbr(unsigned int port);
int sdctrl_remove(unsigned int port);
int sdctrl_insert(unsigned int port);
int sdctrl_disable_mount(unsigned int port);
int sdctrl_invalidate_partition(unsigned int port);
SceUID kscePUIDtoGUID(SceUID pid, SceUID puid);
extern uint32_t sdstorOpenCount[5];

#if FW_DEBUG
static const uint32_t sdstorVnodeMountOptions[5] = {
	0x100, 0x201, 0x202, 0x203, 0x202
};
#endif

#if !FW_DEBUG
static const SceVfsOpTable sdstorVfsOps = {
	.vfs_mount = dev_sdstor_vfs_mount,
	.vfs_umount = dev_sdstor_vfs_umount,
	.vfs_set_root = dev_sdstor_vfs_set_root,
	.vfs_init = dev_sdstor_vfs_init,
	.vfs_devctl = dev_sdstor_vfs_devctl,
	.vfs_decode_path_elem = dev_sdstor_vfs_decode_path_elem
};
static const uint32_t sdstorVnodeMountOptions[5] = {
	0x100, 0x201, 0x202, 0x203, 0x202
};
#endif

static const SceVopTable sdstorVnodeOps = {
	.vop_open = dev_sdstor_vfs_open,
	.vop_close = dev_sdstor_vfs_close,
	.vop_lookup = dev_sdstor_vfs_lookup,
	.vop_read = dev_sdstor_vfs_read,
	.vop_write = dev_sdstor_vfs_write,
	.vop_lseek = dev_sdstor_vfs_lseek,
	.vop_remove = dev_sdstor_vfs_remove,
	.vop_pread = dev_sdstor_vfs_pread,
	.vop_pwrite = dev_sdstor_vfs_pwrite,
	.vop_inactive = dev_sdstor_vfs_inactive,
	.vop_sync = dev_sdstor_vfs_sync
};

#if FW_DEBUG
static const SceVfsOpTable sdstorVfsOps = {
	.vfs_mount = dev_sdstor_vfs_mount,
	.vfs_umount = dev_sdstor_vfs_umount,
	.vfs_set_root = dev_sdstor_vfs_set_root,
	.vfs_init = dev_sdstor_vfs_init,
	.vfs_devctl = dev_sdstor_vfs_devctl,
	.vfs_decode_path_elem = dev_sdstor_vfs_decode_path_elem
};
#endif

static SceUID sdstorVfsHeapId;
static SceVfsInfo sdstorVfsInfo;

int dev_sdstor_vfs_sync(SceVopSyncArgs *args)
{
	return 0;
}

int dev_sdstor_vfs_inactive(SceVopInactiveArgs *args)
{
	return 0;
}

int dev_sdstor_vfs_init(SceVfsOpInitArgs *args)
{
	SCE_DBG_PRINTF_LEVEL(2, 15, 165, "called INIT\n");
	return 0;
}

int dev_sdstor_vfs_set_root(SceVfsOpSetRootArgs *args)
{
	SceVfsVnode *vnode = args->vp;
	SceVfsMount *mount = args->mnt;

	SCE_DBG_PRINTF_LEVEL(2, 15, 208, "called SET ROOT vp=%p %p\n",
			     vnode, mount);
	vnode->core.mnt = mount;
	vnode->core.state = SCE_VNODE_STATE_ACTIVE;
	vnode->core.type = SCE_VNODE_TYPE_ROOTDIR_DEVFS;
	vnode->core.dd = NULL;
	vnode->core.size = 0;
	vnode->core.acl_data[0] = 0;
	vnode->core.acl_data[1] = 0;
	return 0;
}

int dev_sdstor_vfs_umount(SceVfsOpUmountArgs *args)
{
	SCE_DBG_PRINTF_LEVEL(2, 15, 196, "called UMOUNT %p flags = 0x%08x\n",
			     args->mnt, args->flags);
	return 0x80010010;
}

int dev_sdstor_vfs_mount(SceVfsOpMountArgs *args)
{
	SceVfsMount *mount = args->mnt;

	SCE_DBG_PRINTF_LEVEL(2, 15, 176, "called MOUNT set %p\n", mount);
	mount->default_io_cache_size = 512;
	mount->available_entry_num = 20;
	mount->allocator = sdstorVfsHeapId;
	return 0;
}

SceSSize dev_sdstor_vfs_pwrite(SceVopPwriteArgs *args)
{
	SceVfsFile *file = args->file;
	SceVfsVnode *vnode = args->vp;
	const void *buffer = args->buf;
	uint32_t size = args->nbyte;
	int64_t offset = args->offset;
	SceSdstorFd *handle = (SceSdstorFd *)(uintptr_t)file->fd;
	uint32_t flags;
	int result, unlockResult;

	SCE_DBG_PRINTF_LEVEL(2, 15, 473,
		"SDSTOR PWRITE vp=%p size=0x%08x offset=0x%llx buf=%p\n",
		vnode, size, offset, buffer);
	if (!vnode->core.node_data)
		return 0x80010013;
	result = ksceKernelLockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	flags = handle->flags;
	handle->flags = file->flags;
	result = sdstor_write(handle, offset, buffer, size);
	handle->flags = flags;
	unlockResult = ksceKernelUnlockFastMutex(&handle->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

SceOff dev_sdstor_vfs_lseek(SceVopLseekArgs *args)
{
	SceVfsFile *file = args->file;
	SceOff offset = args->offset;
	SceSdstorFd *handle = (SceSdstorFd *)(uintptr_t)file->fd;
	int whence = args->whence;
	uint32_t blockSize, sectorCount;
	SceOff capacity, position;
	int result;

	if (!args->vp->core.node_data)
		return (int)0x80010013;
	result = ksceKernelLockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	blockSize = handle->blockSize;
	sectorCount = handle->n_sectors;
	if (offset & (blockSize - 1)) {
		result = ksceKernelUnlockFastMutex(&handle->fastmutex);
		return result < 0 ? result : (int)0x80010022;
	}
	capacity = (uint64_t)blockSize * sectorCount;
	if (whence == 1) {
		position = (SceOff)((uint64_t)file->position + (uint64_t)offset);
	} else if (whence == 2) {
		position = (SceOff)((uint64_t)blockSize * sectorCount +
				    (uint64_t)offset);
	} else if (whence == 0) {
		position = offset;
	} else {
		position = (int)0x80010016;
		goto unlock;
	}
	if (position >= 0) {
		if (capacity < position)
			position = (int)0x80010021;
		else
			file->position = position;
	}
unlock:
	result = ksceKernelUnlockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	SCE_DBG_PRINTF_LEVEL(2, 15, 551,
		"called LSEEK set %d offset=0x%llx newpos=0x%llx\n",
		whence, offset, position);
	return position;
}

SceSSize dev_sdstor_vfs_write(SceVopWriteArgs *args)
{
	SceVfsFile *file = args->file;
	SceVfsVnode *vnode = args->vp;
	const void *buffer = args->buf;
	uint32_t size = args->nbyte;
	SceSdstorFd *handle = (SceSdstorFd *)(uintptr_t)file->fd;
	uint32_t flags;
	int result, unlockResult;

	SCE_DBG_PRINTF_LEVEL(2, 15, 401,
		"SDSTOR WRITE vp=%p size=0x%08x buf=%p fpos=0x%llx\n",
		vnode, size, buffer, file->position);
	if (!vnode->core.node_data)
		return 0x80010013;
	result = ksceKernelLockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	flags = handle->flags;
	handle->flags = file->flags;
	result = sdstor_write(handle, file->position, buffer, size);
	if (result > 0)
		file->position += result;
	handle->flags = flags;
	unlockResult = ksceKernelUnlockFastMutex(&handle->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

SceSSize dev_sdstor_vfs_pread(SceVopPreadArgs *args)
{
	SceVfsFile *file = args->file;
	SceVfsVnode *vnode = args->vp;
	void *buffer = args->buf;
	uint32_t size = args->nbyte;
	int64_t offset = args->offset;
	SceSdstorFd *handle = (SceSdstorFd *)(uintptr_t)file->fd;
	uint32_t flags;
	int result, unlockResult;

	SCE_DBG_PRINTF_LEVEL(2, 15, 439,
		"called PREAD vp=%p size=0x%08x offset=0x%llx buf=%p\n",
		vnode, size, offset, buffer);
	if (!vnode->core.node_data)
		return 0x80010013;
	result = ksceKernelLockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	flags = handle->flags;
	handle->flags = file->flags;
	result = sdstor_read(handle, offset, buffer, size);
	handle->flags = flags;
	unlockResult = ksceKernelUnlockFastMutex(&handle->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

SceSSize dev_sdstor_vfs_read(SceVopReadArgs *args)
{
	SceVfsFile *file = args->file;
	SceVfsVnode *vnode = args->vp;
	void *buffer = args->buf;
	uint32_t size = args->nbyte;
	SceSdstorFd *handle = (SceSdstorFd *)(uintptr_t)file->fd;
	uint32_t flags;
	int result, unlockResult;

	SCE_DBG_PRINTF_LEVEL(2, 15, 365,
		"called READ vp=%p size=0x%08x buf=%p fpos=0x%llx\n",
		vnode, size, buffer, file->position);
	if (!vnode->core.node_data)
		return 0x80010013;
	result = ksceKernelLockFastMutex(&handle->fastmutex);
	if (result < 0)
		return result;
	flags = handle->flags;
	handle->flags = file->flags;
	result = sdstor_read(handle, file->position, buffer, size);
	if (result > 0)
		file->position += result;
	handle->flags = flags;
	unlockResult = ksceKernelUnlockFastMutex(&handle->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

int dev_sdstor_vfs_remove(SceVopRemoveArgs *args)
{
	SceVfsVnode *vnode = args->vp;
	SceUID heap = args->dvp->core.allocator;
	SceSdstorVnodeData *data = vnode->core.node_data;
	SceSdstorPartitionEntry *entry;
	SceSdstorSdpart *table;
	int result;

	SCE_DBG_PRINTF_LEVEL(2, 15, 677, "elem: %s, fullPath: %s\n",
			     args->path->name, args->path->path);
	if (!data)
		return 0x80010013;
	entry = data->pentry;
	table = entry->sdpart;
	if (table) {
		result = ksceKernelLockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
		if (table->stat != 2)
			memset(entry, 0, sizeof(*entry));
		result = ksceKernelUnlockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
	}
	result = ksceKernelFreeHeapMemory(heap, vnode->core.node_data);
	if (result != 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 705, "0x%08x\n", result);
	vnode->core.node_data = NULL;
	return result;
}

int dev_sdstor_vfs_lookup(SceVopLookupArgs *args)
{
	SceVfsVnode *parent = args->dvp;
	const char *name = args->path->name;
	unsigned int length = args->path->name_length;
	SceUID heap = parent->core.allocator;
	SceVfsVnode **resultVnode = args->vpp;
	SceSdstorVnodeData *data;
	SceSdstorPartitionEntry *entry;
	SceSdstorSdpart *table;
	SceVfsVnode *vnode;
	uint32_t blockSize, sectorCount;
	unsigned int port;
	int result, freeResult;

	SCE_DBG_PRINTF_LEVEL(2, 15, 577, "called LOOCK UP dvp=%p %s\n",
			     parent, name);
	data = ksceKernelAllocHeapMemory(heap, sizeof(*data));
	if (!data)
		return 0x8001000c;
	__builtin_memset(data, 0, sizeof(*data));
	data->pentry = sdpart_get_object(name, length);
	if (!data->pentry) {
		result = ksceKernelFreeHeapMemory(heap, data);
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 591, "0x%08x\n", result);
			return result;
		}
		return 0x80010013;
	}
	table = data->pentry->sdpart;
	if (!table) {
		result = ksceKernelFreeHeapMemory(heap, data);
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 601, "0x%08x\n", result);
			return result;
		}
		return 0x80010013;
	}
	result = ksceKernelLockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	if (table->stat == 2) {
		result = ksceKernelUnlockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
		result = ksceKernelFreeHeapMemory(heap, data);
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 613, "0x%08x\n", result);
			return result;
		}
		return 0x80010013;
	}
	blockSize = table->blockSize;
	data->sddev = table->sddev;
	sectorCount = data->pentry->n_sectors;
	result = ksceKernelUnlockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	port = sddev_get_port(data->sddev);
	SCE_DBG_ASSERT(port <= 4, 626);
	result = vfsGetNewVnode(parent->core.mnt,
		(SceVopTable *)parent->core.mnt->mnt_vfs_inf->default_vops, 0, &vnode);
	if (result != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 652, "err ret = 0x%08x\n", result);
		freeResult = ksceKernelFreeHeapMemory(heap, data);
		if (freeResult != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 656, "0x%08x\n", freeResult);
			return freeResult;
		}
		return result;
	}
	vfsLockVnode(vnode);
	SCE_DBG_PRINTF_LEVEL(2, 15, 635, "new vp =%p\n", vnode);
	entry = data->pentry;
	vnode->core.mnt = parent->core.mnt;
	vnode->core.mnt_opt = sdstorVnodeMountOptions[port];
	vnode->core.state = SCE_VNODE_STATE_ACTIVE;
	vnode->core.type = SCE_VNODE_TYPE_DEV;
	vnode->core.dd = parent;
	vnode->core.size = (uint64_t)sectorCount * blockSize;
	vnode->core.node_data = data;
	vnode->core.acl_data[0] = 0;
	vnode->core.acl_data[1] = 0;
	__builtin_memcpy(vnode->core.acl_data, &entry->flags,
			 sizeof(SceSdstorPartitionAccess));
	*resultVnode = vnode;
	return 0;
}

int dev_sdstor_vfs_close(SceVopCloseArgs *args)
{
	return sdstor_destruct((SceSdstorFd *)(uintptr_t)args->file->fd);
}

int dev_sdstor_vfs_open(SceVopOpenArgs *args)
{
	SceSdstorVnodeData *data = args->vp->core.node_data;
	int flags = args->flags;
	SceVfsFile *file = args->file;
	SceSdstorFd *handle = NULL;
	SceSdstorPartitionEntry *entry;
	SceSdstorSdpart *table;
	SceSdstorPartitionAccess access;
	SceSblACMgrValue attr;
	int result;

	SCE_DBG_PRINTF_LEVEL(2, 15, 284, "called OPEN vp=%p %s\n",
			     args->vp, args->path->name);
	if (!data)
		return 0x80010013;
	entry = data->pentry;
	result = sddev_is_read_write(data->sddev);
	if (result != 0 && result != (int)0x8001000d)
		return result;
	if (!(flags & 3) || ((flags & 2) && result != 0))
		return 0x8001000d;
	table = data->pentry->sdpart;
	if (!table)
		return 0x80010013;
	result = ksceKernelLockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	if (table->stat == 2) {
		result = ksceKernelUnlockFastMutex(&table->fastmutex);
		return result < 0 ? result : (int)0x80010013;
	}
	access.flags = entry->flags;
	access.mode = entry->mode;
	result = ksceKernelUnlockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	attr.type = 8;
	attr.value = (unsigned char *)&access;
	attr.size = sizeof(access);
	result = SceSblACMgrForDriver_BE5667C5(0, &attr, flags & 2 ? 3 : 1);
	if (result < 0)
		return result;
	result = sdstor_construct(entry, flags, &handle);
	if (result == 0)
		file->fd = (uint32_t)(uintptr_t)handle;
	return result;
}

int dev_sdstor_vfs_decode_path_elem(SceVfsOpDecodePathElemArgs *args)
{
	const char *path;
	SceSdstorPartitionEntry *entry;
	SceSdstorSdpart *table;
	unsigned int bufferSize;
	int length, result;

	SCE_DBG_PRINTF_LEVEL(2, 15, 226, "path = %s, bufsz = %d\n",
			     args->path, args->buf_len);
	path = args->path;
	length = strnlen(path, 20);
	bufferSize = args->buf_len;
	if (bufferSize <= 3)
		goto copyOriginal;
	entry = sdpart_get_object(path, length);
	if (!entry || !(table = entry->sdpart))
		goto unchanged;
	result = ksceKernelLockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	if (table->stat == 2) {
		result = ksceKernelUnlockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
		goto unchanged;
	}
	strncpy(args->buf, entry->numeric_name, 3);
	result = ksceKernelUnlockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	args->buf[3] = 0;
	*args->decode_len = 3;
	return 0;
unchanged:
	path = args->path;
	bufferSize = args->buf_len;
copyOriginal:
	strncpy(args->buf, path, bufferSize);
	*args->decode_len = length;
	return 0;
}

int dev_sdstor_vfs_devctl(SceVfsOpDevctlArg *args)
{
	unsigned int command, argSize, bufferSize, mask;
	const void *arg;
	void *buffer;
	const SceSdstorEraseRequest *erase;
	SceSdstorPartitionInfo *info;
	SceSdstorPartitionEntry *entry;
	SceSdstorSdpart *table;
	SceSdstorSddev *device;
	SceSdstorSdinst sdinst;
	SceSdstorPartitionAccess access;
	SceSblACMgrValue attr;
	SceUID callback;
	int result, unlockResult, length;

	SCE_DBG_ASSERT(args != NULL, 715);
	command = args->cmd;
	arg = args->arg;
	argSize = args->arg_len;
	buffer = args->buf;
	bufferSize = args->buf_len;
	SCE_DBG_PRINTF_LEVEL(2, 15, 730,
		"cmd = %d, arg = %p, arglen = %d, buf = %p, buflen = %d\n",
		command, arg, argSize, buffer, bufferSize);
	if (command == 6) {
		erase = arg;
		attr.type = 8;
		attr.value = (unsigned char *)&access;
		attr.size = sizeof(access);
		length = strnlen(erase->name, 20);
		result = sdpart_decode_name(erase->name, length, &sdinst);
		if (result != 0)
			return result;
		if (sdinst.id != 15)
			SCE_DBG_PRINTF_LEVEL(1, 15, 959, "only entire allowed %d\n",
					     sdinst.id);
		if (sdinst.port == 255) {
			sdinst.port = sdpart_get_mc_port();
			SCE_DBG_PRINTF_LEVEL(2, 15, 967, "logical mc rerouted to %d\n",
					     sdinst.port);
		}
		if (sdinst.port != 1 && sdinst.port != 4) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 972, "port not MC= %d\n", sdinst.port);
			return 0x80010016;
		}
		if (sdinst.port == 1) {
			if (SceSysrootForDriver_ksceSysrootIsManufacturingMode())
				goto eraseAccess;
			if (!SceSysrootForDriver_F804F761()) {
				SCE_DBG_PRINTF_LEVEL(1, 15, 979,
					"SD in GC slot not allowed at this mode\n");
				return 0x80010016;
			}
		}
		if (sdinst.port == 4 && !ksceKernelSysrootCheckModelCapability(11)) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 986,
				"SD in SD slot not supported in this machine\n");
			return 0x80010016;
		}
eraseAccess:
		access = sdstorDevctlAccess;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0)
			return result;
		device = sddev_get_object(sdinst.port);
		if (!device)
			return 0x80010013;
		if (sdstorOpenCount[sdinst.port]) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 1008, "port %d is in use\n", sdinst.port);
			return 0x80010010;
		}
		callback = erase->callback;
		if (callback != -1) {
			callback = kscePUIDtoGUID(0, callback);
			if (erase->callback != -1 && callback < 0)
				return callback;
		}
		return sddev_erase_sd(device, callback);
	}
	if (argSize > 20)
		return 0x80010013;
	length = strnlen(arg, argSize);
	attr.type = 8;
	attr.value = (unsigned char *)&access;
	attr.size = sizeof(access);
	switch (command) {
	case 1:
		entry = sdpart_get_object(arg, length);
		if (!entry || !(table = entry->sdpart))
			return 0x80010013;
		if (!buffer || bufferSize != sizeof(*info))
			return 0x80010016;
		result = ksceKernelLockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
		if (table->stat == 2) {
			result = ksceKernelUnlockFastMutex(&table->fastmutex);
			return result < 0 ? result : (int)0x80010013;
		}
		access.flags = entry->flags;
		access.mode = entry->mode;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0) {
			unlockResult = ksceKernelUnlockFastMutex(&table->fastmutex);
			return unlockResult < 0 ? unlockResult : result;
		}
		info = buffer;
		info->startSector = entry->start_lba;
		info->sectorCount = entry->n_sectors;
		info->eraseBlockCount = table->mediaInfo.eraseBlockCount;
		sddev_has_write_protect_area(table->sddev, entry->start_lba,
					   entry->n_sectors, &info->writeProtected);
		info->reserved[0] = 0;
		info->reserved[1] = 0;
		result = ksceKernelUnlockFastMutex(&table->fastmutex);
		if (result < 0)
			return result;
		info->memoryCardPort = sdpart_get_mc_port();
		if (info->memoryCardPort == 255)
			info->memoryCardPort = sdpart_get_mc_port();
		return 0;
	case 2:
		result = sdpart_decode_name(arg, length, &sdinst);
		if (result != 0)
			return result;
		if (sdinst.port == 255) {
			sdinst.port = sdpart_get_mc_port();
			SCE_DBG_PRINTF_LEVEL(2, 15, 820, "logical mc specified %d\n", sdinst.port);
		}
		if (sdinst.port != 2)
			return 0x80010016;
		access = sdstorDevctlAccess;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0)
			return result;
		device = sddev_get_object(2);
		return device ? sddev_format_rm(device) : (int)0x80010013;
	case 3:
		result = sdpart_decode_name(arg, length, &sdinst);
		if (result != 0)
			return result;
		if (sdinst.port != 4 && sdinst.port != 2) {
			if (sdinst.port == 1)
				mask = 2;
			else if (sdinst.port == 255)
				mask = 22;
			else {
				SCE_DBG_PRINTF_LEVEL(2, 15, 857, "port not MC= %d\n", sdinst.port);
				return 0x80010016;
			}
		} else {
			switch (sdinst.port) {
			case 2:
				mask = 4;
				break;
			case 4:
				mask = 16;
				break;
			case 1:
				mask = 2;
				break;
			case 255:
				mask = 22;
				break;
			default:
				SCE_DBG_PRINTF_LEVEL(2, 15, 872, "port not MC= %d\n", sdinst.port);
				return 0x80010016;
			}
		}
		access = sdstorDevctlAccess;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0)
			return result;
		if (mask & 4) {
			result = sdctrl_disable_mount(2);
			if (result < 0)
				return result;
		}
		if (mask & 16) {
			result = sdctrl_disable_mount(4);
			if (result < 0)
				return result;
		}
		if ((mask & 2) && SceSysrootForDriver_F804F761())
			result = sdctrl_disable_mount(1);
		return result;
	case 4:
	case 5:
		result = sdpart_decode_name(arg, length, &sdinst);
		if (result != 0)
			return result;
		if (sdinst.port == 255) {
			sdinst.port = sdpart_get_mc_port();
			SCE_DBG_PRINTF_LEVEL(2, 15, 915, "logical mc specified %d\n", sdinst.port);
		}
		if (sdinst.port != 1 && sdinst.port != 2 && sdinst.port != 4) {
			SCE_DBG_PRINTF_LEVEL(2, 15, 921, "port not MC= %d\n", sdinst.port);
			return 0x80010016;
		}
		access = sdstorDevctlAccess;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0)
			return result;
		return command == 5 ? sdctrl_insert(sdinst.port) : sdctrl_remove(sdinst.port);
	case 7:
	case 8:
		result = sdpart_decode_name(arg, length, &sdinst);
		if (result != 0)
			return result;
		if (sdinst.id != 15)
			SCE_DBG_PRINTF_LEVEL(1, 15, 1036, "only entire allowed %d\n", sdinst.id);
		if (sdinst.port == 255) {
			sdinst.port = sdpart_get_mc_port();
			SCE_DBG_PRINTF_LEVEL(2, 15, 1042, "logical mc rerouted to %d\n", sdinst.port);
		}
		if (sdinst.port != 1 && sdinst.port != 4) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 1047, "port not MC= %d\n", sdinst.port);
			return 0x80010016;
		}
		if (sdinst.port == 1) {
			if (SceSysrootForDriver_ksceSysrootIsManufacturingMode())
				goto invalidateAccess;
			if (!SceSysrootForDriver_F804F761()) {
				SCE_DBG_PRINTF_LEVEL(1, 15, 1054,
					"SD in GC slot not allowed at this mode\n");
				return 0x80010016;
			}
		}
		if (sdinst.port == 4 && !ksceKernelSysrootCheckModelCapability(11)) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 1061,
				"SD in SD slot not supported in this machine\n");
			return 0x80010016;
		}
invalidateAccess:
		access = sdstorDevctlAccess;
		result = SceSblACMgrForDriver_BE5667C5(0, &attr, 4);
		if (result < 0)
			return result;
		if (!sddev_get_object(sdinst.port))
			return 0x80010013;
		if (sdstorOpenCount[sdinst.port]) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 1084, "port %d is in use\n", sdinst.port);
			return 0x80010010;
		}
		if (command == 7) {
			result = sdpart_write_sd_mbr(sdinst.port);
			if (result < 0)
				return result;
		}
		return sdctrl_invalidate_partition(sdinst.port);
	default:
		SCE_DBG_PRINTF_LEVEL(2, 15, 1101, "cmd = %d\n", command);
		return 0x80010030;
	}
}

int sdstor_vfs_initialize(void)
{
	SceKernelHeapCreateOpt opt;
	int result, error;

	opt.size = sizeof(opt);
	opt.flags = HEAP_AUTO_EXTEND;
	sdstorVfsHeapId = ksceKernelCreateHeap("SceSdStorVfs", 0x2000, &opt);
	if (sdstorVfsHeapId < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 111, "0x%08x\n", sdstorVfsHeapId);
		return sdstorVfsHeapId;
	}
	sdstorVfsInfo.vfs_ops = &sdstorVfsOps;
	sdstorVfsInfo.vfs_name = "sdstor_dev_fs";
	sdstorVfsInfo.vfs_name_len = 14;
	sdstorVfsInfo.type = 16;
	sdstorVfsInfo.default_vops = &sdstorVnodeOps;
	sdstorVfsInfo.vfs_data = NULL;
	result = ksceVfsAddVfs(&sdstorVfsInfo);
	if (result) {
		error = ksceKernelDeleteHeap(sdstorVfsHeapId);
		SCE_DBG_PRINTF_LEVEL(1, 15, 125, "0x%08x\n", result);
		if (error) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 127, "0x%08x\n", error);
			return error;
		}
		return result;
	}
	result = ksceIoMount(1, NULL, 0, 0, 0, 0);
	if (result) {
		error = ksceKernelDeleteHeap(sdstorVfsHeapId);
		SCE_DBG_PRINTF_LEVEL(1, 15, 136, "0x%08x\n", result);
		if (error) {
			SCE_DBG_PRINTF_LEVEL(1, 15, 138, "0x%08x\n", error);
			return error;
		}
	}
	return result;
}
