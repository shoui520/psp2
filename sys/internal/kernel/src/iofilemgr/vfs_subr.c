/*
 * SceIofilemgr: VFS support routines (Sony's vfs_subr.c): the mount, vnode, file and
 * name cache objects, their locks and lists, and the per-process cleanup.
 */

#include "iofilemgr_internal.h"
#include <sce/mutex.h>
#include "../../../../../common/sysclib/atomic.h"

extern int vfsGetMutexInfo(SceUID uid, ThreadmgrMutexInfo *info)
	__asm__("ksceKernelGetMutexInfo");

SceClass g_vfsFileClass;
SceVopTable *g_vopTableCache[VFS_VOP_TABLE_CACHE_NUM];
char *g_pathBuffCache[VFS_PATH_BUFF_CACHE_NUM];
SceKernelFastMutex g_mntListMutex;
SceKernelFastMutex g_ncacheMutex;
SceKernelFastMutex g_vfsMutex;
SceVfsMount *g_mntList;
SceVfsMount *g_freeMntList;
SceKernelSpinlock g_freeMntListLock;
SceVfsInfo *g_vfsList;
struct Ncache *g_ncacheList;
SceKernelSpinlock g_pathBuffLock;
int g_total_vnode_num;
BufferPool g_vnodePool;
int g_vfsSuspended;
int g_vfsReclaimIndex;
int g_need_mnt_sync;
int g_vfsIoCount;
int g_vfsSuspendQueryCount;
int g_vfsInitialized;
struct FdLock g_vfsDummyFdLock;
struct Ncache *g_negativeNcacheList;
int g_negativeNcacheNum;
const char *g_vfsHostfsAssignName;
volatile int g_available_vnode_num = 0x400;
SceUID g_vfsDevWaitForMCEvf = -1;
SceUInt32 g_vfsDevWaitForMCState = 1;
SceUID g_vfsDevWaitForSDEvf = -1;
SceUInt32 g_vfsDevWaitForSDState = 1;
SceUID g_vfsDevWaitForVCEvf = -1;
SceUInt32 g_vfsDevWaitForVCState = 1;
SceUID g_vfsDevEvf = -1;
SceUInt32 g_vfsDevState = 1;
SceUID g_vfsDebugMemBlock[VFS_DEBUG_BUFF_POOL_NUM] __attribute__((section(".data"))) = {0};
BufferPool *g_vfsDebugBufferPool[VFS_DEBUG_BUFF_POOL_NUM] __attribute__((section(".data"))) = {0};

/* the vops a mount's vnodes get while its device is gone */
int vfs_fumount_vop_enodev(void *argp)
{
	return SCE_ERROR_ERRNO_ENODEV;
}

/* SceUIDVfsFileClass constructor and destructor */
int vfs_file_class_constructor(void *data)
{
	SceUIDVfsFileObject *obj = data;

	obj->file.vp = NULL;
	obj->file.debug_path = NULL;
	return 0;
}

int vfs_file_class_destructor(void *data)
{
	return 0;
}

/* name unknown: takes a mount from the free list and creates its fd lock */
SceVfsMount *vfs_alloc_mount(void)
{
	struct FdLock *fdlock;
	SceVfsMount *mnt;
	SceUID uid;
	int intr;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_freeMntListLock);
	mnt = g_freeMntList;
	if (mnt == NULL) {
		ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_freeMntListLock, intr);
		return NULL;
	}
	g_freeMntList = mnt->mnt_list_next;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_freeMntListLock, intr);
	fdlock = (struct FdLock *)mnt->opaque;
	mnt->fd_lock = fdlock;
	uid = ksceKernelCreateMutex("SceVfsFdLock", 0, 0, NULL);
	if (uid >= 0) {
		fdlock->mutex = uid;
		uid = ksceKernelCreateCond("SceVfsFdCond", 0, uid, NULL);
		if (uid >= 0) {
			fdlock->cond = uid;
			fdlock->state = 0;
			fdlock->waitNum = 0;
			mnt->fumount = (struct Fumount *)mnt->opaque2;
			return mnt;
		}
		ksceKernelDeleteMutex(fdlock->mutex);
		fdlock->mutex = -1;
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_freeMntListLock);
	if (g_freeMntList != NULL)
		mnt->mnt_list_next = g_freeMntList;
	g_freeMntList = mnt;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_freeMntListLock, intr);
	return NULL;
}

/* name unknown: initializes a mount taken by vfs_alloc_mount */
void vfs_init_mount(SceVfsMount *mnt, SceVfsVnode *vp, SceUID allocator, SceVfsInfo *vfsInf)
{
	struct Fumount *fumount;

	ksceKernelInitializeFastMutex(&mnt->fast_mutex, "SceVfsMnt", 2, 0);
	mnt->allocator = allocator;
	mnt->mnt_vfs_inf = vfsInf;
	mnt->mnt_ref_count = 1;
	mnt->vnode_list = NULL;
	mnt->vnode_num = 0;
	mnt->opened_entry_num = 0;
	mnt->available_entry_num = 0;
	mnt->pid = 0;
	mnt->mnted_on_list = NULL;
	mnt->mnted_on_list_prev = NULL;
	mnt->mnt_list_next = NULL;
	mnt->mnt_data = NULL;
	memset(mnt->path, 0, sizeof(mnt->path));
	mnt->mnt_vnode = vp;
	if (vp != NULL) {
		vp->core.type |= SCE_VNODE_TYPE_MOUNTED;
		vp->core.ref_count++;
	}
	fumount = mnt->fumount;
	fumount->devState = &g_vfsDevState;
	fumount->activationEvf = &g_vfsDevEvf;
	fumount->vops = NULL;
	fumount->busy = 0;
}

int vfs_create_mnt_list_mtx(void)
{
	int ret;

	ret = ksceKernelInitializeFastMutex(&g_mntListMutex, "SceVfsMntlistLock", 0, 0);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 693, "mnt_list lock creation failed ret=0x%x\n", ret);
	return ret;
}

int vfs_lock_mnt_list(void)
{
	int ret;

	ret = ksceKernelLockFastMutex(&g_mntListMutex);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 711, "mnt_list lock failed ret=0x%x\n", ret);
	return ret;
}

int vfs_unlock_mnt_list(void)
{
	int ret;

	ret = ksceKernelUnlockFastMutex(&g_mntListMutex);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 730, "mnt_list unlock failed ret=0x%x\n", ret);
	return ret;
}

const char *find_mounted_path(const char *name, SceSize len)
{
	SceVfsMount *mnt;
	const signed char *assign;

	vfs_lock_mnt_list();
	for (mnt = g_mntList; mnt != NULL; mnt = mnt->mnt_list_next) {
		assign = (const signed char *)mnt->mnt_data->assign_name;
		if (strncmp((const char *)assign, name, len) == 0 && assign[len] == ':') {
			vfs_unlock_mnt_list();
			return mnt->path;
		}
	}
	vfs_unlock_mnt_list();
	return NULL;
}

int vfs_create_ncache_mtx(void)
{
	int ret;

	ret = ksceKernelInitializeFastMutex(&g_ncacheMutex, "SceVfsNcacheLock", 2, 0);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 743, "ncache lock creation failed ret=0x%x\n", ret);
	return ret;
}

void vfs_lock_ncache(void)
{
	ksceKernelLockFastMutex(&g_ncacheMutex);
}

void vfs_unlock_ncache(void)
{
	ksceKernelUnlockFastMutex(&g_ncacheMutex);
}

int vfs_create_vfs_mtx(void)
{
	int ret;

	ret = ksceKernelInitializeFastMutex(&g_vfsMutex, "SceVfsLock", 0, 0);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 787, "::0x10f::004::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 788, "vfs lock creation failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfs_lock_vfs(void)
{
	int ret;

	ret = ksceKernelLockFastMutex(&g_vfsMutex);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 805, "::0x10f::006::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 806, "vfs lock failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfs_unlock_vfs(void)
{
	int ret;

	ret = ksceKernelUnlockFastMutex(&g_vfsMutex);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 825, "::0x10f::008::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 826, "vfs unlock failed ret=0x%x\n", ret);
	}
	return ret;
}

/* name unknown: returns a vop table to the cache, or frees it */
int vfs_free_vop_table(SceVopTable *tbl)
{
	int ret, i;

	vfs_lock_vfs();
	for (i = 0; i < VFS_VOP_TABLE_CACHE_NUM; i++) {
		if (g_vopTableCache[i] == NULL) {
			g_vopTableCache[i] = tbl;
			vfs_unlock_vfs();
			return 0;
		}
	}
	ret = ksceKernelFreeHeapMemory(g_vfsNcacheHeap, tbl);
	vfs_unlock_vfs();
	return ret;
}

static inline __attribute__((always_inline)) int unlock_vdlock(SceVfsVnode *vp)
{
	SceUInt64 expected, value;
	SceUID thid;
	int ret;

	thid = ksceKernelGetThreadId();
	if (vp->vdlock.owner_id != thid)
		return SCE_ERROR_ERRNO_EPERM;
	if (vp->vdlock.recursive_count != 0) {
		vp->vdlock.recursive_count--;
		return 0;
	}
	if (vp->vdlock.waiter == 0) {
		expected = (SceUInt64)*(volatile SceUInt32 *)&vp->vdlock.owner_id << 32;
		AtomicFullBarrier();
		for (;;) {
			value = AtomicLoadReserved64((SceUInt64 *)&vp->vdlock);
			if (value != expected)
				break;
			if (!AtomicStoreReserved64((SceUInt64 *)&vp->vdlock,
						  (SceUInt64)0xffffffff << 32))
				return 0;
		}
	}
	vp->vdlock.owner_id = VDLOCK_OWNER_HANDOVER;
	ret = ksceKernelSetEventFlag(vp->vdlock.wait_id, vp->vdlock.wait_pattern);
	if (ret != 0)
		SCE_DBG_PRINTF_FUNC(0xf, 682, "unlock_vdlock",
				    "sceKernelSetEventFlag(0x%x, 0x%x) failed ret=0x%x\n",
				    vp->vdlock.wait_id, vp->vdlock.wait_pattern, ret);
	return ret;
}

void fini_vnode(SceVfsVnode *vp)
{
	int ret;

	if (vp->core.vop_tbl != NULL) {
		ret = vfs_free_vop_table(vp->core.vop_tbl);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 1291, "vop table free failed ret=0x%x tbl=%p\n", ret,
				       vp->core.vop_tbl);
	}
	if (vp->core.bc != NULL) {
		if (vp->core.mnt->state == VFS_MOUNT_STATE_UMOUNTING)
			vfsBuffFree(vp->core.bc, 0, vp, vp->core.fd_list);
		else
			SCE_DBG_ASSERT(0, 1320);
	}
	memset(&vp->core, 0, 0x98);
	if (ksceKernelGetThreadId() != vp->vdlock.owner_id)
		return;
	vp->vdlock.recursive_count = 0;
	ret = unlock_vdlock(vp);
	SCE_DBG_ASSERT(ret == 0, 1339);
}

