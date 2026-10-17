/*
 * SceIofilemgr: the virtual file system (Sony's vfs.c): name cache, path lookup,
 * mount and unmount, and the vfs* entry points behind the sceIo* calls.
 */

#include "iofilemgr_internal.h"

SceUID g_vfsNcacheHeap __attribute__((section(".data"))) = 0;

/* a mount whose last reference went away is torn down, and its parent mount with it */
void vfs_umount_cleanup(SceVfsMount *mnt)
{
	SceVfsMount *pmnt;

	if ((int)mnt->opened_entry_num > 0) {
		SCE_DBG_PRINTF(0xf, 6939, "NOT CLEANUP mnt=%p mnt->openedEntryNum=%d\n", mnt,
			       mnt->opened_entry_num);
		SCE_DBG_ASSERT(0, 6940);
	}
	if (mnt->fs_type == SCE_VFS_FS_TYPE_BLKDEV || mnt->fs_type == SCE_VFS_FS_TYPE_CHRDEV)
		pmnt = NULL;
	else if (mnt->fs_type == SCE_VFS_FS_TYPE_HOSTFS)
		pmnt = NULL;
	else if (mnt->mnt_vnode != NULL)
		pmnt = mnt->mnt_vnode->core.mnt;
	else
		pmnt = NULL;
	vfs_release_vfs(mnt->mnt_vfs_inf->vfs_name);
	if (vfs_finalize_mount(mnt) != 0)
		goto unlock;
	vfs_lock_mnt_list();
	vfs_remove_mnt_list(mnt);
	vfs_unlock_mnt_list();
	vfs_free_mount(mnt);
	if (pmnt == NULL)
		return;
	vfsLockMnt(pmnt);
	vfs_remove_mntedon_list(pmnt, mnt);
	pmnt->mnt_ref_count--;
	if (pmnt->state == VFS_MOUNT_STATE_UMOUNTING && pmnt->mnt_ref_count == 0) {
		vfs_umount_cleanup(pmnt);
		return;
	}
	mnt = pmnt;
unlock:
	vfsUnlockMnt(mnt);
}

/* name unknown: the root vnode of a path's device, referenced and locked */
int vfs_get_root_vnode(const VfsPathElem *elem, int num, int *depth, SceUInt32 flags,
			      SceVfsVnode **vpp)
{
	RfsRootInfo root;
	SceVfsMount *mnt;
	SceVfsMount *vnodeMnt;
	SceVfsVnode *vp;
	int rootDepth;
	int ret;

	flags &= 0xfffff;
	for (;;) {
		if (flags == VFS_LOOKUP_TRYLOCK) {
			if (rfs_trylock() == SCE_KERNEL_ERROR_FAST_MUTEX_FAILED_TO_OWN) {
				ret = VFS_LOOKUP_AGAIN;
				goto out;
			}
		} else {
			rfs_lock();
		}
		if (rfs_get_root(elem, num, &root, &rootDepth) != 0) {
			rfs_unlock();
			if (flags == 0x6000)
				goto noEntry;
			goto noDevice;
		}
		vp = root.vp;
		mnt = vp->core.mnt;
		vfsLockMnt(mnt);
		rfs_unlock();
		if (mnt->state != VFS_MOUNT_STATE_MOUNTED) {
			vfsUnlockMnt(mnt);
			if (flags == 0x6000)
				goto noEntry;
			goto noDevice;
		}
		mnt->mnt_ref_count++;
		vfsUnlockMnt(mnt);
		*depth = ++rootDepth;
		if (flags == VFS_LOOKUP_TRYLOCK) {
			if (lock_vdlock(vp) == SCE_ERROR_ERRNO_EBUSY) {
				vfsLockMnt(mnt);
				mnt->mnt_ref_count--;
				vfsUnlockMnt(mnt);
				ret = VFS_LOOKUP_AGAIN;
				goto out;
			}
		} else {
			vfsLockVnode(vp);
		}
		vnodeMnt = vp->core.mnt;
		if (vnodeMnt != mnt) {
			vfsUnlockVnode(vp);
			vfsLockMnt(mnt);
			if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 1) {
				vfs_umount_cleanup(mnt);
				goto noDevice;
			}
			mnt->mnt_ref_count--;
			vfsUnlockMnt(mnt);
			goto noDevice;
		}
		if (mnt->state == VFS_MOUNT_STATE_MOUNTED) {
			vp->core.ref_count++;
			*vpp = vp;
			ret = 0;
			goto out;
		}
		vfsLockMnt(mnt);
		if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 1) {
			vfs_umount_cleanup(mnt);
			goto noDevice;
		}
		vnodeMnt->mnt_ref_count = mnt->mnt_ref_count - 1;
		vfsUnlockMnt(vnodeMnt);
	}
noEntry:
	ret = SCE_ERROR_ERRNO_ENOENT;
	goto out;
noDevice:
	ret = SCE_ERROR_ERRNO_ENODEV;
out:
	return ret;
}

/* drops a vnode reference and the mount reference that came with it */
void vfs_decrement_vp_mnt_ref(SceVfsVnode *vp)
{
	SceVfsMount *mnt = vp->core.mnt;

	vp->core.ref_count--;
	vfsUnlockVnode(vp);
	vfsLockMnt(mnt);
	if ((int)--mnt->mnt_ref_count < 0) {
		SCE_DBG_PRINTF(0xf, 219, "mnt->mntRefCount = %d\n", mnt->mnt_ref_count);
		SCE_DBG_ASSERT(0, 220);
	}
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
		vfs_umount_cleanup(mnt);
	else
		vfsUnlockMnt(mnt);
}

static inline __attribute__((always_inline)) void vfs_decrement_vp_mnt_ref_inline(SceVfsVnode *vp)
{
	SceVfsMount *mnt = vp->core.mnt;

	vp->core.ref_count--;
	vfsUnlockVnode(vp);
	vfsLockMnt(mnt);
	if ((int)--mnt->mnt_ref_count < 0) {
		SCE_DBG_PRINTF_FUNC(0xf, 219, "vfs_decrement_vp_mnt_ref", "mnt->mntRefCount = %d\n",
				    mnt->mnt_ref_count);
		SCE_DBG_ASSERT_FUNC(0, 220, "vfs_decrement_vp_mnt_ref");
	}
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
		vfs_umount_cleanup(mnt);
	else
		vfsUnlockMnt(mnt);
}

/* name unknown: gives a root vnode its name cache entry */
int vfs_enter_root_ncache(SceVfsVnode *vp, const VfsPathElem *elem)
{
	struct Ncache *nc;

	nc = vfs_alloc_ncache(vp->core.allocator, elem);
	if (nc == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	vfs_lock_ncache();
	vfs_set_ncache_entry(nc, NULL, vp, elem, NCACHE_STATE_VALID);
	vfs_insert_ncache_list(nc);
	vfs_unlock_ncache();
	return 0;
}

void remove_negative_list(struct Ncache *nc)
{
	struct Ncache *prev, *cur;

	if (g_negativeNcacheList == NULL)
		return;
	if (g_negativeNcacheList == nc) {
		g_negativeNcacheList = nc->negativeNext;
	} else {
		cur = g_negativeNcacheList;
		do {
			prev = cur;
			cur = prev->negativeNext;
			if (cur == nc)
				break;
		} while (cur != NULL);
		prev->negativeNext = nc->negativeNext;
	}
	nc->negativeNext = NULL;
	g_negativeNcacheNum--;
}

int clear_ncache(struct Ncache *nc, int recursive)
{
	struct Ncache *child, *next, *prev, *cur, *ncdd;
	SceVfsVnode *vp;
	int ret;

	if (nc == NULL)
		return 0;
	vfs_lock_ncache();
	if (nc->state == NCACHE_STATE_NEGATIVE) {
		vp = NULL;
		remove_negative_list(nc);
		ret = 0;
	} else {
		vp = nc->vp;
		if (vp->core.ncache != nc) {
			SCE_DBG_PRINTF(0xf, 1646, "vp->ncache %p != nc %p %s vp=%p\n", vp->core.ncache,
				       nc, nc->name, vp);
			vfs_dump_vnode(vp, 0);
		}
		child = nc->child;
		if (child == NULL) {
			ret = 0;
		} else {
			if (nc->state != NCACHE_STATE_WHITEOUT)
				nc->state = NCACHE_STATE_INVALID;
			if (recursive != 1) {
				vfs_unlock_ncache();
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 1678, "nc %s is just changed to INVALID state\n",
						     nc->name);
				return 0;
			}
			do {
				next = child->next;
				ret = clear_ncache(child, 1);
				if (ret < 0)
					SCE_DBG_PRINTF(0xf, 1671, "nc %s :: my child could not be cleaned\n",
						       nc->name);
				child = next;
			} while (next != NULL);
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 1675, "children are all cleared\n");
		}
	}
	vfs_remove_ncache_list(nc);
	ncdd = nc->ncdd;
	if (ncdd == NULL) {
		if (nc->vp->core.type > (SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_REG) || nc->state == NCACHE_STATE_WHITEOUT) {
			vfs_free_ncache(nc);
			vp->core.ncache = NULL;
		}
		goto out;
	}
	cur = ncdd->child;
	if (cur == NULL) {
		if (nc->state != NCACHE_STATE_WHITEOUT) {
			SCE_DBG_PRINTF(0xf, 1709, "nc %s does`t exist ... \n", nc->name);
			SCE_DBG_PRINTF(0xf, 1710, "ncdd is %s \n", nc->ncdd->name);
			ret = SCE_ERROR_ERRNO_ENOENT;
			goto out;
		}
	} else {
		if (cur == nc) {
			ncdd->child = nc->next;
		} else {
			do {
				prev = cur;
				cur = prev->next;
				if (cur == nc)
					break;
			} while (cur != NULL);
			prev->next = nc->next;
		}
		ret = 0;
		nc->next = NULL;
		if (nc->state == NCACHE_STATE_WHITEOUT) {
			nc->ncdd = NULL;
			goto out;
		}
		vfs_free_ncache(nc);
		if (vp != NULL) {
			vp->core.ncache = NULL;
			goto out;
		}
	}
	ret = 0;
out:
	vfs_unlock_ncache();
	return ret;
}

static inline __attribute__((always_inline)) void insert_negative_list(struct Ncache *nc)
{
	struct Ncache *last, *prev, *cur;

	if (nc->state != NCACHE_STATE_NEGATIVE) {
		SCE_DBG_PRINTF_FUNC(0xf, 6651, "insert_negative_list", "not negative entry %p %s\n",
				    nc, nc->name);
		SCE_DBG_ASSERT_FUNC(0, 6652, "insert_negative_list");
	}
	if (g_negativeNcacheNum < NCACHE_NEGATIVE_MAX) {
		if (g_negativeNcacheList != NULL)
			nc->negativeNext = g_negativeNcacheList;
		g_negativeNcacheNum++;
		g_negativeNcacheList = nc;
		return;
	}
	last = g_negativeNcacheList;
	if (last == NULL) {
		SCE_DBG_PRINTF_FUNC(0xf, 6668, "insert_negative_list",
				    "can not find old one but entry num is %d\n", g_negativeNcacheNum);
		if (g_negativeNcacheList == NULL) {
			g_negativeNcacheList = nc;
			return;
		}
	} else {
		prev = NULL;
		cur = g_negativeNcacheList;
		do {
			last = cur;
			if (last->negativeNext == NULL)
				break;
			prev = last;
			cur = last->negativeNext;
		} while (1);
		last->negativeNext = NULL;
		prev->negativeNext = NULL;
	}
	nc->negativeNext = g_negativeNcacheList;
	if (last != NULL) {
		g_negativeNcacheList = nc;
		clear_ncache(last, 0);
	} else {
		g_negativeNcacheList = nc;
	}
}

int enter_ncache(SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem, int state, int decode)
{
	struct Ncache *nc, *cur, *last;
	VfsPathElem name;
	const char *path2, *path3;
	SceSize len;
	SceUID heap;
	char *buf;
	int ret;

	if (dvp->core.ncache == NULL) {
		SCE_DBG_PRINTF(0xf, 1410, "\n");
		return SCE_ERROR_ERRNO_ENOENT;
	}
	if (vp != NULL) {
		if (vp->core.ncache != NULL)
			return SCE_ERROR_ERRNO_EPERM;
		heap = vp->core.allocator;
	} else {
		heap = g_vfsNcacheHeap;
	}
	buf = NULL;
	if (decode != 0) {
		buf = vfs_path_buff_alloc();
		if (buf == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ret = ksceVfsOpDecodePathElem(dvp->core.mnt, elem->name, &path2, &path3, buf,
					      VFS_PATH_BUFF_SIZE, &len);
		if (ret < 0) {
			if (ret != SCE_ERROR_ERRNO_EUNSUP) {
				vfs_path_buff_free(buf);
				return ret;
			}
			__builtin_memcpy(&name, elem, sizeof(name));
		} else {
			name.name = buf;
			name.len = len;
		}
	} else {
		__builtin_memcpy(&name, elem, sizeof(name));
	}
	nc = vfs_alloc_ncache(heap, &name);
	if (nc == NULL) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 1463, "can not allocate ncache = %p, %s\n", vp, elem->name);
		vfs_path_buff_free(buf);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	vfs_lock_ncache();
	if (dvp->core.ncache == NULL || (dvp->core.ncache->state & NCACHE_STATE_VALID) == 0) {
		vfs_free_ncache_heap(nc, heap);
		vfs_unlock_ncache();
		SCE_DBG_PRINTF(0xf, 1475, "\n");
		vfs_path_buff_free(buf);
		return SCE_ERROR_ERRNO_ENOENT;
	}
	vfs_set_ncache_entry(nc, dvp, vp, &name, state);
	cur = dvp->core.ncache->child;
	if (cur == NULL) {
		dvp->core.ncache->child = nc;
	} else {
		do {
			last = cur;
			cur = last->next;
		} while (cur != NULL);
		last->next = nc;
	}
	vfs_insert_ncache_list(nc);
	if (state == NCACHE_STATE_NEGATIVE)
		insert_negative_list(nc);
	vfs_unlock_ncache();
	vfs_path_buff_free(buf);
	return 0;
}

/* a struct copy of a name cache entry, taken under the ncache lock */
static inline __attribute__((always_inline)) int clear_negative_cache(SceVfsVnode *dvp,
								      const VfsPathElem *name,
								      const char *orgName)
{
	struct Ncache *ncdd, *nc, *prev, *cur;

	ncdd = dvp->core.ncache;
	vfs_lock_ncache();
	if (ncdd == NULL || ncdd->state == NCACHE_STATE_NEGATIVE)
		goto fail;
	for (nc = ncdd->child; nc != NULL; nc = nc->next) {
		if (name->len != nc->nameLength || strncmp(name->name, nc->name, name->len) != 0)
			continue;
		remove_negative_list(nc);
		vfs_remove_ncache_list(nc);
		if (ncdd != nc->ncdd) {
			SCE_DBG_PRINTF_FUNC(0xf, 8616, "clear_negative_cache", "pnc=%p nc=%p\n",
					    nc->ncdd, ncdd);
			SCE_PANIC_FUNC(8617, "clear_negative_cache");
		}
		cur = ncdd->child;
		if (cur == NULL) {
			SCE_DBG_PRINTF_FUNC(0xf, 8626, "clear_negative_cache",
					    "nnc %s does`t exist ... \n", nc->name);
			SCE_DBG_PRINTF_FUNC(0xf, 8627, "clear_negative_cache", "ncdd is %s \n",
					    nc->ncdd->name);
			goto fail;
		}
		if (cur == nc) {
			ncdd->child = nc->next;
		} else {
			do {
				prev = cur;
				cur = prev->next;
				if (cur == nc)
					break;
			} while (cur != NULL);
			prev->next = nc->next;
		}
		nc->next = NULL;
		vfs_free_ncache(nc);
		goto fail;
	}
fail:
	vfs_unlock_ncache();
	SCE_DBG_PRINTF_FUNC(0xf, 1276, "vfs_lookup", "can not clear negative %s\n", orgName);
	return SCE_ERROR_ERRNO_ENOENT;
}

int vfs_lookup(SceVfsVnode *dvp, const VfsPathElem *elem, const char *path, SceUInt32 flags,
	       SceVfsVnode **vpp)
{
	struct Ncache *ncdd, *nc;
	struct Ncache copy;
	VfsLookupPath lpath;
	VfsPathElem name;
	const char *path2, *path3;
	SceVfsVnode *vp;
	SceSize len;
	char *buf;
	int ret;

	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(dvp) == 1, 1187);
	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = ksceVfsOpDecodePathElem(dvp->core.mnt, elem->name, &path2, &path3, buf,
				      VFS_PATH_BUFF_SIZE, &len);
	if (ret < 0) {
		if (ret != SCE_ERROR_ERRNO_EUNSUP) {
			vfs_path_buff_free(buf);
			return ret;
		}
		__builtin_memcpy(&name, elem, sizeof(name));
	} else {
		name.name = buf;
		name.len = len;
	}
	vfs_lock_ncache();
	ncdd = dvp->core.ncache;
	if (ncdd != NULL && ncdd->state != NCACHE_STATE_NEGATIVE) {
		for (nc = ncdd->child; nc != NULL; nc = nc->next) {
			if (name.len != nc->nameLength || strncmp(name.name, nc->name, name.len) != 0)
				continue;
			__builtin_memcpy(&copy, nc, sizeof(copy));
			vfs_unlock_ncache();
			vp = copy.vp;
			if (copy.state == NCACHE_STATE_VALID) {
				if ((flags & 0xfffff) == VFS_LOOKUP_TRYLOCK) {
					if (lock_vdlock(vp) == SCE_ERROR_ERRNO_EBUSY) {
						ret = -2;
						goto out;
					}
				} else {
					vfsLockVnode(vp);
				}
				if (vp->core.state < SCE_VNODE_STATE_INACTIVE) {
					ret = 0;
					vp->core.ref_count++;
					*vpp = vp;
				} else {
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 1256, "target is ZOMBIE %p\n", vp);
					ret = SCE_ERROR_ERRNO_ENOENT;
					vfsUnlockVnode(vp);
				}
				goto out;
			}
			if (copy.state == NCACHE_STATE_NEGATIVE) {
				if ((flags & VFS_LOOKUP_CLEAR_NEGATIVE) == 0) {
					ret = SCE_ERROR_ERRNO_ENOENT;
					goto out;
				}
				ret = clear_negative_cache(dvp, &name, copy.name);
				goto out;
			}
			if (copy.state == NCACHE_STATE_RENAMING || copy.state == NCACHE_STATE_WHITEOUT) {
				ret = SCE_ERROR_ERRNO_ENOENT;
				goto out;
			}
			goto lookup;
		}
	}
	vfs_unlock_ncache();
lookup:
	lpath.path.name = name.name;
	lpath.path.name_length = name.len;
	lpath.path.path = path;
	lpath.elem = elem->name;
	ret = ksceVopLookup(dvp, vpp, &lpath.path, flags & 0xfffff);
	if (ret == 0) {
		SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(*vpp) == 1, 1318);
		ret = enter_ncache(dvp, *vpp, &name, NCACHE_STATE_VALID, 0);
		if (ret < 0)
			vfsFreeVnode(*vpp);
	} else if (ret == SCE_ERROR_ERRNO_ENOENT) {
		if ((dvp->core.mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_REMOTE) != 0 ||
		    (flags & VFS_LOOKUP_CLEAR_NEGATIVE) != 0)
			ret = SCE_ERROR_ERRNO_ENOENT;
		else
			enter_ncache(dvp, NULL, &name, NCACHE_STATE_NEGATIVE, 0);
	}
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfs_find_in_vnode(SceVfsVnode *dvp, const VfsPathElem *elem, const char *path, SceUInt32 flags,
		      SceVfsVnode **vpp)
{
	SceVfsVnode *vp;
	int ret;

	ret = vfs_lookup(dvp, elem, path, flags, &vp);
	if (ret != 0)
		return ret;
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 5870);
	ret = vfs_check_vnode_read_access(vp);
	if (ret < 0) {
		vp->core.ref_count--;
		vfsUnlockVnode(vp);
		if (ret == SCE_ERROR_ERRNO_EACCES || ret == VFS_ERROR_PFS_EACCES)
			ret = SCE_ERROR_ERRNO_ENOENT;
		return ret;
	}
	vfsLockMnt(vp->core.mnt);
	vp->core.mnt->mnt_ref_count++;
	vfsUnlockMnt(vp->core.mnt);
	*vpp = vp;
	return ret;
}

