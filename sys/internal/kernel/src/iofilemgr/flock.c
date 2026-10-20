/*
 * SceIofilemgr: advisory file locks (Sony's flock.c).
 *
 * Each file id of a mount has one Flock entry, found through the mount's
 * FlockList, and the entries that share a file id share one FlockLock. A
 * process holds a lock through its tag: exclusive, shared, or reserved
 * (shared now, and the next to get it exclusive). lock_val keeps the
 * exclusive bit, which a waiting writer sets too, and two per shared holder.
 */

#include "iofilemgr_internal.h"

int g_flockInitialized;
SceKernelFastMutex g_flockMutex;
FlockList g_flockList[FLOCK_LIST_MAX] __attribute__((section(".data"))) = {0};

static inline __attribute__((always_inline)) FlockTag *get_tag(FlockLock *lock, SceUID pid)
{
	FlockTag *tag = lock->tag;
	int i;

	for (i = 0; i < FLOCK_TAG_MAX; i++) {
		if (tag[i].pid == pid)
			return &lock->tag[i];
	}
	for (i = 0; i < FLOCK_TAG_MAX; i++) {
		if (tag[i].pid == -1) {
			lock->tag[i].pid = pid;
			lock->tag[i].type = FLOCK_TAG_NONE;
			lock->tag[i].ref_count = 0;
			return &lock->tag[i];
		}
	}
	return NULL;
}

/* drops a process's file from its tag: -2 while the process still holds the lock */
static int release_lock_tag(FlockLock *lock, SceUID pid)
{
	int i;

	for (i = 0; i < FLOCK_TAG_MAX; i++) {
		if (lock->tag[i].pid != pid)
			continue;
		if (lock->tag[i].type != FLOCK_TAG_NONE)
			return -2;
		if (--lock->tag[i].ref_count != 0)
			return 0;
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 330, "RELEASE tag PID = 0x%08x lock =%p\n", pid, lock);
		lock->tag[i].pid = -1;
		lock->tag[i].type = FLOCK_TAG_FREE;
		return 0;
	}
	return 0;
}

static struct Flock *alloc_lock_entry(void)
{
	struct Flock *flock;
	FlockLock *lock;

	flock = sceKernelAllocHeapMemory(g_ioVfsHeapUid, sizeof(*flock));
	if (flock == NULL)
		return NULL;
	lock = sceKernelAllocHeapMemory(g_ioVfsHeapUid, sizeof(*lock));
	if (lock == NULL) {
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, flock);
		return NULL;
	}
	flock->lock = lock;
	return flock;
}

static FlockList *get_lock_list(SceVfsMount *mnt)
{
	int i;

	for (i = 0; i < FLOCK_LIST_MAX; i++) {
		if (g_flockList[i].mnt == mnt)
			return &g_flockList[i];
	}
	for (i = 0; i < FLOCK_LIST_MAX; i++) {
		if (g_flockList[i].mnt == NULL) {
			g_flockList[i].mnt = mnt;
			g_flockList[i].head = NULL;
			return &g_flockList[i];
		}
	}
	for (i = 0; i < FLOCK_LIST_MAX; i++) {
		if (g_flockList[i].head == NULL) {
			g_flockList[i].mnt = mnt;
			g_flockList[i].head = NULL;
			return &g_flockList[i];
		}
	}
	return NULL;
}

static inline __attribute__((always_inline)) void finalize_lock(struct Flock *flock)
{
	FlockLock *lock = flock->lock;
	int i;

	if (--lock->use_count == 0) {
		if (lock->lock_val != 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 973, "lock_val=0x%x\n", lock->lock_val);
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 974, "write_waiters=%d\n", lock->write_waiters);
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 975, "use_count=%d\n", lock->use_count);
			SCE_DBG_PRINTF_LEVEL(1, 0xf, 976, "reserved_waiter=%d\n", lock->reserved_waiter);
			for (i = 0; i < FLOCK_TAG_MAX; i++) {
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 981, "lock->tag[%d].pid=0x%x\n", i, lock->tag[i].pid);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 983, "lock->tag[%d].type=0x%x\n", i, lock->tag[i].type);
				SCE_DBG_PRINTF_LEVEL(1, 0xf, 985, "lock->tag[%d].ref_count=%d\n", i,
					       lock->tag[i].ref_count);
			}
			SCE_DBG_ASSERT(0, 987);
		}
		sceKernelFinalizeFastMutex(&lock->mutex);
		sceKernelDeleteEventFlag(lock->waitQueue);
		sceKernelFreeHeapMemory(g_ioVfsHeapUid, flock->lock);
	}
}