/* name unknown: a vop table from the cache, or a new one */
SceVopTable *vfs_alloc_vop_table(void)
{
	SceVopTable *tbl;
	int i;

	vfs_lock_vfs();
	for (i = 0; i < VFS_VOP_TABLE_CACHE_NUM; i++) {
		tbl = g_vopTableCache[i];
		if (tbl != NULL) {
			g_vopTableCache[i] = NULL;
			vfs_unlock_vfs();
			return tbl;
		}
	}
	tbl = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, sizeof(SceVopTable));
	vfs_unlock_vfs();
	return tbl;
}

int vfs_debug_extend_buff(int index)
{
	SceUID uid;
	BufferPool *pool;
	void *base;
	int ret;

	if (index != 0)
		vfs_lock_vfs();
	if (g_vfsDebugMemBlock[index] != -1) {
		ret = 0;
		goto out;
	}
	uid = ksceKernelAllocMemBlock("SceIoFileDebug", 0x10f0d006, 0x10000, NULL);
	if (uid < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 2918, "cound not allocate dbg memory ... %d 0x%x\n", uid,
				     uid);
		ret = uid;
		goto out;
	}
	ksceKernelGetMemBlockBase(uid, &base);
	pool = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, 0x220);
	if (pool == NULL) {
		ksceKernelFreeMemBlock(uid);
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	memset(base, 0, 0x10000);
	memset(pool, 0, 0x220);
	ret = bufferPoolInitialize(pool, 0x40, base, 0x400, (BufferPoolNode *)(pool + 1));
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 2948, "bufferPoolInitialize() failed ... %d 0x%x\n",
				     ret, ret);
		ksceKernelFreeMemBlock(uid);
		ksceKernelFreeHeapMemory(g_vfsNcacheHeap, pool);
	} else {
		g_vfsDebugMemBlock[index] = uid;
		g_vfsDebugBufferPool[index] = pool;
	}
out:
	if (index != 0)
		vfs_unlock_vfs();
	return ret;
}

char *vfs_debug_allocate_buff(int *index)
{
	char *buf;
	int ret, i;

	for (i = 0; i < VFS_DEBUG_BUFF_POOL_NUM; i++) {
		if (g_vfsDebugMemBlock[i] == -1) {
			ret = vfs_debug_extend_buff(i);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 2977,
						     "cound not extend dbg memory ... %d 0x%x\n", ret,
						     ret);
				return NULL;
			}
		}
		buf = bufferPoolAlloc(g_vfsDebugBufferPool[i]);
		if (buf != NULL)
			break;
	}
	*index = i;
	return buf;
}

int vfsLockMnt(SceVfsMount *mnt)
{
	int ret;

	ret = ksceKernelLockFastMutex(&mnt->fast_mutex);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 835, "::0x10f::009::%p 0x%x\n", mnt, ret);
		SCE_DBG_PRINTF(0xf, 836, "mnt[%p] lock failed ret=0x%x\n", mnt, ret);
	}
	return ret;
}

int vfsUnlockMnt(SceVfsMount *mnt)
{
	int ret;

	ret = ksceKernelUnlockFastMutex(&mnt->fast_mutex);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 855, "::0x10f::011::%p 0x%x\n", mnt, ret);
		SCE_DBG_PRINTF(0xf, 856, "mnt[%p] unlock failed ret=0x%x\n", mnt, ret);
	}
	return ret;
}

static inline __attribute__((always_inline)) void fini_mtx(SceKernelFastMutex *mtx, int ret)
{
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xf, 485, "fini_mtx", "failed ret = 0x%x\n", ret);
		vfs_dump_fast_mutex(mtx);
	}
}

static inline __attribute__((always_inline)) void fini_fdlock(struct FdLock *fdlock)
{
	SceUID uid;
	int ret;

	uid = fdlock->cond;
	fdlock->cond = -1;
	ret = ksceKernelDeleteCond(uid);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xf, 2567, "fini_fdlock", "sceKernelDeleteCond failed ret = 0x%x\n",
				    ret);
	uid = fdlock->mutex;
	fdlock->mutex = -1;
	ret = ksceKernelDeleteMutex(uid);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xf, 2574, "fini_fdlock",
				    "sceKernelDeleteMutex failed ret = 0x%x\n", ret);
}

/* finalizes an unmounted mount and returns it to the free list */
void vfs_free_mount(SceVfsMount *mnt)
{
	ThreadmgrFastMutexInfo info;
	int ret, intr;

	info.size = sizeof(info);
	ret = ksceKernelGetFastMutexInfo(&mnt->fast_mutex, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 360, "can not get fmtx info ... %d 0x%X\n", ret, ret);
		vfsUnlockMnt(mnt);
	} else if (info.lockCount != 0) {
		vfsUnlockMnt(mnt);
	}
	ret = ksceKernelFinalizeFastMutex(&mnt->fast_mutex);
	fini_mtx(&mnt->fast_mutex, ret);
	fini_fdlock(mnt->fd_lock);
	if (mnt->fumount != NULL) {
		ret = vfs_free_vop_table(mnt->fumount->vops);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 379, "fumount table free failed ret=0x%x tbl=%p\n", ret,
				       mnt->fumount);
	}
	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_freeMntListLock);
	if (g_freeMntList != NULL)
		mnt->mnt_list_next = g_freeMntList;
	g_freeMntList = mnt;
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_freeMntListLock, intr);
}

SceBool vfsIsLockedMnt(SceVfsMount *mnt)
{
	ThreadmgrFastMutexInfo info;
	int ret;

	info.size = sizeof(info);
	ret = ksceKernelGetFastMutexInfo(&mnt->fast_mutex, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 869, "sceKernelGetFastMutexInfo(%p): 0x%08x\n", mnt, ret);
		SCE_DBG_ASSERT_CHECK(0, 870);
		return ret;
	}
	if (info.currentOwnerId != ksceKernelGetThreadId()) {
		SCE_DBG_PRINTF(0xf, 874, "sceKernelGetFastMutexInfo(%p): 0x%08x\n", mnt, ret);
		SCE_DBG_PRINTF(0xf, 875, "inf.currentOwnerId:     0x%08x\n", info.currentOwnerId);
		SCE_DBG_PRINTF(0xf, 876, "inf.lockCount:          0x%08x\n", info.lockCount);
		SCE_DBG_PRINTF(0xf, 877, "sceKernelGetThreadId(): 0x%08x\n", ksceKernelGetThreadId());
		SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 878);
		return 0;
	}
	if (info.lockCount != 0)
		return 1;
	SCE_DBG_PRINTF(0xf, 881, "sceKernelGetFastMutexInfo(%p): 0x%08x\n", mnt, ret);
	SCE_DBG_PRINTF(0xf, 882, "inf.currentOwnerId:     0x%08x\n", info.currentOwnerId);
	SCE_DBG_PRINTF(0xf, 883, "inf.lockCount:          0x%08x\n", info.lockCount);
	SCE_DBG_PRINTF(0xf, 884, "sceKernelGetThreadId(): 0x%08x\n", ksceKernelGetThreadId());
	SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 885);
	return 0;
}

static inline __attribute__((always_inline)) int lock_fdlock(struct FdLock *fdlock)
{
	int ret;

	ret = ksceKernelLockMutex(fdlock->mutex, 1, NULL);
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xf, 2520, "lock_fdlock",
				    "sceKernelLockMutex failed ... %d 0x%08x\n", ret, ret);
	return ret;
}