/* name unknown: the vnode's name (vfsGetVnodeName) */
int vfs_get_vnode_name(SceVfsVnode *vp, char *name, SceSize n, SceSize *result)
{
	struct Ncache *nc;

	if (n == 0)
		return SCE_ERROR_ERRNO_EINVAL;
	vfsLockVnode(vp);
	vfs_lock_ncache();
	nc = vp->core.ncache;
	if (nc == NULL) {
		vfs_unlock_ncache();
		vfsUnlockVnode(vp);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (n <= nc->nameLength)
		goto truncate;
	strncpy(name, nc->name, nc->nameLength);
	name[nc->nameLength] = '\0';
	*result = nc->nameLength;
out:
	vfs_unlock_ncache();
	vfsUnlockVnode(vp);
	return 0;

truncate:
	strncpy(name, nc->name, n);
	name[n - 1] = '\0';
	*result = n - 1;
	goto out;
}

int set_ncache_state(struct Ncache *nc, SceUInt32 from, SceUInt32 to)
{
	struct Ncache *child, *next;
	SceVfsVnode *vp;
	int ret;

	if (nc == NULL)
		return 0;
	if (nc->state != NCACHE_STATE_NEGATIVE) {
		vp = nc->vp;
		if (nc != vp->core.ncache) {
			SCE_DBG_PRINTF(0xf, 1812, "vp->ncache %p != nc %p %s \n", vp->core.ncache, nc,
				       nc->name);
			vfs_dump_vnode(vp, 0);
		}
	}
	vfs_lock_ncache();
	if (nc->state == from)
		nc->state = to;
	ret = 0;
	for (child = nc->child; child != NULL; child = next) {
		next = child->next;
		ret = set_ncache_state(child, from, to);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 1839, "nc %s :: my child could not be cleaned\n",
				       nc->name);
	}
	vfs_unlock_ncache();
	return ret;
}

int restore_ncache(SceVfsVnode *dvp, SceVfsVnode *vp, const VfsPathElem *elem, SceVfsVnode *ovp,
		   SceUID oldHeap)
{
	struct Ncache *nc, *ncdd, *cur, *prev;
	VfsPathElem name;
	SceSize len;
	const char *path2, *path3;
	SceUID heap;
	char *buf, *newName, *oldName;
	int ret;

	heap = vp->core.allocator;
	if (dvp->core.ncache == NULL || vp->core.ncache != NULL)
		return SCE_ERROR_ERRNO_EPERM;
	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = ksceVfsOpDecodePathElem(dvp->core.mnt, elem->name, &path2, &path3, buf,
				      VFS_PATH_BUFF_SIZE, &len);
	if (ret < 0) {
		if (ret != SCE_ERROR_ERRNO_EUNSUP) {
			vfs_path_buff_free(buf);
			return ret;
		}
		name.name = elem->name;
		name.len = elem->len;
	} else {
		name.name = buf;
		name.len = len;
	}
	newName = SceSysmemForDriver_7B4CB60A(heap, name.len + 1);
	if (newName == NULL) {
		vfs_path_buff_free(buf);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	vfs_lock_ncache();
	nc = ovp->core.ncache;
	if (nc == NULL || dvp->core.ncache == NULL ||
	    (dvp->core.ncache->state & NCACHE_STATE_VALID) == 0) {
		vfs_unlock_ncache();
		sceKernelFreeHeapMemory(heap, newName);
		vfs_path_buff_free(buf);
		return SCE_ERROR_ERRNO_ENOENT;
	}
	ncdd = nc->ncdd;
	if (ncdd != NULL) {
		cur = ncdd->child;
		if (cur == NULL) {
			SCE_DBG_PRINTF(0xf, 1588, "can not find nc entry 0x%d 0x%x\n",
				       SCE_ERROR_ERRNO_ENOENT, SCE_ERROR_ERRNO_ENOENT);
		} else {
			if (cur == nc) {
				ncdd->child = nc->next;
			} else {
				do {
					prev = cur;
					cur = prev->next;
					if (cur == nc)
						break;
				} while (cur != NULL);
				prev->next = nc->next;
			}
			nc->next = NULL;
		}
	}
	oldName = nc->name;
	nc->name = newName;
	vfs_set_ncache_entry(nc, dvp, vp, &name, NCACHE_STATE_VALID);
	cur = dvp->core.ncache->child;
	if (cur == NULL) {
		dvp->core.ncache->child = nc;
	} else {
		do {
			prev = cur;
			cur = prev->next;
		} while (cur != NULL);
		prev->next = nc;
	}
	set_ncache_state(nc, NCACHE_STATE_RENAMING, NCACHE_STATE_VALID);
	vfs_unlock_ncache();
	vfs_path_buff_free(buf);
	ret = sceKernelFreeHeapMemory(oldHeap, oldName);
	if (ret != 0)
		SCE_DBG_PRINTF(0xf, 1612, "can not free memory %p ret = %d 0x%x\n", nc->name, ret,
			       ret);
	return 0;
}

int check_ncache(struct Ncache *nc)
{
	struct Ncache *child, *next;
	SceVfsVnode *vp;
	int ret;

	if (nc == NULL)
		return 0;
	if (nc->state == NCACHE_STATE_NEGATIVE)
		return 0;
	vp = nc->vp;
	if (nc != vp->core.ncache) {
		SCE_DBG_PRINTF(0xf, 1754, "vp->ncache %p != nc %p %s \n", vp->core.ncache, nc,
			       nc->name);
		vfs_dump_vnode(vp, 0);
	}
	vfs_lock_ncache();
	if ((int)vp->core.fd_num > 0) {
		SCE_DBG_PRINTF(0xf, 1763,
			       "############# PFS::WARNING %s is still opened !! ###############\n",
			       nc->name);
		vfs_unlock_ncache();
		return SCE_ERROR_ERRNO_EBUSY;
	}
	child = nc->child;
	if (child == NULL) {
		vfs_unlock_ncache();
		return 0;
	}
	do {
		next = child->next;
		ret = check_ncache(child);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 1778, "nc %s :: my child could not be cleaned nnc=%p\n",
				       nc->name, next);
		child = next;
	} while (next != NULL);
	vfs_unlock_ncache();
	return ret;
}

/* name unknown: splits a path into its elements, resolving "." and ".." */
int vfs_split_path(const char *path, VfsPathElem *elem, int *num)
{
	SceSize dots;
	int len, allDots, i = 0;
	SceUInt8 byte;
	SceInt8 c;

	c = *path;
	if (c == '/')
		path++;
	for (;;) {
		elem[i].name = path;
		byte = *path++;
		c = byte;
		if (c == '/')
			goto next;
		if (byte == '\0')
			break;
		dots = 0;
		len = 0;
		for (;;) {
			allDots = 0;
			if (c == '.') {
				allDots = len == dots;
				dots++;
			}
			len++;
			byte = *path++;
			c = byte;
			if (c == '/')
				break;
			if (byte == '\0') {
				if (len == 0)
					goto done;
				if (!allDots || dots > 2) {
					elem[i].len = len;
					*num = i + 1;
					return 0;
				}
				if (dots == 2) {
					if (i != 0)
						i--;
				}
				*num = i;
				return 0;
			}
		}
		if (len == 0)
			goto next;
		if (allDots) {
			if (dots == 1)
				goto next;
			if (dots == 2) {
				if (i != 0)
					i--;
				goto next;
			}
		}
		elem[i].len = len;
		i++;
next:
		if (i > VFS_PATH_ELEM_MAX - 1) {
			*num = i + 1;
			return SCE_ERROR_ERRNO_EINVAL;
		}
	}
done:
	*num = i;
	return 0;
}

int replace_flat_path(const char *path, char *flat, VfsPathElem *elem, int *num, int absolute)
{
	VfsPathElem relem[VFS_PATH_ELEM_MAX];
	SceVfsMount *mnt;
	const char *colon, *mntPath, *name;
	SceSize depth, len, plen;
	SceUID pid;
	char *buf, *p, *digit;
	int rnum, ret, i;
	IofilemgrKpls *kpls;

	while (*path == ' ')
		path++;
	ret = vfs_check_path_length(path, &depth, &len, VFS_PATH_BUFF_SIZE);
	if (ret < 0)
		return ret;
	if ((int)depth > 0x104)
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	getIofilemgrTls()->pathLength = len;
	if (*path == '/') {
		if (absolute == 0)
			return SCE_ERROR_ERRNO_EINVAL;
		strncpy(flat, path, VFS_PATH_BUFF_SIZE - 1);
		flat[VFS_PATH_BUFF_SIZE - 1] = '\0';
		goto split;
	}
	colon = strchr(path, ':');
	if (colon == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	plen = colon - path;
	if (plen >= 0x1f)
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	if (g_vfsHostfsAssignName == NULL ||
	    strncmp(path, g_vfsHostfsAssignName, plen + 1) != 0) {
		buf = vfs_path_buff_alloc();
		if (buf == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ret = vfs_split_path(colon + 1, relem, &rnum);
		if (ret < 0)
			goto free;
		pid = ksceKernelGetProcessIdFromTLS();
		if ((pid & ~0x10) != SCE_KERNEL_PROCESS_ID &&
		    pid != SCE_KERNEL_DUMMY_PROCESS_GAME_ID) {
			ret = ksceKernelGetProcessLocalStorageAddrForPid(pid, g_iofilemgrKplsKey,
									 (void **)&kpls, 1);
			if (ret < 0)
				goto free;
			if (kpls->depth < rnum) {
				vfs_path_buff_free(buf);
				SCE_DBG_PRINTF(0xf, 998, "path=%s num=%d limit=%d\n", path, rnum,
					       kpls->depth);
				return SCE_ERROR_ERRNO_EINVAL;
			}
		}
		strncpy(buf, path, plen + 1);
		p = buf + plen + 1;
		for (i = 0; i < rnum; i++) {
			strncpy(p, relem[i].name, relem[i].len + 1);
			p += relem[i].len + 1;
		}
		*p = '\0';
		mntPath = find_mounted_path(buf, plen);
		p = buf + plen;
		if (mntPath == NULL) {
			if (p[1] != '\0')
				goto nodev;
			if ((unsigned int)(unsigned char)p[-1] - '0' < 10) {
				digit = p - 1;
				len = digit - buf;
			} else {
				len = p - buf;
				digit = NULL;
			}
			vfs_lock_mnt_list();
			for (mnt = vfs_get_mnt_list(); mnt != NULL; mnt = mnt->mnt_list_next) {
				if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_NUMBERED) == 0)
					continue;
				name = mnt->mnt_data->assign_name;
				if (strncmp(name, buf, len) != 0 || name[len] != '\0')
					continue;
				vfs_unlock_mnt_list();
				strncpy(flat, buf, len);
				flat[len] = '/';
				strncpy(flat + len + 1, buf, len);
				if (digit == NULL)
					flat[len * 2 + 1] = '0';
				else
					flat[len * 2 + 1] = *digit;
				goto check;
			}
			vfs_unlock_mnt_list();
			goto nodev;
		}
		len = strnlen(mntPath, 0x40);
		strncpy(flat, mntPath, len);
		flat[len] = '/';
		flat[len + 1] = '\0';
		strncat(flat, p + 1, VFS_PATH_BUFF_SIZE - 1 - len);
check:
		if (flat != NULL) {
			if (strnlen(flat, VFS_PATH_BUFF_SIZE) < VFS_PATH_BUFF_SIZE - 1) {
				vfs_path_buff_free(buf);
				goto split;
			}
			ret = SCE_ERROR_ERRNO_ENAMETOOLONG;
			goto free;
		}
nodev:
		ret = SCE_ERROR_ERRNO_ENODEV;
free:
		vfs_path_buff_free(buf);
		return ret;
	}
	colon = strchr(path, ':');
	if (colon == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	if ((SceSize)(colon - path) >= 0x1f)
		return SCE_ERROR_ERRNO_ENAMETOOLONG;
	p = (char *)find_mounted_path(path, colon - path);
	if (p == NULL)
		return SCE_ERROR_ERRNO_ENODEV;
	while (*p == '/')
		p++;
	len = strnlen(p, 0x40);
	strncpy(flat, p, len);
	flat[len] = '/';
	flat[len + 1] = '\0';
	elem[0].name = flat;
	elem[0].len = len;
	p = flat + len + 1;
	strncat(flat, colon + 1, VFS_PATH_BUFF_SIZE - 1 - len);
	elem[1].name = p;
	len = strnlen(p, VFS_PATH_BUFF_SIZE - 1 - len);
	elem[1].len = len;
	if (strncmp(p, "", len + 1) == 0 || strncmp(p, ".", len + 1) == 0)
		*num = 1;
	else
		*num = 2;
	return 0;
split:
	ret = vfs_split_path(flat, elem, num);
	if (ret == 0)
		((char *)elem[*num - 1].name)[elem[*num - 1].len] = '\0';
	return ret;
}

/* name unknown: drops everything a lookup holds: two path buffers and four vnodes */
void vfs_release_lookup(char *path, SceVfsVnode *vp1, SceVfsVnode *vp2, char *path2,
			SceVfsVnode *vp3, SceVfsVnode *vp4, int keepMnt)
{
	if (vp4 != NULL) {
		if (keepMnt == 0) {
			vfs_decrement_vp_mnt_ref_inline(vp4);
		} else {
			vp4->core.ref_count--;
			vfsUnlockVnode(vp4);
		}
	}
	if (vp3 != NULL)
		vfs_decrement_vp_mnt_ref_inline(vp3);
	if (vp2 != NULL)
		vfs_decrement_vp_mnt_ref_inline(vp2);
	if (vp1 != NULL)
		vfs_decrement_vp_mnt_ref_inline(vp1);
	if (path2 != NULL)
		vfs_path_buff_free(path2);
	if (path != NULL)
		vfs_path_buff_free(path);
}

int vfs_find_in_path(VfsPathElem *elem, int num, SceVfsVnode **vpp, const char *path,
		     SceUInt32 flags)
{
	SceVfsMount *mnt;
	SceVfsVnode *vp, *dvp;
	SceUInt32 pathLength;
	int depth, ret, i, type;

	if (num < 1)
		return SCE_ERROR_ERRNO_ENOENT;
	ret = vfs_get_root_vnode(elem, num, &depth, flags, &vp);
	if (ret < 0)
		return ret;
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 262);
	mnt = vp->core.mnt;
	if (mnt->fs_type == SCE_VFS_FS_TYPE_PFS) {
		pathLength = getIofilemgrTls()->pathLength;
		if ((SceInt32)pathLength > 0xc9) {
			SCE_DBG_PRINTF(0xf, 269, "%s pathlen=%d\n", elem->name, pathLength);
			vfs_decrement_vp_mnt_ref(vp);
			return SCE_ERROR_ERRNO_ENAMETOOLONG;
		}
	}
	if ((mnt->mnt_flags & 0x20000) != 0 && *mnt->fumount->devState != 1) {
		type = flags & 0xfffff;
		switch (type) {
		case 0x1000:
		case 0x2000:
		case 0x6000:
		case 0x7000:
			break;
		default:
			vfsUnlockVnode(vp);
			ret = vfs_wait_fumount(mnt);
			vfsLockVnode(vp);
			if (ret < 0)
				goto release;
		}
	}
	if ((vp->core.type & SCE_VNODE_TYPE_ROOT_MOUNT) == 0) {
		ret = vfs_check_vnode_read_access(vp);
		if (ret < 0) {
			if (ret == SCE_ERROR_ERRNO_EACCES || ret == VFS_ERROR_PFS_EACCES)
				ret = SCE_ERROR_ERRNO_ENOENT;
			goto release;
		}
	}
	for (i = depth; i < num; i++) {
		dvp = vp;
		ret = vfs_lookup(dvp, &elem[i], path, flags, &vp);
		SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 326);
		dvp->core.ref_count--;
		vfsUnlockVnode(dvp);
		if (ret != 0) {
			vfsLockMnt(mnt);
			if ((int)--mnt->mnt_ref_count < 0)
				SCE_DBG_PRINTF(0xf, 355, "mnt->mntRefCount = %d\n", mnt->mnt_ref_count);
			goto unlock;
		}
		if (vp->core.mnt != mnt) {
			vfsLockMnt(mnt);
			if ((int)--mnt->mnt_ref_count < 0)
				SCE_DBG_PRINTF(0xf, 337, "mnt->mntRefCount = %d\n", mnt->mnt_ref_count);
			vfsUnlockMnt(mnt);
			mnt = vp->core.mnt;
			vfsLockMnt(mnt);
			mnt->mnt_ref_count++;
			vfsUnlockMnt(mnt);
		}
		ret = vfs_check_vnode_read_access(vp);
		if (ret < 0) {
			vp->core.ref_count--;
			vfsUnlockVnode(vp);
			vfsLockMnt(mnt);
			if ((int)--mnt->mnt_ref_count < 0)
				SCE_DBG_PRINTF(0xf, 378, "mnt->mntRefCount = %d\n", mnt->mnt_ref_count);
			if (ret == SCE_ERROR_ERRNO_EACCES || ret == VFS_ERROR_PFS_EACCES)
				ret = SCE_ERROR_ERRNO_ENOENT;
			if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
				vfs_umount_cleanup(mnt);
			else
				vfsUnlockMnt(mnt);
			return ret;
		}
	}
	*vpp = vp;
	return ret;
release:
	vfs_decrement_vp_mnt_ref_inline(vp);
	return ret;
unlock:
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
		vfs_umount_cleanup(mnt);
	else
		vfsUnlockMnt(mnt);
	return ret;
}

SceVfsVnode *vfs_get_devfile_vnode(const char *path, char *flat, VfsPathElem *last)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *vp = NULL;
	int num;

	if (replace_flat_path(path, flat, elem, &num, 0) < 0 || num == 0)
		return vp;
	if (vfs_find_in_path(elem, num, &vp, flat, 0x2000) < 0)
		return NULL;
	if ((vp->core.type & SCE_VNODE_TYPE_DEV) == 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 3011, "vp is %p %s\n", vp, elem[0].name);
		return NULL;
	}
	*last = elem[num - 1];
	return vp;
}

/* the PFS mount: a file system stacked on a vnode of another mount */
static int vfs_mount_pfs(SceVfsMountParam *param, SceVfsInfo *vfsInf)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsPath devPath;
	SceVfsVnode *dvp, *vp, *rvp;
	SceVfsMount *mnt, *pmnt;
	const SceVopTable *vops = param->vops;
	VfsPathElem *last;
	char *path, *root;
	int num, ret, err;

	path = vfs_path_buff_alloc();
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	root = vfs_path_buff_alloc();
	if (root == NULL) {
		vfs_path_buff_free(path);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = replace_flat_path(param->blockdev_name, path, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto out;
	last = &elem[num - 1];
	ret = vfs_find_in_path(elem, num - 1, &dvp, path, 0x3000);
	if (ret < 0) {
		if (ret != SCE_ERROR_ERRNO_ENOENT)
			goto out;
		ret = vfs_find_in_path(elem, num, &vp, path, 0x3000);
		if (ret < 0)
			goto out;
		dvp = NULL;
	} else {
		ret = vfs_find_in_vnode(dvp, last, path, 0x3000, &vp);
		if (ret < 0)
			goto release_dvp;
	}
	if (vp->core.mnt->fs_type == SCE_VFS_FS_TYPE_PFS) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release;
	}
	if ((param->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) == 0) {
		ret = vfs_check_vnode_rw_access(vp);
		if (ret < 0) {
			if (ret == VFS_ERROR_PFS_EACCES || ret == SCE_ERROR_ERRNO_EACCES)
				ret = SCE_ERROR_ERRNO_EROFS;
			goto release;
		}
		if ((vp->core.mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) != 0) {
			ret = SCE_ERROR_ERRNO_EROFS;
			goto release;
		}
	} else {
		ret = vfs_check_vnode_read_access(vp);
		if (ret < 0)
			goto release;
	}
	devPath.name = last->name;
	devPath.name_length = last->len;
	devPath.path = path;
	ret = replace_flat_path(param->root_path, root, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto release;
	mnt = vfs_alloc_mount();
	if (mnt == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto release;
	}
	vfs_init_mount(mnt, vp, g_vfsNcacheHeap, vfsInf);
	mnt->mnt_data = param->misc;
	strncpy(mnt->path, root, sizeof(mnt->path));
	mnt->path[sizeof(mnt->path) - 1] = '\0';
	mnt->data = param->data;
	__builtin_memcpy(&mnt->fs_type, &param->fs_type, sizeof(SceUInt32));
	mnt->mnt_flags = param->mnt_flags & VFS_MOUNT_FLAG_PARAM_MASK;
	mnt->state = VFS_MOUNT_STATE_MOUNTING;
	vfsLockMnt(vp->core.mnt);
	pmnt = vp->core.mnt;
	pmnt->mnt_ref_count++;
	vfs_insert_mntedon_list(pmnt, mnt);
	vfsUnlockMnt(vp->core.mnt);
	mnt->opt |= vp->core.mnt->opt;
	if (vops == NULL)
		vops = vfsInf->default_vops;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)vops, 0, &rvp);
	if (ret >= 0) {
		if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0 &&
		    (mnt->opt & VFS_MOUNT_OPT_DEV_MASK) != VFS_MOUNT_OPT_DEV_EXTERNAL) {
			mnt->mnt_flags &= ~SCE_VFS_MOUNT_FLAG_EXTERNAL;
			SCE_DBG_PRINTF_FUNC(0xf, 2414, "vfs_mount_pfs",
					    "########## WARNING: FS`s dev type is not external media ! ############\n");
		}
		pmnt = vp->core.mnt;
		if ((pmnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0 &&
		    (pmnt->opt & VFS_MOUNT_OPT_DEV_MASK) == VFS_MOUNT_OPT_DEV_EXTERNAL) {
			mnt->fumount->devState = pmnt->fumount->devState;
			mnt->fumount->activationEvf = pmnt->fumount->activationEvf;
			mnt->mnt_flags |= SCE_VFS_MOUNT_FLAG_EXTERNAL;
		}
		rvp->core.mnt = mnt;
		rvp->core.dd = dvp;
		rvp->core.type = SCE_VNODE_TYPE_ROOTDIR;
		if (vp->core.ncache != NULL) {
			ret = check_ncache(vp->core.ncache);
			if (ret < 0)
				goto free_vnode;
			err = clear_ncache(vp->core.ncache, 1);
			if (err < 0)
				SCE_DBG_PRINTF_FUNC(0xf, 2453, "vfs_mount_pfs",
						    "name cache cleanup failed ... %p ret = %d 0x%x\n", vp,
						    err, err);
		}
		rfs_lock();
		if (vp->core.ncache == NULL || (ret = check_ncache(vp->core.ncache)) >= 0) {
			ret = rfs_add_entry(elem, num, rvp, 0xd8);
			rfs_unlock();
			if (ret < 0)
				goto free_vnode;
			vfsLockVnode(rvp);
			if (dvp == NULL)
				ret = vfs_enter_root_ncache(rvp, last);
			else
				ret = enter_ncache(dvp, rvp, last, 1, 1);
			if (ret >= 0) {
				vfs_lock_mnt_list();
				vfs_insert_mnt_list(mnt);
				vfs_unlock_mnt_list();
				ret = sceVfsOpMount(mnt, &devPath);
				if (ret == 0 && (ret = sceVfsOpSetRoot(mnt, 0, rvp)) >= 0) {
					vfsLockMnt(mnt);
					mnt->state &= ~VFS_MOUNT_STATE_MOUNTING_BIT;
					mnt->mnt_ref_count--;
					vfsUnlockMnt(mnt);
					rvp->core.ref_count--;
					vfsUnlockVnode(rvp);
					goto release;
				}
				vfs_lock_mnt_list();
				vfs_remove_mnt_list(mnt);
				vfs_unlock_mnt_list();
				clear_ncache(rvp->core.ncache, 0);
			}
			rfs_lock();
			rfs_remove_entry(elem, num);
		}
		rfs_unlock();
free_vnode:
		vfsFreeVnode(rvp);
	}
	vfsLockMnt(vp->core.mnt);
	pmnt = vp->core.mnt;
	pmnt->mnt_ref_count--;
	vfs_remove_mntedon_list(pmnt, mnt);
	vfsUnlockMnt(vp->core.mnt);
	vfs_finalize_mount(mnt);
	vfs_free_mount(mnt);