/* drops a vnode's use of the entry for fid, and frees the entry with its last use */
static int release_lock_entry(struct Flock **head, SceUInt64 fid, SceUID pid, int releaseTag)
{
	struct Flock *flock, *prev;
	int ret;

	prev = NULL;
	for (flock = *head; flock != NULL; flock = flock->next) {
		if (flock->fid == fid)
			break;
		prev = flock;
	}
	if (flock == NULL)
		return 0;

	if (releaseTag) {
		ret = release_lock_tag(flock->lock, pid);
		if (ret < 0)
			return ret;
	}

	flock->refcount--;
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 1047, "refcount=%d\n", flock->refcount);
	ret = flock->refcount;
	if (ret != 0)
		return 0;
	if (prev == NULL)
		*head = flock->next;
	else
		prev->next = flock->next;
	finalize_lock(flock);
	sceKernelFreeHeapMemory(g_ioVfsHeapUid, flock);
	return ret;
}

int vfs_flock_init(void)
{
	int ret;
	int i;

	if (g_flockInitialized)
		return 0;

	ret = sceKernelInitializeFastMutex(&g_flockMutex, "SceIofileFlock", 0, NULL);
	if (ret < 0)
		return ret;
	for (i = 0; i < FLOCK_LIST_MAX; i++) {
		g_flockList[i].mnt = NULL;
		g_flockList[i].head = NULL;
	}
	g_flockInitialized = 1;
	return ret;
}

/* the vnode's lock, created with its first use; the file's process gets a tag */
static inline __attribute__((always_inline)) FlockLock *vfs_get_lock(SceVfsFile *file, SceVfsVnode *vp)
{
	struct Flock *flock;
	FlockList *list;
	FlockLock *lock;
	FlockTag *tag;
	SceUInt64 fid;
	SceUID evf;
	int i;

	if (lockFastMutex(&g_flockMutex) < 0)
		return NULL;

	flock = vp->core.flock;
	if (flock != NULL) {
		if (!file->hasFlockEnt) {
			tag = get_tag(flock->lock, file->pid);
			if (tag == NULL)
				goto no_tag;
			tag->ref_count++;
			file->hasFlockEnt = 1;
		}
		sceKernelUnlockFastMutex(&g_flockMutex);
		return flock->lock;
	}

	lock = NULL;
	list = get_lock_list(vp->core.mnt);
	if (list == NULL)
		goto out;

	fid = vnodeFid(vp);
	for (flock = list->head; flock != NULL; flock = flock->next) {
		if (flock->fid == fid)
			break;
	}
	if (flock == NULL) {
		flock = alloc_lock_entry();
		if (flock == NULL)
			goto out;
		flock->fid = fid;
		flock->refcount = 0;
		lock = flock->lock;
		evf = sceKernelCreateEventFlag("SceIofileFlockWaitQueue",
						SCE_KERNEL_EVF_ATTR_MULTI,
						0, NULL);
		if (evf >= 0) {
			lock->waitQueue = evf;
			if (sceKernelInitializeFastMutex(&lock->mutex, "SceIofileFlockLock",
							  0, NULL) < 0) {
				sceKernelDeleteEventFlag(evf);
			} else {
				lock->lock_val = 0;
				lock->write_waiters = 0;
				lock->reserved_waiter = 0;
				lock->reserved_pid = 0;
				lock->use_count = 1;
				for (i = 0; i < FLOCK_TAG_MAX; i++) {
					lock->tag[i].pid = -1;
					lock->tag[i].type = FLOCK_TAG_FREE;
				}
			}
		}
		flock->next = list->head;
		list->head = flock;
	}
	vp->core.flock = flock;
	flock->refcount++;
	SCE_DBG_PRINTF_LEVEL(2, 0xf, 1148, "vp=%p ref=%d\n", vp, flock->refcount);
	lock = flock->lock;
	if (!file->hasFlockEnt) {
		tag = get_tag(lock, file->pid);
		if (tag == NULL)
			goto no_tag;
		tag->ref_count++;
		file->hasFlockEnt = 1;
	}
out:
	if (sceKernelUnlockFastMutex(&g_flockMutex) < 0)
		SCE_DBG_ASSERT(0, 1167);
	return lock;

no_tag:
	sceKernelUnlockFastMutex(&g_flockMutex);
	return NULL;
}