int vfsLockFd(SceVfsFile *file)
{
	int ret;

	ret = lock_fdlock(file->fd_lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 932, "::0x10f::012::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 933, "fdmnt lock failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfsLockFdByVnode(SceVfsVnode *vp)
{
	int ret;

	ret = lock_fdlock(vp->core.mnt->fd_lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 946, "::0x10f::013::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 947, "fdmnt lock failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfsTrylockFd(SceVfsFile *file)
{
	int ret;

	ret = ksceKernelTryLockMutex(file->fd_lock->mutex, 1);
	if (ret < 0) {
		if (ret != SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN) {
			SCE_PRINTF_WITH_INFO(0xf, 961, "::0x10f::014::0x%x\n", ret);
			SCE_DBG_PRINTF(0xf, 962, "fdmnt trylock failed ret=0x%x\n", ret);
		}
	}
	return ret;
}

int vfsUnlockFd(SceVfsFile *file)
{
	int ret;

	ret = ksceKernelUnlockMutex(file->fd_lock->mutex, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 976, "::0x10f::015::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 977, "fdmnt unlock failed ret=0x%x\n", ret);
	}
	return ret;
}

static inline __attribute__((always_inline)) int wait_fdlock(struct FdLock *fdlock)
{
	int ret;

	fdlock->waitNum++;
	ret = ksceKernelWaitCond(fdlock->cond, NULL);
	fdlock->waitNum--;
	if (ret < 0)
		SCE_DBG_PRINTF_FUNC(0xf, 2543, "wait_fdlock", "sceKernelWaitCond failed ... %d 0x%08x\n",
				    ret, ret);
	return ret;
}

int vfsWaitFd(SceVfsFile *file)
{
	int ret;

	ret = wait_fdlock(file->fd_lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 985, "::0x10f::016::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 986, "fdmnt wait failed ret=0x%x\n", ret);
	}
	return ret;
}

#if FW_DEBUG
SceBool vfsIsLockedFd(SceVfsFile *file)
{
	ThreadmgrMutexInfo info;
	SceUID owner;
	int ret;

	info.size = sizeof(info);
	ret = vfsGetMutexInfo(file->fd_lock->mutex, &info);
	if (ret < 0) {
		SCE_DBG_ASSERT_CHECK(0, 998);
		return ret;
	}
	if ((info.currentOwnerId & 0x40000000) != 0)
		info.currentOwnerId = kscePUIDtoGUID(SCE_KERNEL_PROCESS_ID, info.currentOwnerId);
	owner = info.currentOwnerId;
	if (ksceKernelGetThreadId() != owner) {
		SCE_DBG_PRINTF(0xf, 1005, "sceKernelGetMutexInfo(0x%08x): 0x%08x\n",
			       file->fd_lock->mutex, ret);
		SCE_DBG_PRINTF(0xf, 1006, "inf.currentOwnerId:     0x%08x\n", info.currentOwnerId);
		SCE_DBG_PRINTF(0xf, 1007, "inf.currentCount:       0x%08x\n", info.currentCount);
		SCE_DBG_PRINTF(0xf, 1008, "sceKernelGetThreadId(): 0x%08x\n", ksceKernelGetThreadId());
		SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 1009);
		return 0;
	}
	if (info.currentCount != 0)
		return 1;
	SCE_DBG_PRINTF(0xf, 1012, "sceKernelGetMutexInfo(0x%08x): 0x%08x\n", file->fd_lock->mutex,
		       ret);
	SCE_DBG_PRINTF(0xf, 1013, "inf.currentOwnerId:     0x%08x\n", info.currentOwnerId);
	SCE_DBG_PRINTF(0xf, 1014, "inf.currentCount:       0x%08x\n", info.currentCount);
	SCE_DBG_PRINTF(0xf, 1015, "sceKernelGetThreadId(): 0x%08x\n", ksceKernelGetThreadId());
	SCE_DBG_ASSERT_LEVEL_CHECK(2, 0, 1016);
	return 0;
}
#endif

int vfsLockFdAll(SceVfsMount *mnt)
{
	int ret;

	ret = lock_fdlock(mnt->fd_lock);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1030, "::0x10f::017::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 1031, "fdmnt lock all failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfsUnlockFdAll(SceVfsMount *mnt)
{
	int ret;

	ret = ksceKernelUnlockMutex(mnt->fd_lock->mutex, 1);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1060, "::0x10f::019::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 1061, "fdmnt unlock all failed ret=0x%x\n", ret);
	}
	return ret;
}

int vfsSignalFd(SceVfsMount *mnt)
{
	int ret;

	if (mnt->fd_lock->waitNum < 1)
		return 0;
	ret = ksceKernelSignalCondAll(mnt->fd_lock->cond);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1074, "::0x10f::020::0x%x\n", ret);
		SCE_DBG_PRINTF(0xf, 1075, "fdmnt signal failed ret=0x%x\n", ret);
	}
	return ret;
}

/* takes the vnode lock if it is free; -1 owner means free */
static inline __attribute__((always_inline)) SceUID vdlock_try_own(SceVfsVnode *vp, SceUID thid)
{
	SceUID owner;

	for (;;) {
		owner = AtomicLoadReserved32((SceUInt32 *)&vp->vdlock.owner_id);
		if (owner != -1)
			break;
		if (!AtomicStoreReserved32((SceUInt32 *)&vp->vdlock.owner_id, thid)) {
			break;
		}
	}
	return owner;
}

/* name unknown: the waiter count and owner are one exclusive doubleword. */
static inline __attribute__((always_inline)) void vdlock_change_waiters(SceVfsVnode *vp,
							     SceUInt64 change)
{
	SceUInt64 value;

	do {
		value = AtomicLoadReserved64((SceUInt64 *)&vp->vdlock);
	} while (AtomicStoreReserved64((SceUInt64 *)&vp->vdlock, value + change));
}

int vfsLockVnode(SceVfsVnode *vp)
{
	SceUID thid, owner;
	int ret;

	thid = ksceKernelGetThreadId();
	owner = vdlock_try_own(vp, thid);
	if (owner == -1) {
		AtomicFullBarrier();
		return 0;
	}
	if (thid == owner) {
		SCE_DBG_ASSERT_FUNC(vp->vdlock.recursive_count != -1, 573, "lock_vdlock");
		vp->vdlock.recursive_count++;
		return 0;
	}
	vdlock_change_waiters(vp, 1);
	if (vdlock_try_own(vp, thid) == -1) {
		vdlock_change_waiters(vp, (SceUInt64)-1);
		AtomicFullBarrier();
		return 0;
	}
	ret = ksceKernelWaitEventFlag(vp->vdlock.wait_id, vp->vdlock.wait_pattern,
				      SCE_EVENT_WAITCLEAR_PAT, NULL, NULL);
	vdlock_change_waiters(vp, (SceUInt64)-1);
	if (ret == 0) {
		vp->vdlock.owner_id = thid;
		AtomicFullBarrier();
		return 0;
	}
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1090, "::0x10f::021::%p 0x%x\n", vp, ret);
		SCE_DBG_PRINTF(0xf, 1091, "vnode=%p lock failed ret=0x%x\n", vp, ret);
		SCE_PANIC(1092);
	}
	return ret;
}

int lock_vdlock(SceVfsVnode *vp)
{
	SceUID thid, owner;

	thid = ksceKernelGetThreadId();
	owner = vdlock_try_own(vp, thid);
	if (owner == -1) {
		AtomicFullBarrier();
		return 0;
	}
	if (thid != owner)
		return SCE_ERROR_ERRNO_EBUSY;
	SCE_DBG_ASSERT(vp->vdlock.recursive_count != -1, 573);
	vp->vdlock.recursive_count++;
	return 0;
}

int vfsUnlockVnode(SceVfsVnode *vp)
{
	int ret;

	ret = unlock_vdlock(vp);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 1126, "::0x10f::023::%p 0x%x\n", vp, ret);
		SCE_DBG_PRINTF(0xf, 1127, "vnode=%p unlock failed ret=0x%x\n", vp, ret);
		SCE_PANIC(1128);
	}
	return ret;
}

SceBool vfsIsOwnerVnode(SceVfsVnode *vp)
{
	return vp->vdlock.owner_id == ksceKernelGetThreadId();
}

struct Ncache *vfs_alloc_ncache(SceUID heap, const VfsPathElem *elem)
{
	struct Ncache *nc;
	char *name;

	name = ksceKernelAllocHeapMemory(heap, elem->len + 1);
	if (name == NULL)
		return NULL;
	nc = ksceKernelAllocHeapMemory(heap, sizeof(*nc));
	if (nc == NULL) {
		ksceKernelFreeHeapMemory(heap, name);
		return NULL;
	}
	memset(nc, 0, sizeof(*nc));
	nc->name = name;
	return nc;
}

void vfs_free_ncache_heap(struct Ncache *nc, SceUID heap)
{
	ksceKernelFreeHeapMemory(heap, nc->name);
	ksceKernelFreeHeapMemory(heap, nc);
}

void vfs_free_ncache(struct Ncache *nc)
{
	SceUID heap;

	if (nc->state == NCACHE_STATE_NEGATIVE)
		heap = g_vfsNcacheHeap;
	else
		heap = nc->vp->core.allocator;
	vfs_free_ncache_heap(nc, heap);
}