release:
	vfs_decrement_vp_mnt_ref(vp);
release_dvp:
	if (dvp != NULL)
		vfs_decrement_vp_mnt_ref(dvp);
out:
	vfs_path_buff_free(path);
	vfs_path_buff_free(root);
	return ret;
}

/* the root file system of a block device */
static int vfs_mount_fsroot(SceVfsMountParam *param, SceVfsInfo *vfsInf)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	VfsPathElem dev;
	SceVfsPath devPath;
	SceVfsVnode *dvp, *rvp;
	SceVfsMount *mnt, *pmnt;
	SceVfsMountData *misc = param->misc;
	const SceVopTable *vops = param->vops;
	char *path, *devfile;
	int num, ret, err, mret;

	dev.name = NULL;
	dev.len = 0;
	path = vfs_path_buff_alloc();
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	devfile = vfs_path_buff_alloc();
	if (devfile == NULL) {
		vfs_path_buff_free(path);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	if (param->blockdev_name == NULL) {
		dvp = vfs_get_devfile_vnode(misc->blockdev_name, devfile, &dev);
		if (dvp == NULL && (misc->blockdev_name_no_part == NULL ||
				    (dvp = vfs_get_devfile_vnode(misc->blockdev_name_no_part, devfile,
								 &dev)) == NULL)) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			goto out;
		}
	} else {
		ret = replace_flat_path(param->blockdev_name, devfile, elem, &num, 1);
		if (ret < 0 || num == 0)
			goto out;
		ret = vfs_find_in_path(elem, num, &dvp, devfile, 0x2000);
		if (ret < 0)
			goto out;
		dev = elem[num - 1];
	}
	ret = replace_flat_path(param->root_path, path, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto release;
	if ((param->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) == 0) {
		ret = vfs_check_vnode_rw_access(dvp);
		if (ret < 0) {
			if (ret == VFS_ERROR_PFS_EACCES || ret == SCE_ERROR_ERRNO_EACCES)
				ret = SCE_ERROR_ERRNO_EROFS;
			goto release;
		}
	} else {
		ret = vfs_check_vnode_read_access(dvp);
		if (ret < 0)
			goto release;
	}
	mnt = vfs_alloc_mount();
	if (mnt == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto release;
	}
	vfs_init_mount(mnt, dvp, g_vfsNcacheHeap, vfsInf);
	mnt->mnt_data = misc;
	strncpy(mnt->path, param->root_path, sizeof(mnt->path));
	mnt->path[sizeof(mnt->path) - 1] = '\0';
	mnt->data = param->data;
	mnt->fs_type = param->fs_type;
	mnt->opt = param->opt;
	mnt->mnt_flags = param->mnt_flags & VFS_MOUNT_FLAG_PARAM_MASK;
	mnt->state = VFS_MOUNT_STATE_MOUNTING;
	vfsLockMnt(dvp->core.mnt);
	pmnt = dvp->core.mnt;
	pmnt->mnt_ref_count++;
	vfs_insert_mntedon_list(pmnt, mnt);
	vfsUnlockMnt(dvp->core.mnt);
	if (vops == NULL)
		vops = vfsInf->default_vops;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)vops, 0, &rvp);
	if (ret < 0)
		goto unlink;
	if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0) {
		if ((mnt->opt & VFS_MOUNT_OPT_DEV_MASK) == VFS_MOUNT_OPT_DEV_EXTERNAL) {
			ret = vfs_set_dev_wait_evf(mnt);
			if (ret < 0)
				goto unlink;
		} else {
			mnt->mnt_flags &= ~SCE_VFS_MOUNT_FLAG_EXTERNAL;
			SCE_DBG_PRINTF_FUNC(0xf, 2142, "vfs_mount_fsroot",
					    "########## WARNING: FS`s dev type is not external media ! ############\n");
		}
	}
	rvp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	rvp->core.allocator = mnt->allocator;
	rvp->core.mnt = mnt;
	rfs_lock();
	err = rfs_add_entry(elem, num, rvp, 0xd8);
	rfs_unlock();
	if (err >= 0) {
		err = vfs_enter_root_ncache(rvp, &elem[num - 1]);
		if (err >= 0) {
			vfs_lock_mnt_list();
			vfs_insert_mnt_list(mnt);
			vfs_unlock_mnt_list();
			devPath.name = dev.name;
			devPath.name_length = dev.len;
			devPath.path = devfile;
			mret = sceVfsOpMount(mnt, &devPath);
			err = mret;
			if (mret == 0 && (err = sceVfsOpSetRoot(mnt, 0, rvp)) >= 0) {
				ret = err;
				if ((param->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) == 0) {
					err = ksceVfsOpDevctl(mnt, misc->assign_name, 0x3802, NULL, 0, NULL, 0);
					ret = mret;
					if (err != SCE_ERROR_ERRNO_ENOSPC && err < 0)
						goto devctl_failed;
				}
				vfsLockMnt(mnt);
				mnt->state &= ~VFS_MOUNT_STATE_MOUNTING_BIT;
				mnt->mnt_ref_count--;
				vfsUnlockMnt(mnt);
				rvp->core.ref_count--;
				goto release;
devctl_failed:
				SCE_DBG_PRINTF_FUNC(0xf, 2196, "vfs_mount_fsroot",
						    "devctl(SCE_EXFATFS_DEVCTL_CREATT_WDIR) ret=%x\n", err);
				vfsLockMnt(mnt);
				mret = sceVfsOpUmount(mnt, 0);
				if (mret < 0)
					SCE_DBG_PRINTF_FUNC(0xf, 2200, "vfs_mount_fsroot",
							    "Umount failed %d 0x%x\n", mret, mret);
				vfsUnlockMnt(mnt);
				ksceVopInactive(rvp);
			}
			vfs_lock_mnt_list();
			vfs_remove_mnt_list(mnt);
			vfs_unlock_mnt_list();
			clear_ncache(rvp->core.ncache, 0);
		}
		rfs_lock();
		rfs_remove_entry(elem, num);
		rfs_unlock();
	}
	vfsFreeVnode(rvp);
	ret = err;
unlink:
	vfsLockMnt(dvp->core.mnt);
	vfs_remove_mntedon_list(dvp->core.mnt, mnt);
	pmnt = dvp->core.mnt;
	pmnt->mnt_ref_count--;
	vfsUnlockMnt(pmnt);
	vfs_finalize_mount(mnt);
	vfs_free_mount(mnt);
release:
	vfs_decrement_vp_mnt_ref_inline(dvp);
out:
	vfs_path_buff_free(devfile);
	vfs_path_buff_free(path);
	return ret;
}

/* name unknown: a device file system (/dev, /tty, /md) */
static int vfs_mount_devfs(SceVfsMountParam *param, SceVfsInfo *vfsInf)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *rvp;
	SceVfsMount *mnt;
	const SceVopTable *vops = param->vops;
	char *path;
	int num, ret;

	path = vfs_path_buff_alloc();
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(param->root_path, path, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto out;
	if (strncmp(elem[0].name, "dev", elem[0].len) != 0 &&
	    strncmp(elem[0].name, "tty", elem[0].len) != 0) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		if (strncmp(elem[0].name, "md", elem[0].len) != 0)
			goto out;
	}
	mnt = vfs_alloc_mount();
	if (mnt == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	vfs_init_mount(mnt, NULL, g_vfsNcacheHeap, vfsInf);
	mnt->mnt_data = param->misc;
	strncpy(mnt->path, param->root_path, sizeof(mnt->path));
	mnt->fs_type = param->fs_type;
	mnt->opt = param->opt;
	mnt->mnt_flags = (param->mnt_flags & VFS_MOUNT_FLAG_PARAM_MASK) |
			 SCE_VFS_MOUNT_FLAG_NO_RECLAIM;
	mnt->path[sizeof(mnt->path) - 1] = '\0';
	mnt->state = VFS_MOUNT_STATE_MOUNTING;
	mnt->data = param->data;
	if (vops == NULL)
		vops = vfsInf->default_vops;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)vops, 0, &rvp);
	if (ret >= 0) {
		rvp->core.mnt = mnt;
		mnt->mnt_vnode = rvp;
		rvp->core.type = SCE_VNODE_TYPE_DEV;
		rvp->core.allocator = mnt->allocator;
		rfs_lock();
		ret = rfs_add_entry(elem, num, rvp, 0xd8);
		rfs_unlock();
		if (ret >= 0) {
			ret = vfs_enter_root_ncache(rvp, &elem[num - 1]);
			if (ret >= 0) {
				vfs_lock_mnt_list();
				vfs_insert_mnt_list(mnt);
				vfs_unlock_mnt_list();
				ret = sceVfsOpMount(mnt, NULL);
				if (ret == 0) {
					ret = sceVfsOpSetRoot(mnt, 0, rvp);
					if (ret >= 0) {
						vfsLockMnt(mnt);
						mnt->state &= ~VFS_MOUNT_STATE_MOUNTING_BIT;
						mnt->mnt_ref_count--;
						vfsUnlockMnt(mnt);
						rvp->core.ref_count--;
						goto out;
					}
					clear_ncache(rvp->core.ncache, 0);
				}
				vfs_lock_mnt_list();
				vfs_remove_mnt_list(mnt);
				vfs_unlock_mnt_list();
			}
			rfs_lock();
			rfs_remove_entry(elem, num);
			rfs_unlock();
		}
		vfsFreeVnode(rvp);
	}
	vfs_free_mount(mnt);
out:
	vfs_path_buff_free(path);
	return ret;
}

/* the loop file system: a file system mounted from a file */
static int vfs_mount_stackfs(SceVfsMountParam *param, SceVfsInfo *vfsInf)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsPath devPath;
	SceVfsVnode *vp, *rvp;
	SceVfsMount *mnt, *pmnt;
	const SceVopTable *vops = param->vops;
	char *path, *root;
	int num, ret, err, mret;

	path = vfs_path_buff_alloc();
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	root = vfs_path_buff_alloc();
	if (root == NULL) {
		vfs_path_buff_free(path);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	ret = replace_flat_path(param->blockdev_name, path, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto out;
	ret = vfs_find_in_path(elem, num, &vp, path, 0x4000);
	if (ret < 0)
		goto out;
	if ((vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_REG)) == 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 2621, "src is not a regular file. \n");
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto release;
	}
	if ((int)vp->core.fd_num > 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 2627, "someone opened this src file\n");
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release;
	}
	if ((vp->core.state & SCE_VNODE_STATE_ACTIVE) == 0) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto release;
	}
	if ((param->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) == 0) {
		ret = vfs_check_vnode_rw_access(vp);
		if (ret < 0) {
			if (ret == VFS_ERROR_PFS_EACCES || ret == SCE_ERROR_ERRNO_EACCES) {
				ret = vfs_check_vnode_read_access(vp);
				if (ret >= 0) {
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 2659, "src file is Read Only\n");
					ret = SCE_ERROR_ERRNO_EINVAL;
				} else if (ret == VFS_ERROR_PFS_EACCES || ret == SCE_ERROR_ERRNO_EACCES) {
					ret = SCE_ERROR_ERRNO_ENOENT;
				}
			}
			goto release;
		}
	} else {
		ret = vfs_check_vnode_read_access(vp);
		if (ret < 0)
			goto release;
	}
	devPath.name = elem[num - 1].name;
	devPath.name_length = elem[num - 1].len;
	devPath.path = path;
	ret = replace_flat_path(param->root_path, root, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto release;
	mnt = vfs_alloc_mount();
	if (mnt == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto release;
	}
	vfs_init_mount(mnt, vp, g_vfsNcacheHeap, vfsInf);
	mnt->mnt_data = param->misc;
	strncpy(mnt->path, param->root_path, sizeof(mnt->path));
	mnt->path[sizeof(mnt->path) - 1] = '\0';
	mnt->data = param->data;
	mnt->fs_type = param->fs_type;
	mnt->opt = param->opt;
	mnt->mnt_flags = param->mnt_flags & VFS_MOUNT_FLAG_PARAM_MASK;
	mnt->state = VFS_MOUNT_STATE_MOUNTING;
	vfsLockMnt(vp->core.mnt);
	pmnt = vp->core.mnt;
	pmnt->mnt_ref_count++;
	vfs_insert_mntedon_list(pmnt, mnt);
	vfsUnlockMnt(vp->core.mnt);
	mnt->opt |= vp->core.mnt->opt;
	if (vops == NULL)
		vops = vfsInf->default_vops;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)vops, 0, &rvp);
	if (ret < 0)
		goto unlink;
	if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0 &&
	    (mnt->opt & VFS_MOUNT_OPT_DEV_MASK) != VFS_MOUNT_OPT_DEV_EXTERNAL) {
		mnt->mnt_flags &= ~SCE_VFS_MOUNT_FLAG_EXTERNAL;
		SCE_DBG_PRINTF(0xf, 2720,
			       "########## WARNING: FS`s dev type is not external media ! ############\n");
	}
	pmnt = vp->core.mnt;
	if ((pmnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0 &&
	    (pmnt->opt & VFS_MOUNT_OPT_DEV_MASK) == VFS_MOUNT_OPT_DEV_EXTERNAL) {
		mnt->fumount->devState = pmnt->fumount->devState;
		mnt->fumount->activationEvf = pmnt->fumount->activationEvf;
		mnt->mnt_flags |= SCE_VFS_MOUNT_FLAG_EXTERNAL;
	}
	rvp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	rvp->core.mnt = mnt;
	rvp->core.allocator = mnt->allocator;
	rvp->core.state = SCE_VNODE_STATE_ACTIVE;
	rfs_lock();
	err = rfs_add_entry(elem, num, rvp, 0xd8);
	rfs_unlock();
	if (err >= 0) {
		err = vfs_enter_root_ncache(rvp, &elem[num - 1]);
		if (err >= 0) {
			vfs_lock_mnt_list();
			vfs_insert_mnt_list(mnt);
			vfs_unlock_mnt_list();
			mret = sceVfsOpMount(mnt, &devPath);
			err = mret;
			if (mret == 0 && (err = sceVfsOpSetRoot(mnt, 0, rvp)) >= 0) {
				ret = err;
				if ((param->mnt_flags & SCE_VFS_MOUNT_FLAG_RDONLY) == 0) {
					err = ksceVfsOpDevctl(mnt, param->misc->assign_name, 0x3802, NULL,
							      0, NULL, 0);
					ret = mret;
					if (err != SCE_ERROR_ERRNO_ENOSPC && err < 0)
						goto devctl_failed;
				}
				vfsLockMnt(mnt);
				mnt->state &= ~VFS_MOUNT_STATE_MOUNTING_BIT;
				mnt->mnt_ref_count--;
				vfsUnlockMnt(mnt);
				rvp->core.ref_count--;
				goto release;
devctl_failed:
				SCE_DBG_PRINTF(0xf, 2784, "devctl(SCE_EXFATFS_DEVCTL_CREATT_WDIR) ret=%x\n",
					       err);
				vfsLockMnt(mnt);
				mret = sceVfsOpUmount(mnt, 0);
				if (mret < 0)
					SCE_DBG_PRINTF(0xf, 2788, "Umount failed %d 0x%x\n", mret, mret);
				vfsUnlockMnt(mnt);
				ksceVopInactive(rvp);
			}
			vfs_lock_mnt_list();
			vfs_remove_mnt_list(mnt);
			vfs_unlock_mnt_list();
			clear_ncache(rvp->core.ncache, 0);
		}
		rfs_lock();
		rfs_remove_entry(elem, num);
		rfs_unlock();
	}
	vfsFreeVnode(rvp);
	ret = err;
unlink:
	vfsLockMnt(vp->core.mnt);
	vfs_remove_mntedon_list(vp->core.mnt, mnt);
	pmnt = vp->core.mnt;
	pmnt->mnt_ref_count--;
	vfsUnlockMnt(pmnt);
	vfs_finalize_mount(mnt);
	vfs_free_mount(mnt);
release:
	vfs_decrement_vp_mnt_ref_inline(vp);
out:
	vfs_path_buff_free(path);
	vfs_path_buff_free(root);
	return ret;
}

/* name unknown: the host file system (host0:), one at a time */
static int vfs_mount_hostfs(SceVfsMountParam *param, SceVfsInfo *vfsInf)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *rvp;
	SceVfsMount *mnt;
	const SceVopTable *vops = param->vops;
	char *path;
	int num, ret;

	if (g_vfsHostfsAssignName != NULL)
		return SCE_ERROR_ERRNO_EBUSY;
	path = vfs_path_buff_alloc();
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(param->root_path, path, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto out;
	if (strncmp(elem[0].name, "host", elem[0].len) != 0) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto out;
	}
	mnt = vfs_alloc_mount();
	if (mnt == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	vfs_init_mount(mnt, NULL, g_vfsNcacheHeap, vfsInf);
	mnt->mnt_data = param->misc;
	strncpy(mnt->path, param->root_path, sizeof(mnt->path));
	mnt->path[sizeof(mnt->path) - 1] = '\0';
	g_vfsHostfsAssignName = param->misc->assign_name;
	mnt->fs_type = param->fs_type;
	mnt->opt = param->opt;
	mnt->mnt_flags = param->mnt_flags & VFS_MOUNT_FLAG_PARAM_MASK;
	mnt->data = param->data;
	mnt->state = VFS_MOUNT_STATE_MOUNTING;
	if (vops == NULL)
		vops = vfsInf->default_vops;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)vops, 0, &rvp);
	if (ret >= 0) {
		rvp->core.mnt = mnt;
		mnt->mnt_vnode = rvp;
		rvp->core.type = VFS_VNODE_TYPE_HOST_ROOT;
		rvp->core.allocator = mnt->allocator;
		rfs_lock();
		ret = rfs_add_entry(elem, num, rvp, 0xd8);
		rfs_unlock();
		if (ret >= 0) {
			ret = vfs_enter_root_ncache(rvp, &elem[num - 1]);
			if (ret >= 0) {
				vfs_lock_mnt_list();
				vfs_insert_mnt_list(mnt);
				vfs_unlock_mnt_list();
				ret = sceVfsOpMount(mnt, NULL);
				if (ret == 0) {
					ret = sceVfsOpSetRoot(mnt, 0, rvp);
					if (ret >= 0) {
						vfsLockMnt(mnt);
						mnt->state &= ~VFS_MOUNT_STATE_MOUNTING_BIT;
						mnt->mnt_ref_count--;
						vfsUnlockMnt(mnt);
						rvp->core.ref_count--;
						goto out;
					}
					clear_ncache(rvp->core.ncache, 0);
				}
				vfs_lock_mnt_list();
				vfs_remove_mnt_list(mnt);
				vfs_unlock_mnt_list();
			}
			rfs_lock();
			rfs_remove_entry(elem, num);
			rfs_unlock();
		}
		vfsFreeVnode(rvp);
	}
	vfs_free_mount(mnt);