static inline __attribute__((always_inline)) int lock_shared(SceVfsFile *file, FlockLock *lock, SceUID pid, int op)
{
	FlockTag *tag;
	int reserved, locked;
	int ret;
	SceUInt32 lockVal;

	reserved = 0;
	for (;;) {
		ret = lockFastMutex(&lock->mutex);
		if (ret < 0)
			return ret;
		tag = get_tag(lock, pid);
		if (tag == NULL) {
			ret = SCE_ERROR_ERRNO_ENOMEM;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
		if (tag->type == FLOCK_TAG_RS) {
			reserved = 1;
			SCE_DBG_ASSERT(pid == lock->reserved_pid, 367);
		}

		locked = 0;
		if ((lock->lock_val & FLOCK_LOCK_EX) == 0) {
			if (lock->reserved_pid == 0 || reserved) {
				if (tag->type == FLOCK_TAG_NONE) {
					tag->pid = pid;
					tag->type = FLOCK_TAG_SH;
					lock->lock_val += FLOCK_LOCK_SH;
				} else if (reserved) {
					SCE_DBG_ASSERT(tag->type == FLOCK_TAG_RS, 397);
					tag->type = FLOCK_TAG_SH;
					lock->reserved_pid = 0;
				} else {
					SCE_DBG_ASSERT(tag->type == FLOCK_TAG_SH, 403);
				}
				locked = 1;
			}
		} else {
			if (tag->type == FLOCK_TAG_SH) {
				SCE_DBG_ASSERT(lock->write_waiters > 0, 413);
				locked = 1;
			} else if (tag->type == FLOCK_TAG_EX) {
				if (lock->write_waiters == 0) {
					ret = sceKernelSetEventFlag(lock->waitQueue, FLOCK_WAKE_SH);
					if (ret < 0)
						goto out;
					lockVal = lock->lock_val & ~FLOCK_LOCK_EX;
					lock->lock_val = lockVal + FLOCK_LOCK_SH;
					tag->type = FLOCK_TAG_SH;
					SCE_DBG_ASSERT(lockVal == 0, 447);
					locked = 1;
				} else {
					/* the writers come first: unlock, then wait as a reader */
					tag->type = FLOCK_TAG_NONE;
					if (lock->reserved_waiter == 0)
						ret = sceKernelSetEventFlag(lock->waitQueue,
									     FLOCK_WAKE_EX);
					else
						ret = sceKernelSetEventFlag(lock->waitQueue,
									     FLOCK_WAKE_RS);
					if (ret < 0)
						goto out;
				}
			} else if (tag->type == FLOCK_TAG_RS) {
				tag->type = FLOCK_TAG_SH;
				lock->reserved_pid = 0;
				locked = 1;
			}
		}
		if (locked && file != NULL)
			file->isLocked = 1;

		if (sceKernelUnlockFastMutex(&lock->mutex) < 0)
			SCE_DBG_ASSERT(0, 486);
		if (locked)
			return 0;
		if (op & FLOCK_OP_NB)
			return SCE_KERNEL_ERROR_IO_WOULD_BLOCK;
		ret = sceKernelWaitEventFlag(lock->waitQueue, FLOCK_WAKE_SH,
					      SCE_KERNEL_EVF_WAITMODE_AND,
					      NULL, NULL);
		if (ret < 0)
			return ret;
	}

out:
	sceKernelUnlockFastMutex(&lock->mutex);
	return ret;
}

static inline __attribute__((always_inline)) int lock_exclusive(SceVfsFile *file, FlockLock *lock, SceUID pid, int op)
{
	FlockTag *tag;
	int isR;
	int ret;

	ret = lockFastMutex(&lock->mutex);
	if (ret < 0)
		return ret;
	tag = get_tag(lock, pid);
	if (tag == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}

	if (lock->lock_val == 0) {
		if (tag->type == FLOCK_TAG_NONE)
			tag->pid = pid;
		tag->type = FLOCK_TAG_EX;
		lock->lock_val = FLOCK_LOCK_EX;
		sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
		if (file != NULL)
			file->isLocked = 1;
		ret = 0;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}

	if (pid == lock->reserved_pid) {
		isR = 1;
	} else {
		isR = 0;
		if (lock->reserved_pid != 0)
			goto wait;
	}

	if (tag->type == FLOCK_TAG_NONE) {
		/* wait */
	} else if (tag->type == FLOCK_TAG_EX) {
		SCE_DBG_ASSERT(!isR, 565);
		SCE_DBG_ASSERT(pid == tag->pid, 568);
		if (file != NULL)
			file->isLocked = 1;
		ret = 0;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	} else if (tag->type == FLOCK_TAG_SH) {
		SCE_DBG_ASSERT(!isR, 579);
		if (lock->lock_val == FLOCK_LOCK_SH) {
			lock->lock_val = FLOCK_LOCK_EX;
			tag->type = FLOCK_TAG_EX;
			sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
			if (file != NULL)
				file->isLocked = 1;
			ret = 0;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
	} else if (tag->type == FLOCK_TAG_RS) {
		SCE_DBG_ASSERT(isR, 599);
		if (lock->lock_val == FLOCK_LOCK_SH) {
			lock->lock_val = FLOCK_LOCK_EX;
			tag->type = FLOCK_TAG_EX;
			lock->reserved_pid = 0;
			sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
			if (file != NULL)
				file->isLocked = 1;
			ret = 0;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
	} else {
		SCE_DBG_ASSERT(0, 623);
	}

wait:
	SCE_DBG_ASSERT(lock->lock_val > 0, 629);
	if (op & FLOCK_OP_NB) {
		ret = SCE_KERNEL_ERROR_IO_WOULD_BLOCK;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}
	lock->lock_val |= FLOCK_LOCK_EX;
	lock->write_waiters++;
	if (tag->type == FLOCK_TAG_SH || isR)
		lock->lock_val -= FLOCK_LOCK_SH;
	tag->type = FLOCK_TAG_NONE;
	if (file != NULL)
		file->isLocked = 0;
	sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
	if (isR) {
		SCE_DBG_ASSERT(lock->reserved_waiter == 0, 655);
		lock->reserved_waiter = 1;
	}
	if (sceKernelUnlockFastMutex(&lock->mutex) < 0)
		SCE_DBG_ASSERT(0, 661);

	if (isR)
		ret = sceKernelWaitEventFlag(lock->waitQueue, FLOCK_WAKE_RS,
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	else
		ret = sceKernelWaitEventFlag(lock->waitQueue, FLOCK_WAKE_EX,
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	if (ret < 0)
		return ret;
	ret = lockFastMutex(&lock->mutex);
	if (ret < 0)
		return ret;

	SCE_DBG_ASSERT(lock->lock_val == FLOCK_LOCK_EX, 691);
	SCE_DBG_ASSERT(lock->write_waiters >= 0, 693);
	lock->write_waiters--;
	tag->type = FLOCK_TAG_EX;
	if (isR) {
		lock->reserved_pid = 0;
		lock->reserved_waiter = 0;
	}
	if (file != NULL)
		file->isLocked = 1;
	if (sceKernelUnlockFastMutex(&lock->mutex) < 0)
		SCE_DBG_ASSERT(0, 709);
	return 0;
}

static inline __attribute__((always_inline)) int lock_reserved(SceVfsFile *file, FlockLock *lock, SceUID pid)
{
	FlockTag *tag;
	int ret;

	ret = lockFastMutex(&lock->mutex);
	if (ret < 0)
		return ret;
	tag = get_tag(lock, pid);
	if (tag == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}

	if (lock->reserved_pid != 0)
		goto busy;

	if (lock->lock_val == 0) {
		if (tag->type == FLOCK_TAG_NONE)
			tag->pid = pid;
		tag->type = FLOCK_TAG_RS;
		lock->reserved_pid = pid;
		lock->lock_val = FLOCK_LOCK_SH;
		sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
		if (file != NULL)
			file->isLocked = 1;
		ret = 0;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}

	if (tag->type == FLOCK_TAG_NONE) {
		if ((lock->lock_val & FLOCK_LOCK_EX) == 0) {
			tag->pid = pid;
			tag->type = FLOCK_TAG_RS;
			lock->lock_val += FLOCK_LOCK_SH;
			lock->reserved_pid = pid;
			if (file != NULL)
				file->isLocked = 1;
			ret = 0;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
	} else if (tag->type == FLOCK_TAG_EX) {
		SCE_DBG_ASSERT(pid == tag->pid, 765);
		if (lock->write_waiters == 0) {
			ret = sceKernelSetEventFlag(lock->waitQueue, FLOCK_WAKE_SH);
			if (ret < 0) {
				sceKernelUnlockFastMutex(&lock->mutex);
				return ret;
			}
		}
		tag->type = FLOCK_TAG_RS;
		lock->reserved_pid = pid;
		lock->lock_val = FLOCK_LOCK_SH;
		if (file != NULL)
			file->isLocked = 1;
		/* Sony's code returns with lock->mutex still locked */
		return 0;
	} else if (tag->type == FLOCK_TAG_SH) {
		if (lock->lock_val == FLOCK_LOCK_SH) {
			lock->reserved_pid = pid;
			tag->type = FLOCK_TAG_RS;
			sceKernelClearEventFlag(lock->waitQueue, ~FLOCK_WAKE_SH);
			if (file != NULL)
				file->isLocked = 1;
			ret = 0;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
		if ((lock->lock_val & FLOCK_LOCK_EX) == 0) {
			tag->type = FLOCK_TAG_RS;
			lock->reserved_pid = pid;
			if (file != NULL)
				file->isLocked = 1;
			ret = 0;
			sceKernelUnlockFastMutex(&lock->mutex);
			return ret;
		}
	} else if (tag->type == FLOCK_TAG_RS) {
		ret = lock->reserved_pid;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	} else {
		SCE_DBG_ASSERT(lock->reserved_pid, 825);
	}
	SCE_DBG_ASSERT(lock->lock_val > 0, 843);
busy:
	ret = SCE_ERROR_ERRNO_EBUSY;
	sceKernelUnlockFastMutex(&lock->mutex);
	return ret;
}

static inline __attribute__((always_inline)) int unlock(SceVfsFile *file, FlockLock *lock, SceUID pid)
{
	FlockTag *tag;
	int ret;

	ret = lockFastMutex(&lock->mutex);
	if (ret < 0)
		return ret;
	tag = get_tag(lock, pid);
	if (tag == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	}

	if (tag->type == FLOCK_TAG_NONE) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		sceKernelUnlockFastMutex(&lock->mutex);
		return ret;
	} else if (tag->type == FLOCK_TAG_EX) {
		if (lock->write_waiters == 0) {
			ret = sceKernelSetEventFlag(lock->waitQueue, FLOCK_WAKE_SH);
			if (ret < 0)
				goto out;
			lock->lock_val &= ~FLOCK_LOCK_EX;
		} else {
			ret = sceKernelSetEventFlag(lock->waitQueue,
						     lock->reserved_waiter != 0 ? FLOCK_WAKE_RS
										: FLOCK_WAKE_EX);
			if (ret < 0)
				goto out;
		}
	} else if (tag->type == FLOCK_TAG_SH) {
		lock->lock_val -= FLOCK_LOCK_SH;
		if (lock->lock_val == FLOCK_LOCK_EX)
			ret = sceKernelSetEventFlag(lock->waitQueue,
						     lock->reserved_waiter != 0 ? FLOCK_WAKE_RS
										: FLOCK_WAKE_EX);
		else
			ret = 0;
	} else if (tag->type == FLOCK_TAG_RS) {
		lock->reserved_pid = 0;
		lock->lock_val -= FLOCK_LOCK_SH;
		SCE_DBG_ASSERT(lock->reserved_waiter == 0, 939);
		ret = sceKernelSetEventFlag(lock->waitQueue,
					     lock->lock_val == FLOCK_LOCK_EX ? FLOCK_WAKE_EX
									     : FLOCK_WAKE_SH);
	} else {
		SCE_DBG_ASSERT(0, 952);
	}
	tag->type = FLOCK_TAG_NONE;
	if (file != NULL)
		file->isLocked = 0;
out:
	sceKernelUnlockFastMutex(&lock->mutex);
	return ret;
}

int vfs_flock(SceVfsFile *file, int op, SceUID pid, SceVfsVnode *vp)
{
	FlockLock *lock;

	lock = vfs_get_lock(file, vp);
	if (lock == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;

	switch (op & FLOCK_OP_MASK) {
	case FLOCK_OP_RS:
		return lock_reserved(file, lock, pid);
	case FLOCK_OP_SH:
		return lock_shared(file, lock, pid, op);
	case FLOCK_OP_EX:
		return lock_exclusive(file, lock, pid, op);
	case FLOCK_OP_UN:
		return unlock(file, lock, pid);
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
}

/* a file's close: drops its use of the vnode's lock, unlocking what its process holds */
int vfs_flock_close(SceVfsFile *file, SceUID pid, SceVfsVnode *vp)
{
	FlockList *list;
	int ret;

	while (vp->core.flock != NULL) {
		ret = lockFastMutex(&g_flockMutex);
		if (ret < 0) {
			if (ret == -2)
				goto unlock_file;
		} else {
			list = get_lock_list(vp->core.mnt);
			if (list != NULL)
				ret = release_lock_entry(&list->head, vnodeFid(vp), pid,
							 file->hasFlockEnt);
			sceKernelUnlockFastMutex(&g_flockMutex);
			if (ret == -2) {
unlock_file:
				/* the process still holds the lock: unlock it, then try again */
				vfs_flock(file, FLOCK_OP_UN, pid, vp);
			} else if (ret == 0) {
				return 0;
			}
		}
	}
	return 0;
}

/* dst shares src's lock under its own file id */
int vfs_flock_dup(SceVfsVnode *src, SceVfsVnode *dst)
{
	struct Flock *flock, *dup, *head;
	SceInt16 count;
	FlockList *list;
	FlockLock *lock;
	SceUInt64 fid;
	int ret;

	if (src->core.flock == NULL)
		return SCE_ERROR_ERRNO_EINVAL;

	ret = lockFastMutex(&g_flockMutex);
	if (ret < 0)
		return ret;

	list = get_lock_list(src->core.mnt);
	if (list == NULL) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		flock = src->core.flock;
		fid = vnodeFid(dst);
		dup = alloc_lock_entry();
		if (dup == NULL) {
			ret = SCE_ERROR_ERRNO_ENOMEM;
		} else {
			lock = flock->lock;
			dup->refcount = 1;
			dup->fid = fid;
			count = lock->use_count;
			/* the store keeps its place before list->head's load (a pointer store through void **) */
			*(void **)&dup->lock = lock;
			head = list->head;
			lock->use_count = count + 1;
			dup->next = head;
			list->head = dup;
			dst->core.flock = dup;
		}
	}
	if (sceKernelUnlockFastMutex(&g_flockMutex) < 0)
		SCE_DBG_ASSERT(0, 1313);
	return ret;
}

/* drops a process's file from the tag of the vnode's lock */
int vfs_flock_release(SceUID pid, SceVfsVnode *vp)
{
	struct Flock *flock;
	FlockList *list;
	int ret;

	ret = lockFastMutex(&g_flockMutex);
	if (ret < 0)
		return ret;

	flock = vp->core.flock;
	if (flock != NULL) {
		ret = release_lock_tag(flock->lock, pid);
	} else {
		list = get_lock_list(vp->core.mnt);
		if (list == NULL) {
			sceKernelUnlockFastMutex(&g_flockMutex);
			return SCE_ERROR_ERRNO_ENOENT;
		}
		for (flock = list->head; flock != NULL; flock = flock->next) {
			if (flock->fid == vnodeFid(vp))
				break;
		}
		if (flock == NULL)
			ret = SCE_ERROR_ERRNO_ENOENT;
		else
			ret = release_lock_tag(flock->lock, pid);
	}
	sceKernelUnlockFastMutex(&g_flockMutex);
	return ret;
}

/* drops a vnode's use of its lock entry */
int vfs_release_lock_entry(SceVfsVnode *vp)
{
	FlockList *list;
	int ret;

	ret = lockFastMutex(&g_flockMutex);
	if (ret < 0)
		return ret;

	list = get_lock_list(vp->core.mnt);
	if (list != NULL)
		ret = release_lock_entry(&list->head, vnodeFid(vp), 0, 0);
	sceKernelUnlockFastMutex(&g_flockMutex);
	return ret;
}