void vfs_set_ncache_entry(struct Ncache *nc, SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem,
		    SceUInt32 state)
{
	SceSize len = elem->len;

	nc->vp = vp;
	nc->nameLength = len;
	strncpy(nc->name, elem->name, len);
	nc->name[len] = '\0';
	if (dvp == NULL)
		nc->ncdd = NULL;
	else
		nc->ncdd = dvp->core.ncache;
	nc->state = state;
	if (vp != NULL) {
		vp->core.ncache = nc;
		if (vp->core.dd != dvp)
			vp->core.dd = dvp;
	}
}

void vfs_insert_mnt_list(SceVfsMount *mnt)
{
	if (g_mntList != NULL)
		mnt->mnt_list_next = g_mntList;
	g_mntList = mnt;
}

int vfs_remove_mnt_list(SceVfsMount *mnt)
{
	SceVfsMount *prev, *cur;
	int ret;

	if (g_mntList == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (g_mntList == mnt) {
			g_mntList = mnt->mnt_list_next;
		} else {
			cur = g_mntList;
			do {
				prev = cur;
				cur = prev->mnt_list_next;
			} while (cur != mnt && cur != NULL);
			prev->mnt_list_next = mnt->mnt_list_next;
		}
		ret = 0;
		mnt->mnt_list_next = NULL;
	}
	SCE_DBG_ASSERT_CHECK(ret == 0, 1498);
	return ret;
}

void vfs_insert_mntedon_list(SceVfsMount *pmnt, SceVfsMount *mnt)
{
	if (pmnt->mnted_on_list != NULL)
		mnt->mnted_on_list_prev = pmnt->mnted_on_list;
	pmnt->mnted_on_list = mnt;
}

int vfs_remove_mntedon_list(SceVfsMount *pmnt, SceVfsMount *mnt)
{
	SceVfsMount *prev, *cur;
	int ret;

	cur = pmnt->mnted_on_list;
	if (cur == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (cur == mnt) {
			pmnt->mnted_on_list = mnt->mnted_on_list_prev;
		} else {
			do {
				prev = cur;
				cur = prev->mnted_on_list_prev;
			} while (cur != mnt && cur != NULL);
			prev->mnted_on_list_prev = mnt->mnted_on_list_prev;
		}
		ret = 0;
		mnt->mnted_on_list_prev = NULL;
	}
	SCE_DBG_ASSERT_CHECK(ret == 0, 1513);
	return ret;
}

int vfs_remove_mnt_vnode_list(SceVfsMount *mnt, SceVfsVnode *vp)
{
	SceVfsVnode *prev, *cur;
	int ret;

	cur = mnt->vnode_list;
	if (cur == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (cur == vp) {
			mnt->vnode_list = vp->core.next;
		} else {
			do {
				prev = cur;
				cur = prev->core.next;
			} while (cur != vp && cur != NULL);
			prev->core.next = vp->core.next;
		}
		ret = 0;
		vp->core.next = NULL;
	}
	SCE_DBG_ASSERT_CHECK(ret == 0, 1537);
	if (ret == 0)
		mnt->vnode_num--;
	return ret;
}

int vfsFreeVnode(SceVfsVnode *vp)
{
	int ret;

	vfsLockMnt(vp->core.mnt);
	vfs_remove_mnt_vnode_list(vp->core.mnt, vp);
	vfsUnlockMnt(vp->core.mnt);
	fini_vnode(vp);
	ret = bufferPoolFree(&g_vnodePool, vp);
	ksceKernelAtomicSubAndGet32(&g_total_vnode_num, 1);
	return ret;
}

int vfs_finalize_mount(SceVfsMount *mnt)
{
	SceVfsVnode *vp, *next;
	int retry, busy, num, freed, ret;

	if ((int)mnt->opened_entry_num > 0) {
		SCE_DBG_PRINTF(0xf, 271, "mnt=%p some entries are still opened %d 0x%x\n", mnt,
			       mnt->opened_entry_num, mnt->opened_entry_num);
		SCE_DBG_ASSERT(0, 272);
	}
	busy = 1;
	retry = 11;
	for (;;) {
		num = mnt->vnode_num;
		if (num < 1 || !busy)
			break;
		if (--retry == 0) {
			SCE_DBG_PRINTF(0xf, 284, "try next reclaim ... \n");
			break;
		}
		busy = 0;
		freed = 0;
		for (vp = mnt->vnode_list; vp != NULL; vp = next) {
			next = vp->core.next;
			ret = lock_vdlock(vp);
			if (ret != 0) {
				if (ret == SCE_ERROR_ERRNO_EBUSY) {
					busy = 1;
					freed++;
					continue;
				}
				SCE_DBG_PRINTF(0xf, 304, "vnode lock failed %p retval=0x%x\n", vp, ret);
			}
			if (vp->core.ref_count != 0) {
				SCE_DBG_PRINTF(0xf, 315, "vp refcount 0 != %d vp=%p\n", vp->core.ref_count,
					       vp);
				vfsUnlockVnode(vp);
				return SCE_ERROR_ERRNO_EBUSY;
			}
			ret = ksceVopInactive(vp);
			if (ret < 0 && ret != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF(0xf, 326, "inactive failed ... %d 0x%x\n", ret, ret);
			vfsFreeVnode(vp);
			freed++;
		}
		if (num != freed)
			SCE_DBG_PRINTF(0xf, 335, "%d vnode are freed but expected %d node\n", freed,
				       num);
	}
	if (mnt->mnt_vnode != NULL) {
		vfsLockVnode(mnt->mnt_vnode);
		vp = mnt->mnt_vnode;
		vp->core.type &= ~SCE_VNODE_TYPE_MOUNTED;
		vp->core.ref_count--;
		vfsUnlockVnode(vp);
	}
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 345, "g_total_vnode_num=%d\n", g_total_vnode_num);
	return 0;
}

int ksceVfsAddVfs(SceVfsInfo *vfsInf)
{
	SceVfsInfo *cur;
	const char *name;
	int ret;

	vfs_lock_mnt_list();
	name = vfsInf->vfs_name;
	for (cur = g_vfsList; cur != NULL; cur = cur->next) {
		if (strncmp(cur->vfs_name, name, 0x20) == 0) {
			ret = SCE_ERROR_ERRNO_EEXIST;
			goto out;
		}
	}
	ret = sceVfsOpInit(vfsInf);
	if (ret < 0)
		goto out;
	if (g_vfsList != NULL)
		vfsInf->next = g_vfsList;
	g_vfsList = vfsInf;
	vfsInf->ref_count = 0;
out:
	vfs_unlock_mnt_list();
	return ret;
}

int ksceVfsDeleteVfs(const char *name, SceVfsInfo **vfsInf)
{
	SceVfsInfo *inf, *prev, *cur;
	int ret;

	vfs_lock_mnt_list();
	for (inf = g_vfsList; inf != NULL; inf = inf->next) {
		if (strncmp(inf->vfs_name, name, 0x20) == 0)
			break;
	}
	if (inf == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
		goto out;
	}
	ret = sceVfsOpFini(inf);
	if (ret < 0)
		goto out;
	if (inf->ref_count != 0) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto out;
	}
	if (g_vfsList == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (g_vfsList == inf) {
			g_vfsList = inf->next;
		} else {
			cur = g_vfsList;
			do {
				prev = cur;
				cur = prev->next;
			} while (cur != inf && cur != NULL);
			prev->next = inf->next;
		}
		ret = 0;
		inf->next = NULL;
	}
	SCE_DBG_ASSERT_CHECK_FUNC(ret == 0, 1599, "sceVfsDeleteVfs");
	if (vfsInf != NULL)
		*vfsInf = inf;
out:
	vfs_unlock_mnt_list();
	return ret;
}

/* name unknown: a registered vfs by name, referenced */
SceVfsInfo *vfs_acquire_vfs(const char *name)
{
	SceVfsInfo *inf;

	vfs_lock_mnt_list();
	for (inf = g_vfsList; inf != NULL; inf = inf->next) {
		if (strncmp(inf->vfs_name, name, 0x20) == 0) {
			inf->ref_count++;
			break;
		}
	}
	vfs_unlock_mnt_list();
	return inf;
}

/* name unknown: drops the reference vfs_acquire_vfs took */
int vfs_release_vfs(const char *name)
{
	SceVfsInfo *inf;
	int ret = SCE_ERROR_ERRNO_ENOENT;

	vfs_lock_mnt_list();
	for (inf = g_vfsList; inf != NULL; inf = inf->next) {
		if (strncmp(inf->vfs_name, name, 0x20) == 0) {
			inf->ref_count--;
			ret = 0;
			break;
		}
	}
	vfs_unlock_mnt_list();
	return ret;
}

void vfs_initialize_file(SceVfsVnode *vp, SceVfsFile *file)
{
	SceVfsMount *mnt = vp->core.mnt;
	SceUInt32 attr;

	SCE_DBG_ASSERT_CHECK(vp->vdlock.owner_id == ksceKernelGetThreadId(), 1734);
	vp->core.fd_num++;
	file->state = SCE_VFS_FILE_STATE_OPEN;
	file->vp = vp;
	file->position = 0;
	file->next = NULL;
	file->is_dir = 0;
	file->flags = 0;
	file->fd = 0;
	file->pid = ksceKernelGetProcessIdFromTLS();
	file->has_flock_ent = 0;
	file->fd_lock = mnt->fd_lock;
	attr = (mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_INTERNAL) != 0;
	file->idata = attr;
	if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0)
		file->idata = attr | VFS_FILE_ATTR_EXTERNAL;
	__builtin_memset(file->ioSchedData, 0, sizeof(file->ioSchedData));
	file->flock_busy_count = 0;
	file->debug_path = NULL;
	if (vp->core.fd_list != NULL)
		file->next = vp->core.fd_list;
	vp->core.fd_list = file;
}