out:
	vfs_path_buff_free(path);
	return ret;
}

int vfsMount(SceVfsMountParam *param)
{
	SceVfsMountParam mountParam;
	SceVfsInfo *vfsInf;
	int ret;

	if (param == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	__builtin_memcpy(&mountParam, param, sizeof(mountParam));
	param = &mountParam;
	if (param->root_path == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	vfsInf = vfs_acquire_vfs(param->vfs_name);
	if (vfsInf == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	switch (param->mnt_flags & SCE_VFS_MOUNT_FLAG_TYPE_MASK) {
	case SCE_VFS_MOUNT_TYPE_PFS:
		if (param->fs_type != SCE_VFS_FS_TYPE_PFS)
			goto invalid;
		ret = vfs_mount_pfs(param, vfsInf);
		break;
	case SCE_VFS_MOUNT_TYPE_FSROOT:
		if ((SceUInt8)(param->fs_type - 1) > 1)
			goto invalid;
		ret = vfs_mount_fsroot(param, vfsInf);
		break;
	case SCE_VFS_MOUNT_TYPE_DEVFS:
		if (vfsInf->type != SCE_VFS_TYPE_DEVFS)
			goto invalid;
		ret = vfs_mount_devfs(param, vfsInf);
		break;
	case VFS_MOUNT_TYPE_NONE:
		if (param->blockdev_name == NULL)
			goto invalid;
		return 0;
	case SCE_VFS_MOUNT_TYPE_STACKFS:
		if ((SceUInt8)(param->fs_type - 1) > 1 || param->blockdev_name == NULL)
			goto invalid;
		ret = vfs_mount_stackfs(param, vfsInf);
		break;
	case SCE_VFS_MOUNT_TYPE_HOSTFS:
		if (param->fs_type != SCE_VFS_FS_TYPE_HOSTFS)
			goto invalid;
		ret = vfs_mount_hostfs(param, vfsInf);
		break;
	default:
		goto invalid;
	}
	if (ret == 0)
		return 0;
	goto release;
invalid:
	ret = SCE_ERROR_ERRNO_EINVAL;
release:
	vfs_release_vfs(param->vfs_name);
	return ret;
}

int vfsInitialize(void)
{
	SceKernelHeapCreateOpt opt;
	int ret = 0;

	if (g_vfsInitialized != 0)
		return ret;
	opt.size = sizeof(opt);
	opt.attr = 1;
	g_vfsNcacheHeap = SceSysmemForDriver_9328E0E8("SceIoVfsHeap", 0x1f000, &opt);
	if (g_vfsNcacheHeap < 0) {
		SCE_DBG_PRINTF(0xf, 566, "create heap failed ... %d\n", g_vfsNcacheHeap);
		return g_vfsNcacheHeap;
	}
	ret = vfs_create_mnt_list_mtx();
	if (ret < 0)
		return ret;
	ret = vfs_create_ncache_mtx();
	if (ret < 0)
		return ret;
	ret = vfs_create_vfs_mtx();
	if (ret < 0)
		return ret;
	ret = vfs_mount_initialize(g_vfsNcacheHeap);
	if (ret < 0)
		return ret;
	ret = vfs_vnode_initialize();
	if (ret < 0)
		return ret;
	rfs_table_initialize();
	ret = vfs_file_class_init();
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 613, "vfs_file_class_init failed %d 0x%X !!\n", ret, ret);
		return ret;
	}
	vfs_vop_table_cache_initialize();
	vfs_path_buff_initialize();
	ret = mnt_initialize();
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 623, "mnt_initialize failed %d 0x%X !!\n", ret, ret);
		return ret;
	}
	ret = sceKernelRegisterSysEventHandler("SceIofilemgr", vfs_system_event_handler, NULL);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xf, 632, "sceKernelRegisterSysEventHandler failed %d 0x%X !!\n", ret,
			       ret);
		return ret;
	}
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0)
		vfs_debug_buff_initialize();
	ret = ioerror_initialize();
	if (ret < 0)
		return ret;
	ret = vfs_create_fdlock(&g_vfsDummyFdLock);
	if (ret < 0)
		return ret;
	g_vfsInitialized = 1;
	return ret;
}

int vfsRead(IofilemgrReadParam *param)
{
	SceUID fd = param->fd;
	void *data = param->data;
	SceSize size = param->size;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int result, ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	if ((file->flags & SCE_O_RDONLY) == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EACCES;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		ret = vfs_check_vnode_read_access(vp);
		if (ret == 0)
			ret = ksceVopRead(vp, file, data, size, (SceSize *)&result);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = vfs_check_vnode_read_access(vp->core.link_to);
		if (ret == 0)
			ret = ksceVopRead(vp->core.link_to, file, data, size, (SceSize *)&result);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret < 0)
		result = ret;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return result;
}

int vfsWrite(IofilemgrWriteParam *param)
{
	SceUID fd = param->fd;
	const void *data = param->data;
	SceSize size = param->size;
	int result;
	SceVfsFile *file;
	SceVfsVnode *vp;
	SceUInt32 flags;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	flags = file->flags;
	if ((flags & SCE_O_WRONLY) == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EACCES;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL)
		goto direct;
	vfsLockVnode(vp->core.link_to);
	ret = vfs_check_vnode_write_access(vp->core.link_to);
	if (ret == 0) {
		if ((flags & SCE_O_APPEND) != 0)
			ret = ksceVopPwrite(vp->core.link_to, file, data, size,
					    vp->core.link_to->core.size, (SceSize *)&result);
		else
			ret = ksceVopWrite(vp->core.link_to, file, data, size,
					   (SceSize *)&result);
	}
	vfsUnlockVnode(vp->core.link_to);
out:
	if (ret < 0)
		result = ret;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return result;

direct:
	ret = vfs_check_vnode_write_access(vp);
	if (ret == 0) {
		if ((flags & SCE_O_APPEND) != 0)
			ret = ksceVopPwrite(vp, file, data, size, vp->core.size,
					    (SceSize *)&result);
		else
			ret = ksceVopWrite(vp, file, data, size, (SceSize *)&result);
	}
	goto out;
}

int vfsPread(IofilemgrPreadParam *param)
{
	SceUID fd = param->fd;
	void *data = param->data;
	SceSize size = param->size;
	SceOff offset = param->offset;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int result, ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	if ((file->flags & SCE_O_RDONLY) == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EACCES;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		ret = vfs_check_vnode_read_access(vp);
		if (ret == 0)
			ret = ksceVopPread(vp, file, data, size, offset, (SceSize *)&result);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = vfs_check_vnode_read_access(vp->core.link_to);
		if (ret == 0)
			ret = ksceVopPread(vp->core.link_to, file, data, size, offset,
					   (SceSize *)&result);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret < 0)
		result = ret;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return result;
}

int vfsPwrite(IofilemgrPwriteParam *param)
{
	SceUID fd = param->fd;
	const void *data = param->data;
	SceSize size = param->size;
	SceOff offset = param->offset;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int result, ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	if ((file->flags & SCE_O_WRONLY) == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EACCES;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		ret = vfs_check_vnode_write_access(vp);
		if (ret == 0)
			ret = ksceVopPwrite(vp, file, data, size, offset, (SceSize *)&result);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = vfs_check_vnode_write_access(vp->core.link_to);
		if (ret == 0)
			ret = ksceVopPwrite(vp->core.link_to, file, data, size, offset,
					    (SceSize *)&result);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret < 0)
		result = ret;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return result;
}

SceOff vfsLseek(IofilemgrLseekParam *param)
{
	SceUID fd = param->fd;
	int whence = param->whence;
	SceOff offset = param->offset;
	SceVfsFile *file;
	SceVfsVnode *vp;
	SceOff pos;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		pos = ksceVopLseek(vp, file, offset, whence);
	} else {
		vfsLockVnode(vp->core.link_to);
		pos = ksceVopLseek(vp->core.link_to, file, offset, whence);
		vfsUnlockVnode(vp->core.link_to);
	}
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return pos;
}

int vfsClose(IofilemgrCloseParam *param)
{
	SceUID fd = param->fd;
	SceVfsFile *file;
	SceVfsVnode *vp, *dvp;
	SceVfsMount *mnt;
	struct Ncache *nc;
	SceVfsPath path;
	int ret, err, refs;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 1);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (file->flock_busy_count != 0) {
		vfsUnlockVnode(vp);
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EBUSY;
	}
	mnt = vp->core.mnt;
	if (vp->core.link_to == NULL) {
		ret = ksceVopClose(vp, file);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = ksceVopClose(vp->core.link_to, file);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		goto unlock_vnode;
	}
	if (vp->core.ref_count == 1) {
		if (vp->core.flock != NULL) {
			ret = vfs_flock_close(file, file->pid, vp);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 3869, "vfs_flock_close() failed ... 0x%x %d %p\n", fd,
					       fd, file);
			vp->core.flock = NULL;
		}
	} else if (file->has_flock_ent != 0) {
		if (file->is_locked != 0) {
			ret = vfs_flock(file, FLOCK_OP_UN, file->pid, vp);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 3880, "flock unlock failed ... 0x%x %d\n", ret, ret);
		}
		ret = vfs_flock_release(file->pid, vp);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 3888, "flock release failed ... 0x%x %d\n", ret, ret);
	}
	file->state = SCE_VFS_FILE_STATE_CLOSED;
	ret = vfs_finalize_file(vp, file);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 3897, "finalize fd failed ... 0x%x %d %p\n", fd, fd, vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	ret = vfs_close_file(vp, fd);
	refs = vp->core.ref_count;
	if ((vp->core.state & SCE_VNODE_STATE_INACTIVE) == 0 || refs != 1)
		goto release_vnode;
	if ((vp->core.state & SCE_VNODE_STATE_DELETED) != 0 &&
	    vp->core.mnt->state == VFS_MOUNT_STATE_MOUNTED) {
		nc = vp->core.ncache;
		if (nc == NULL) {
			SCE_DBG_PRINTF(0xf, 3943, "unexpected error !! vp=%p vp->ncache=%p\n", vp, NULL);
			vp->core.ref_count--;
			vfsUnlockVnode(vp);
			goto unlocked;
		}
		if (nc->ncdd == NULL) {
			dvp = NULL;
		} else {
			dvp = nc->ncdd->vp;
			if (vp->core.dd != dvp)
				vp->core.dd = dvp;
			err = lock_vdlock(dvp);
			if (err != 0) {
				if (err == SCE_ERROR_ERRNO_EBUSY) {
					vfsUnlockVnode(vp);
					vfsLockVnode(dvp);
					vfsLockVnode(vp);
					refs = vp->core.ref_count;
					if (refs == 1)
						goto remove;
				} else {
					SCE_DBG_PRINTF(0xf, 4015, "unexpected error = %d 0x%x\n", err, err);
					refs = vp->core.ref_count;
				}
				goto release_vnode;
			}
		}
remove:
		nc = vp->core.ncache;
		path.name = nc->name;
		path.name_length = nc->nameLength;
		path.path = NULL;
		ret = ksceVopRemove(dvp, vp, &path, 0);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 3969, "sceVopRemove() ret = %d 0x%x\n", ret, ret);
		ret = clear_ncache(vp->core.ncache, 0);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 3976, "clear_ncache() ret = %d 0x%x\n", ret, ret);
		if (dvp != NULL)
			vfsUnlockVnode(dvp);
		ret = ksceVopInactive(vp);
		if (ret < 0) {
			if (ret != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF(0xf, 3986, "inactive failed ... %d 0x%x\n", ret, ret);
		}
	} else {
		if (vp->core.link_to != NULL) {
			if (vp->core.fd_num != 0) {
				SCE_DBG_PRINTF(0xf, 4023, "vp->fdNum=%d\n", vp->core.fd_num);
				SCE_DBG_ASSERT(0, 4024);
			}
			vfsLockVnode(vp->core.link_to);
			vp->core.link_to->core.linked_num--;
			SCE_DBG_PRINTF(0xf, 4029, "vp->linkTo->linkedNum=%d\n",
				       vp->core.link_to->core.linked_num);
			vp->core.link_to->core.ref_count--;
			vfsUnlockVnode(vp->core.link_to);
		}
		ret = ksceVopInactive(vp);
		if (ret < 0) {
			if (ret != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF(0xf, 4042, "inactive failed ... %d 0x%x\n", ret, ret);
		}
	}
	vfsFreeVnode(vp);
	vfsLockMnt(mnt);
	goto release_mnt;
release_vnode:
	vp->core.ref_count = refs - 1;
unlock_vnode:
	vfsUnlockVnode(vp);
unlocked:
	vfsLockMnt(mnt);
	if (ret != 0) {
		refs = mnt->mnt_ref_count;
		goto check;
	}
release_mnt:
	ret = 0;
	refs = --mnt->mnt_ref_count;
check:
	if (refs < 0)
		SCE_DBG_PRINTF(0xf, 4069, "mnt->mntRefCount = %d\n", refs);
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
		vfs_umount_cleanup(mnt);
	else
		vfsUnlockMnt(mnt);
	return ret;
}

int vfsRmdir(IofilemgrRmdirParam *param)
{
	const char *dir = param->path;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	VfsPathElem *lastElem;
	SceVfsVnode *dvp, *vp;
	SceVfsPath path;
	char *buf;
	int num, last, ret, err;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(dir, buf, elem, &num, 0);
	if (ret < 0)
		goto out;
	if (num == 0) {
		vfs_path_buff_free(buf);
		return ret;
	}
	last = num - 1;
	lastElem = &elem[last];
	ret = vfs_find_in_path(elem, last, &dvp, buf, 0x80020);
	if (ret < 0)
		goto out;
	ret = vfs_check_vnode_rw_access(dvp);
	if (ret < 0)
		goto release_dvp;
	ret = vfs_find_in_vnode(dvp, lastElem, buf, 0x20, &vp);
	if (ret < 0)
		goto release_dvp;
	if ((vp->core.type & SCE_VNODE_TYPE_DIR) == 0) {
		ret = SCE_ERROR_ERRNO_ENOTDIR;
		goto release_vp;
	}
	if (vp->core.type >= SCE_VNODE_TYPE_ROOTDIR || (int)vp->core.fd_num > 0) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	path.name = lastElem->name;
	path.name_length = lastElem->len;
	path.path = buf;
	ret = ksceVopRmdir(dvp, vp, &path, 0);
	if (ret < 0)
		goto release_vp;
	vp->core.state = SCE_VNODE_STATE_INACTIVE;
	if (vp->core.ncache != NULL && (ret = clear_ncache(vp->core.ncache, 1)) < 0)
		SCE_DBG_PRINTF(0xf, 4311, "name cache cleanup failed ... %p\n", vp);
	if (vp->core.ref_count != 1)
		goto release_vp;
	err = ksceVopInactive(vp);
	if (err < 0) {
		if (err != SCE_ERROR_ERRNO_EUNSUP)
			SCE_DBG_PRINTF(0xf, 4325, "inactive failed ... %d 0x%x\n", err, err);
	}
	vfsLockMnt(vp->core.mnt);
	if ((int)--vp->core.mnt->mnt_ref_count < 0)
		SCE_DBG_PRINTF(0xf, 4333, "vp->mnt->mntRefCount = %d\n", vp->core.mnt->mnt_ref_count);
	vfsUnlockMnt(vp->core.mnt);
	ret = vfsFreeVnode(vp);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 4341, "vfsFreeVnode() failed ... vp=%p %d 0x%x\n", vp, ret, ret);
	goto release_dvp;
release_vp:
	vfs_decrement_vp_mnt_ref_inline(vp);
release_dvp:
	vfs_decrement_vp_mnt_ref_inline(dvp);
out:
	vfs_path_buff_free(buf);
	if (ret == VFS_ERROR_RMDIR_IGNORED)
		ret = 0;
	return ret;
}

int vfsGetstat(IofilemgrGetstatParam *param)
{
	const char *file = param->file;
	SceIoStat *stat = param->stat;
	int raw = param->preserveAttributes;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *vp;
	SceVfsPath path;
	char *buf;
	int num, count, ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(file, buf, elem, &num, 0);
	if (ret >= 0 && (count = num) != 0) {
		ret = vfs_find_in_path(elem, count, &vp, buf, 3);
		if (ret >= 0) {
			VfsPathElem *lastElem = &elem[--count];

			path.name = lastElem->name;
			path.name_length = lastElem->len;
			path.path = buf;
			ret = ksceVopGetstat(vp, &path, stat);
			if (ret == 0) {
				if (raw == 0)
					stat->st_attr = 0;
			}
			vfs_decrement_vp_mnt_ref_inline(vp);
		}
	}
	vfs_path_buff_free(buf);
	return ret;
}