/* name unknown: drops a file's opened-entry count and closes its UID */
int vfs_close_file(SceVfsVnode *vp, SceUID fd)
{
	ksceKernelAtomicSubAndGet32(&vp->core.mnt->opened_entry_num, 1);
	return ksceGUIDClose(fd);
}

/* name unknown: closes a dummy file's UID */
int vfs_close_file_dummy(SceUID fd)
{
	return ksceGUIDClose(fd);
}

int vfs_convert_file(SceUID fd, SceVfsFile **file)
{
	SceUIDVfsFileObject *obj;
	int ret;

	ret = ksceGUIDReferObjectWithClass(fd, &g_vfsFileClass, (SceObjectBase **)&obj);
	if (ret == 0) {
		*file = &obj->file;
	} else {
		switch (ret) {
		case SCE_KERNEL_ERROR_INVALID_UID:
		case SCE_KERNEL_ERROR_INVALID_UID_CLASS:
		case SCE_KERNEL_ERROR_INVALID_UID_SUBCLASS:
			ret = SCE_ERROR_ERRNO_EBADF;
			break;
		}
	}
	return ret;
}

int vfs_release_file(SceUID fd)
{
	int ret;

	ret = ksceGUIDReleaseObject(fd);
	if (ret < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1844, "sceGUIDReleaseObject failed 0x%x 0x%x\n", ret,
				     ret);
	return ret;
}

/* name unknown: the error for a file that is not open */
int vfs_get_file_state_error(SceVfsFile *file)
{
	SceUInt32 state = file->state;

	if ((state & SCE_VFS_FILE_STATE_OPEN) != 0)
		return 0;
	switch (state) {
	case 0x400:
	case SCE_VFS_FILE_STATE_UNK:
		return SCE_ERROR_ERRNO_ENODEV;
	case SCE_VFS_FILE_STATE_CLOSED:
		return SCE_ERROR_ERRNO_EBADF;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

/* name unknown: the error for a closed file */
int vfs_get_file_closed_error(SceVfsFile *file)
{
	if ((file->state & SCE_VFS_FILE_STATE_OPEN) != 0)
		return 0;
	if (file->state == SCE_VFS_FILE_STATE_CLOSED)
		return SCE_ERROR_ERRNO_EBADF;
	return 0;
}

void vfs_insert_ncache_list(struct Ncache *nc)
{
	if (g_ncacheList != NULL)
		nc->listNext = g_ncacheList;
	g_ncacheList = nc;
}

void vfs_remove_ncache_list(struct Ncache *nc)
{
	struct Ncache *prev, *cur;
	int ret;

	if (g_ncacheList == NULL) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1965, "no entry in ncache list nc = %p %s\n", nc,
				     nc->name);
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (g_ncacheList == nc) {
			g_ncacheList = nc->listNext;
		} else {
			cur = g_ncacheList;
			do {
				prev = cur;
				cur = prev->listNext;
			} while (cur != nc && cur != NULL);
			prev->listNext = nc->listNext;
		}
		ret = 0;
		nc->listNext = NULL;
	}
	SCE_DBG_ASSERT_CHECK(ret == 0, 1967);
}

SceVfsMount *vfs_get_mnt_list(void)
{
	return g_mntList;
}

int vfs_file_class_init(void)
{
	__builtin_memset(&g_vfsFileClass, 0, sizeof(g_vfsFileClass));
	return ksceUIDClassInitClass(&g_vfsFileClass, "SceUIDVfsFileClass", ksceKernelGetUIDClass(),
				     sizeof(SceUIDVfsFileObject), vfs_file_class_constructor,
				     vfs_file_class_destructor);
}

int vfs_vnode_initialize(void)
{
	SceVfsVnode *vp;
	BufferPoolNode *nodes;
	void *buf;
	SceUID evf;
	SceSize size;
	int ret, i;

	buf = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, VFS_VNODE_MAX * sizeof(SceVfsVnode) + 8);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	vp = (SceVfsVnode *)(((SceUIntPtr)buf + 7) & ~7);
	nodes = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, VFS_VNODE_MAX * sizeof(BufferPoolNode));
	if (nodes == NULL) {
		ksceKernelFreeHeapMemory(g_vfsNcacheHeap, vp);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	memset(vp, 0, VFS_VNODE_MAX * sizeof(SceVfsVnode));
	memset(nodes, 0, VFS_VNODE_MAX * sizeof(BufferPoolNode));
	__builtin_memset(&g_vnodePool, 0, sizeof(g_vnodePool));
	evf = 0;
	ret = bufferPoolInitialize(&g_vnodePool, VFS_VNODE_MAX, vp, sizeof(SceVfsVnode), nodes);
	size = g_vnodePool.size;
	for (i = 0; i < VFS_VNODE_MAX; i++) {
		if ((i & 0x1f) == 0)
			evf = ksceKernelCreateEventFlag("SceVfsVnode", 0x1000, 0, NULL);
		SCE_DBG_ASSERT(evf > 0, 2109);
		vp->vdlock.waiter = 0;
		vp->vdlock.wait_id = evf;
		vp->vdlock.wait_pattern = 1 << (i & 0x1f);
		vp->vdlock.owner_id = -1;
		vp->vdlock.recursive_count = 0;
		vp = (SceVfsVnode *)((char *)vp + size);
	}
	return ret;
}

/* name unknown: whether a path is absolute or names a device */
int vfs_is_full_path(const char *path)
{
	const signed char *p = (const signed char *)path;

	while (*p == ' ')
		p++;
	if (*p == '/')
		return 1;
	return strchr((const char *)p, ':') != NULL;
}

/* name unknown: fills a vop table with the ENODEV vops, keeping three of ops */
static inline __attribute__((always_inline)) void vfs_fill_fumount_vop(SceVopTable *tbl,
									const SceVopTable *ops,
									int preserveDclose)
{
	tbl->vop_open = (void *)vfs_fumount_vop_enodev;
	tbl->vop_inactive = ops->vop_inactive;
	tbl->vop_close = ops->vop_close;
	if (preserveDclose)
		tbl->vop_dclose = ops->vop_dclose;
	tbl->vop_create = (void *)vfs_fumount_vop_enodev;
	tbl->vop_lookup = (void *)vfs_fumount_vop_enodev;
	tbl->vop_read = (void *)vfs_fumount_vop_enodev;
	tbl->vop_write = (void *)vfs_fumount_vop_enodev;
	tbl->vop_lseek = (void *)vfs_fumount_vop_enodev;
	tbl->vop_ioctl = (void *)vfs_fumount_vop_enodev;
	tbl->vop_remove = (void *)vfs_fumount_vop_enodev;
	tbl->vop_mkdir = (void *)vfs_fumount_vop_enodev;
	tbl->vop_rmdir = (void *)vfs_fumount_vop_enodev;
	tbl->vop_dopen = (void *)vfs_fumount_vop_enodev;
	tbl->vop_dread = (void *)vfs_fumount_vop_enodev;
	tbl->vop_getstat = (void *)vfs_fumount_vop_enodev;
	tbl->vop_chstat = (void *)vfs_fumount_vop_enodev;
	tbl->vop_rename = (void *)vfs_fumount_vop_enodev;
	tbl->reserved = (void *)vfs_fumount_vop_enodev;
	tbl->vop_pread = (void *)vfs_fumount_vop_enodev;
	tbl->vop_pwrite = (void *)vfs_fumount_vop_enodev;
	tbl->vop_sync = (void *)vfs_fumount_vop_enodev;
	tbl->vop_fgetstat = (void *)vfs_fumount_vop_enodev;
	tbl->vop_fchstat = (void *)vfs_fumount_vop_enodev;
}

int vfs_set_fumount_vop(SceVfsMount *mnt)
{
	SceVopTable *tbl;
	SceVfsVnode *vp, *first;

	first = mnt->vnode_list;
	tbl = vfs_alloc_vop_table();
	if (tbl == NULL) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 2149, "could not allocate memory\n");
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	mnt->fumount->vops = tbl;
	memset(tbl, 0, sizeof(*tbl));
	vfs_fill_fumount_vop(tbl, first->core.ops, 1);
	for (vp = mnt->vnode_list; vp != NULL; vp = vp->core.next)
		vp->core.ops = tbl;
	return 0;
}