int vfsChstat(IofilemgrChstatParam *param)
{
	const char *file = param->file;
	SceIoStat *stat = param->stat;
	int bits = param->bits;
	int forVshbridge = param->forVshbridge;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	VfsPathElem *lastElem;
	SceVfsVnode *vp;
	SceVfsPath path;
	char *buf;
	int num, count, ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(file, buf, elem, &num, 0);
	if (ret < 0 || (count = num) == 0)
		goto out;
	ret = vfs_find_in_path(elem, count, &vp, buf, 4);
	if (ret < 0)
		goto out;
	ret = vfs_check_vnode_chstat_access(vp);
	if (ret < 0)
		goto release;
	if ((bits & VFS_CST_VSHBRIDGE_ONLY) != 0 && forVshbridge == 0)
		bits &= ~VFS_CST_VSHBRIDGE_ONLY;
	if ((bits & SCE_CST_SIZE) != 0) {
		if ((vp->core.type & SCE_VNODE_TYPE_DIR) != 0) {
			ret = SCE_ERROR_ERRNO_EISDIR;
			goto release;
		}
		ret = vfs_check_vnode_rw_access(vp);
		if (ret < 0)
			goto release;
	}
	lastElem = &elem[--count];
	path.name = lastElem->name;
	path.name_length = lastElem->len;
	path.path = buf;
	ret = ksceVopChstat(vp, &path, stat, bits);
release:
	vfs_decrement_vp_mnt_ref_inline(vp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsRemove(IofilemgrRemoveParam *param)
{
	const char *file = param->file;
	int flags = param->flags;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	VfsPathElem *lastElem;
	VfsPathElem tmpElem;
	VfsLookupPath path;
	SceVfsPath tmpPath;
	SceVfsVnode *dvp, *vp, *nvp;
	SceVfsFile *fd;
	char *buf, *tmp;
	int num, last, i, ret, err;
	SceUInt32 type;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(file, buf, elem, &num, 0);
	if (ret < 0)
		goto out;
	if (num == 0) {
		vfs_path_buff_free(buf);
		return ret;
	}
	last = num - 1;
	ret = vfs_find_in_path(elem, last, &dvp, buf, 0x80005);
	if (ret < 0)
		goto out;
	ret = vfs_check_vnode_rw_access(dvp);
	if (ret < 0)
		goto release_dvp;
	lastElem = &elem[last];
	ret = vfs_find_in_vnode(dvp, lastElem, buf, 5, &vp);
	if (ret < 0)
		goto release_dvp;
	type = vp->core.type;
	if ((type & SCE_VNODE_TYPE_REG) == 0) {
		if ((type & SCE_VNODE_TYPE_DIR) != 0)
			ret = SCE_ERROR_ERRNO_EISDIR;
		else
			ret = SCE_ERROR_ERRNO_EPERM;
		goto release_vp;
	}
	if (type >= SCE_VNODE_TYPE_ROOTDIR) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 4578, "%s is mounted\n", file);
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	if ((flags & 1) != 0 && (int)vp->core.fd_num > 0) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	if (vp->core.fd_num == 0)
		goto check_linked;
	do {
		SCE_DBG_ASSERT(vp->core.fd_list != NULL, 4648);
		ret = vfsTrylockFd(vp->core.fd_list);
		if (ret == 0)
			goto whiteout;
		if (ret != SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN)
			goto release_vp;
		vfsUnlockVnode(vp);
		vfsUnlockVnode(dvp);
		ret = vfsLockFdByVnode(vp);
		if (ret < 0) {
			vfsLockVnode(dvp);
			vfsLockVnode(vp);
			goto release_vp;
		}
		vfsLockVnode(dvp);
		vfsLockVnode(vp);
		if (vp->core.fd_num != 0)
			goto whiteout;
check_linked:
		;
	} while (vp->core.linked_num != 0);
	path.path.name = lastElem->name;
	path.path.name_length = lastElem->len;
	path.path.path = buf;
	ret = ksceVopRemove(dvp, vp, &path.path, 0);
	if (ret < 0)
		goto release_vp;
	vp->core.state = SCE_VNODE_STATE_INACTIVE;
	vfs_lock_ncache();
	if (vp->core.ncache != NULL && (ret = clear_ncache(vp->core.ncache, 0)) < 0)
		SCE_DBG_PRINTF(0xf, 4616, "name cache cleanup failed ... %p\n", vp);
	vfs_unlock_ncache();
	if (vp->core.ref_count != 1)
		goto release_vp;
	ret = ksceVopInactive(vp);
	if (ret < 0 && ret != SCE_ERROR_ERRNO_EUNSUP)
		SCE_DBG_PRINTF(0xf, 4628, "inactive failed ... %d 0x%x\n", ret, ret);
	vfsLockMnt(vp->core.mnt);
	if ((int)--vp->core.mnt->mnt_ref_count < 0)
		SCE_DBG_PRINTF(0xf, 4636, "vp->mnt->mntRefCount = %d\n", vp->core.mnt->mnt_ref_count);
	vfsUnlockMnt(vp->core.mnt);
	vfsFreeVnode(vp);
	goto release_dvp;

whiteout:
	ret = ksceVopSync(vp, vp->core.fd_list, 0);
	if (ret < 0)
		goto unlock_fd;
	path.path.name = lastElem->name;
	path.path.name_length = lastElem->len;
	path.path.path = buf;
	path.elem = NULL;
	tmp = vfs_path_buff_alloc();
	if (tmp == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		vfsUnlockFd(vp->core.fd_list);
		goto release_vp;
	}
	strncpy(tmp, lastElem->name, lastElem->len);
	__builtin_memcpy(tmp, "SCEDEL~", 7);
	tmpPath.path = NULL;
	tmpPath.name = tmp;
	i = -2;
	do {
		tmp[++i + 7] = '~';
		tmpPath.name_length = strnlen(tmp, VFS_PATH_BUFF_SIZE);
		nvp = NULL;
		ret = ksceVopWhiteout(dvp, vp, &path.path, &tmpPath, 0, 0);
		if (ret == SCE_ERROR_ERRNO_EUNSUP)
			ret = ksceVopRename(dvp, vp, &path.path, NULL, &nvp, &tmpPath);
	} while (i < 0x3f8 && ret == SCE_ERROR_ERRNO_EEXIST);
	if (ret < 0) {
		if (ret == SCE_ERROR_ERRNO_EEXIST)
			SCE_DBG_PRINTF(0xf, 4743, "can not create temporal file %s\n", tmp);
		vfs_path_buff_free(tmp);
		goto unlock_fd;
	}
	if (nvp == NULL) {
		vfsUnlockFd(vp->core.fd_list);
	} else {
		fd = vp->core.fd_list;
		nvp->core.fd_list = fd;
		nvp->core.bc = vp->core.bc;
		nvp->core.fd_num = vp->core.fd_num;
		do {
			fd->vp = nvp;
			fd = fd->next;
		} while (fd != NULL);
		vfsUnlockFd(nvp->core.fd_list);
		nvp->core.ref_count += nvp->core.fd_num;
		vp->core.fd_num = 0;
		vp->core.fd_list = NULL;
		vp->core.bc = NULL;
		vp->core.ref_count -= nvp->core.fd_num;
		if (vp->core.flock != NULL) {
			err = vfs_flock_dup(vp, nvp);
			if (err < 0)
				SCE_DBG_PRINTF(0xf, 4778, "can not dup flock %d 0x%x\n", err, err);
			err = vfs_release_lock_entry(vp);
			if (err < 0)
				SCE_DBG_PRINTF(0xf, 4784, "vfs_release_lock_entry failed %d 0x%x\n", err,
					       err);
		}
	}
	vp->core.state = SCE_VNODE_STATE_INACTIVE | SCE_VNODE_STATE_DELETED;
	if (nvp != NULL)
		nvp->core.state = SCE_VNODE_STATE_INACTIVE | SCE_VNODE_STATE_DELETED;
	vfs_lock_ncache();
	if (vp->core.ncache != NULL && (ret = clear_ncache(vp->core.ncache, 0)) < 0)
		SCE_DBG_PRINTF(0xf, 4803, "name cache cleanup failed ... %p\n", vp);
	vfs_unlock_ncache();
	tmpElem.name = tmp;
	if (nvp == NULL) {
		tmpElem.len = strnlen(tmp, VFS_PATH_BUFF_SIZE);
		ret = enter_ncache(dvp, vp, &tmpElem, NCACHE_STATE_WHITEOUT, 1);
		vfs_path_buff_free(tmp);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 4822, "enter_negavie_ncache() failed ... %p %p\n", vp,
				       vp->core.ncache);
	} else {
		tmpElem.len = strnlen(tmp, VFS_PATH_BUFF_SIZE);
		ret = enter_ncache(dvp, nvp, &tmpElem, NCACHE_STATE_WHITEOUT, 1);
		vfs_path_buff_free(tmp);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 4845, "enter_negavie_ncache() failed ... %p %p\n", vp,
				       vp->core.ncache);
	}
	if (nvp != NULL) {
		nvp->core.ref_count--;
		vfsUnlockVnode(nvp);
	}
	if (vp->core.ref_count != 1)
		goto release_vp;
	ret = ksceVopInactive(vp);
	if (ret < 0 && ret != SCE_ERROR_ERRNO_EUNSUP)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 4864, "inactive failed ... %d 0x%x\n", ret, ret);
	vfsLockMnt(vp->core.mnt);
	if ((int)--vp->core.mnt->mnt_ref_count < 0)
		SCE_DBG_PRINTF(0xf, 4872, "vp->mnt->mntRefCount = %d\n", vp->core.mnt->mnt_ref_count);
	vfsUnlockMnt(vp->core.mnt);
	vfsFreeVnode(vp);
	goto release_dvp;
unlock_fd:
	vfsUnlockFd(vp->core.fd_list);
release_vp:
	vfs_decrement_vp_mnt_ref_inline(vp);
release_dvp:
	vfs_decrement_vp_mnt_ref_inline(dvp);
out:
	vfs_path_buff_free(buf);
	if (ret == VFS_ERROR_RMDIR_IGNORED)
		ret = 0;
	return ret;
}

int vfsUmount(SceVfsUmountParam *param)
{
	const char *assignName = param->assign_name;
	int flag = param->flag;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsUmountParam sub;
	struct {
		SceVfsMountData *mnt_data;
		char path[sizeof(((SceVfsMount *)0)->path)];
	} subMount;
	SceVfsVnode *rvp, *vp;
	SceVfsMount *mnt, *cur, *pmnt;
	BufferPool *pool;
	SceSize size;
	char *buf;
	int num, avail, force, i, ret, err;
	SceUInt32 state;
	SceUInt8 fsType;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(assignName, buf, elem, &num, 1);
	if (ret < 0 || num == 0)
		goto out;
	rfs_lock();
	ret = vfs_find_in_path(elem, num, &rvp, buf, 0x6000);
	if (ret < 0)
		goto unlock_rfs;
	mnt = rvp->core.mnt;
	if (rvp->core.type < SCE_VNODE_TYPE_ROOTDIR) {
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto release_vp;
	}
	vfsLockMnt(mnt);
	for (cur = mnt->mnted_on_list; cur != NULL; cur = mnt->mnted_on_list) {
		while (((rvp->core.type & SCE_VNODE_TYPE_ROOTDIR) != SCE_VNODE_TYPE_ROOTDIR &&
			cur->mnt_vnode != rvp) || cur->state != VFS_MOUNT_STATE_MOUNTED) {
			cur = cur->mnted_on_list_prev;
			if (cur == NULL)
				goto umount;
		}
		memcpy(&subMount, &cur->mnt_data, sizeof(subMount));
		vfsUnlockMnt(mnt);
		vfsUnlockVnode(rvp);
		sub.assign_name = subMount.path;
		sub.flag = flag;
		ret = vfsUmount(&sub);
		vfsLockVnode(rvp);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xf, 5571, "Umount failed ... %s mode=%d ret=0x%x\n",
				       sub.assign_name, sub.flag, ret);
			goto release_vp;
		}
		vfsLockMnt(mnt);
	}
umount:
	state = rvp->core.state;
	if (state > 0xff) {
		if ((state & SCE_VNODE_STATE_UNMOUNTED) != 0)
			SCE_DBG_PRINTF(0xf, 5586, "vp status is not expected value 0x%x\n", state);
		ret = 0;
		vfsUnlockMnt(mnt);
		goto release_vp;
	}
	if ((rvp->core.type & SCE_VNODE_TYPE_DEV) != 0) {
		ret = ksceVopSync(rvp, rvp->core.fd_list, 0);
		vfsUnlockMnt(mnt);
		goto release_vp;
	}
	if ((int)mnt->mnt_ref_count > 1 && (flag & 3) == 0) {
		vfsUnlockMnt(mnt);
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	force = flag & SCE_VFS_UMOUNT_FLAG_FORCE;
	if (force != 0) {
		vfsLockFdAll(mnt);
		mnt->fd_lock->state = FDLOCK_STATE_BUSY;
		vfsUnlockFdAll(mnt);
		pool = vfs_get_vnode_pool();
		size = pool->size;
		i = 0;
		do {
			avail = vfs_get_available_vnode_num();
			vp = pool->base;
			for (; i < avail; i++, vp = (SceVfsVnode *)((char *)vp + size)) {
				err = lock_vdlock(vp);
				while (err == SCE_ERROR_ERRNO_EBUSY) {
					if (rvp->core.dd == vp)
						goto next;
					vfsUnlockMnt(mnt);
					sceKernelDelayThread(100);
					vfsLockMnt(mnt);
					err = lock_vdlock(vp);
				}
				if (err != 0) {
					SCE_DBG_PRINTF(0xf, 5669, "vfsTrylockVnode=0x%x %d\n", err, err);
					goto next;
				}
				if (vp->core.mnt == mnt && vp->core.state < 0x1000) {
					if (vp->core.link_to == NULL) {
						err = ksceVopSync(vp, vp->core.fd_list, 1);
					} else {
						vfsLockVnode(vp->core.link_to);
						err = ksceVopSync(vp->core.link_to,
								  vp->core.link_to->core.fd_list, 1);
						vfsUnlockVnode(vp->core.link_to);
					}
					if (err < 0) {
						SCE_DBG_PRINTF(0xf, 5688, "vop sync failed vp=%p\n", vp);
						if (vp->core.ncache != NULL)
							SCE_DBG_PRINTF(0xf, 5691, "ncache name is %s\n",
								       vp->core.ncache->name);
						SCE_DBG_PRINTF(0xf, 5699, "some file/dir in %s\n",
							       mnt->mnt_data->assign_name);
					}
				}
				vfsUnlockVnode(vp);
next:
				;
			}
		} while (vfs_get_available_vnode_num() != avail);
		ret = sceVfsOpUmount(mnt, flag);
		if (ret < 0) {
			vfsLockFdAll(mnt);
			mnt->fd_lock->state = 0;
			vfsSignalFd(mnt);
			vfsUnlockFdAll(mnt);
			goto fail;
		}
		vfsLockFdAll(mnt);
		mnt->fd_lock->state = FDLOCK_STATE_UMOUNTING;
		vfsSignalFd(mnt);
		vfsUnlockFdAll(mnt);
	} else {
		ret = sceVfsOpUmount(mnt, flag);
		if (ret < 0)
			goto fail;
	}
	vfs_clear_mnt_sync_request(mnt);
	rvp->core.state = SCE_VNODE_STATE_UNMOUNTED;
	mnt->state = VFS_MOUNT_STATE_UMOUNTING;
	vp = rfs_remove_entry(elem, num);
	if (vp != rvp) {
		SCE_DBG_PRINTF(0xf, 5738, "failed ... returned data %p is not expected val %p\n", vp,
			       rvp);
		vp = rvp;
	}
	if (vp->core.ncache != NULL && (ret = clear_ncache(vp->core.ncache, 1)) < 0)
		SCE_DBG_PRINTF(0xf, 5745, "name cache cleanup failed ... %p\n", rvp);
	if (mnt->fs_type == SCE_VFS_FS_TYPE_HOSTFS)
		g_vfsHostfsAssignName = NULL;
	if (force != 0 && (ret = vfs_set_fumount_vop(mnt)) < 0)
		goto fail;
	if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0) {
		*mnt->fumount->devState = 1;
		vfs_signal_fumount(mnt);
	}
	fsType = mnt->fs_type;
	if (fsType != SCE_VFS_FS_TYPE_BLKDEV && fsType != SCE_VFS_FS_TYPE_CHRDEV &&
	    fsType != SCE_VFS_FS_TYPE_HOSTFS && mnt->mnt_vnode != NULL &&
	    (mnt->mnt_vnode->core.type & SCE_VNODE_TYPE_DEV) != 0 && force != 0) {
		pmnt = mnt->mnt_vnode->core.mnt;
		vfsLockMnt(pmnt);
		vfs_remove_mntedon_list(pmnt, mnt);
		pmnt->mnt_ref_count--;
		vfsUnlockMnt(pmnt);
		if (mnt->mnt_vnode != NULL) {
			vfsLockVnode(mnt->mnt_vnode);
			vp = mnt->mnt_vnode;
			vp->core.type &= ~SCE_VNODE_TYPE_MOUNTED;
			vp->core.ref_count--;
			vfsUnlockVnode(vp);
			mnt->mnt_vnode = NULL;
		}
	}
	if (rvp->core.ref_count != 1) {
		vfsUnlockMnt(mnt);
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 5831, "UMOUNT not cleanup mnt=%p vp->refCount = %d\n",
				     mnt, rvp->core.ref_count);
		goto release_vp;
	}
	ret = ksceVopInactive(rvp);
	if (ret < 0 && ret != SCE_ERROR_ERRNO_EUNSUP)
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 5807, "inactive failed ... %d 0x%x\n", ret, ret);
	vfsFreeVnode(rvp);
	if (mnt->mnt_ref_count != 1 || mnt->opened_entry_num != 0) {
		vfsUnlockMnt(mnt);
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 5823,
				     "UMOUNT not cleanup mnt=%p mnt->mntRefCount = %d mnt->openedEntryNum=%d\n",
				     mnt, mnt->mnt_ref_count, mnt->opened_entry_num);
		goto release_mnt;
	}
	goto cleanup;
fail:
	vfsUnlockMnt(mnt);
release_vp:
	rvp->core.ref_count--;
	vfsUnlockVnode(rvp);
release_mnt:
	vfsLockMnt(mnt);
	if (--mnt->mnt_ref_count != 0 || mnt->state != VFS_MOUNT_STATE_UMOUNTING) {
		vfsUnlockMnt(mnt);
		goto unlock_rfs;
	}
cleanup:
	vfs_umount_cleanup(mnt);
unlock_rfs:
	rfs_unlock();
out:
	vfs_path_buff_free(buf);
	return ret;
}