/* name unknown: gives one vnode the ENODEV vops */
int vfs_set_fumount_vop_vnode(SceVfsVnode *vp)
{
	SceVopTable *tbl;

	tbl = vfs_alloc_vop_table();
	if (tbl == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(tbl, 0, sizeof(*tbl));
	vfs_fill_fumount_vop(tbl, vp->core.ops, 0);
	vp->core.ops = tbl;
	vp->core.vop_tbl = tbl;
	return 0;
}

static inline __attribute__((always_inline)) int vfs_process_close(SceUID guid, SceVfsFile *file)
{
	union {
		IofilemgrCloseParam close;
		IofilemgrDcloseParam dclose;
	} param;
	int ret, i;

	if (file->is_dir != 0) {
		param.dclose.fd = guid;
		param.dclose.reserved = 0;
		ret = ksceKernelRunWithStack(0x2000, (void *)vfsDclose, &param);
		if (ret < 0) {
			sceKernelDelayThread(10000);
			for (i = 0; i < 3; i++) {
				ret = ksceKernelRunWithStack(0x2000, (void *)vfsDclose, &param);
				if (ret >= 0)
					goto done;
				sceKernelDelayThread(10000);
			}
			ret = vfsCleanup(guid);
			if (ret < 0)
				SCE_DBG_PRINTF_FUNC(0xf, 2267, "vfs_process_finalize",
					"vfsCleanup(guid=0x%x) failed = 0x%x\n", guid, ret);
			ret = ksceKernelRunWithStack(0x2000, (void *)vfsDclose, &param);
			if (ret < 0)
				SCE_DBG_PRINTF_FUNC(0xf, 2282, "vfs_process_finalize",
					"vfsDclose failed() ... %d 0x%x uid=0x%x %d\n",
					ret, ret, guid, guid);
		}
	} else {
		param.close.fd = guid;
		ret = ksceKernelRunWithStack(0x2000, (void *)vfsClose, &param);
		if (ret < 0) {
			sceKernelDelayThread(10000);
			for (i = 0; i < 3; i++) {
				ret = ksceKernelRunWithStack(0x2000, (void *)vfsClose, &param);
				if (ret >= 0)
					goto done;
				sceKernelDelayThread(10000);
			}
			ret = vfsCleanup(guid);
			if (ret < 0)
				SCE_DBG_PRINTF_FUNC(0xf, 2311, "vfs_process_finalize",
					"vfsCleanup(guid=0x%x) failed = 0x%x\n", guid, ret);
			ret = ksceKernelRunWithStack(0x2000, (void *)vfsClose, &param);
			if (ret < 0)
				SCE_DBG_PRINTF_FUNC(0xf, 2328, "vfs_process_finalize",
					"vfsClose failed() ... %d 0x%x uid=0x%x %d\n",
					ret, ret, guid, guid);
		}
	}
done:
	return ret;
}

int vfs_process_finalize(SceUID pid)
{
	SceUID puid[0x80];
	SceVfsFile *file;
	SceSize num;
	SceUID guid;
	int ret, i;

	do {
		ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, puid, 0x40, &num);
		if (ret < 0 || (int)num < 1)
			return ret;
		for (i = 0; i < (int)num; i++) {
			guid = kscePUIDtoGUID(pid, puid[i]);
			if (guid < 0) {
				SCE_DBG_PRINTF(0xf, 2224, "PUIDtoGUID failed err= %d 0x%x puid=0x%x\n",
					       guid, guid, puid[i]);
				continue;
			}
			ret = vfs_convert_file(guid, &file);
			if (ret != 0) {
				SCE_DBG_PRINTF(0xf, 2339,
					       "vfs_convert_file failed() ... %d 0x%x uid=0x%x %d\n", ret,
					       ret, guid, guid);
				continue;
			}
			if (file->state == SCE_VFS_FILE_STATE_DUMMY) {
				ret = SceIofilemgrForDriver_3A79FAC9(guid);
			} else {
				ret = vfs_process_close(guid, file);
			}
			if (ret == 0)
				kscePUIDClose(pid, puid[i]);
			vfs_release_file(guid);
		}
	} while (num == 0x40);
	return ret;
}

BufferPool *vfs_get_vnode_pool(void)
{
	return &g_vnodePool;
}

/* name unknown: fills the vop table cache */
void vfs_vop_table_cache_initialize(void)
{
	SceVopTable **entry;

	for (entry = g_vopTableCache; entry != g_vopTableCache + VFS_VOP_TABLE_CACHE_NUM;
	     entry++)
		*entry = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, sizeof(SceVopTable));
}

char *vfs_path_buff_alloc(void)
{
	char *buf;
	int intr, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_pathBuffLock);
	for (i = 0; i < VFS_PATH_BUFF_CACHE_NUM; i++) {
		buf = g_pathBuffCache[i];
		if (buf != NULL) {
			g_pathBuffCache[i] = NULL;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_pathBuffLock, intr);
			goto clear;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_pathBuffLock, intr);
	buf = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, VFS_PATH_BUFF_SIZE);
	if (buf == NULL)
		return NULL;
clear:
	memset(buf, 0, VFS_PATH_BUFF_SIZE);
	return buf;
}

void vfs_path_buff_free(char *buf)
{
	int intr, ret, i;

	intr = ksceKernelSpinlockLowLockCpuSuspendIntr(&g_pathBuffLock);
	for (i = 0; i < VFS_PATH_BUFF_CACHE_NUM; i++) {
		if (g_pathBuffCache[i] == NULL) {
			g_pathBuffCache[i] = buf;
			ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_pathBuffLock, intr);
			return;
		}
	}
	ksceKernelSpinlockLowUnlockCpuResumeIntr(&g_pathBuffLock, intr);
	ret = ksceKernelFreeHeapMemory(g_vfsNcacheHeap, buf);
	if (ret != 0) {
		SCE_DBG_PRINTF(0xf, 2455, "free buffer is %p retval=0x%x\n", buf, ret);
		SCE_DBG_ASSERT(0, 2456);
	}
}

/* name unknown: empties the path buffer cache */
void vfs_path_buff_initialize(void)
{
	g_pathBuffCache[0] = NULL;
	g_pathBuffCache[1] = NULL;
	g_pathBuffCache[2] = NULL;
	g_pathBuffCache[3] = NULL;
	g_pathBuffLock = 0;
}

int vfs_sync(SceVfsMount *mnt)
{
	SceVfsVnode *vp, *link;
	SceSize size;
	int num, ret, i;

	size = g_vnodePool.size;
	i = 0;
	do {
		num = g_available_vnode_num;
		vp = (SceVfsVnode *)((char *)g_vnodePool.base + size * i);
		for (; i < num; i++, vp = (SceVfsVnode *)((char *)vp + size)) {
			vfsLockVnode(vp);
			if (vp->core.mnt != mnt ||
			    (vp->core.type & (SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_DIR)) ==
				    (SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_DIR) ||
			    vp->core.state >= 0x1000) {
				vfsUnlockVnode(vp);
				continue;
			}
			link = vp->core.link_to;
			if (link == NULL) {
				ret = ksceVopSync(vp, vp->core.fd_list, 1);
			} else {
				vfsLockVnode(link);
				ret = ksceVopSync(vp->core.link_to, vp->core.link_to->core.fd_list, 1);
				vfsUnlockVnode(vp->core.link_to);
			}
			if (ret < 0 && ret != SCE_ERROR_ERRNO_EUNSUP) {
				SCE_DBG_PRINTF(0xf, 2621, "vop sync failed vp=%p ret=0x%x\n", vp, ret);
				if (vp->core.ncache != NULL)
					SCE_DBG_PRINTF(0xf, 2624, "ncache name is %s\n",
						       vp->core.ncache->name);
				SCE_DBG_PRINTF(0xf, 2632, "some file/dir in %s\n",
					       mnt->mnt_data->assign_name);
			}
			vfsUnlockVnode(vp);
		}
	} while (num != g_available_vnode_num);
	return sceVfsOpSync(mnt, 0);
}

/* name unknown: marks a mount for vfs_sync_all_mount_point */
void vfs_set_mnt_sync_request(SceVfsMount *mnt)
{
	mnt->fumount->busy = 1;
}

void vfs_clear_mnt_sync_request(SceVfsMount *mnt)
{
	mnt->fumount->busy = 0;
}

int vfs_sync_all_mount_point(void)
{
	SceVfsMount *mnt;
	int synced, tty, again, facility, ret, i;

	synced = 0;
	tty = 0;
	do {
		again = 0;
		vfs_lock_mnt_list();
restart:
		for (mnt = g_mntList; mnt != NULL; mnt = mnt->mnt_list_next) {
			if (mnt->fumount->busy == 0)
				continue;
			vfs_unlock_mnt_list();
			if (mnt->fs_type == SCE_VFS_FS_TYPE_CHRDEV)
				tty = 1;
			if (mnt->state == VFS_MOUNT_STATE_MOUNTED) {
				ret = vfs_sync(mnt);
				if (ret >= 0 || ret == SCE_ERROR_ERRNO_EUNSUP) {
					mnt->fumount->busy = 0;
				} else {
					SCE_DBG_PRINTF(0xf, 2699, "vfs_sync failed ... 0x%x %d\n", ret,
						       ret);
					facility = ((SceUInt32)ret >> 16) & 0xfff;
					if (facility == 0x32 || facility == 0x3d) {
						for (i = 0; i < 3; i++) {
							if (mnt->state != VFS_MOUNT_STATE_MOUNTED) {
								ret = 0;
								break;
							}
							ret = vfs_sync(mnt);
							if (ret == 0)
								break;
						}
						SCE_DBG_PRINTF(0xf, 2719, "vfs_sync retry ret=0x%x %d\n",
							       ret, ret);
						mnt->fumount->busy = 0;
					}
				}
				synced++;
				again = 1;
			} else {
				mnt->fumount->busy = 0;
			}
			vfs_lock_mnt_list();
			if (mnt->state != VFS_MOUNT_STATE_MOUNTED) {
				vfs_unlock_mnt_list();
				vfs_lock_mnt_list();
				synced = 0;
				tty = 0;
				goto restart;
			}
		}
		vfs_unlock_mnt_list();
	} while (again);
	if (tty && synced == 1)
		vfs_clear_sync_pending();
	return 0;
}