SceUID vfsDopen(IofilemgrDopenParam *param)
{
	const char *dirname = param->dirname;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *vp;
	int num;
	SceVfsFile *file;
	SceVfsPath path;
	char *buf;
	SceUID fd;
	int ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(dirname, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	ret = vfs_find_in_path(elem, num, &vp, buf, 6);
	if (ret < 0)
		goto out;
	ret = vfs_check_vnode_read_access(vp);
	if (ret < 0) {
		if (ret == SCE_ERROR_ERRNO_EACCES || ret == VFS_ERROR_PFS_EACCES)
			ret = SCE_ERROR_ERRNO_ENOENT;
		goto out;
	}
	if ((vp->core.type & SCE_VNODE_TYPE_DIR) == 0) {
		ret = SCE_ERROR_ERRNO_ENOTDIR;
		goto release;
	}
	fd = vfs_allocate_file(vp, &file, elem[num - 1].name);
	ret = fd;
	if (fd < 0)
		goto release;
	vfs_initialize_file(vp, file);
	path.path = buf;
	path.name = elem[num - 1].name;
	path.name_length = elem[num - 1].len;
	ret = ksceVopDopen(vp, &path, file);
	if (ret < 0) {
		vfs_finalize_file(vp, file);
		vfs_close_file(vp, fd);
		goto release;
	}
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0) {
		vfs_debug_set_resolved_path(file, dirname);
		vfs_debug_set_open_param(file, 0, 0);
	}
	vfsUnlockVnode(vp);
	file->is_dir = 1;
	if (g_vfsSuspended > 0 && (file->idata & VFS_FILE_ATTR_INTERNAL) != 0)
		file->state = SCE_VFS_FILE_STATE_UNK;
	ret = fd;
	goto out;
release:
	vfs_decrement_vp_mnt_ref_inline(vp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsDread(IofilemgrDreadParam *param)
{
	SceUID fd = param->fd;
	SceIoDirent *dir = param->dir;
	int raw = param->preserveAttributes;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_ENOTDIR;
	}
	for (;;) {
		vfsLockFd(file);
		ret = vfs_check_fd_status(file, 0);
		if (ret != 0) {
			vfsUnlockFd(file);
			vfs_release_file(fd);
			return ret;
		}
		vp = file->vp;
		if (lock_vdlock(vp) != SCE_ERROR_ERRNO_EBUSY)
			break;
		if (vp->core.type < SCE_VNODE_TYPE_ROOTDIR) {
			vfsLockVnode(vp);
			break;
		}
		vfsUnlockFd(file);
	}
	ret = ksceVopDread(vp, file, dir);
	if (raw == 0 && ret > 0)
		dir->d_stat.st_attr = 0;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int vfsDclose(IofilemgrDcloseParam *param)
{
	SceUID fd = param->fd;
	SceVfsFile *file;
	SceVfsVnode *vp;
	SceVfsMount *mnt;
	int ret, err, refs;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir == 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_ENOTDIR;
	}
	for (;;) {
		vfsLockFd(file);
		ret = vfs_check_fd_status(file, 1);
		if (ret != 0) {
			vfsUnlockFd(file);
			vfs_release_file(fd);
			return ret;
		}
		vp = file->vp;
		if (lock_vdlock(vp) != SCE_ERROR_ERRNO_EBUSY)
			break;
		if (vp->core.type < SCE_VNODE_TYPE_ROOTDIR) {
			vfsLockVnode(vp);
			break;
		}
		vfsUnlockFd(file);
	}
	mnt = vp->core.mnt;
	ret = ksceVopDclose(vp, file);
	if (ret == 0) {
		file->state = SCE_VFS_FILE_STATE_CLOSED;
		err = vfs_finalize_file(vp, file);
		if (err < 0)
			SCE_DBG_PRINTF(0xf, 6139, "finalize file failed ret=0x%x fd=0x%x vp=%p\n",
				       err, fd, vp);
		vfsUnlockFd(file);
		vfs_release_file(fd);
		err = vfs_close_file(vp, fd);
		if (err < 0)
			SCE_DBG_PRINTF(0xf, 6148, "free file failed ... 0x%x %d %p\n", fd, fd, vp);
		refs = --vp->core.ref_count;
		if ((vp->core.state & SCE_VNODE_STATE_INACTIVE) != 0 && refs == 0) {
			err = ksceVopInactive(vp);
			if (err < 0) {
				if (err != SCE_ERROR_ERRNO_EUNSUP)
					SCE_DBG_PRINTF(0xf, 6160, "inactive failed ... %d 0x%x\n", err, err);
			}
			vfsFreeVnode(vp);
		} else {
			vfsUnlockVnode(vp);
		}
		vfsLockMnt(mnt);
		refs = --mnt->mnt_ref_count;
		if (refs < 0)
			SCE_DBG_PRINTF(0xf, 6183, "mnt->mntRefCount = %d\n", refs);
	} else {
		vfsUnlockVnode(vp);
		vfsUnlockFd(file);
		vfs_release_file(fd);
		vfsLockMnt(mnt);
	}
	if (mnt->state == VFS_MOUNT_STATE_UMOUNTING && mnt->mnt_ref_count == 0)
		vfs_umount_cleanup(mnt);
	else
		vfsUnlockMnt(mnt);
	return ret;
}

int vfsSync(IofilemgrSyncParam *param)
{
	const char *device = param->device;
	unsigned int flag = param->flag;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *vp, *cur;
	SceVfsMount *mnt;
	BufferPool *pool;
	SceSize size;
	char *buf;
	int num, avail, i = 0, ret, err;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(device, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	ret = vfs_find_in_path(elem, num, &vp, buf, 0x50);
	if (ret < 0)
		goto out;
	if ((vp->core.type & SCE_VNODE_TYPE_ROOTDIR) == SCE_VNODE_TYPE_ROOTDIR) {
		mnt = vp->core.mnt;
		vfsUnlockVnode(vp);
		pool = vfs_get_vnode_pool();
		size = pool->size;
		do {
			avail = vfs_get_available_vnode_num();
			cur = (SceVfsVnode *)((char *)pool->base + size * i);
			for (; i < avail; i++, cur = (SceVfsVnode *)((char *)cur + size)) {
				vfsLockVnode(cur);
				if (cur->core.mnt == mnt && cur != vp && cur->core.state < 0x1000) {
					if (cur->core.link_to == NULL) {
						err = ksceVopSync(cur, cur->core.fd_list, 1);
					} else {
						vfsLockVnode(cur->core.link_to);
						err = ksceVopSync(cur->core.link_to,
								  cur->core.link_to->core.fd_list, 1);
						vfsUnlockVnode(cur->core.link_to);
					}
					if (err < 0) {
						SCE_DBG_PRINTF(0xf, 6464, "vop sync failed vp=%p\n", cur);
						if (cur->core.ncache != NULL)
							SCE_DBG_PRINTF(0xf, 6467, "ncache name is %s\n",
								       cur->core.ncache->name);
						SCE_DBG_PRINTF(0xf, 6475, "some file/dir in %s\n",
							       mnt->mnt_data->assign_name);
					}
				}
				vfsUnlockVnode(cur);
			}
		} while (vfs_get_available_vnode_num() != avail);
		vfsLockVnode(vp);
		ret = sceVfsOpSync(vp->core.mnt, flag);
		if (ret == 0)
			vfs_clear_mnt_sync_request(vp->core.mnt);
	} else if (vp->core.link_to == NULL) {
		ret = ksceVopSync(vp, vp->core.fd_list, 0);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = ksceVopSync(vp->core.link_to, vp->core.link_to->core.fd_list, 0);
		vfsUnlockVnode(vp->core.link_to);
	}
	vfs_decrement_vp_mnt_ref_inline(vp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsDevctl(IofilemgrDevctlParam *param)
{
	const char *dev = param->dev;
	unsigned int cmd = param->cmd;
	void *indata = param->indata;
	int inlen = param->inlen;
	void *outdata = param->outdata;
	int outlen = param->outlen;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	int num;
	SceVfsVnode *vp;
	SceUID pid;
	char *buf;
	int ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(dev, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	ret = vfs_find_in_path(elem, num, &vp, buf, 0x60);
	if (ret < 0)
		goto out;
	if ((vp->core.type & SCE_VNODE_TYPE_ROOTDIR) != SCE_VNODE_TYPE_ROOTDIR) {
		ret = SCE_ERROR_ERRNO_ENODEV;
		if ((vp->core.type & (SCE_VNODE_TYPE_ROOTDIR_DEVFS)) == 0)
			goto release;
	}
	if (cmd == VFS_DEVCTL_SYSTEM_ONLY) {
		pid = ksceKernelGetProcessIdFromTLS();
		if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(pid) == 0 && SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid) == 0) {
			ret = SCE_ERROR_ERRNO_EUNSUP;
			goto release;
		}
	}
	ret = ksceVfsOpDevctl(vp->core.mnt, dev, cmd, indata, inlen, outdata, outlen);
release:
	vfs_decrement_vp_mnt_ref_inline(vp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsSyncByFd(IofilemgrSyncByFdParam *param)
{
	SceUID fd = param->fd;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	if ((file->flags & SCE_O_WRONLY) == 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EACCES;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		ret = ksceVopSync(vp, file, 2);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = ksceVopSync(vp->core.link_to, file, 2);
		vfsUnlockVnode(vp->core.link_to);
	}
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int vfs_reclaim_vnode(SceVfsMount *mnt, SceVfsVnode **vpp)
{
	BufferPool *pool = vfs_get_vnode_pool();
	SceSize size = pool->size;
	int start = g_vfsReclaimIndex;
	SceVfsVnode *vp, *dvp;
	int avail, i;

	for (;;) {
		avail = vfs_get_available_vnode_num();
		vp = (SceVfsVnode *)((char *)pool->base + size * start);
		for (i = start; i < avail; i++, vp = (SceVfsVnode *)((char *)vp + size)) {
			if (lock_vdlock(vp) != 0)
				continue;
			if (vp->core.ref_count != 0 || (vp->core.state & SCE_VNODE_STATE_ACTIVE) == 0 ||
			    vp->core.type > (SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_REG)) {
				vfsUnlockVnode(vp);
				continue;
			}
			vfsLockMnt(vp->core.mnt);
			if (vp->core.mnt->state != VFS_MOUNT_STATE_MOUNTED ||
			    (vp->core.mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_NO_RECLAIM) != 0) {
				vfsUnlockMnt(vp->core.mnt);
				vfsUnlockVnode(vp);
				continue;
			}
			vfsUnlockMnt(vp->core.mnt);
			if ((vp->core.type & SCE_VNODE_TYPE_DIR) != 0) {
				vfs_lock_ncache();
				if (vp->core.ncache != NULL && vp->core.ncache->child != NULL) {
					vfs_unlock_ncache();
					vfsUnlockVnode(vp);
					continue;
				}
				vfs_unlock_ncache();
			}
			dvp = vp->core.dd;
			if (dvp == NULL) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 6763, "vp=%p does not have parent vnode\n", vp);
				vfsUnlockVnode(vp);
				continue;
			}
			if (lock_vdlock(dvp) != 0) {
				vfsUnlockVnode(vp);
				continue;
			}
			if (vp->core.fd_num != 0 || vp->core.linked_num != 0) {
				SCE_DBG_PRINTF(0xf, 6776, "try to reclaim %p but fdNum = %d linkedNum = %d\n",
					       vp, vp->core.fd_num, vp->core.linked_num);
				SCE_DBG_ASSERT(0, 6777);
			}
			clear_ncache(vp->core.ncache, 0);
			ksceVopInactive(vp);
			vp->core.state = SCE_VNODE_STATE_RECLAIMED;
			vfsUnlockVnode(vp);
			vfsUnlockVnode(dvp);
			*vpp = vp;
			vfsLockMnt(vp->core.mnt);
			vfs_remove_mnt_vnode_list(vp->core.mnt, vp);
			vfsUnlockMnt(vp->core.mnt);
			if (i + 1 < avail)
				g_vfsReclaimIndex = i + 1;
			else
				g_vfsReclaimIndex = 0;
			return 0;
		}
		if (start == 0)
			return SCE_ERROR_ERRNO_ENOMEM;
		start = 0;
	}
}

SceUID vfsAllocateFile(SceVfsVnode *vp, SceVfsFile **file, const char *name)
{
	SceUID fd;

	fd = vfs_allocate_file(vp, file, name);
	if (fd >= 0)
		vfs_initialize_file(vp, *file);
	return fd;
}

int vfsFreeFile(SceVfsVnode *vp, SceUID fd)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	ret = vfs_finalize_file(vp, file);
	if (ret < 0)
		SCE_DBG_PRINTF(0xf, 6856, "finalize_file failed ... 0x%x %d %p\n", fd, fd, vp);
	vfs_release_file(fd);
	return vfs_close_file(vp, fd);
}

SceUID vfsAllocateFileDummy(int flags, int isDir, const char *path)
{
	SceVfsFile *file;
	SceUID fd;

	fd = vfs_allocate_file_dummy(&file, path);
	if (fd < 0)
		return fd;
	file->flags = flags;
	file->state = SCE_VFS_FILE_STATE_DUMMY;
	file->position = 0;
	file->vp = NULL;
	file->next = NULL;
	if (isDir != 0)
		file->is_dir = 1;
	else
		file->is_dir = 0;
	file->fd = 0;
	file->pid = ksceKernelGetProcessIdFromTLS();
	file->fd_lock = &g_vfsDummyFdLock;
	file->flock_busy_count = 0;
	file->has_flock_ent = 0;
	file->idata = 0;
	__builtin_memset(file->ioSchedData, 0, sizeof(file->ioSchedData));
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0)
		vfs_debug_path_create(file, 0);
	return fd;
}

int SceIofilemgrForDriver_3A79FAC9(SceUID fd)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->state != SCE_VFS_FILE_STATE_DUMMY) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (SceDipswForDriver_A98FC2FD(0xd2) != 0)
		vfs_debug_path_free(file);
	vfs_release_file(fd);
	return vfs_close_file_dummy(fd);
}

int vfsGetVnodeName(SceVfsVnode *vp, char *name, SceSize n, SceSize *result)
{
	return vfs_get_vnode_name(vp, name, n, result);
}

int vfsGetstatByFd(IofilemgrGetstatByFdParam *param)
{
	SceUID fd = param->fd;
	SceIoStat *stat = param->stat;
	SceVfsFile *file;
	SceVfsVnode *vp;
	VfsLookupPath path;
	SceSize len;
	char *buf;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	buf = vfs_path_buff_alloc();
	if (buf == NULL) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		vfs_path_buff_free(buf);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	if (vp->core.link_to == NULL) {
		ret = ksceVopFgetstat(vp, file, stat);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = ksceVopFgetstat(vp->core.link_to, file, stat);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret == SCE_ERROR_ERRNO_EUNSUP) {
		if (vp->core.link_to == NULL) {
			ret = vfs_get_vnode_name(vp, buf, 0x400, &len);
		} else {
			vfsLockVnode(vp->core.link_to);
			ret = vfs_get_vnode_name(vp->core.link_to, buf, 0x400, &len);
			vfsUnlockVnode(vp->core.link_to);
		}
		if (ret < 0) {
			vfsUnlockVnode(vp);
			vfsUnlockFd(file);
			vfs_release_file(fd);
			vfs_path_buff_free(buf);
			return ret;
		}
		path.path.name = buf;
		path.path.name_length = len;
		path.path.path = NULL;
		path.elem = NULL;
		if (vp->core.link_to == NULL) {
			ret = ksceVopGetstat(vp, &path.path, stat);
		} else {
			vfsLockVnode(vp->core.link_to);
			ret = ksceVopGetstat(vp->core.link_to, &path.path, stat);
			vfsUnlockVnode(vp->core.link_to);
		}
	}
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	vfs_path_buff_free(buf);
	if (ret == 0)
		stat->st_attr = 0;
	return ret;
}

int vfsChstatByFd(IofilemgrChstatByFdParam *param)
{
	SceUID fd = param->fd;
	SceIoStat *stat = param->stat;
	unsigned int bits = param->bits;
	SceVfsFile *file;
	SceVfsVnode *vp;
	SceVfsPath path;
	SceUInt32 flags;
	SceSize len;
	char *buf;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	buf = vfs_path_buff_alloc();
	if (buf == NULL) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	flags = file->flags;
	vfsLockVnode(vp);
	ret = vfs_check_vnode_chstat_access(vp);
	if (ret < 0)
		goto out;
	if ((bits & SCE_CST_SIZE) != 0) {
		ret = SCE_ERROR_ERRNO_EISDIR;
		if (file->is_dir != 0)
			goto out;
		if ((flags & SCE_O_WRONLY) == 0) {
			ret = SCE_ERROR_ERRNO_EACCES;
			goto out;
		}
		ret = vfs_check_vnode_rw_access(vp);
		if (ret < 0)
			goto out;
	}
	if (vp->core.link_to == NULL) {
		ret = ksceVopFchstat(vp, file, stat, bits);
	} else {
		vfsLockVnode(vp->core.link_to);
		ret = ksceVopFchstat(vp->core.link_to, file, stat, bits);
		vfsUnlockVnode(vp->core.link_to);
	}
	if (ret == SCE_ERROR_ERRNO_EUNSUP) {
		if (vp->core.link_to == NULL) {
			ret = vfs_get_vnode_name(vp, buf, 0x400, &len);
		} else {
			vfsLockVnode(vp->core.link_to);
			ret = vfs_get_vnode_name(vp->core.link_to, buf, 0x400, &len);
			vfsUnlockVnode(vp->core.link_to);
		}
		if (ret >= 0) {
			path.name = buf;
			path.name_length = len;
			path.path = NULL;
			if (vp->core.link_to == NULL) {
				ret = ksceVopChstat(vp, &path, stat, bits);
			} else {
				vfsLockVnode(vp->core.link_to);
				ret = ksceVopChstat(vp->core.link_to, &path, stat, bits);
				vfsUnlockVnode(vp->core.link_to);
			}
		}
	}
out:
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	vfs_path_buff_free(buf);
	return ret;
}

int vfsRmdev(IofilemgrRmdevParam *param)
{
	const char *dev = param->dev;
	unsigned int flags = param->flags;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	VfsPathElem *lastElem;
	SceVfsVnode *dvp, *vp;
	SceVfsPath path;
	char *buf;
	int num, last, ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(dev, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	last = num - 1;
	ret = vfs_find_in_path(elem, last, &dvp, buf, 5);
	if (ret < 0)
		goto out;
	ret = vfs_find_in_vnode(dvp, &elem[last], buf, 5, &vp);
	if (ret < 0)
		goto release_dvp;
	ret = vp->core.type;
	if ((ret & SCE_VNODE_TYPE_DEV) == 0) {
		ret = SCE_ERROR_ERRNO_EPERM;
		goto release_vp;
	}
	if (ret != SCE_VNODE_TYPE_DEV) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 7381, "%s is mounted vp=%p type=0x%x\n", dev, vp, ret);
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	if ((flags & VFS_RMDEV_FLAG_BUSY_CHECK) != 0 && (int)vp->core.fd_num > 0) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto release_vp;
	}
	if (vp->core.linked_num != 0) {
		SCE_DBG_PRINTF(0xf, 7396, "devfile %s is linked to %p\n", dev, vp->core.link_to);
		SCE_DBG_ASSERT(0, 7397);
	}
	ret = ksceVopSync(vp, vp->core.fd_list, 0);
	if (ret < 0)
		goto release_vp;
	lastElem = &elem[last];
	path.name = lastElem->name;
	path.name_length = lastElem->len;
	path.path = buf;
	ret = ksceVopRemove(dvp, vp, &path, 0);
	if (ret < 0)
		goto release_vp;
	vp->core.state = SCE_VNODE_STATE_INACTIVE;
	if (vp->core.ncache != NULL && (ret = clear_ncache(vp->core.ncache, 0)) < 0)
		SCE_DBG_PRINTF(0xf, 7424, "name cache cleanup failed ... %p\n", vp);
	if (vp->core.bc != NULL) {
		ret = vfsBuffFree(vp->core.bc, 1, vp, vp->core.fd_list);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 7434, "vfsBuffFree() failed ... 0x%x %d %p\n", ret,
					     ret, vp);
			goto release_vp;
		}
		vp->core.bc = NULL;
	}
	if (vp->core.ref_count == 1) {
		ret = ksceVopInactive(vp);
		if (ret < 0) {
			if (ret != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 7448, "inactive failed ... %d 0x%x\n", ret, ret);
		}
		vfsLockMnt(vp->core.mnt);
		if ((int)--vp->core.mnt->mnt_ref_count < 0)
			SCE_DBG_PRINTF(0xf, 7456, "vp->mnt->mntRefCount = %d\n",
				       vp->core.mnt->mnt_ref_count);
		vfsUnlockMnt(vp->core.mnt);
		vfsFreeVnode(vp);
		goto release_dvp;
	}
	if ((int)vp->core.fd_num > 0)
		ret = vfs_set_fumount_vop_vnode(vp);
release_vp:
	vfs_decrement_vp_mnt_ref_inline(vp);
release_dvp:
	vfs_decrement_vp_mnt_ref_inline(dvp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsFlock(IofilemgrFlockParam *param)
{
	SceUID fd = param->fd;
	int op = param->op;
	SceVfsFile *file;
	SceVfsVnode *vp;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0) {
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return ret;
	}
	vp = file->vp;
	vfsLockVnode(vp);
	file->flock_busy_count++;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	ret = vfs_flock(file, op, file->pid, vp);
	vfsLockFd(file);
	vfsLockVnode(vp);
	file->flock_busy_count--;
	vfsUnlockVnode(vp);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int vfsBuffCtl(IofilemgrBuffCtlParam *param)
{
	unsigned int cmd = param->cmd;
	int mode = param->mode;
	SceUID fd = param->fd;
	SceUInt32 mask = param->mask;
	VfsBuffCacheInfo *info = param->outdata;
	SceSize outlen = param->outlen;
	SceSize dataCacheSize, lineSize, ioCacheSize;
	SceUInt32 wayNum;
	struct BuffCache *bc;
	SceVfsVnode *vp, *lvp;
	SceVfsMount *mnt;
	SceVfsFile *file, *f;
	int ret;

	if (cmd - VFS_IOCTL_BUFFCTL_SET > 1)
		return SCE_ERROR_ERRNO_EINVAL;
	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	if (file->is_dir != 0) {
		vfs_release_file(fd);
		return SCE_ERROR_ERRNO_EISDIR;
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 0);
	if (ret != 0)
		goto unlock_fd;
	vp = file->vp;
	vfsLockVnode(vp);
	lvp = vp->core.link_to;
	if (lvp == NULL) {
		bc = vp->core.bc;
	} else {
		vfsLockVnode(lvp);
		bc = lvp->core.bc;
	}
	mnt = vp->core.mnt;
	if ((mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_NOBUF) != 0) {
		ret = SCE_ERROR_ERRNO_EUNSUP;
		goto unlock_vp;
	}
	if (bc == NULL) {
		ret = SCE_ERROR_ERRNO_ENOBUFS;
		goto unlock_vp;
	}
	if (cmd == VFS_IOCTL_BUFFCTL_GET) {
		if (outlen < sizeof(*info)) {
			ret = SCE_ERROR_ERRNO_EINVAL;
		} else {
			info->lineSize = bc->lineSize;
			info->dataCacheSize = (short)bc->setNum * (short)bc->wayNum * bc->lineSize;
			info->ioCacheSize = bc->ioBuffSize;
			info->reserved[0] = 0;
			info->reserved[1] = 0;
			info->wayNum = bc->wayNum;
		}
		goto unlock_vp;
	}
	dataCacheSize = (mask & VFS_BUFFCTL_SET_DATA_CACHE_SIZE) != 0 ?
			param->dataCacheSize :
			bc->lineSize * (short)bc->setNum * (short)bc->wayNum;
	lineSize = (mask & VFS_BUFFCTL_SET_LINE_SIZE) != 0 ? param->lineSize : bc->lineSize;
	wayNum = (mask & VFS_BUFFCTL_SET_WAY_NUM) != 0 ? param->wayNum : bc->wayNum;
	ioCacheSize = (mask & VFS_BUFFCTL_SET_IO_CACHE_SIZE) != 0 ? param->ioCacheSize :
								      bc->ioBuffSize;
	if (ioCacheSize % lineSize != 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 7651, "ioCacheSize=0x%x lineSize=0x%x != 0\n",
				     ioCacheSize, lineSize);
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto unlock_vp;
	}
	if (lineSize == 0 || dataCacheSize == 0 || ioCacheSize == 0) {
		SCE_DBG_PRINTF(0xf, 7660, "\n");
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto unlock_vp;
	}
	if (mnt->default_io_cache_size < ioCacheSize) {
		for (f = vp->core.fd_list; f != NULL; f = f->next) {
			if ((f->flags & SCE_O_WRONLY) != 0) {
				SCE_DBG_PRINTF(0xf, 7674, "\n");
				ret = SCE_ERROR_ERRNO_EINVAL;
				goto unlock_vp;
			}
		}
		if (lvp != NULL) {
			for (f = lvp->core.fd_list; f != NULL; f = f->next) {
				if ((f->flags & SCE_O_WRONLY) != 0) {
					ret = SCE_ERROR_ERRNO_EINVAL;
					SCE_DBG_PRINTF(0xf, 7683, "\n");
					goto unlock_vp;
				}
			}
		}
	} else if (mnt->default_io_cache_size % ioCacheSize != 0) {
		SCE_DBG_PRINTF(0xf, 7691, "\n");
		ret = SCE_ERROR_ERRNO_EINVAL;
		goto unlock_vp;
	}
	if (lvp != NULL) {
		ret = vfsBuffCacheCtl(lvp, dataCacheSize, wayNum & 0xff, lineSize,
				      ioCacheSize, mode, 0);
	} else {
		ret = vfsBuffCacheCtl(vp, dataCacheSize, wayNum & 0xff, lineSize,
				      ioCacheSize, mode, 0);
		goto unlock_base_vp;
	}
unlock_vp:
	if (lvp != NULL)
		vfsUnlockVnode(lvp);
unlock_base_vp:
	vfsUnlockVnode(vp);
unlock_fd:
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int vfsIoctl(IofilemgrIoctlParam *param)
{
	SceUID fd = param->fd;
	unsigned int cmd = param->cmd;
	const VfsBuffCtlIoctlArg *indata = param->indata;
	int inlen = param->inlen;
	void *outdata = param->outdata;
	int outlen = param->outlen;
	IofilemgrBuffCtlParam buffCtl;
	SceVfsFile *file;
	SceVfsVnode *vp;
	SceUID pid;
	int ret;

	if ((int)cmd > VFS_IOCTL_MAX)
		return SCE_ERROR_ERRNO_EINVAL;
	if (cmd - VFS_IOCTL_BUFFCTL_SET <= VFS_IOCTL_VFS_MAX - VFS_IOCTL_BUFFCTL_SET) {
		if ((int)cmd > VFS_IOCTL_BUFFCTL_MAX) {
			pid = ksceKernelGetProcessIdFromTLS();
			if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(pid) == 0 && SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid) == 0)
				return SCE_ERROR_ERRNO_EUNSUP;
			if (cmd == VFS_IOCTL_SET_INTERNAL)
				return vfs_change_file_attr(fd, VFS_FILE_ATTR_INTERNAL, 1);
			if (cmd == VFS_IOCTL_CLEAR_INTERNAL)
				return vfs_change_file_attr(fd, VFS_FILE_ATTR_INTERNAL, 0);
#if FW_VERSION < 0x04000000
			if (cmd == VFS_IOCTL_IS_PFS) {
				if (outdata == NULL || outlen != sizeof(SceUInt32))
					return SCE_ERROR_ERRNO_EINVAL;
				ret = vfs_convert_file(fd, &file);
				if (ret < 0)
					return ret;
				vfsLockFd(file);
				ret = vfs_check_fd_status(file, 0);
				if (ret == 0)
					*(SceUInt32 *)outdata =
						file->vp->core.mnt->fs_type == SCE_VFS_FS_TYPE_PFS;
				vfsUnlockFd(file);
				vfs_release_file(fd);
				return ret;
			}
#endif
			return SCE_ERROR_ERRNO_EINVAL;
		}
		if ((int)cmd <= VFS_IOCTL_BUFFCTL_GET) {
			buffCtl.fd = fd;
			if (cmd == VFS_IOCTL_BUFFCTL_SET) {
				if (indata == NULL || indata->reserved[0] != 0)
					return SCE_ERROR_ERRNO_EINVAL;
				buffCtl.dataCacheSize = indata->dataCacheSize;
				buffCtl.lineSize = indata->lineSize;
				buffCtl.wayNum = indata->wayNum;
				buffCtl.ioCacheSize = indata->ioCacheSize;
				buffCtl.mask = indata->mask;
				buffCtl.mode = indata->mode;
			} else {
				if (outdata == NULL || ((VfsBuffCtlIoctlArg *)outdata)->reserved[0] != 0)
					return SCE_ERROR_ERRNO_EINVAL;
				buffCtl.outdata = outdata;
				buffCtl.outlen = outlen;
			}
			buffCtl.cmd = cmd;
			return vfsBuffCtl(&buffCtl);
		}
	}
	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	if (cmd - VFS_IOCTL_SET_IO_SCHED_DATA > 1) {
		ret = vfs_check_fd_status(file, 0);
		if (ret != 0) {
			vfsUnlockFd(file);
			vfs_release_file(fd);
			return ret;
		}
		vp = file->vp;
		vfsLockVnode(vp);
		if (vp->core.link_to == NULL) {
			ret = ksceVopIoctl(vp, file, cmd, indata, inlen, outdata, outlen);
		} else {
			vfsLockVnode(vp->core.link_to);
			ret = ksceVopIoctl(vp->core.link_to, file, cmd, indata, inlen, outdata, outlen);
			vfsUnlockVnode(vp->core.link_to);
		}
		vfsUnlockVnode(vp);
	} else {
		if (file->state != SCE_VFS_FILE_STATE_DUMMY) {
			ret = vfs_check_fd_status(file, (indata != NULL && inlen == 4) ?
							      *(const int *)indata : 0);
			if (ret != 0) {
				vfsUnlockFd(file);
				vfs_release_file(fd);
				return ret;
			}
		}
		if (cmd == VFS_IOCTL_GET_IO_SCHED_DATA) {
			if (outdata == NULL || outlen != sizeof(file->ioSchedData)) {
				ret = SCE_ERROR_ERRNO_EINVAL;
			} else {
				ret = 0;
				__builtin_memcpy(outdata, file->ioSchedData, sizeof(file->ioSchedData));
			}
		} else if (outdata == NULL || outlen != sizeof(file->ioSchedData)) {
			ret = SCE_ERROR_ERRNO_EINVAL;
		} else {
			ret = 0;
			__builtin_memcpy(file->ioSchedData, outdata, sizeof(file->ioSchedData));
		}
	}
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

/* name unknown: the mount ID (SceVfsMountData.mnt_id) of the mount a path is on */
int vfsGetMntId(const char *path)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	int num;
	SceVfsVnode *vp;
	char *buf;
	int ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(path, buf, elem, &num, 0);
	if (ret >= 0 && num != 0) {
		ret = vfs_find_in_path(elem, num, &vp, buf, 0x8000);
		if (ret >= 0) {
			ret = vp->core.mnt->mnt_data->mnt_id;
			vfs_decrement_vp_mnt_ref_inline(vp);
		}
	}
	vfs_path_buff_free(buf);
	return ret;
}

void vfs_set_sync_pending(void)
{
	g_need_mnt_sync = 1;
}

void vfs_clear_sync_pending(void)
{
	g_need_mnt_sync = 0;
}

void vfs_increment_io_count(void)
{
	SceCpuForDriver_341B6E81(&g_vfsIoCount, 1);
}

void vfs_decrement_io_count(void)
{
	SceCpuForDriver_341B6E81(&g_vfsIoCount, -1);
}

/* name unknown: marks the devices of every mounted external mount as gone */
void vfs_clear_external_dev_state(void)
{
	SceVfsMount *mnt;

	for (mnt = vfs_get_mnt_list(); mnt != NULL; mnt = mnt->mnt_list_next) {
		if (mnt->state == VFS_MOUNT_STATE_MOUNTED &&
		    (mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_EXTERNAL) != 0)
			*mnt->fumount->devState = 0;
	}
}

int vfs_system_event_handler(int resume, int eventid, void *args, void *opt)
{
	SceUID pid;
	int ret, num;

	if (resume != 0) {
		if (resume == 1) {
			if (eventid == VFS_SYSEVENT_RESUME) {
				vfs_clear_external_dev_state();
				g_vfsSuspended = 0;
				return 0;
			}
			if (eventid == VFS_SYSEVENT_RESUME_DONE) {
				g_vfsSuspendQueryCount = 0;
				g_need_mnt_sync = 0;
				return 0;
			}
		}
		return 0;
	}
	switch (eventid) {
	case VFS_SYSEVENT_SUSPEND:
		ret = ksceKernelRunWithStack(0x2000, (int (*)(void *))vfs_sync_all_mount_point,
					     NULL);
		if (g_vfsSuspendQueryCount == 0) {
			g_vfsSuspended++;
			pid = SceSysrootForDriver_05093E7B();
			if (pid >= 0) {
				num = SceSysmemForKernel_08C05493(pid, &g_vfsFileClass, 7, NULL, 0,
								  NULL);
				if (num < 0) {
					SCE_DBG_PRINTF(0xf, 7803,
						       "scePUIDGetUIDVectorByClass failed ... 0x%x %d\n",
						       num, num);
				} else {
					ret = vfs_process_suspend(pid, num);
					if (ret < 0)
						SCE_DBG_PRINTF(0xf, 7808,
							       "vfs_process_suspend failed ... 0x%x %d\n",
							       ret, ret);
				}
			}
			ret = vfs_prepare_for_resume();
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 7816,
					       "vfs_prepare_resume failed ... 0x%x %d\n", ret, ret);
		}
		break;
	case VFS_SYSEVENT_SUSPEND_QUERY:
		if (g_vfsSuspendQueryCount == 0) {
			ret = SCE_ERROR_ERRNO_EAGAIN;
		} else {
			ret = g_need_mnt_sync == 0 ? 0 : SCE_ERROR_ERRNO_EAGAIN;
			if (g_vfsIoCount != 0)
				ret = SCE_ERROR_ERRNO_EBUSY;
		}
		g_vfsSuspendQueryCount++;
		g_need_mnt_sync = 0;
		break;
	default:
		ret = 0;
		if (eventid == VFS_SYSEVENT_SUSPEND_PREPARE)
			g_need_mnt_sync = 0;
		break;
	}
	return ret;
}

int SceIofilemgrForDriver_2EFDFA12(SceUID fd, const char *path)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	if (file->state != SCE_VFS_FILE_STATE_DUMMY) {
		ret = vfs_check_fd_status(file, 0);
		if (ret != 0) {
			vfsUnlockFd(file);
			vfs_release_file(fd);
			return ret;
		}
	}
	ret = vfs_debug_set_open_path(file, path);
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int vfsGetFdInfoForDebugger(SceUID fd, SceIofileInfo *info)
{
	SceVfsFile *file;
	struct DebugPath *dp;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 7995, "convert failed fd is %d 0x%x ret=%d 0x%x\n", fd,
				     fd, ret, ret);
		return ret;
	}
	info->pid = file->pid;
	info->position = file->position;
	if (file->is_dir == 0)
		info->isDir = 0;
	else
		info->isDir = 1;
	if (file->vp != NULL)
		info->size = file->vp->core.size;
	else if (file->state == SCE_VFS_FILE_STATE_DUMMY)
		info->size = 0;
	else
		info->size = -1;
	dp = file->debug_path;
	info->ioSchedData = file->ioSchedData[0];
	if (dp == NULL) {
		info->resolvedOpenPath[0] = '\0';
		info->openPath[0] = '\0';
		info->flags = -1;
		info->mode = -1;
	} else {
		strncpy(info->resolvedOpenPath, dp->resolvedOpenPath, sizeof(info->resolvedOpenPath));
		if (dp->openPath == NULL)
			info->openPath[0] = '\0';
		else
			strncpy(info->openPath, dp->openPath, sizeof(info->openPath));
		info->flags = dp->flags;
		info->mode = dp->mode;
	}
	vfs_release_file(fd);
	return ret;
}

/* name unknown: the mount option word of the vnode a path names */
int vfs_get_vnode_mnt_opt(VfsPathParam *param)
{
	const char *path = param->path;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	SceVfsVnode *vp;
	char *buf;
	int num, ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(path, buf, elem, &num, 0);
	if (ret >= 0 && num != 0) {
		ret = vfs_find_in_path(elem, num, &vp, buf, 0x100);
		if (ret >= 0) {
			ret = vp->core.mnt_opt;
			vfs_decrement_vp_mnt_ref_inline(vp);
		}
	}
	vfs_path_buff_free(buf);
	return ret;
}

int vfs_check_and_sync_on_host(SceVfsVnode *vp, SceVfsPath *path)
{
	SceIoStat stat;
	int ret, err;

	ret = ksceVopGetstat(vp, path, &stat);
	if (ret != SCE_ERROR_ERRNO_ENOENT)
		return ret;
	if (vp->core.ncache != NULL && (err = clear_ncache(vp->core.ncache, 0)) < 0)
		SCE_DBG_PRINTF(0xf, 8114, "name cache cleanup failed vp=%p retval=%d\n", vp, err);
	vp->core.state = SCE_VNODE_STATE_INACTIVE;
	if (vp->core.ref_count == 1) {
		err = ksceVopInactive(vp);
		if (err < 0) {
			if (err != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF(0xf, 8125, "inactive failed ... %d 0x%x\n", err, err);
		}
		vfsLockMnt(vp->core.mnt);
		if ((int)--vp->core.mnt->mnt_ref_count < 0)
			SCE_DBG_PRINTF(0xf, 8134, "vp->mnt->mntRefCount = %d\n",
				       vp->core.mnt->mnt_ref_count);
		vfsUnlockMnt(vp->core.mnt);
		vfsFreeVnode(vp);
	} else {
		vfs_decrement_vp_mnt_ref_inline(vp);
	}
	return ret;
}

SceUID vfsOpen(IofilemgrOpenParam *param)
{
	const char *filename = param->path;
	int flags = param->flags;
	SceMode mode = param->mode;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	const VfsPathElem *name;
	SceVfsVnode *dvp, *vp;
	SceVfsMount *mnt;
	SceVfsFile *file, *f;
	SceVfsPath path;
	SceIoStat stat;
	SceUID pid, fd;
	char *buf;
	int num, last, creat, writable, exists, ret, err;

	if (((unsigned int)flags >> 28) != 0) {
		flags &= 0x0fffffff;
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 3051, "flag = %x\n", param->flags);
	}
	pid = ksceKernelGetProcessIdFromTLS();
	if ((flags & SCE_O_NOBUF) != 0 && SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(pid) == 0 &&
	    SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid) == 0)
		flags &= ~SCE_O_NOBUF;
	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(filename, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	last = num - 1;
	ret = vfs_find_in_path(elem, last, &dvp, buf, 1);
	if (ret < 0)
		goto out;
	mnt = dvp->core.mnt;
	if ((dvp->core.type & SCE_VNODE_TYPE_ROOT_MOUNT) != 0) {
		if ((flags & (SCE_O_CREAT | SCE_O_TRUNC)) != 0) {
			ret = SCE_ERROR_ERRNO_EINVAL;
			goto release_dvp;
		}
		writable = 1;
	} else {
		ret = vfs_check_vnode_rw_access(dvp);
		if (ret >= 0)
			writable = 1;
		else if (ret == SCE_ERROR_ERRNO_EACCES || ret == VFS_ERROR_PFS_EACCES)
			writable = 0;
		else
			goto release_dvp;
	}
	creat = flags & SCE_O_CREAT;
	name = &elem[last];
	for (;;) {
		ret = vfs_find_in_vnode(dvp, name, buf, creat != 0 ? 0x100002 : 0x100001, &vp);
		if (ret == 0) {
			if ((vp->core.type & (SCE_VNODE_TYPE_DEV | SCE_VNODE_TYPE_REG)) == 0) {
				if ((vp->core.type & SCE_VNODE_TYPE_DIR) != 0)
					ret = SCE_ERROR_ERRNO_EISDIR;
				else
					ret = SCE_ERROR_ERRNO_EACCES;
				goto release_vp;
			}
			exists = 1;
			if ((flags & (SCE_O_CREAT | SCE_O_EXCL)) == (SCE_O_CREAT | SCE_O_EXCL)) {
				ret = SCE_ERROR_ERRNO_EEXIST;
				goto check_host;
			}
		} else if (ret == SCE_ERROR_ERRNO_ENOENT) {
			exists = 0;
		} else {
			goto release_dvp;
		}
		if ((flags & (SCE_O_APPEND | SCE_O_TRUNC)) == (SCE_O_APPEND | SCE_O_TRUNC))
			goto invalid;
		if (creat == 0) {
			if (!exists) {
				ret = SCE_ERROR_ERRNO_ENOENT;
				goto release_dvp;
			}
		} else if (!writable) {
			ret = SCE_ERROR_ERRNO_EACCES;
			if (!exists)
				goto release_dvp;
			if ((flags & SCE_O_TRUNC) != 0)
				goto check_host;
		}
		if ((flags & SCE_O_RDWR) != SCE_O_WRONLY && (flags & SCE_O_RDWR) != SCE_O_RDWR &&
		    ((flags & SCE_O_RDWR) != SCE_O_RDONLY || (flags & (SCE_O_APPEND | SCE_O_TRUNC)) != 0))
			goto invalid;
		path.name = name->name;
		path.name_length = name->len;
		path.path = buf;
		if (!exists) {
			if ((mode & VFS_OPEN_MODE_VALID) == 0 || (mode & VFS_OPEN_MODE_INVALID) != 0) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				SceDebugForDriver_391B74B7("#### Invalid mode !! sceIoOpen(%s, flag=0x%x, mode=0%o(0x%x)) ####\n",
						 filename, flags, mode, mode);
				goto release_dvp;
			}
			ret = ksceVopCreate(dvp, &vp, &path, flags, mode);
			if (ret < 0)
				goto release_dvp;
			SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 3243);
			ret = sceVfsACCheckOpen(vp, flags);
			if (ret < 0) {
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 3248, "sceVfsACCheckOpen() failed %d 0x%x\n",
						     ret, ret);
				goto unlock_vp;
			}
			goto open;
		}
		for (f = vp->core.fd_list; f != NULL; f = f->next) {
			if ((flags & SCE_O_NOBUF) == 0 && (f->flags & SCE_O_NOBUF) != 0) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				goto release_vp;
			}
		}
		if (vp->core.bc != NULL) {
			if ((flags & SCE_O_NOBUF) != 0) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				goto release_vp;
			}
			if (vp->core.mnt->default_io_cache_size < vp->core.bc->ioBuffSize &&
			    (flags & SCE_O_WRONLY) != 0) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				goto release_vp;
			}
		}
		if (creat == 0 || vp->core.mnt->fs_type != SCE_VFS_FS_TYPE_HOSTFS ||
		    vfs_check_and_sync_on_host(vp, &path) != SCE_ERROR_ERRNO_ENOENT)
			break;
		continue;
invalid:
		ret = SCE_ERROR_ERRNO_EINVAL;
		if (!exists)
			goto release_dvp;
check_host:
		if (creat == 0 || vp->core.mnt->fs_type != SCE_VFS_FS_TYPE_HOSTFS)
			goto release_vp;
		path.name = name->name;
		path.name_length = name->len;
		path.path = buf;
		if (vfs_check_and_sync_on_host(vp, &path) != SCE_ERROR_ERRNO_ENOENT)
			goto release_vp;
	}
	ret = sceVfsACCheckOpen(vp, flags);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 3257, "sceVfsACCheckOpen() failed %d 0x%x\n", ret, ret);
		goto release_vp;
	}
	if ((flags & SCE_O_TRUNC) != 0) {
		stat.st_size = 0;
		ret = ksceVopChstat(vp, &path, &stat, SCE_CST_SIZE);
		if (ret < 0) {
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 3268, "truncate failed %d 0x%x\n", ret, ret);
			goto release_vp;
		}
	}
open:
	fd = vfs_allocate_file(vp, &file, path.name);
	ret = fd;
	if (fd < 0)
		goto fail;
	vfs_initialize_file(vp, file);
	file->flags = flags & (SCE_O_NOBUF | SCE_O_APPEND | SCE_O_RDWR);
	ret = ksceVopOpen(vp, &path, flags, file);
	if (ret < 0) {
		SCE_DBG_PRINTF_LEVEL(2, 0xf, 3290, "open failed %d 0x%x\n", ret, ret);
	} else if (vp->core.ncache == NULL && (ret = enter_ncache(dvp, vp, name, 1, 1)) < 0) {
		SCE_DBG_PRINTF(0xf, 3299, "enter failed %d 0x%x\n", ret, ret);
		err = ksceVopClose(vp, file);
		if (err < 0)
			SCE_DBG_PRINTF(0xf, 3302, "Close failed %d 0x%x\n", err, err);
	} else {
		if (SceDipswForDriver_A98FC2FD(0xd2) != 0) {
			vfs_debug_set_resolved_path(file, filename);
			vfs_debug_set_open_param(file, flags, mode);
		}
		if ((flags & VFS_O_EXTERNAL_FILE) != 0 &&
		    (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(pid) != 0 || SceSblACMgrForKernel_sceSblACMgrIsKernelProgram(pid) != 0))
			file->idata &= ~VFS_FILE_ATTR_INTERNAL;
		vfsUnlockVnode(vp);
		if (g_vfsSuspended > 0 && (file->idata & VFS_FILE_ATTR_INTERNAL) != 0)
			file->state = SCE_VFS_FILE_STATE_UNK;
		if (!exists) {
			vfsLockMnt(mnt);
			mnt->mnt_ref_count++;
			vfsUnlockMnt(mnt);
		}
		ret = fd;
		goto release_dvp;
	}
	vfs_finalize_file(vp, file);
	vfs_close_file(vp, fd);
fail:
	if (exists)
		goto release_vp;
unlock_vp:
	vp->core.ref_count--;
	vfsUnlockVnode(vp);
	goto release_dvp;
release_vp:
	vfs_decrement_vp_mnt_ref_inline(vp);
release_dvp:
	vfs_decrement_vp_mnt_ref_inline(dvp);
	vfs_path_buff_free(buf);
	if (ret > 0 && vp->core.bc == NULL &&
	    (vp->core.mnt->mnt_flags & SCE_VFS_MOUNT_FLAG_NOBUF) == 0 && (flags & SCE_O_NOBUF) == 0)
		SCE_DBG_PRINTF(0xf, 3376, "No memory, can not allocate buffer cache %s\n", filename);
	return ret;
out:
	vfs_path_buff_free(buf);
	return ret;
}

#if !FW_DEBUG
static const SceKernelDebugInfo vfsMkdirModeErrorInfo
__attribute__((section(".sce_dbginfo"), used)) = {
	.fileHash = 0x45541a74,
	.lineHash = 0xe80e0af6,
	.funcHash = 0xfbca1c1f,
};
#endif