/* name unknown: gives a file its debug path record */
int vfs_debug_path_create(SceVfsFile *file, const char *path)
{
	struct DebugPath *dp;
	char *buf;
	int index;

	dp = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, sizeof(*dp));
	if (dp == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(dp, 0, sizeof(*dp));
	buf = vfs_debug_allocate_buff(&index);
	if (buf == NULL) {
		ksceKernelFreeHeapMemory(g_vfsNcacheHeap, dp);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	file->debug_path = dp;
	dp->resolvedOpenPath = buf;
	if (path == NULL) {
		buf[0] = '\0';
		dp->resolvedOpenPathLength = 0;
	} else {
		strncpy(buf, path, VFS_PATH_BUFF_SIZE);
		dp->resolvedOpenPathLength = strnlen(path, VFS_PATH_BUFF_SIZE);
	}
	dp->resolvedOpenPathPool = index;
	return 0;
}

/* name unknown: sets a file's resolved open path */
int vfs_debug_set_resolved_path(SceVfsFile *file, const char *path)
{
	struct DebugPath *dp = file->debug_path;

	if (dp == NULL)
		return vfs_debug_path_create(file, path);
	if (path == NULL) {
		dp->resolvedOpenPath[0] = '\0';
		dp->resolvedOpenPathLength = 0;
		return 0;
	}
	strncpy(dp->resolvedOpenPath, path, VFS_PATH_BUFF_SIZE);
	dp->resolvedOpenPathLength = strnlen(path, VFS_PATH_BUFF_SIZE);
	return 0;
}

/* name unknown: sets a file's open path, after the resolved one when it fits */
int vfs_debug_set_open_path(SceVfsFile *file, const char *path)
{
	struct DebugPath *dp = file->debug_path;
	SceSize len;
	char *buf;
	int index;

	if (dp == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if (path == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	len = strnlen(path, VFS_PATH_BUFF_SIZE);
	dp->openPathLength = len;
	if (dp->resolvedOpenPathLength + 2 + len > VFS_PATH_BUFF_SIZE) {
		buf = vfs_debug_allocate_buff(&index);
		if (buf == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		strncpy(buf, path, VFS_PATH_BUFF_SIZE);
		dp->openPath = buf;
		dp->openPathPool = index;
		return 0;
	}
	buf = dp->resolvedOpenPath + dp->resolvedOpenPathLength + 1;
	dp->openPath = buf;
	strncpy(buf, path, len + 1);
	return 0;
}

/* name unknown: records the open flags and mode in a file's debug path record */
int vfs_debug_set_open_param(SceVfsFile *file, int flags, SceMode mode)
{
	struct DebugPath *dp = file->debug_path;

	if (dp == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	dp->flags = flags;
	dp->mode = mode;
	return 0;
}

/* name unknown: frees a file's debug path record */
int vfs_debug_path_free(SceVfsFile *file)
{
	struct DebugPath *dp = file->debug_path;

	if (dp == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	bufferPoolFree(g_vfsDebugBufferPool[dp->resolvedOpenPathPool], dp->resolvedOpenPath);
	if (dp->openPathLength != 0 &&
	    dp->openPathLength + dp->resolvedOpenPathLength + 2 > VFS_PATH_BUFF_SIZE)
		bufferPoolFree(g_vfsDebugBufferPool[dp->openPathPool], dp->openPath);
	ksceKernelFreeHeapMemory(g_vfsNcacheHeap, dp);
	file->debug_path = NULL;
	return 0;
}

int vfs_finalize_file(SceVfsVnode *vp, SceVfsFile *file)
{
	SceVfsFile *prev, *cur;
	int ret;

	SCE_DBG_ASSERT_CHECK(vp->vdlock.owner_id == ksceKernelGetThreadId(), 1780);
	cur = vp->core.fd_list;
	if (cur == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		if (cur == file) {
			vp->core.fd_list = file->next;
		} else {
			do {
				prev = cur;
				cur = prev->next;
			} while (cur != file && cur != NULL);
			prev->next = file->next;
		}
		ret = 0;
		file->next = NULL;
	}
	SCE_DBG_ASSERT_CHECK(ret == 0, 1783);
	if (ret == 0) {
		vp->core.fd_num--;
		file->vp = NULL;
		if (ksceKernelCheckDipsw(0xd2) != 0) {
			vfs_debug_path_free(file);
			return 0;
		}
	}
	return ret;
}

/* name unknown: empties the debug buffer pools and creates the first */
void vfs_debug_buff_initialize(void)
{
	int i;

	for (i = 0; i < VFS_DEBUG_BUFF_POOL_NUM; i++) {
		g_vfsDebugMemBlock[i] = -1;
		g_vfsDebugBufferPool[i] = NULL;
	}
	vfs_debug_extend_buff(0);
}

/* name unknown: the device activation event flag of a removable media mount */
int vfs_set_dev_wait_evf(SceVfsMount *mnt)
{
	struct Fumount *fumount = mnt->fumount;
	SceInt16 opt = (SceInt16)mnt->opt;

	switch (opt) {
	case VFS_MOUNT_OPT_MC:
		if (g_vfsDevWaitForMCEvf == -1)
			g_vfsDevWaitForMCEvf = ksceKernelCreateEventFlag("SceIofileDevWaitForMC",
									  0x1000, 0, NULL);
		if (g_vfsDevWaitForMCEvf > 0) {
			fumount->activationEvf = &g_vfsDevWaitForMCEvf;
			fumount->devState = &g_vfsDevWaitForMCState;
		}
		return g_vfsDevWaitForMCEvf;
	case VFS_MOUNT_OPT_SD:
		if (g_vfsDevWaitForSDEvf == -1)
			g_vfsDevWaitForSDEvf = ksceKernelCreateEventFlag("SceIofileDevWaitForSD",
									  0x1000, 0, NULL);
		if (g_vfsDevWaitForSDEvf > 0) {
			fumount->activationEvf = &g_vfsDevWaitForSDEvf;
			fumount->devState = &g_vfsDevWaitForSDState;
		}
		return g_vfsDevWaitForSDEvf;
	case VFS_MOUNT_OPT_VC:
		if (g_vfsDevWaitForVCEvf == -1)
			g_vfsDevWaitForVCEvf = ksceKernelCreateEventFlag("SceIofileDevWaitForVC",
									  0x1000, 0, NULL);
		if (g_vfsDevWaitForVCEvf > 0) {
			fumount->activationEvf = &g_vfsDevWaitForVCEvf;
			fumount->devState = &g_vfsDevWaitForVCState;
		}
		return g_vfsDevWaitForVCEvf;
	default:
		return 0;
	}
}

int vfs_wait_fumount(SceVfsMount *mnt)
{
	return ksceKernelWaitEventFlag(*mnt->fumount->activationEvf, 1, 0, NULL, NULL);
}

int vfs_check_fd_status(SceVfsFile *file, int closing)
{
	struct FdLock *fdlock = file->fd_lock;
	volatile SceUInt32 state = fdlock->state;
	SceVfsMount *mnt;
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsLockedFd(file) == 1, 1855);
	if (file->state == SCE_VFS_FILE_STATE_DUMMY)
		return SCE_ERROR_ERRNO_EBADF;
	if ((file->idata & VFS_FILE_ATTR_EXTERNAL) != 0) {
		if (file->vp == NULL)
			return vfs_get_file_closed_error(file);
		mnt = file->vp->core.mnt;
		if (*mnt->fumount->devState != 1) {
			vfsUnlockFd(file);
			ret = vfs_wait_fumount(mnt);
			vfsLockFd(file);
			if (ret < 0)
				return ret;
		}
	}
	if (state != 0) {
		while (state == FDLOCK_STATE_BUSY) {
			ret = vfsWaitFd(file);
			if (ret < 0)
				return ret;
			state = fdlock->state;
		}
		if (state == FDLOCK_STATE_UMOUNTING) {
			if (closing == 0)
				return SCE_ERROR_ERRNO_ENODEV;
			return vfs_get_file_closed_error(file);
		}
		SCE_DBG_ASSERT(state == 0, 1898);
	}
	if (closing != 0)
		return vfs_get_file_closed_error(file);
	return vfs_get_file_state_error(file);
}

/* name unknown: marks a process's open file suspended */
int vfs_suspend_fd(SceUID guid)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(guid, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	if ((file->state == SCE_VFS_FILE_STATE_DUMMY || (ret = vfs_check_fd_status(file, 1)) == 0) &&
	    (file->idata & VFS_FILE_ATTR_INTERNAL) != 0)
		file->state = SCE_VFS_FILE_STATE_UNK;
	vfsUnlockFd(file);
	vfs_release_file(guid);
	return ret;
}

int vfs_process_suspend(SceUID pid, int num)
{
	SceUID *puid;
	SceSize got;
	SceUID guid;
	int ret, err, i;

	puid = ksceKernelAllocHeapMemory(g_vfsNcacheHeap, num * sizeof(SceUID));
	if (puid == NULL) {
		SCE_DBG_PRINTF(0xf, 2779, "no memory ... 0x%x\n", num * sizeof(SceUID));
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, puid, num, &got);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 2791, "scePUIDGetUIDVectorByClass failed ... 0x%x %d\n", ret, ret);
		ksceKernelFreeHeapMemory(g_vfsNcacheHeap, puid);
		return ret;
	}
	if (num != (int)got) {
		SCE_DBG_PRINTF(0xf, 2798, "unexpected value %d(fd_num) != %d(num)\n", num, got);
		num = got;
	}
	for (i = 0; i < (int)got; i++) {
		guid = kscePUIDtoGUID(pid, puid[i]);
		if (guid < 0) {
			SCE_DBG_PRINTF(0xf, 2807, "PUIDtoGUID failed err= %d 0x%x puid=0x%x\n", guid,
				       guid, puid[i]);
			continue;
		}
		err = vfs_suspend_fd(guid);
		if (err != 0)
			SCE_DBG_PRINTF(0xf, 2815, "retval=0x%x\n", err);
	}
	ksceKernelFreeHeapMemory(g_vfsNcacheHeap, puid);
	return ret;
}

/* name unknown: sets or clears bits of a file's attributes */
int vfs_change_file_attr(SceUID guid, SceUInt32 bits, int set)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(guid, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	if (file->state == SCE_VFS_FILE_STATE_DUMMY || (ret = vfs_check_fd_status(file, 0)) == 0) {
		if (set == 1)
			file->idata |= bits;
		else
			file->idata &= ~bits;
	}
	vfsUnlockFd(file);
	vfs_release_file(guid);
	return ret;
}

/* name unknown: wakes the threads waiting for a mount's device */
int vfs_signal_fumount(SceVfsMount *mnt)
{
	return ksceKernelSetEventFlag(*mnt->fumount->activationEvf, 1);
}

int vfs_prepare_for_resume(void)
{
	SceVfsMount *mnt;
	int ret = 0, err;

	vfs_lock_mnt_list();
	for (mnt = g_mntList; mnt != NULL; mnt = mnt->mnt_list_next) {
		if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) == 0)
			continue;
		err = ksceKernelClearEventFlag(*mnt->fumount->activationEvf, 0);
		if (err < 0) {
			SCE_DBG_PRINTF(0xf, 3278,
				       "vfs_prepare_dev_activation failed ... 0x%x mnt=%p %s\n", err,
				       mnt, mnt->mnt_data->assign_name);
			ret = err;
		}
	}
	vfs_unlock_mnt_list();
	return ret;
}

/* name unknown: creates a fd lock */
int vfs_create_fdlock(struct FdLock *fdlock)
{
	SceUID uid;

	uid = ksceKernelCreateMutex("SceVfsFdLock", 0, 0, NULL);
	if (uid < 0)
		return uid;
	fdlock->mutex = uid;
	uid = ksceKernelCreateCond("SceVfsFdCond", 0, uid, NULL);
	if (uid < 0) {
		ksceKernelDeleteMutex(fdlock->mutex);
		fdlock->mutex = -1;
		return uid;
	}
	fdlock->cond = uid;
	fdlock->state = 0;
	fdlock->waitNum = 0;
	return 0;
}

/* name unknown: whether the calling thread is a Common Dialog thread */
int vfs_is_cdialog_thread(void)
{
	return SceThreadmgrForDriver_1096042E(getIofilemgrTls()->requesterThreadId);
}

/* name unknown: creates a file's UID object, in the caller's process */
int vfs_create_file_object(SceUIDVfsFileObject **obj, const char *name)
{
	UIDCreateOpt opt;
	SceUID pid;

	pid = ksceKernelGetProcessIdFromTLS();
	switch (pid) {
	case SCE_KERNEL_PROCESS_ID:
	case SCE_KERNEL_DUMMY_PROCESS_GAME_ID:
	case SCE_KERNEL_DUMMY_PROCESS_SYSTEM_ID:
		return ksceGUIDKernelCreate(&g_vfsFileClass, name, (SceObjectBase **)obj);
	}
	opt.pid = pid;
	opt.object = NULL;
	opt.heapGuid = 0;
	opt.heapKind = 0;
	opt.attr = 0;
	opt.ownerPid = 0;
	opt.cls = NULL;
	opt.flags = UID_CREATE_OPT_PROCESS_HEAP | UID_CREATE_OPT_TYPE;
	opt.type = 3;
	if (vfs_is_cdialog_thread() != 0)
		opt.heapKind = 1;
	else
		opt.heapKind = 0;
	return ksceGUIDKernelCreateWithOpt(&g_vfsFileClass, name,
		(SceGUIDKernelCreateOpt *)&opt, (SceObjectBase **)obj);
}

SceUID vfs_allocate_file(SceVfsVnode *vp, SceVfsFile **file, const char *name)
{
	SceUIDVfsFileObject *obj;
	SceVfsMount *mnt = vp->core.mnt;
	SceVfsMountData *data;
	char objName[0x1f + 1];
	SceUID uid;

	data = mnt->mnt_data;
	SCE_DBG_ASSERT_CHECK(vp->vdlock.owner_id == ksceKernelGetThreadId(), 1682);
	if (name == NULL) {
		strncpy(objName, "SceVfsFile", 0x1f);
	} else {
		snprintf(objName, 0x1f, "file %s %s", data->assign_name, name);
		objName[0x1f] = '\0';
	}
	uid = vfs_create_file_object(&obj, objName);
	if (uid < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1697, "sceGUIDKernelCreate failed ... %d 0x%x\n", uid,
				     uid);
		return uid;
	}
	*file = &obj->file;
	ksceKernelAtomicAddAndGet32(&mnt->opened_entry_num, 1);
	return uid;
}