int vfsMkdir(IofilemgrMkdirParam *param)
{
	SceMode mode = param->mode;
	const char *dir = param->dir;
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	const VfsPathElem *name;
	SceVfsVnode *dvp, *vp;
	SceVfsPath path;
	char *buf;
	int num, last, ret;

	if ((mode & VFS_OPEN_MODE_VALID) == 0 || (mode & VFS_OPEN_MODE_INVALID) != 0) {
#if FW_DEBUG
		SceDebugForDriver_391B74B7("#### Invalid mode !! sceIoMkdir(%s, mode=0%o(0x%x)) ####\n", dir,
				 mode, mode);
#else
		SceDebugForDriver_02B04343(0xf, &vfsMkdirModeErrorInfo, "::0x10f::003::0%o(0x%x)\n",
					 mode, param->mode);
#endif
		return SCE_ERROR_ERRNO_EINVAL;
	}
	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(dir, buf, elem, &num, 0);
	if (ret < 0 || num == 0)
		goto out;
	last = num - 1;
	ret = vfs_find_in_path(elem, last, &dvp, buf, 0x10);
	if (ret < 0)
		goto out;
	ret = vfs_check_vnode_rw_access(dvp);
	if (ret < 0)
		goto release_dvp;
	name = &elem[last];
	for (;;) {
		ret = vfs_find_in_vnode(dvp, name, buf, 0x100010, &vp);
		if (ret == SCE_ERROR_ERRNO_ENOENT) {
			path.name = name->name;
			path.name_length = name->len;
			path.path = buf;
			ret = ksceVopMkdir(dvp, &vp, &path, mode);
			if (ret >= 0) {
				SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(vp) == 1, 4188);
				ret = enter_ncache(dvp, vp, name, 1, 1);
				if (ret < 0)
					SCE_DBG_PRINTF(0xf, 4197, "enter cache failed ... %d 0x%x %s\n",
						       ret, ret, name->name);
				vp->core.ref_count--;
				vfsUnlockVnode(vp);
			}
			goto release_dvp;
		}
		if (ret != 0)
			goto release_dvp;
		if (vp->core.mnt->fs_type != SCE_VFS_FS_TYPE_HOSTFS)
			break;
		path.name = name->name;
		path.name_length = name->len;
		path.path = buf;
		if (vfs_check_and_sync_on_host(vp, &path) != SCE_ERROR_ERRNO_ENOENT)
			break;
	}
	vfs_decrement_vp_mnt_ref_inline(vp);
	ret = SCE_ERROR_ERRNO_EEXIST;
release_dvp:
	vfs_decrement_vp_mnt_ref_inline(dvp);
out:
	vfs_path_buff_free(buf);
	return ret;
}

int vfsRename(IofilemgrRenameParam *param)
{
	const char *oldname = param->oldname;
	const char *newname = param->newname;
	VfsPathElem oelem[VFS_PATH_ELEM_MAX], nelem[VFS_PATH_ELEM_MAX];
	const VfsPathElem *oname, *nname;
	SceVfsVnode *odvp, *ovp, *ndvp, *nvp;
	SceVfsMount *mnt;
	SceVfsFile *f;
	VfsLookupPath opath, npath;
	VfsHostRenameArg arg;
	VfsPathElem resolved;
	char *obuf, *nbuf;
	SceSize len;
	int onum, nnum, olast, nlast, ret, err, n;

	for (;;) {
		ndvp = NULL;
		nvp = NULL;
		obuf = vfs_path_buff_alloc();
		if (obuf == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		ret = replace_flat_path(oldname, obuf, oelem, &onum, 0);
		if (ret < 0 || onum == 0)
			goto free_obuf;
		olast = onum - 1;
		oname = &oelem[olast];
		ret = vfs_find_in_path(oelem, olast, &odvp, obuf, 0x30);
		if (ret < 0)
			goto free_obuf;
		mnt = odvp->core.mnt;
		ret = vfs_check_vnode_rw_access(odvp);
		if (ret < 0 || (ret = vfs_find_in_vnode(odvp, oname, obuf, 0x30, &ovp)) < 0) {
			if (odvp != NULL)
				vfs_decrement_vp_mnt_ref_inline(odvp);
			goto free_obuf;
		}
		if (ovp->core.type > (SCE_VNODE_TYPE_ROOT | SCE_VNODE_TYPE_REG)) {
			vfs_release_lookup(obuf, odvp, ovp, NULL, NULL, NULL, 0);
			return SCE_ERROR_ERRNO_EBUSY;
		}
		nbuf = vfs_path_buff_alloc();
		if (nbuf == NULL) {
			vfs_release_lookup(obuf, odvp, ovp, NULL, NULL, NULL, 0);
			return SCE_ERROR_ERRNO_ENOMEM;
		}
		if (vfs_is_full_path(newname) == 0) {
			/* a new name in the same directory */
			len = strnlen(newname, 0x400);
			if (len >= 0x400) {
				vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
				return SCE_ERROR_ERRNO_ENAMETOOLONG;
			}
			if (strchr(newname, '/') != NULL) {
				vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
				return SCE_ERROR_ERRNO_EINVAL;
			}
			if (odvp->core.mnt->fs_type == SCE_VFS_FS_TYPE_HOSTFS) {
				arg.oldPath = oelem[1].name;
				arg.oldPathLength = strnlen(oelem[1].name, 0x400);
				arg.newName = newname;
				arg.newNameLength = len;
				resolved.name = nbuf;
				ret = ksceVfsOpDevctl(odvp->core.mnt, odvp->core.mnt->mnt_data->assign_name,
						      VFS_DEVCTL_HOST_RENAME_PATH, &arg, sizeof(arg),
						      &resolved, sizeof(resolved));
				if (ret < 0) {
					vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
					return ret;
				}
				nelem[0] = resolved;
			} else {
				strncpy(nbuf, newname, 0x3ff);
				nbuf[0x3ff] = '\0';
				ret = vfs_split_path(nbuf, nelem, &nnum);
				if (ret < 0) {
					vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
					return ret;
				}
				if (nnum != 1) {
					SCE_DBG_PRINTF_LEVEL(2, 0xf, 5158, "path element is %d %s\n", nnum,
							     newname);
					vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
					return SCE_ERROR_ERRNO_EINVAL;
				}
			}
			nname = &nelem[0];
			for (;;) {
				ret = vfs_find_in_vnode(odvp, nname, nbuf, 0x100040, &nvp);
				if (ret == SCE_ERROR_ERRNO_ENOENT)
					goto rename;
				if (ret != 0)
					break;
				if (nvp->core.mnt->fs_type != SCE_VFS_FS_TYPE_HOSTFS) {
					ret = SCE_ERROR_ERRNO_EEXIST;
					break;
				}
				npath.path.name = nelem[0].name;
				npath.path.name_length = nelem[0].len;
				npath.path.path = nbuf;
				npath.elem = NULL;
				if (vfs_check_and_sync_on_host(nvp, &npath.path) != SCE_ERROR_ERRNO_ENOENT) {
					ret = SCE_ERROR_ERRNO_EEXIST;
					break;
				}
			}
			vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, nvp, 0);
			return ret;
		}
		ret = replace_flat_path(newname, nbuf, nelem, &nnum, 0);
		if (ret < 0 || nnum == 0) {
			vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
			return ret;
		}
		nlast = nnum - 1;
		ret = vfs_find_in_path(nelem, nlast, &ndvp, nbuf, 0x40);
		if (ret < 0) {
			if (ret != VFS_LOOKUP_AGAIN) {
				vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
				return ret;
			}
			err = strncmp(oelem[0].name, nelem[0].name, oelem[0].len + 1);
			vfs_release_lookup(obuf, odvp, ovp, nbuf, NULL, NULL, 0);
			if (err != 0)
				return SCE_ERROR_ERRNO_EPERM;
		} else {
			if (mnt != ndvp->core.mnt) {
				vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
				return SCE_ERROR_ERRNO_EPERM;
			}
			ret = vfs_check_vnode_rw_access(ndvp);
			if (ret < 0) {
				vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
				return ret;
			}
			nname = &nelem[nlast];
			for (;;) {
				ret = vfs_find_in_vnode(ndvp, nname, nbuf, 0x100040, &nvp);
				if (ret == SCE_ERROR_ERRNO_ENOENT)
					goto rename;
				if (ret != 0)
					break;
				if (nvp->core.mnt->fs_type != SCE_VFS_FS_TYPE_HOSTFS) {
					vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, nvp, 0);
					return SCE_ERROR_ERRNO_EEXIST;
				}
				npath.path.name = nname->name;
				npath.path.name_length = nname->len;
				npath.path.path = nbuf;
				npath.elem = NULL;
				if (vfs_check_and_sync_on_host(nvp, &npath.path) != SCE_ERROR_ERRNO_ENOENT) {
					vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, nvp, 0);
					return SCE_ERROR_ERRNO_EEXIST;
				}
			}
			vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
			if (ret != VFS_LOOKUP_AGAIN)
				return ret;
		}
		sceKernelDelayThread(1000);
	}
free_obuf:
	vfs_path_buff_free(obuf);
	return ret;
rename:
	vfs_lock_ncache();
	if (ovp->core.ncache != NULL)
		set_ncache_state(ovp->core.ncache, NCACHE_STATE_VALID, NCACHE_STATE_RENAMING);
	vfs_unlock_ncache();
	if ((int)ovp->core.fd_num > 0) {
		SCE_DBG_ASSERT(ovp->core.fd_list != NULL, 5214);
		ret = vfsTrylockFd(ovp->core.fd_list);
		if (ret != 0) {
			if (ret == SCE_KERNEL_ERROR_MUTEX_FAILED_TO_OWN) {
				vfsUnlockVnode(ovp);
				vfsUnlockVnode(odvp);
				ret = vfsLockFdByVnode(ovp);
				vfsLockVnode(odvp);
				vfsLockVnode(ovp);
				if (ret >= 0) {
					if ((int)ovp->core.fd_num <= 0 ||
					    (ret = ksceVopSync(ovp, ovp->core.fd_list, 0)) >= 0)
						goto do_rename;
					vfs_lock_ncache();
					if (ovp->core.ncache != NULL)
						set_ncache_state(ovp->core.ncache, NCACHE_STATE_RENAMING,
								 NCACHE_STATE_VALID);
					vfs_unlock_ncache();
					vfsUnlockFd(ovp->core.fd_list);
					vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
					return ret;
				}
			}
			vfs_lock_ncache();
			if (ovp->core.ncache != NULL)
				set_ncache_state(ovp->core.ncache, NCACHE_STATE_RENAMING, NCACHE_STATE_VALID);
			vfs_unlock_ncache();
			vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
			return ret;
		}
	}
do_rename:
	opath.path.name = oname->name;
	opath.path.name_length = oname->len;
	opath.path.path = obuf;
	opath.elem = NULL;
	npath.path.name = nname->name;
	npath.path.name_length = nname->len;
	npath.path.path = nbuf;
	npath.elem = NULL;
	ret = ksceVopRename(odvp, ovp, &opath.path, ndvp, &nvp, &npath.path);
	if (ret < 0) {
		vfs_lock_ncache();
		if (ovp->core.ncache != NULL)
			set_ncache_state(ovp->core.ncache, NCACHE_STATE_RENAMING, NCACHE_STATE_VALID);
		vfs_unlock_ncache();
		if ((int)ovp->core.fd_num > 0)
			vfsUnlockFd(ovp->core.fd_list);
		vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, NULL, 0);
		return ret;
	}
	SCE_DBG_ASSERT_CHECK(vfsIsOwnerVnode(nvp) == 1, 5324);
	if (ovp->core.flock != NULL) {
		err = vfs_flock_dup(ovp, nvp);
		if (err < 0)
			SCE_DBG_PRINTF(0xf, 5332, "can not dup flock %d 0x%x\n", err, err);
		ret = vfs_release_lock_entry(ovp);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 5338, "vfs_release_lock_entry failed %d 0x%x\n", ret, ret);
	}
	n = ovp->core.fd_num;
	ovp->core.state = SCE_VNODE_STATE_INACTIVE;
	if (n > 0) {
		f = ovp->core.fd_list;
		nvp->core.bc = ovp->core.bc;
		nvp->core.fd_list = f;
		nvp->core.fd_num = n;
		do {
			f->vp = nvp;
			f = f->next;
		} while (f != NULL);
		vfsUnlockFd(nvp->core.fd_list);
		nvp->core.ref_count += nvp->core.fd_num;
		ovp->core.fd_num = 0;
		ovp->core.fd_list = NULL;
		ovp->core.bc = NULL;
		ovp->core.ref_count -= nvp->core.fd_num;
	}
	if (ovp->core.ncache != NULL) {
		ret = restore_ncache(ndvp != NULL ? ndvp : odvp, nvp, nname, ovp, ovp->core.allocator);
		if (ret < 0)
			SCE_DBG_PRINTF(0xf, 5383, "name cache restore failed ... %p\n", nvp);
	}
	ovp->core.ncache = NULL;
	if (ovp->core.ref_count == 1) {
		ret = ksceVopInactive(ovp);
		if (ret < 0) {
			if (ret != SCE_ERROR_ERRNO_EUNSUP)
				SCE_DBG_PRINTF(0xf, 5401, "inactive failed ... %d 0x%x\n", ret, ret);
		}
		vfsLockMnt(ovp->core.mnt);
		ovp->core.mnt->mnt_ref_count--;
		vfsUnlockMnt(ovp->core.mnt);
		vfsFreeVnode(ovp);
		ovp = NULL;
	} else {
		SCE_DBG_PRINTF(0xf, 5413, "Rename ovp->refCount = %d\n", ovp->core.ref_count);
	}
	vfs_release_lookup(obuf, odvp, ovp, nbuf, ndvp, nvp, 1);
	return ret;
}

int vfsNotifyDeviceStatus(int mntId)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	const char *mntPath;
	int num, depth;
	SceVfsVnode *vp;
	char *buf;
	int ret;

	mntPath = mount_get_path_by_id(mntId);
	if (mntPath == NULL)
		return SCE_ERROR_ERRNO_EINVAL;
	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(mntPath, buf, elem, &num, 1);
	if (ret >= 0 && num != 0) {
		ret = vfs_get_root_vnode(elem, num, &depth, 0, &vp);
		if (ret >= 0) {
			if (depth != num) {
				ret = SCE_ERROR_ERRNO_EINVAL;
				SCE_DBG_PRINTF_LEVEL(2, 0xf, 8190, "n_root_path_elem(%d) != penum(%d)\n",
						     depth, num);
			} else {
				*vp->core.mnt->fumount->devState = 1;
				ret = vfs_signal_fumount(vp->core.mnt);
			}
			vfs_decrement_vp_mnt_ref_inline(vp);
		}
	}
	vfs_path_buff_free(buf);
	return ret;
}

/* a dummy file's Fios2 handle is kept in is_dir */
int SceIofilemgrForDriver_6BEEDDB4(SceUID fd, int handle)
{
	SceVfsFile *file;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	vfsLockFd(file);
	if (file->state == SCE_VFS_FILE_STATE_DUMMY || (ret = vfs_check_fd_status(file, 0)) == 0) {
		if (file->is_dir == 0)
			ret = SCE_ERROR_ERRNO_EINVAL;
		else
			file->is_dir = handle;
	}
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}

int SceIofilemgrForDriver_338DCD68(SceUID fd, int *error)
{
	SceVfsFile *file;
	int ret, handle;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0) {
		*error = ret;
		return 0;
	}
	vfsLockFd(file);
	if (file->state == SCE_VFS_FILE_STATE_DUMMY || (ret = vfs_check_fd_status(file, 0)) == 0) {
		handle = file->is_dir;
		*error = 0;
		vfsUnlockFd(file);
		vfs_release_file(fd);
		return handle;
	}
	vfsUnlockFd(file);
	vfs_release_file(fd);
	*error = ret;
	return 0;
}

/* name unknown: lists the mounts; with no list, counts them */
int vfs_get_mount_list(VfsMountListEntry *list, int max, int *num)
{
	SceVfsMount *mnt;
	int n;

	if (list == NULL) {
		mnt = vfs_get_mnt_list();
		if (mnt == NULL)
			return 0;
		n = 0;
		do {
			mnt = mnt->mnt_list_next;
			n++;
		} while (mnt != NULL);
		return n;
	}
	mnt = vfs_get_mnt_list();
	n = 0;
	if (mnt != NULL && max > 0) {
		do {
			SceVfsMountData *data = mnt->mnt_data;

			list->mnt = mnt;
			n++;
			strncpy(list->assignName, data->assign_name, sizeof(list->assignName));
			list->openedEntryNum = mnt->opened_entry_num;
			if (mnt->fs_type == SCE_VFS_FS_TYPE_PFS) {
				list->isPfs = 1;
				list->mntId = -1;
			} else {
				list->isPfs = 0;
				list->mntId = data->mnt_id;
			}
			list->data = mnt->data;
			mnt = mnt->mnt_list_next;
			list++;
		} while (mnt != NULL && max - n > 0);
	}
	*num = n;
	return 0;
}

/* name unknown: a mount's state for the debugger */
int vfs_get_mount_info(SceVfsMount *target, VfsMountInfo *info)
{
	SceVfsMount *mnt;
	SceVfsMountData *data;
	struct FdLock *fdLock;
	struct Fumount *fumount;

	for (mnt = vfs_get_mnt_list(); mnt != target && mnt != NULL; mnt = mnt->mnt_list_next)
		;
	if (mnt == NULL)
		return SCE_ERROR_ERRNO_ENOENT;
	data = mnt->mnt_data;
	fdLock = mnt->fd_lock;
	fumount = mnt->fumount;
	info->mnt = mnt;
	info->mntId = data->mnt_id;
	strncpy(info->assignName, data->assign_name, sizeof(info->assignName));
	info->fdLockMutex = fdLock->mutex;
	info->state = mnt->state;
	info->mntFlags = mnt->mnt_flags;
	__builtin_memcpy(&info->fsType, &mnt->fs_type, sizeof(info->fsType));
	info->mntVnode = mnt->mnt_vnode;
	info->openedEntryNum = mnt->opened_entry_num;
	info->allocator = mnt->allocator;
	info->mntRefCount = mnt->mnt_ref_count;
	info->vnodeNum = mnt->vnode_num;
	info->self = mnt;
	info->vnodeList = mnt->vnode_list;
	info->mntedOnList = mnt->mnted_on_list;
	info->mntedOnListPrev = mnt->mnted_on_list_prev;
	info->defaultIoCacheSize = mnt->default_io_cache_size;
	info->data = mnt->data;
	info->fdLock = fdLock;
	info->fdLockCond = fdLock->cond;
	info->fdLockState = fdLock->state;
	info->fdLockWaitNum = fdLock->waitNum;
	info->fumount = fumount;
	info->fumountBusy = fumount->busy;
	info->devState = *fumount->devState;
	info->activationEvf = *fumount->activationEvf;
	info->mntDataPtr = &mnt->mnt_data;
	strncpy(info->path, mnt->path, sizeof(info->path));
	info->mntData = data;
	strncpy(info->fsName, data->fs_name, sizeof(info->fsName));
	if (data->blockdev_name != NULL)
		strncpy(info->blockdevName, data->blockdev_name, sizeof(info->blockdevName));
	else
		info->blockdevName[0] = '\0';
	if (data->blockdev_name_no_part != NULL)
		strncpy(info->blockdevNameNoPart, data->blockdev_name_no_part,
			sizeof(info->blockdevNameNoPart));
	else
		info->blockdevNameNoPart[0] = '\0';
	return 0;
}

/* name unknown: 1 when fd is not open on the vnode path names, 0 when it is */
int vfs_check_fd_vnode(const char *path, SceUID fd)
{
	VfsPathElem elem[VFS_PATH_ELEM_MAX];
	int num;
	SceVfsVnode *vp;
	SceVfsFile *file;
	char *buf;
	int ret;

	buf = vfs_path_buff_alloc();
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = replace_flat_path(path, buf, elem, &num, 0);
	if (ret >= 0 && num != 0) {
		ret = vfs_find_in_path(elem, num, &vp, buf, 0x400);
		if (ret >= 0) {
			vfs_decrement_vp_mnt_ref_inline(vp);
			ret = vfs_convert_file(fd, &file);
			if (ret >= 0) {
				vfsLockFd(file);
				ret = vfs_check_fd_status(file, 0);
				if (ret != 0)
					goto unlock_file;
				ret = file->vp != vp;
unlock_file:
				vfsUnlockFd(file);
				vfs_release_file(fd);
			}
		}
	}
	vfs_path_buff_free(buf);
	return ret;
}

int vfsCleanup(SceUID fd)
{
	SceVfsFile *file;
	struct DebugPath *dp;
	SceVfsVnode *vp;
	int ret;

	ret = vfs_convert_file(fd, &file);
	if (ret < 0)
		return ret;
	dp = file->debug_path;
	if (dp != NULL) {
		SCE_DBG_PRINTF(0xf, 8540, "openPath = %s\n", dp->openPath);
		SCE_DBG_PRINTF(0xf, 8541, "resolvedOpenPath = %s\n", dp->resolvedOpenPath);
	}
	vfsLockFd(file);
	ret = vfs_check_fd_status(file, 1);
	if (ret == 0) {
		vp = file->vp;
		vfsLockVnode(vp);
		ret = ksceVopCleanup(vp, file, 0, 0);
		vfsUnlockVnode(vp);
	}
	vfsUnlockFd(file);
	vfs_release_file(fd);
	return ret;
}