SceUID vfs_allocate_file_dummy(SceVfsFile **file, const char *path)
{
	SceUIDVfsFileObject *obj;
	SceUID uid;

	uid = vfs_create_file_object(&obj, "SceVfsFile");
	if (uid < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1717, "sceGUIDKernelCreate failed ... %d 0x%x\n", uid,
				     uid);
		return uid;
	}
	*file = &obj->file;
	return uid;
}

int vfs_extend_vnode_buffer(int cur)
{
	if (cur != g_available_vnode_num)
		return 0;
	if (g_available_vnode_num == VFS_VNODE_MAX)
		return SCE_ERROR_ERRNO_ENOMEM;
	vfs_lock_vfs();
	if (cur == g_available_vnode_num)
		g_available_vnode_num += 0x40;
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 3401, "cur_val = %d, g_available_vnode_num = %d\n", cur,
			     g_available_vnode_num);
	vfs_unlock_vfs();
	return 0;
}

static inline __attribute__((always_inline)) void init_vnode(SceVfsVnode *vp, SceVfsMount *mnt,
							     SceVopTable *ops, SceUID heap)
{
	if (vp->core.state != 0) {
		vfs_dump_vnode(vp, 1);
		SCE_DBG_ASSERT_FUNC(0, 1275, "init_vnode");
	}
	memset(&vp->core, 0, 0x98);
	vp->core.state = SCE_VNODE_STATE_NEW;
	vp->core.ops = ops;
	vp->core.mnt_opt = mnt->opt;
	vp->core.mnt = mnt;
	vp->core.ref_count++;
	vp->core.allocator = heap;
	vfsLockMnt(mnt);
	vp->core.next = NULL;
	if (mnt->vnode_list != NULL)
		vp->core.next = mnt->vnode_list;
	mnt->vnode_list = vp;
	mnt->vnode_num++;
	vfsUnlockMnt(mnt);
}

int vfsGetNewVnode(SceVfsMount *mnt, SceVopTable *ops, int unused, SceVfsVnode **vpp)
{
	SceVfsVnode *vp = NULL;
	SceUID heap;
	int cur, ret;

	heap = mnt->allocator;
	if (heap == 0)
		heap = g_vfsNcacheHeap;
	do {
		while ((cur = g_total_vnode_num) < g_available_vnode_num) {
			if (ksceKernelAtomicCompareAndSet32(&g_total_vnode_num, cur, cur + 1) != cur)
				continue;
			vp = bufferPoolAlloc(&g_vnodePool);
			if (vp == NULL) {
				SCE_DBG_PRINTF(0xf, 1196, "can not allocate ... total vnode=%d\n",
					       g_total_vnode_num);
				break;
			}
			goto found;
		}
		if (vp != NULL)
			goto found;
		ret = vfs_reclaim_vnode(mnt, &vp);
		if (ret >= 0) {
			fini_vnode(vp);
			goto found;
		}
		ret = vfs_extend_vnode_buffer(g_available_vnode_num);
	} while (ret == 0);
	SCE_DBG_PRINTF(0xf, 1220, "Vnode shortage ... \n");
	SCE_PRINTF_WITH_INFO(0xf, 1221, "::0x10f::001::%d\n", g_total_vnode_num);
	return ret;
found:
	init_vnode(vp, mnt, ops, heap);
	*vpp = vp;
	return 0;
}

int vfs_get_available_vnode_num(void)
{
	return g_available_vnode_num;
}

int vfs_mount_initialize(SceUID heap)
{
	SceVfsMount *mnt, *end;

	mnt = ksceKernelAllocHeapMemory(heap, VFS_MOUNT_MAX * sizeof(SceVfsMount));
	if (mnt == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	g_freeMntListLock = 0;
	end = mnt + VFS_MOUNT_MAX;
	for (; mnt != end; mnt++) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 3436, "mnt=%p\n", mnt);
		if (g_freeMntList != NULL)
			mnt->mnt_list_next = g_freeMntList;
		g_freeMntList = mnt;
	}
	return 0;
}
